#include "lua/modules/monsters.hpp"

#include <charconv>
#include <string_view>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "lua/metadoc.hpp"
#include "monstdat.h"
#include "utils/language.h"
#include "utils/str_split.hpp"

namespace devilution {

namespace {

void AddMonsterData(const std::string_view monsterId, const std::string_view name, const std::string_view assetsSuffix, const std::string_view soundSuffix, const std::string_view trnFile, const std::string_view availability, const uint16_t width, const uint16_t image, const bool hasSpecial, const bool hasSpecialSound, const std::string_view frames, const std::string_view rate, const int8_t minDunLvl, const int8_t maxDunLvl, const int8_t level, const uint16_t hitPointsMinimum, const uint16_t hitPointsMaximum, const std::string_view ai, const std::string_view abilityFlags, const uint8_t intelligence, const uint8_t toHit, const int8_t animFrameNum, const uint8_t minDamage, const uint8_t maxDamage, const uint8_t toHitSpecial, const int8_t animFrameNumSpecial, const uint8_t minDamageSpecial, const uint8_t maxDamageSpecial, const uint8_t armorClass, const std::string_view monsterClass, const std::string_view resistance, const std::string_view resistanceHell, const std::string_view selectionRegion, const std::string_view treasure, const uint16_t exp)
{
	if (MonstersData.size() >= static_cast<size_t>(NUM_MAX_MTYPES)) {
		DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("The maximum monster type number of {} has already been reached.")), static_cast<size_t>(NUM_MAX_MTYPES)));
	}

	MonsterData monster;

	monster.name = name;

	{
		const auto findIt = std::find(MonsterSpritePaths.begin(), MonsterSpritePaths.end(), assetsSuffix);
		if (findIt != MonsterSpritePaths.end()) {
			monster.spriteId = static_cast<uint16_t>(findIt - MonsterSpritePaths.begin());
		} else {
			monster.spriteId = static_cast<uint16_t>(MonsterSpritePaths.size());
			MonsterSpritePaths.push_back(std::string(assetsSuffix));
		}
	}

	monster.soundSuffix = soundSuffix;
	monster.trnFile = trnFile;

	const auto availabilityResult = ParseMonsterAvailability(availability);
	if (!availabilityResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster availability \"{}\": {}")), availability, availabilityResult.error()));
	}

	monster.availability = availabilityResult.value();
	monster.width = width;
	monster.image = image;
	monster.hasSpecial = hasSpecial;
	monster.hasSpecialSound = hasSpecialSound;

	size_t numElements = sizeof(monster.frames) / sizeof(monster.frames[0]);
	size_t i = 0;
	for (const std::string_view framesPart : SplitByChar(frames, ',')) {
		if (i >= numElements) {
			DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("The number of monster frames must be exactly {}.")), numElements));
		}

		const std::from_chars_result result
		    = std::from_chars(framesPart.data(), framesPart.data() + framesPart.size(), monster.frames[i]);
		if (result.ec != std::errc()) {
			DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster frames \"{}\".")), frames));
		}

		++i;
	}
	if (i != numElements) {
		DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("The number of monster frames must be exactly {}.")), numElements));
	}

	numElements = sizeof(monster.rate) / sizeof(monster.rate[0]);
	i = 0;
	for (const std::string_view ratePart : SplitByChar(rate, ',')) {
		if (i >= numElements) {
			DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("The number of monster rate elements must be exactly {}.")), numElements));
		}

		const std::from_chars_result result
		    = std::from_chars(ratePart.data(), ratePart.data() + ratePart.size(), monster.rate[i]);
		if (result.ec != std::errc()) {
			DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster rate \"{}\".")), rate));
		}

		++i;
	}
	if (i != numElements) {
		DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("The number of monster rate elements must be exactly {}.")), numElements));
	}

	monster.minDunLvl = minDunLvl;
	monster.maxDunLvl = maxDunLvl;
	monster.level = level;
	monster.hitPointsMinimum = hitPointsMinimum;
	monster.hitPointsMaximum = hitPointsMaximum;

	const auto aiResult = ParseAiId(ai);
	if (!aiResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster AI ID \"{}\": {}")), ai, aiResult.error()));
	}

	monster.ai = aiResult.value();

	monster.abilityFlags = {};

	if (!abilityFlags.empty()) {
		for (const std::string_view abilityFlagsPart : SplitByChar(abilityFlags, ',')) {
			const auto abilityFlagsResult = ParseMonsterFlag(abilityFlagsPart);
			if (!abilityFlagsResult.has_value()) {
				DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster ability flag \"{}\": {}")), abilityFlags, abilityFlagsResult.error()));
			}

			monster.abilityFlags |= abilityFlagsResult.value();
		}
	}

	monster.intelligence = intelligence;
	monster.toHit = toHit;
	monster.animFrameNum = animFrameNum;
	monster.minDamage = minDamage;
	monster.maxDamage = maxDamage;
	monster.toHitSpecial = toHitSpecial;
	monster.animFrameNumSpecial = animFrameNumSpecial;
	monster.minDamageSpecial = minDamageSpecial;
	monster.maxDamageSpecial = maxDamageSpecial;
	monster.armorClass = armorClass;

	const auto monsterClassResult = ParseMonsterClass(monsterClass);
	if (!monsterClassResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster class \"{}\": {}")), monsterClass, monsterClassResult.error()));
	}

	monster.monsterClass = monsterClassResult.value();

	monster.resistance = {};

	if (!resistance.empty()) {
		for (const std::string_view resistancePart : SplitByChar(resistance, ',')) {
			const auto monsterResistanceResult = ParseMonsterResistance(resistancePart);
			if (!monsterResistanceResult.has_value()) {
				DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster resistance \"{}\": {}")), resistance, monsterResistanceResult.error()));
			}

			monster.resistance |= monsterResistanceResult.value();
		}
	}

	monster.resistanceHell = {};

	if (!resistanceHell.empty()) {
		for (const std::string_view resistancePart : SplitByChar(resistanceHell, ',')) {
			const auto monsterResistanceResult = ParseMonsterResistance(resistancePart);
			if (!monsterResistanceResult.has_value()) {
				DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster resistance hell \"{}\": {}")), resistanceHell, monsterResistanceResult.error()));
			}

			monster.resistanceHell |= monsterResistanceResult.value();
		}
	}

	monster.selectionRegion = {};

	if (!selectionRegion.empty()) {
		for (const std::string_view selectionRegionPart : SplitByChar(selectionRegion, ',')) {
			const auto selectionRegionResult = ParseSelectionRegion(selectionRegionPart);
			if (!selectionRegionResult.has_value()) {
				DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse selection region \"{}\": {}")), selectionRegion, selectionRegionResult.error()));
			}

			monster.selectionRegion |= selectionRegionResult.value();
		}
	}

	const auto treasureResult = ParseMonsterTreasure(treasure);
	if (!treasureResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster treasure \"{}\": {}")), treasure, treasureResult.error()));
	}

	monster.treasure = treasureResult.value();
	monster.exp = exp;

	const auto [it, inserted] = AdditionalMonsterIdStringsToIndices.emplace(std::string(monsterId), static_cast<int16_t>(MonstersData.size()));
	if (!inserted) {
		DisplayFatalErrorAndExit(_("Adding Monster Failed"), fmt::format(fmt::runtime(_("A monster type already exists for ID \"{}\".")), monsterId));
	}

	MonstersData.push_back(std::move(monster));
}

void AddUniqueMonsterData(const std::string_view type, const std::string_view name, const std::string_view trn, const uint8_t level, const uint16_t maxHp, const std::string_view ai, const uint8_t intelligence, const uint8_t minDamage, const uint8_t maxDamage, const std::string_view resistance, const std::string_view monsterPack, const std::optional<uint8_t> customToHit, const std::optional<uint8_t> customArmorClass)
{
	UniqueMonsterData monster;

	const auto monsterTypeResult = ParseMonsterId(type);
	if (!monsterTypeResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Unique Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster type ID \"{}\": {}")), type, monsterTypeResult.error()));
	}

	monster.mtype = monsterTypeResult.value();
	monster.mName = name;
	monster.mTrnName = trn;
	monster.mlevel = level;
	monster.mmaxhp = maxHp;

	const auto monsterAiResult = ParseAiId(ai);
	if (!monsterAiResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Unique Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster AI ID \"{}\": {}")), ai, monsterAiResult.error()));
	}

	monster.mAi = monsterAiResult.value();
	monster.mint = intelligence;
	monster.mMinDamage = minDamage;
	monster.mMaxDamage = maxDamage;

	monster.mMagicRes = {};

	if (!resistance.empty()) {
		for (const std::string_view resistancePart : SplitByChar(resistance, ',')) {
			const auto monsterResistanceResult = ParseMonsterResistance(resistancePart);
			if (!monsterResistanceResult.has_value()) {
				DisplayFatalErrorAndExit(_("Adding Unique Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse monster resistance \"{}\": {}")), resistance, monsterResistanceResult.error()));
			}

			monster.mMagicRes |= monsterResistanceResult.value();
		}
	}

	const auto monsterPackResult = ParseUniqueMonsterPack(monsterPack);
	if (!monsterPackResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Unique Monster Failed"), fmt::format(fmt::runtime(_("Failed to parse unique monster pack \"{}\": {}")), monsterPack, monsterPackResult.error()));
	}

	monster.monsterPack = monsterPackResult.value();
	monster.customToHit = customToHit.value_or(0);
	monster.customArmorClass = customArmorClass.value_or(0);
	monster.mtalkmsg = TEXT_NONE;

	UniqueMonstersData.push_back(std::move(monster));
}

} // namespace

sol::table LuaMonstersModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "addMonsterData", "(monsterId: string, name: string, assetsSuffix: string, soundSuffix: string, trnFile: string, availability: string, width: number, image: number, hasSpecial: boolean, hasSpecialSound: boolean, frames: string, rate: string, minDunLvl: number, maxDunLvl: number, level: number, hitPointsMinimum: number, hitPointsMaximum: number, ai: string, abilityFlags: string, intelligence: number, toHit: number, animFrameNum: number, minDamage: number, maxDamage: number, toHitSpecial: number, animFrameNumSpecial: number, minDamageSpecial: number, maxDamageSpecial: number, armorClass: number, monsterClass: string, resistance: string, resistanceHell: string, selectionRegion: string, treasure: string, exp: number)", AddMonsterData);
	LuaSetDocFn(table, "addUniqueMonsterData", "(type: string, name: string, trn: string, level: number, maxHp: number, ai: string, intelligence: number, minDamage: number, maxDamage: number, resistance: string, monsterPack: string, customToHit: number = nil, customArmorClass: number = nil)", AddUniqueMonsterData);
	return table;
}

} // namespace devilution
