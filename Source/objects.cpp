/**
 * @file objects.cpp
 *
 * Implementation of object functionality, interaction, spawning, loading, etc.
 */
#include <climits>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <string>

#include <algorithm>

#include <expected.hpp>
#include <fmt/core.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "cursor.h"
#ifdef _DEBUG
#include "debug.h"
#endif
#include "diablo_msg.hpp"
#include "engine/backbuffer_state.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_file.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "headless_mode.hpp"
#include "inv.h"
#include "inv_iterators.hpp"
#include "levels/crypt.h"
#include "levels/drlg_l4.h"
#include "levels/setmaps.h"
#include "levels/themes.h"
#include "levels/tile_properties.hpp"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "monster.h"
#include "objdat.h"
#include "options.h"
#include "qol/stash.h"
#include "stores.h"
#include "towners.h"
#include "track.h"
#include "utils/algorithm/container.hpp"
#include "utils/endian_swap.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

Object Objects[MAXOBJECTS];
int AvailableObjects[MAXOBJECTS];
int ActiveObjects[MAXOBJECTS];
int ActiveObjectCount;
bool LoadingMapObjects;

namespace {

enum shrine_type : uint8_t {
	ShrineMysterious,
	ShrineHidden,
	ShrineGloomy,
	ShrineWeird,
	ShrineMagical,
	ShrineStone,
	ShrineReligious,
	ShrineEnchanted,
	ShrineThaumaturgic,
	ShrineFascinating,
	ShrineCryptic,
	ShrineMagicaL2,
	ShrineEldritch,
	ShrineEerie,
	ShrineDivine,
	ShrineHoly,
	ShrineSacred,
	ShrineSpiritual,
	ShrineSpooky,
	ShrineAbandoned,
	ShrineCreepy,
	ShrineQuiet,
	ShrineSecluded,
	ShrineOrnate,
	ShrineGlimmering,
	ShrineTainted,
	ShrineOily,
	ShrineGlowing,
	ShrineMendicant,
	ShrineSparkling,
	ShrineTown,
	ShrineShimmering,
	ShrineSolar,
	ShrineMurphys,
	NumberOfShrineTypes
};

enum {
	// clang-format off
	DOOR_CLOSED  =  0,
	DOOR_OPEN    =  1,
	DOOR_BLOCKED =  2,
	// clang-format on
};

int trapid;
int trapdir;
OptionalOwnedClxSpriteList pObjCels[40];
object_graphic_id ObjFileList[40];
/** Specifies the number of active objects. */
int leverid;
int numobjfiles;

/** Tracks progress through the tome sequence that spawns Na-Krul (see OperateNakrulBook()) */
int NaKrulTomeSequence;

/** Specifies the X-coordinate delta between barrels. */
int bxadd[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
/** Specifies the Y-coordinate delta between barrels. */
int byadd[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
/** Maps from shrine_id to shrine name. */
const char *const ShrineNames[] = {
	// TRANSLATORS: Shrine Name Block
	N_("Mysterious"),
	N_("Hidden"),
	N_("Gloomy"),
	N_("Weird"),
	N_("Magical"),
	N_("Stone"),
	N_("Religious"),
	N_("Enchanted"),
	N_("Thaumaturgic"),
	N_("Fascinating"),
	N_("Cryptic"),
	N_("Magical"),
	N_("Eldritch"),
	N_("Eerie"),
	N_("Divine"),
	N_("Holy"),
	N_("Sacred"),
	N_("Spiritual"),
	N_("Spooky"),
	N_("Abandoned"),
	N_("Creepy"),
	N_("Quiet"),
	N_("Secluded"),
	N_("Ornate"),
	N_("Glimmering"),
	N_("Tainted"),
	N_("Oily"),
	N_("Glowing"),
	N_("Mendicant's"),
	N_("Sparkling"),
	N_("Town"),
	N_("Shimmering"),
	N_("Solar"),
	// TRANSLATORS: Shrine Name Block end
	N_("Murphy's"),
};

/**
 * Specifies the game type for which each shrine may appear.
 * ShrineTypeAny - sp & mp
 * ShrineTypeSingle - sp only
 * ShrineTypeMulti - mp only
 */
enum shrine_gametype : uint8_t {
	ShrineTypeAny,
	ShrineTypeSingle,
	ShrineTypeMulti,
};

shrine_gametype shrineavail[] = {
	ShrineTypeAny,    // Mysterious
	ShrineTypeAny,    // Hidden
	ShrineTypeSingle, // Gloomy
	ShrineTypeSingle, // Weird
	ShrineTypeAny,    // Magical
	ShrineTypeAny,    // Stone
	ShrineTypeAny,    // Religious
	ShrineTypeAny,    // Enchanted
	ShrineTypeSingle, // Thaumaturgic
	ShrineTypeAny,    // Fascinating
	ShrineTypeAny,    // Cryptic
	ShrineTypeAny,    // Magical
	ShrineTypeAny,    // Eldritch
	ShrineTypeAny,    // Eerie
	ShrineTypeAny,    // Divine
	ShrineTypeAny,    // Holy
	ShrineTypeAny,    // Sacred
	ShrineTypeAny,    // Spiritual
	ShrineTypeMulti,  // Spooky
	ShrineTypeAny,    // Abandoned
	ShrineTypeAny,    // Creepy
	ShrineTypeAny,    // Quiet
	ShrineTypeAny,    // Secluded
	ShrineTypeAny,    // Ornate
	ShrineTypeAny,    // Glimmering
	ShrineTypeMulti,  // Tainted
	ShrineTypeAny,    // Oily
	ShrineTypeAny,    // Glowing
	ShrineTypeAny,    // Mendicant's
	ShrineTypeAny,    // Sparkling
	ShrineTypeAny,    // Town
	ShrineTypeAny,    // Shimmering
	ShrineTypeSingle, // Solar,
	ShrineTypeAny,    // Murphy's
};
/** Maps from book_id to book name. */
const char *const StoryBookName[] = {
	N_(/* TRANSLATORS: Book Title */ "The Great Conflict"),
	N_(/* TRANSLATORS: Book Title */ "The Wages of Sin are War"),
	N_(/* TRANSLATORS: Book Title */ "The Tale of the Horadrim"),
	N_(/* TRANSLATORS: Book Title */ "The Dark Exile"),
	N_(/* TRANSLATORS: Book Title */ "The Sin War"),
	N_(/* TRANSLATORS: Book Title */ "The Binding of the Three"),
	N_(/* TRANSLATORS: Book Title */ "The Realms Beyond"),
	N_(/* TRANSLATORS: Book Title */ "Tale of the Three"),
	N_(/* TRANSLATORS: Book Title */ "The Black King"),
	N_(/* TRANSLATORS: Book Title */ "Journal: The Ensorcellment"),
	N_(/* TRANSLATORS: Book Title */ "Journal: The Meeting"),
	N_(/* TRANSLATORS: Book Title */ "Journal: The Tirade"),
	N_(/* TRANSLATORS: Book Title */ "Journal: His Power Grows"),
	N_(/* TRANSLATORS: Book Title */ "Journal: NA-KRUL"),
	N_(/* TRANSLATORS: Book Title */ "Journal: The End"),
	N_(/* TRANSLATORS: Book Title */ "A Spellbook"),
};
/** Specifies the speech IDs of each dungeon type narrator book, for each player class. */
_speech_id StoryText[3][3] = {
	{ TEXT_BOOK11, TEXT_BOOK12, TEXT_BOOK13 },
	{ TEXT_BOOK21, TEXT_BOOK22, TEXT_BOOK23 },
	{ TEXT_BOOK31, TEXT_BOOK32, TEXT_BOOK33 }
};

bool RndLocOk(Point p)
{
	if (dMonster[p.x][p.y] != 0)
		return false;
	if (dPlayer[p.x][p.y] != 0)
		return false;
	if (IsObjectAtPosition(p))
		return false;
	if (TileContainsSetPiece(p))
		return false;
	if (TileHasAny(p, TileProperties::Solid))
		return false;
	return IsNoneOf(leveltype, DTYPE_CATHEDRAL, DTYPE_CRYPT) || dPiece[p.x][p.y] <= 125 || dPiece[p.x][p.y] >= 143;
}

bool IsAreaOk(Rectangle rect)
{
	return c_all_of(PointsInRectangle(rect), &RndLocOk);
}

bool CanPlaceWallTrap(Point pos)
{
	if (dObject[pos.x][pos.y] != 0)
		return false;
	if (TileContainsSetPiece(pos))
		return false;

	return TileHasAny(pos, TileProperties::Trap);
}

void InitRndLocObj(int min, int max, _object_id objtype)
{
	const int numobjs = GenerateRnd(max - min) + min;

	for (int i = 0; i < numobjs; i++) {
		while (true) {
			const int xp = GenerateRnd(80) + 16;
			const int yp = GenerateRnd(80) + 16;
			if (IsAreaOk(Rectangle { { xp - 1, yp - 1 }, { 3, 3 } })) {
				AddObject(objtype, { xp, yp });
				break;
			}
		}
	}
}

void InitRndLocBigObj(int min, int max, _object_id objtype)
{
	const int numobjs = GenerateRnd(max - min) + min;
	for (int i = 0; i < numobjs; i++) {
		while (true) {
			const int xp = GenerateRnd(80) + 16;
			const int yp = GenerateRnd(80) + 16;
			if (IsAreaOk(Rectangle { { xp - 1, yp - 2 }, { 3, 4 } })) {
				AddObject(objtype, { xp, yp });
				break;
			}
		}
	}
}

bool CanPlaceRandomObject(Point position, Displacement standoff)
{
	return IsAreaOk(Rectangle { position - standoff,
	    Size { standoff.deltaX * 2 + 1, standoff.deltaY * 2 + 1 } });
}

std::optional<Point> GetRandomObjectPosition(Displacement standoff)
{
	for (int i = 0; i <= 20000; i++) {
		Point position = Point { GenerateRnd(80), GenerateRnd(80) } + Displacement { 16, 16 };
		if (CanPlaceRandomObject(position, standoff))
			return position;
	}
	return {};
}

void InitRndLocObj5x5(int min, int max, _object_id objtype)
{
	const int numobjs = min + GenerateRnd(max - min);
	for (int i = 0; i < numobjs; i++) {
		std::optional<Point> position = GetRandomObjectPosition({ 2, 2 });
		if (!position)
			return;
		AddObject(objtype, *position);
	}
}

void ClrAllObjects()
{
	for (Object &object : Objects) {
		object = {};
	}
	ActiveObjectCount = 0;
	for (int i = 0; i < MAXOBJECTS; i++) {
		AvailableObjects[i] = i;
	}
	memset(ActiveObjects, 0, sizeof(ActiveObjects));
	trapdir = 0;
	trapid = 1;
	leverid = 1;
}

void AddTortures()
{
	for (int oy = 0; oy < MAXDUNY; oy++) {
		for (int ox = 0; ox < MAXDUNX; ox++) {
			if (dPiece[ox][oy] == 366) {
				AddObject(OBJ_TORTURE1, { ox, oy + 1 });
				AddObject(OBJ_TORTURE3, { ox + 2, oy - 1 });
				AddObject(OBJ_TORTURE2, { ox, oy + 3 });
				AddObject(OBJ_TORTURE4, { ox + 4, oy - 1 });
				AddObject(OBJ_TORTURE5, { ox, oy + 5 });
				AddObject(OBJ_TNUDEM1, { ox + 1, oy + 3 });
				AddObject(OBJ_TNUDEM2, { ox + 4, oy + 5 });
				AddObject(OBJ_TNUDEM3, { ox + 2, oy });
				AddObject(OBJ_TNUDEM4, { ox + 3, oy + 2 });
				AddObject(OBJ_TNUDEW1, { ox + 2, oy + 4 });
				AddObject(OBJ_TNUDEW2, { ox + 2, oy + 1 });
				AddObject(OBJ_TNUDEW3, { ox + 4, oy + 2 });
			}
		}
	}
}

void AddCandles()
{
	const int tx = Quests[Q_PWATER].position.x;
	const int ty = Quests[Q_PWATER].position.y;
	AddObject(OBJ_STORYCANDLE, { tx - 2, ty + 1 });
	AddObject(OBJ_STORYCANDLE, { tx + 3, ty + 1 });
	AddObject(OBJ_STORYCANDLE, { tx - 1, ty + 2 });
	AddObject(OBJ_STORYCANDLE, { tx + 2, ty + 2 });
}

/**
 * @brief Attempts to spawn a book somewhere on the current floor which when activated will change a region of the map.
 *
 * This object acts like a lever and will cause a change to the map based on what quest is active. The exact effect is
 * determined by OperateBookLever().
 *
 * @param affectedArea The map region to be updated when this object is activated by the player.
 * @param msg The quest text to play when the player activates the book.
 */
void AddBookLever(_object_id type, WorldTileRectangle affectedArea, _speech_id msg)
{
	std::optional<Point> position = GetRandomObjectPosition({ 2, 2 });
	if (!position)
		return;

	if (type == OBJ_BLOODBOOK)
		position = SetPiece.position.megaToWorld() + Displacement { 9, 24 };

	Object *lever = AddObject(type, *position);
	assert(lever != nullptr);

	lever->InitializeQuestBook(affectedArea, leverid, msg);
	leverid++;
}

void InitRndBarrels()
{
	_object_id barrelId = OBJ_BARREL;
	_object_id explosiveBarrelId = OBJ_BARRELEX;
	if (leveltype == DTYPE_NEST) {
		barrelId = OBJ_POD;
		explosiveBarrelId = OBJ_PODEX;
	} else if (leveltype == DTYPE_CRYPT) {
		barrelId = OBJ_URN;
		explosiveBarrelId = OBJ_URNEX;
	}

	/** number of groups of barrels to generate */
	const int numobjs = GenerateRnd(5) + 3;
	for (int i = 0; i < numobjs; i++) {
		int xp;
		int yp;
		do {
			xp = GenerateRnd(80) + 16;
			yp = GenerateRnd(80) + 16;
		} while (!RndLocOk({ xp, yp }));
		_object_id o = FlipCoin(4) ? explosiveBarrelId : barrelId;
		AddObject(o, { xp, yp });
		bool found = true;
		/** regulates chance to stop placing barrels in current group */
		int p = 0;
		/** number of barrels in current group */
		int c = 1;
		while (FlipCoin(p) && found) {
			/** number of tries of placing next barrel in current group */
			int t = 0;
			found = false;
			while (true) {
				if (t >= 3)
					break;
				const int dir = GenerateRnd(8);
				xp += bxadd[dir];
				yp += byadd[dir];
				found = RndLocOk({ xp, yp });
				t++;
				if (found)
					break;
			}
			if (found) {
				o = FlipCoin(5) ? explosiveBarrelId : barrelId;
				AddObject(o, { xp, yp });
				c++;
			}
			p = c / 2;
		}
	}
}

void AddL2Torches()
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			const Point testPosition = { i, j };
			if (TileContainsSetPiece(testPosition))
				continue;

			const int pn = dPiece[i][j];
			if (pn == 0 && FlipCoin(3)) {
				AddObject(OBJ_TORCHL2, testPosition);
			}

			if (pn == 4 && FlipCoin(3)) {
				AddObject(OBJ_TORCHR2, testPosition);
			}

			if (pn == 36 && FlipCoin(10) && !IsObjectAtPosition(testPosition + Direction::NorthWest)) {
				AddObject(OBJ_TORCHL, testPosition + Direction::NorthWest);
			}

			if (pn == 40 && FlipCoin(10) && !IsObjectAtPosition(testPosition + Direction::NorthEast)) {
				AddObject(OBJ_TORCHR, testPosition + Direction::NorthEast);
			}
		}
	}
}

void AddObjTraps()
{
	int rndv;
	if (currlevel == 1)
		rndv = 10;
	if (currlevel >= 2)
		rndv = 15;
	if (currlevel >= 5)
		rndv = 20;
	if (currlevel >= 7)
		rndv = 25;
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			Object *triggerObject = FindObjectAtPosition({ i, j }, false);
			if (triggerObject == nullptr || GenerateRnd(100) >= rndv)
				continue;

			if (!AllObjects[triggerObject->_otype].isTrap())
				continue;

			Object *trapObject = nullptr;
			if (FlipCoin()) {
				int xp = i - 1;
				while (IsTileNotSolid({ xp, j }))
					xp--;

				if (!CanPlaceWallTrap({ xp, j }) || i - xp <= 1)
					continue;

				trapObject = AddObject(OBJ_TRAPL, { xp, j });
			} else {
				int yp = j - 1;
				while (IsTileNotSolid({ i, yp }))
					yp--;

				if (!CanPlaceWallTrap({ i, yp }) || j - yp <= 1)
					continue;

				trapObject = AddObject(OBJ_TRAPR, { i, yp });
			}

			if (trapObject != nullptr) {
				// nullptr check just in case we fail to find a valid location to place a trap in the chosen direction
				trapObject->_oVar1 = i;
				trapObject->_oVar2 = j;
				triggerObject->_oTrapFlag = true;
			}
		}
	}
}

void AddChestTraps()
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) { // NOLINT(modernize-loop-convert)
			Object *chestObject = FindObjectAtPosition({ i, j }, false);
			if (chestObject != nullptr && chestObject->IsUntrappedChest() && GenerateRnd(100) < 10) {
				switch (chestObject->_otype) {
				case OBJ_CHEST1:
					chestObject->_otype = OBJ_TCHEST1;
					break;
				case OBJ_CHEST2:
					chestObject->_otype = OBJ_TCHEST2;
					break;
				case OBJ_CHEST3:
					chestObject->_otype = OBJ_TCHEST3;
					break;
				default:
					break;
				}
				chestObject->_oTrapFlag = true;
				if (leveltype == DTYPE_CATACOMBS) {
					chestObject->_oVar4 = GenerateRnd(2);
				} else {
					chestObject->_oVar4 = GenerateRnd(gbIsHellfire ? 6 : 3);
				}
			}
		}
	}
}

void LoadMapObjects(const char *path, Point start, WorldTileRectangle mapRange = {}, int leveridx = 0)
{
	LoadingMapObjects = true;

	auto dunData = LoadFileInMem<uint16_t>(path);

	WorldTileSize size = GetDunSize(dunData.get());

	const int layer2Offset = 2 + size.width * size.height;

	// The rest of the layers are at dPiece scale
	size *= static_cast<WorldTileCoord>(2);

	const uint16_t *objectLayer = &dunData[layer2Offset + size.width * size.height * 2];

	for (WorldTileCoord j = 0; j < size.height; j++) {
		for (WorldTileCoord i = 0; i < size.width; i++) {
			auto objectId = static_cast<uint8_t>(Swap16LE(objectLayer[j * size.width + i]));
			if (objectId != 0) {
				const Point mapPos = start + Displacement { i, j };
				Object *mapObject = AddObject(ObjTypeConv[objectId], mapPos);
				if (leveridx > 0 && mapObject != nullptr)
					mapObject->InitializeLoadedObject(mapRange, leveridx);
			}
		}
	}

	LoadingMapObjects = false;
}

void AddDiabObjs()
{
	LoadMapObjects("levels\\l4data\\diab1.dun", DiabloQuad1.megaToWorld(), { DiabloQuad2, { 11, 12 } }, 1);
	LoadMapObjects("levels\\l4data\\diab2a.dun", DiabloQuad2.megaToWorld(), { DiabloQuad3, { 11, 11 } }, 2);
	LoadMapObjects("levels\\l4data\\diab3a.dun", DiabloQuad3.megaToWorld(), { DiabloQuad4, { 9, 9 } }, 3);
}

void AddCryptObject(Object &object, int a2)
{
	if (a2 > 5) {
		const Player &myPlayer = *MyPlayer;
		switch (a2) {
		case 6:
			switch (myPlayer._pClass) {
			case HeroClass::Warrior:
			case HeroClass::Barbarian:
				object._oVar2 = TEXT_BOOKA;
				break;
			case HeroClass::Rogue:
				object._oVar2 = TEXT_RBOOKA;
				break;
			case HeroClass::Sorcerer:
				object._oVar2 = TEXT_MBOOKA;
				break;
			case HeroClass::Monk:
				object._oVar2 = TEXT_OBOOKA;
				break;
			case HeroClass::Bard:
				object._oVar2 = TEXT_BBOOKA;
				break;
			default:
				break;
			}
			break;
		case 7:
			switch (myPlayer._pClass) {
			case HeroClass::Warrior:
			case HeroClass::Barbarian:
				object._oVar2 = TEXT_BOOKB;
				break;
			case HeroClass::Rogue:
				object._oVar2 = TEXT_RBOOKB;
				break;
			case HeroClass::Sorcerer:
				object._oVar2 = TEXT_MBOOKB;
				break;
			case HeroClass::Monk:
				object._oVar2 = TEXT_OBOOKB;
				break;
			case HeroClass::Bard:
				object._oVar2 = TEXT_BBOOKB;
				break;
			default:
				break;
			}
			break;
		case 8:
			switch (myPlayer._pClass) {
			case HeroClass::Warrior:
			case HeroClass::Barbarian:
				object._oVar2 = TEXT_BOOKC;
				break;
			case HeroClass::Rogue:
				object._oVar2 = TEXT_RBOOKC;
				break;
			case HeroClass::Sorcerer:
				object._oVar2 = TEXT_MBOOKC;
				break;
			case HeroClass::Monk:
				object._oVar2 = TEXT_OBOOKC;
				break;
			case HeroClass::Bard:
				object._oVar2 = TEXT_BBOOKC;
				break;
			default:
				break;
			}
			break;
		}
		object._oVar3 = 15;
		object._oVar8 = a2;
	} else {
		object._oVar2 = a2 + TEXT_SKLJRN;
		object._oVar3 = a2 + 9;
		object._oVar8 = 0;
	}
	object._oVar1 = 1;
	object._oAnimFrame = 5 - 2 * object._oVar1;
	object._oVar4 = object._oAnimFrame + 1;
}

void SetupObject(Object &object, Point position, _object_id ot)
{
	const ObjectData &objectData = AllObjects[ot];
	object._otype = ot;
	object_graphic_id ofi = objectData.ofindex;
	object.position = position;

	if (!HeadlessMode) {
		const auto &found = c_find(ObjFileList, ofi);
		if (found == std::end(ObjFileList)) {
			LogCritical("Unable to find object_graphic_id {} in list of objects to load, level generation error.", static_cast<int>(ofi));
			return;
		}

		const size_t j = std::distance(std::begin(ObjFileList), found);

		if (pObjCels[j]) {
			object._oAnimData.emplace(*pObjCels[j]);
		} else {
			object._oAnimData = std::nullopt;
		}
	}
	object._oAnimFlag = objectData.isAnimated();
	if (object._oAnimFlag) {
		object._oAnimDelay = objectData.animDelay;
		object._oAnimCnt = GenerateRnd(object._oAnimDelay);
		object._oAnimLen = objectData.animLen;
		object._oAnimFrame = GenerateRnd(object._oAnimLen - 1) + 1;
	} else {
		object._oAnimDelay = 1000;
		object._oAnimCnt = 0;
		object._oAnimLen = objectData.animLen;
		object._oAnimFrame = objectData.animDelay;
	}
	object._oAnimWidth = objectData.animWidth;
	object._oSolidFlag = objectData.isSolid() ? 1 : 0;
	object._oMissFlag = objectData.missilesPassThrough() ? 1 : 0;
	object.applyLighting = objectData.applyLighting();
	object._oDelFlag = false;
	object._oBreak = objectData.isBreakable() ? 1 : 0;
	object.selectionRegion = objectData.selectionRegion;
	object._oPreFlag = false;
	object._oTrapFlag = false;
	object._oDoorFlag = false;
}

void AddCryptBook(_object_id ot, int v2, Point position)
{
	if (ActiveObjectCount >= MAXOBJECTS)
		return;

	const int oi = AvailableObjects[0];
	AvailableObjects[0] = AvailableObjects[MAXOBJECTS - 1 - ActiveObjectCount];
	ActiveObjects[ActiveObjectCount] = oi;
	dObject[position.x][position.y] = oi + 1;
	Object &object = Objects[oi];
	SetupObject(object, position, ot);
	AddCryptObject(object, v2);
	ActiveObjectCount++;
}

void AddCryptStoryBook(int s)
{
	std::optional<Point> position = GetRandomObjectPosition({ 3, 2 });
	if (!position)
		return;
	AddCryptBook(OBJ_L5BOOKS, s, *position);
	AddObject(OBJ_L5CANDLE, *position + Displacement { -2, 1 });
	AddObject(OBJ_L5CANDLE, *position + Displacement { -2, 0 });
	AddObject(OBJ_L5CANDLE, *position + Displacement { -1, -1 });
	AddObject(OBJ_L5CANDLE, *position + Displacement { 1, -1 });
	AddObject(OBJ_L5CANDLE, *position + Displacement { 2, 0 });
	AddObject(OBJ_L5CANDLE, *position + Displacement { 2, 1 });
}

void AddNakrulLever()
{
	while (true) {
		const int xp = GenerateRnd(80) + 16;
		const int yp = GenerateRnd(80) + 16;
		if (IsAreaOk(Rectangle { { xp - 1, yp - 1 }, { 3, 3 } })) {
			break;
		}
	}
	AddObject(OBJ_L5LEVER, { UberRow + 3, UberCol - 1 });
}

void AddNakrulBook(int a1, Point position)
{
	AddCryptBook(OBJ_L5BOOKS, a1, position);
}

void AddNakrulGate()
{
	AddNakrulLever();
	switch (GenerateRnd(6)) {
	case 0:
		AddNakrulBook(6, { UberRow + 3, UberCol });
		AddNakrulBook(7, { UberRow + 2, UberCol - 3 });
		AddNakrulBook(8, { UberRow + 2, UberCol + 2 });
		break;
	case 1:
		AddNakrulBook(6, { UberRow + 3, UberCol });
		AddNakrulBook(8, { UberRow + 2, UberCol - 3 });
		AddNakrulBook(7, { UberRow + 2, UberCol + 2 });
		break;
	case 2:
		AddNakrulBook(7, { UberRow + 3, UberCol });
		AddNakrulBook(6, { UberRow + 2, UberCol - 3 });
		AddNakrulBook(8, { UberRow + 2, UberCol + 2 });
		break;
	case 3:
		AddNakrulBook(7, { UberRow + 3, UberCol });
		AddNakrulBook(8, { UberRow + 2, UberCol - 3 });
		AddNakrulBook(6, { UberRow + 2, UberCol + 2 });
		break;
	case 4:
		AddNakrulBook(8, { UberRow + 3, UberCol });
		AddNakrulBook(7, { UberRow + 2, UberCol - 3 });
		AddNakrulBook(6, { UberRow + 2, UberCol + 2 });
		break;
	case 5:
		AddNakrulBook(8, { UberRow + 3, UberCol });
		AddNakrulBook(6, { UberRow + 2, UberCol - 3 });
		AddNakrulBook(7, { UberRow + 2, UberCol + 2 });
		break;
	}
}

void AddStoryBooks()
{
	std::optional<Point> position = GetRandomObjectPosition({ 3, 2 });
	if (!position)
		return;

	AddObject(OBJ_STORYBOOK, *position);
	AddObject(OBJ_STORYCANDLE, *position + Displacement { -2, 1 });
	AddObject(OBJ_STORYCANDLE, *position + Displacement { -2, 0 });
	AddObject(OBJ_STORYCANDLE, *position + Displacement { -1, -1 });
	AddObject(OBJ_STORYCANDLE, *position + Displacement { 1, -1 });
	AddObject(OBJ_STORYCANDLE, *position + Displacement { 2, 0 });
	AddObject(OBJ_STORYCANDLE, *position + Displacement { 2, 1 });
}

void AddHookedBodies(int freq)
{
	for (WorldTileCoord j = 0; j < DMAXY; j++) {
		const WorldTileCoord jj = 16 + j * 2;
		for (WorldTileCoord i = 0; i < DMAXX; i++) {
			const WorldTileCoord ii = 16 + i * 2;
			if (dungeon[i][j] != 1 && dungeon[i][j] != 2)
				continue;
			if (!FlipCoin(freq))
				continue;
			if (IsNearThemeRoom({ i, j }))
				continue;
			if (dungeon[i][j] == 1 && dungeon[i + 1][j] == 6) {
				switch (GenerateRnd(3)) {
				case 0:
					AddObject(OBJ_TORTURE1, { ii + 1, jj });
					break;
				case 1:
					AddObject(OBJ_TORTURE2, { ii + 1, jj });
					break;
				case 2:
					AddObject(OBJ_TORTURE5, { ii + 1, jj });
					break;
				}
				continue;
			}
			if (dungeon[i][j] == 2 && dungeon[i][j + 1] == 6) {
				AddObject(PickRandomlyAmong({ OBJ_TORTURE3, OBJ_TORTURE4 }), { ii, jj });
			}
		}
	}
}

void AddL4Goodies()
{
	AddHookedBodies(6);
	InitRndLocObj(2, 6, OBJ_TNUDEM1);
	InitRndLocObj(2, 6, OBJ_TNUDEM2);
	InitRndLocObj(2, 6, OBJ_TNUDEM3);
	InitRndLocObj(2, 6, OBJ_TNUDEM4);
	InitRndLocObj(2, 6, OBJ_TNUDEW1);
	InitRndLocObj(2, 6, OBJ_TNUDEW2);
	InitRndLocObj(2, 6, OBJ_TNUDEW3);
	InitRndLocObj(2, 6, OBJ_DECAP);
	InitRndLocObj(1, 3, OBJ_CAULDRON);
}

void AddLazStand()
{
	int cnt = 0;
	int xp;
	int yp;
	while (true) {
		xp = GenerateRnd(80) + 16;
		yp = GenerateRnd(80) + 16;

		if (!IsAreaOk(Rectangle { { xp - 2, yp - 3 }, { 6, 7 } })) {
			cnt++;
			if (cnt > 10000) {
				InitRndLocObj(1, 1, OBJ_LAZSTAND);
				return;
			}
		} else {
			break;
		}
	}
	AddObject(OBJ_LAZSTAND, { xp, yp });
	AddObject(OBJ_TNUDEM2, { xp, yp + 2 });
	AddObject(OBJ_STORYCANDLE, { xp + 1, yp + 2 });
	AddObject(OBJ_TNUDEM3, { xp + 2, yp + 2 });
	AddObject(OBJ_TNUDEW1, { xp, yp - 2 });
	AddObject(OBJ_STORYCANDLE, { xp + 1, yp - 2 });
	AddObject(OBJ_TNUDEW2, { xp + 2, yp - 2 });
	AddObject(OBJ_STORYCANDLE, { xp - 1, yp - 1 });
	AddObject(OBJ_TNUDEW3, { xp - 1, yp });
	AddObject(OBJ_STORYCANDLE, { xp - 1, yp + 1 });
}

void DeleteObject(int oi, int i)
{
	const Object &object = Objects[oi];
	const Point position = object.position;
	dObject[position.x][position.y] = 0;
	AvailableObjects[-ActiveObjectCount + MAXOBJECTS] = oi;
	ActiveObjectCount--;
	if (ObjectUnderCursor == &object) // Unselect object if this was highlighted by player
		ObjectUnderCursor = nullptr;
	if (ActiveObjectCount > 0 && i != ActiveObjectCount)
		ActiveObjects[i] = ActiveObjects[ActiveObjectCount];
}

void AddChest(Object &chest)
{
	if (FlipCoin())
		chest._oAnimFrame += 3;
	chest._oRndSeed = AdvanceRndSeed();
	switch (chest._otype) {
	case OBJ_CHEST1:
	case OBJ_TCHEST1:
		if (setlevel) {
			chest._oVar1 = 1;
			break;
		}
		chest._oVar1 = GenerateRnd(2);
		break;
	case OBJ_TCHEST2:
	case OBJ_CHEST2:
		if (setlevel) {
			chest._oVar1 = 2;
			break;
		}
		chest._oVar1 = GenerateRnd(3);
		break;
	case OBJ_TCHEST3:
	case OBJ_CHEST3:
		if (setlevel) {
			chest._oVar1 = 3;
			break;
		}
		chest._oVar1 = GenerateRnd(4);
		break;
	default:
		break;
	}
	chest._oVar2 = GenerateRnd(8);
}

void ObjSetMicro(Point position, int pn)
{
	dPiece[position.x][position.y] = pn;
}

void DoorSet(Point position, bool isLeftDoor)
{
	const int pn = dPiece[position.x][position.y];
	switch (pn) {
	case 42:
		ObjSetMicro(position, 391);
		break;
	case 44:
		ObjSetMicro(position, 393);
		break;
	case 49:
		ObjSetMicro(position, isLeftDoor ? 410 : 411);
		break;
	case 53:
		ObjSetMicro(position, 396);
		break;
	case 54:
		ObjSetMicro(position, 397);
		break;
	case 60:
		ObjSetMicro(position, 398);
		break;
	case 66:
		ObjSetMicro(position, 399);
		break;
	case 67:
		ObjSetMicro(position, 400);
		break;
	case 68:
		ObjSetMicro(position, 402);
		break;
	case 69:
		ObjSetMicro(position, 403);
		break;
	case 71:
		ObjSetMicro(position, 405);
		break;
	case 211:
		ObjSetMicro(position, 406);
		break;
	case 353:
		ObjSetMicro(position, 408);
		break;
	case 354:
		ObjSetMicro(position, 409);
		break;
	case 410:
	case 411:
		ObjSetMicro(position, 395);
		break;
	}
}

void CryptDoorSet(Point position, bool isLeftDoor)
{
	const int pn = dPiece[position.x][position.y];
	switch (pn) {
	case 74:
		ObjSetMicro(position, 203);
		break;
	case 78:
		ObjSetMicro(position, 207);
		break;
	case 85:
		ObjSetMicro(position, isLeftDoor ? 231 : 233);
		break;
	case 90:
		ObjSetMicro(position, 214);
		break;
	case 92:
		ObjSetMicro(position, 217);
		break;
	case 98:
		ObjSetMicro(position, 219);
		break;
	case 110:
		ObjSetMicro(position, 221);
		break;
	case 112:
		ObjSetMicro(position, 223);
		break;
	case 114:
		ObjSetMicro(position, 225);
		break;
	case 116:
		ObjSetMicro(position, 227);
		break;
	case 118:
		ObjSetMicro(position, 229);
		break;
	case 231:
	case 233:
		ObjSetMicro(position, 211);
		break;
	}
}

void SetDoorStateOpen(Object &door)
{
	door._oVar4 = DOOR_OPEN;
	door._oPreFlag = true;
	door._oMissFlag = true;
	door.selectionRegion = SelectionRegion::Middle;

	switch (door._otype) {
	case OBJ_L1LDOOR:
		// 214: blood splater
		// 407: blood pool
		// 392: open door (no frame)
		ObjSetMicro(door.position, door._oVar1 == 214 ? 407 : 392);
		dSpecial[door.position.x][door.position.y] = 7;
		DoorSet(door.position + Direction::NorthEast, true);
		break;
	case OBJ_L1RDOOR:
		ObjSetMicro(door.position, 394);
		dSpecial[door.position.x][door.position.y] = 8;
		DoorSet(door.position + Direction::NorthWest, false);
		break;
	case OBJ_L2LDOOR:
		ObjSetMicro(door.position, 12);
		dSpecial[door.position.x][door.position.y] = 5;
		break;
	case OBJ_L2RDOOR:
		ObjSetMicro(door.position, 16);
		dSpecial[door.position.x][door.position.y] = 6;
		break;
	case OBJ_L3LDOOR:
		ObjSetMicro(door.position, 537);
		break;
	case OBJ_L3RDOOR:
		ObjSetMicro(door.position, 540);
		break;
	case OBJ_L5LDOOR:
		ObjSetMicro(door.position, 205);
		CryptDoorSet(door.position + Direction::NorthEast, true);
		break;
	case OBJ_L5RDOOR:
		ObjSetMicro(door.position, 208);
		CryptDoorSet(door.position + Direction::NorthWest, false);
		break;
	default:
		break;
	}
}

void SetDoorStateClosed(Object &door)
{
	door._oVar4 = DOOR_CLOSED;
	door._oPreFlag = false;
	door._oMissFlag = false;
	door.selectionRegion = SelectionRegion::Bottom | SelectionRegion::Middle;

	switch (door._otype) {
	case OBJ_L1LDOOR: {
		// Clear overlapping arches
		dSpecial[door.position.x][door.position.y] = 0;
		ObjSetMicro(door.position, door._oVar1 - 1);

		// Restore the normal tile where the open door used to be
		auto openPosition = door.position + Direction::NorthEast;
		if (door._oVar2 == 50 && dPiece[openPosition.x][openPosition.y] == 395)
			ObjSetMicro(openPosition, 411);
		else
			ObjSetMicro(openPosition, door._oVar2 - 1);
		break;
	} break;
	case OBJ_L1RDOOR: {
		// Clear overlapping arches
		dSpecial[door.position.x][door.position.y] = 0;
		ObjSetMicro(door.position, door._oVar1 - 1);

		// Restore the normal tile where the open door used to be
		auto openPosition = door.position + Direction::NorthWest;
		if (door._oVar2 == 50 && dPiece[openPosition.x][openPosition.y] == 395)
			ObjSetMicro(openPosition, 410);
		else
			ObjSetMicro(openPosition, door._oVar2 - 1);
		break;
	} break;
	case OBJ_L2LDOOR:
		// Clear overlapping arches
		dSpecial[door.position.x][door.position.y] = 0;
		ObjSetMicro(door.position, 537);
		break;
	case OBJ_L2RDOOR:
		// Clear overlapping arches
		dSpecial[door.position.x][door.position.y] = 0;
		ObjSetMicro(door.position, 539);
		break;
	case OBJ_L3LDOOR:
		ObjSetMicro(door.position, 530);
		break;
	case OBJ_L3RDOOR:
		ObjSetMicro(door.position, 533);
		break;
	case OBJ_L5LDOOR: {
		ObjSetMicro(door.position, door._oVar1 - 1);

		// Restore the normal tile where the open door used to be
		auto openPosition = door.position + Direction::NorthEast;
		if (door._oVar2 == 86 && dPiece[openPosition.x][openPosition.y] == 209)
			ObjSetMicro(openPosition, 233);
		else
			ObjSetMicro(openPosition, door._oVar2 - 1);
	} break;
	case OBJ_L5RDOOR: {
		ObjSetMicro(door.position, door._oVar1 - 1);

		// Restore the normal tile where the open door used to be
		auto openPosition = door.position + Direction::NorthWest;
		if (door._oVar2 == 86 && dPiece[openPosition.x][openPosition.y] == 209)
			ObjSetMicro(openPosition, 231);
		else
			ObjSetMicro(openPosition, door._oVar2 - 1);
	} break;
	default:
		break;
	}
}

void AddDoor(Object &door)
{
	door._oDoorFlag = true;

	switch (door._otype) {
	case OBJ_L1LDOOR:
	case OBJ_L5LDOOR:
		door._oVar1 = dPiece[door.position.x][door.position.y] + 1;
		door._oVar2 = dPiece[door.position.x][door.position.y - 1] + 1;
		break;
	case OBJ_L1RDOOR:
	case OBJ_L5RDOOR:
		door._oVar1 = dPiece[door.position.x][door.position.y] + 1;
		door._oVar2 = dPiece[door.position.x - 1][door.position.y] + 1;
		break;
	default:
		break;
	}

	SetDoorStateClosed(door);
}

void AddSarcophagus(Object &sarcophagus)
{
	dObject[sarcophagus.position.x][sarcophagus.position.y - 1] = -(static_cast<int8_t>(sarcophagus.GetId()) + 1);
	sarcophagus._oVar1 = GenerateRnd(10);
	sarcophagus._oRndSeed = AdvanceRndSeed();
	if (sarcophagus._oVar1 >= 8) {
		Monster *monster = PreSpawnSkeleton();
		if (monster != nullptr) {
			sarcophagus._oVar2 = static_cast<int>(monster->getId());
		} else {
			sarcophagus._oVar2 = -1;
		}
	}
}

void AddFlameTrap(Object &flameTrap)
{
	flameTrap._oVar1 = trapid;
	flameTrap._oVar2 = 0;
	flameTrap._oVar3 = trapdir;
	flameTrap._oVar4 = 0;
}

void AddFlameLever(Object &flameLever)
{
	flameLever._oVar1 = trapid;
	flameLever._oVar2 = static_cast<int8_t>(MissileID::InfernoControl);
}

void AddTrap(Object &trap)
{
	int effectiveLevel = currlevel;
	if (leveltype == DTYPE_NEST)
		effectiveLevel -= 4;
	else if (leveltype == DTYPE_CRYPT)
		effectiveLevel -= 8;

	const int missileType = GenerateRnd(effectiveLevel / 3 + 1);
	if (missileType == 0)
		trap._oVar3 = static_cast<int8_t>(MissileID::Arrow);
	if (missileType == 1)
		trap._oVar3 = static_cast<int8_t>(MissileID::Firebolt);
	if (missileType == 2)
		trap._oVar3 = static_cast<int8_t>(MissileID::LightningControl);
	trap._oVar4 = 0;
}

void AddObjectLight(Object &object)
{
	int radius;
	switch (object._otype) {
	case OBJ_STORYCANDLE:
	case OBJ_L5CANDLE:
		radius = 3;
		break;
	case OBJ_L1LIGHT:
	case OBJ_SKFIRE:
	case OBJ_CANDLE1:
	case OBJ_CANDLE2:
	case OBJ_BOOKCANDLE:
	case OBJ_BCROSS:
	case OBJ_TBCROSS:
		radius = 5;
		break;
	case OBJ_TORCHL:
	case OBJ_TORCHR:
	case OBJ_TORCHL2:
	case OBJ_TORCHR2:
		radius = 8;
		break;
	default:
		return;
	}

	DoLighting(object.position, radius, {});
	if (LoadingMapObjects) {
		DoUnLight(object.position, radius);
		UpdateLighting = true;
	}
	object._oVar1 = -1;
}

void AddBarrel(Object &barrel)
{
	barrel._oVar1 = 0;
	barrel._oRndSeed = AdvanceRndSeed();
	barrel._oVar2 = barrel.isExplosive() ? 0 : GenerateRnd(10);
	barrel._oVar3 = GenerateRnd(3);

	if (barrel._oVar2 >= 8) {
		Monster *skeleton = PreSpawnSkeleton();
		if (skeleton != nullptr) {
			barrel._oVar4 = static_cast<int>(skeleton->getId());
		} else {
			barrel._oVar4 = -1;
		}
	}
}

void AddShrine(Object &shrine)
{
	shrine._oRndSeed = AdvanceRndSeed();
	shrine._oPreFlag = true;

	const int shrineCount = gbIsHellfire ? NumberOfShrineTypes : 26;
	bool slist[NumberOfShrineTypes] = {};

	for (int i = 0; i < shrineCount; i++) {
		bool isShrineAvailable = true;

		if (gbIsMultiplayer) {
			isShrineAvailable = (shrineavail[i] != ShrineTypeSingle);
		} else {
			isShrineAvailable = (shrineavail[i] != ShrineTypeMulti);
		}

		const bool isEnchantedShrine = (i == ShrineEnchanted);
		const bool isCorrectLevelType = IsAnyOf(leveltype, DTYPE_CATHEDRAL, DTYPE_CATACOMBS);

		slist[i] = isShrineAvailable && (!isEnchantedShrine || isCorrectLevelType);
	}

	int selectedIndex;
	do {
		selectedIndex = GenerateRnd(shrineCount);
	} while (!slist[selectedIndex]);

	shrine._oVar1 = selectedIndex;

	if (!FlipCoin()) {
		shrine._oAnimFrame = 12;
		shrine._oAnimLen = 22;
	}
}

void AddBookcase(Object &bookcase)
{
	bookcase._oRndSeed = AdvanceRndSeed();
	bookcase._oPreFlag = true;
}

void AddLargeFountain(Object &fountain)
{
	const int ox = fountain.position.x;
	const int oy = fountain.position.y;
	const uint8_t id = -(static_cast<int8_t>(fountain.GetId()) + 1);
	dObject[ox][oy - 1] = id;
	dObject[ox - 1][oy] = id;
	dObject[ox - 1][oy - 1] = id;
	fountain._oRndSeed = AdvanceRndSeed();
}

void AddArmorStand(Object &armorStand)
{
	if (!armorFlag) {
		armorStand._oAnimFlag = true;
		armorStand.selectionRegion = SelectionRegion::None;
	}

	armorStand._oRndSeed = AdvanceRndSeed();
}

void AddDecapitatedBody(Object &decapitatedBody)
{
	decapitatedBody._oRndSeed = AdvanceRndSeed();
	decapitatedBody._oAnimFrame = GenerateRnd(8) + 1;
	decapitatedBody._oPreFlag = true;
}

void AddBookOfVileness(Object &bookOfVileness)
{
	if (setlevel && setlvlnum == SL_VILEBETRAYER) {
		bookOfVileness._oAnimFrame = 4;
	}
}

void AddMagicCircle(Object &magicCircle)
{
	magicCircle._oRndSeed = AdvanceRndSeed();
	magicCircle._oPreFlag = true;
	magicCircle._oVar6 = 0;
	magicCircle._oVar5 = 1;
}

void AddPedestalOfBlood(Object &pedestalOfBlood)
{
	pedestalOfBlood._oVar1 = SetPiece.position.x;
	pedestalOfBlood._oVar2 = SetPiece.position.y;
	pedestalOfBlood._oVar3 = SetPiece.position.x + SetPiece.size.width;
	pedestalOfBlood._oVar4 = SetPiece.position.y + SetPiece.size.height;
	pedestalOfBlood._oVar6 = 0;
}

void AddStoryBook(Object &storyBook)
{
	storyBook._oVar1 = (DungeonSeeds[16] >> 16) % 3;
	if (currlevel == 4)
		storyBook._oVar2 = StoryText[storyBook._oVar1][0];
	else if (currlevel == 8)
		storyBook._oVar2 = StoryText[storyBook._oVar1][1];
	else if (currlevel == 12)
		storyBook._oVar2 = StoryText[storyBook._oVar1][2];
	storyBook._oVar3 = (currlevel / 4) + 3 * storyBook._oVar1 - 1;
	storyBook._oAnimFrame = 5 - 2 * storyBook._oVar1;
	storyBook._oVar4 = storyBook._oAnimFrame + 1;
}

void AddWeaponRack(Object &weaponRack)
{
	if (!weaponFlag) {
		weaponRack._oAnimFlag = true;
		weaponRack.selectionRegion = SelectionRegion::None;
	}
	weaponRack._oRndSeed = AdvanceRndSeed();
}

void AddTorturedBody(Object &torturedBody)
{
	torturedBody._oRndSeed = AdvanceRndSeed();
	torturedBody._oAnimFrame = GenerateRnd(4) + 1;
	torturedBody._oPreFlag = true;
}

Point GetRndObjLoc(int randarea)
{
	if (randarea == 0)
		return { 0, 0 };

	int tries = 0;
	int x;
	int y;
	while (true) {
		tries++;
		if (tries > 1000 && randarea > 1)
			randarea--;
		x = GenerateRnd(MAXDUNX);
		y = GenerateRnd(MAXDUNY);
		if (IsAreaOk(Rectangle { { x, y }, { randarea, randarea } }))
			break;
	}
	return { x, y };
}

void AddMushPatch()
{
	if (ActiveObjectCount < MAXOBJECTS) {
		const int i = AvailableObjects[0];
		const Point loc = GetRndObjLoc(5);
		dObject[loc.x + 1][loc.y + 1] = -(i + 1);
		dObject[loc.x + 2][loc.y + 1] = -(i + 1);
		dObject[loc.x + 1][loc.y + 2] = -(i + 1);
		AddObject(OBJ_MUSHPATCH, { loc.x + 2, loc.y + 2 });
	}
}

bool IsLightVisible(Object &light, int lightRadius)
{
#ifdef _DEBUG
	if (DisableLighting)
		return false;
#endif

	for (const Player &player : Players) {
		if (!player.plractive)
			continue;

		if (!player.isOnActiveLevel())
			continue;

		if (player.position.tile.WalkingDistance(light.position) < lightRadius + 10) {
			return true;
		}
	}

	return false;
}

void UpdateObjectLight(Object &light, int lightRadius)
{
	if (light._oVar1 == -1) {
		return;
	}

	if (IsLightVisible(light, lightRadius)) {
		if (light._oVar1 == 0)
			light._olid = AddLight(light.position, lightRadius);
		light._oVar1 = 1;
	} else {
		if (light._oVar1 == 1)
			AddUnLight(light._olid);
		light._oVar1 = 0;
	}
}

void UpdateCircle(Object &circle)
{
	Player *playerOnCircle = PlayerAtPosition(circle.position);

	if (!playerOnCircle) {
		if (circle._otype == OBJ_MCIRCLE1)
			circle._oAnimFrame = 1;
		if (circle._otype == OBJ_MCIRCLE2)
			circle._oAnimFrame = 3;
		circle._oVar6 = 0;
		return;
	}

	if (circle._otype == OBJ_MCIRCLE1)
		circle._oAnimFrame = 2;
	if (circle._otype == OBJ_MCIRCLE2)
		circle._oAnimFrame = 4;
	if (circle.position == Point { 45, 47 }) {
		circle._oVar6 = 2;
	} else if (circle.position == Point { 26, 46 }) {
		circle._oVar6 = 1;
	} else {
		circle._oVar6 = 0;
	}
	if (circle.position == Point { 35, 36 } && circle._oVar5 == 3) {
		circle._oVar6 = 4;
		if (Quests[Q_BETRAYER]._qvar1 <= 4) {
			LoadingMapObjects = true;
			ObjChangeMap(circle._oVar1, circle._oVar2, circle._oVar3, circle._oVar4);
			LoadingMapObjects = false;
			Quests[Q_BETRAYER]._qvar1 = 4;
			NetSendCmdQuest(true, Quests[Q_BETRAYER]);
		}
		AddMissile(playerOnCircle->position.tile, { 35, 46 }, Direction::South, MissileID::Phasing, TARGET_BOTH, *playerOnCircle, 0, 0);
		if (playerOnCircle == MyPlayer) {
			LastPlayerAction = PlayerActionType::None;
			sgbMouseDown = CLICK_NONE;
		}
		ClrPlrPath(*playerOnCircle);
		StartStand(*playerOnCircle, Direction::South);
	}
}

void ObjectStopAnim(Object &object)
{
	if (object._oAnimFrame == object._oAnimLen) {
		object._oAnimCnt = 0;
		object._oAnimDelay = 1000;
	}
}

/**
 * @brief Checks if an open door can be closed
 *
 * In order to be able to close a door the space where the closed door would be must be free of bodies, monsters, players, and items
 *
 * @param doorPosition Map tile where the door is in its closed position
 * @return true if the door is free to be closed, false if anything is blocking it
 */
inline bool IsDoorClear(const Object &door)
{
	return dCorpse[door.position.x][door.position.y] == 0
	    && dMonster[door.position.x][door.position.y] == 0
	    && dItem[door.position.x][door.position.y] == 0
	    && dPlayer[door.position.x][door.position.y] == 0;
}

void UpdateDoor(Object &door)
{
	if (door._oVar4 == DOOR_CLOSED) {
		return;
	}

	door._oVar4 = IsDoorClear(door) ? DOOR_OPEN : DOOR_BLOCKED;
}

void UpdateSarcophagus(Object &sarcophagus)
{
	if (sarcophagus._oAnimFrame == sarcophagus._oAnimLen)
		sarcophagus._oAnimFlag = false;
}

void ActivateTrapLine(int ttype, int tid)
{
	for (int i = 0; i < ActiveObjectCount; i++) {
		Object &trap = Objects[ActiveObjects[i]];
		if (trap._otype == ttype && trap._oVar1 == tid) {
			trap._oVar4 = 1;
			trap._oAnimFlag = true;
			trap._oAnimDelay = 1;
			trap._olid = AddLight(trap.position, 1);
		}
	}
}

void UpdateFlameTrap(Object &trap)
{
	if (trap._oVar2 != 0) {
		if (trap._oVar4 != 0) {
			trap._oAnimFrame--;
			if (trap._oAnimFrame == 1) {
				trap._oVar4 = 0;
				AddUnLight(trap._olid);
			} else if (trap._oAnimFrame <= 4) {
				ChangeLightRadius(trap._olid, trap._oAnimFrame);
			}
		}
	} else if (trap._oVar4 == 0) {
		if (trap._oVar3 == 2) {
			int x = trap.position.x - 2;
			const int y = trap.position.y;
			for (int j = 0; j < 5; j++) {
				if (dPlayer[x][y] != 0 || dMonster[x][y] != 0)
					trap._oVar4 = 1;
				x++;
			}
		} else {
			const int x = trap.position.x;
			int y = trap.position.y - 2;
			for (int k = 0; k < 5; k++) {
				if (dPlayer[x][y] != 0 || dMonster[x][y] != 0)
					trap._oVar4 = 1;
				y++;
			}
		}
		if (trap._oVar4 != 0)
			ActivateTrapLine(trap._otype, trap._oVar1);
	} else {
		const int damage[6] = { 6, 8, 10, 12, 10, 12 };

		const int mindam = damage[leveltype - 1];
		const int maxdam = mindam * 2;

		constexpr MissileID TrapMissile = MissileID::FireWallControl;
		Monster *monster = FindMonsterAtPosition(trap.position, true);
		if (monster != nullptr)
			MonsterTrapHit(*monster, mindam / 2, maxdam / 2, 0, TrapMissile, GetMissileData(TrapMissile).damageType(), false);
		Player *player = PlayerAtPosition(trap.position, true);
		if (player != nullptr) {
			bool unused;
			PlayerMHit(*player, nullptr, 0, mindam, maxdam, TrapMissile, GetMissileData(TrapMissile).damageType(), false, DeathReason::MonsterOrTrap, &unused);
		}

		if (trap._oAnimFrame == trap._oAnimLen)
			trap._oAnimFrame = 11;
		if (trap._oAnimFrame <= 5)
			ChangeLightRadius(trap._olid, trap._oAnimFrame);
	}
}

void UpdateBurningCrossDamage(Object &cross)
{
	int damage[6] = { 6, 8, 10, 12, 10, 12 };

	Player &myPlayer = *MyPlayer;

	if (myPlayer._pmode == PM_DEATH)
		return;

	const int8_t fireResist = myPlayer._pFireResist;
	if (fireResist > 0)
		damage[leveltype - 1] -= fireResist * damage[leveltype - 1] / 100;

	if (myPlayer.position.tile != cross.position + Displacement { 0, -1 })
		return;

	ApplyPlrDamage(DamageType::Fire, myPlayer, 0, 0, damage[leveltype - 1]);
	if (myPlayer._pHitPoints >> 6 > 0) {
		myPlayer.Say(HeroSpeech::Argh);
	}
}

void ObjSetMini(Point position, int v)
{
	const MegaTile mega = pMegaTiles[v - 1];

	const Point megaOrigin = position.megaToWorld();

	ObjSetMicro(megaOrigin, Swap16LE(mega.micro1));
	ObjSetMicro(megaOrigin + Direction::SouthEast, Swap16LE(mega.micro2));
	ObjSetMicro(megaOrigin + Direction::SouthWest, Swap16LE(mega.micro3));
	ObjSetMicro(megaOrigin + Direction::South, Swap16LE(mega.micro4));
}

void ObjL1Special(int x1, int y1, int x2, int y2)
{
	for (int i = y1; i <= y2; ++i) {
		for (int j = x1; j <= x2; ++j) {
			dSpecial[j][i] = 0;
			if (dPiece[j][i] == 11)
				dSpecial[j][i] = 1;
			if (dPiece[j][i] == 10)
				dSpecial[j][i] = 2;
			if (dPiece[j][i] == 70)
				dSpecial[j][i] = 1;
			if (dPiece[j][i] == 252)
				dSpecial[j][i] = 3;
			if (dPiece[j][i] == 266)
				dSpecial[j][i] = 6;
			if (dPiece[j][i] == 258)
				dSpecial[j][i] = 5;
			if (dPiece[j][i] == 248)
				dSpecial[j][i] = 2;
			if (dPiece[j][i] == 324)
				dSpecial[j][i] = 2;
			if (dPiece[j][i] == 320)
				dSpecial[j][i] = 1;
			if (dPiece[j][i] == 254)
				dSpecial[j][i] = 4;
			if (dPiece[j][i] == 210)
				dSpecial[j][i] = 1;
			if (dPiece[j][i] == 343)
				dSpecial[j][i] = 2;
			if (dPiece[j][i] == 340)
				dSpecial[j][i] = 1;
			if (dPiece[j][i] == 330)
				dSpecial[j][i] = 2;
			if (dPiece[j][i] == 417)
				dSpecial[j][i] = 1;
			if (dPiece[j][i] == 420)
				dSpecial[j][i] = 2;
		}
	}
}

void ObjL2Special(int x1, int y1, int x2, int y2)
{
	for (int j = y1; j <= y2; j++) {
		for (int i = x1; i <= x2; i++) {
			dSpecial[i][j] = 0;
			if (dPiece[i][j] == 540)
				dSpecial[i][j] = 5;
			if (dPiece[i][j] == 177)
				dSpecial[i][j] = 5;
			if (dPiece[i][j] == 550)
				dSpecial[i][j] = 5;
			if (dPiece[i][j] == 541)
				dSpecial[i][j] = 6;
			if (dPiece[i][j] == 552)
				dSpecial[i][j] = 6;
		}
	}
	for (int j = y1; j <= y2; j++) {
		for (int i = x1; i <= x2; i++) {
			if (dPiece[i][j] == 131) {
				dSpecial[i][j + 1] = 2;
				dSpecial[i][j + 2] = 1;
			}
			if (dPiece[i][j] == 134 || dPiece[i][j] == 138) {
				dSpecial[i + 1][j] = 3;
				dSpecial[i + 2][j] = 4;
			}
		}
	}
}

void OpenDoor(Object &door)
{
	door._oAnimFrame += 2;
	SetDoorStateOpen(door);
}

void CloseDoor(Object &door)
{
	door._oAnimFrame -= 2;
	SetDoorStateClosed(door);
}

void OperateDoor(Object &door, bool sendflag)
{
	const bool isCrypt = IsAnyOf(door._otype, OBJ_L5LDOOR, OBJ_L5RDOOR);
	const bool openDoor = door._oVar4 == DOOR_CLOSED;

	if (!openDoor && !IsDoorClear(door)) {
		PlaySfxLoc(isCrypt ? SfxID::CryptDoorClose : SfxID::DoorClose, door.position);
		door._oVar4 = DOOR_BLOCKED;
		return;
	}

	if (openDoor) {
		PlaySfxLoc(isCrypt ? SfxID::CryptDoorOpen : SfxID::DoorOpen, door.position);
		OpenDoor(door);
	} else {
		PlaySfxLoc(isCrypt ? SfxID::CryptDoorClose : SfxID::DoorClose, door.position);
		CloseDoor(door);
	}

	RedoPlayerVision();

	if (sendflag)
		NetSendCmdLoc(MyPlayerId, true, openDoor ? CMD_OPENDOOR : CMD_CLOSEDOOR, door.position);
}

bool AreAllLeversActivated(int leverId)
{
	for (int j = 0; j < ActiveObjectCount; j++) {
		const Object &lever = Objects[ActiveObjects[j]];
		if (lever._otype == OBJ_SWITCHSKL
		    && lever._oVar8 == leverId
		    && lever.canInteractWith()) {
			return false;
		}
	}
	return true;
}

void UpdateLeverState(Object &object)
{
	if (!object.canInteractWith()) {
		return;
	}

	object.selectionRegion = SelectionRegion::None;
	object._oAnimFrame++;

	if (currlevel == 16 && !AreAllLeversActivated(object._oVar8))
		return;

	if (currlevel == 24) {
		SyncNakrulRoom();
		IsUberLeverActivated = true;
		return;
	}

	if (setlevel && setlvlnum == SL_VILEBETRAYER)
		ObjectAtPosition({ 35, 36 })._oVar5++;

	ObjChangeMap(object._oVar1, object._oVar2, object._oVar3, object._oVar4);
}

void OperateLever(Object &object, bool sendmsg)
{
	if (!object.canInteractWith()) {
		return;
	}

	PlaySfxLoc(SfxID::OperateLever, object.position);

	UpdateLeverState(object);

	if (currlevel == 24) {
		PlaySfxLoc(SfxID::CryptDoorOpen, { UberRow, UberCol });
		Quests[Q_NAKRUL]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_NAKRUL]);
	}

	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, object.position);
}

void OperateBook(Player &player, Object &book, bool sendmsg)
{
	if (!book.canInteractWith()) {
		return;
	}

	if (setlevel && setlvlnum == SL_VILEBETRAYER) {
		Point target {};
		if (book.position == Point { 26, 45 }) {
			target = { 27, 29 };
		} else if (book.position == Point { 45, 46 }) {
			target = { 43, 29 };
		} else {
			return;
		}

		Object &circle = ObjectAtPosition(book.position + Direction::SouthWest);
		assert(circle._otype == OBJ_MCIRCLE2);

		// Only verify that the player stands on the circle when it's the local player (sendmsg), because for remote players the position could be desynced
		if (sendmsg && circle.position != player.position.tile) {
			return;
		}

		circle._oVar6 = 4;
		ObjectAtPosition({ 35, 36 })._oVar5++;
		AddMissile(player.position.tile, target, Direction::South, MissileID::Phasing, TARGET_BOTH, player, 0, 0);
	}

	book.selectionRegion = SelectionRegion::None;
	book._oAnimFrame++;

	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, book.position);

	if (!setlevel) {
		return;
	}

	if (setlvlnum == SL_BONECHAMB) {
		if (sendmsg) {
			const uint8_t newSpellLevel = player._pSplLvl[static_cast<int8_t>(SpellID::Guardian)] + 1;
			if (newSpellLevel <= MaxSpellLevel) {
				player._pSplLvl[static_cast<int8_t>(SpellID::Guardian)] = newSpellLevel;
				NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, static_cast<uint16_t>(SpellID::Guardian), newSpellLevel);
			}

			if (&player == MyPlayer) {
				for (Item &item : InventoryPlayerItemsRange { player }) {
					item.updateRequiredStatsCacheForPlayer(player);
				}
				if (IsStashOpen) {
					Stash.RefreshItemStatFlags();
				}
			}

			Quests[Q_SCHAMB]._qactive = QUEST_DONE;
			NetSendCmdQuest(true, Quests[Q_SCHAMB]);
		}
		PlaySfxLoc(SfxID::QuestDone, book.position);
		InitDiabloMsg(EMSG_BONECHAMB);
		AddMissile(
		    player.position.tile,
		    book.position + Displacement { -2, -4 },
		    player._pdir,
		    MissileID::Guardian,
		    TARGET_MONSTERS,
		    player,
		    0,
		    0);
	}
	if (setlvlnum == SL_VILEBETRAYER) {
		ObjChangeMap(
		    book._oVar1,
		    book._oVar2,
		    book._oVar3,
		    book._oVar4);
		for (int j = 0; j < ActiveObjectCount; j++)
			SyncObjectAnim(Objects[ActiveObjects[j]]);
	}
}

void OperateBookLever(Object &questBook, bool sendmsg)
{
	if (ActiveItemCount >= MAXITEMS) {
		return;
	}
	if (questBook.canInteractWith() && !qtextflag) {
		if (questBook._otype == OBJ_BLINDBOOK && Quests[Q_BLIND]._qvar1 == 0) {
			Quests[Q_BLIND]._qactive = QUEST_ACTIVE;
			Quests[Q_BLIND]._qlog = true;
			Quests[Q_BLIND]._qvar1 = 1;
			NetSendCmdQuest(true, Quests[Q_BLIND]);
		}
		if (questBook._otype == OBJ_BLOODBOOK && Quests[Q_BLOOD]._qvar1 == 0) {
			Quests[Q_BLOOD]._qactive = QUEST_ACTIVE;
			Quests[Q_BLOOD]._qlog = true;
			Quests[Q_BLOOD]._qvar1 = 1;
			NetSendCmdQuest(true, Quests[Q_BLOOD]);
			if (sendmsg)
				SpawnQuestItem(IDI_BLDSTONE, SetPiece.position.megaToWorld() + Displacement { 9, 17 }, 0, SelectionRegion::Bottom, true);
		}
		if (questBook._otype == OBJ_STEELTOME && Quests[Q_WARLORD]._qvar1 == QS_WARLORD_INIT) {
			Quests[Q_WARLORD]._qactive = QUEST_ACTIVE;
			Quests[Q_WARLORD]._qlog = true;
			Quests[Q_WARLORD]._qvar1 = QS_WARLORD_STEELTOME_READ;
			NetSendCmdQuest(true, Quests[Q_WARLORD]);
		}
		if (questBook._oAnimFrame != questBook._oVar6) {
			if (questBook._otype != OBJ_BLOODBOOK)
				ObjChangeMap(questBook._oVar1, questBook._oVar2, questBook._oVar3, questBook._oVar4);
			if (questBook._otype == OBJ_BLINDBOOK) {
				if (sendmsg)
					SpawnUnique(UITEM_OPTAMULET, SetPiece.position.megaToWorld() + Displacement { 5, 5 }, std::nullopt, true, true);
				auto tren = TransVal;
				TransVal = 9;
				DRLG_MRectTrans(WorldTilePosition(questBook._oVar1, questBook._oVar2), WorldTilePosition(questBook._oVar3, questBook._oVar4));
				TransVal = tren;
			}
		}
		questBook._oAnimFrame = questBook._oVar6;
		InitQTextMsg(questBook.bookMessage);
		if (sendmsg)
			NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, questBook.position);
	}
}

void OperateChamberOfBoneBook(Object &questBook, bool sendmsg)
{
	if (!questBook.canInteractWith() || qtextflag) {
		return;
	}

	if (questBook._oAnimFrame != questBook._oVar6) {
		ObjChangeMapResync(questBook._oVar1, questBook._oVar2, questBook._oVar3, questBook._oVar4);
		for (int j = 0; j < ActiveObjectCount; j++) {
			SyncObjectAnim(Objects[ActiveObjects[j]]);
		}
	}
	questBook._oAnimFrame = questBook._oVar6;
	if (Quests[Q_SCHAMB]._qactive == QUEST_INIT) {
		Quests[Q_SCHAMB]._qactive = QUEST_ACTIVE;
		Quests[Q_SCHAMB]._qlog = true;
	}

	_speech_id textdef;
	switch (MyPlayer->_pClass) {
	case HeroClass::Warrior:
		textdef = TEXT_BONER;
		break;
	case HeroClass::Rogue:
		textdef = TEXT_RBONER;
		break;
	case HeroClass::Sorcerer:
		textdef = TEXT_MBONER;
		break;
	case HeroClass::Monk:
		textdef = TEXT_HBONER;
		break;
	case HeroClass::Bard:
		textdef = TEXT_RBONER;
		break;
	case HeroClass::Barbarian:
		textdef = TEXT_BONER;
		break;
	default:
		break;
	}
	if (sendmsg) {
		Quests[Q_SCHAMB]._qmsg = textdef;
		NetSendCmdQuest(true, Quests[Q_SCHAMB]);
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, questBook.position);
		InitQTextMsg(textdef);
	}
}

void OperateChest(const Player &player, Object &chest, bool sendLootMsg)
{
	if (!chest.canInteractWith()) {
		return;
	}

	PlaySfxLoc(SfxID::ChestOpen, chest.position);
	chest.selectionRegion = SelectionRegion::None;
	chest._oAnimFrame += 2;
	SetRndSeed(chest._oRndSeed);
	if (setlevel) {
		for (int j = 0; j < chest._oVar1; j++) {
			CreateRndItem(chest.position, true, sendLootMsg, false);
		}
	} else {
		for (int j = 0; j < chest._oVar1; j++) {
			if (chest._oVar2 != 0)
				CreateRndItem(chest.position, false, sendLootMsg, false);
			else
				CreateRndUseful(chest.position, sendLootMsg);
		}
	}
	if (chest.IsTrappedChest()) {
		const Direction mdir = GetDirection(chest.position, player.position.tile);
		MissileID mtype;
		switch (chest._oVar4) {
		case 0:
			mtype = MissileID::Arrow;
			break;
		case 1:
			mtype = MissileID::FireArrow;
			break;
		case 2:
			mtype = MissileID::Nova;
			break;
		case 3:
			mtype = MissileID::RingOfFire;
			break;
		case 4:
			mtype = MissileID::StealPotions;
			break;
		case 5:
			mtype = MissileID::StealMana;
			break;
		default:
			mtype = MissileID::Arrow;
		}
		AddMissile(chest.position, player.position.tile, mdir, mtype, TARGET_PLAYERS, -1, 0, 0);
		PlaySfxLoc(SfxID::TriggerTrap, chest.position);
		chest._oTrapFlag = false;
	}
	if (&player == MyPlayer)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, chest.position);
}

void OperateMushroomPatch(const Player &player, Object &mushroomPatch)
{
	if (ActiveItemCount >= MAXITEMS) {
		return;
	}

	if (Quests[Q_MUSHROOM]._qactive != QUEST_ACTIVE) {
		if (&player == MyPlayer) {
			player.Say(HeroSpeech::ICantUseThisYet);
		}
		return;
	}

	if (!mushroomPatch.canInteractWith()) {
		return;
	}

	mushroomPatch.selectionRegion = SelectionRegion::None;
	mushroomPatch._oAnimFrame++;

	PlaySfxLoc(SfxID::ChestOpen, mushroomPatch.position);
	const Point pos = GetSuperItemLoc(mushroomPatch.position);

	if (&player == MyPlayer) {
		SpawnQuestItem(IDI_MUSHROOM, pos, 0, SelectionRegion::None, true);
		Quests[Q_MUSHROOM]._qvar1 = QS_MUSHSPAWNED;
		NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, mushroomPatch.position);
	}
}

void OperateInnSignChest(const Player &player, Object &questContainer, bool sendmsg)
{
	if (ActiveItemCount >= MAXITEMS) {
		return;
	}

	if (Quests[Q_LTBANNER]._qvar1 != 2) {
		if (&player == MyPlayer) {
			player.Say(HeroSpeech::ICantOpenThisYet);
		}
		return;
	}

	if (!questContainer.canInteractWith()) {
		return;
	}

	questContainer.selectionRegion = SelectionRegion::None;
	questContainer._oAnimFrame += 2;

	PlaySfxLoc(SfxID::ChestOpen, questContainer.position);

	if (sendmsg) {
		const Point pos = GetSuperItemLoc(questContainer.position);
		SpawnQuestItem(IDI_BANNER, pos, 0, SelectionRegion::None, true);
		NetSendCmdLoc(MyPlayerId, true, CMD_OPERATEOBJ, questContainer.position);
	}
}

void OperateSlainHero(const Player &player, Object &corpse, bool sendmsg)
{
	if (!corpse.canInteractWith()) {
		return;
	}
	corpse.selectionRegion = SelectionRegion::None;

	SetRndSeed(corpse._oRndSeed);

	if (player._pClass == HeroClass::Warrior) {
		CreateMagicArmor(corpse.position, ItemType::HeavyArmor, ICURS_BREAST_PLATE, sendmsg, false);
	} else if (player._pClass == HeroClass::Rogue) {
		CreateMagicWeapon(corpse.position, ItemType::Bow, ICURS_LONG_BATTLE_BOW, sendmsg, false);
	} else if (player._pClass == HeroClass::Sorcerer) {
		CreateSpellBook(corpse.position, SpellID::Lightning, sendmsg, false);
	} else if (player._pClass == HeroClass::Monk) {
		CreateMagicWeapon(corpse.position, ItemType::Staff, ICURS_WAR_STAFF, sendmsg, false);
	} else if (player._pClass == HeroClass::Bard) {
		CreateMagicWeapon(corpse.position, ItemType::Sword, ICURS_BASTARD_SWORD, sendmsg, false);
	} else if (player._pClass == HeroClass::Barbarian) {
		CreateMagicWeapon(corpse.position, ItemType::Axe, ICURS_BATTLE_AXE, sendmsg, false);
	}
	MyPlayer->Say(HeroSpeech::RestInPeaceMyFriend);
	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, corpse.position);
}

void OperateTrapLever(Object &flameLever)
{
	PlaySfxLoc(SfxID::OperateLever, flameLever.position);

	if (flameLever._oAnimFrame == 1) {
		flameLever._oAnimFrame = 2;
		for (int j = 0; j < ActiveObjectCount; j++) {
			Object &target = Objects[ActiveObjects[j]];
			if (target._otype == flameLever._oVar2 && target._oVar1 == flameLever._oVar1) {
				target._oVar2 = 1;
				target._oAnimFlag = false;
			}
		}
		return;
	}

	flameLever._oAnimFrame--;
	for (int j = 0; j < ActiveObjectCount; j++) {
		Object &target = Objects[ActiveObjects[j]];
		if (target._otype == flameLever._oVar2 && target._oVar1 == flameLever._oVar1) {
			target._oVar2 = 0;
			if (target._oVar4 != 0) {
				target._oAnimFlag = true;
			}
		}
	}
}

void OperateSarcophagus(Object &sarcophagus, bool sendMsg, bool sendLootMsg)
{
	if (!sarcophagus.canInteractWith()) {
		return;
	}

	PlaySfxLoc(SfxID::Sarcophagus, sarcophagus.position);
	sarcophagus.selectionRegion = SelectionRegion::None;
	sarcophagus._oAnimFlag = true;
	sarcophagus._oAnimDelay = 3;
	SetRndSeed(sarcophagus._oRndSeed);
	if (sarcophagus._oVar1 <= 2)
		CreateRndItem(sarcophagus.position, false, sendLootMsg, false);
	if (sarcophagus._oVar1 >= 8 && sarcophagus._oVar2 >= 0)
		ActivateSkeleton(Monsters[sarcophagus._oVar2], sarcophagus.position);
	if (sendMsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, sarcophagus.position);
}

void OperatePedestal(Player &player, Object &pedestal, bool sendmsg)
{
	if (ActiveItemCount >= MAXITEMS) {
		return;
	}

	if (pedestal._oVar6 == 3 || (sendmsg && !RemoveInventoryItemById(player, IDI_BLDSTONE))) {
		return;
	}

	if (sendmsg) {
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, pedestal.position);
		if (gbIsMultiplayer) {
			// Store added stones to pedestal in qvar2, because we get only one CMD_OPERATEOBJ from DeltaLoadLevel even if we add multiple stones
			Quests[Q_BLOOD]._qvar2++;
			NetSendCmdQuest(true, Quests[Q_BLOOD]);
		}
	}

	pedestal._oAnimFrame++;
	pedestal._oVar6++;
	if (pedestal._oVar6 == 1) {
		PlaySfxLoc(SfxID::SpellPuddle, pedestal.position);
		ObjChangeMap(SetPiece.position.x, SetPiece.position.y + 3, SetPiece.position.x + 2, SetPiece.position.y + 7);
		if (sendmsg)
			SpawnQuestItem(IDI_BLDSTONE, SetPiece.position.megaToWorld() + Displacement { 3, 10 }, 0, SelectionRegion::Bottom, true);
	}
	if (pedestal._oVar6 == 2) {
		PlaySfxLoc(SfxID::SpellPuddle, pedestal.position);
		ObjChangeMap(SetPiece.position.x + 6, SetPiece.position.y + 3, SetPiece.position.x + SetPiece.size.width, SetPiece.position.y + 7);
		if (sendmsg)
			SpawnQuestItem(IDI_BLDSTONE, SetPiece.position.megaToWorld() + Displacement { 15, 10 }, 0, SelectionRegion::Bottom, true);
	}
	if (pedestal._oVar6 == 3) {
		PlaySfxLoc(SfxID::SpellBloodStar, pedestal.position);
		ObjChangeMap(pedestal._oVar1, pedestal._oVar2, pedestal._oVar3, pedestal._oVar4);
		LoadMapObjects("levels\\l2data\\blood2.dun", SetPiece.position.megaToWorld());
		if (sendmsg)
			SpawnUnique(UITEM_ARMOFVAL, SetPiece.position.megaToWorld() + Displacement { 9, 3 }, std::nullopt, true, true);
		pedestal.selectionRegion = SelectionRegion::None;
	}
}

void OperateShrineMysterious(DiabloGenerator &rng, Player &player)
{
	if (&player != MyPlayer)
		return;

	ModifyPlrStr(player, -1);
	ModifyPlrMag(player, -1);
	ModifyPlrDex(player, -1);
	ModifyPlrVit(player, -1);

	switch (static_cast<CharacterAttribute>(rng.generateRnd(4))) {
	case CharacterAttribute::Strength:
		ModifyPlrStr(player, 6);
		break;
	case CharacterAttribute::Magic:
		ModifyPlrMag(player, 6);
		break;
	case CharacterAttribute::Dexterity:
		ModifyPlrDex(player, 6);
		break;
	case CharacterAttribute::Vitality:
		ModifyPlrVit(player, 6);
		break;
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_MYSTERIOUS);
}

void OperateShrineHidden(DiabloGenerator &rng, Player &player)
{
	if (&player != MyPlayer)
		return;

	int cnt = 0;
	for (const auto &item : player.InvBody) {
		if (!item.isEmpty())
			cnt++;
	}
	if (cnt > 0) {
		for (auto &item : player.InvBody) {
			if (!item.isEmpty()
			    && item._iMaxDur != DUR_INDESTRUCTIBLE
			    && item._iMaxDur != 0) {
				item._iDurability += 10;
				item._iMaxDur += 10;
				if (item._iDurability > item._iMaxDur)
					item._iDurability = item._iMaxDur;
			}
		}
		while (true) {
			cnt = 0;
			for (auto &item : player.InvBody) {
				if (!item.isEmpty() && item._iMaxDur != DUR_INDESTRUCTIBLE && item._iMaxDur != 0) {
					cnt++;
				}
			}
			if (cnt == 0)
				break;
			const int r = rng.generateRnd(NUM_INVLOC);
			if (player.InvBody[r].isEmpty() || player.InvBody[r]._iMaxDur == DUR_INDESTRUCTIBLE || player.InvBody[r]._iMaxDur == 0)
				continue;

			player.InvBody[r]._iDurability -= 20;
			player.InvBody[r]._iMaxDur -= 20;
			if (player.InvBody[r]._iDurability <= 0)
				player.InvBody[r]._iDurability = 1;
			if (player.InvBody[r]._iMaxDur <= 0)
				player.InvBody[r]._iMaxDur = 1;
			break;
		}
	}

	InitDiabloMsg(EMSG_SHRINE_HIDDEN);
}

void OperateShrineGloomy(Player &player)
{
	if (&player != MyPlayer)
		return;

	// Increment armor class by 2 and decrements max damage by 1.
	for (Item &item : PlayerItemsRange(player)) {
		switch (item._itype) {
		case ItemType::Sword:
		case ItemType::Axe:
		case ItemType::Bow:
		case ItemType::Mace:
		case ItemType::Staff:
			item._iMaxDam--;
			if (item._iMaxDam < item._iMinDam)
				item._iMaxDam = item._iMinDam;
			break;
		case ItemType::Shield:
		case ItemType::Helm:
		case ItemType::LightArmor:
		case ItemType::MediumArmor:
		case ItemType::HeavyArmor:
			item._iAC += 2;
			break;
		default:
			break;
		}
	}

	CalcPlrInv(player, true);

	InitDiabloMsg(EMSG_SHRINE_GLOOMY);
}

void OperateShrineWeird(Player &player)
{
	if (&player != MyPlayer)
		return;

	if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._itype != ItemType::Shield)
		player.InvBody[INVLOC_HAND_LEFT]._iMaxDam++;
	if (!player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._itype != ItemType::Shield)
		player.InvBody[INVLOC_HAND_RIGHT]._iMaxDam++;

	for (Item &item : InventoryPlayerItemsRange { player }) {
		switch (item._itype) {
		case ItemType::Sword:
		case ItemType::Axe:
		case ItemType::Bow:
		case ItemType::Mace:
		case ItemType::Staff:
			item._iMaxDam++;
			break;
		default:
			break;
		}
	}

	CalcPlrInv(player, true);

	InitDiabloMsg(EMSG_SHRINE_WEIRD);
}

void OperateShrineMagical(const Player &player)
{
	AddMissile(
	    player.position.tile,
	    player.position.tile,
	    player._pdir,
	    MissileID::ManaShield,
	    TARGET_MONSTERS,
	    player,
	    0,
	    2 * leveltype);

	if (&player != MyPlayer)
		return;

	InitDiabloMsg(EMSG_SHRINE_MAGICAL);
}

void OperateShrineStone(Player &player)
{
	if (&player != MyPlayer)
		return;

	for (Item &item : PlayerItemsRange { player }) {
		if (item._itype == ItemType::Staff)
			item._iCharges = item._iMaxCharges;
	}

	CalcPlrInv(player, true);

	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_STONE);
}

void OperateShrineReligious(Player &player)
{
	if (&player != MyPlayer)
		return;

	for (Item &item : PlayerItemsRange { player }) {
		item._iDurability = item._iMaxDur;
	}

	InitDiabloMsg(EMSG_SHRINE_RELIGIOUS);
}

void OperateShrineEnchanted(DiabloGenerator &rng, Player &player)
{
	if (&player != MyPlayer)
		return;

	int cnt = 0;
	uint64_t spell = 1;
	const uint64_t spells = player._pMemSpells;
	for (uint16_t j = 0; j < SpellsData.size(); j++) {
		if ((spell & spells) != 0)
			cnt++;
		spell *= 2;
	}
	if (cnt > 1) {
		int spellToReduce;
		do {
			spellToReduce = rng.generateRnd(static_cast<int32_t>(SpellsData.size())) + 1;
		} while ((player._pMemSpells & GetSpellBitmask(static_cast<SpellID>(spellToReduce))) == 0);

		spell = 1;
		for (uint8_t j = static_cast<uint8_t>(SpellID::Firebolt); j < SpellsData.size(); j++) {
			if ((player._pMemSpells & spell) != 0 && player._pSplLvl[j] < MaxSpellLevel && j != spellToReduce) {
				const uint8_t newSpellLevel = static_cast<uint8_t>(player._pSplLvl[j] + 1);
				player._pSplLvl[j] = newSpellLevel;
				NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, j, newSpellLevel);
			}
			spell *= 2;
		}

		if (player._pSplLvl[spellToReduce] > 0) {
			const uint8_t newSpellLevel = static_cast<uint8_t>(player._pSplLvl[spellToReduce] - 1);
			player._pSplLvl[spellToReduce] = newSpellLevel;
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, spellToReduce, newSpellLevel);
		}

		if (&player == MyPlayer) {
			for (Item &item : InventoryPlayerItemsRange { player }) {
				item.updateRequiredStatsCacheForPlayer(player);
			}
			if (IsStashOpen) {
				Stash.RefreshItemStatFlags();
			}
		}
	}

	InitDiabloMsg(EMSG_SHRINE_ENCHANTED);
}

void OperateShrineThaumaturgic(DiabloGenerator &rng, const Player &player)
{
	for (int j = 0; j < ActiveObjectCount; j++) {
		Object &object = Objects[ActiveObjects[j]];
		if (object.IsChest() && !object.canInteractWith()) {
			object._oRndSeed = rng.advanceRndSeed();
			object.selectionRegion = SelectionRegion::Bottom;
			object._oAnimFrame -= 2;
		}
	}

	if (&player != MyPlayer)
		return;

	InitDiabloMsg(EMSG_SHRINE_THAUMATURGIC);
}

void OperateShrineCostOfWisdom(Player &player, SpellID spellId, diablo_message message)
{
	if (&player != MyPlayer)
		return;

	player._pMemSpells |= GetSpellBitmask(spellId);

	const uint8_t curSpellLevel = player._pSplLvl[static_cast<int8_t>(spellId)];
	if (curSpellLevel < MaxSpellLevel) {
		const uint8_t newSpellLevel = std::min(static_cast<uint8_t>(curSpellLevel + 2), MaxSpellLevel);
		player._pSplLvl[static_cast<int8_t>(spellId)] = newSpellLevel;
		NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, static_cast<uint16_t>(spellId), newSpellLevel);
	}

	if (&player == MyPlayer) {
		for (Item &item : InventoryPlayerItemsRange { player }) {
			item.updateRequiredStatsCacheForPlayer(player);
		}
		if (IsStashOpen) {
			Stash.RefreshItemStatFlags();
		}
	}

	const uint32_t t = player._pMaxManaBase / 10;
	const int v1 = player._pMana - player._pManaBase;
	const int v2 = player._pMaxMana - player._pMaxManaBase;
	player._pManaBase -= t;
	player._pMana -= t;
	player._pMaxMana -= t;
	player._pMaxManaBase -= t;
	if (player._pMana >> 6 <= 0) {
		player._pMana = v1;
		player._pManaBase = 0;
	}
	if (player._pMaxMana >> 6 <= 0) {
		player._pMaxMana = v2;
		player._pMaxManaBase = 0;
	}

	RedrawEverything();

	InitDiabloMsg(message);
}

void OperateShrineCryptic(Player &player)
{
	AddMissile(
	    player.position.tile,
	    player.position.tile,
	    player._pdir,
	    MissileID::Nova,
	    TARGET_MONSTERS,
	    player,
	    0,
	    2 * leveltype);

	if (&player != MyPlayer)
		return;

	player._pMana = player._pMaxMana;
	player._pManaBase = player._pMaxManaBase;

	InitDiabloMsg(EMSG_SHRINE_CRYPTIC);

	RedrawEverything();
}

void OperateShrineEldritch(Player &player)
{
	if (&player != MyPlayer)
		return;

	for (Item &item : InventoryAndBeltPlayerItemsRange { player }) {
		if (item._itype != ItemType::Misc) {
			continue;
		}
		if (IsAnyOf(item._iMiscId, IMISC_HEAL, IMISC_MANA)) {
			// Reinitializing the item zeroes out the seed, we save and restore here to avoid triggering false
			// positives on duplicated item checks (e.g. when picking up the item).
			auto seed = item._iSeed;
			InitializeItem(item, ItemMiscIdIdx(IMISC_REJUV));
			item._iSeed = seed;
			item._iStatFlag = true;
			continue;
		}
		if (IsAnyOf(item._iMiscId, IMISC_FULLHEAL, IMISC_FULLMANA)) {
			// As above.
			auto seed = item._iSeed;
			InitializeItem(item, ItemMiscIdIdx(IMISC_FULLREJUV));
			item._iSeed = seed;
			item._iStatFlag = true;
			continue;
		}
	}

	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_ELDRITCH);
}

void OperateShrineEerie(Player &player)
{
	if (&player != MyPlayer)
		return;

	ModifyPlrMag(player, 2);
	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_EERIE);
}

/**
 * @brief Fully restores HP and Mana of the active player and spawns a pair of potions
 *        in response to the player activating a Divine shrine
 * @param player The player who activated the shrine
 * @param spawnPosition The map tile where the potions will be spawned
 */
void OperateShrineDivine(Player &player, Point spawnPosition)
{
	if (&player != MyPlayer)
		return;

	if (currlevel < 4) {
		CreateTypeItem(spawnPosition, false, ItemType::Misc, IMISC_FULLMANA, false, false, true);
		CreateTypeItem(spawnPosition, false, ItemType::Misc, IMISC_FULLHEAL, false, false, true);
	} else {
		CreateTypeItem(spawnPosition, false, ItemType::Misc, IMISC_FULLREJUV, false, false, true);
		CreateTypeItem(spawnPosition, false, ItemType::Misc, IMISC_FULLREJUV, false, false, true);
	}

	player._pMana = player._pMaxMana;
	player._pManaBase = player._pMaxManaBase;
	player._pHitPoints = player._pMaxHP;
	player._pHPBase = player._pMaxHPBase;

	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_DIVINE);
}

void OperateShrineHoly(const Player &player)
{
	AddMissile(player.position.tile, { 0, 0 }, Direction::South, MissileID::Phasing, TARGET_MONSTERS, player, 0, 2 * leveltype);

	if (&player != MyPlayer)
		return;

	InitDiabloMsg(EMSG_SHRINE_HOLY);
}

void OperateShrineSpiritual(DiabloGenerator &rng, Player &player)
{
	if (&player != MyPlayer)
		return;

	for (int8_t &itemIndex : player.InvGrid) {
		if (itemIndex == 0) {
			Item &goldItem = player.InvList[player._pNumInv];
			MakeGoldStack(goldItem, 5 * leveltype + rng.generateRnd(10 * leveltype));
			player._pNumInv++;
			itemIndex = player._pNumInv;

			player._pGold += goldItem._ivalue;
		}
	}

	InitDiabloMsg(EMSG_SHRINE_SPIRITUAL);
}

void OperateShrineSpooky(const Player &player)
{
	if (&player == MyPlayer) {
		InitDiabloMsg(EMSG_SHRINE_SPOOKY1);
		return;
	}

	Player &myPlayer = *MyPlayer;

	myPlayer._pHitPoints = myPlayer._pMaxHP;
	myPlayer._pHPBase = myPlayer._pMaxHPBase;
	myPlayer._pMana = myPlayer._pMaxMana;
	myPlayer._pManaBase = myPlayer._pMaxManaBase;

	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_SPOOKY2);
}

void OperateShrineAbandoned(Player &player)
{
	if (&player != MyPlayer)
		return;

	ModifyPlrDex(player, 2);
	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_ABANDONED);
}

void OperateShrineCreepy(Player &player)
{
	if (&player != MyPlayer)
		return;

	ModifyPlrStr(player, 2);
	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_CREEPY);
}

void OperateShrineQuiet(Player &player)
{
	if (&player != MyPlayer)
		return;

	ModifyPlrVit(player, 2);
	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_QUIET);
}

void OperateShrineSecluded(const Player &player)
{
	if (&player != MyPlayer)
		return;

	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			UpdateAutomapExplorer({ x, y }, MAP_EXP_SHRINE);

	InitDiabloMsg(EMSG_SHRINE_SECLUDED);
}

void OperateShrineGlimmering(Player &player)
{
	if (&player != MyPlayer)
		return;

	for (Item &item : PlayerItemsRange { player }) {
		if (item._iMagical != ITEM_QUALITY_NORMAL && !item._iIdentified) {
			item._iIdentified = true;
		}
	}

	CalcPlrInv(player, true);
	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_GLIMMERING);
}

void OperateShrineTainted(DiabloGenerator &rng, const Player &player)
{
	if (&player == MyPlayer) {
		InitDiabloMsg(EMSG_SHRINE_TAINTED1);
		return;
	}

	const int r = rng.generateRnd(4);

	const int v1 = r == 0 ? 1 : -1;
	const int v2 = r == 1 ? 1 : -1;
	const int v3 = r == 2 ? 1 : -1;
	const int v4 = r == 3 ? 1 : -1;

	Player &myPlayer = *MyPlayer;

	ModifyPlrStr(myPlayer, v1);
	ModifyPlrMag(myPlayer, v2);
	ModifyPlrDex(myPlayer, v3);
	ModifyPlrVit(myPlayer, v4);

	CheckStats(myPlayer);
	CalcPlrInv(myPlayer, true);
	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_TAINTED2);
}

/**
 * @brief Oily shrines increase the players primary stat(s) by a total of two, but spawn a
 *        firewall near the shrine that will spread towards the player
 * @param player The player that will be affected by the shrine
 * @param spawnPosition Start location for the firewall
 */
void OperateShrineOily(Player &player, Point spawnPosition)
{
	if (&player != MyPlayer)
		return;

	switch (player._pClass) {
	case HeroClass::Warrior:
		ModifyPlrStr(player, 2);
		break;
	case HeroClass::Rogue:
		ModifyPlrDex(player, 2);
		break;
	case HeroClass::Sorcerer:
		ModifyPlrMag(player, 2);
		break;
	case HeroClass::Barbarian:
		ModifyPlrVit(player, 2);
		break;
	case HeroClass::Monk:
		ModifyPlrStr(player, 1);
		ModifyPlrDex(player, 1);
		break;
	case HeroClass::Bard:
		ModifyPlrDex(player, 1);
		ModifyPlrMag(player, 1);
		break;
	default:
		break;
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();

	AddMissile(
	    spawnPosition,
	    player.position.tile,
	    player._pdir,
	    MissileID::FireWall,
	    TARGET_PLAYERS,
	    -1,
	    2 * currlevel + 2,
	    0);

	InitDiabloMsg(EMSG_SHRINE_OILY);
}

void OperateShrineGlowing(Player &player)
{
	if (&player != MyPlayer)
		return;

	// Add 0-5 points to Magic (0.1% of the players XP)
	ModifyPlrMag(player, static_cast<int>(std::min<uint32_t>(player._pExperience / 1000, 5)));

	// Take 5% of the players experience to offset the bonus, unless they're very low level in which case take all their experience.
	if (player._pExperience > 5000)
		player._pExperience = static_cast<uint32_t>(player._pExperience * 0.95);
	else
		player._pExperience = 0;

	CheckStats(player);
	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_GLOWING);
}

void OperateShrineMendicant(Player &player)
{
	if (&player != MyPlayer)
		return;

	const int gold = player._pGold / 2;
	player.addExperience(gold);
	TakePlrsMoney(gold);

	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_MENDICANT);
}

/**
 * @brief Grants experience to the player based on the current dungeon level while also triggering a magic trap
 * @param player The player that will be affected by the shrine
 * @param spawnPosition The trap results in casting flash from this location targeting the player
 */
void OperateShrineSparkling(Player &player, Point spawnPosition)
{
	if (&player != MyPlayer)
		return;

	player.addExperience(1000 * currlevel);

	AddMissile(
	    spawnPosition,
	    player.position.tile,
	    player._pdir,
	    MissileID::FlashBottom,
	    TARGET_PLAYERS,
	    -1,
	    3 * currlevel + 2,
	    0);

	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_SPARKLING);
}

/**
 * @brief Spawns a town portal near the active player
 * @param pnum The player that activated the shrine
 * @param spawnPosition The position of the shrine, the portal will be placed on the side closest to the player
 */
void OperateShrineTown(const Player &player, Point spawnPosition)
{
	if (&player != MyPlayer)
		return;

	AddMissile(
	    spawnPosition,
	    player.position.tile,
	    player._pdir,
	    MissileID::TownPortal,
	    TARGET_MONSTERS,
	    player,
	    0,
	    0);

	InitDiabloMsg(EMSG_SHRINE_TOWN);
}

void OperateShrineShimmering(Player &player)
{
	if (&player != MyPlayer)
		return;

	player._pMana = player._pMaxMana;
	player._pManaBase = player._pMaxManaBase;

	RedrawEverything();

	InitDiabloMsg(EMSG_SHRINE_SHIMMERING);
}

void OperateShrineSolar(Player &player)
{
	if (&player != MyPlayer)
		return;

	const time_t timeResult = time(nullptr);
	const std::tm *localtimeResult = localtime(&timeResult);
	const int hour = localtimeResult != nullptr ? localtimeResult->tm_hour : 20;
	if (hour >= 20 || hour < 4) {
		InitDiabloMsg(EMSG_SHRINE_SOLAR4);
		ModifyPlrVit(player, 2);
	} else if (hour >= 18) {
		InitDiabloMsg(EMSG_SHRINE_SOLAR3);
		ModifyPlrMag(player, 2);
	} else if (hour >= 12) {
		InitDiabloMsg(EMSG_SHRINE_SOLAR2);
		ModifyPlrStr(player, 2);
	} else /* 4:00 to 11:59 */ {
		InitDiabloMsg(EMSG_SHRINE_SOLAR1);
		ModifyPlrDex(player, 2);
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();
}

void OperateShrineMurphys(DiabloGenerator &rng, Player &player)
{
	if (&player != MyPlayer)
		return;

	bool broke = false;
	for (auto &item : player.InvBody) {
		if (!item.isEmpty() && rng.flipCoin(3)) {
			if (item._iDurability != DUR_INDESTRUCTIBLE) {
				if (item._iDurability > 0) {
					item._iDurability /= 2;
					broke = true;
					break;
				}
			}
		}
	}
	if (!broke) {
		TakePlrsMoney(player._pGold / 3);
	}

	InitDiabloMsg(EMSG_SHRINE_MURPHYS);
}

void OperateShrine(Player &player, Object &shrine, SfxID sType)
{
	if (!shrine.canInteractWith())
		return;

	CloseGoldDrop();

	DiabloGenerator rng(shrine._oRndSeed);
	shrine.selectionRegion = SelectionRegion::None;

	PlaySfxLoc(sType, shrine.position);
	shrine._oAnimFlag = true;
	shrine._oAnimDelay = 1;

	switch (shrine._oVar1) {
	case ShrineMysterious:
		OperateShrineMysterious(rng, player);
		break;
	case ShrineHidden:
		OperateShrineHidden(rng, player);
		break;
	case ShrineGloomy:
		OperateShrineGloomy(player);
		break;
	case ShrineWeird:
		OperateShrineWeird(player);
		break;
	case ShrineMagical:
	case ShrineMagicaL2:
		OperateShrineMagical(player);
		break;
	case ShrineStone:
		OperateShrineStone(player);
		break;
	case ShrineReligious:
		OperateShrineReligious(player);
		break;
	case ShrineEnchanted:
		OperateShrineEnchanted(rng, player);
		break;
	case ShrineThaumaturgic:
		OperateShrineThaumaturgic(rng, player);
		break;
	case ShrineFascinating:
		OperateShrineCostOfWisdom(player, SpellID::Firebolt, EMSG_SHRINE_FASCINATING);
		break;
	case ShrineCryptic:
		OperateShrineCryptic(player);
		break;
	case ShrineEldritch:
		OperateShrineEldritch(player);
		break;
	case ShrineEerie:
		OperateShrineEerie(player);
		break;
	case ShrineDivine:
		OperateShrineDivine(player, shrine.position);
		break;
	case ShrineHoly:
		OperateShrineHoly(player);
		break;
	case ShrineSacred:
		OperateShrineCostOfWisdom(player, SpellID::ChargedBolt, EMSG_SHRINE_SACRED);
		break;
	case ShrineSpiritual:
		OperateShrineSpiritual(rng, player);
		break;
	case ShrineSpooky:
		OperateShrineSpooky(player);
		break;
	case ShrineAbandoned:
		OperateShrineAbandoned(player);
		break;
	case ShrineCreepy:
		OperateShrineCreepy(player);
		break;
	case ShrineQuiet:
		OperateShrineQuiet(player);
		break;
	case ShrineSecluded:
		OperateShrineSecluded(player);
		break;
	case ShrineOrnate:
		OperateShrineCostOfWisdom(player, SpellID::HolyBolt, EMSG_SHRINE_ORNATE);
		break;
	case ShrineGlimmering:
		OperateShrineGlimmering(player);
		break;
	case ShrineTainted:
		OperateShrineTainted(rng, player);
		break;
	case ShrineOily:
		OperateShrineOily(player, shrine.position);
		break;
	case ShrineGlowing:
		OperateShrineGlowing(player);
		break;
	case ShrineMendicant:
		OperateShrineMendicant(player);
		break;
	case ShrineSparkling:
		OperateShrineSparkling(player, shrine.position);
		break;
	case ShrineTown:
		OperateShrineTown(player, shrine.position);
		break;
	case ShrineShimmering:
		OperateShrineShimmering(player);
		break;
	case ShrineSolar:
		OperateShrineSolar(player);
		break;
	case ShrineMurphys:
		OperateShrineMurphys(rng, player);
		break;
	}

	if (&player == MyPlayer)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, shrine.position);
}

void OperateBookStand(Object &bookStand, bool sendmsg, bool sendLootMsg)
{
	if (!bookStand.canInteractWith()) {
		return;
	}

	PlaySfxLoc(SfxID::ItemScroll, bookStand.position);
	bookStand.selectionRegion = SelectionRegion::None;
	bookStand._oAnimFrame += 2;
	SetRndSeed(bookStand._oRndSeed);
	if (FlipCoin(5))
		CreateTypeItem(bookStand.position, false, ItemType::Misc, IMISC_BOOK, sendLootMsg, false);
	else
		CreateTypeItem(bookStand.position, false, ItemType::Misc, IMISC_SCROLL, sendLootMsg, false);
	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, bookStand.position);
}

void OperateBookcase(Object &bookcase, bool sendmsg, bool sendLootMsg)
{
	if (!bookcase.canInteractWith()) {
		return;
	}

	PlaySfxLoc(SfxID::ItemScroll, bookcase.position);
	bookcase.selectionRegion = SelectionRegion::None;
	bookcase._oAnimFrame -= 2;
	SetRndSeed(bookcase._oRndSeed);
	CreateTypeItem(bookcase.position, false, ItemType::Misc, IMISC_BOOK, sendLootMsg, false);

	if (Quests[Q_ZHAR].IsAvailable()) {
		Monster &zhar = Monsters[MAX_PLRS];
		if (zhar.mode == MonsterMode::Stand // prevents playing the "angry" message for the second time if zhar got aggroed by losing vision and talking again
		    && zhar.uniqueType == UniqueMonsterType::Zhar
		    && zhar.activeForTicks == UINT8_MAX
		    && zhar.hitPoints > 0) {
			zhar.talkMsg = TEXT_ZHAR2;
			M_StartStand(zhar, zhar.direction); // BUGFIX: first parameter in call to M_StartStand should be MAX_PLRS, not 0. (fixed)
			zhar.goal = MonsterGoal::Attack;
			if (sendmsg)
				zhar.mode = MonsterMode::Talk;
		}
	}
	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, bookcase.position);
}

void OperateDecapitatedBody(Object &corpse, bool sendmsg, bool sendLootMsg)
{
	if (!corpse.canInteractWith()) {
		return;
	}
	corpse.selectionRegion = SelectionRegion::None;
	SetRndSeed(corpse._oRndSeed);
	CreateRndItem(corpse.position, false, sendLootMsg, false);
	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, corpse.position);
}

void OperateArmorStand(Object &armorStand, bool sendmsg, bool sendLootMsg)
{
	if (!armorStand.canInteractWith()) {
		return;
	}
	armorStand.selectionRegion = SelectionRegion::None;
	armorStand._oAnimFrame++;
	SetRndSeed(armorStand._oRndSeed);
	const bool uniqueRnd = !FlipCoin();
	if (currlevel <= 5) {
		CreateTypeItem(armorStand.position, true, ItemType::LightArmor, IMISC_NONE, sendLootMsg, false);
	} else if (currlevel >= 6 && currlevel <= 9) {
		CreateTypeItem(armorStand.position, uniqueRnd, ItemType::MediumArmor, IMISC_NONE, sendLootMsg, false);
	} else if (currlevel >= 10 && currlevel <= 12) {
		CreateTypeItem(armorStand.position, false, ItemType::HeavyArmor, IMISC_NONE, sendLootMsg, false);
	} else if (currlevel >= 13) {
		CreateTypeItem(armorStand.position, true, ItemType::HeavyArmor, IMISC_NONE, sendLootMsg, false);
	}
	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, armorStand.position);
}

int FindValidShrine()
{
	for (;;) {
		const int rv = GenerateRnd(gbIsHellfire ? NumberOfShrineTypes : 26);
		if ((rv == ShrineEnchanted && !IsAnyOf(leveltype, DTYPE_CATHEDRAL, DTYPE_CATACOMBS)) || rv == ShrineThaumaturgic)
			continue;
		if (gbIsMultiplayer && shrineavail[rv] == ShrineTypeSingle)
			continue;
		if (!gbIsMultiplayer && shrineavail[rv] == ShrineTypeMulti)
			continue;
		return rv;
	}
}

void OperateGoatShrine(Player &player, Object &object, SfxID sType)
{
	SetRndSeed(object._oRndSeed);
	object._oVar1 = FindValidShrine();
	OperateShrine(player, object, sType);
	object._oAnimDelay = 2;
	RedrawEverything();
}

void OperateCauldron(Player &player, Object &object, SfxID sType)
{
	SetRndSeed(object._oRndSeed);
	object._oVar1 = FindValidShrine();
	OperateShrine(player, object, sType);
	object._oAnimFrame = 3;
	object._oAnimFlag = false;
	RedrawEverything();
}

bool OperateFountains(Player &player, Object &fountain)
{
	bool applied = false;
	switch (fountain._otype) {
	case OBJ_BLOODFTN:
		if (&player != MyPlayer)
			return false;

		if (player._pHitPoints < player._pMaxHP) {
			PlaySfxLoc(SfxID::OperateFountain, fountain.position);
			player._pHitPoints += 64;
			player._pHPBase += 64;
			if (player._pHitPoints > player._pMaxHP) {
				player._pHitPoints = player._pMaxHP;
				player._pHPBase = player._pMaxHPBase;
			}
			applied = true;
		} else
			PlaySfxLoc(SfxID::OperateFountain, fountain.position);
		break;
	case OBJ_PURIFYINGFTN:
		if (&player != MyPlayer)
			return false;

		if (player._pMana < player._pMaxMana) {
			PlaySfxLoc(SfxID::OperateFountain, fountain.position);

			player._pMana += 64;
			player._pManaBase += 64;
			if (player._pMana > player._pMaxMana) {
				player._pMana = player._pMaxMana;
				player._pManaBase = player._pMaxManaBase;
			}

			applied = true;
		} else
			PlaySfxLoc(SfxID::OperateFountain, fountain.position);
		break;
	case OBJ_MURKYFTN:
		if (!fountain.canInteractWith())
			break;
		PlaySfxLoc(SfxID::OperateFountain, fountain.position);
		fountain.selectionRegion = SelectionRegion::None;
		AddMissile(
		    player.position.tile,
		    player.position.tile,
		    player._pdir,
		    MissileID::Infravision,
		    TARGET_MONSTERS,
		    player,
		    0,
		    2 * leveltype);
		applied = true;
		if (&player == MyPlayer)
			NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, fountain.position);
		break;
	case OBJ_TEARFTN: {
		if (!fountain.canInteractWith())
			break;
		PlaySfxLoc(SfxID::OperateFountain, fountain.position);
		fountain.selectionRegion = SelectionRegion::None;
		if (&player != MyPlayer)
			return false;

		const unsigned randomValue = (fountain._oRndSeed >> 16) % 12;
		const unsigned fromStat = randomValue / 3;
		unsigned toStat = randomValue % 3;
		if (toStat >= fromStat)
			toStat++;

		const std::pair<unsigned, int> alterations[] = { { fromStat, -1 }, { toStat, 1 } };
		for (const auto &[stat, delta] : alterations) {
			switch (stat) {
			case 0:
				ModifyPlrStr(player, delta);
				break;
			case 1:
				ModifyPlrMag(player, delta);
				break;
			case 2:
				ModifyPlrDex(player, delta);
				break;
			case 3:
				ModifyPlrVit(player, delta);
				break;
			}
		}

		CheckStats(player);
		applied = true;
		if (&player == MyPlayer)
			NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, fountain.position);
	} break;
	default:
		break;
	}
	RedrawEverything();
	return applied;
}

void OperateWeaponRack(Object &weaponRack, bool sendmsg, bool sendLootMsg)
{
	if (!weaponRack.canInteractWith())
		return;
	SetRndSeed(weaponRack._oRndSeed);

	const ItemType weaponType { PickRandomlyAmong({ ItemType::Sword, ItemType::Axe, ItemType::Bow, ItemType::Mace }) };

	weaponRack.selectionRegion = SelectionRegion::None;
	weaponRack._oAnimFrame++;

	CreateTypeItem(weaponRack.position, leveltype != DTYPE_CATHEDRAL, weaponType, IMISC_NONE, sendLootMsg, false);

	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, weaponRack.position);
}

/**
 * @brief Checks whether the player is activating Na-Krul's spell tomes in the correct order
 *
 * Used as part of the final Diablo: Hellfire quest (from the hints provided to the player in the
 * reconstructed note). This function both updates the state of the variable that tracks progress
 * and also determines whether the spawn conditions are met (i.e. all tomes have been triggered
 * in the correct order).
 *
 * @param s the id of the spell tome
 * @return true if the player has activated all three tomes in the correct order, false otherwise
 */
bool OperateNakrulBook(int s)
{
	switch (s) {
	case 6:
		NaKrulTomeSequence = 1;
		break;
	case 7:
		if (NaKrulTomeSequence == 1) {
			NaKrulTomeSequence = 2;
		} else {
			NaKrulTomeSequence = 0;
		}
		break;
	case 8:
		if (NaKrulTomeSequence == 2)
			return true;
		NaKrulTomeSequence = 0;
		break;
	}
	return false;
}

void OperateStoryBook(Object &storyBook)
{
	if (!storyBook.canInteractWith() || qtextflag) {
		return;
	}
	storyBook._oAnimFrame = storyBook._oVar4;
	PlaySfxLoc(SfxID::ItemScroll, storyBook.position);
	auto msg = static_cast<_speech_id>(storyBook._oVar2);
	if (storyBook._oVar8 != 0 && currlevel == 24) {
		if (!IsUberLeverActivated && Quests[Q_NAKRUL]._qactive != QUEST_DONE && OperateNakrulBook(storyBook._oVar8)) {
			NetSendCmd(false, CMD_NAKRUL);
			return;
		}
	} else if (leveltype == DTYPE_CRYPT) {
		Quests[Q_NAKRUL]._qactive = QUEST_ACTIVE;
		Quests[Q_NAKRUL]._qlog = true;
		Quests[Q_NAKRUL]._qmsg = msg;
		NetSendCmdQuest(true, Quests[Q_NAKRUL]);
	}
	InitQTextMsg(msg);
	NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, storyBook.position);
}

void OperateLazStand(Object &stand)
{
	if (ActiveItemCount >= MAXITEMS) {
		return;
	}

	if (!stand.canInteractWith() || qtextflag) {
		return;
	}

	stand._oAnimFrame++;
	stand.selectionRegion = SelectionRegion::None;
	const Point pos = GetSuperItemLoc(stand.position);
	SpawnQuestItem(IDI_LAZSTAFF, pos, 0, SelectionRegion::None, true);
	NetSendCmdLoc(MyPlayerId, false, CMD_OPERATEOBJ, stand.position);
}

/**
 * @brief Checks if all active crux objects of the given type have been broken.
 *
 * Called by BreakCrux and SyncCrux to see if the linked map area needs to be updated. In practice I think this is
 * always true when called by BreakCrux as there *should* only be one instance of each crux with a given _oVar8 value?
 *
 * @param cruxType Discriminator/type (_oVar8 value) of the crux object which is currently changing state
 * @return true if all active cruxes of that type on the level are broken, false if at least one remains unbroken
 */
bool AreAllCruxesOfTypeBroken(int cruxType)
{
	for (int j = 0; j < ActiveObjectCount; j++) {
		const auto &testObject = Objects[ActiveObjects[j]];
		if (!testObject.IsCrux())
			continue; // Not a Crux object, keep searching
		if (cruxType != testObject._oVar8 || testObject._oBreak == -1)
			continue; // Found either a different crux or a previously broken crux, keep searching

		// Found an unbroken crux of this type
		return false;
	}
	return true;
}

void BreakCrux(Object &crux, bool sendmsg)
{
	if (!crux.canInteractWith())
		return;

	crux._oAnimFlag = true;
	crux._oAnimFrame = 1;
	crux._oAnimDelay = 1;
	crux._oSolidFlag = true;
	crux._oMissFlag = true;
	crux._oBreak = -1;
	crux.selectionRegion = SelectionRegion::None;

	if (sendmsg)
		NetSendCmdLoc(MyPlayerId, false, CMD_BREAKOBJ, crux.position);

	if (!AreAllCruxesOfTypeBroken(crux._oVar8))
		return;

	PlaySfxLoc(SfxID::OperateLever, crux.position);
	ObjChangeMap(crux._oVar1, crux._oVar2, crux._oVar3, crux._oVar4);
}

void BreakBarrel(const Player &player, Object &barrel, bool forcebreak, bool sendmsg)
{
	if (!barrel.canInteractWith())
		return;
	if (!forcebreak && &player != MyPlayer) {
		return;
	}

	barrel._oAnimFlag = true;
	barrel._oAnimFrame = 1;
	barrel._oAnimDelay = 1;
	barrel._oSolidFlag = false;
	barrel._oMissFlag = true;
	barrel._oBreak = -1;
	barrel.selectionRegion = SelectionRegion::None;
	barrel._oPreFlag = true;

	if (barrel.isExplosive()) {
		if (barrel._otype == _object_id::OBJ_URNEX)
			PlaySfxLoc(SfxID::UrnExpload, barrel.position);
		else if (barrel._otype == _object_id::OBJ_PODEX)
			PlaySfxLoc(SfxID::PodExpload, barrel.position);
		else
			PlaySfxLoc(SfxID::BarrelExpload, barrel.position);
		for (int yp = barrel.position.y - 1; yp <= barrel.position.y + 1; yp++) {
			for (int xp = barrel.position.x - 1; xp <= barrel.position.x + 1; xp++) {
				constexpr MissileID TrapMissile = MissileID::Firebolt;
				Monster *monster = FindMonsterAtPosition({ xp, yp }, true);
				if (monster != nullptr) {
					MonsterTrapHit(*monster, 1, 4, 0, TrapMissile, GetMissileData(TrapMissile).damageType(), false);
				}
				Player *adjacentPlayer = PlayerAtPosition({ xp, yp }, true);
				if (adjacentPlayer != nullptr) {
					bool unused;
					PlayerMHit(*adjacentPlayer, nullptr, 0, 8, 16, TrapMissile, GetMissileData(TrapMissile).damageType(), false, DeathReason::MonsterOrTrap, &unused);
				}
				// don't really need to exclude large objects as explosive barrels are single tile objects, but using considerLargeObjects == false as this matches the old logic.
				Object *adjacentObject = FindObjectAtPosition({ xp, yp }, false);
				if (adjacentObject != nullptr && adjacentObject->isExplosive() && !adjacentObject->IsBroken()) {
					BreakBarrel(player, *adjacentObject, true, sendmsg);
				}
			}
		}
	} else {
		if (barrel._otype == _object_id::OBJ_URN)
			PlaySfxLoc(SfxID::UrnBreak, barrel.position);
		else if (barrel._otype == _object_id::OBJ_POD)
			PlaySfxLoc(SfxID::PodPop, barrel.position);
		else
			PlaySfxLoc(SfxID::BarrelBreak, barrel.position);
		SetRndSeed(barrel._oRndSeed);
		if (barrel._oVar2 <= 1) {
			if (barrel._oVar3 == 0)
				CreateRndUseful(barrel.position, sendmsg);
			else
				CreateRndItem(barrel.position, false, sendmsg, false);
		}
		if (barrel._oVar2 >= 8 && barrel._oVar4 >= 0)
			ActivateSkeleton(Monsters[barrel._oVar4], barrel.position);
	}
	if (&player == MyPlayer) {
		NetSendCmdLoc(MyPlayerId, false, CMD_BREAKOBJ, barrel.position);
	}
}

void SyncCrux(const Object &crux)
{
	if (AreAllCruxesOfTypeBroken(crux._oVar8))
		ObjChangeMap(crux._oVar1, crux._oVar2, crux._oVar3, crux._oVar4);
}

void SyncLever(const Object &lever)
{
	if (lever.canInteractWith())
		return;

	if (currlevel == 16 && !AreAllLeversActivated(lever._oVar8))
		return;

	ObjChangeMap(lever._oVar1, lever._oVar2, lever._oVar3, lever._oVar4);
}

void SyncQSTLever(const Object &qstLever)
{
	if (qstLever._oAnimFrame == qstLever._oVar6) {
		if (qstLever._otype != OBJ_BLOODBOOK)
			ObjChangeMapResync(qstLever._oVar1, qstLever._oVar2, qstLever._oVar3, qstLever._oVar4);
		if (qstLever._otype == OBJ_BLINDBOOK) {
			auto tren = TransVal;
			TransVal = 9;
			DRLG_MRectTrans(WorldTilePosition(qstLever._oVar1, qstLever._oVar2), WorldTilePosition(qstLever._oVar3, qstLever._oVar4));
			TransVal = tren;
		}
	}
}

void SyncPedestal(const Object &pedestal)
{
	if (pedestal._oVar6 == 1)
		ObjChangeMapResync(SetPiece.position.x, SetPiece.position.y + 3, SetPiece.position.x + 2, SetPiece.position.y + 7);
	if (pedestal._oVar6 == 2) {
		ObjChangeMapResync(SetPiece.position.x, SetPiece.position.y + 3, SetPiece.position.x + 2, SetPiece.position.y + 7);
		ObjChangeMapResync(SetPiece.position.x + 6, SetPiece.position.y + 3, SetPiece.position.x + SetPiece.size.width, SetPiece.position.y + 7);
	}
	if (pedestal._oVar6 >= 3) {
		ObjChangeMapResync(pedestal._oVar1, pedestal._oVar2, pedestal._oVar3, pedestal._oVar4);
		LoadMapObjects("levels\\l2data\\blood2.dun", SetPiece.position.megaToWorld());
	}
}

void UpdatePedestalState(Object &pedestal)
{
	const int addedStones = Quests[Q_BLOOD]._qvar2;
	pedestal._oAnimFrame += addedStones;
	pedestal._oVar6 += addedStones;
	SyncPedestal(pedestal);
	if (pedestal._oVar6 >= 3)
		pedestal.selectionRegion = SelectionRegion::None;
}

void SyncDoor(Object &door)
{
	if (door._oVar4 == DOOR_CLOSED) {
		SetDoorStateClosed(door);
	} else {
		SetDoorStateOpen(door);
	}
}

void ResyncDoors(WorldTilePosition p1, WorldTilePosition p2, bool sendmsg)
{
	const WorldTileSize size { static_cast<WorldTileCoord>(p2.x - p1.x), static_cast<WorldTileCoord>(p2.y - p1.y) };
	const WorldTileRectangle area { p1, size };

	for (const WorldTilePosition p : PointsInRectangle { area }) {
		Object *obj = FindObjectAtPosition(p);
		if (obj == nullptr)
			continue;
		if (IsNoneOf(obj->_otype, OBJ_L1LDOOR, OBJ_L1RDOOR, OBJ_L2LDOOR, OBJ_L2RDOOR, OBJ_L3LDOOR, OBJ_L3RDOOR, OBJ_L5LDOOR, OBJ_L5RDOOR))
			continue;
		SyncDoor(*obj);
		if (sendmsg) {
			const bool isOpen = obj->_oVar4 == DOOR_OPEN;
			NetSendCmdLoc(MyPlayerId, true, isOpen ? CMD_OPENDOOR : CMD_CLOSEDOOR, obj->position);
		}
	}
}

void UpdateState(Object &object, int frame)
{
	if (!object.canInteractWith()) {
		return;
	}

	object.selectionRegion = SelectionRegion::None;
	object._oAnimFrame = frame;
	object._oAnimFlag = false;
}

} // namespace

unsigned int Object::GetId() const
{
	return std::abs(dObject[position.x][position.y]) - 1;
}

bool Object::IsDisabled() const
{
	if (!*GetOptions().Gameplay.disableCripplingShrines) {
		return false;
	}
	if (IsAnyOf(_otype, _object_id::OBJ_GOATSHRINE, _object_id::OBJ_CAULDRON)) {
		return true;
	}
	if (!IsShrine()) {
		return false;
	}
	return IsAnyOf(static_cast<shrine_type>(_oVar1), shrine_type::ShrineFascinating, shrine_type::ShrineOrnate, shrine_type::ShrineSacred, shrine_type::ShrineMurphys);
}

Object *FindObjectAtPosition(Point position, bool considerLargeObjects)
{
	if (!InDungeonBounds(position)) {
		return nullptr;
	}

	auto objectId = dObject[position.x][position.y];

	if (objectId > 0 || (considerLargeObjects && objectId != 0)) {
		return &Objects[std::abs(objectId) - 1];
	}

	// nothing at this position, return a nullptr
	return nullptr;
}

bool IsItemBlockingObjectAtPosition(Point position)
{
	Object *object = FindObjectAtPosition(position);
	if (object != nullptr && object->_oSolidFlag) {
		// solid object
		return true;
	}

	object = FindObjectAtPosition(position + Direction::South);
	if (object != nullptr && object->canInteractWith()) {
		// An unopened container or breakable object exists which potentially overlaps this tile, the player might not be able to pick up an item dropped here.
		return true;
	}

	object = FindObjectAtPosition(position + Direction::SouthEast, false);
	if (object != nullptr) {
		Object *otherDoor = FindObjectAtPosition(position + Direction::SouthWest, false);
		if (otherDoor != nullptr && object->canInteractWith() && otherDoor->canInteractWith()) {
			// Two interactive objects potentially overlap both sides of this tile, as above the player might not be able to pick up an item which is dropped here.
			return true;
		}
	}

	return false;
}

tl::expected<void, std::string> LoadLevelObjects(uint16_t filesWidths[65])
{
	if (HeadlessMode)
		return {};

	for (const ObjectData objectData : AllObjects) {
		if (leveltype == objectData.olvltype) {
			filesWidths[objectData.ofindex] = objectData.animWidth;
		}
	}

	for (size_t i = 0, n = ObjMasterLoadList.size(); i < n; ++i) {
		if (filesWidths[i] == 0) {
			continue;
		}

		ObjFileList[numobjfiles] = static_cast<object_graphic_id>(i);
		char filestr[32];
		*BufCopy(filestr, "objects\\", ObjMasterLoadList[i]) = '\0';
		ASSIGN_OR_RETURN(pObjCels[numobjfiles], LoadCelWithStatus(filestr, filesWidths[i]));
		numobjfiles++;
	}
	return {};
}

tl::expected<void, std::string> InitObjectGFX()
{
	uint16_t filesWidths[65] = {};

	if (IsAnyOf(currlevel, 4, 8, 12)) {
		for (const auto id : { OBJ_STORYBOOK, OBJ_STORYCANDLE }) {
			const ObjectData &obj = AllObjects[id];
			filesWidths[obj.ofindex] = obj.animWidth;
		}
	}

	for (size_t id = 0, n = AllObjects.size(); id < n; ++id) {
		const ObjectData &objectData = AllObjects[id];
		if (objectData.minlvl != 0 && currlevel >= objectData.minlvl && currlevel <= objectData.maxlvl) {
			if (IsAnyOf(static_cast<_object_id>(id), OBJ_TRAPL, OBJ_TRAPR) && leveltype == DTYPE_HELL) {
				continue;
			}

			filesWidths[objectData.ofindex] = objectData.animWidth;
		}
		if (objectData.otheme != THEME_NONE) {
			for (int j = 0; j < numthemes; j++) {
				if (themes[j].ttype == objectData.otheme) {
					filesWidths[objectData.ofindex] = objectData.animWidth;
				}
			}
		}

		if (objectData.oquest != Q_INVALID && Quests[objectData.oquest].IsAvailable()) {
			filesWidths[objectData.ofindex] = objectData.animWidth;
		}
	}

	return LoadLevelObjects(filesWidths);
}

void FreeObjectGFX()
{
	for (int i = 0; i < numobjfiles; i++) {
		pObjCels[i] = std::nullopt;
	}
	numobjfiles = 0;
}

void AddL1Objs(int x1, int y1, int x2, int y2)
{
	for (int j = y1; j < y2; j++) {
		for (int i = x1; i < x2; i++) {
			const int pn = dPiece[i][j];
			if (pn == 269)
				AddObject(OBJ_L1LIGHT, { i, j });
			if (pn == 43 || pn == 50 || pn == 213)
				AddObject(OBJ_L1LDOOR, { i, j });
			if (pn == 45 || pn == 55)
				AddObject(OBJ_L1RDOOR, { i, j });
		}
	}
}

void AddL2Objs(int x1, int y1, int x2, int y2)
{
	for (int j = y1; j < y2; j++) {
		for (int i = x1; i < x2; i++) {
			const int pn = dPiece[i][j];
			if (pn == 12 || pn == 540)
				AddObject(OBJ_L2LDOOR, { i, j });
			if (pn == 16 || pn == 541)
				AddObject(OBJ_L2RDOOR, { i, j });
		}
	}
}

void AddL3Objs(int x1, int y1, int x2, int y2)
{
	for (int j = y1; j < y2; j++) {
		for (int i = x1; i < x2; i++) {
			const int pn = dPiece[i][j];
			if (pn == 530)
				AddObject(OBJ_L3LDOOR, { i, j });
			if (pn == 533)
				AddObject(OBJ_L3RDOOR, { i, j });
		}
	}
}

void AddCryptObjects(int x1, int y1, int x2, int y2)
{
	for (int j = y1; j < y2; j++) {
		for (int i = x1; i < x2; i++) {
			const int pn = dPiece[i][j];
			if (pn == 76)
				AddObject(OBJ_L5LDOOR, { i, j });
			if (pn == 79)
				AddObject(OBJ_L5RDOOR, { i, j });
		}
	}
}

void AddSlainHero()
{
	const Point rndObjLoc = GetRndObjLoc(5);
	AddObject(OBJ_SLAINHERO, rndObjLoc + Displacement { 2, 2 });
}

void InitObjects()
{
	ClrAllObjects();
	NaKrulTomeSequence = 0;
	if (currlevel == 16) {
		AddDiabObjs();
	} else {
		DiscardRandomValues(1);
		if (currlevel == 9 && !UseMultiplayerQuests())
			AddSlainHero();
		if (Quests[Q_MUSHROOM].IsAvailable())
			AddMushPatch();

		if (currlevel == 4 || currlevel == 8 || currlevel == 12)
			AddStoryBooks();
		if (currlevel == 21) {
			AddCryptStoryBook(1);
		} else if (currlevel == 22) {
			AddCryptStoryBook(2);
			AddCryptStoryBook(3);
		} else if (currlevel == 23) {
			AddCryptStoryBook(4);
			AddCryptStoryBook(5);
		}
		if (currlevel == 24) {
			AddNakrulGate();
		}
		if (leveltype == DTYPE_CATHEDRAL) {
			if (Quests[Q_BUTCHER].IsAvailable())
				AddTortures();
			if (Quests[Q_PWATER].IsAvailable())
				AddCandles();
			if (Quests[Q_LTBANNER].IsAvailable())
				AddObject(OBJ_SIGNCHEST, SetPiece.position.megaToWorld() + Displacement { 10, 3 });
			InitRndLocBigObj(10, 15, OBJ_SARC);
			AddL1Objs(0, 0, MAXDUNX, MAXDUNY);
			InitRndBarrels();
		}
		if (leveltype == DTYPE_CATACOMBS) {
			if (Quests[Q_ROCK].IsAvailable())
				InitRndLocObj5x5(1, 1, OBJ_STAND);
			if (Quests[Q_SCHAMB].IsAvailable())
				InitRndLocObj5x5(1, 1, OBJ_BOOK2R);
			AddL2Objs(0, 0, MAXDUNX, MAXDUNY);
			AddL2Torches();
			if (Quests[Q_BLIND].IsAvailable()) {
				_speech_id spId;
				switch (MyPlayer->_pClass) {
				case HeroClass::Warrior:
					spId = TEXT_BLINDING;
					break;
				case HeroClass::Rogue:
					spId = TEXT_RBLINDING;
					break;
				case HeroClass::Sorcerer:
					spId = TEXT_MBLINDING;
					break;
				case HeroClass::Monk:
					spId = TEXT_HBLINDING;
					break;
				case HeroClass::Bard:
					spId = TEXT_RBLINDING;
					break;
				case HeroClass::Barbarian:
					spId = TEXT_BLINDING;
					break;
				default:
					break;
				}
				Quests[Q_BLIND]._qmsg = spId;
				AddBookLever(OBJ_BLINDBOOK, { SetPiece.position, SetPiece.size + 1 }, spId);
				LoadMapObjects("levels\\l2data\\blind2.dun", SetPiece.position.megaToWorld());
			}
			if (Quests[Q_BLOOD].IsAvailable()) {
				_speech_id spId;
				switch (MyPlayer->_pClass) {
				case HeroClass::Warrior:
					spId = TEXT_BLOODY;
					break;
				case HeroClass::Rogue:
					spId = TEXT_RBLOODY;
					break;
				case HeroClass::Sorcerer:
					spId = TEXT_MBLOODY;
					break;
				case HeroClass::Monk:
					spId = TEXT_HBLOODY;
					break;
				case HeroClass::Bard:
					spId = TEXT_RBLOODY;
					break;
				case HeroClass::Barbarian:
					spId = TEXT_BLOODY;
					break;
				default:
					break;
				}
				Quests[Q_BLOOD]._qmsg = spId;
				AddBookLever(OBJ_BLOODBOOK, { SetPiece.position + Displacement { 0, 3 }, { 2, 4 } }, spId);
				AddObject(OBJ_PEDESTAL, SetPiece.position.megaToWorld() + Displacement { 9, 16 });
			}
			InitRndBarrels();
		}
		if (leveltype == DTYPE_CAVES) {
			AddL3Objs(0, 0, MAXDUNX, MAXDUNY);
			InitRndBarrels();
		}
		if (leveltype == DTYPE_HELL) {
			if (Quests[Q_WARLORD].IsAvailable()) {
				_speech_id spId;
				switch (MyPlayer->_pClass) {
				case HeroClass::Warrior:
					spId = TEXT_BLOODWAR;
					break;
				case HeroClass::Rogue:
					spId = TEXT_RBLOODWAR;
					break;
				case HeroClass::Sorcerer:
					spId = TEXT_MBLOODWAR;
					break;
				case HeroClass::Monk:
					spId = TEXT_HBLOODWAR;
					break;
				case HeroClass::Bard:
					spId = TEXT_RBLOODWAR;
					break;
				case HeroClass::Barbarian:
					spId = TEXT_BLOODWAR;
					break;
				default:
					break;
				}
				Quests[Q_WARLORD]._qmsg = spId;
				AddBookLever(OBJ_STEELTOME, SetPiece, spId);
				LoadMapObjects("levels\\l4data\\warlord.dun", SetPiece.position.megaToWorld());
			}
			if (Quests[Q_BETRAYER].IsAvailable() && !UseMultiplayerQuests())
				AddLazStand();
			InitRndBarrels();
			AddL4Goodies();
		}
		if (leveltype == DTYPE_NEST) {
			InitRndBarrels();
		}
		if (leveltype == DTYPE_CRYPT) {
			InitRndLocBigObj(10, 15, OBJ_L5SARC);
			AddCryptObjects(0, 0, MAXDUNX, MAXDUNY);
			InitRndBarrels();
		}
		InitRndLocObj(5, 10, OBJ_CHEST1);
		InitRndLocObj(3, 6, OBJ_CHEST2);
		InitRndLocObj(1, 5, OBJ_CHEST3);
		if (leveltype != DTYPE_HELL)
			AddObjTraps();
		if (IsAnyOf(leveltype, DTYPE_CATACOMBS, DTYPE_CAVES, DTYPE_HELL, DTYPE_NEST))
			AddChestTraps();
	}
}

void SetMapObjects(const uint16_t *dunData, int startx, int starty)
{
	uint16_t filesWidths[65] = {};

	ClrAllObjects();

	WorldTileSize size = GetDunSize(dunData);

	const int layer2Offset = 2 + size.width * size.height;

	// The rest of the layers are at dPiece scale
	size *= static_cast<WorldTileCoord>(2);

	const uint16_t *objectLayer = &dunData[layer2Offset + size.width * size.height * 2];

	for (WorldTileCoord j = 0; j < size.height; j++) {
		for (WorldTileCoord i = 0; i < size.width; i++) {
			auto objectId = static_cast<uint8_t>(Swap16LE(objectLayer[j * size.width + i]));
			if (objectId != 0) {
				const ObjectData &objectData = AllObjects[ObjTypeConv[objectId]];
				filesWidths[objectData.ofindex] = objectData.animWidth;
			}
		}
	}

	LoadLevelObjects(filesWidths);

	for (WorldTileCoord j = 0; j < size.height; j++) {
		for (WorldTileCoord i = 0; i < size.width; i++) {
			auto objectId = static_cast<uint8_t>(Swap16LE(objectLayer[j * size.width + i]));
			if (objectId != 0) {
				AddObject(ObjTypeConv[objectId], { startx + 16 + i, starty + 16 + j });
			}
		}
	}
}

Object *AddObject(_object_id objType, Point objPos)
{
	if (ActiveObjectCount >= MAXOBJECTS)
		return nullptr;

	const int oi = AvailableObjects[0];
	AvailableObjects[0] = AvailableObjects[MAXOBJECTS - 1 - ActiveObjectCount];
	ActiveObjects[ActiveObjectCount] = oi;
	dObject[objPos.x][objPos.y] = oi + 1;
	Object &object = Objects[oi];
	SetupObject(object, objPos, objType);
	switch (object._otype) {
	case OBJ_L1LDOOR:
	case OBJ_L1RDOOR:
	case OBJ_L2LDOOR:
	case OBJ_L2RDOOR:
	case OBJ_L3LDOOR:
	case OBJ_L3RDOOR:
	case OBJ_L5LDOOR:
	case OBJ_L5RDOOR:
		AddDoor(object);
		break;
	case OBJ_BOOK2R:
		object.InitializeBook({ SetPiece.position, WorldTileSize(SetPiece.size.width + 1, SetPiece.size.height + 1) });
		break;
	case OBJ_CHEST1:
	case OBJ_CHEST2:
	case OBJ_CHEST3:
		AddChest(object);
		break;
	case OBJ_TCHEST1:
	case OBJ_TCHEST2:
	case OBJ_TCHEST3:
		AddChest(object);
		object._oTrapFlag = true;
		if (leveltype == DTYPE_CATACOMBS) {
			object._oVar4 = GenerateRnd(2);
		} else {
			object._oVar4 = GenerateRnd(3);
		}
		break;
	case OBJ_SARC:
	case OBJ_L5SARC:
		AddSarcophagus(object);
		break;
	case OBJ_FLAMEHOLE:
		AddFlameTrap(object);
		break;
	case OBJ_FLAMELVR:
		AddFlameLever(object);
		break;
	case OBJ_WATER:
		object._oAnimFrame = 1;
		break;
	case OBJ_TRAPL:
	case OBJ_TRAPR:
		AddTrap(object);
		break;
	case OBJ_BARREL:
	case OBJ_BARRELEX:
	case OBJ_POD:
	case OBJ_PODEX:
	case OBJ_URN:
	case OBJ_URNEX:
		AddBarrel(object);
		break;
	case OBJ_SHRINEL:
	case OBJ_SHRINER:
		AddShrine(object);
		break;
	case OBJ_BOOKCASEL:
	case OBJ_BOOKCASER:
		AddBookcase(object);
		break;
	case OBJ_SKELBOOK:
	case OBJ_BOOKSTAND:
	case OBJ_BLOODFTN:
	case OBJ_GOATSHRINE:
	case OBJ_CAULDRON:
	case OBJ_TEARFTN:
	case OBJ_SLAINHERO:
		object._oRndSeed = AdvanceRndSeed();
		break;
	case OBJ_DECAP:
		AddDecapitatedBody(object);
		break;
	case OBJ_PURIFYINGFTN:
	case OBJ_MURKYFTN:
		AddLargeFountain(object);
		break;
	case OBJ_ARMORSTAND:
	case OBJ_WARARMOR:
		AddArmorStand(object);
		break;
	case OBJ_BOOK2L:
		AddBookOfVileness(object);
		break;
	case OBJ_MCIRCLE1:
	case OBJ_MCIRCLE2:
		AddMagicCircle(object);
		break;
	case OBJ_STORYBOOK:
	case OBJ_L5BOOKS:
		AddStoryBook(object);
		break;
	case OBJ_BCROSS:
	case OBJ_TBCROSS:
		object._oRndSeed = AdvanceRndSeed();
		break;
	case OBJ_PEDESTAL:
		AddPedestalOfBlood(object);
		break;
	case OBJ_WARWEAP:
	case OBJ_WEAPONRACK:
		AddWeaponRack(object);
		break;
	case OBJ_TNUDEM2:
		AddTorturedBody(object);
		break;
	default:
		break;
	}

	AddObjectLight(object);

	ActiveObjectCount++;
	return &object;
}

bool UpdateTrapState(Object &trap)
{
	if (trap._oVar4 != 0)
		return false;

	Object &trigger = ObjectAtPosition({ trap._oVar1, trap._oVar2 });
	switch (trigger._otype) {
	case OBJ_L1LDOOR:
	case OBJ_L1RDOOR:
	case OBJ_L2LDOOR:
	case OBJ_L2RDOOR:
	case OBJ_L3LDOOR:
	case OBJ_L3RDOOR:
	case OBJ_L5LDOOR:
	case OBJ_L5RDOOR:
		if (trigger._oVar4 == DOOR_CLOSED && trigger._oTrapFlag)
			return false;
		break;
	case OBJ_LEVER:
	case OBJ_CHEST1:
	case OBJ_CHEST2:
	case OBJ_CHEST3:
	case OBJ_SWITCHSKL:
	case OBJ_SARC:
	case OBJ_L5LEVER:
	case OBJ_L5SARC:
		if (trigger.canInteractWith() && trigger._oTrapFlag)
			return false;
		break;
	default:
		return false;
	}

	trap._oVar4 = 1;
	trigger._oTrapFlag = false;
	return true;
}

void OperateTrap(Object &trap)
{
	if (!UpdateTrapState(trap))
		return;

	// default to firing at the trigger object
	const Point triggerPosition = { trap._oVar1, trap._oVar2 };
	Point target = triggerPosition;

	auto searchArea = PointsInRectangle(Rectangle { target, 1 });
	// look for a player near the trigger (using a reverse search to match vanilla behaviour)
	auto foundPosition = std::find_if(searchArea.crbegin(), searchArea.crend(), [](Point testPosition) { return InDungeonBounds(testPosition) && dPlayer[testPosition.x][testPosition.y] != 0; });
	if (foundPosition != searchArea.crend()) {
		// if a player is standing near the trigger then target them instead
		target = *foundPosition;
	}

	const Direction dir = GetDirection(trap.position, target);
	AddMissile(trap.position, target, dir, static_cast<MissileID>(trap._oVar3), TARGET_PLAYERS, -1, 0, 0);
	PlaySfxLoc(SfxID::TriggerTrap, triggerPosition);
}

void ProcessObjects()
{
	for (int i = 0; i < ActiveObjectCount; ++i) {
		Object &object = Objects[ActiveObjects[i]];
		switch (object._otype) {
		case OBJ_L1LIGHT:
		case OBJ_SKFIRE:
		case OBJ_CANDLE1:
		case OBJ_CANDLE2:
		case OBJ_BOOKCANDLE:
			UpdateObjectLight(object, 5);
			break;
		case OBJ_STORYCANDLE:
		case OBJ_L5CANDLE:
			UpdateObjectLight(object, 3);
			break;
		case OBJ_CRUX1:
		case OBJ_CRUX2:
		case OBJ_CRUX3:
		case OBJ_BARREL:
		case OBJ_BARRELEX:
		case OBJ_POD:
		case OBJ_PODEX:
		case OBJ_URN:
		case OBJ_URNEX:
		case OBJ_SHRINEL:
		case OBJ_SHRINER:
			ObjectStopAnim(object);
			break;
		case OBJ_L1LDOOR:
		case OBJ_L1RDOOR:
		case OBJ_L2LDOOR:
		case OBJ_L2RDOOR:
		case OBJ_L3LDOOR:
		case OBJ_L3RDOOR:
		case OBJ_L5LDOOR:
		case OBJ_L5RDOOR:
			UpdateDoor(object);
			break;
		case OBJ_TORCHL:
		case OBJ_TORCHR:
		case OBJ_TORCHL2:
		case OBJ_TORCHR2:
			UpdateObjectLight(object, 8);
			break;
		case OBJ_SARC:
		case OBJ_L5SARC:
			UpdateSarcophagus(object);
			break;
		case OBJ_FLAMEHOLE:
			UpdateFlameTrap(object);
			break;
		case OBJ_TRAPL:
		case OBJ_TRAPR:
			OperateTrap(object);
			break;
		case OBJ_MCIRCLE1:
		case OBJ_MCIRCLE2:
			UpdateCircle(object);
			break;
		case OBJ_BCROSS:
		case OBJ_TBCROSS:
			UpdateObjectLight(object, 5);
			UpdateBurningCrossDamage(object);
			break;
		default:
			break;
		}
		if (!object._oAnimFlag)
			continue;

		object._oAnimCnt++;

		if (object._oAnimCnt < object._oAnimDelay)
			continue;

		object._oAnimCnt = 0;
		object._oAnimFrame++;
		if (object._oAnimFrame > object._oAnimLen)
			object._oAnimFrame = 1;
	}

	for (int i = 0; i < ActiveObjectCount;) {
		const int oi = ActiveObjects[i];
		if (Objects[oi]._oDelFlag) {
			DeleteObject(oi, i);
		} else {
			i++;
		}
	}
}

void RedoPlayerVision()
{
	for (const Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel()) {
			ChangeVisionXY(player.getId(), player.position.tile);
		}
	}
}

void MonstCheckDoors(const Monster &monster)
{
	for (const Direction dir : { Direction::NorthEast, Direction::SouthWest, Direction::North, Direction::East, Direction::South, Direction::West, Direction::NorthWest, Direction::SouthEast }) {
		Object *object = FindObjectAtPosition(monster.position.tile + dir);
		if (object == nullptr)
			continue;

		Object &door = *object;
		// Doors use _oVar4 to track open/closed state, non-zero values indicate an open door
		if (!door.isDoor() || door._oVar4 != DOOR_CLOSED)
			continue;

		OperateDoor(door, true);
	}
}

void ObjChangeMap(int x1, int y1, int x2, int y2)
{
	for (int j = y1; j <= y2; j++) {
		for (int i = x1; i <= x2; i++) {
			ObjSetMini({ i, j }, pdungeon[i][j]);
			dungeon[i][j] = pdungeon[i][j];
		}
	}

	const WorldTilePosition mega1 { static_cast<WorldTileCoord>(x1), static_cast<WorldTileCoord>(y1) };
	const WorldTilePosition mega2 { static_cast<WorldTileCoord>(x2), static_cast<WorldTileCoord>(y2) };
	const WorldTilePosition world1 = mega1.megaToWorld();
	const WorldTilePosition world2 = mega2.megaToWorld() + Displacement { 1, 1 };
	if (leveltype == DTYPE_CATHEDRAL) {
		ObjL1Special(world1.x, world1.y, world2.x, world2.y);
		AddL1Objs(world1.x, world1.y, world2.x, world2.y);
	}
	if (leveltype == DTYPE_CATACOMBS) {
		ObjL2Special(world1.x, world1.y, world2.x, world2.y);
		AddL2Objs(world1.x, world1.y, world2.x, world2.y);
	}
	if (leveltype == DTYPE_CAVES) {
		AddL3Objs(world1.x, world1.y, world2.x, world2.y);
	}
	if (leveltype == DTYPE_CRYPT) {
		AddCryptObjects(world1.x, world1.y, world2.x, world2.y);
	}
	ResyncDoors(world1, world2, true);
}

void ObjChangeMapResync(int x1, int y1, int x2, int y2)
{
	for (int j = y1; j <= y2; j++) {
		for (int i = x1; i <= x2; i++) {
			ObjSetMini({ i, j }, pdungeon[i][j]);
			dungeon[i][j] = pdungeon[i][j];
		}
	}

	const WorldTilePosition mega1 { static_cast<WorldTileCoord>(x1), static_cast<WorldTileCoord>(y1) };
	const WorldTilePosition mega2 { static_cast<WorldTileCoord>(x2), static_cast<WorldTileCoord>(y2) };
	const WorldTilePosition world1 = mega1.megaToWorld();
	const WorldTilePosition world2 = mega2.megaToWorld() + Displacement { 1, 1 };
	if (leveltype == DTYPE_CATHEDRAL) {
		ObjL1Special(world1.x, world1.y, world2.x, world2.y);
	}
	if (leveltype == DTYPE_CATACOMBS) {
		ObjL2Special(world1.x, world1.y, world2.x, world2.y);
	}
	ResyncDoors(world1, world2, false);
}

_item_indexes ItemMiscIdIdx(item_misc_id imiscid)
{
	std::underlying_type_t<_item_indexes> i = IDI_GOLD;
	while (AllItemsList[i].dropRate == 0 || AllItemsList[i].iMiscId != imiscid) {
		i++;
	}

	return static_cast<_item_indexes>(i);
}

void OperateObject(Player &player, Object &object)
{
	const bool sendmsg = &player == MyPlayer;

	switch (object._otype) {
	case OBJ_L1LDOOR:
	case OBJ_L1RDOOR:
	case OBJ_L2LDOOR:
	case OBJ_L2RDOOR:
	case OBJ_L3LDOOR:
	case OBJ_L3RDOOR:
	case OBJ_L5LDOOR:
	case OBJ_L5RDOOR:
		if (sendmsg)
			OperateDoor(object, sendmsg);
		break;
	case OBJ_LEVER:
	case OBJ_L5LEVER:
	case OBJ_SWITCHSKL:
		OperateLever(object, sendmsg);
		break;
	case OBJ_BOOK2L:
		if (sendmsg)
			OperateBook(player, object, sendmsg);
		break;
	case OBJ_BOOK2R:
		OperateChamberOfBoneBook(object, sendmsg);
		break;
	case OBJ_CHEST1:
	case OBJ_CHEST2:
	case OBJ_CHEST3:
	case OBJ_TCHEST1:
	case OBJ_TCHEST2:
	case OBJ_TCHEST3:
		OperateChest(player, object, sendmsg);
		break;
	case OBJ_SARC:
	case OBJ_L5SARC:
		OperateSarcophagus(object, sendmsg, sendmsg);
		break;
	case OBJ_FLAMELVR:
		OperateTrapLever(object);
		break;
	case OBJ_BLINDBOOK:
	case OBJ_BLOODBOOK:
	case OBJ_STEELTOME:
		if (sendmsg)
			OperateBookLever(object, sendmsg);
		break;
	case OBJ_SHRINEL:
	case OBJ_SHRINER:
		OperateShrine(player, object, SfxID::OperateShrine);
		break;
	case OBJ_SKELBOOK:
	case OBJ_BOOKSTAND:
		OperateBookStand(object, sendmsg, sendmsg);
		break;
	case OBJ_BOOKCASEL:
	case OBJ_BOOKCASER:
		OperateBookcase(object, sendmsg, sendmsg);
		break;
	case OBJ_DECAP:
		OperateDecapitatedBody(object, sendmsg, sendmsg);
		break;
	case OBJ_ARMORSTAND:
	case OBJ_WARARMOR:
		OperateArmorStand(object, sendmsg, sendmsg);
		break;
	case OBJ_GOATSHRINE:
		OperateGoatShrine(player, object, SfxID::OperateGoatShrine);
		break;
	case OBJ_CAULDRON:
		OperateCauldron(player, object, SfxID::OperateCaldron);
		break;
	case OBJ_BLOODFTN:
	case OBJ_PURIFYINGFTN:
	case OBJ_MURKYFTN:
	case OBJ_TEARFTN:
		OperateFountains(player, object);
		break;
	case OBJ_STORYBOOK:
	case OBJ_L5BOOKS:
		if (sendmsg)
			OperateStoryBook(object);
		break;
	case OBJ_PEDESTAL:
		if (sendmsg)
			OperatePedestal(player, object, sendmsg);
		break;
	case OBJ_WARWEAP:
	case OBJ_WEAPONRACK:
		OperateWeaponRack(object, sendmsg, sendmsg);
		break;
	case OBJ_MUSHPATCH:
		OperateMushroomPatch(player, object);
		break;
	case OBJ_LAZSTAND:
		if (sendmsg)
			OperateLazStand(object);
		break;
	case OBJ_SLAINHERO:
		OperateSlainHero(player, object, sendmsg);
		break;
	case OBJ_SIGNCHEST:
		OperateInnSignChest(player, object, sendmsg);
		break;
	default:
		break;
	}
}

void DeltaSyncOpObject(Object &object)
{
	switch (object._otype) {
	case OBJ_L1LDOOR:
	case OBJ_L1RDOOR:
	case OBJ_L2LDOOR:
	case OBJ_L2RDOOR:
	case OBJ_L3LDOOR:
	case OBJ_L3RDOOR:
	case OBJ_L5LDOOR:
	case OBJ_L5RDOOR:
		OpenDoor(object);
		break;
	case OBJ_LEVER:
	case OBJ_L5LEVER:
	case OBJ_SWITCHSKL:
	case OBJ_BOOK2L:
		UpdateLeverState(object);
		break;
	case OBJ_CHEST1:
	case OBJ_CHEST2:
	case OBJ_CHEST3:
	case OBJ_TCHEST1:
	case OBJ_TCHEST2:
	case OBJ_TCHEST3:
	case OBJ_SKELBOOK:
	case OBJ_BOOKSTAND:
		UpdateState(object, object._oAnimFrame + 2);
		break;
	case OBJ_SARC:
	case OBJ_L5SARC:
	case OBJ_GOATSHRINE:
	case OBJ_SHRINEL:
	case OBJ_SHRINER:
		UpdateState(object, object._oAnimLen);
		break;
	case OBJ_BLINDBOOK:
	case OBJ_BLOODBOOK:
	case OBJ_STEELTOME:
	case OBJ_BOOK2R:
		object._oAnimFrame = object._oVar6;
		SyncQSTLever(object);
		break;
	case OBJ_BOOKCASEL:
	case OBJ_BOOKCASER:
		UpdateState(object, object._oAnimFrame - 2);
		break;
	case OBJ_DECAP:
	case OBJ_MURKYFTN:
	case OBJ_TEARFTN:
	case OBJ_SLAINHERO:
		UpdateState(object, object._oAnimFrame);
		break;
	case OBJ_ARMORSTAND:
	case OBJ_WARARMOR:
	case OBJ_WARWEAP:
	case OBJ_WEAPONRACK:
	case OBJ_LAZSTAND:
		UpdateState(object, object._oAnimFrame + 1);
		break;
	case OBJ_CAULDRON:
		UpdateState(object, 3);
		break;
	case OBJ_STORYBOOK:
	case OBJ_L5BOOKS:
		object._oAnimFrame = object._oVar4;
		break;
	case OBJ_MUSHPATCH:
		if (Quests[Q_MUSHROOM]._qvar1 >= QS_MUSHSPAWNED) {
			UpdateState(object, object._oAnimFrame + 1);
		}
		break;
	case OBJ_SIGNCHEST:
		if (Quests[Q_LTBANNER]._qvar1 >= 2) {
			UpdateState(object, object._oAnimFrame + 2);
		}
		break;
	case OBJ_PEDESTAL:
		UpdatePedestalState(object);
		break;
	default:
		break;
	}
}

void DeltaSyncCloseObj(Object &object)
{
	// Object was closed.
	// That means it was opened once, so all traps have been activated.
	object._oTrapFlag = false;
}

void SyncOpObject(Player &player, int cmd, Object &object)
{
	const bool sendmsg = &player == MyPlayer;

	switch (object._otype) {
	case OBJ_L1LDOOR:
	case OBJ_L1RDOOR:
	case OBJ_L2LDOOR:
	case OBJ_L2RDOOR:
	case OBJ_L3LDOOR:
	case OBJ_L3RDOOR:
	case OBJ_L5LDOOR:
	case OBJ_L5RDOOR:
		if (sendmsg)
			break;
		if (cmd == CMD_CLOSEDOOR && object._oVar4 == DOOR_CLOSED)
			break;
		if (cmd == CMD_OPENDOOR && object._oVar4 == DOOR_OPEN)
			break;
		OperateDoor(object, false);
		break;
	case OBJ_LEVER:
	case OBJ_L5LEVER:
	case OBJ_SWITCHSKL:
		OperateLever(object, sendmsg);
		break;
	case OBJ_BOOK2L:
		if (!sendmsg)
			OperateBook(player, object, sendmsg);
		break;
	case OBJ_CHEST1:
	case OBJ_CHEST2:
	case OBJ_CHEST3:
	case OBJ_TCHEST1:
	case OBJ_TCHEST2:
	case OBJ_TCHEST3:
		OperateChest(player, object, false);
		break;
	case OBJ_SARC:
	case OBJ_L5SARC:
		OperateSarcophagus(object, sendmsg, false);
		break;
	case OBJ_BLINDBOOK:
	case OBJ_BLOODBOOK:
	case OBJ_STEELTOME:
		if (sendmsg)
			break;
		object._oAnimFrame = object._oVar6;
		SyncQSTLever(object);
		break;
	case OBJ_SHRINEL:
	case OBJ_SHRINER:
		OperateShrine(player, object, SfxID::OperateShrine);
		break;
	case OBJ_SKELBOOK:
	case OBJ_BOOKSTAND:
		OperateBookStand(object, sendmsg, false);
		break;
	case OBJ_BOOKCASEL:
	case OBJ_BOOKCASER:
		OperateBookcase(object, sendmsg, false);
		break;
	case OBJ_DECAP:
		OperateDecapitatedBody(object, sendmsg, false);
		break;
	case OBJ_ARMORSTAND:
	case OBJ_WARARMOR:
		OperateArmorStand(object, sendmsg, false);
		break;
	case OBJ_GOATSHRINE:
		OperateGoatShrine(player, object, SfxID::OperateGoatShrine);
		break;
	case OBJ_LAZSTAND:
		if (!sendmsg)
			UpdateState(object, object._oAnimFrame + 1);
		break;
	case OBJ_CAULDRON:
		OperateCauldron(player, object, SfxID::OperateCaldron);
		break;
	case OBJ_MURKYFTN:
	case OBJ_TEARFTN:
		OperateFountains(player, object);
		break;
	case OBJ_STORYBOOK:
	case OBJ_L5BOOKS:
		if (sendmsg)
			OperateStoryBook(object);
		break;
	case OBJ_PEDESTAL:
		if (!sendmsg)
			OperatePedestal(player, object, sendmsg);
		break;
	case OBJ_WARWEAP:
	case OBJ_WEAPONRACK:
		OperateWeaponRack(object, sendmsg, false);
		break;
	case OBJ_MUSHPATCH:
		OperateMushroomPatch(player, object);
		break;
	case OBJ_SLAINHERO:
		OperateSlainHero(player, object, sendmsg);
		break;
	case OBJ_SIGNCHEST:
		OperateInnSignChest(player, object, sendmsg);
		break;
	default:
		break;
	}
}

void BreakObjectMissile(const Player *player, Object &object)
{
	if (object.IsCrux())
		BreakCrux(object, true);
}
void BreakObject(const Player &player, Object &object)
{
	if (object.IsBarrel()) {
		BreakBarrel(player, object, false, true);
	} else if (object.IsCrux()) {
		BreakCrux(object, true);
	}
}

void DeltaSyncBreakObj(Object &object)
{
	if (!object.IsBreakable() || !object.canInteractWith())
		return;

	object._oMissFlag = true;
	object._oBreak = -1;
	object.selectionRegion = SelectionRegion::None;
	object._oPreFlag = true;
	object._oAnimFlag = false;
	object._oAnimFrame = object._oAnimLen;

	if (object.IsBarrel()) {
		object._oSolidFlag = false;
	} else if (object.IsCrux() && AreAllCruxesOfTypeBroken(object._oVar8)) {
		ObjChangeMap(object._oVar1, object._oVar2, object._oVar3, object._oVar4);
	}
}

void SyncBreakObj(const Player &player, Object &object)
{
	if (object.IsBarrel()) {
		BreakBarrel(player, object, true, false);
	} else if (object.IsCrux()) {
		BreakCrux(object, false);
	}
}

void SyncObjectAnim(Object &object)
{
	object_graphic_id index = AllObjects[object._otype].ofindex;

	if (!HeadlessMode) {
		const auto &found = c_find(ObjFileList, index);
		if (found == std::end(ObjFileList)) {
			LogCritical("Unable to find object_graphic_id {} in list of objects to load, level generation error.", static_cast<int>(index));
			return;
		}

		const size_t i = std::distance(std::begin(ObjFileList), found);

		if (pObjCels[i]) {
			object._oAnimData.emplace(*pObjCels[i]);
		} else {
			object._oAnimData = std::nullopt;
		}
	}

	switch (object._otype) {
	case OBJ_L1LDOOR:
	case OBJ_L1RDOOR:
	case OBJ_L2LDOOR:
	case OBJ_L2RDOOR:
	case OBJ_L3LDOOR:
	case OBJ_L3RDOOR:
	case OBJ_L5LDOOR:
	case OBJ_L5RDOOR:
		SyncDoor(object);
		break;
	case OBJ_CRUX1:
	case OBJ_CRUX2:
	case OBJ_CRUX3:
		SyncCrux(object);
		break;
	case OBJ_LEVER:
	case OBJ_L5LEVER:
	case OBJ_BOOK2L:
	case OBJ_SWITCHSKL:
		SyncLever(object);
		break;
	case OBJ_BOOK2R:
	case OBJ_BLINDBOOK:
	case OBJ_STEELTOME:
		SyncQSTLever(object);
		break;
	case OBJ_PEDESTAL:
		SyncPedestal(object);
		break;
	default:
		break;
	}
}

StringOrView Object::name() const
{
	switch (_otype) {
	case OBJ_CRUX1:
	case OBJ_CRUX2:
	case OBJ_CRUX3:
		return _("Crucified Skeleton");
	case OBJ_LEVER:
	case OBJ_L5LEVER:
	case OBJ_FLAMELVR:
		return _("Lever");
	case OBJ_L1LDOOR:
	case OBJ_L1RDOOR:
	case OBJ_L2LDOOR:
	case OBJ_L2RDOOR:
	case OBJ_L3LDOOR:
	case OBJ_L3RDOOR:
	case OBJ_L5LDOOR:
	case OBJ_L5RDOOR:
		if (_oVar4 == DOOR_OPEN)
			return _("Open Door");
		if (_oVar4 == DOOR_CLOSED)
			return _("Closed Door");
		if (_oVar4 == DOOR_BLOCKED)
			return _("Blocked Door");
		break;
	case OBJ_BOOK2L:
		if (setlevel) {
			if (setlvlnum == SL_BONECHAMB) {
				return _("Ancient Tome");
			} else if (setlvlnum == SL_VILEBETRAYER) {
				return _("Book of Vileness");
			}
		}
		break;
	case OBJ_SWITCHSKL:
		return _("Skull Lever");
	case OBJ_BOOK2R:
		return _("Mythical Book");
	case OBJ_CHEST1:
	case OBJ_TCHEST1:
		return _("Small Chest");
	case OBJ_CHEST2:
	case OBJ_TCHEST2:
		return _("Chest");
	case OBJ_CHEST3:
	case OBJ_TCHEST3:
	case OBJ_SIGNCHEST:
		return _("Large Chest");
	case OBJ_SARC:
	case OBJ_L5SARC:
		return _("Sarcophagus");
	case OBJ_BOOKSHELF:
		return _("Bookshelf");
	case OBJ_BOOKCASEL:
	case OBJ_BOOKCASER:
		return _("Bookcase");
	case OBJ_BARREL:
	case OBJ_BARRELEX:
		return _("Barrel");
	case OBJ_POD:
	case OBJ_PODEX:
		return _("Pod");
	case OBJ_URN:
	case OBJ_URNEX:
		return _("Urn");
	case OBJ_SHRINEL:
	case OBJ_SHRINER:
		return fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} will be a name from the Shrine block above */ "{:s} Shrine")), _(ShrineNames[_oVar1]));
	case OBJ_SKELBOOK:
		return _("Skeleton Tome");
	case OBJ_BOOKSTAND:
		return _("Library Book");
	case OBJ_BLOODFTN:
		return _("Blood Fountain");
	case OBJ_DECAP:
		return _("Decapitated Body");
	case OBJ_BLINDBOOK:
		return _("Book of the Blind");
	case OBJ_BLOODBOOK:
		return _("Book of Blood");
	case OBJ_PURIFYINGFTN:
		return _("Purifying Spring");
	case OBJ_ARMORSTAND:
	case OBJ_WARARMOR:
		return _("Armor");
	case OBJ_WARWEAP:
		return _("Weapon Rack");
	case OBJ_GOATSHRINE:
		return _("Goat Shrine");
	case OBJ_CAULDRON:
		return _("Cauldron");
	case OBJ_MURKYFTN:
		return _("Murky Pool");
	case OBJ_TEARFTN:
		return _("Fountain of Tears");
	case OBJ_STEELTOME:
		return _("Steel Tome");
	case OBJ_PEDESTAL:
		return _("Pedestal of Blood");
	case OBJ_STORYBOOK:
	case OBJ_L5BOOKS:
		return _(StoryBookName[_oVar3]);
	case OBJ_WEAPONRACK:
		return _("Weapon Rack");
	case OBJ_MUSHPATCH:
		return _("Mushroom Patch");
	case OBJ_LAZSTAND:
		return _("Vile Stand");
	case OBJ_SLAINHERO:
		return _("Slain Hero");
	default:
		break;
	}
	return std::string_view();
}

void GetObjectStr(const Object &object)
{
	InfoString = object.name();
	const ClassAttributes &classAttributes = GetClassAttributes(MyPlayer->_pClass);
	if (HasAnyOf(classAttributes.classFlags, PlayerClassFlag::TrapSense)) {
		if (object._oTrapFlag) {
			InfoString = fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} will either be a chest or a door */ "Trapped {:s}")), InfoString.str());
			InfoColor = UiFlags::ColorRed;
		}
	}
	if (object.IsDisabled()) {
		InfoString = fmt::format(fmt::runtime(_(/* TRANSLATORS: If user enabled diablo.ini setting "Disable Crippling Shrines" is set to 1; also used for Na-Kruls lever */ "{:s} (disabled)")), InfoString.str());
		InfoColor = UiFlags::ColorRed;
	}
}

void SyncNakrulRoom()
{
	dPiece[UberRow][UberCol] = 297;
	dPiece[UberRow][UberCol - 1] = 300;
	dPiece[UberRow][UberCol - 2] = 299;
	dPiece[UberRow][UberCol + 1] = 298;
}

} // namespace devilution
