/**
 * @file monstdat.cpp
 *
 * Implementation of all monster data.
 */
#include "monstdat.h"

#include <cstdint>

#include <ankerl/unordered_dense.h>

#include "cursor.h"
#include "data/file.hpp"
#include "data/record_reader.hpp"
#include "items.h"
#include "monster.h"
#include "textdat.h"
#include "utils/language.h"

namespace devilution {

namespace {

// Returns a `treasure` value for the given item.
constexpr uint16_t Uniq(_unique_items item)
{
	return static_cast<uint16_t>(T_UNIQ) + item;
}

std::vector<std::string> MonsterSpritePaths;

} // namespace

const char *MonsterData::spritePath() const
{
	return MonsterSpritePaths[static_cast<size_t>(spriteId)].c_str();
}

/** Contains the data related to each monster ID. */
std::vector<MonsterData> MonstersData;

/** Contains the data related to each unique monster ID. */
std::vector<UniqueMonsterData> UniqueMonstersData;

/** Contains the additional unique monster data from mods. */
std::vector<UniqueMonsterData> AdditionalUniqueMonstersData;

/**
 * Map between .DUN file value and monster type enum
 */
const _monster_id MonstConvTbl[] = {
	MT_NZOMBIE,
	MT_BZOMBIE,
	MT_GZOMBIE,
	MT_YZOMBIE,
	MT_RFALLSP,
	MT_DFALLSP,
	MT_YFALLSP,
	MT_BFALLSP,
	MT_WSKELAX,
	MT_TSKELAX,
	MT_RSKELAX,
	MT_XSKELAX,
	MT_RFALLSD,
	MT_DFALLSD,
	MT_YFALLSD,
	MT_BFALLSD,
	MT_NSCAV,
	MT_BSCAV,
	MT_WSCAV,
	MT_YSCAV,
	MT_WSKELBW,
	MT_TSKELBW,
	MT_RSKELBW,
	MT_XSKELBW,
	MT_WSKELSD,
	MT_TSKELSD,
	MT_RSKELSD,
	MT_XSKELSD,
	MT_SNEAK,
	MT_STALKER,
	MT_UNSEEN,
	MT_ILLWEAV,
	MT_NGOATMC,
	MT_BGOATMC,
	MT_RGOATMC,
	MT_GGOATMC,
	MT_FIEND,
	MT_GLOOM,
	MT_BLINK,
	MT_FAMILIAR,
	MT_NGOATBW,
	MT_BGOATBW,
	MT_RGOATBW,
	MT_GGOATBW,
	MT_NACID,
	MT_RACID,
	MT_BACID,
	MT_XACID,
	MT_SKING,
	MT_FAT,
	MT_MUDMAN,
	MT_TOAD,
	MT_FLAYED,
	MT_WYRM,
	MT_CAVSLUG,
	MT_DEVOUR,
	MT_DVLWYRM,
	MT_NMAGMA,
	MT_YMAGMA,
	MT_BMAGMA,
	MT_WMAGMA,
	MT_HORNED,
	MT_MUDRUN,
	MT_FROSTC,
	MT_OBLORD,
	MT_BONEDMN,
	MT_REDDTH,
	MT_LTCHDMN,
	MT_UDEDBLRG,
	MT_INVALID,
	MT_INVALID,
	MT_INVALID,
	MT_INVALID,
	MT_INCIN,
	MT_FLAMLRD,
	MT_DOOMFIRE,
	MT_HELLBURN,
	MT_INVALID,
	MT_INVALID,
	MT_INVALID,
	MT_INVALID,
	MT_RSTORM,
	MT_STORM,
	MT_STORML,
	MT_MAEL,
	MT_WINGED,
	MT_GARGOYLE,
	MT_BLOODCLW,
	MT_DEATHW,
	MT_MEGA,
	MT_GUARD,
	MT_VTEXLRD,
	MT_BALROG,
	MT_NSNAKE,
	MT_RSNAKE,
	MT_GSNAKE,
	MT_BSNAKE,
	MT_NBLACK,
	MT_RTBLACK,
	MT_BTBLACK,
	MT_RBLACK,
	MT_UNRAV,
	MT_HOLOWONE,
	MT_PAINMSTR,
	MT_REALWEAV,
	MT_SUCCUBUS,
	MT_SNOWWICH,
	MT_HLSPWN,
	MT_SOLBRNR,
	MT_COUNSLR,
	MT_MAGISTR,
	MT_CABALIST,
	MT_ADVOCATE,
	MT_INVALID,
	MT_DIABLO,
	MT_INVALID,
	MT_GOLEM,
	MT_INVALID,
	MT_INVALID,
	MT_INVALID, // Monster from blood1.dun and blood2.dun
	MT_INVALID,
	MT_INVALID,
	MT_INVALID,
	MT_INVALID, // Snotspill from banner2.dun
	MT_INVALID,
	MT_INVALID,
	MT_BIGFALL,
	MT_DARKMAGE,
	MT_HELLBOAR,
	MT_STINGER,
	MT_PSYCHORB,
	MT_ARACHNON,
	MT_FELLTWIN,
	MT_HORKSPWN,
	MT_VENMTAIL,
	MT_NECRMORB,
	MT_SPIDLORD,
	MT_LASHWORM,
	MT_TORCHANT,
	MT_HORKDMN,
	MT_DEFILER,
	MT_GRAVEDIG,
	MT_TOMBRAT,
	MT_FIREBAT,
	MT_SKLWING,
	MT_LICH,
	MT_CRYPTDMN,
	MT_HELLBAT,
	MT_BONEDEMN,
	MT_LICH,
	MT_BICLOPS,
	MT_FLESTHNG,
	MT_REAPER,
	MT_NAKRUL,
	MT_CLEAVER,
	MT_INVILORD,
	MT_LRDSAYTR,
};

tl::expected<_monster_id, std::string> ParseMonsterId(std::string_view value)
{
	if (value == "MT_NZOMBIE") return MT_NZOMBIE;
	if (value == "MT_BZOMBIE") return MT_BZOMBIE;
	if (value == "MT_GZOMBIE") return MT_GZOMBIE;
	if (value == "MT_YZOMBIE") return MT_YZOMBIE;
	if (value == "MT_RFALLSP") return MT_RFALLSP;
	if (value == "MT_DFALLSP") return MT_DFALLSP;
	if (value == "MT_YFALLSP") return MT_YFALLSP;
	if (value == "MT_BFALLSP") return MT_BFALLSP;
	if (value == "MT_WSKELAX") return MT_WSKELAX;
	if (value == "MT_TSKELAX") return MT_TSKELAX;
	if (value == "MT_RSKELAX") return MT_RSKELAX;
	if (value == "MT_XSKELAX") return MT_XSKELAX;
	if (value == "MT_RFALLSD") return MT_RFALLSD;
	if (value == "MT_DFALLSD") return MT_DFALLSD;
	if (value == "MT_YFALLSD") return MT_YFALLSD;
	if (value == "MT_BFALLSD") return MT_BFALLSD;
	if (value == "MT_NSCAV") return MT_NSCAV;
	if (value == "MT_BSCAV") return MT_BSCAV;
	if (value == "MT_WSCAV") return MT_WSCAV;
	if (value == "MT_YSCAV") return MT_YSCAV;
	if (value == "MT_WSKELBW") return MT_WSKELBW;
	if (value == "MT_TSKELBW") return MT_TSKELBW;
	if (value == "MT_RSKELBW") return MT_RSKELBW;
	if (value == "MT_XSKELBW") return MT_XSKELBW;
	if (value == "MT_WSKELSD") return MT_WSKELSD;
	if (value == "MT_TSKELSD") return MT_TSKELSD;
	if (value == "MT_RSKELSD") return MT_RSKELSD;
	if (value == "MT_XSKELSD") return MT_XSKELSD;
	if (value == "MT_SNEAK") return MT_SNEAK;
	if (value == "MT_STALKER") return MT_STALKER;
	if (value == "MT_UNSEEN") return MT_UNSEEN;
	if (value == "MT_ILLWEAV") return MT_ILLWEAV;
	if (value == "MT_NGOATMC") return MT_NGOATMC;
	if (value == "MT_BGOATMC") return MT_BGOATMC;
	if (value == "MT_RGOATMC") return MT_RGOATMC;
	if (value == "MT_GGOATMC") return MT_GGOATMC;
	if (value == "MT_FIEND") return MT_FIEND;
	if (value == "MT_GLOOM") return MT_GLOOM;
	if (value == "MT_BLINK") return MT_BLINK;
	if (value == "MT_FAMILIAR") return MT_FAMILIAR;
	if (value == "MT_NGOATBW") return MT_NGOATBW;
	if (value == "MT_BGOATBW") return MT_BGOATBW;
	if (value == "MT_RGOATBW") return MT_RGOATBW;
	if (value == "MT_GGOATBW") return MT_GGOATBW;
	if (value == "MT_NACID") return MT_NACID;
	if (value == "MT_RACID") return MT_RACID;
	if (value == "MT_BACID") return MT_BACID;
	if (value == "MT_XACID") return MT_XACID;
	if (value == "MT_SKING") return MT_SKING;
	if (value == "MT_FAT") return MT_FAT;
	if (value == "MT_MUDMAN") return MT_MUDMAN;
	if (value == "MT_TOAD") return MT_TOAD;
	if (value == "MT_FLAYED") return MT_FLAYED;
	if (value == "MT_WYRM") return MT_WYRM;
	if (value == "MT_CAVSLUG") return MT_CAVSLUG;
	if (value == "MT_DEVOUR") return MT_DEVOUR;
	if (value == "MT_DVLWYRM") return MT_DVLWYRM;
	if (value == "MT_NMAGMA") return MT_NMAGMA;
	if (value == "MT_YMAGMA") return MT_YMAGMA;
	if (value == "MT_BMAGMA") return MT_BMAGMA;
	if (value == "MT_WMAGMA") return MT_WMAGMA;
	if (value == "MT_HORNED") return MT_HORNED;
	if (value == "MT_MUDRUN") return MT_MUDRUN;
	if (value == "MT_FROSTC") return MT_FROSTC;
	if (value == "MT_OBLORD") return MT_OBLORD;
	if (value == "MT_BONEDMN") return MT_BONEDMN;
	if (value == "MT_REDDTH") return MT_REDDTH;
	if (value == "MT_LTCHDMN") return MT_LTCHDMN;
	if (value == "MT_UDEDBLRG") return MT_UDEDBLRG;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INCIN") return MT_INCIN;
	if (value == "MT_FLAMLRD") return MT_FLAMLRD;
	if (value == "MT_DOOMFIRE") return MT_DOOMFIRE;
	if (value == "MT_HELLBURN") return MT_HELLBURN;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_RSTORM") return MT_RSTORM;
	if (value == "MT_STORM") return MT_STORM;
	if (value == "MT_STORML") return MT_STORML;
	if (value == "MT_MAEL") return MT_MAEL;
	if (value == "MT_WINGED") return MT_WINGED;
	if (value == "MT_GARGOYLE") return MT_GARGOYLE;
	if (value == "MT_BLOODCLW") return MT_BLOODCLW;
	if (value == "MT_DEATHW") return MT_DEATHW;
	if (value == "MT_MEGA") return MT_MEGA;
	if (value == "MT_GUARD") return MT_GUARD;
	if (value == "MT_VTEXLRD") return MT_VTEXLRD;
	if (value == "MT_BALROG") return MT_BALROG;
	if (value == "MT_NSNAKE") return MT_NSNAKE;
	if (value == "MT_RSNAKE") return MT_RSNAKE;
	if (value == "MT_GSNAKE") return MT_GSNAKE;
	if (value == "MT_BSNAKE") return MT_BSNAKE;
	if (value == "MT_NBLACK") return MT_NBLACK;
	if (value == "MT_RTBLACK") return MT_RTBLACK;
	if (value == "MT_BTBLACK") return MT_BTBLACK;
	if (value == "MT_RBLACK") return MT_RBLACK;
	if (value == "MT_UNRAV") return MT_UNRAV;
	if (value == "MT_HOLOWONE") return MT_HOLOWONE;
	if (value == "MT_PAINMSTR") return MT_PAINMSTR;
	if (value == "MT_REALWEAV") return MT_REALWEAV;
	if (value == "MT_SUCCUBUS") return MT_SUCCUBUS;
	if (value == "MT_SNOWWICH") return MT_SNOWWICH;
	if (value == "MT_HLSPWN") return MT_HLSPWN;
	if (value == "MT_SOLBRNR") return MT_SOLBRNR;
	if (value == "MT_COUNSLR") return MT_COUNSLR;
	if (value == "MT_MAGISTR") return MT_MAGISTR;
	if (value == "MT_CABALIST") return MT_CABALIST;
	if (value == "MT_ADVOCATE") return MT_ADVOCATE;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_DIABLO") return MT_DIABLO;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_GOLEM") return MT_GOLEM;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_INVALID") return MT_INVALID;
	if (value == "MT_BIGFALL") return MT_BIGFALL;
	if (value == "MT_DARKMAGE") return MT_DARKMAGE;
	if (value == "MT_HELLBOAR") return MT_HELLBOAR;
	if (value == "MT_STINGER") return MT_STINGER;
	if (value == "MT_PSYCHORB") return MT_PSYCHORB;
	if (value == "MT_ARACHNON") return MT_ARACHNON;
	if (value == "MT_FELLTWIN") return MT_FELLTWIN;
	if (value == "MT_HORKSPWN") return MT_HORKSPWN;
	if (value == "MT_VENMTAIL") return MT_VENMTAIL;
	if (value == "MT_NECRMORB") return MT_NECRMORB;
	if (value == "MT_SPIDLORD") return MT_SPIDLORD;
	if (value == "MT_LASHWORM") return MT_LASHWORM;
	if (value == "MT_TORCHANT") return MT_TORCHANT;
	if (value == "MT_HORKDMN") return MT_HORKDMN;
	if (value == "MT_DEFILER") return MT_DEFILER;
	if (value == "MT_GRAVEDIG") return MT_GRAVEDIG;
	if (value == "MT_TOMBRAT") return MT_TOMBRAT;
	if (value == "MT_FIREBAT") return MT_FIREBAT;
	if (value == "MT_SKLWING") return MT_SKLWING;
	if (value == "MT_LICH") return MT_LICH;
	if (value == "MT_CRYPTDMN") return MT_CRYPTDMN;
	if (value == "MT_HELLBAT") return MT_HELLBAT;
	if (value == "MT_BONEDEMN") return MT_BONEDEMN;
	if (value == "MT_LICH") return MT_LICH;
	if (value == "MT_BICLOPS") return MT_BICLOPS;
	if (value == "MT_FLESTHNG") return MT_FLESTHNG;
	if (value == "MT_REAPER") return MT_REAPER;
	if (value == "MT_NAKRUL") return MT_NAKRUL;
	if (value == "MT_CLEAVER") return MT_CLEAVER;
	if (value == "MT_INVILORD") return MT_INVILORD;
	if (value == "MT_LRDSAYTR") return MT_LRDSAYTR;
	return tl::make_unexpected("Unknown enum value");
}

namespace {

tl::expected<MonsterAvailability, std::string> ParseMonsterAvailability(std::string_view value)
{
	if (value == "Always") return MonsterAvailability::Always;
	if (value == "Never") return MonsterAvailability::Never;
	if (value == "Retail") return MonsterAvailability::Retail;
	return tl::make_unexpected("Expected one of: Always, Never, or Retail");
}

} // namespace

tl::expected<MonsterAIID, std::string> ParseAiId(std::string_view value)
{
	if (value == "Zombie") return MonsterAIID::Zombie;
	if (value == "Fat") return MonsterAIID::Fat;
	if (value == "SkeletonMelee") return MonsterAIID::SkeletonMelee;
	if (value == "SkeletonRanged") return MonsterAIID::SkeletonRanged;
	if (value == "Scavenger") return MonsterAIID::Scavenger;
	if (value == "Rhino") return MonsterAIID::Rhino;
	if (value == "GoatMelee") return MonsterAIID::GoatMelee;
	if (value == "GoatRanged") return MonsterAIID::GoatRanged;
	if (value == "Fallen") return MonsterAIID::Fallen;
	if (value == "Magma") return MonsterAIID::Magma;
	if (value == "SkeletonKing") return MonsterAIID::SkeletonKing;
	if (value == "Bat") return MonsterAIID::Bat;
	if (value == "Gargoyle") return MonsterAIID::Gargoyle;
	if (value == "Butcher") return MonsterAIID::Butcher;
	if (value == "Succubus") return MonsterAIID::Succubus;
	if (value == "Sneak") return MonsterAIID::Sneak;
	if (value == "Storm") return MonsterAIID::Storm;
	if (value == "FireMan") return MonsterAIID::FireMan;
	if (value == "Gharbad") return MonsterAIID::Gharbad;
	if (value == "Acid") return MonsterAIID::Acid;
	if (value == "AcidUnique") return MonsterAIID::AcidUnique;
	if (value == "Golem") return MonsterAIID::Golem;
	if (value == "Zhar") return MonsterAIID::Zhar;
	if (value == "Snotspill") return MonsterAIID::Snotspill;
	if (value == "Snake") return MonsterAIID::Snake;
	if (value == "Counselor") return MonsterAIID::Counselor;
	if (value == "Mega") return MonsterAIID::Mega;
	if (value == "Diablo") return MonsterAIID::Diablo;
	if (value == "Lazarus") return MonsterAIID::Lazarus;
	if (value == "LazarusSuccubus") return MonsterAIID::LazarusSuccubus;
	if (value == "Lachdanan") return MonsterAIID::Lachdanan;
	if (value == "Warlord") return MonsterAIID::Warlord;
	if (value == "FireBat") return MonsterAIID::FireBat;
	if (value == "Torchant") return MonsterAIID::Torchant;
	if (value == "HorkDemon") return MonsterAIID::HorkDemon;
	if (value == "Lich") return MonsterAIID::Lich;
	if (value == "ArchLich") return MonsterAIID::ArchLich;
	if (value == "Psychorb") return MonsterAIID::Psychorb;
	if (value == "Necromorb") return MonsterAIID::Necromorb;
	if (value == "BoneDemon") return MonsterAIID::BoneDemon;
	return tl::make_unexpected("Unknown enum value");
}

namespace {

tl::expected<monster_flag, std::string> ParseMonsterFlag(std::string_view value)
{
	if (value == "HIDDEN") return MFLAG_HIDDEN;
	if (value == "LOCK_ANIMATION") return MFLAG_LOCK_ANIMATION;
	if (value == "ALLOW_SPECIAL") return MFLAG_ALLOW_SPECIAL;
	if (value == "TARGETS_MONSTER") return MFLAG_TARGETS_MONSTER;
	if (value == "GOLEM") return MFLAG_GOLEM;
	if (value == "QUEST_COMPLETE") return MFLAG_QUEST_COMPLETE;
	if (value == "KNOCKBACK") return MFLAG_KNOCKBACK;
	if (value == "SEARCH") return MFLAG_SEARCH;
	if (value == "CAN_OPEN_DOOR") return MFLAG_CAN_OPEN_DOOR;
	if (value == "NO_ENEMY") return MFLAG_NO_ENEMY;
	if (value == "BERSERK") return MFLAG_BERSERK;
	if (value == "NOLIFESTEAL") return MFLAG_NOLIFESTEAL;
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<MonsterClass, std::string> ParseMonsterClass(std::string_view value)
{
	if (value == "Undead") return MonsterClass::Undead;
	if (value == "Demon") return MonsterClass::Demon;
	if (value == "Animal") return MonsterClass::Animal;
	return tl::make_unexpected("Unknown enum value");
}

} // namespace

tl::expected<monster_resistance, std::string> ParseMonsterResistance(std::string_view value)
{
	if (value == "RESIST_MAGIC") return RESIST_MAGIC;
	if (value == "RESIST_FIRE") return RESIST_FIRE;
	if (value == "RESIST_LIGHTNING") return RESIST_LIGHTNING;
	if (value == "IMMUNE_MAGIC") return IMMUNE_MAGIC;
	if (value == "IMMUNE_FIRE") return IMMUNE_FIRE;
	if (value == "IMMUNE_LIGHTNING") return IMMUNE_LIGHTNING;
	if (value == "IMMUNE_ACID") return IMMUNE_ACID;
	return tl::make_unexpected("Unknown enum value");
}

namespace {

tl::expected<SelectionRegion, std::string> ParseSelectionRegion(std::string_view value)
{
	if (value.empty()) return SelectionRegion::None;
	if (value == "Bottom") return SelectionRegion::Bottom;
	if (value == "Middle") return SelectionRegion::Middle;
	if (value == "Top") return SelectionRegion::Top;
	return tl::make_unexpected("Unknown enum value");
}

} // namespace

tl::expected<UniqueMonsterPack, std::string> ParseUniqueMonsterPack(std::string_view value)
{
	if (value == "None") return UniqueMonsterPack::None;
	if (value == "Independent") return UniqueMonsterPack::Independent;
	if (value == "Leashed") return UniqueMonsterPack::Leashed;
	return tl::make_unexpected("Unknown enum value");
}

namespace {

void LoadMonstDat()
{
	const std::string_view filename = "txtdata\\monsters\\monstdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	dataFile.skipHeaderOrDie(filename);

	MonstersData.clear();
	MonstersData.reserve(dataFile.numRecords());
	ankerl::unordered_dense::map<std::string, size_t> spritePathToId;
	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		MonsterData &monster = MonstersData.emplace_back();
		reader.advance(); // Skip the first column (monster ID).
		reader.readString("name", monster.name);
		{
			std::string assetsSuffix;
			reader.readString("assetsSuffix", assetsSuffix);
			const auto [it, inserted] = spritePathToId.emplace(assetsSuffix, spritePathToId.size());
			if (inserted)
				MonsterSpritePaths.push_back(it->first);
			monster.spriteId = static_cast<uint16_t>(it->second);
		}
		reader.readString("soundSuffix", monster.soundSuffix);
		reader.readString("trnFile", monster.trnFile);
		reader.read("availability", monster.availability, ParseMonsterAvailability);
		reader.readInt("width", monster.width);
		reader.readInt("image", monster.image);
		reader.readBool("hasSpecial", monster.hasSpecial);
		reader.readBool("hasSpecialSound", monster.hasSpecialSound);
		reader.readIntArray("frames", monster.frames);
		reader.readIntArray("rate", monster.rate);
		reader.readInt("minDunLvl", monster.minDunLvl);
		reader.readInt("maxDunLvl", monster.maxDunLvl);
		reader.readInt("level", monster.level);
		reader.readInt("hitPointsMinimum", monster.hitPointsMinimum);
		reader.readInt("hitPointsMaximum", monster.hitPointsMaximum);
		reader.read("ai", monster.ai, ParseAiId);
		reader.readEnumList("abilityFlags", monster.abilityFlags, ParseMonsterFlag);
		reader.readInt("intelligence", monster.intelligence);
		reader.readInt("toHit", monster.toHit);
		reader.readInt("animFrameNum", monster.animFrameNum);
		reader.readInt("minDamage", monster.minDamage);
		reader.readInt("maxDamage", monster.maxDamage);
		reader.readInt("toHitSpecial", monster.toHitSpecial);
		reader.readInt("animFrameNumSpecial", monster.animFrameNumSpecial);
		reader.readInt("minDamageSpecial", monster.minDamageSpecial);
		reader.readInt("maxDamageSpecial", monster.maxDamageSpecial);
		reader.readInt("armorClass", monster.armorClass);
		reader.read("monsterClass", monster.monsterClass, ParseMonsterClass);
		reader.readEnumList("resistance", monster.resistance, ParseMonsterResistance);
		reader.readEnumList("resistanceHell", monster.resistanceHell, ParseMonsterResistance);
		reader.readEnumList("selectionRegion", monster.selectionRegion, ParseSelectionRegion);

		// treasure
		// TODO: Replace this hack with proper parsing once items have been migrated to data files.
		reader.read("treasure", monster.treasure, [](std::string_view value) -> tl::expected<uint16_t, std::string> {
			if (value.empty()) return 0;
			if (value == "None") return T_NODROP;
			if (value == "Uniq(SKCROWN)") return Uniq(UITEM_SKCROWN);
			if (value == "Uniq(CLEAVER)") return Uniq(UITEM_CLEAVER);
			return tl::make_unexpected("Invalid value. NOTE: Parser is incomplete");
		});

		reader.readInt("exp", monster.exp);
	}
	MonstersData.shrink_to_fit();
}

void LoadUniqueMonstDat()
{
	const std::string_view filename = "txtdata\\monsters\\unique_monstdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	dataFile.skipHeaderOrDie(filename);

	UniqueMonstersData.clear();
	UniqueMonstersData.reserve(dataFile.numRecords());
	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		UniqueMonsterData &monster = UniqueMonstersData.emplace_back();
		reader.read("type", monster.mtype, ParseMonsterId);
		reader.readString("name", monster.mName);
		reader.readString("trn", monster.mTrnName);
		reader.readInt("level", monster.mlevel);
		reader.readInt("maxHp", monster.mmaxhp);
		reader.read("ai", monster.mAi, ParseAiId);
		reader.readInt("intelligence", monster.mint);
		reader.readInt("minDamage", monster.mMinDamage);
		reader.readInt("maxDamage", monster.mMaxDamage);
		reader.readEnumList("resistance", monster.mMagicRes, ParseMonsterResistance);
		reader.read("monsterPack", monster.monsterPack, ParseUniqueMonsterPack);
		reader.readInt("customToHit", monster.customToHit);
		reader.readInt("customArmorClass", monster.customArmorClass);

		// talkMessage
		// TODO: Replace this hack with proper parsing once messages have been migrated to data files.
		reader.read("talkMessage", monster.mtalkmsg, [](std::string_view value) -> tl::expected<_speech_id, std::string> {
			if (value.empty()) return TEXT_NONE;
			if (value == "TEXT_GARBUD1") return TEXT_GARBUD1;
			if (value == "TEXT_ZHAR1") return TEXT_ZHAR1;
			if (value == "TEXT_BANNER10") return TEXT_BANNER10;
			if (value == "TEXT_VILE13") return TEXT_VILE13;
			if (value == "TEXT_VEIL9") return TEXT_VEIL9;
			if (value == "TEXT_WARLRD9") return TEXT_WARLRD9;
			return tl::make_unexpected("Invalid value. NOTE: Parser is incomplete");
		});
	}

	UniqueMonstersData.insert(UniqueMonstersData.end(), AdditionalUniqueMonstersData.begin(), AdditionalUniqueMonstersData.end());

	UniqueMonstersData.shrink_to_fit();
}

} // namespace

void LoadMonsterData()
{
	LoadMonstDat();
	LoadUniqueMonstDat();
}

size_t GetNumMonsterSprites()
{
	return MonsterSpritePaths.size();
}

} // namespace devilution
