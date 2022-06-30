/**
 * @file monster.cpp
 *
 * Implementation of monster functionality, AI, actions, spawning, loading, etc.
 */
#include "monster.h"

#include <algorithm>
#include <array>
#include <climits>

#include <fmt/format.h>

#include "control.h"
#include "cursor.h"
#include "dead.h"
#include "engine/cel_header.hpp"
#include "engine/load_file.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "engine/render/cl2_render.hpp"
#include "init.h"
#include "levels/drlg_l1.h"
#include "levels/drlg_l4.h"
#include "levels/themes.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "movie.h"
#include "options.h"
#include "spelldat.h"
#include "storm/storm_net.hpp"
#include "towners.h"
#include "utils/file_name_generator.hpp"
#include "utils/language.h"
#include "utils/stdcompat/string_view.hpp"
#include "utils/utf8.hpp"

#ifdef _DEBUG
#include "debug.h"
#endif

namespace devilution {

CMonster LevelMonsterTypes[MaxLvlMTypes];
int LevelMonsterTypeCount;
Monster Monsters[MaxMonsters];
int ActiveMonsters[MaxMonsters];
int ActiveMonsterCount;
// BUGFIX: replace MonsterKillCounts[MaxMonsters] with MonsterKillCounts[NUM_MTYPES].
/** Tracks the total number of monsters killed per monster_id. */
int MonsterKillCounts[MaxMonsters];
bool sgbSaveSoundOn;

namespace {

constexpr int NightmareToHitBonus = 85;
constexpr int HellToHitBonus = 120;

constexpr int NightmareAcBonus = 50;
constexpr int HellAcBonus = 80;

/** Tracks which missile files are already loaded */
int totalmonsters;
int monstimgtot;
int uniquetrans;

constexpr std::array<_monster_id, 12> SkeletonTypes {
	MT_WSKELAX,
	MT_TSKELAX,
	MT_RSKELAX,
	MT_XSKELAX,
	MT_WSKELBW,
	MT_TSKELBW,
	MT_RSKELBW,
	MT_XSKELBW,
	MT_WSKELSD,
	MT_TSKELSD,
	MT_RSKELSD,
	MT_XSKELSD,
};

// BUGFIX: MWVel velocity values are not rounded consistently. The correct
// formula for monster walk velocity is calculated as follows (for 16, 32 and 64
// pixel distances, respectively):
//
//    vel16 = (16 << monsterWalkShift) / nframes
//    vel32 = (32 << monsterWalkShift) / nframes
//    vel64 = (64 << monsterWalkShift) / nframes
//
// The correct monster walk velocity table is as follows:
//
//   int MWVel[24][3] = {
//      { 256, 512, 1024 },
//      { 128, 256, 512 },
//      { 85, 171, 341 },
//      { 64, 128, 256 },
//      { 51, 102, 205 },
//      { 43, 85, 171 },
//      { 37, 73, 146 },
//      { 32, 64, 128 },
//      { 28, 57, 114 },
//      { 26, 51, 102 },
//      { 23, 47, 93 },
//      { 21, 43, 85 },
//      { 20, 39, 79 },
//      { 18, 37, 73 },
//      { 17, 34, 68 },
//      { 16, 32, 64 },
//      { 15, 30, 60 },
//      { 14, 28, 57 },
//      { 13, 27, 54 },
//      { 13, 26, 51 },
//      { 12, 24, 49 },
//      { 12, 23, 47 },
//      { 11, 22, 45 },
//      { 11, 21, 43 }
//   };

/** Maps from monster walk animation frame num to monster velocity. */
constexpr int MWVel[24][3] = {
	{ 256, 512, 1024 },
	{ 128, 256, 512 },
	{ 85, 170, 341 },
	{ 64, 128, 256 },
	{ 51, 102, 204 },
	{ 42, 85, 170 },
	{ 36, 73, 146 },
	{ 32, 64, 128 },
	{ 28, 56, 113 },
	{ 26, 51, 102 },
	{ 23, 46, 93 },
	{ 21, 42, 85 },
	{ 19, 39, 78 },
	{ 18, 36, 73 },
	{ 17, 34, 68 },
	{ 16, 32, 64 },
	{ 15, 30, 60 },
	{ 14, 28, 57 },
	{ 13, 26, 54 },
	{ 12, 25, 51 },
	{ 12, 24, 48 },
	{ 11, 23, 46 },
	{ 11, 22, 44 },
	{ 10, 21, 42 }
};
/** Maps from monster action to monster animation letter. */
constexpr char Animletter[7] = "nwahds";

size_t GetNumAnims(const MonsterData &monsterData)
{
	return monsterData.has_special ? 6 : 5;
}

bool IsDirectionalAnim(const CMonster &monster, size_t animIndex)
{
	return monster.mtype != MT_GOLEM || animIndex < 4;
}

void InitMonsterTRN(CMonster &monst)
{
	std::array<uint8_t, 256> colorTranslations;
	LoadFileInMem(monst.MData->TransFile, colorTranslations);

	std::replace(colorTranslations.begin(), colorTranslations.end(), 255, 0);

	const size_t numAnims = GetNumAnims(*monst.MData);
	for (size_t i = 0; i < numAnims; i++) {
		if (i == 1 && IsAnyOf(monst.mtype, MT_COUNSLR, MT_MAGISTR, MT_CABALIST, MT_ADVOCATE)) {
			continue;
		}

		AnimStruct &anim = monst.Anims[i];
		if (IsDirectionalAnim(monst, i)) {
			for (size_t j = 0; j < 8; ++j) {
				Cl2ApplyTrans(anim.CelSpritesForDirections[j], colorTranslations, anim.Frames);
			}
		} else {
			byte *frames[8];
			CelGetDirectionFrames(anim.CelSpritesForDirections[0], frames);
			for (byte *frame : frames) {
				Cl2ApplyTrans(frame, colorTranslations, anim.Frames);
			}
		}
	}
}

void InitMonster(Monster &monster, Direction rd, int mtype, Point position)
{
	monster._mdir = rd;
	monster.position.tile = position;
	monster.position.future = position;
	monster.position.old = position;
	monster._mMTidx = mtype;
	monster._mmode = MonsterMode::Stand;
	monster.MType = &LevelMonsterTypes[mtype];
	monster.MData = monster.MType->MData;
	monster.mName = pgettext("monster", monster.MData->mName).data();
	monster.AnimInfo = {};
	monster.ChangeAnimationData(MonsterGraphic::Stand);
	monster.AnimInfo.TickCounterOfCurrentFrame = GenerateRnd(monster.AnimInfo.TicksPerFrame - 1);
	monster.AnimInfo.CurrentFrame = GenerateRnd(monster.AnimInfo.NumberOfFrames - 1);

	monster.mLevel = monster.MData->mLevel;
	int maxhp = monster.MData->mMinHP + GenerateRnd(monster.MData->mMaxHP - monster.MData->mMinHP + 1);
	if (monster.MType->mtype == MT_DIABLO && !gbIsHellfire) {
		maxhp /= 2;
		monster.mLevel -= 15;
	}
	monster._mmaxhp = maxhp << 6;

	if (!gbIsMultiplayer)
		monster._mmaxhp = std::max(monster._mmaxhp / 2, 64);

	monster._mhitpoints = monster._mmaxhp;
	monster._mAi = monster.MData->mAi;
	monster._mint = monster.MData->mInt;
	monster._mgoal = MGOAL_NORMAL;
	monster._mgoalvar1 = 0;
	monster._mgoalvar2 = 0;
	monster._mgoalvar3 = 0;
	monster._pathcount = 0;
	monster._mDelFlag = false;
	monster._uniqtype = 0;
	monster._msquelch = 0;
	monster.mlid = NO_LIGHT; // BUGFIX monsters initial light id should be -1 (fixed)
	monster._mRndSeed = AdvanceRndSeed();
	monster._mAISeed = AdvanceRndSeed();
	monster.mWhoHit = 0;
	monster.mExp = monster.MData->mExp;
	monster.mHit = monster.MData->mHit;
	monster.mMinDamage = monster.MData->mMinDamage;
	monster.mMaxDamage = monster.MData->mMaxDamage;
	monster.mHit2 = monster.MData->mHit2;
	monster.mMinDamage2 = monster.MData->mMinDamage2;
	monster.mMaxDamage2 = monster.MData->mMaxDamage2;
	monster.mArmorClass = monster.MData->mArmorClass;
	monster.mMagicRes = monster.MData->mMagicRes;
	monster.leader = 0;
	monster.leaderRelation = LeaderRelation::None;
	monster._mFlags = monster.MData->mFlags;
	monster.mtalkmsg = TEXT_NONE;

	if (monster._mAi == AI_GARG) {
		monster.ChangeAnimationData(MonsterGraphic::Special);
		monster.AnimInfo.CurrentFrame = 0;
		monster._mFlags |= MFLAG_ALLOW_SPECIAL;
		monster._mmode = MonsterMode::SpecialMeleeAttack;
	}

	if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
		monster._mmaxhp = 3 * monster._mmaxhp;
		if (gbIsHellfire)
			monster._mmaxhp += (gbIsMultiplayer ? 100 : 50) << 6;
		else
			monster._mmaxhp += 64;
		monster._mhitpoints = monster._mmaxhp;
		monster.mLevel += 15;
		monster.mExp = 2 * (monster.mExp + 1000);
		monster.mHit += NightmareToHitBonus;
		monster.mMinDamage = 2 * (monster.mMinDamage + 2);
		monster.mMaxDamage = 2 * (monster.mMaxDamage + 2);
		monster.mHit2 += NightmareToHitBonus;
		monster.mMinDamage2 = 2 * (monster.mMinDamage2 + 2);
		monster.mMaxDamage2 = 2 * (monster.mMaxDamage2 + 2);
		monster.mArmorClass += NightmareAcBonus;
	} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
		monster._mmaxhp = 4 * monster._mmaxhp;
		if (gbIsHellfire)
			monster._mmaxhp += (gbIsMultiplayer ? 200 : 100) << 6;
		else
			monster._mmaxhp += 192;
		monster._mhitpoints = monster._mmaxhp;
		monster.mLevel += 30;
		monster.mExp = 4 * (monster.mExp + 1000);
		monster.mHit += HellToHitBonus;
		monster.mMinDamage = 4 * monster.mMinDamage + 6;
		monster.mMaxDamage = 4 * monster.mMaxDamage + 6;
		monster.mHit2 += HellToHitBonus;
		monster.mMinDamage2 = 4 * monster.mMinDamage2 + 6;
		monster.mMaxDamage2 = 4 * monster.mMaxDamage2 + 6;
		monster.mArmorClass += HellAcBonus;
		monster.mMagicRes = monster.MData->mMagicRes2;
	}
}

bool CanPlaceMonster(Point position)
{
	return InDungeonBounds(position)
	    && dMonster[position.x][position.y] == 0
	    && dPlayer[position.x][position.y] == 0
	    && !IsTileVisible(position)
	    && !TileContainsSetPiece(position)
	    && !IsTileOccupied(position);
}

void PlaceMonster(int i, int mtype, Point position)
{
	if (LevelMonsterTypes[mtype].mtype == MT_NAKRUL) {
		for (int j = 0; j < ActiveMonsterCount; j++) {
			if (Monsters[j]._mMTidx == mtype) {
				return;
			}
			if (Monsters[j].MType->mtype == MT_NAKRUL) {
				return;
			}
		}
	}
	dMonster[position.x][position.y] = i + 1;

	auto rd = static_cast<Direction>(GenerateRnd(8));
	InitMonster(Monsters[i], rd, mtype, position);
}

void PlaceGroup(int mtype, int num, UniqueMonsterPack uniqueMonsterPack, int leaderId)
{
	int placed = 0;

	auto &leader = Monsters[leaderId];

	for (int try1 = 0; try1 < 10; try1++) {
		while (placed != 0) {
			ActiveMonsterCount--;
			placed--;
			const auto &position = Monsters[ActiveMonsterCount].position.tile;
			dMonster[position.x][position.y] = 0;
		}

		int xp;
		int yp;
		if (uniqueMonsterPack != UniqueMonsterPack::None) {
			int offset = GenerateRnd(8);
			auto position = leader.position.tile + static_cast<Direction>(offset);
			xp = position.x;
			yp = position.y;
		} else {
			do {
				xp = GenerateRnd(80) + 16;
				yp = GenerateRnd(80) + 16;
			} while (!CanPlaceMonster({ xp, yp }));
		}
		int x1 = xp;
		int y1 = yp;

		if (num + ActiveMonsterCount > totalmonsters) {
			num = totalmonsters - ActiveMonsterCount;
		}

		int j = 0;
		for (int try2 = 0; j < num && try2 < 100; xp += Displacement(static_cast<Direction>(GenerateRnd(8))).deltaX, yp += Displacement(static_cast<Direction>(GenerateRnd(8))).deltaX) { /// BUGFIX: `yp += Point.y`
			if (!CanPlaceMonster({ xp, yp })
			    || (dTransVal[xp][yp] != dTransVal[x1][y1])
			    || (uniqueMonsterPack == UniqueMonsterPack::Leashed && (abs(xp - x1) >= 4 || abs(yp - y1) >= 4))) {
				try2++;
				continue;
			}

			PlaceMonster(ActiveMonsterCount, mtype, { xp, yp });
			if (uniqueMonsterPack != UniqueMonsterPack::None) {
				auto &minion = Monsters[ActiveMonsterCount];
				minion._mmaxhp *= 2;
				minion._mhitpoints = minion._mmaxhp;
				minion._mint = leader._mint;

				if (uniqueMonsterPack == UniqueMonsterPack::Leashed) {
					minion.leader = leaderId;
					minion.leaderRelation = LeaderRelation::Leashed;
					minion._mAi = leader._mAi;
				}

				if (minion._mAi != AI_GARG) {
					minion.ChangeAnimationData(MonsterGraphic::Stand);
					minion.AnimInfo.CurrentFrame = GenerateRnd(minion.AnimInfo.NumberOfFrames - 1);
					minion._mFlags &= ~MFLAG_ALLOW_SPECIAL;
					minion._mmode = MonsterMode::Stand;
				}
			}
			ActiveMonsterCount++;
			placed++;
			j++;
		}

		if (placed >= num) {
			break;
		}
	}

	if (uniqueMonsterPack == UniqueMonsterPack::Leashed) {
		leader.packsize = placed;
	}
}

void PlaceUniqueMonst(int uniqindex, int miniontype, int bosspacksize)
{
	auto &monster = Monsters[ActiveMonsterCount];
	const auto &uniqueMonsterData = UniqueMonstersData[uniqindex];

	int uniqtype;
	for (uniqtype = 0; uniqtype < LevelMonsterTypeCount; uniqtype++) {
		if (LevelMonsterTypes[uniqtype].mtype == uniqueMonsterData.mtype) {
			break;
		}
	}

	int count = 0;
	Point position;
	while (true) {
		position = Point { GenerateRnd(80), GenerateRnd(80) } + Displacement { 16, 16 };
		int count2 = 0;
		for (int x = position.x - 3; x < position.x + 3; x++) {
			for (int y = position.y - 3; y < position.y + 3; y++) {
				if (InDungeonBounds({ x, y }) && CanPlaceMonster({ x, y })) {
					count2++;
				}
			}
		}

		if (count2 < 9) {
			count++;
			if (count < 1000) {
				continue;
			}
		}

		if (CanPlaceMonster(position)) {
			break;
		}
	}

	if (uniqindex == UMT_SNOTSPIL) {
		position = SetPiece.position.megaToWorld() + Displacement { 8, 12 };
	}
	if (uniqindex == UMT_WARLORD) {
		position = SetPiece.position.megaToWorld() + Displacement { 6, 7 };
	}
	if (uniqindex == UMT_ZHAR) {
		for (int i = 0; i < themeCount; i++) {
			if (i == zharlib) {
				position = themeLoc[i].room.position.megaToWorld() + Displacement { 4, 4 };
				break;
			}
		}
	}
	if (setlevel) {
		if (uniqindex == UMT_LAZARUS) {
			position = { 32, 46 };
		}
		if (uniqindex == UMT_RED_VEX) {
			position = { 40, 45 };
		}
		if (uniqindex == UMT_BLACKJADE) {
			position = { 38, 49 };
		}
		if (uniqindex == UMT_SKELKING) {
			position = { 35, 47 };
		}
	} else {
		if (uniqindex == UMT_LAZARUS) {
			position = SetPiece.position.megaToWorld() + Displacement { 3, 6 };
		}
		if (uniqindex == UMT_RED_VEX) {
			position = SetPiece.position.megaToWorld() + Displacement { 5, 3 };
		}
		if (uniqindex == UMT_BLACKJADE) {
			position = SetPiece.position.megaToWorld() + Displacement { 5, 9 };
		}
	}
	if (uniqindex == UMT_BUTCHER) {
		position = SetPiece.position.megaToWorld() + Displacement { 4, 4 };
	}

	if (uniqindex == UMT_NAKRUL) {
		if (UberRow == 0 || UberCol == 0) {
			UberDiabloMonsterIndex = -1;
			return;
		}
		position = { UberRow - 2, UberCol };
		UberDiabloMonsterIndex = ActiveMonsterCount;
	}
	PlaceMonster(ActiveMonsterCount, uniqtype, position);
	PrepareUniqueMonst(monster, uniqindex, miniontype, bosspacksize, uniqueMonsterData);
}

int GetMonsterTypeIndex(_monster_id type)
{
	for (int i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].mtype == type)
			return i;
	}
	return LevelMonsterTypeCount;
}

int AddMonsterType(_monster_id type, placeflag placeflag)
{
	int typeIndex = GetMonsterTypeIndex(type);

	if (typeIndex == LevelMonsterTypeCount) {
		LevelMonsterTypeCount++;
		LevelMonsterTypes[typeIndex].mtype = type;
		monstimgtot += MonstersData[type].mImage;
		InitMonsterGFX(typeIndex);
		InitMonsterSND(typeIndex);
	}

	LevelMonsterTypes[typeIndex].mPlaceFlags |= placeflag;
	return typeIndex;
}

void ClearMVars(Monster &monster)
{
	monster._mVar1 = 0;
	monster._mVar2 = 0;
	monster._mVar3 = 0;
	monster.position.temp = { 0, 0 };
	monster.position.offset2 = { 0, 0 };
}

void ClrAllMonsters()
{
	for (auto &monster : Monsters) {
		ClearMVars(monster);
		monster.mName = "Invalid Monster";
		monster._mgoal = MGOAL_NONE;
		monster._mmode = MonsterMode::Stand;
		monster._mVar1 = 0;
		monster._mVar2 = 0;
		monster.position.tile = { 0, 0 };
		monster.position.future = { 0, 0 };
		monster.position.old = { 0, 0 };
		monster._mdir = static_cast<Direction>(GenerateRnd(8));
		monster.position.velocity = { 0, 0 };
		monster.AnimInfo = {};
		monster._mFlags = 0;
		monster._mDelFlag = false;
		monster._menemy = GenerateRnd(gbActivePlayers);
		monster.enemyPosition = Players[monster._menemy].position.future;
	}
}

void PlaceUniqueMonsters()
{
	for (int u = 0; UniqueMonstersData[u].mtype != -1; u++) {
		if (UniqueMonstersData[u].mlevel != currlevel)
			continue;

		int mt = GetMonsterTypeIndex(UniqueMonstersData[u].mtype);
		if (mt == LevelMonsterTypeCount)
			continue;

		if (u == UMT_GARBUD && Quests[Q_GARBUD]._qactive == QUEST_NOTAVAIL)
			continue;
		if (u == UMT_ZHAR && Quests[Q_ZHAR]._qactive == QUEST_NOTAVAIL)
			continue;
		if (u == UMT_SNOTSPIL && Quests[Q_LTBANNER]._qactive == QUEST_NOTAVAIL)
			continue;
		if (u == UMT_LACHDAN && Quests[Q_VEIL]._qactive == QUEST_NOTAVAIL)
			continue;
		if (u == UMT_WARLORD && Quests[Q_WARLORD]._qactive == QUEST_NOTAVAIL)
			continue;

		PlaceUniqueMonst(u, mt, 8);
	}
}

void PlaceQuestMonsters()
{
	if (!setlevel) {
		if (Quests[Q_BUTCHER].IsAvailable()) {
			PlaceUniqueMonst(UMT_BUTCHER, 0, 0);
		}

		if (currlevel == Quests[Q_SKELKING]._qlevel && gbIsMultiplayer) {
			for (int i = 0; i < LevelMonsterTypeCount; i++) {
				if (IsSkel(LevelMonsterTypes[i].mtype)) {
					PlaceUniqueMonst(UMT_SKELKING, i, 30);
					break;
				}
			}
		}

		if (Quests[Q_LTBANNER].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("Levels\\L1Data\\Banner1.DUN");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
		}
		if (Quests[Q_BLOOD].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("Levels\\L2Data\\Blood2.DUN");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
		}
		if (Quests[Q_BLIND].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("Levels\\L2Data\\Blind2.DUN");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
		}
		if (Quests[Q_ANVIL].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("Levels\\L3Data\\Anvil.DUN");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld() + Displacement { 2, 2 });
		}
		if (Quests[Q_WARLORD].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("Levels\\L4Data\\Warlord.DUN");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
			AddMonsterType(UniqueMonstersData[UMT_WARLORD].mtype, PLACE_SCATTER);
		}
		if (Quests[Q_VEIL].IsAvailable()) {
			AddMonsterType(UniqueMonstersData[UMT_LACHDAN].mtype, PLACE_SCATTER);
		}
		if (Quests[Q_ZHAR].IsAvailable() && zharlib == -1) {
			Quests[Q_ZHAR]._qactive = QUEST_NOTAVAIL;
		}

		if (currlevel == Quests[Q_BETRAYER]._qlevel && gbIsMultiplayer) {
			AddMonsterType(UniqueMonstersData[UMT_LAZARUS].mtype, PLACE_UNIQUE);
			AddMonsterType(UniqueMonstersData[UMT_RED_VEX].mtype, PLACE_UNIQUE);
			PlaceUniqueMonst(UMT_LAZARUS, 0, 0);
			PlaceUniqueMonst(UMT_RED_VEX, 0, 0);
			PlaceUniqueMonst(UMT_BLACKJADE, 0, 0);
			auto dunData = LoadFileInMem<uint16_t>("Levels\\L4Data\\Vile1.DUN");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
		}

		if (currlevel == 24) {
			UberDiabloMonsterIndex = -1;
			int i1;
			for (i1 = 0; i1 < LevelMonsterTypeCount; i1++) {
				if (LevelMonsterTypes[i1].mtype == UniqueMonstersData[UMT_NAKRUL].mtype)
					break;
			}

			if (i1 < LevelMonsterTypeCount) {
				for (int i2 = 0; i2 < ActiveMonsterCount; i2++) {
					auto &monster = Monsters[i2];
					if (monster._uniqtype != 0 || monster._mMTidx == i1) {
						UberDiabloMonsterIndex = i2;
						break;
					}
				}
			}
			if (UberDiabloMonsterIndex == -1)
				PlaceUniqueMonst(UMT_NAKRUL, 0, 0);
		}
	} else if (setlvlnum == SL_SKELKING) {
		PlaceUniqueMonst(UMT_SKELKING, 0, 0);
	}
}

void LoadDiabMonsts()
{
	{
		auto dunData = LoadFileInMem<uint16_t>("Levels\\L4Data\\diab1.DUN");
		SetMapMonsters(dunData.get(), DiabloQuad1.megaToWorld());
	}
	{
		auto dunData = LoadFileInMem<uint16_t>("Levels\\L4Data\\diab2a.DUN");
		SetMapMonsters(dunData.get(), DiabloQuad2.megaToWorld());
	}
	{
		auto dunData = LoadFileInMem<uint16_t>("Levels\\L4Data\\diab3a.DUN");
		SetMapMonsters(dunData.get(), DiabloQuad3.megaToWorld());
	}
	{
		auto dunData = LoadFileInMem<uint16_t>("Levels\\L4Data\\diab4a.DUN");
		SetMapMonsters(dunData.get(), DiabloQuad4.megaToWorld());
	}
}

void DeleteMonster(size_t activeIndex)
{
	const auto &monster = Monsters[ActiveMonsters[activeIndex]];
	if ((monster._mFlags & MFLAG_BERSERK) != 0) {
		AddUnLight(monster.mlid);
	}

	ActiveMonsterCount--;
	std::swap(ActiveMonsters[activeIndex], ActiveMonsters[ActiveMonsterCount]); // This ensures alive monsters are before ActiveMonsterCount in the array and any deleted monster after
}

void NewMonsterAnim(Monster &monster, MonsterGraphic graphic, Direction md, AnimationDistributionFlags flags = AnimationDistributionFlags::None, int numSkippedFrames = 0, int distributeFramesBeforeFrame = 0)
{
	const auto &animData = monster.MType->GetAnimData(graphic);
	monster.AnimInfo.SetNewAnimation(animData.GetCelSpritesForDirection(md), animData.Frames, animData.Rate, flags, numSkippedFrames, distributeFramesBeforeFrame);
	monster._mFlags &= ~(MFLAG_LOCK_ANIMATION | MFLAG_ALLOW_SPECIAL);
	monster._mdir = md;
}

void StartMonsterGotHit(int monsterId)
{
	auto &monster = Monsters[monsterId];
	if (monster.MType->mtype != MT_GOLEM) {
		auto animationFlags = gGameLogicStep < GameLogicStep::ProcessMonsters ? AnimationDistributionFlags::ProcessAnimationPending : AnimationDistributionFlags::None;
		int numSkippedFrames = (gbIsHellfire && monster.MType->mtype == MT_DIABLO) ? 4 : 0;
		NewMonsterAnim(monster, MonsterGraphic::GotHit, monster._mdir, animationFlags, numSkippedFrames);
		monster._mmode = MonsterMode::HitRecovery;
	}
	monster.position.offset = { 0, 0 };
	monster.position.tile = monster.position.old;
	monster.position.future = monster.position.old;
	M_ClearSquares(monsterId);
	dMonster[monster.position.tile.x][monster.position.tile.y] = monsterId + 1;
}

bool IsRanged(Monster &monster)
{
	return IsAnyOf(monster._mAi, AI_SKELBOW, AI_GOATBOW, AI_SUCC, AI_LAZHELP);
}

void UpdateEnemy(Monster &monster)
{
	Point target;
	int menemy = -1;
	int bestDist = -1;
	bool bestsameroom = false;
	const auto &position = monster.position.tile;
	if ((monster._mFlags & MFLAG_BERSERK) != 0 || (monster._mFlags & MFLAG_GOLEM) == 0) {
		for (int pnum = 0; pnum < MAX_PLRS; pnum++) {
			Player &player = Players[pnum];
			if (!player.plractive || !player.isOnActiveLevel() || player._pLvlChanging
			    || (((player._pHitPoints >> 6) == 0) && gbIsMultiplayer))
				continue;
			bool sameroom = (dTransVal[position.x][position.y] == dTransVal[player.position.tile.x][player.position.tile.y]);
			int dist = position.WalkingDistance(player.position.tile);
			if ((sameroom && !bestsameroom)
			    || ((sameroom || !bestsameroom) && dist < bestDist)
			    || (menemy == -1)) {
				monster._mFlags &= ~MFLAG_TARGETS_MONSTER;
				menemy = pnum;
				target = player.position.future;
				bestDist = dist;
				bestsameroom = sameroom;
			}
		}
	}
	for (int j = 0; j < ActiveMonsterCount; j++) {
		int mi = ActiveMonsters[j];
		auto &otherMonster = Monsters[mi];
		if (&otherMonster == &monster)
			continue;
		if ((otherMonster._mhitpoints >> 6) <= 0)
			continue;
		if (otherMonster.position.tile == GolemHoldingCell)
			continue;
		if (M_Talker(otherMonster) && otherMonster.mtalkmsg != TEXT_NONE)
			continue;
		bool isBerserked = (monster._mFlags & MFLAG_BERSERK) != 0 || (otherMonster._mFlags & MFLAG_BERSERK) != 0;
		if ((monster._mFlags & MFLAG_GOLEM) != 0 && (otherMonster._mFlags & MFLAG_GOLEM) != 0 && !isBerserked) // prevent golems from fighting each other
			continue;

		int dist = otherMonster.position.tile.WalkingDistance(position);
		if (((monster._mFlags & MFLAG_GOLEM) == 0
		        && (monster._mFlags & MFLAG_BERSERK) == 0
		        && dist >= 2
		        && !IsRanged(monster))
		    || ((monster._mFlags & MFLAG_GOLEM) == 0
		        && (monster._mFlags & MFLAG_BERSERK) == 0
		        && (otherMonster._mFlags & MFLAG_GOLEM) == 0)) {
			continue;
		}
		bool sameroom = dTransVal[position.x][position.y] == dTransVal[otherMonster.position.tile.x][otherMonster.position.tile.y];
		if ((sameroom && !bestsameroom)
		    || ((sameroom || !bestsameroom) && dist < bestDist)
		    || (menemy == -1)) {
			monster._mFlags |= MFLAG_TARGETS_MONSTER;
			menemy = mi;
			target = otherMonster.position.future;
			bestDist = dist;
			bestsameroom = sameroom;
		}
	}
	if (menemy != -1) {
		monster._mFlags &= ~MFLAG_NO_ENEMY;
		monster._menemy = menemy;
		monster.enemyPosition = target;
	} else {
		monster._mFlags |= MFLAG_NO_ENEMY;
	}
}

/**
 * @brief Make the AI wait a bit before thinking again
 * @param len
 */
void AiDelay(Monster &monster, int len)
{
	if (len <= 0) {
		return;
	}

	if (monster._mAi == AI_LAZARUS) {
		return;
	}

	monster._mVar2 = len;
	monster._mmode = MonsterMode::Delay;
}

/**
 * @brief Get the direction from the monster to its current enemy
 */
Direction GetMonsterDirection(Monster &monster)
{
	return GetDirection(monster.position.tile, monster.enemyPosition);
}

void StartSpecialStand(Monster &monster, Direction md)
{
	NewMonsterAnim(monster, MonsterGraphic::Special, md);
	monster._mmode = MonsterMode::SpecialStand;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartWalk(int monsterId, int xvel, int yvel, int xadd, int yadd, Direction endDir)
{
	auto &monster = Monsters[monsterId];

	int fx = xadd + monster.position.tile.x;
	int fy = yadd + monster.position.tile.y;

	dMonster[fx][fy] = -(monsterId + 1);
	monster._mmode = MonsterMode::MoveNorthwards;
	monster.position.old = monster.position.tile;
	monster.position.future = { fx, fy };
	monster.position.velocity = { xvel, yvel };
	monster._mVar1 = xadd;
	monster._mVar2 = yadd;
	monster._mVar3 = static_cast<int>(endDir);
	NewMonsterAnim(monster, MonsterGraphic::Walk, endDir, AnimationDistributionFlags::ProcessAnimationPending, -1);
	monster.position.offset2 = { 0, 0 };
}

void StartWalk2(int monsterId, int xvel, int yvel, int xoff, int yoff, int xadd, int yadd, Direction endDir)
{
	auto &monster = Monsters[monsterId];

	int fx = xadd + monster.position.tile.x;
	int fy = yadd + monster.position.tile.y;

	dMonster[monster.position.tile.x][monster.position.tile.y] = -(monsterId + 1);
	monster._mVar1 = monster.position.tile.x;
	monster._mVar2 = monster.position.tile.y;
	monster.position.old = monster.position.tile;
	monster.position.tile = { fx, fy };
	monster.position.future = { fx, fy };
	dMonster[fx][fy] = monsterId + 1;
	if (monster.mlid != NO_LIGHT)
		ChangeLightXY(monster.mlid, monster.position.tile);
	monster.position.offset = { xoff, yoff };
	monster._mmode = MonsterMode::MoveSouthwards;
	monster.position.velocity = { xvel, yvel };
	monster._mVar3 = static_cast<int>(endDir);
	NewMonsterAnim(monster, MonsterGraphic::Walk, endDir, AnimationDistributionFlags::ProcessAnimationPending, -1);
	monster.position.offset2 = { 16 * xoff, 16 * yoff };
}

void StartWalk3(int monsterId, int xvel, int yvel, int xoff, int yoff, int xadd, int yadd, int mapx, int mapy, Direction endDir)
{
	auto &monster = Monsters[monsterId];

	int fx = xadd + monster.position.tile.x;
	int fy = yadd + monster.position.tile.y;
	int x = mapx + monster.position.tile.x;
	int y = mapy + monster.position.tile.y;

	if (monster.mlid != NO_LIGHT)
		ChangeLightXY(monster.mlid, { x, y });

	dMonster[monster.position.tile.x][monster.position.tile.y] = -(monsterId + 1);
	dMonster[fx][fy] = monsterId + 1;
	monster.position.temp = { x, y };
	monster.position.old = monster.position.tile;
	monster.position.future = { fx, fy };
	monster.position.offset = { xoff, yoff };
	monster._mmode = MonsterMode::MoveSideways;
	monster.position.velocity = { xvel, yvel };
	monster._mVar1 = fx;
	monster._mVar2 = fy;
	monster._mVar3 = static_cast<int>(endDir);
	NewMonsterAnim(monster, MonsterGraphic::Walk, endDir, AnimationDistributionFlags::ProcessAnimationPending, -1);
	monster.position.offset2 = { 16 * xoff, 16 * yoff };
}

void StartAttack(Monster &monster)
{
	Direction md = GetMonsterDirection(monster);
	NewMonsterAnim(monster, MonsterGraphic::Attack, md, AnimationDistributionFlags::ProcessAnimationPending);
	monster._mmode = MonsterMode::MeleeAttack;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartRangedAttack(Monster &monster, missile_id missileType, int dam)
{
	Direction md = GetMonsterDirection(monster);
	NewMonsterAnim(monster, MonsterGraphic::Attack, md, AnimationDistributionFlags::ProcessAnimationPending);
	monster._mmode = MonsterMode::RangedAttack;
	monster._mVar1 = missileType;
	monster._mVar2 = dam;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartRangedSpecialAttack(Monster &monster, missile_id missileType, int dam)
{
	Direction md = GetMonsterDirection(monster);
	int distributeFramesBeforeFrame = 0;
	if (monster._mAi == AI_MEGA)
		distributeFramesBeforeFrame = monster.MData->mAFNum2;
	NewMonsterAnim(monster, MonsterGraphic::Special, md, AnimationDistributionFlags::ProcessAnimationPending, 0, distributeFramesBeforeFrame);
	monster._mmode = MonsterMode::SpecialRangedAttack;
	monster._mVar1 = missileType;
	monster._mVar2 = 0;
	monster._mVar3 = dam;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartSpecialAttack(Monster &monster)
{
	Direction md = GetMonsterDirection(monster);
	NewMonsterAnim(monster, MonsterGraphic::Special, md);
	monster._mmode = MonsterMode::SpecialMeleeAttack;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartEating(Monster &monster)
{
	NewMonsterAnim(monster, MonsterGraphic::Special, monster._mdir);
	monster._mmode = MonsterMode::SpecialMeleeAttack;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void DiabloDeath(Monster &diablo, bool sendmsg)
{
	PlaySFX(USFX_DIABLOD);
	auto &quest = Quests[Q_DIABLO];
	quest._qactive = QUEST_DONE;
	if (sendmsg)
		NetSendCmdQuest(true, quest);
	sgbSaveSoundOn = gbSoundOn;
	gbProcessPlayers = false;
	for (int j = 0; j < ActiveMonsterCount; j++) {
		int k = ActiveMonsters[j];
		auto &monster = Monsters[k];
		if (monster.MType->mtype == MT_DIABLO || diablo._msquelch == 0)
			continue;

		NewMonsterAnim(monster, MonsterGraphic::Death, monster._mdir);
		monster._mmode = MonsterMode::Death;
		monster.position.offset = { 0, 0 };
		monster._mVar1 = 0;
		monster.position.tile = monster.position.old;
		monster.position.future = monster.position.tile;
		M_ClearSquares(k);
		dMonster[monster.position.tile.x][monster.position.tile.y] = k + 1;
	}
	AddLight(diablo.position.tile, 8);
	DoVision(diablo.position.tile, 8, MAP_EXP_NONE, true);
	int dist = diablo.position.tile.WalkingDistance(ViewPosition);
	if (dist > 20)
		dist = 20;
	diablo._mVar3 = ViewPosition.x << 16;
	diablo.position.temp.x = ViewPosition.y << 16;
	diablo.position.temp.y = (int)((diablo._mVar3 - (diablo.position.tile.x << 16)) / (double)dist);
	diablo.position.offset2.deltaX = (int)((diablo.position.temp.x - (diablo.position.tile.y << 16)) / (double)dist);
}

void SpawnLoot(Monster &monster, bool sendmsg)
{
	if (monster.MType->mtype == MT_HORKSPWN) {
		return;
	}

	if (Quests[Q_GARBUD].IsAvailable() && monster._uniqtype - 1 == UMT_GARBUD) {
		CreateTypeItem(monster.position.tile + Displacement { 1, 1 }, true, ItemType::Mace, IMISC_NONE, sendmsg, false);
	} else if (monster._uniqtype - 1 == UMT_DEFILER) {
		if (effect_is_playing(USFX_DEFILER8))
			stream_stop();
		Quests[Q_DEFILER]._qlog = false;
		SpawnMapOfDoom(monster.position.tile, sendmsg);
	} else if (monster._uniqtype - 1 == UMT_HORKDMN) {
		if (sgGameInitInfo.bTheoQuest != 0) {
			SpawnTheodore(monster.position.tile, sendmsg);
		} else {
			CreateAmulet(monster.position.tile, 13, sendmsg, false);
		}
	} else if (monster.MType->mtype == MT_NAKRUL) {
		int nSFX = IsUberRoomOpened ? USFX_NAKRUL4 : USFX_NAKRUL5;
		if (sgGameInitInfo.bCowQuest != 0)
			nSFX = USFX_NAKRUL6;
		if (effect_is_playing(nSFX))
			stream_stop();
		Quests[Q_NAKRUL]._qlog = false;
		UberDiabloMonsterIndex = -2;
		CreateMagicWeapon(monster.position.tile, ItemType::Sword, ICURS_GREAT_SWORD, sendmsg, false);
		CreateMagicWeapon(monster.position.tile, ItemType::Staff, ICURS_WAR_STAFF, sendmsg, false);
		CreateMagicWeapon(monster.position.tile, ItemType::Bow, ICURS_LONG_WAR_BOW, sendmsg, false);
		CreateSpellBook(monster.position.tile, SPL_APOCA, sendmsg, false);
	} else if (monster.MType->mtype != MT_GOLEM) {
		SpawnItem(monster, monster.position.tile, sendmsg);
	}
}

std::optional<Point> GetTeleportTile(const Monster &monster)
{
	int mx = monster.enemyPosition.x;
	int my = monster.enemyPosition.y;
	int rx = 2 * GenerateRnd(2) - 1;
	int ry = 2 * GenerateRnd(2) - 1;

	for (int j = -1; j <= 1; j++) {
		for (int k = -1; k < 1; k++) {
			if (j == 0 && k == 0)
				continue;
			int x = mx + rx * j;
			int y = my + ry * k;
			if (!InDungeonBounds({ x, y }) || x == monster.position.tile.x || y == monster.position.tile.y)
				continue;
			if (IsTileAvailable(monster, { x, y }))
				return Point { x, y };
		}
	}
	return {};
}

void Teleport(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode == MonsterMode::Petrified)
		return;

	std::optional<Point> position = GetTeleportTile(monster);
	if (!position)
		return;

	M_ClearSquares(monsterId);
	dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
	dMonster[position->x][position->y] = monsterId + 1;
	monster.position.old = *position;
	monster._mdir = GetMonsterDirection(monster);

	if (monster.mlid != NO_LIGHT) {
		ChangeLightXY(monster.mlid, *position);
	}
}

void HitMonster(int monsterId, int dam)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);

	delta_monster_hp(monsterId, monster._mhitpoints, *MyPlayer);
	NetSendCmdMonDmg(false, monsterId, dam);
	PlayEffect(monster, 1);

	if (IsAnyOf(monster.MType->mtype, MT_SNEAK, MT_STALKER, MT_UNSEEN, MT_ILLWEAV) || dam >> 6 >= monster.mLevel + 3) {
		if (monster.MType->mtype == MT_BLINK) {
			Teleport(monsterId);
		} else if (IsAnyOf(monster.MType->mtype, MT_NSCAV, MT_BSCAV, MT_WSCAV, MT_YSCAV, MT_GRAVEDIG)) {
			monster._mgoal = MGOAL_NORMAL;
			monster._mgoalvar1 = 0;
			monster._mgoalvar2 = 0;
		}

		if (monster._mmode != MonsterMode::Petrified) {
			StartMonsterGotHit(monsterId);
		}
	}
}

void MonsterHitMonster(int monsterId, int i, int dam)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);

	if (i < MAX_PLRS)
		monster.mWhoHit |= 1 << i;

	if (IsAnyOf(monster.MType->mtype, MT_SNEAK, MT_STALKER, MT_UNSEEN, MT_ILLWEAV) || dam >> 6 >= monster.mLevel + 3) {
		monster._mdir = Opposite(Monsters[i]._mdir);
	}

	HitMonster(monsterId, dam);
}

void MonsterDeath(int monsterId, int pnum, Direction md, bool sendmsg)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);

	if (pnum < MAX_PLRS) {
		if (pnum >= 0)
			monster.mWhoHit |= 1 << pnum;
		if (monster.MType->mtype != MT_GOLEM)
			AddPlrMonstExper(monster.mLevel, monster.mExp, monster.mWhoHit);
	}

	MonsterKillCounts[monster.MType->mtype]++;
	monster._mhitpoints = 0;
	SetRndSeed(monster._mRndSeed);

	SpawnLoot(monster, sendmsg);

	if (monster.MType->mtype == MT_DIABLO)
		DiabloDeath(monster, true);
	else
		PlayEffect(monster, 2);

	if (monster._mmode != MonsterMode::Petrified) {
		if (monster.MType->mtype == MT_GOLEM)
			md = Direction::South;
		NewMonsterAnim(monster, MonsterGraphic::Death, md, gGameLogicStep < GameLogicStep::ProcessMonsters ? AnimationDistributionFlags::ProcessAnimationPending : AnimationDistributionFlags::None);
		monster._mmode = MonsterMode::Death;
	}
	monster._mgoal = MGOAL_NONE;
	monster._mVar1 = 0;
	monster.position.offset = { 0, 0 };
	monster.position.tile = monster.position.old;
	monster.position.future = monster.position.old;
	M_ClearSquares(monsterId);
	dMonster[monster.position.tile.x][monster.position.tile.y] = monsterId + 1;
	CheckQuestKill(monster, sendmsg);
	M_FallenFear(monster.position.tile);
	if (IsAnyOf(monster.MType->mtype, MT_NACID, MT_RACID, MT_BACID, MT_XACID, MT_SPIDLORD))
		AddMissile(monster.position.tile, { 0, 0 }, Direction::South, MIS_ACIDPUD, TARGET_PLAYERS, monsterId, monster._mint + 1, 0);
}

void StartDeathFromMonster(int i, int mid)
{
	assert(i >= 0 && i < MaxMonsters);
	Monster &killer = Monsters[i];
	assert(mid >= 0 && mid < MaxMonsters);
	Monster &monster = Monsters[mid];

	delta_kill_monster(mid, monster.position.tile, *MyPlayer);
	NetSendCmdLocParam1(false, CMD_MONSTDEATH, monster.position.tile, mid);

	Direction md = GetDirection(monster.position.tile, killer.position.tile);
	MonsterDeath(mid, i, md, true);
	if (gbIsHellfire)
		M_StartStand(killer, killer._mdir);
}

void StartFadein(Monster &monster, Direction md, bool backwards)
{
	NewMonsterAnim(monster, MonsterGraphic::Special, md);
	monster._mmode = MonsterMode::FadeIn;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
	monster._mFlags &= ~MFLAG_HIDDEN;
	if (backwards) {
		monster._mFlags |= MFLAG_LOCK_ANIMATION;
		monster.AnimInfo.CurrentFrame = monster.AnimInfo.NumberOfFrames - 1;
	}
}

void StartFadeout(Monster &monster, Direction md, bool backwards)
{
	NewMonsterAnim(monster, MonsterGraphic::Special, md);
	monster._mmode = MonsterMode::FadeOut;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
	if (backwards) {
		monster._mFlags |= MFLAG_LOCK_ANIMATION;
		monster.AnimInfo.CurrentFrame = monster.AnimInfo.NumberOfFrames - 1;
	}
}

void StartHeal(Monster &monster)
{
	monster.ChangeAnimationData(MonsterGraphic::Special);
	monster.AnimInfo.CurrentFrame = monster.MType->GetAnimData(MonsterGraphic::Special).Frames - 1;
	monster._mFlags |= MFLAG_LOCK_ANIMATION;
	monster._mmode = MonsterMode::Heal;
	monster._mVar1 = monster._mmaxhp / (16 * (GenerateRnd(5) + 4));
}

void SyncLightPosition(Monster &monster)
{
	int lx = (monster.position.offset.deltaX + 2 * monster.position.offset.deltaY) / 8;
	int ly = (2 * monster.position.offset.deltaY - monster.position.offset.deltaX) / 8;

	if (monster.mlid != NO_LIGHT)
		ChangeLightOffset(monster.mlid, { lx, ly });
}

bool MonsterIdle(Monster &monster)
{
	if (monster.MType->mtype == MT_GOLEM)
		monster.ChangeAnimationData(MonsterGraphic::Walk);
	else
		monster.ChangeAnimationData(MonsterGraphic::Stand);

	if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1)
		UpdateEnemy(monster);

	monster._mVar2++;

	return false;
}

/**
 * @brief Continue movement towards new tile
 */
bool MonsterWalk(int monsterId, MonsterMode variant)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);

	// Check if we reached new tile
	const bool isAnimationEnd = monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1;
	if (isAnimationEnd) {
		switch (variant) {
		case MonsterMode::MoveNorthwards:
			dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
			monster.position.tile.x += monster._mVar1;
			monster.position.tile.y += monster._mVar2;
			dMonster[monster.position.tile.x][monster.position.tile.y] = monsterId + 1;
			break;
		case MonsterMode::MoveSouthwards:
			dMonster[monster._mVar1][monster._mVar2] = 0;
			break;
		case MonsterMode::MoveSideways:
			dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
			monster.position.tile = { monster._mVar1, monster._mVar2 };
			// dMonster is set here for backwards comparability, without it the monster would be invisible if loaded from a vanilla save.
			dMonster[monster.position.tile.x][monster.position.tile.y] = monsterId + 1;
			break;
		default:
			break;
		}
		if (monster.mlid != NO_LIGHT)
			ChangeLightXY(monster.mlid, monster.position.tile);
		M_StartStand(monster, monster._mdir);
	} else { // We didn't reach new tile so update monster's "sub-tile" position
		if (monster.AnimInfo.TickCounterOfCurrentFrame == 0) {
			if (monster.AnimInfo.CurrentFrame == 0 && monster.MType->mtype == MT_FLESTHNG)
				PlayEffect(monster, 3);
			monster.position.offset2 += monster.position.velocity;
			monster.position.offset.deltaX = monster.position.offset2.deltaX >> 4;
			monster.position.offset.deltaY = monster.position.offset2.deltaY >> 4;
		}
	}

	if (monster.mlid != NO_LIGHT) // BUGFIX: change uniqtype check to mlid check like it is in all other places (fixed)
		SyncLightPosition(monster);

	return isAnimationEnd;
}

void MonsterAttackMonster(int i, int mid, int hper, int mind, int maxd)
{
	assert(mid >= 0 && mid < MaxMonsters);
	auto &monster = Monsters[mid];
	assert(monster.MType != nullptr);

	if (!monster.IsPossibleToHit())
		return;

	int hit = GenerateRnd(100);
	if (monster._mmode == MonsterMode::Petrified)
		hit = 0;
	if (monster.TryLiftGargoyle())
		return;
	if (hit >= hper)
		return;

	int dam = (mind + GenerateRnd(maxd - mind + 1)) << 6;
	monster._mhitpoints -= dam;
	if (monster._mhitpoints >> 6 <= 0) {
		StartDeathFromMonster(i, mid);
	} else {
		MonsterHitMonster(mid, i, dam);
	}

	Monster &attackingMonster = Monsters[i];
	if (monster._msquelch == 0) {
		monster._msquelch = UINT8_MAX;
		monster.position.last = attackingMonster.position.tile;
	}
}

void CheckReflect(int monsterId, int pnum, int dam)
{
	auto &monster = Monsters[monsterId];
	Player &player = Players[pnum];

	player.wReflections--;
	if (player.wReflections <= 0)
		NetSendCmdParam1(true, CMD_SETREFLECT, 0);
	// reflects 20-30% damage
	int mdam = dam * (GenerateRnd(10) + 20L) / 100;
	monster._mhitpoints -= mdam;
	dam = std::max(dam - mdam, 0);
	if (monster._mhitpoints >> 6 <= 0)
		M_StartKill(monsterId, pnum);
	else
		M_StartHit(monsterId, pnum, mdam);
}

void MonsterAttackPlayer(int monsterId, int pnum, int hit, int minDam, int maxDam)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);

	if ((monster._mFlags & MFLAG_TARGETS_MONSTER) != 0) {
		MonsterAttackMonster(monsterId, pnum, hit, minDam, maxDam);
		return;
	}

	Player &player = Players[pnum];

	if (player._pHitPoints >> 6 <= 0 || player._pInvincible || HasAnyOf(player._pSpellFlags, SpellFlag::Etherealize))
		return;
	if (monster.position.tile.WalkingDistance(player.position.tile) >= 2)
		return;

	int hper = GenerateRnd(100);
#ifdef _DEBUG
	if (DebugGodMode)
		hper = 1000;
#endif
	int ac = player.GetArmor();
	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::ACAgainstDemons) && monster.MData->mMonstClass == MonsterClass::Demon)
		ac += 40;
	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::ACAgainstUndead) && monster.MData->mMonstClass == MonsterClass::Undead)
		ac += 20;
	hit += 2 * (monster.mLevel - player._pLevel)
	    + 30
	    - ac;
	int minhit = 15;
	if (currlevel == 14)
		minhit = 20;
	if (currlevel == 15)
		minhit = 25;
	if (currlevel == 16)
		minhit = 30;
	hit = std::max(hit, minhit);
	int blkper = 100;
	if ((player._pmode == PM_STAND || player._pmode == PM_ATTACK) && player._pBlockFlag) {
		blkper = GenerateRnd(100);
	}
	int blk = player.GetBlockChance() - (monster.mLevel * 2);
	blk = clamp(blk, 0, 100);
	if (hper >= hit)
		return;
	if (blkper < blk) {
		Direction dir = GetDirection(player.position.tile, monster.position.tile);
		StartPlrBlock(pnum, dir);
		if (pnum == MyPlayerId && player.wReflections > 0) {
			int dam = GenerateRnd(((maxDam - minDam) << 6) + 1) + (minDam << 6);
			dam = std::max(dam + (player._pIGetHit << 6), 64);
			CheckReflect(monsterId, pnum, dam);
		}
		return;
	}
	if (monster.MType->mtype == MT_YZOMBIE && pnum == MyPlayerId) {
		if (player._pMaxHP > 64) {
			if (player._pMaxHPBase > 64) {
				player._pMaxHP -= 64;
				if (player._pHitPoints > player._pMaxHP) {
					player._pHitPoints = player._pMaxHP;
				}
				player._pMaxHPBase -= 64;
				if (player._pHPBase > player._pMaxHPBase) {
					player._pHPBase = player._pMaxHPBase;
				}
			}
		}
	}
	int dam = (minDam << 6) + GenerateRnd(((maxDam - minDam) << 6) + 1);
	dam = std::max(dam + (player._pIGetHit << 6), 64);
	if (pnum == MyPlayerId) {
		if (player.wReflections > 0)
			CheckReflect(monsterId, pnum, dam);
		ApplyPlrDamage(pnum, 0, 0, dam);
	}

	// Reflect can also kill a monster, so make sure the monster is still alive
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::Thorns) && monster._mmode != MonsterMode::Death) {
		int mdam = (GenerateRnd(3) + 1) << 6;
		monster._mhitpoints -= mdam;
		if (monster._mhitpoints >> 6 <= 0)
			M_StartKill(monsterId, pnum);
		else
			M_StartHit(monsterId, pnum, mdam);
	}
	if ((monster._mFlags & MFLAG_NOLIFESTEAL) == 0 && monster.MType->mtype == MT_SKING && gbIsMultiplayer)
		monster._mhitpoints += dam;
	if (player._pHitPoints >> 6 <= 0) {
		if (gbIsHellfire)
			M_StartStand(monster, monster._mdir);
		return;
	}
	StartPlrHit(pnum, dam, false);
	if ((monster._mFlags & MFLAG_KNOCKBACK) != 0) {
		if (player._pmode != PM_GOTHIT)
			StartPlrHit(pnum, 0, true);

		Point newPosition = player.position.tile + monster._mdir;
		if (PosOkPlayer(player, newPosition)) {
			player.position.tile = newPosition;
			FixPlayerLocation(player, player._pdir);
			FixPlrWalkTags(pnum);
			dPlayer[newPosition.x][newPosition.y] = pnum + 1;
			SetPlayerOld(player);
		}
	}
}

bool MonsterAttack(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);
	assert(monster.MData != nullptr);

	if (monster.AnimInfo.CurrentFrame == monster.MData->mAFNum - 1) {
		MonsterAttackPlayer(monsterId, monster._menemy, monster.mHit, monster.mMinDamage, monster.mMaxDamage);
		if (monster._mAi != AI_SNAKE)
			PlayEffect(monster, 0);
	}
	if (IsAnyOf(monster.MType->mtype, MT_NMAGMA, MT_YMAGMA, MT_BMAGMA, MT_WMAGMA) && monster.AnimInfo.CurrentFrame == 8) {
		MonsterAttackPlayer(monsterId, monster._menemy, monster.mHit + 10, monster.mMinDamage - 2, monster.mMaxDamage - 2);
		PlayEffect(monster, 0);
	}
	if (IsAnyOf(monster.MType->mtype, MT_STORM, MT_RSTORM, MT_STORML, MT_MAEL) && monster.AnimInfo.CurrentFrame == 12) {
		MonsterAttackPlayer(monsterId, monster._menemy, monster.mHit - 20, monster.mMinDamage + 4, monster.mMaxDamage + 4);
		PlayEffect(monster, 0);
	}
	if (monster._mAi == AI_SNAKE && monster.AnimInfo.CurrentFrame == 0)
		PlayEffect(monster, 0);
	if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1) {
		M_StartStand(monster, monster._mdir);
		return true;
	}

	return false;
}

bool MonsterRangedAttack(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);
	assert(monster.MData != nullptr);

	if (monster.AnimInfo.CurrentFrame == monster.MData->mAFNum - 1) {
		const auto &missileType = static_cast<missile_id>(monster._mVar1);
		if (missileType != MIS_NULL) {
			int multimissiles = 1;
			if (missileType == MIS_CBOLT)
				multimissiles = 3;
			for (int mi = 0; mi < multimissiles; mi++) {
				AddMissile(
				    monster.position.tile,
				    monster.enemyPosition,
				    monster._mdir,
				    missileType,
				    TARGET_PLAYERS,
				    monsterId,
				    monster._mVar2,
				    0);
			}
		}
		PlayEffect(monster, 0);
	}

	if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1) {
		M_StartStand(monster, monster._mdir);
		return true;
	}

	return false;
}

bool MonsterRangedSpecialAttack(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);
	assert(monster.MData != nullptr);

	if (monster.AnimInfo.CurrentFrame == monster.MData->mAFNum2 - 1 && monster.AnimInfo.TickCounterOfCurrentFrame == 0) {
		if (AddMissile(
		        monster.position.tile,
		        monster.enemyPosition,
		        monster._mdir,
		        static_cast<missile_id>(monster._mVar1),
		        TARGET_PLAYERS,
		        monsterId,
		        monster._mVar3,
		        0)
		    != nullptr) {
			PlayEffect(monster, 3);
		}
	}

	if (monster._mAi == AI_MEGA && monster.AnimInfo.CurrentFrame == monster.MData->mAFNum2 - 1) {
		if (monster._mVar2++ == 0) {
			monster._mFlags |= MFLAG_ALLOW_SPECIAL;
		} else if (monster._mVar2 == 15) {
			monster._mFlags &= ~MFLAG_ALLOW_SPECIAL;
		}
	}

	if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1) {
		M_StartStand(monster, monster._mdir);
		return true;
	}

	return false;
}

bool MonsterSpecialAttack(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);
	assert(monster.MData != nullptr);

	if (monster.AnimInfo.CurrentFrame == monster.MData->mAFNum2 - 1)
		MonsterAttackPlayer(monsterId, monster._menemy, monster.mHit2, monster.mMinDamage2, monster.mMaxDamage2);

	if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1) {
		M_StartStand(monster, monster._mdir);
		return true;
	}

	return false;
}

bool MonsterFadein(Monster &monster)
{
	if (((monster._mFlags & MFLAG_LOCK_ANIMATION) == 0 || monster.AnimInfo.CurrentFrame != 0)
	    && ((monster._mFlags & MFLAG_LOCK_ANIMATION) != 0 || monster.AnimInfo.CurrentFrame != monster.AnimInfo.NumberOfFrames - 1)) {
		return false;
	}

	M_StartStand(monster, monster._mdir);
	monster._mFlags &= ~MFLAG_LOCK_ANIMATION;

	return true;
}

bool MonsterFadeout(Monster &monster)
{
	if (((monster._mFlags & MFLAG_LOCK_ANIMATION) == 0 || monster.AnimInfo.CurrentFrame != 0)
	    && ((monster._mFlags & MFLAG_LOCK_ANIMATION) != 0 || monster.AnimInfo.CurrentFrame != monster.AnimInfo.NumberOfFrames - 1)) {
		return false;
	}

	monster._mFlags &= ~MFLAG_LOCK_ANIMATION;
	monster._mFlags |= MFLAG_HIDDEN;

	M_StartStand(monster, monster._mdir);

	return true;
}

bool MonsterHeal(Monster &monster)
{
	if ((monster._mFlags & MFLAG_NOHEAL) != 0) {
		monster._mFlags &= ~MFLAG_ALLOW_SPECIAL;
		monster._mmode = MonsterMode::SpecialMeleeAttack;
		return false;
	}

	if (monster.AnimInfo.CurrentFrame == 0) {
		monster._mFlags &= ~MFLAG_LOCK_ANIMATION;
		monster._mFlags |= MFLAG_ALLOW_SPECIAL;
		if (monster._mVar1 + monster._mhitpoints < monster._mmaxhp) {
			monster._mhitpoints = monster._mVar1 + monster._mhitpoints;
		} else {
			monster._mhitpoints = monster._mmaxhp;
			monster._mFlags &= ~MFLAG_ALLOW_SPECIAL;
			monster._mmode = MonsterMode::SpecialMeleeAttack;
		}
	}
	return false;
}

bool MonsterTalk(Monster &monster)
{
	M_StartStand(monster, monster._mdir);
	monster._mgoal = MGOAL_TALKING;
	if (effect_is_playing(Speeches[monster.mtalkmsg].sfxnr))
		return false;
	InitQTextMsg(monster.mtalkmsg);
	if (monster._uniqtype - 1 == UMT_GARBUD) {
		if (monster.mtalkmsg == TEXT_GARBUD1) {
			Quests[Q_GARBUD]._qactive = QUEST_ACTIVE;
			Quests[Q_GARBUD]._qlog = true; // BUGFIX: (?) for other quests qactive and qlog go together, maybe this should actually go into the if above (fixed)
		}
		if (monster.mtalkmsg == TEXT_GARBUD2 && (monster._mFlags & MFLAG_QUEST_COMPLETE) == 0) {
			SpawnItem(monster, monster.position.tile + Displacement { 1, 1 }, true);
			monster._mFlags |= MFLAG_QUEST_COMPLETE;
		}
	}
	if (monster._uniqtype - 1 == UMT_ZHAR
	    && monster.mtalkmsg == TEXT_ZHAR1
	    && (monster._mFlags & MFLAG_QUEST_COMPLETE) == 0) {
		Quests[Q_ZHAR]._qactive = QUEST_ACTIVE;
		Quests[Q_ZHAR]._qlog = true;
		CreateTypeItem(monster.position.tile + Displacement { 1, 1 }, false, ItemType::Misc, IMISC_BOOK, true, false);
		monster._mFlags |= MFLAG_QUEST_COMPLETE;
	}
	if (monster._uniqtype - 1 == UMT_SNOTSPIL) {
		if (monster.mtalkmsg == TEXT_BANNER10 && (monster._mFlags & MFLAG_QUEST_COMPLETE) == 0) {
			ObjChangeMap(SetPiece.position.x, SetPiece.position.y, SetPiece.position.x + (SetPiece.size.width / 2) + 2, SetPiece.position.y + (SetPiece.size.height / 2) - 2);
			auto tren = TransVal;
			TransVal = 9;
			DRLG_MRectTrans({ SetPiece.position, { SetPiece.size.width / 2 + 4, SetPiece.size.height / 2 } });
			TransVal = tren;
			Quests[Q_LTBANNER]._qvar1 = 2;
			if (Quests[Q_LTBANNER]._qactive == QUEST_INIT)
				Quests[Q_LTBANNER]._qactive = QUEST_ACTIVE;
			monster._mFlags |= MFLAG_QUEST_COMPLETE;
		}
		if (Quests[Q_LTBANNER]._qvar1 < 2) {
			app_fatal("SS Talk = %i, Flags = %i", monster.mtalkmsg, monster._mFlags);
		}
	}
	if (monster._uniqtype - 1 == UMT_LACHDAN) {
		if (monster.mtalkmsg == TEXT_VEIL9) {
			Quests[Q_VEIL]._qactive = QUEST_ACTIVE;
			Quests[Q_VEIL]._qlog = true;
		}
		if (monster.mtalkmsg == TEXT_VEIL11 && (monster._mFlags & MFLAG_QUEST_COMPLETE) == 0) {
			SpawnUnique(UITEM_STEELVEIL, monster.position.tile + Direction::South);
			monster._mFlags |= MFLAG_QUEST_COMPLETE;
		}
	}
	if (monster._uniqtype - 1 == UMT_WARLORD)
		Quests[Q_WARLORD]._qvar1 = 2;
	if (monster._uniqtype - 1 == UMT_LAZARUS && gbIsMultiplayer) {
		Quests[Q_BETRAYER]._qvar1 = 6;
		monster._mgoal = MGOAL_NORMAL;
		monster._msquelch = UINT8_MAX;
		monster.mtalkmsg = TEXT_NONE;
	}
	return false;
}

bool MonsterGotHit(Monster &monster)
{
	if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1) {
		M_StartStand(monster, monster._mdir);

		return true;
	}

	return false;
}

bool MonsterDeath(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	assert(monster.MType != nullptr);

	monster._mVar1++;
	if (monster.MType->mtype == MT_DIABLO) {
		if (monster.position.tile.x < ViewPosition.x) {
			ViewPosition.x--;
		} else if (monster.position.tile.x > ViewPosition.x) {
			ViewPosition.x++;
		}

		if (monster.position.tile.y < ViewPosition.y) {
			ViewPosition.y--;
		} else if (monster.position.tile.y > ViewPosition.y) {
			ViewPosition.y++;
		}

		if (monster._mVar1 == 140)
			PrepDoEnding();
	} else if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1) {
		if (monster._uniqtype == 0)
			AddCorpse(monster.position.tile, monster.MType->mdeadval, monster._mdir);
		else
			AddCorpse(monster.position.tile, monster._udeadval, monster._mdir);

		dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
		monster._mDelFlag = true;

		M_UpdateLeader(monsterId);
	}
	return false;
}

bool MonsterSpecialStand(Monster &monster)
{
	if (monster.AnimInfo.CurrentFrame == monster.MData->mAFNum2 - 1)
		PlayEffect(monster, 3);

	if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1) {
		M_StartStand(monster, monster._mdir);
		return true;
	}

	return false;
}

bool MonsterDelay(Monster &monster)
{
	monster.ChangeAnimationData(MonsterGraphic::Stand, GetMonsterDirection(monster));
	if (monster._mAi == AI_LAZARUS) {
		if (monster._mVar2 > 8 || monster._mVar2 < 0)
			monster._mVar2 = 8;
	}

	if (monster._mVar2-- == 0) {
		int oFrame = monster.AnimInfo.CurrentFrame;
		M_StartStand(monster, monster._mdir);
		monster.AnimInfo.CurrentFrame = oFrame;
		return true;
	}

	return false;
}

bool MonsterPetrified(Monster &monster)
{
	if (monster._mhitpoints <= 0) {
		dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
		monster._mDelFlag = true;
	}

	return false;
}

int AddSkeleton(Point position, Direction dir, bool inMap)
{
	int j = 0;
	for (int i = 0; i < LevelMonsterTypeCount; i++) {
		if (IsSkel(LevelMonsterTypes[i].mtype))
			j++;
	}

	if (j == 0) {
		return -1;
	}

	int skeltypes = GenerateRnd(j);
	int m = 0;
	for (int i = 0; m < LevelMonsterTypeCount && i <= skeltypes; m++) {
		if (IsSkel(LevelMonsterTypes[m].mtype))
			i++;
	}
	return AddMonster(position, dir, m - 1, inMap);
}

void SpawnSkeleton(Point position, Direction dir)
{
	int skel = AddSkeleton(position, dir, true);
	if (skel != -1)
		StartSpecialStand(Monsters[skel], dir);
}

bool IsLineNotSolid(Point startPoint, Point endPoint)
{
	return LineClear(IsTileNotSolid, startPoint, endPoint);
}

void FollowTheLeader(Monster &monster)
{
	if (monster.leader == 0)
		return;

	if (monster.leaderRelation != LeaderRelation::Leashed)
		return;

	auto &leader = Monsters[monster.leader];
	if (monster._msquelch >= leader._msquelch)
		return;

	monster.position.last = leader.position.tile;
	monster._msquelch = leader._msquelch - 1;
}

void GroupUnity(Monster &monster)
{
	if (monster.leaderRelation == LeaderRelation::None)
		return;

	// Someone with a leaderRelation should have a leader ...
	assert(monster.leader >= 0);
	// And no unique monster would be a minion of someone else!
	assert(monster._uniqtype == 0);

	auto &leader = Monsters[monster.leader];
	if (IsLineNotSolid(monster.position.tile, leader.position.future)) {
		if (monster.leaderRelation == LeaderRelation::Separated
		    && monster.position.tile.WalkingDistance(leader.position.future) < 4) {
			// Reunite the separated monster with the pack
			leader.packsize++;
			monster.leaderRelation = LeaderRelation::Leashed;
		}
	} else if (monster.leaderRelation == LeaderRelation::Leashed) {
		leader.packsize--;
		monster.leaderRelation = LeaderRelation::Separated;
	}

	if (monster.leaderRelation == LeaderRelation::Leashed) {
		if (monster._msquelch > leader._msquelch) {
			leader.position.last = monster.position.tile;
			leader._msquelch = monster._msquelch - 1;
		}
		if (leader._mAi == AI_GARG && (leader._mFlags & MFLAG_ALLOW_SPECIAL) != 0) {
			leader._mFlags &= ~MFLAG_ALLOW_SPECIAL;
			leader._mmode = MonsterMode::SpecialMeleeAttack;
		}
	}
}

bool RandomWalk(int monsterId, Direction md)
{
	Direction mdtemp = md;
	bool ok = DirOK(monsterId, md);
	if (GenerateRnd(2) != 0)
		ok = ok || (md = Left(mdtemp), DirOK(monsterId, md)) || (md = Right(mdtemp), DirOK(monsterId, md));
	else
		ok = ok || (md = Right(mdtemp), DirOK(monsterId, md)) || (md = Left(mdtemp), DirOK(monsterId, md));
	if (GenerateRnd(2) != 0) {
		ok = ok
		    || (md = Right(Right(mdtemp)), DirOK(monsterId, md))
		    || (md = Left(Left(mdtemp)), DirOK(monsterId, md));
	} else {
		ok = ok
		    || (md = Left(Left(mdtemp)), DirOK(monsterId, md))
		    || (md = Right(Right(mdtemp)), DirOK(monsterId, md));
	}
	if (ok)
		M_WalkDir(monsterId, md);
	return ok;
}

bool RandomWalk2(int monsterId, Direction md)
{
	Direction mdtemp = md;
	bool ok = DirOK(monsterId, md); // Can we continue in the same direction
	if (GenerateRnd(2) != 0) {      // Randomly go left or right
		ok = ok || (mdtemp = Left(md), DirOK(monsterId, Left(md))) || (mdtemp = Right(md), DirOK(monsterId, Right(md)));
	} else {
		ok = ok || (mdtemp = Right(md), DirOK(monsterId, Right(md))) || (mdtemp = Left(md), DirOK(monsterId, Left(md)));
	}

	if (ok)
		M_WalkDir(monsterId, mdtemp);

	return ok;
}

/**
 * @brief Check if a tile is affected by a spell we are vunerable to
 */
bool IsTileSafe(const Monster &monster, Point position)
{
	if (!TileContainsMissile(position)) {
		return true;
	}

	bool fearsFire = (monster.mMagicRes & IMMUNE_FIRE) == 0 || monster.MType->mtype == MT_DIABLO;
	bool fearsLightning = (monster.mMagicRes & IMMUNE_LIGHTNING) == 0 || monster.MType->mtype == MT_DIABLO;

	for (auto &missile : Missiles) {
		if (missile.position.tile == position) {
			if (fearsFire && missile._mitype == MIS_FIREWALL) {
				return false;
			}
			if (fearsLightning && missile._mitype == MIS_LIGHTWALL) {
				return false;
			}
		}
	}

	return true;
}

/**
 * @brief Check that the given tile is not currently blocked
 */
bool IsTileAvailable(Point position)
{
	if (dPlayer[position.x][position.y] != 0 || dMonster[position.x][position.y] != 0)
		return false;

	if (!IsTileWalkable(position))
		return false;

	return true;
}

/**
 * @brief If a monster can access the given tile (possibly by opening a door)
 */
bool IsTileAccessible(const Monster &monster, Point position)
{
	if (dPlayer[position.x][position.y] != 0 || dMonster[position.x][position.y] != 0)
		return false;

	if (!IsTileWalkable(position, (monster._mFlags & MFLAG_CAN_OPEN_DOOR) != 0))
		return false;

	return IsTileSafe(monster, position);
}

bool AiPlanWalk(int monsterId)
{
	int8_t path[MaxPathLength];

	/** Maps from walking path step to facing direction. */
	const Direction plr2monst[9] = { Direction::South, Direction::NorthEast, Direction::NorthWest, Direction::SouthEast, Direction::SouthWest, Direction::North, Direction::East, Direction::South, Direction::West };

	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (FindPath([&monster](Point position) { return IsTileAccessible(monster, position); }, monster.position.tile, monster.enemyPosition, path) == 0) {
		return false;
	}

	RandomWalk(monsterId, plr2monst[path[0]]);
	return true;
}

bool DumbWalk(int monsterId, Direction md)
{
	bool ok = DirOK(monsterId, md);
	if (ok)
		M_WalkDir(monsterId, md);

	return ok;
}

Direction Turn(Direction direction, bool turnLeft)
{
	return turnLeft ? Left(direction) : Right(direction);
}

bool RoundWalk(int monsterId, Direction direction, int *dir)
{
	Direction turn45deg = Turn(direction, *dir != 0);
	Direction turn90deg = Turn(turn45deg, *dir != 0);

	if (DirOK(monsterId, turn90deg)) {
		// Turn 90 degrees
		M_WalkDir(monsterId, turn90deg);
		return true;
	}

	if (DirOK(monsterId, turn45deg)) {
		// Only do a small turn
		M_WalkDir(monsterId, turn45deg);
		return true;
	}

	if (DirOK(monsterId, direction)) {
		// Continue straight
		M_WalkDir(monsterId, direction);
		return true;
	}

	// Try 90 degrees in the opposite than desired direction
	*dir = (*dir == 0) ? 1 : 0;
	return RandomWalk(monsterId, Opposite(turn90deg));
}

bool AiPlanPath(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster.MType->mtype != MT_GOLEM) {
		if (monster._msquelch == 0)
			return false;
		if (monster._mmode != MonsterMode::Stand)
			return false;
		if (monster._mgoal != MGOAL_NORMAL && monster._mgoal != MGOAL_MOVE && monster._mgoal != MGOAL_ATTACK2)
			return false;
		if (monster.position.tile.x == 1 && monster.position.tile.y == 0)
			return false;
	}

	bool clear = LineClear(
	    [&monster](Point position) { return IsTileAvailable(monster, position); },
	    monster.position.tile,
	    monster.enemyPosition);
	if (!clear || (monster._pathcount >= 5 && monster._pathcount < 8)) {
		if ((monster._mFlags & MFLAG_CAN_OPEN_DOOR) != 0)
			MonstCheckDoors(monster);
		monster._pathcount++;
		if (monster._pathcount < 5)
			return false;
		if (AiPlanWalk(monsterId))
			return true;
	}

	if (monster.MType->mtype != MT_GOLEM)
		monster._pathcount = 0;

	return false;
}

void AiAvoidance(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	int mx = monster.position.tile.x - fx;
	int my = monster.position.tile.y - fy;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster._msquelch < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	if ((abs(mx) >= 2 || abs(my) >= 2) && monster._msquelch == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[fx][fy]) {
		if (monster._mgoal == MGOAL_MOVE || ((abs(mx) >= 4 || abs(my) >= 4) && GenerateRnd(4) == 0)) {
			if (monster._mgoal != MGOAL_MOVE) {
				monster._mgoalvar1 = 0;
				monster._mgoalvar2 = GenerateRnd(2);
			}
			monster._mgoal = MGOAL_MOVE;
			int dist = std::max(abs(mx), abs(my));
			if ((monster._mgoalvar1++ >= 2 * dist && DirOK(monsterId, md)) || dTransVal[monster.position.tile.x][monster.position.tile.y] != dTransVal[fx][fy]) {
				monster._mgoal = MGOAL_NORMAL;
			} else if (!RoundWalk(monsterId, md, &monster._mgoalvar2)) {
				AiDelay(monster, GenerateRnd(10) + 10);
			}
		}
	} else {
		monster._mgoal = MGOAL_NORMAL;
	}
	if (monster._mgoal == MGOAL_NORMAL) {
		if (abs(mx) >= 2 || abs(my) >= 2) {
			if ((monster._mVar2 > 20 && v < 2 * monster._mint + 28)
			    || (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways)
			        && monster._mVar2 == 0
			        && v < 2 * monster._mint + 78)) {
				RandomWalk(monsterId, md);
			}
		} else if (v < 2 * monster._mint + 23) {
			monster._mdir = md;
			if (IsAnyOf(monster._mAi, AI_GOATMC, AI_GARBUD) && monster._mhitpoints < (monster._mmaxhp / 2) && GenerateRnd(2) != 0)
				StartSpecialAttack(monster);
			else
				StartAttack(monster);
		}
	}

	monster.CheckStandAnimationIsLoaded(md);
}

missile_id GetMissileType(_mai_id ai)
{
	switch (ai) {
	case AI_GOATMC:
		return MIS_ARROW;
	case AI_SUCC:
	case AI_LAZHELP:
		return MIS_FLARE;
	case AI_ACID:
	case AI_ACIDUNIQ:
		return MIS_ACID;
	case AI_FIREBAT:
		return MIS_FIREBOLT;
	case AI_TORCHANT:
		return MIS_FIREBALL;
	case AI_LICH:
		return MIS_LICH;
	case AI_ARCHLICH:
		return MIS_ARCHLICH;
	case AI_PSYCHORB:
		return MIS_PSYCHORB;
	case AI_NECROMORB:
		return MIS_NECROMORB;
	case AI_MAGMA:
		return MIS_MAGMABALL;
	case AI_STORM:
		return MIS_LIGHTCTRL2;
	case AI_DIABLO:
		return MIS_DIABAPOCA;
	case AI_BONEDEMON:
		return MIS_BONEDEMON;
	default:
		return MIS_ARROW;
	}
}

void AiRanged(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}

	if (monster._msquelch == UINT8_MAX || (monster._mFlags & MFLAG_TARGETS_MONSTER) != 0) {
		int fx = monster.enemyPosition.x;
		int fy = monster.enemyPosition.y;
		int mx = monster.position.tile.x - fx;
		int my = monster.position.tile.y - fy;
		Direction md = GetMonsterDirection(monster);
		if (monster._msquelch < UINT8_MAX)
			MonstCheckDoors(monster);
		monster._mdir = md;
		if (static_cast<MonsterMode>(monster._mVar1) == MonsterMode::RangedAttack) {
			AiDelay(monster, GenerateRnd(20));
		} else if (abs(mx) < 4 && abs(my) < 4) {
			if (GenerateRnd(100) < 10 * (monster._mint + 7))
				RandomWalk(monsterId, Opposite(md));
		}
		if (monster._mmode == MonsterMode::Stand) {
			if (LineClearMissile(monster.position.tile, { fx, fy })) {
				missile_id missileType = GetMissileType(monster._mAi);
				if (monster._mAi == AI_ACIDUNIQ)
					StartRangedSpecialAttack(monster, missileType, 4);
				else
					StartRangedAttack(monster, missileType, 4);
			} else {
				monster.CheckStandAnimationIsLoaded(md);
			}
		}
		return;
	}

	if (monster._msquelch != 0) {
		int fx = monster.position.last.x;
		int fy = monster.position.last.y;
		Direction md = GetDirection(monster.position.tile, { fx, fy });
		RandomWalk(monsterId, md);
	}
}

void AiRangedAvoidance(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	int mx = monster.position.tile.x - fx;
	int my = monster.position.tile.y - fy;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (IsAnyOf(monster._mAi, AI_MAGMA, AI_STORM, AI_BONEDEMON) && monster._msquelch < UINT8_MAX)
		MonstCheckDoors(monster);
	int lessmissiles = (monster._mAi == AI_ACID) ? 1 : 0;
	int dam = (monster._mAi == AI_DIABLO) ? 40 : 4;
	missile_id missileType = GetMissileType(monster._mAi);
	int v = GenerateRnd(10000);
	int dist = std::max(abs(mx), abs(my));
	if (dist >= 2 && monster._msquelch == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[fx][fy]) {
		if (monster._mgoal == MGOAL_MOVE || (dist >= 3 && GenerateRnd(4 << lessmissiles) == 0)) {
			if (monster._mgoal != MGOAL_MOVE) {
				monster._mgoalvar1 = 0;
				monster._mgoalvar2 = GenerateRnd(2);
			}
			monster._mgoal = MGOAL_MOVE;
			if (monster._mgoalvar1++ >= 2 * dist && DirOK(monsterId, md)) {
				monster._mgoal = MGOAL_NORMAL;
			} else if (v < (500 * (monster._mint + 1) >> lessmissiles)
			    && (LineClearMissile(monster.position.tile, { fx, fy }))) {
				StartRangedSpecialAttack(monster, missileType, dam);
			} else {
				RoundWalk(monsterId, md, &monster._mgoalvar2);
			}
		}
	} else {
		monster._mgoal = MGOAL_NORMAL;
	}
	if (monster._mgoal == MGOAL_NORMAL) {
		if (((dist >= 3 && v < ((500 * (monster._mint + 2)) >> lessmissiles))
		        || v < ((500 * (monster._mint + 1)) >> lessmissiles))
		    && LineClearMissile(monster.position.tile, { fx, fy })) {
			StartRangedSpecialAttack(monster, missileType, dam);
		} else if (dist >= 2) {
			v = GenerateRnd(100);
			if (v < 1000 * (monster._mint + 5)
			    || (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways) && monster._mVar2 == 0 && v < 1000 * (monster._mint + 8))) {
				RandomWalk(monsterId, md);
			}
		} else if (v < 1000 * (monster._mint + 6)) {
			monster._mdir = md;
			StartAttack(monster);
		}
	}
	if (monster._mmode == MonsterMode::Stand) {
		AiDelay(monster, GenerateRnd(10) + 5);
	}
}

void ZombieAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}

	if (!IsTileVisible(monster.position.tile)) {
		return;
	}

	if (GenerateRnd(100) < 2 * monster._mint + 10) {
		int dist = monster.enemyPosition.WalkingDistance(monster.position.tile);
		if (dist >= 2) {
			if (dist >= 2 * monster._mint + 4) {
				Direction md = monster._mdir;
				if (GenerateRnd(100) < 2 * monster._mint + 20) {
					md = static_cast<Direction>(GenerateRnd(8));
				}
				DumbWalk(monsterId, md);
			} else {
				RandomWalk(monsterId, GetMonsterDirection(monster));
			}
		} else {
			StartAttack(monster);
		}
	}

	monster.CheckStandAnimationIsLoaded(monster._mdir);
}

void OverlordAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int mx = monster.position.tile.x - monster.enemyPosition.x;
	int my = monster.position.tile.y - monster.enemyPosition.y;
	Direction md = GetMonsterDirection(monster);
	monster._mdir = md;
	int v = GenerateRnd(100);
	if (abs(mx) >= 2 || abs(my) >= 2) {
		if ((monster._mVar2 > 20 && v < 4 * monster._mint + 20)
		    || (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways)
		        && monster._mVar2 == 0
		        && v < 4 * monster._mint + 70)) {
			RandomWalk(monsterId, md);
		}
	} else if (v < 4 * monster._mint + 15) {
		StartAttack(monster);
	} else if (v < 4 * monster._mint + 20) {
		StartSpecialAttack(monster);
	}

	monster.CheckStandAnimationIsLoaded(md);
}

void SkeletonAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int x = monster.position.tile.x - monster.enemyPosition.x;
	int y = monster.position.tile.y - monster.enemyPosition.y;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	monster._mdir = md;
	if (abs(x) >= 2 || abs(y) >= 2) {
		if (static_cast<MonsterMode>(monster._mVar1) == MonsterMode::Delay || (GenerateRnd(100) >= 35 - 4 * monster._mint)) {
			RandomWalk(monsterId, md);
		} else {
			AiDelay(monster, 15 - 2 * monster._mint + GenerateRnd(10));
		}
	} else {
		if (static_cast<MonsterMode>(monster._mVar1) == MonsterMode::Delay || (GenerateRnd(100) < 2 * monster._mint + 20)) {
			StartAttack(monster);
		} else {
			AiDelay(monster, 2 * (5 - monster._mint) + GenerateRnd(10));
		}
	}

	monster.CheckStandAnimationIsLoaded(md);
}

void SkeletonBowAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int mx = monster.position.tile.x - monster.enemyPosition.x;
	int my = monster.position.tile.y - monster.enemyPosition.y;

	Direction md = GetMonsterDirection(monster);
	monster._mdir = md;
	int v = GenerateRnd(100);

	bool walking = false;

	if (abs(mx) < 4 && abs(my) < 4) {
		if ((monster._mVar2 > 20 && v < 2 * monster._mint + 13)
		    || (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways)
		        && monster._mVar2 == 0
		        && v < 2 * monster._mint + 63)) {
			walking = DumbWalk(monsterId, Opposite(md));
		}
	}

	if (!walking) {
		if (GenerateRnd(100) < 2 * monster._mint + 3) {
			if (LineClearMissile(monster.position.tile, monster.enemyPosition))
				StartRangedAttack(monster, MIS_ARROW, 4);
		}
	}

	monster.CheckStandAnimationIsLoaded(md);
}

std::optional<Point> ScavengerFindCorpse(const Monster &scavenger)
{
	bool lowToHigh = GenerateRnd(2) != 0;
	int first = lowToHigh ? -4 : 4;
	int last = lowToHigh ? 4 : -4;
	int increment = lowToHigh ? 1 : -1;

	for (int y = first; y <= last; y += increment) {
		for (int x = first; x <= last; x += increment) {
			Point position = scavenger.position.tile + Displacement { x, y };
			// BUGFIX: incorrect check of offset against limits of the dungeon (fixed)
			if (!InDungeonBounds(position))
				continue;
			if (dCorpse[position.x][position.y] == 0)
				continue;
			if (!IsLineNotSolid(scavenger.position.tile, position))
				continue;
			return position;
		}
	}
	return {};
}

void ScavengerAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand)
		return;
	if (monster._mhitpoints < (monster._mmaxhp / 2) && monster._mgoal != MGOAL_HEALING) {
		if (monster.leaderRelation != LeaderRelation::None) {
			if (monster.leaderRelation == LeaderRelation::Leashed)
				Monsters[monster.leader].packsize--;
			monster.leaderRelation = LeaderRelation::None;
		}
		monster._mgoal = MGOAL_HEALING;
		monster._mgoalvar3 = 10;
	}
	if (monster._mgoal == MGOAL_HEALING && monster._mgoalvar3 != 0) {
		monster._mgoalvar3--;
		if (dCorpse[monster.position.tile.x][monster.position.tile.y] != 0) {
			StartEating(monster);
			if ((monster._mFlags & MFLAG_NOHEAL) == 0) {
				if (gbIsHellfire) {
					int mMaxHP = monster._mmaxhp; // BUGFIX use _mmaxhp or we loose health when difficulty isn't normal (fixed)
					monster._mhitpoints += mMaxHP / 8;
					if (monster._mhitpoints > monster._mmaxhp)
						monster._mhitpoints = monster._mmaxhp;
					if (monster._mgoalvar3 <= 0 || monster._mhitpoints == monster._mmaxhp)
						dCorpse[monster.position.tile.x][monster.position.tile.y] = 0;
				} else {
					monster._mhitpoints += 64;
				}
			}
			int targetHealth = monster._mmaxhp;
			if (!gbIsHellfire)
				targetHealth = (monster._mmaxhp / 2) + (monster._mmaxhp / 4);
			if (monster._mhitpoints >= targetHealth) {
				monster._mgoal = MGOAL_NORMAL;
				monster._mgoalvar1 = 0;
				monster._mgoalvar2 = 0;
			}
		} else {
			if (monster._mgoalvar1 == 0) {
				std::optional<Point> position = ScavengerFindCorpse(monster);
				if (position) {
					monster._mgoalvar1 = position->x + 1;
					monster._mgoalvar2 = position->y + 1;
				}
			}
			if (monster._mgoalvar1 != 0) {
				int x = monster._mgoalvar1 - 1;
				int y = monster._mgoalvar2 - 1;
				monster._mdir = GetDirection(monster.position.tile, { x, y });
				RandomWalk(monsterId, monster._mdir);
			}
		}
	}

	if (monster._mmode == MonsterMode::Stand)
		SkeletonAi(monsterId);
}

void RhinoAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	int mx = monster.position.tile.x - fx;
	int my = monster.position.tile.y - fy;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster._msquelch < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	int dist = std::max(abs(mx), abs(my));
	if (dist >= 2) {
		if (monster._mgoal == MGOAL_MOVE || (dist >= 5 && GenerateRnd(4) != 0)) {
			if (monster._mgoal != MGOAL_MOVE) {
				monster._mgoalvar1 = 0;
				monster._mgoalvar2 = GenerateRnd(2);
			}
			monster._mgoal = MGOAL_MOVE;
			if (monster._mgoalvar1++ >= 2 * dist || dTransVal[monster.position.tile.x][monster.position.tile.y] != dTransVal[fx][fy]) {
				monster._mgoal = MGOAL_NORMAL;
			} else if (!RoundWalk(monsterId, md, &monster._mgoalvar2)) {
				AiDelay(monster, GenerateRnd(10) + 10);
			}
		}
	} else {
		monster._mgoal = MGOAL_NORMAL;
	}
	if (monster._mgoal == MGOAL_NORMAL) {
		if (dist >= 5
		    && v < 2 * monster._mint + 43
		    && LineClear([&monster](Point position) { return IsTileAvailable(monster, position); }, monster.position.tile, { fx, fy })) {
			if (AddMissile(monster.position.tile, { fx, fy }, md, MIS_RHINO, TARGET_PLAYERS, monsterId, 0, 0) != nullptr) {
				if (monster.MData->snd_special)
					PlayEffect(monster, 3);
				dMonster[monster.position.tile.x][monster.position.tile.y] = -(monsterId + 1);
				monster._mmode = MonsterMode::Charge;
			}
		} else {
			if (dist >= 2) {
				v = GenerateRnd(100);
				if (v >= 2 * monster._mint + 33
				    && (IsNoneOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways)
				        || monster._mVar2 != 0
				        || v >= 2 * monster._mint + 83)) {
					AiDelay(monster, GenerateRnd(10) + 10);
				} else {
					RandomWalk(monsterId, md);
				}
			} else if (v < 2 * monster._mint + 28) {
				monster._mdir = md;
				StartAttack(monster);
			}
		}
	}

	monster.CheckStandAnimationIsLoaded(monster._mdir);
}

void FallenAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mgoal == MGOAL_ATTACK2) {
		if (monster._mgoalvar1 != 0)
			monster._mgoalvar1--;
		else
			monster._mgoal = MGOAL_NORMAL;
	}
	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	if (monster._mgoal == MGOAL_RETREAT) {
		if (monster._mgoalvar1-- == 0) {
			monster._mgoal = MGOAL_NORMAL;
			M_StartStand(monster, Opposite(static_cast<Direction>(monster._mgoalvar2)));
		}
	}

	if (monster.AnimInfo.CurrentFrame == monster.AnimInfo.NumberOfFrames - 1) {
		if (GenerateRnd(4) != 0) {
			return;
		}
		if ((monster._mFlags & MFLAG_NOHEAL) == 0) {
			StartSpecialStand(monster, monster._mdir);
			if (monster._mmaxhp - (2 * monster._mint + 2) >= monster._mhitpoints)
				monster._mhitpoints += 2 * monster._mint + 2;
			else
				monster._mhitpoints = monster._mmaxhp;
		}
		int rad = 2 * monster._mint + 4;
		for (int y = -rad; y <= rad; y++) {
			for (int x = -rad; x <= rad; x++) {
				int xpos = monster.position.tile.x + x;
				int ypos = monster.position.tile.y + y;
				// BUGFIX: incorrect check of offset against limits of the dungeon (fixed)
				if (InDungeonBounds({ xpos, ypos })) {
					int m = dMonster[xpos][ypos];
					if (m <= 0)
						continue;

					auto &otherMonster = Monsters[m - 1];
					if (otherMonster._mAi != AI_FALLEN)
						continue;

					otherMonster._mgoal = MGOAL_ATTACK2;
					otherMonster._mgoalvar1 = 30 * monster._mint + 105;
				}
			}
		}
	} else if (monster._mgoal == MGOAL_RETREAT) {
		monster._mdir = static_cast<Direction>(monster._mgoalvar2);
		RandomWalk(monsterId, monster._mdir);
	} else if (monster._mgoal == MGOAL_ATTACK2) {
		int xpos = monster.position.tile.x - monster.enemyPosition.x;
		int ypos = monster.position.tile.y - monster.enemyPosition.y;
		if (abs(xpos) < 2 && abs(ypos) < 2)
			StartAttack(monster);
		else
			RandomWalk(monsterId, GetMonsterDirection(monster));
	} else
		SkeletonAi(monsterId);
}

void LeoricAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	int mx = monster.position.tile.x - fx;
	int my = monster.position.tile.y - fy;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster._msquelch < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	int dist = std::max(abs(mx), abs(my));
	if (dist >= 2 && monster._msquelch == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[fx][fy]) {
		if (monster._mgoal == MGOAL_MOVE || ((abs(mx) >= 3 || abs(my) >= 3) && GenerateRnd(4) == 0)) {
			if (monster._mgoal != MGOAL_MOVE) {
				monster._mgoalvar1 = 0;
				monster._mgoalvar2 = GenerateRnd(2);
			}
			monster._mgoal = MGOAL_MOVE;
			if ((monster._mgoalvar1++ >= 2 * dist && DirOK(monsterId, md)) || dTransVal[monster.position.tile.x][monster.position.tile.y] != dTransVal[fx][fy]) {
				monster._mgoal = MGOAL_NORMAL;
			} else if (!RoundWalk(monsterId, md, &monster._mgoalvar2)) {
				AiDelay(monster, GenerateRnd(10) + 10);
			}
		}
	} else {
		monster._mgoal = MGOAL_NORMAL;
	}
	if (monster._mgoal == MGOAL_NORMAL) {
		if (!gbIsMultiplayer
		    && ((dist >= 3 && v < 4 * monster._mint + 35) || v < 6)
		    && LineClearMissile(monster.position.tile, { fx, fy })) {
			Point newPosition = monster.position.tile + md;
			if (IsTileAvailable(monster, newPosition) && ActiveMonsterCount < MaxMonsters) {
				SpawnSkeleton(newPosition, md);
				StartSpecialStand(monster, md);
			}
		} else {
			if (dist >= 2) {
				v = GenerateRnd(100);
				if (v >= monster._mint + 25
				    && (IsNoneOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways) || monster._mVar2 != 0 || (v >= monster._mint + 75))) {
					AiDelay(monster, GenerateRnd(10) + 10);
				} else {
					RandomWalk(monsterId, md);
				}
			} else if (v < monster._mint + 20) {
				monster._mdir = md;
				StartAttack(monster);
			}
		}
	}

	monster.CheckStandAnimationIsLoaded(md);
}

void BatAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int xd = monster.position.tile.x - monster.enemyPosition.x;
	int yd = monster.position.tile.y - monster.enemyPosition.y;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	monster._mdir = md;
	int v = GenerateRnd(100);
	if (monster._mgoal == MGOAL_RETREAT) {
		if (monster._mgoalvar1 == 0) {
			RandomWalk(monsterId, Opposite(md));
			monster._mgoalvar1++;
		} else {
			if (GenerateRnd(2) != 0)
				RandomWalk(monsterId, Left(md));
			else
				RandomWalk(monsterId, Right(md));
			monster._mgoal = MGOAL_NORMAL;
		}
		return;
	}

	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	if (monster.MType->mtype == MT_GLOOM
	    && (abs(xd) >= 5 || abs(yd) >= 5)
	    && v < 4 * monster._mint + 33
	    && LineClear([&monster](Point position) { return IsTileAvailable(monster, position); }, monster.position.tile, { fx, fy })) {
		if (AddMissile(monster.position.tile, { fx, fy }, md, MIS_RHINO, TARGET_PLAYERS, monsterId, 0, 0) != nullptr) {
			dMonster[monster.position.tile.x][monster.position.tile.y] = -(monsterId + 1);
			monster._mmode = MonsterMode::Charge;
		}
	} else if (abs(xd) >= 2 || abs(yd) >= 2) {
		if ((monster._mVar2 > 20 && v < monster._mint + 13)
		    || (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways)
		        && monster._mVar2 == 0
		        && v < monster._mint + 63)) {
			RandomWalk(monsterId, md);
		}
	} else if (v < 4 * monster._mint + 8) {
		StartAttack(monster);
		monster._mgoal = MGOAL_RETREAT;
		monster._mgoalvar1 = 0;
		if (monster.MType->mtype == MT_FAMILIAR) {
			AddMissile(monster.enemyPosition, { monster.enemyPosition.x + 1, 0 }, Direction::South, MIS_LIGHTNING, TARGET_PLAYERS, monsterId, GenerateRnd(10) + 1, 0);
		}
	}

	monster.CheckStandAnimationIsLoaded(md);
}

void GargoyleAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	int dx = monster.position.tile.x - monster.position.last.x;
	int dy = monster.position.tile.y - monster.position.last.y;
	Direction md = GetMonsterDirection(monster);
	if (monster._msquelch != 0 && (monster._mFlags & MFLAG_ALLOW_SPECIAL) != 0) {
		UpdateEnemy(monster);
		int mx = monster.position.tile.x - monster.enemyPosition.x;
		int my = monster.position.tile.y - monster.enemyPosition.y;
		if (abs(mx) < monster._mint + 2 && abs(my) < monster._mint + 2) {
			monster._mFlags &= ~MFLAG_ALLOW_SPECIAL;
		}
		return;
	}

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	if (monster._mhitpoints < (monster._mmaxhp / 2))
		if ((monster._mFlags & MFLAG_NOHEAL) == 0)
			monster._mgoal = MGOAL_RETREAT;
	if (monster._mgoal == MGOAL_RETREAT) {
		if (abs(dx) >= monster._mint + 2 || abs(dy) >= monster._mint + 2) {
			monster._mgoal = MGOAL_NORMAL;
			StartHeal(monster);
		} else if (!RandomWalk(monsterId, Opposite(md))) {
			monster._mgoal = MGOAL_NORMAL;
		}
	}
	AiAvoidance(monsterId);
}

void ButcherAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int mx = monster.position.tile.x;
	int my = monster.position.tile.y;
	int x = mx - monster.enemyPosition.x;
	int y = my - monster.enemyPosition.y;

	Direction md = GetDirection({ mx, my }, monster.position.last);
	monster._mdir = md;

	if (abs(x) >= 2 || abs(y) >= 2)
		RandomWalk(monsterId, md);
	else
		StartAttack(monster);

	monster.CheckStandAnimationIsLoaded(md);
}

void SneakAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}
	int mx = monster.position.tile.x;
	int my = monster.position.tile.y;
	if (dLight[mx][my] == LightsMax) {
		return;
	}
	mx -= monster.enemyPosition.x;
	my -= monster.enemyPosition.y;

	int dist = 5 - monster._mint;
	if (static_cast<MonsterMode>(monster._mVar1) == MonsterMode::HitRecovery) {
		monster._mgoal = MGOAL_RETREAT;
		monster._mgoalvar1 = 0;
	} else if (abs(mx) >= dist + 3 || abs(my) >= dist + 3 || monster._mgoalvar1 > 8) {
		monster._mgoal = MGOAL_NORMAL;
		monster._mgoalvar1 = 0;
	}
	Direction md = GetMonsterDirection(monster);
	if (monster._mgoal == MGOAL_RETREAT && (monster._mFlags & MFLAG_NO_ENEMY) == 0) {
		if ((monster._mFlags & MFLAG_TARGETS_MONSTER) != 0)
			md = GetDirection(monster.position.tile, Monsters[monster._menemy].position.tile);
		else
			md = GetDirection(monster.position.tile, Players[monster._menemy].position.last);
		md = Opposite(md);
		if (monster.MType->mtype == MT_UNSEEN) {
			if (GenerateRnd(2) != 0)
				md = Left(md);
			else
				md = Right(md);
		}
	}
	monster._mdir = md;
	int v = GenerateRnd(100);
	if (abs(mx) < dist && abs(my) < dist && (monster._mFlags & MFLAG_HIDDEN) != 0) {
		StartFadein(monster, md, false);
	} else {
		if ((abs(mx) >= dist + 1 || abs(my) >= dist + 1) && (monster._mFlags & MFLAG_HIDDEN) == 0) {
			StartFadeout(monster, md, true);
		} else {
			if (monster._mgoal == MGOAL_RETREAT
			    || ((abs(mx) >= 2 || abs(my) >= 2) && ((monster._mVar2 > 20 && v < 4 * monster._mint + 14) || (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways) && monster._mVar2 == 0 && v < 4 * monster._mint + 64)))) {
				monster._mgoalvar1++;
				RandomWalk(monsterId, md);
			}
		}
	}
	if (monster._mmode == MonsterMode::Stand) {
		if (abs(mx) >= 2 || abs(my) >= 2 || v >= 4 * monster._mint + 10)
			monster.ChangeAnimationData(MonsterGraphic::Stand);
		else
			StartAttack(monster);
	}
}

void GharbadAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);

	if (monster.mtalkmsg >= TEXT_GARBUD1
	    && monster.mtalkmsg <= TEXT_GARBUD3
	    && !IsTileVisible(monster.position.tile)
	    && monster._mgoal == MGOAL_TALKING) {
		monster._mgoal = MGOAL_INQUIRING;
		switch (monster.mtalkmsg) {
		case TEXT_GARBUD1:
			monster.mtalkmsg = TEXT_GARBUD2;
			break;
		case TEXT_GARBUD2:
			monster.mtalkmsg = TEXT_GARBUD3;
			break;
		case TEXT_GARBUD3:
			monster.mtalkmsg = TEXT_GARBUD4;
			break;
		default:
			break;
		}
	}

	if (IsTileVisible(monster.position.tile)) {
		if (monster.mtalkmsg == TEXT_GARBUD4) {
			if (!effect_is_playing(USFX_GARBUD4) && monster._mgoal == MGOAL_TALKING) {
				monster._mgoal = MGOAL_NORMAL;
				monster._msquelch = UINT8_MAX;
				monster.mtalkmsg = TEXT_NONE;
			}
		}
	}

	if (monster._mgoal == MGOAL_NORMAL || monster._mgoal == MGOAL_MOVE)
		AiAvoidance(monsterId);

	monster.CheckStandAnimationIsLoaded(md);
}

void SnotSpilAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);

	if (monster.mtalkmsg == TEXT_BANNER10 && !IsTileVisible(monster.position.tile) && monster._mgoal == MGOAL_TALKING) {
		monster.mtalkmsg = TEXT_BANNER11;
		monster._mgoal = MGOAL_INQUIRING;
	}

	if (monster.mtalkmsg == TEXT_BANNER11 && Quests[Q_LTBANNER]._qvar1 == 3) {
		monster.mtalkmsg = TEXT_NONE;
		monster._mgoal = MGOAL_NORMAL;
	}

	if (IsTileVisible(monster.position.tile)) {
		if (monster.mtalkmsg == TEXT_BANNER12) {
			if (!effect_is_playing(USFX_SNOT3) && monster._mgoal == MGOAL_TALKING) {
				ObjChangeMap(SetPiece.position.x, SetPiece.position.y, SetPiece.position.x + SetPiece.size.width + 1, SetPiece.position.y + SetPiece.size.height + 1);
				Quests[Q_LTBANNER]._qvar1 = 3;
				RedoPlayerVision();
				monster._msquelch = UINT8_MAX;
				monster.mtalkmsg = TEXT_NONE;
				monster._mgoal = MGOAL_NORMAL;
			}
		}
		if (Quests[Q_LTBANNER]._qvar1 == 3) {
			if (monster._mgoal == MGOAL_NORMAL || monster._mgoal == MGOAL_ATTACK2)
				FallenAi(monsterId);
		}
	}

	monster.CheckStandAnimationIsLoaded(md);
}

void SnakeAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	char pattern[6] = { 1, 1, 0, -1, -1, 0 };
	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0)
		return;
	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	int mx = monster.position.tile.x - fx;
	int my = monster.position.tile.y - fy;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	monster._mdir = md;
	if (abs(mx) >= 2 || abs(my) >= 2) {
		if (abs(mx) < 3 && abs(my) < 3 && LineClear([&monster](Point position) { return IsTileAvailable(monster, position); }, monster.position.tile, { fx, fy }) && static_cast<MonsterMode>(monster._mVar1) != MonsterMode::Charge) {
			if (AddMissile(monster.position.tile, { fx, fy }, md, MIS_RHINO, TARGET_PLAYERS, monsterId, 0, 0) != nullptr) {
				PlayEffect(monster, 0);
				dMonster[monster.position.tile.x][monster.position.tile.y] = -(monsterId + 1);
				monster._mmode = MonsterMode::Charge;
			}
		} else if (static_cast<MonsterMode>(monster._mVar1) == MonsterMode::Delay || GenerateRnd(100) >= 35 - 2 * monster._mint) {
			if (pattern[monster._mgoalvar1] == -1)
				md = Left(md);
			else if (pattern[monster._mgoalvar1] == 1)
				md = Right(md);

			monster._mgoalvar1++;
			if (monster._mgoalvar1 > 5)
				monster._mgoalvar1 = 0;

			Direction targetDirection = static_cast<Direction>(monster._mgoalvar2);
			if (md != targetDirection) {
				int drift = static_cast<int>(md) - monster._mgoalvar2;
				if (drift < 0)
					drift += 8;

				if (drift < 4)
					md = Right(targetDirection);
				else if (drift > 4)
					md = Left(targetDirection);
				monster._mgoalvar2 = static_cast<int>(md);
			}

			if (!DumbWalk(monsterId, md))
				RandomWalk2(monsterId, monster._mdir);
		} else {
			AiDelay(monster, 15 - monster._mint + GenerateRnd(10));
		}
	} else {
		if (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::Delay, MonsterMode::Charge)
		    || (GenerateRnd(100) < monster._mint + 20)) {
			StartAttack(monster);
		} else
			AiDelay(monster, 10 - monster._mint + GenerateRnd(10));
	}

	monster.CheckStandAnimationIsLoaded(monster._mdir);
}

void CounselorAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}
	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	int mx = monster.position.tile.x - fx;
	int my = monster.position.tile.y - fy;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster._msquelch < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	if (monster._mgoal == MGOAL_RETREAT) {
		if (monster._mgoalvar1++ <= 3)
			RandomWalk(monsterId, Opposite(md));
		else {
			monster._mgoal = MGOAL_NORMAL;
			StartFadein(monster, md, true);
		}
	} else if (monster._mgoal == MGOAL_MOVE) {
		int dist = std::max(abs(mx), abs(my));
		if (dist >= 2 && monster._msquelch == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[fx][fy]) {
			if (monster._mgoalvar1++ < 2 * dist || !DirOK(monsterId, md)) {
				RoundWalk(monsterId, md, &monster._mgoalvar2);
			} else {
				monster._mgoal = MGOAL_NORMAL;
				StartFadein(monster, md, true);
			}
		} else {
			monster._mgoal = MGOAL_NORMAL;
			StartFadein(monster, md, true);
		}
	} else if (monster._mgoal == MGOAL_NORMAL) {
		if (abs(mx) >= 2 || abs(my) >= 2) {
			if (v < 5 * (monster._mint + 10) && LineClearMissile(monster.position.tile, { fx, fy })) {
				constexpr missile_id MissileTypes[4] = { MIS_FIREBOLT, MIS_CBOLT, MIS_LIGHTCTRL, MIS_FIREBALL };
				StartRangedAttack(monster, MissileTypes[monster._mint], monster.mMinDamage + GenerateRnd(monster.mMaxDamage - monster.mMinDamage + 1));
			} else if (GenerateRnd(100) < 30) {
				monster._mgoal = MGOAL_MOVE;
				monster._mgoalvar1 = 0;
				StartFadeout(monster, md, false);
			} else
				AiDelay(monster, GenerateRnd(10) + 2 * (5 - monster._mint));
		} else {
			monster._mdir = md;
			if (monster._mhitpoints < (monster._mmaxhp / 2)) {
				monster._mgoal = MGOAL_RETREAT;
				monster._mgoalvar1 = 0;
				StartFadeout(monster, md, false);
			} else if (static_cast<MonsterMode>(monster._mVar1) == MonsterMode::Delay
			    || GenerateRnd(100) < 2 * monster._mint + 20) {
				StartRangedAttack(monster, MIS_NULL, 0);
				AddMissile(monster.position.tile, { 0, 0 }, monster._mdir, MIS_FLASH, TARGET_PLAYERS, monsterId, 4, 0);
				AddMissile(monster.position.tile, { 0, 0 }, monster._mdir, MIS_FLASH2, TARGET_PLAYERS, monsterId, 4, 0);
			} else
				AiDelay(monster, GenerateRnd(10) + 2 * (5 - monster._mint));
		}
	}
	if (monster._mmode == MonsterMode::Stand) {
		AiDelay(monster, GenerateRnd(10) + 5);
	}
}

void ZharAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);
	if (monster.mtalkmsg == TEXT_ZHAR1 && !IsTileVisible(monster.position.tile) && monster._mgoal == MGOAL_TALKING) {
		monster.mtalkmsg = TEXT_ZHAR2;
		monster._mgoal = MGOAL_INQUIRING;
	}

	if (IsTileVisible(monster.position.tile)) {
		if (monster.mtalkmsg == TEXT_ZHAR2) {
			if (!effect_is_playing(USFX_ZHAR2) && monster._mgoal == MGOAL_TALKING) {
				monster._msquelch = UINT8_MAX;
				monster.mtalkmsg = TEXT_NONE;
				monster._mgoal = MGOAL_NORMAL;
			}
		}
	}

	if (monster._mgoal == MGOAL_NORMAL || monster._mgoal == MGOAL_RETREAT || monster._mgoal == MGOAL_MOVE)
		CounselorAi(monsterId);

	monster.CheckStandAnimationIsLoaded(md);
}

void MegaAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	int mx = monster.position.tile.x - monster.enemyPosition.x;
	int my = monster.position.tile.y - monster.enemyPosition.y;
	if (abs(mx) >= 5 || abs(my) >= 5) {
		SkeletonAi(monsterId);
		return;
	}

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	mx = monster.position.tile.x - fx;
	my = monster.position.tile.y - fy;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster._msquelch < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	int dist = std::max(abs(mx), abs(my));
	if (dist >= 2 && monster._msquelch == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[fx][fy]) {
		if (monster._mgoal == MGOAL_MOVE || dist >= 3) {
			if (monster._mgoal != MGOAL_MOVE) {
				monster._mgoalvar1 = 0;
				monster._mgoalvar2 = GenerateRnd(2);
			}
			monster._mgoal = MGOAL_MOVE;
			monster._mgoalvar3 = 4;
			if (monster._mgoalvar1++ < 2 * dist || !DirOK(monsterId, md)) {
				if (v < 5 * (monster._mint + 16))
					RoundWalk(monsterId, md, &monster._mgoalvar2);
			} else
				monster._mgoal = MGOAL_NORMAL;
		}
	} else {
		monster._mgoal = MGOAL_NORMAL;
	}
	if (monster._mgoal == MGOAL_NORMAL) {
		if (((dist >= 3 && v < 5 * (monster._mint + 2)) || v < 5 * (monster._mint + 1) || monster._mgoalvar3 == 4) && LineClearMissile(monster.position.tile, { fx, fy })) {
			StartRangedSpecialAttack(monster, MIS_FLAMEC, 0);
		} else if (dist >= 2) {
			v = GenerateRnd(100);
			if (v < 2 * (5 * monster._mint + 25)
			    || (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways)
			        && monster._mVar2 == 0
			        && v < 2 * (5 * monster._mint + 40))) {
				RandomWalk(monsterId, md);
			}
		} else {
			if (GenerateRnd(100) < 10 * (monster._mint + 4)) {
				monster._mdir = md;
				if (GenerateRnd(2) != 0)
					StartAttack(monster);
				else
					StartRangedSpecialAttack(monster, MIS_FLAMEC, 0);
			}
		}
		monster._mgoalvar3 = 1;
	}
	if (monster._mmode == MonsterMode::Stand) {
		AiDelay(monster, GenerateRnd(10) + 5);
	}
}

void LazarusAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);
	if (IsTileVisible(monster.position.tile)) {
		if (!gbIsMultiplayer) {
			Player &myPlayer = *MyPlayer;
			if (monster.mtalkmsg == TEXT_VILE13 && monster._mgoal == MGOAL_INQUIRING && myPlayer.position.tile == Point { 35, 46 }) {
				PlayInGameMovie("gendata\\fprst3.smk");
				monster._mmode = MonsterMode::Talk;
				Quests[Q_BETRAYER]._qvar1 = 5;
			}

			if (monster.mtalkmsg == TEXT_VILE13 && !effect_is_playing(USFX_LAZ1) && monster._mgoal == MGOAL_TALKING) {
				ObjChangeMap(1, 18, 20, 24);
				RedoPlayerVision();
				Quests[Q_BETRAYER]._qvar1 = 6;
				monster._mgoal = MGOAL_NORMAL;
				monster._msquelch = UINT8_MAX;
				monster.mtalkmsg = TEXT_NONE;
			}
		}

		if (gbIsMultiplayer && monster.mtalkmsg == TEXT_VILE13 && monster._mgoal == MGOAL_INQUIRING && Quests[Q_BETRAYER]._qvar1 <= 3) {
			monster._mmode = MonsterMode::Talk;
		}
	}

	if (monster._mgoal == MGOAL_NORMAL || monster._mgoal == MGOAL_RETREAT || monster._mgoal == MGOAL_MOVE) {
		if (!gbIsMultiplayer && Quests[Q_BETRAYER]._qvar1 == 4 && monster.mtalkmsg == TEXT_NONE) { // Fix save games affected by teleport bug
			ObjChangeMapResync(1, 18, 20, 24);
			RedoPlayerVision();
			Quests[Q_BETRAYER]._qvar1 = 6;
		}
		monster.mtalkmsg = TEXT_NONE;
		CounselorAi(monsterId);
	}

	monster.CheckStandAnimationIsLoaded(md);
}

void LazarusMinionAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand)
		return;

	Direction md = GetMonsterDirection(monster);

	if (IsTileVisible(monster.position.tile)) {
		if (!gbIsMultiplayer) {
			if (Quests[Q_BETRAYER]._qvar1 <= 5) {
				monster._mgoal = MGOAL_INQUIRING;
			} else {
				monster._mgoal = MGOAL_NORMAL;
				monster.mtalkmsg = TEXT_NONE;
			}
		} else
			monster._mgoal = MGOAL_NORMAL;
	}
	if (monster._mgoal == MGOAL_NORMAL)
		AiRanged(monsterId);

	monster.CheckStandAnimationIsLoaded(md);
}

void LachdananAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);

	if (monster.mtalkmsg == TEXT_VEIL9 && !IsTileVisible(monster.position.tile) && monster._mgoal == MGOAL_TALKING) {
		monster.mtalkmsg = TEXT_VEIL10;
		monster._mgoal = MGOAL_INQUIRING;
	}

	if (IsTileVisible(monster.position.tile)) {
		if (monster.mtalkmsg == TEXT_VEIL11) {
			if (!effect_is_playing(USFX_LACH3) && monster._mgoal == MGOAL_TALKING) {
				monster.mtalkmsg = TEXT_NONE;
				Quests[Q_VEIL]._qactive = QUEST_DONE;
				StartMonsterDeath(monsterId, -1, true);
			}
		}
	}

	monster.CheckStandAnimationIsLoaded(md);
}

void WarlordAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);
	if (IsTileVisible(monster.position.tile)) {
		if (monster.mtalkmsg == TEXT_WARLRD9 && monster._mgoal == MGOAL_INQUIRING)
			monster._mmode = MonsterMode::Talk;
		if (monster.mtalkmsg == TEXT_WARLRD9 && !effect_is_playing(USFX_WARLRD1) && monster._mgoal == MGOAL_TALKING) {
			monster._msquelch = UINT8_MAX;
			monster.mtalkmsg = TEXT_NONE;
			monster._mgoal = MGOAL_NORMAL;
		}
	}

	if (monster._mgoal == MGOAL_NORMAL)
		SkeletonAi(monsterId);

	monster.CheckStandAnimationIsLoaded(md);
}

void HorkDemonAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mmode != MonsterMode::Stand || monster._msquelch == 0) {
		return;
	}

	int fx = monster.enemyPosition.x;
	int fy = monster.enemyPosition.y;
	int mx = monster.position.tile.x - fx;
	int my = monster.position.tile.y - fy;
	Direction md = GetDirection(monster.position.tile, monster.position.last);

	if (monster._msquelch < 255) {
		MonstCheckDoors(monster);
	}

	int v = GenerateRnd(100);

	if (abs(mx) < 2 && abs(my) < 2) {
		monster._mgoal = MGOAL_NORMAL;
	} else if (monster._mgoal == 4 || ((abs(mx) >= 5 || abs(my) >= 5) && GenerateRnd(4) != 0)) {
		if (monster._mgoal != 4) {
			monster._mgoalvar1 = 0;
			monster._mgoalvar2 = GenerateRnd(2);
		}
		monster._mgoal = MGOAL_MOVE;
		int dist = std::max(abs(mx), abs(my));
		if (monster._mgoalvar1++ >= 2 * dist || dTransVal[monster.position.tile.x][monster.position.tile.y] != dTransVal[fx][fy]) {
			monster._mgoal = MGOAL_NORMAL;
		} else if (!RoundWalk(monsterId, md, &monster._mgoalvar2)) {
			AiDelay(monster, GenerateRnd(10) + 10);
		}
	}

	if (monster._mgoal == 1) {
		if ((abs(mx) >= 3 || abs(my) >= 3) && v < 2 * monster._mint + 43) {
			Point position = monster.position.tile + monster._mdir;
			if (IsTileAvailable(monster, position) && ActiveMonsterCount < MaxMonsters) {
				StartRangedSpecialAttack(monster, MIS_HORKDMN, 0);
			}
		} else if (abs(mx) < 2 && abs(my) < 2) {
			if (v < 2 * monster._mint + 28) {
				monster._mdir = md;
				StartAttack(monster);
			}
		} else {
			v = GenerateRnd(100);
			if (v < 2 * monster._mint + 33
			    || (IsAnyOf(static_cast<MonsterMode>(monster._mVar1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways) && monster._mVar2 == 0 && v < 2 * monster._mint + 83)) {
				RandomWalk(monsterId, md);
			} else {
				AiDelay(monster, GenerateRnd(10) + 10);
			}
		}
	}

	monster.CheckStandAnimationIsLoaded(monster._mdir);
}

string_view GetMonsterTypeText(const MonsterData &monsterData)
{
	switch (monsterData.mMonstClass) {
	case MonsterClass::Animal:
		return _("Animal");
	case MonsterClass::Demon:
		return _("Demon");
	case MonsterClass::Undead:
		return _("Undead");
	}

	app_fatal("Unknown mMonstClass %i", static_cast<int>(monsterData.mMonstClass));
}

void ActivateSpawn(int monsterId, Point position, Direction dir)
{
	auto &monster = Monsters[monsterId];
	dMonster[position.x][position.y] = monsterId + 1;
	monster.position.tile = position;
	monster.position.future = position;
	monster.position.old = position;
	StartSpecialStand(monster, dir);
}

/** Maps from monster AI ID to monster AI function. */
void (*AiProc[])(int i) = {
	/*AI_ZOMBIE   */ &ZombieAi,
	/*AI_FAT      */ &OverlordAi,
	/*AI_SKELSD   */ &SkeletonAi,
	/*AI_SKELBOW  */ &SkeletonBowAi,
	/*AI_SCAV     */ &ScavengerAi,
	/*AI_RHINO    */ &RhinoAi,
	/*AI_GOATMC   */ &AiAvoidance,
	/*AI_GOATBOW  */ &AiRanged,
	/*AI_FALLEN   */ &FallenAi,
	/*AI_MAGMA    */ &AiRangedAvoidance,
	/*AI_SKELKING */ &LeoricAi,
	/*AI_BAT      */ &BatAi,
	/*AI_GARG     */ &GargoyleAi,
	/*AI_CLEAVER  */ &ButcherAi,
	/*AI_SUCC     */ &AiRanged,
	/*AI_SNEAK    */ &SneakAi,
	/*AI_STORM    */ &AiRangedAvoidance,
	/*AI_FIREMAN  */ nullptr,
	/*AI_GARBUD   */ &GharbadAi,
	/*AI_ACID     */ &AiRangedAvoidance,
	/*AI_ACIDUNIQ */ &AiRanged,
	/*AI_GOLUM    */ &GolumAi,
	/*AI_ZHAR     */ &ZharAi,
	/*AI_SNOTSPIL */ &SnotSpilAi,
	/*AI_SNAKE    */ &SnakeAi,
	/*AI_COUNSLR  */ &CounselorAi,
	/*AI_MEGA     */ &MegaAi,
	/*AI_DIABLO   */ &AiRangedAvoidance,
	/*AI_LAZARUS  */ &LazarusAi,
	/*AI_LAZHELP  */ &LazarusMinionAi,
	/*AI_LACHDAN  */ &LachdananAi,
	/*AI_WARLORD  */ &WarlordAi,
	/*AI_FIREBAT  */ &AiRanged,
	/*AI_TORCHANT */ &AiRanged,
	/*AI_HORKDMN  */ &HorkDemonAi,
	/*AI_LICH     */ &AiRanged,
	/*AI_ARCHLICH */ &AiRanged,
	/*AI_PSYCHORB */ &AiRanged,
	/*AI_NECROMORB*/ &AiRanged,
	/*AI_BONEDEMON*/ &AiRangedAvoidance
};

bool IsRelativeMoveOK(const Monster &monster, Point position, Direction mdir)
{
	Point futurePosition = position + mdir;
	if (!InDungeonBounds(futurePosition) || !IsTileAvailable(monster, futurePosition))
		return false;
	if (mdir == Direction::East) {
		if (IsTileSolid(position + Direction::SouthEast))
			return false;
	} else if (mdir == Direction::West) {
		if (IsTileSolid(position + Direction::SouthWest))
			return false;
	} else if (mdir == Direction::North) {
		if (IsTileSolid(position + Direction::NorthEast) || IsTileSolid(position + Direction::NorthWest))
			return false;
	} else if (mdir == Direction::South)
		if (IsTileSolid(position + Direction::SouthWest) || IsTileSolid(position + Direction::SouthEast))
			return false;
	return true;
}

bool IsMonsterAvalible(const MonsterData &monsterData)
{
	if (monsterData.availability == MonsterAvailability::Never)
		return false;

	if (gbIsSpawn && monsterData.availability == MonsterAvailability::Retail)
		return false;

	return currlevel >= monsterData.mMinDLvl && currlevel <= monsterData.mMaxDLvl;
}

} // namespace

void InitTRNForUniqueMonster(Monster &monster)
{
	char filestr[64];
	sprintf(filestr, "Monsters\\Monsters\\%s.TRN", UniqueMonstersData[monster._uniqtype - 1].mTrnName);
	monster.uniqueTRN = LoadFileInMem<uint8_t>(filestr);
}

void PrepareUniqueMonst(Monster &monster, int uniqindex, int miniontype, int bosspacksize, const UniqueMonsterData &uniqueMonsterData)
{
	monster._uniqtype = uniqindex + 1;

	if (uniqueMonsterData.mlevel != 0) {
		monster.mLevel = 2 * uniqueMonsterData.mlevel;
	} else {
		monster.mLevel = monster.MData->mLevel + 5;
	}

	monster.mExp *= 2;
	monster.mName = pgettext("monster", uniqueMonsterData.mName).data();
	monster._mmaxhp = uniqueMonsterData.mmaxhp << 6;

	if (!gbIsMultiplayer)
		monster._mmaxhp = std::max(monster._mmaxhp / 2, 64);

	monster._mhitpoints = monster._mmaxhp;
	monster._mAi = uniqueMonsterData.mAi;
	monster._mint = uniqueMonsterData.mint;
	monster.mMinDamage = uniqueMonsterData.mMinDamage;
	monster.mMaxDamage = uniqueMonsterData.mMaxDamage;
	monster.mMinDamage2 = uniqueMonsterData.mMinDamage;
	monster.mMaxDamage2 = uniqueMonsterData.mMaxDamage;
	monster.mMagicRes = uniqueMonsterData.mMagicRes;
	monster.mtalkmsg = uniqueMonsterData.mtalkmsg;
	if (uniqindex == UMT_HORKDMN)
		monster.mlid = NO_LIGHT; // BUGFIX monsters initial light id should be -1 (fixed)
	else
		monster.mlid = AddLight(monster.position.tile, 3);

	if (gbIsMultiplayer) {
		if (monster._mAi == AI_LAZHELP)
			monster.mtalkmsg = TEXT_NONE;
		if (monster._mAi == AI_LAZARUS && Quests[Q_BETRAYER]._qvar1 > 3) {
			monster._mgoal = MGOAL_NORMAL;
		} else if (monster.mtalkmsg != TEXT_NONE) {
			monster._mgoal = MGOAL_INQUIRING;
		}
	} else if (monster.mtalkmsg != TEXT_NONE) {
		monster._mgoal = MGOAL_INQUIRING;
	}

	if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
		monster._mmaxhp = 3 * monster._mmaxhp;
		if (gbIsHellfire)
			monster._mmaxhp += (gbIsMultiplayer ? 100 : 50) << 6;
		else
			monster._mmaxhp += 64;
		monster.mLevel += 15;
		monster._mhitpoints = monster._mmaxhp;
		monster.mExp = 2 * (monster.mExp + 1000);
		monster.mMinDamage = 2 * (monster.mMinDamage + 2);
		monster.mMaxDamage = 2 * (monster.mMaxDamage + 2);
		monster.mMinDamage2 = 2 * (monster.mMinDamage2 + 2);
		monster.mMaxDamage2 = 2 * (monster.mMaxDamage2 + 2);
	} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
		monster._mmaxhp = 4 * monster._mmaxhp;
		if (gbIsHellfire)
			monster._mmaxhp += (gbIsMultiplayer ? 200 : 100) << 6;
		else
			monster._mmaxhp += 192;
		monster.mLevel += 30;
		monster._mhitpoints = monster._mmaxhp;
		monster.mExp = 4 * (monster.mExp + 1000);
		monster.mMinDamage = 4 * monster.mMinDamage + 6;
		monster.mMaxDamage = 4 * monster.mMaxDamage + 6;
		monster.mMinDamage2 = 4 * monster.mMinDamage2 + 6;
		monster.mMaxDamage2 = 4 * monster.mMaxDamage2 + 6;
	}

	InitTRNForUniqueMonster(monster);
	monster._uniqtrans = uniquetrans++;

	if (uniqueMonsterData.customToHit != 0) {
		monster.mHit = uniqueMonsterData.customToHit;
		monster.mHit2 = uniqueMonsterData.customToHit;

		if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
			monster.mHit += NightmareToHitBonus;
			monster.mHit2 += NightmareToHitBonus;
		} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
			monster.mHit += HellToHitBonus;
			monster.mHit2 += HellToHitBonus;
		}
	}
	if (uniqueMonsterData.customArmorClass != 0) {
		monster.mArmorClass = uniqueMonsterData.customArmorClass;

		if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
			monster.mArmorClass += NightmareAcBonus;
		} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
			monster.mArmorClass += HellAcBonus;
		}
	}

	ActiveMonsterCount++;

	if (uniqueMonsterData.monsterPack != UniqueMonsterPack::None) {
		PlaceGroup(miniontype, bosspacksize, uniqueMonsterData.monsterPack, ActiveMonsterCount - 1);
	}

	if (monster._mAi != AI_GARG) {
		monster.ChangeAnimationData(MonsterGraphic::Stand);
		monster.AnimInfo.CurrentFrame = GenerateRnd(monster.AnimInfo.NumberOfFrames - 1);
		monster._mFlags &= ~MFLAG_ALLOW_SPECIAL;
		monster._mmode = MonsterMode::Stand;
	}
}

void InitLevelMonsters()
{
	LevelMonsterTypeCount = 0;
	monstimgtot = 0;

	for (auto &levelMonsterType : LevelMonsterTypes) {
		levelMonsterType.mPlaceFlags = 0;
	}

	ClrAllMonsters();
	ActiveMonsterCount = 0;
	totalmonsters = MaxMonsters;

	for (int i = 0; i < MaxMonsters; i++) {
		ActiveMonsters[i] = i;
	}

	uniquetrans = 0;
}

void GetLevelMTypes()
{
	AddMonsterType(MT_GOLEM, PLACE_SPECIAL);
	if (currlevel == 16) {
		AddMonsterType(MT_ADVOCATE, PLACE_SCATTER);
		AddMonsterType(MT_RBLACK, PLACE_SCATTER);
		AddMonsterType(MT_DIABLO, PLACE_SPECIAL);
		return;
	}

	if (currlevel == 18)
		AddMonsterType(MT_HORKSPWN, PLACE_SCATTER);
	if (currlevel == 19) {
		AddMonsterType(MT_HORKSPWN, PLACE_SCATTER);
		AddMonsterType(MT_HORKDMN, PLACE_UNIQUE);
	}
	if (currlevel == 20)
		AddMonsterType(MT_DEFILER, PLACE_UNIQUE);
	if (currlevel == 24) {
		AddMonsterType(MT_ARCHLICH, PLACE_SCATTER);
		AddMonsterType(MT_NAKRUL, PLACE_SPECIAL);
	}

	if (!setlevel) {
		if (Quests[Q_BUTCHER].IsAvailable())
			AddMonsterType(MT_CLEAVER, PLACE_SPECIAL);
		if (Quests[Q_GARBUD].IsAvailable())
			AddMonsterType(UniqueMonstersData[UMT_GARBUD].mtype, PLACE_UNIQUE);
		if (Quests[Q_ZHAR].IsAvailable())
			AddMonsterType(UniqueMonstersData[UMT_ZHAR].mtype, PLACE_UNIQUE);
		if (Quests[Q_LTBANNER].IsAvailable())
			AddMonsterType(UniqueMonstersData[UMT_SNOTSPIL].mtype, PLACE_UNIQUE);
		if (Quests[Q_VEIL].IsAvailable())
			AddMonsterType(UniqueMonstersData[UMT_LACHDAN].mtype, PLACE_UNIQUE);
		if (Quests[Q_WARLORD].IsAvailable())
			AddMonsterType(UniqueMonstersData[UMT_WARLORD].mtype, PLACE_UNIQUE);

		if (gbIsMultiplayer && currlevel == Quests[Q_SKELKING]._qlevel) {

			AddMonsterType(MT_SKING, PLACE_UNIQUE);

			int skeletonTypeCount = 0;
			_monster_id skeltypes[NUM_MTYPES];
			for (_monster_id skeletonType : SkeletonTypes) {
				if (!IsMonsterAvalible(MonstersData[skeletonType]))
					continue;

				skeltypes[skeletonTypeCount++] = skeletonType;
			}
			AddMonsterType(skeltypes[GenerateRnd(skeletonTypeCount)], PLACE_SCATTER);
		}

		_monster_id typelist[MaxMonsters];

		int nt = 0;
		for (int i = MT_NZOMBIE; i < NUM_MTYPES; i++) {
			if (!IsMonsterAvalible(MonstersData[i]))
				continue;

			typelist[nt++] = (_monster_id)i;
		}

		while (nt > 0 && LevelMonsterTypeCount < MaxLvlMTypes && monstimgtot < 4000) {
			for (int i = 0; i < nt;) {
				if (MonstersData[typelist[i]].mImage > 4000 - monstimgtot) {
					typelist[i] = typelist[--nt];
					continue;
				}

				i++;
			}

			if (nt != 0) {
				int i = GenerateRnd(nt);
				AddMonsterType(typelist[i], PLACE_SCATTER);
				typelist[i] = typelist[--nt];
			}
		}
	} else {
		if (setlvlnum == SL_SKELKING) {
			AddMonsterType(MT_SKING, PLACE_UNIQUE);
		}
	}
}

void InitMonsterGFX(int monsterTypeIndex)
{
	CMonster &monster = LevelMonsterTypes[monsterTypeIndex];
	const _monster_id mtype = monster.mtype;
	const MonsterData &monsterData = MonstersData[mtype];
	const int width = monsterData.width;
	constexpr size_t MaxAnims = sizeof(Animletter) / sizeof(Animletter[0]) - 1;
	const size_t numAnims = GetNumAnims(monsterData);

	const auto hasAnim = [&monsterData](size_t i) {
		return monsterData.Frames[i] != 0;
	};

	std::array<uint32_t, MaxAnims> animOffsets;
	monster.animData = MultiFileLoader<MaxAnims> {}(
	    numAnims,
	    FileNameWithCharAffixGenerator({ "Monsters\\", monsterData.GraphicType }, ".CL2", Animletter),
	    animOffsets.data(),
	    hasAnim);

	for (unsigned animIndex = 0; animIndex < numAnims; animIndex++) {
		AnimStruct &anim = monster.Anims[animIndex];

		if (!hasAnim(animIndex)) {
			anim.Frames = 0;
			continue;
		}

		anim.Frames = monsterData.Frames[animIndex];
		anim.Rate = monsterData.Rate[animIndex];
		anim.Width = width;

		byte *cl2Data = &monster.animData[animOffsets[animIndex]];
		if (IsDirectionalAnim(monster, animIndex)) {
			CelGetDirectionFrames(cl2Data, anim.CelSpritesForDirections.data());
		} else {
			for (size_t i = 0; i < 8; ++i) {
				anim.CelSpritesForDirections[i] = cl2Data;
			}
		}
	}

	monster.MData = &monsterData;

	if (monsterData.has_trans) {
		InitMonsterTRN(monster);
	}

	if (IsAnyOf(mtype, MT_NMAGMA, MT_YMAGMA, MT_BMAGMA, MT_WMAGMA))
		MissileSpriteData[MFILE_MAGBALL].LoadGFX();
	if (IsAnyOf(mtype, MT_STORM, MT_RSTORM, MT_STORML, MT_MAEL))
		MissileSpriteData[MFILE_THINLGHT].LoadGFX();
	if (mtype == MT_SNOWWICH) {
		MissileSpriteData[MFILE_SCUBMISB].LoadGFX();
		MissileSpriteData[MFILE_SCBSEXPB].LoadGFX();
	}
	if (mtype == MT_HLSPWN) {
		MissileSpriteData[MFILE_SCUBMISD].LoadGFX();
		MissileSpriteData[MFILE_SCBSEXPD].LoadGFX();
	}
	if (mtype == MT_SOLBRNR) {
		MissileSpriteData[MFILE_SCUBMISC].LoadGFX();
		MissileSpriteData[MFILE_SCBSEXPC].LoadGFX();
	}
	if (IsAnyOf(mtype, MT_NACID, MT_RACID, MT_BACID, MT_XACID, MT_SPIDLORD)) {
		MissileSpriteData[MFILE_ACIDBF].LoadGFX();
		MissileSpriteData[MFILE_ACIDSPLA].LoadGFX();
		MissileSpriteData[MFILE_ACIDPUD].LoadGFX();
	}
	if (mtype == MT_LICH) {
		MissileSpriteData[MFILE_LICH].LoadGFX();
		MissileSpriteData[MFILE_EXORA1].LoadGFX();
	}
	if (mtype == MT_ARCHLICH) {
		MissileSpriteData[MFILE_ARCHLICH].LoadGFX();
		MissileSpriteData[MFILE_EXYEL2].LoadGFX();
	}
	if (IsAnyOf(mtype, MT_PSYCHORB, MT_BONEDEMN))
		MissileSpriteData[MFILE_BONEDEMON].LoadGFX();
	if (mtype == MT_NECRMORB) {
		MissileSpriteData[MFILE_NECROMORB].LoadGFX();
		MissileSpriteData[MFILE_EXRED3].LoadGFX();
	}
	if (mtype == MT_PSYCHORB)
		MissileSpriteData[MFILE_EXBL2].LoadGFX();
	if (mtype == MT_BONEDEMN)
		MissileSpriteData[MFILE_EXBL3].LoadGFX();
	if (mtype == MT_DIABLO)
		MissileSpriteData[MFILE_FIREPLAR].LoadGFX();
}

void WeakenNaKrul()
{
	if (currlevel != 24 || UberDiabloMonsterIndex < 0 || UberDiabloMonsterIndex >= ActiveMonsterCount)
		return;

	auto &monster = Monsters[UberDiabloMonsterIndex];
	PlayEffect(monster, 2);
	Quests[Q_NAKRUL]._qlog = false;
	monster.mArmorClass -= 50;
	int hp = monster._mmaxhp / 2;
	monster.mMagicRes = 0;
	monster._mhitpoints = hp;
	monster._mmaxhp = hp;
}

void InitGolems()
{
	if (!setlevel) {
		for (int i = 0; i < MAX_PLRS; i++)
			AddMonster(GolemHoldingCell, Direction::South, 0, false);
	}
}

void InitMonsters()
{
	if (!gbIsSpawn && !setlevel && currlevel == 16)
		LoadDiabMonsts();

	int nt = numtrigs;
	if (currlevel == 15)
		nt = 1;
	for (int i = 0; i < nt; i++) {
		for (int s = -2; s < 2; s++) {
			for (int t = -2; t < 2; t++)
				DoVision(trigs[i].position + Displacement { s, t }, 15, MAP_EXP_NONE, false);
		}
	}
	if (!gbIsSpawn)
		PlaceQuestMonsters();
	if (!setlevel) {
		if (!gbIsSpawn)
			PlaceUniqueMonsters();
		int na = 0;
		for (int s = 16; s < 96; s++) {
			for (int t = 16; t < 96; t++) {
				if (!IsTileSolid({ s, t }))
					na++;
			}
		}
		int numplacemonsters = na / 30;
		if (gbIsMultiplayer)
			numplacemonsters += numplacemonsters / 2;
		if (ActiveMonsterCount + numplacemonsters > MaxMonsters - 10)
			numplacemonsters = MaxMonsters - 10 - ActiveMonsterCount;
		totalmonsters = ActiveMonsterCount + numplacemonsters;
		int numscattypes = 0;
		int scattertypes[NUM_MTYPES];
		for (int i = 0; i < LevelMonsterTypeCount; i++) {
			if ((LevelMonsterTypes[i].mPlaceFlags & PLACE_SCATTER) != 0) {
				scattertypes[numscattypes] = i;
				numscattypes++;
			}
		}
		while (ActiveMonsterCount < totalmonsters) {
			int mtype = scattertypes[GenerateRnd(numscattypes)];
			if (currlevel == 1 || GenerateRnd(2) == 0)
				na = 1;
			else if (currlevel == 2 || leveltype == DTYPE_CRYPT)
				na = GenerateRnd(2) + 2;
			else
				na = GenerateRnd(3) + 3;
			PlaceGroup(mtype, na, UniqueMonsterPack::None, 0);
		}
	}
	for (int i = 0; i < nt; i++) {
		for (int s = -2; s < 2; s++) {
			for (int t = -2; t < 2; t++)
				DoUnVision(trigs[i].position + Displacement { s, t }, 15);
		}
	}
}

void SetMapMonsters(const uint16_t *dunData, Point startPosition)
{
	AddMonsterType(MT_GOLEM, PLACE_SPECIAL);
	if (setlevel)
		for (int i = 0; i < MAX_PLRS; i++)
			AddMonster(GolemHoldingCell, Direction::South, 0, false);

	if (setlevel && setlvlnum == SL_VILEBETRAYER) {
		AddMonsterType(UniqueMonstersData[UMT_LAZARUS].mtype, PLACE_UNIQUE);
		AddMonsterType(UniqueMonstersData[UMT_RED_VEX].mtype, PLACE_UNIQUE);
		AddMonsterType(UniqueMonstersData[UMT_BLACKJADE].mtype, PLACE_UNIQUE);
		PlaceUniqueMonst(UMT_LAZARUS, 0, 0);
		PlaceUniqueMonst(UMT_RED_VEX, 0, 0);
		PlaceUniqueMonst(UMT_BLACKJADE, 0, 0);
	}

	int width = SDL_SwapLE16(dunData[0]);
	int height = SDL_SwapLE16(dunData[1]);

	int layer2Offset = 2 + width * height;

	// The rest of the layers are at dPiece scale
	width *= 2;
	height *= 2;

	const uint16_t *monsterLayer = &dunData[layer2Offset + width * height];

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			auto monsterId = static_cast<uint8_t>(SDL_SwapLE16(monsterLayer[j * width + i]));
			if (monsterId != 0) {
				int mtype = AddMonsterType(MonstConvTbl[monsterId - 1], PLACE_SPECIAL);
				PlaceMonster(ActiveMonsterCount++, mtype, startPosition + Displacement { i, j });
			}
		}
	}
}

int AddMonster(Point position, Direction dir, int mtype, bool inMap)
{
	if (ActiveMonsterCount < MaxMonsters) {
		int i = ActiveMonsters[ActiveMonsterCount++];
		if (inMap)
			dMonster[position.x][position.y] = i + 1;
		InitMonster(Monsters[i], dir, mtype, position);
		return i;
	}

	return -1;
}

void AddDoppelganger(Monster &monster)
{
	if (monster.MType == nullptr) {
		return;
	}

	Point target = { 0, 0 };
	for (int d = 0; d < 8; d++) {
		const Point position = monster.position.tile + static_cast<Direction>(d);
		if (!IsTileAvailable(position))
			continue;
		target = position;
	}
	if (target != Point { 0, 0 }) {
		for (int j = 0; j < MaxLvlMTypes; j++) {
			if (LevelMonsterTypes[j].mtype == monster.MType->mtype) {
				AddMonster(target, monster._mdir, j, true);
				break;
			}
		}
	}
}

bool M_Talker(const Monster &monster)
{
	return IsAnyOf(monster._mAi, AI_LAZARUS, AI_WARLORD, AI_GARBUD, AI_ZHAR, AI_SNOTSPIL, AI_LACHDAN, AI_LAZHELP);
}

void M_StartStand(Monster &monster, Direction md)
{
	ClearMVars(monster);
	if (monster.MType->mtype == MT_GOLEM)
		NewMonsterAnim(monster, MonsterGraphic::Walk, md);
	else
		NewMonsterAnim(monster, MonsterGraphic::Stand, md);
	monster._mVar1 = static_cast<int>(monster._mmode);
	monster._mVar2 = 0;
	monster._mmode = MonsterMode::Stand;
	monster.position.offset = { 0, 0 };
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
	UpdateEnemy(monster);
}

void M_ClearSquares(int monsterId)
{
	auto &monster = Monsters[monsterId];

	int mx = monster.position.old.x;
	int my = monster.position.old.y;
	int m1 = -(monsterId + 1);
	int m2 = monsterId + 1;

	for (int y = my - 1; y <= my + 1; y++) {
		for (int x = mx - 1; x <= mx + 1; x++) {
			if (InDungeonBounds({ x, y }) && (dMonster[x][y] == m1 || dMonster[x][y] == m2))
				dMonster[x][y] = 0;
		}
	}
}

void M_GetKnockback(int monsterId)
{
	auto &monster = Monsters[monsterId];

	Direction dir = Opposite(monster._mdir);
	if (!IsRelativeMoveOK(monster, monster.position.old, dir)) {
		return;
	}

	M_ClearSquares(monsterId);
	monster.position.old += dir;
	StartMonsterGotHit(monsterId);
}

void M_StartHit(int monsterId, int dam)
{
	Monster &monster = Monsters[monsterId];

	PlayEffect(monster, 1);

	if (IsAnyOf(monster.MType->mtype, MT_SNEAK, MT_STALKER, MT_UNSEEN, MT_ILLWEAV) || dam >> 6 >= monster.mLevel + 3) {
		if (monster.MType->mtype == MT_BLINK) {
			Teleport(monsterId);
		} else if (IsAnyOf(monster.MType->mtype, MT_NSCAV, MT_BSCAV, MT_WSCAV, MT_YSCAV)
		    || monster.MType->mtype == MT_GRAVEDIG) {
			monster._mgoal = MGOAL_NORMAL;
			monster._mgoalvar1 = 0;
			monster._mgoalvar2 = 0;
		}
		if (monster._mmode != MonsterMode::Petrified) {
			StartMonsterGotHit(monsterId);
		}
	}
}

void M_StartHit(int monsterId, int pnum, int dam)
{
	Monster &monster = Monsters[monsterId];

	monster.mWhoHit |= 1 << pnum;
	if (pnum == MyPlayerId) {
		delta_monster_hp(monsterId, monster._mhitpoints, *MyPlayer);
		NetSendCmdMonDmg(false, monsterId, dam);
	}
	if (IsAnyOf(monster.MType->mtype, MT_SNEAK, MT_STALKER, MT_UNSEEN, MT_ILLWEAV) || dam >> 6 >= monster.mLevel + 3) {
		monster._menemy = pnum;
		monster.enemyPosition = Players[pnum].position.future;
		monster._mFlags &= ~MFLAG_TARGETS_MONSTER;
		monster._mdir = GetMonsterDirection(monster);
	}

	M_StartHit(monsterId, dam);
}

void StartMonsterDeath(int monsterId, int pnum, bool sendmsg)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	Monster &monster = Monsters[monsterId];

	Direction md = pnum >= 0 ? GetDirection(monster.position.tile, Players[pnum].position.tile) : monster._mdir;
	MonsterDeath(monsterId, pnum, md, sendmsg);
}

void M_StartKill(int monsterId, int pnum)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (pnum == MyPlayerId) {
		delta_kill_monster(monsterId, monster.position.tile, *MyPlayer);
		if (monsterId != pnum) {
			NetSendCmdLocParam1(false, CMD_MONSTDEATH, monster.position.tile, monsterId);
		} else {
			NetSendCmdLoc(MyPlayerId, false, CMD_KILLGOLEM, monster.position.tile);
		}
	}

	StartMonsterDeath(monsterId, pnum, true);
}

void M_SyncStartKill(int monsterId, Point position, int pnum)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	if (monster._mhitpoints == 0 || monster._mmode == MonsterMode::Death) {
		return;
	}

	if (dMonster[position.x][position.y] == 0) {
		M_ClearSquares(monsterId);
		monster.position.tile = position;
		monster.position.old = position;
	}

	StartMonsterDeath(monsterId, pnum, false);
}

void M_UpdateLeader(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];

	for (int j = 0; j < ActiveMonsterCount; j++) {
		auto &minion = Monsters[ActiveMonsters[j]];
		if (minion.leaderRelation == LeaderRelation::Leashed && minion.leader == monsterId)
			minion.leaderRelation = LeaderRelation::None;
	}

	if (monster.leaderRelation == LeaderRelation::Leashed) {
		Monsters[monster.leader].packsize--;
	}
}

void DoEnding()
{
	if (gbIsMultiplayer) {
		SNetLeaveGame(LEAVE_ENDING);
	}

	music_stop();

	if (gbIsMultiplayer) {
		SDL_Delay(1000);
	}

	if (gbIsSpawn)
		return;

	switch (MyPlayer->_pClass) {
	case HeroClass::Sorcerer:
	case HeroClass::Monk:
		play_movie("gendata\\DiabVic1.smk", false);
		break;
	case HeroClass::Warrior:
	case HeroClass::Barbarian:
		play_movie("gendata\\DiabVic2.smk", false);
		break;
	default:
		play_movie("gendata\\DiabVic3.smk", false);
		break;
	}
	play_movie("gendata\\Diabend.smk", false);

	bool bMusicOn = gbMusicOn;
	gbMusicOn = true;

	int musicVolume = sound_get_or_set_music_volume(1);
	sound_get_or_set_music_volume(0);

	music_start(TMUSIC_CATACOMBS);
	loop_movie = true;
	play_movie("gendata\\loopdend.smk", true);
	loop_movie = false;
	music_stop();

	sound_get_or_set_music_volume(musicVolume);
	gbMusicOn = bMusicOn;
}

void PrepDoEnding()
{
	gbSoundOn = sgbSaveSoundOn;
	gbRunGame = false;
	MyPlayerIsDead = false;
	cineflag = true;

	Player &myPlayer = *MyPlayer;

	myPlayer.pDiabloKillLevel = std::max(myPlayer.pDiabloKillLevel, static_cast<uint8_t>(sgGameInitInfo.nDifficulty + 1));

	for (Player &player : Players) {
		player._pmode = PM_QUIT;
		player._pInvincible = true;
		if (gbIsMultiplayer) {
			if (player._pHitPoints >> 6 == 0)
				player._pHitPoints = 64;
			if (player._pMana >> 6 == 0)
				player._pMana = 64;
		}
	}
}

void M_WalkDir(int monsterId, Direction md)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);

	int mwi = Monsters[monsterId].MType->GetAnimData(MonsterGraphic::Walk).Frames - 1;
	switch (md) {
	case Direction::North:
		StartWalk(monsterId, 0, -MWVel[mwi][1], -1, -1, Direction::North);
		break;
	case Direction::NorthEast:
		StartWalk(monsterId, MWVel[mwi][1], -MWVel[mwi][0], 0, -1, Direction::NorthEast);
		break;
	case Direction::East:
		StartWalk3(monsterId, MWVel[mwi][2], 0, -32, -16, 1, -1, 1, 0, Direction::East);
		break;
	case Direction::SouthEast:
		StartWalk2(monsterId, MWVel[mwi][1], MWVel[mwi][0], -32, -16, 1, 0, Direction::SouthEast);
		break;
	case Direction::South:
		StartWalk2(monsterId, 0, MWVel[mwi][1], 0, -32, 1, 1, Direction::South);
		break;
	case Direction::SouthWest:
		StartWalk2(monsterId, -MWVel[mwi][1], MWVel[mwi][0], 32, -16, 0, 1, Direction::SouthWest);
		break;
	case Direction::West:
		StartWalk3(monsterId, -MWVel[mwi][2], 0, 32, -16, -1, 1, 0, 1, Direction::West);
		break;
	case Direction::NorthWest:
		StartWalk(monsterId, -MWVel[mwi][1], -MWVel[mwi][0], -1, 0, Direction::NorthWest);
		break;
	}
}

void GolumAi(int monsterId)
{
	assert(monsterId >= 0 && monsterId < MAX_PLRS);
	auto &golem = Monsters[monsterId];

	if (golem.position.tile.x == 1 && golem.position.tile.y == 0) {
		return;
	}

	if (IsAnyOf(golem._mmode, MonsterMode::Death, MonsterMode::SpecialStand) || golem.IsWalking()) {
		return;
	}

	if ((golem._mFlags & MFLAG_TARGETS_MONSTER) == 0)
		UpdateEnemy(golem);

	if (golem._mmode == MonsterMode::MeleeAttack) {
		return;
	}

	if ((golem._mFlags & MFLAG_NO_ENEMY) == 0) {
		auto &enemy = Monsters[golem._menemy];
		int mex = golem.position.tile.x - enemy.position.future.x;
		int mey = golem.position.tile.y - enemy.position.future.y;
		golem._mdir = GetDirection(golem.position.tile, enemy.position.tile);
		if (abs(mex) < 2 && abs(mey) < 2) {
			golem.enemyPosition = enemy.position.tile;
			if (enemy._msquelch == 0) {
				enemy._msquelch = UINT8_MAX;
				enemy.position.last = golem.position.tile;
				for (int j = 0; j < 5; j++) {
					for (int k = 0; k < 5; k++) {
						int enemyId = dMonster[golem.position.tile.x + k - 2][golem.position.tile.y + j - 2]; // BUGFIX: Check if indexes are between 0 and 112
						if (enemyId > 0)
							Monsters[enemyId - 1]._msquelch = UINT8_MAX; // BUGFIX: should be `Monsters[_menemy-1]`, not Monsters[_menemy]. (fixed)
					}
				}
			}
			StartAttack(golem);
			return;
		}
		if (AiPlanPath(monsterId))
			return;
	}

	golem._pathcount++;
	if (golem._pathcount > 8)
		golem._pathcount = 5;

	if (RandomWalk(monsterId, Players[monsterId]._pdir))
		return;

	Direction md = Left(golem._mdir);
	bool ok = false;
	for (int j = 0; j < 8 && !ok; j++) {
		md = Right(md);
		ok = DirOK(monsterId, md);
	}
	if (ok)
		M_WalkDir(monsterId, md);
}

void DeleteMonsterList()
{
	for (int i = 0; i < MAX_PLRS; i++) {
		auto &golem = Monsters[i];
		if (!golem._mDelFlag)
			continue;

		golem.position.tile = GolemHoldingCell;
		golem.position.future = { 0, 0 };
		golem.position.old = { 0, 0 };
		golem._mDelFlag = false;
	}

	for (int i = MAX_PLRS; i < ActiveMonsterCount;) {
		if (Monsters[ActiveMonsters[i]]._mDelFlag) {
			if (pcursmonst == ActiveMonsters[i]) // Unselect monster if player highlighted it
				pcursmonst = -1;
			DeleteMonster(i);
		} else {
			i++;
		}
	}
}

void ProcessMonsters()
{
	DeleteMonsterList();

	assert(ActiveMonsterCount >= 0 && ActiveMonsterCount <= MaxMonsters);
	for (int i = 0; i < ActiveMonsterCount; i++) {
		int monsterId = ActiveMonsters[i];
		auto &monster = Monsters[monsterId];
		FollowTheLeader(monster);
		bool raflag = false;
		if (gbIsMultiplayer) {
			SetRndSeed(monster._mAISeed);
			monster._mAISeed = AdvanceRndSeed();
		}
		if ((monster._mFlags & MFLAG_NOHEAL) == 0 && monster._mhitpoints < monster._mmaxhp && monster._mhitpoints >> 6 > 0) {
			if (monster.mLevel > 1) {
				monster._mhitpoints += monster.mLevel / 2;
			} else {
				monster._mhitpoints += monster.mLevel;
			}
			monster._mhitpoints = std::min(monster._mhitpoints, monster._mmaxhp); // prevent going over max HP with part of a single regen tick
		}

		if (IsTileVisible(monster.position.tile) && monster._msquelch == 0) {
			if (monster.MType->mtype == MT_CLEAVER) {
				PlaySFX(USFX_CLEAVER);
			}
			if (monster.MType->mtype == MT_NAKRUL) {
				if (sgGameInitInfo.bCowQuest != 0) {
					PlaySFX(USFX_NAKRUL6);
				} else {
					if (IsUberRoomOpened)
						PlaySFX(USFX_NAKRUL4);
					else
						PlaySFX(USFX_NAKRUL5);
				}
			}
			if (monster.MType->mtype == MT_DEFILER)
				PlaySFX(USFX_DEFILER8);
			UpdateEnemy(monster);
		}

		if ((monster._mFlags & MFLAG_TARGETS_MONSTER) != 0) {
			assert(monster._menemy >= 0 && monster._menemy < MaxMonsters);
			monster.position.last = Monsters[monster._menemy].position.future;
			monster.enemyPosition = monster.position.last;
		} else {
			assert(monster._menemy >= 0 && monster._menemy < MAX_PLRS);
			Player &player = Players[monster._menemy];
			monster.enemyPosition = player.position.future;
			if (IsTileVisible(monster.position.tile)) {
				monster._msquelch = UINT8_MAX;
				monster.position.last = player.position.future;
			} else if (monster._msquelch != 0 && monster.MType->mtype != MT_DIABLO) {
				monster._msquelch--;
			}
		}
		do {
			if ((monster._mFlags & MFLAG_SEARCH) == 0 || !AiPlanPath(monsterId)) {
				AiProc[monster._mAi](monsterId);
			}
			switch (monster._mmode) {
			case MonsterMode::Stand:
				raflag = MonsterIdle(monster);
				break;
			case MonsterMode::MoveNorthwards:
			case MonsterMode::MoveSouthwards:
			case MonsterMode::MoveSideways:
				raflag = MonsterWalk(monsterId, monster._mmode);
				break;
			case MonsterMode::MeleeAttack:
				raflag = MonsterAttack(monsterId);
				break;
			case MonsterMode::HitRecovery:
				raflag = MonsterGotHit(monster);
				break;
			case MonsterMode::Death:
				raflag = MonsterDeath(monsterId);
				break;
			case MonsterMode::SpecialMeleeAttack:
				raflag = MonsterSpecialAttack(monsterId);
				break;
			case MonsterMode::FadeIn:
				raflag = MonsterFadein(monster);
				break;
			case MonsterMode::FadeOut:
				raflag = MonsterFadeout(monster);
				break;
			case MonsterMode::RangedAttack:
				raflag = MonsterRangedAttack(monsterId);
				break;
			case MonsterMode::SpecialStand:
				raflag = MonsterSpecialStand(monster);
				break;
			case MonsterMode::SpecialRangedAttack:
				raflag = MonsterRangedSpecialAttack(monsterId);
				break;
			case MonsterMode::Delay:
				raflag = MonsterDelay(monster);
				break;
			case MonsterMode::Charge:
				raflag = false;
				break;
			case MonsterMode::Petrified:
				raflag = MonsterPetrified(monster);
				break;
			case MonsterMode::Heal:
				raflag = MonsterHeal(monster);
				break;
			case MonsterMode::Talk:
				raflag = MonsterTalk(monster);
				break;
			}
			if (raflag) {
				GroupUnity(monster);
			}
		} while (raflag);
		if (monster._mmode != MonsterMode::Petrified) {
			monster.AnimInfo.ProcessAnimation((monster._mFlags & MFLAG_LOCK_ANIMATION) != 0, (monster._mFlags & MFLAG_ALLOW_SPECIAL) != 0);
		}
	}

	DeleteMonsterList();
}

void FreeMonsters()
{
	for (int i = 0; i < LevelMonsterTypeCount; i++) {
		LevelMonsterTypes[i].animData = nullptr;
	}
}

bool DirOK(int monsterId, Direction mdir)
{
	assert(monsterId >= 0 && monsterId < MaxMonsters);
	auto &monster = Monsters[monsterId];
	Point position = monster.position.tile;
	Point futurePosition = position + mdir;
	if (!IsRelativeMoveOK(monster, position, mdir))
		return false;
	if (monster.leaderRelation == LeaderRelation::Leashed) {
		return futurePosition.WalkingDistance(Monsters[monster.leader].position.future) < 4;
	}
	if (monster._uniqtype == 0 || UniqueMonstersData[monster._uniqtype - 1].monsterPack != UniqueMonsterPack::Leashed)
		return true;
	int mcount = 0;
	for (int x = futurePosition.x - 3; x <= futurePosition.x + 3; x++) {
		for (int y = futurePosition.y - 3; y <= futurePosition.y + 3; y++) {
			if (!InDungeonBounds({ x, y }))
				continue;
			int mi = dMonster[x][y];
			if (mi <= 0)
				continue;

			auto &minion = Monsters[mi - 1];
			if (minion.leaderRelation == LeaderRelation::Leashed && minion.leader == monsterId) {
				mcount++;
			}
		}
	}
	return mcount == monster.packsize;
}

bool PosOkMissile(Point position)
{
	return !TileHasAny(dPiece[position.x][position.y], TileProperties::BlockMissile);
}

bool LineClearMissile(Point startPoint, Point endPoint)
{
	return LineClear(PosOkMissile, startPoint, endPoint);
}

bool LineClear(const std::function<bool(Point)> &clear, Point startPoint, Point endPoint)
{
	Point position = startPoint;

	int dx = endPoint.x - position.x;
	int dy = endPoint.y - position.y;
	if (abs(dx) > abs(dy)) {
		if (dx < 0) {
			std::swap(position, endPoint);
			dx = -dx;
			dy = -dy;
		}
		int d;
		int yincD;
		int dincD;
		int dincH;
		if (dy > 0) {
			d = 2 * dy - dx;
			dincD = 2 * dy;
			dincH = 2 * (dy - dx);
			yincD = 1;
		} else {
			d = 2 * dy + dx;
			dincD = 2 * dy;
			dincH = 2 * (dx + dy);
			yincD = -1;
		}
		bool done = false;
		while (!done && position != endPoint) {
			if ((d <= 0) ^ (yincD < 0)) {
				d += dincD;
			} else {
				d += dincH;
				position.y += yincD;
			}
			position.x++;
			done = position != startPoint && !clear(position);
		}
	} else {
		if (dy < 0) {
			std::swap(position, endPoint);
			dy = -dy;
			dx = -dx;
		}
		int d;
		int xincD;
		int dincD;
		int dincH;
		if (dx > 0) {
			d = 2 * dx - dy;
			dincD = 2 * dx;
			dincH = 2 * (dx - dy);
			xincD = 1;
		} else {
			d = 2 * dx + dy;
			dincD = 2 * dx;
			dincH = 2 * (dy + dx);
			xincD = -1;
		}
		bool done = false;
		while (!done && position != endPoint) {
			if ((d <= 0) ^ (xincD < 0)) {
				d += dincD;
			} else {
				d += dincH;
				position.x += xincD;
			}
			position.y++;
			done = position != startPoint && !clear(position);
		}
	}
	return position == endPoint;
}

void SyncMonsterAnim(Monster &monster)
{
	monster.MType = &LevelMonsterTypes[monster._mMTidx];
#ifdef _DEBUG
	// fix for saves with debug monsters having type originally not on the level
	if (LevelMonsterTypes[monster._mMTidx].MData == nullptr) {
		InitMonsterGFX(monster._mMTidx);
		LevelMonsterTypes[monster._mMTidx].mdeadval = 1;
	}
#endif
	monster.MData = LevelMonsterTypes[monster._mMTidx].MData;
	if (monster._uniqtype != 0)
		monster.mName = pgettext("monster", UniqueMonstersData[monster._uniqtype - 1].mName).data();
	else
		monster.mName = pgettext("monster", monster.MData->mName).data();

	if (monster._uniqtype != 0)
		InitTRNForUniqueMonster(monster);

	MonsterGraphic graphic = MonsterGraphic::Stand;

	switch (monster._mmode) {
	case MonsterMode::Stand:
	case MonsterMode::Delay:
	case MonsterMode::Talk:
		break;
	case MonsterMode::MoveNorthwards:
	case MonsterMode::MoveSouthwards:
	case MonsterMode::MoveSideways:
		graphic = MonsterGraphic::Walk;
		break;
	case MonsterMode::MeleeAttack:
	case MonsterMode::RangedAttack:
		graphic = MonsterGraphic::Attack;
		break;
	case MonsterMode::HitRecovery:
		graphic = MonsterGraphic::GotHit;
		break;
	case MonsterMode::Death:
		graphic = MonsterGraphic::Death;
		break;
	case MonsterMode::SpecialMeleeAttack:
	case MonsterMode::FadeIn:
	case MonsterMode::FadeOut:
	case MonsterMode::SpecialStand:
	case MonsterMode::SpecialRangedAttack:
	case MonsterMode::Heal:
		graphic = MonsterGraphic::Special;
		break;
	case MonsterMode::Charge:
		graphic = MonsterGraphic::Attack;
		monster.AnimInfo.CurrentFrame = 0;
		break;
	default:
		monster.AnimInfo.CurrentFrame = 0;
		break;
	}

	monster.ChangeAnimationData(graphic);
}

void M_FallenFear(Point position)
{
	const Rectangle fearArea = Rectangle { position, 4 };
	for (const Point tile : PointsInRectangleRange { fearArea }) {
		if (!InDungeonBounds(tile))
			continue;
		int m = dMonster[tile.x][tile.y];
		if (m == 0)
			continue;
		Monster &monster = Monsters[abs(m) - 1];
		if (monster._mAi != AI_FALLEN || monster._mhitpoints >> 6 <= 0)
			continue;

		int runDistance = std::max((8 - monster.MData->mLevel), 2);
		monster._mgoal = MGOAL_RETREAT;
		monster._mgoalvar1 = runDistance;
		monster._mgoalvar2 = static_cast<int>(GetDirection(position, monster.position.tile));
	}
}

void PrintMonstHistory(int mt)
{
	if (*sgOptions.Gameplay.showMonsterType) {
		AddPanelString(fmt::format(fmt::runtime(_("Type: {:s}  Kills: {:d}")), GetMonsterTypeText(MonstersData[mt]), MonsterKillCounts[mt]));
	} else {
		AddPanelString(fmt::format(fmt::runtime(_("Total kills: {:d}")), MonsterKillCounts[mt]));
	}

	if (MonsterKillCounts[mt] >= 30) {
		int minHP = MonstersData[mt].mMinHP;
		int maxHP = MonstersData[mt].mMaxHP;
		if (!gbIsHellfire && mt == MT_DIABLO) {
			minHP /= 2;
			maxHP /= 2;
		}
		if (!gbIsMultiplayer) {
			minHP /= 2;
			maxHP /= 2;
		}
		if (minHP < 1)
			minHP = 1;
		if (maxHP < 1)
			maxHP = 1;

		int hpBonusNightmare = 1;
		int hpBonusHell = 3;
		if (gbIsHellfire) {
			hpBonusNightmare = (!gbIsMultiplayer ? 50 : 100);
			hpBonusHell = (!gbIsMultiplayer ? 100 : 200);
		}
		if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
			minHP = 3 * minHP + hpBonusNightmare;
			maxHP = 3 * maxHP + hpBonusNightmare;
		} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
			minHP = 4 * minHP + hpBonusHell;
			maxHP = 4 * maxHP + hpBonusHell;
		}
		AddPanelString(fmt::format(fmt::runtime(_("Hit Points: {:d}-{:d}")), minHP, maxHP));
	}
	if (MonsterKillCounts[mt] >= 15) {
		int res = (sgGameInitInfo.nDifficulty != DIFF_HELL) ? MonstersData[mt].mMagicRes : MonstersData[mt].mMagicRes2;
		if ((res & (RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING | IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING)) == 0) {
			AddPanelString(_("No magic resistance"));
		} else {
			if ((res & (RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING)) != 0) {
				std::string resists = std::string(_("Resists:"));
				if ((res & RESIST_MAGIC) != 0)
					AppendStrView(resists, _(" Magic"));
				if ((res & RESIST_FIRE) != 0)
					AppendStrView(resists, _(" Fire"));
				if ((res & RESIST_LIGHTNING) != 0)
					AppendStrView(resists, _(" Lightning"));
				AddPanelString(resists);
			}
			if ((res & (IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING)) != 0) {
				std::string immune = std::string(_("Immune:"));
				if ((res & IMMUNE_MAGIC) != 0)
					AppendStrView(immune, _(" Magic"));
				if ((res & IMMUNE_FIRE) != 0)
					AppendStrView(immune, _(" Fire"));
				if ((res & IMMUNE_LIGHTNING) != 0)
					AppendStrView(immune, _(" Lightning"));
				AddPanelString(immune);
			}
		}
	}
}

void PrintUniqueHistory()
{
	auto &monster = Monsters[pcursmonst];
	if (*sgOptions.Gameplay.showMonsterType) {
		AddPanelString(fmt::format(fmt::runtime(_("Type: {:s}")), GetMonsterTypeText(*monster.MData)));
	}

	int res = monster.mMagicRes & (RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING | IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING);
	if (res == 0) {
		AddPanelString(_("No resistances"));
		AddPanelString(_("No Immunities"));
	} else {
		if ((res & (RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING)) != 0)
			AddPanelString(_("Some Magic Resistances"));
		else
			AddPanelString(_("No resistances"));
		if ((res & (IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING)) != 0) {
			AddPanelString(_("Some Magic Immunities"));
		} else {
			AddPanelString(_("No Immunities"));
		}
	}
}

void PlayEffect(Monster &monster, int mode)
{
	if (MyPlayer->pLvlLoad != 0) {
		return;
	}

	int sndIdx = GenerateRnd(2);
	if (!gbSndInited || !gbSoundOn || gbBufferMsgs != 0) {
		return;
	}

	int mi = monster._mMTidx;
	TSnd *snd = LevelMonsterTypes[mi].Snds[mode][sndIdx].get();
	if (snd == nullptr || snd->isPlaying()) {
		return;
	}

	int lVolume = 0;
	int lPan = 0;
	if (!CalculateSoundPosition(monster.position.tile, &lVolume, &lPan))
		return;

	snd_play_snd(snd, lVolume, lPan);
}

void MissToMonst(Missile &missile, Point position)
{
	int m = missile._misource;

	assert(m >= 0 && m < MaxMonsters);
	auto &monster = Monsters[m];

	Point oldPosition = missile.position.tile;
	dMonster[position.x][position.y] = m + 1;
	monster._mdir = static_cast<Direction>(missile._mimfnum);
	monster.position.tile = position;
	M_StartStand(monster, monster._mdir);
	if ((monster._mFlags & MFLAG_TARGETS_MONSTER) == 0)
		M_StartHit(m, 0);
	else
		HitMonster(m, 0);

	if (monster.MType->mtype == MT_GLOOM)
		return;

	if ((monster._mFlags & MFLAG_TARGETS_MONSTER) == 0) {
		if (dPlayer[oldPosition.x][oldPosition.y] <= 0)
			return;

		int pnum = dPlayer[oldPosition.x][oldPosition.y] - 1;
		MonsterAttackPlayer(m, pnum, 500, monster.mMinDamage2, monster.mMaxDamage2);

		if (IsAnyOf(monster.MType->mtype, MT_NSNAKE, MT_RSNAKE, MT_BSNAKE, MT_GSNAKE))
			return;

		Player &player = Players[pnum];
		if (player._pmode != PM_GOTHIT && player._pmode != PM_DEATH)
			StartPlrHit(pnum, 0, true);
		Point newPosition = oldPosition + monster._mdir;
		if (PosOkPlayer(player, newPosition)) {
			player.position.tile = newPosition;
			FixPlayerLocation(player, player._pdir);
			FixPlrWalkTags(pnum);
			dPlayer[newPosition.x][newPosition.y] = pnum + 1;
			SetPlayerOld(player);
		}
		return;
	}

	if (dMonster[oldPosition.x][oldPosition.y] <= 0)
		return;

	int mid = dMonster[oldPosition.x][oldPosition.y] - 1;
	MonsterAttackMonster(m, mid, 500, monster.mMinDamage2, monster.mMaxDamage2);

	if (IsAnyOf(monster.MType->mtype, MT_NSNAKE, MT_RSNAKE, MT_BSNAKE, MT_GSNAKE))
		return;

	Point newPosition = oldPosition + monster._mdir;
	if (IsTileAvailable(Monsters[mid], newPosition)) {
		m = dMonster[oldPosition.x][oldPosition.y];
		dMonster[newPosition.x][newPosition.y] = m;
		dMonster[oldPosition.x][oldPosition.y] = 0;
		m--;
		monster.position.tile = newPosition;
		monster.position.future = newPosition;
	}
}

bool IsTileAvailable(const Monster &monster, Point position)
{
	if (!IsTileAvailable(position))
		return false;

	return IsTileSafe(monster, position);
}

bool IsSkel(_monster_id mt)
{
	return std::find(std::begin(SkeletonTypes), std::end(SkeletonTypes), mt) != std::end(SkeletonTypes);
}

bool IsGoat(_monster_id mt)
{
	return IsAnyOf(mt,
	    MT_NGOATMC, MT_BGOATMC, MT_RGOATMC, MT_GGOATMC,
	    MT_NGOATBW, MT_BGOATBW, MT_RGOATBW, MT_GGOATBW);
}

bool SpawnSkeleton(int ii, Point position)
{
	if (ii == -1)
		return false;

	if (IsTileAvailable(position)) {
		Direction dir = GetDirection(position, position); // TODO useless calculation
		ActivateSpawn(ii, position, dir);
		return true;
	}

	bool monstok[3][3];

	bool savail = false;
	int yy = 0;
	for (int j = position.y - 1; j <= position.y + 1; j++) {
		int xx = 0;
		for (int k = position.x - 1; k <= position.x + 1; k++) {
			monstok[xx][yy] = IsTileAvailable({ k, j });
			savail = savail || monstok[xx][yy];
			xx++;
		}
		yy++;
	}
	if (!savail) {
		return false;
	}

	int rs = GenerateRnd(15) + 1;
	int x2 = 0;
	int y2 = 0;
	while (rs > 0) {
		if (monstok[x2][y2])
			rs--;
		if (rs > 0) {
			x2++;
			if (x2 == 3) {
				x2 = 0;
				y2++;
				if (y2 == 3)
					y2 = 0;
			}
		}
	}

	Point spawn = position + Displacement { x2 - 1, y2 - 1 };
	Direction dir = GetDirection(spawn, position);
	ActivateSpawn(ii, spawn, dir);

	return true;
}

int PreSpawnSkeleton()
{
	int skel = AddSkeleton({ 0, 0 }, Direction::South, false);
	if (skel != -1)
		M_StartStand(Monsters[skel], Direction::South);

	return skel;
}

void TalktoMonster(Monster &monster)
{
	Player &player = Players[monster._menemy];
	monster._mmode = MonsterMode::Talk;
	if (monster._mAi != AI_SNOTSPIL && monster._mAi != AI_LACHDAN) {
		return;
	}

	if (Quests[Q_LTBANNER].IsAvailable() && Quests[Q_LTBANNER]._qvar1 == 2) {
		if (RemoveInventoryItemById(player, IDI_BANNER)) {
			Quests[Q_LTBANNER]._qactive = QUEST_DONE;
			monster.mtalkmsg = TEXT_BANNER12;
			monster._mgoal = MGOAL_INQUIRING;
		}
	}
	if (Quests[Q_VEIL].IsAvailable() && monster.mtalkmsg >= TEXT_VEIL9) {
		if (RemoveInventoryItemById(player, IDI_GLDNELIX)) {
			monster.mtalkmsg = TEXT_VEIL11;
			monster._mgoal = MGOAL_INQUIRING;
		}
	}
}

void SpawnGolem(int id, Point position, Missile &missile)
{
	assert(id >= 0 && id < MAX_PLRS);
	Player &player = Players[id];
	auto &golem = Monsters[id];

	dMonster[position.x][position.y] = id + 1;
	golem.position.tile = position;
	golem.position.future = position;
	golem.position.old = position;
	golem._pathcount = 0;
	golem._mmaxhp = 2 * (320 * missile._mispllvl + player._pMaxMana / 3);
	golem._mhitpoints = golem._mmaxhp;
	golem.mArmorClass = 25;
	golem.mHit = 5 * (missile._mispllvl + 8) + 2 * player._pLevel;
	golem.mMinDamage = 2 * (missile._mispllvl + 4);
	golem.mMaxDamage = 2 * (missile._mispllvl + 8);
	golem._mFlags |= MFLAG_GOLEM;
	StartSpecialStand(golem, Direction::South);
	UpdateEnemy(golem);
	if (id == MyPlayerId) {
		NetSendCmdGolem(
		    golem.position.tile.x,
		    golem.position.tile.y,
		    golem._mdir,
		    golem._menemy,
		    golem._mhitpoints,
		    GetLevelForMultiplayer(player));
	}
}

bool CanTalkToMonst(const Monster &monster)
{
	return IsAnyOf(monster._mgoal, MGOAL_INQUIRING, MGOAL_TALKING);
}

int encode_enemy(Monster &monster)
{
	if ((monster._mFlags & MFLAG_TARGETS_MONSTER) != 0)
		return monster._menemy + MAX_PLRS;

	return monster._menemy;
}

void decode_enemy(Monster &monster, int enemyId)
{
	if (enemyId < MAX_PLRS) {
		monster._mFlags &= ~MFLAG_TARGETS_MONSTER;
		monster._menemy = enemyId;
		monster.enemyPosition = Players[enemyId].position.future;
	} else {
		monster._mFlags |= MFLAG_TARGETS_MONSTER;
		enemyId -= MAX_PLRS;
		monster._menemy = enemyId;
		monster.enemyPosition = Monsters[enemyId].position.future;
	}
}

void Monster::CheckStandAnimationIsLoaded(Direction mdir)
{
	if (IsAnyOf(_mmode, MonsterMode::Stand, MonsterMode::Talk)) {
		_mdir = mdir;
		ChangeAnimationData(MonsterGraphic::Stand);
	}
}

void Monster::Petrify()
{
	_mmode = MonsterMode::Petrified;
	AnimInfo.IsPetrified = true;
}

bool Monster::IsWalking() const
{
	switch (_mmode) {
	case MonsterMode::MoveNorthwards:
	case MonsterMode::MoveSouthwards:
	case MonsterMode::MoveSideways:
		return true;
	default:
		return false;
	}
}

bool Monster::IsImmune(missile_id mName) const
{
	missile_resistance missileElement = MissilesData[mName].mResist;

	if (((mMagicRes & IMMUNE_MAGIC) != 0 && missileElement == MISR_MAGIC)
	    || ((mMagicRes & IMMUNE_FIRE) != 0 && missileElement == MISR_FIRE)
	    || ((mMagicRes & IMMUNE_LIGHTNING) != 0 && missileElement == MISR_LIGHTNING)
	    || ((mMagicRes & IMMUNE_ACID) != 0 && missileElement == MISR_ACID))
		return true;
	if (mName == MIS_HBOLT && MType->mtype != MT_DIABLO && MData->mMonstClass != MonsterClass::Undead)
		return true;
	return false;
}

bool Monster::IsResistant(missile_id mName) const
{
	missile_resistance missileElement = MissilesData[mName].mResist;

	if (((mMagicRes & RESIST_MAGIC) != 0 && missileElement == MISR_MAGIC)
	    || ((mMagicRes & RESIST_FIRE) != 0 && missileElement == MISR_FIRE)
	    || ((mMagicRes & RESIST_LIGHTNING) != 0 && missileElement == MISR_LIGHTNING))
		return true;
	if (gbIsHellfire && mName == MIS_HBOLT && IsAnyOf(MType->mtype, MT_DIABLO, MT_BONEDEMN))
		return true;
	return false;
}

bool Monster::IsPossibleToHit() const
{
	return !(_mhitpoints >> 6 <= 0
	    || mtalkmsg != TEXT_NONE
	    || (MType->mtype == MT_ILLWEAV && _mgoal == MGOAL_RETREAT)
	    || _mmode == MonsterMode::Charge
	    || (IsAnyOf(MType->mtype, MT_COUNSLR, MT_MAGISTR, MT_CABALIST, MT_ADVOCATE) && _mgoal != MGOAL_NORMAL));
}

bool Monster::TryLiftGargoyle()
{
	if (_mAi == AI_GARG && (_mFlags & MFLAG_ALLOW_SPECIAL) != 0) {
		_mFlags &= ~MFLAG_ALLOW_SPECIAL;
		_mmode = MonsterMode::SpecialMeleeAttack;
		return true;
	}
	return false;
}

} // namespace devilution
