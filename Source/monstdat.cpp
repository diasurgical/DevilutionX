/**
 * @file monstdat.cpp
 *
 * Implementation of all monster data.
 */
#include "monstdat.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <expected.hpp>
#include <fmt/core.h>
#include <magic_enum/magic_enum.hpp>

#include "appfat.h"
#include "cursor.h"
#include "data/file.hpp"
#include "data/iterators.hpp"
#include "data/record_reader.hpp"
#include "items.h"
#include "lua/lua_global.hpp"
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

namespace {

/** Contains the mapping between monster ID strings and indices, used for parsing additional monster data. */
ankerl::unordered_dense::map<std::string, int16_t> AdditionalMonsterIdStringsToIndices;

tl::expected<_monster_id, std::string> ParseMonsterId(std::string_view value)
{
	const std::optional<_monster_id> enumValueOpt = magic_enum::enum_cast<_monster_id>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	const auto findIt = AdditionalMonsterIdStringsToIndices.find(std::string(value));
	if (findIt != AdditionalMonsterIdStringsToIndices.end()) {
		return static_cast<_monster_id>(findIt->second);
	}
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<MonsterAvailability, std::string> ParseMonsterAvailability(std::string_view value)
{
	if (value == "Always") return MonsterAvailability::Always;
	if (value == "Never") return MonsterAvailability::Never;
	if (value == "Retail") return MonsterAvailability::Retail;
	return tl::make_unexpected("Expected one of: Always, Never, or Retail");
}

tl::expected<MonsterAIID, std::string> ParseAiId(std::string_view value)
{
	const std::optional<MonsterAIID> enumValueOpt = magic_enum::enum_cast<MonsterAIID>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value");
}

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
	const std::optional<MonsterClass> enumValueOpt = magic_enum::enum_cast<MonsterClass>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<monster_resistance, std::string> ParseMonsterResistance(std::string_view value)
{
	const std::optional<monster_resistance> enumValueOpt = magic_enum::enum_cast<monster_resistance>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<SelectionRegion, std::string> ParseSelectionRegion(std::string_view value)
{
	if (value.empty()) return SelectionRegion::None;
	if (value == "Bottom") return SelectionRegion::Bottom;
	if (value == "Middle") return SelectionRegion::Middle;
	if (value == "Top") return SelectionRegion::Top;
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<uint16_t, std::string> ParseMonsterTreasure(std::string_view value)
{
	// TODO: Replace this hack with proper parsing.

	if (value.empty()) return 0;
	if (value == "None") return T_NODROP;
	if (value == "Uniq(SKCROWN)") return Uniq(UITEM_SKCROWN);
	if (value == "Uniq(CLEAVER)") return Uniq(UITEM_CLEAVER);
	return tl::make_unexpected("Invalid value. NOTE: Parser is incomplete");
}

tl::expected<UniqueMonsterPack, std::string> ParseUniqueMonsterPack(std::string_view value)
{
	const std::optional<UniqueMonsterPack> enumValueOpt = magic_enum::enum_cast<UniqueMonsterPack>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value");
}

} // namespace

void LoadMonstDatFromFile(DataFile &dataFile, const std::string_view filename, bool grow)
{
	dataFile.skipHeaderOrDie(filename);

	if (grow) {
		MonstersData.reserve(MonstersData.size() + dataFile.numRecords());
	}

	for (DataFileRecord record : dataFile) {
		if (MonstersData.size() >= static_cast<size_t>(NUM_MAX_MTYPES)) {
			DisplayFatalErrorAndExit(_("Loading Monster Data Failed"), fmt::format(fmt::runtime(_("Could not add a monster, since the maximum monster type number of {} has already been reached.")), static_cast<size_t>(NUM_MAX_MTYPES)));
		}

		RecordReader reader { record, filename };

		std::string monsterId;
		reader.readString("_monster_id", monsterId);
		const std::optional<_monster_id> monsterIdEnumValueOpt = magic_enum::enum_cast<_monster_id>(monsterId);

		if (!monsterIdEnumValueOpt.has_value()) {
			const size_t monsterIndex = MonstersData.size();
			const auto [it, inserted] = AdditionalMonsterIdStringsToIndices.emplace(monsterId, static_cast<int16_t>(monsterIndex));
			if (!inserted) {
				DisplayFatalErrorAndExit(_("Loading Monster Data Failed"), fmt::format(fmt::runtime(_("A monster type already exists for ID \"{}\".")), monsterId));
			}
		}

		// for hardcoded monsters, use their predetermined slot; for non-hardcoded ones, use the slots after that
		MonsterData &monster = monsterIdEnumValueOpt.has_value() ? MonstersData[monsterIdEnumValueOpt.value()] : MonstersData.emplace_back();

		reader.readString("name", monster.name);
		{
			std::string assetsSuffix;
			reader.readString("assetsSuffix", assetsSuffix);
			const auto findIt = std::find(MonsterSpritePaths.begin(), MonsterSpritePaths.end(), assetsSuffix);
			if (findIt != MonsterSpritePaths.end()) {
				monster.spriteId = static_cast<uint16_t>(findIt - MonsterSpritePaths.begin());
			} else {
				monster.spriteId = static_cast<uint16_t>(MonsterSpritePaths.size());
				MonsterSpritePaths.push_back(std::string(assetsSuffix));
			}
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
		reader.read("treasure", monster.treasure, ParseMonsterTreasure);

		reader.readInt("exp", monster.exp);
	}
}

namespace {

void LoadMonstDat()
{
	const std::string_view filename = "txtdata\\monsters\\monstdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	MonstersData.clear();
	AdditionalMonsterIdStringsToIndices.clear();
	MonstersData.resize(NUM_DEFAULT_MTYPES); // ensure the hardcoded monster type slots are filled
	LoadMonstDatFromFile(dataFile, filename, false);

	LuaEvent("MonsterDataLoaded");

	MonstersData.shrink_to_fit();
}

} // namespace

void LoadUniqueMonstDatFromFile(DataFile &dataFile, std::string_view filename)
{
	dataFile.skipHeaderOrDie(filename);

	UniqueMonstersData.reserve(UniqueMonstersData.size() + dataFile.numRecords());

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
		reader.read("talkMessage", monster.mtalkmsg, ParseSpeechId);
	}
}

namespace {

void LoadUniqueMonstDat()
{
	const std::string_view filename = "txtdata\\monsters\\unique_monstdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);

	UniqueMonstersData.clear();
	LoadUniqueMonstDatFromFile(dataFile, filename);

	LuaEvent("UniqueMonsterDataLoaded");

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
