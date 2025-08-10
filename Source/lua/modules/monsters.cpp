#include "lua/modules/monsters.hpp"

#include <string_view>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "lua/metadoc.hpp"
#include "monstdat.h"
#include "utils/language.h"
#include "utils/str_split.hpp"

namespace devilution {

namespace {

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
	LuaSetDocFn(table, "addUniqueMonsterData", "(type: string, name: string, trn: string, level: number, maxHp: number, ai: string, intelligence: number, minDamage: number, maxDamage: number, resistance: string, monsterPack: string, customToHit: number = nil, customArmorClass: number = nil)", AddUniqueMonsterData);
	return table;
}

} // namespace devilution
