#ifdef _DEBUG
#include "lua/modules/dev/towners.hpp"

#include <string>

#include <ankerl/unordered_dense.h>
#include <sol/sol.hpp>

#include "lua/metadoc.hpp"
#include "player.h"
#include "spells.h"
#include "towners.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

ankerl::unordered_dense::map<std::string_view, _talker_id> TownerShortNameToTownerId = {
	{ "griswold", _talker_id::TOWN_SMITH },
	{ "smith", _talker_id::TOWN_SMITH },
	{ "pepin", _talker_id::TOWN_HEALER },
	{ "healer", _talker_id::TOWN_HEALER },
	{ "ogden", _talker_id::TOWN_TAVERN },
	{ "tavern", _talker_id::TOWN_TAVERN },
	{ "cain", _talker_id::TOWN_STORY },
	{ "story", _talker_id::TOWN_STORY },
	{ "farnham", _talker_id::TOWN_DRUNK },
	{ "drunk", _talker_id::TOWN_DRUNK },
	{ "adria", _talker_id::TOWN_WITCH },
	{ "witch", _talker_id::TOWN_WITCH },
	{ "gillian", _talker_id::TOWN_BMAID },
	{ "bmaid", _talker_id::TOWN_BMAID },
	{ "wirt", _talker_id ::TOWN_PEGBOY },
	{ "pegboy", _talker_id ::TOWN_PEGBOY },
	{ "lester", _talker_id ::TOWN_FARMER },
	{ "farmer", _talker_id ::TOWN_FARMER },
	{ "girl", _talker_id ::TOWN_GIRL },
	{ "nut", _talker_id::TOWN_COWFARM },
	{ "cowfarm", _talker_id::TOWN_COWFARM },
};

std::string DebugCmdVisitTowner(std::string_view name)
{
	Player &myPlayer = *MyPlayer;

	if (setlevel || !myPlayer.isOnLevel(0))
		return StrCat("This command is only available in Town!");

	if (name.empty()) {
		std::string ret;
		ret = StrCat("Please provide the name of a Towner: ");
		for (const auto &[name, _] : TownerShortNameToTownerId) {
			ret += ' ';
			ret.append(name);
		}
		return ret;
	}

	auto it = TownerShortNameToTownerId.find(name);
	if (it == TownerShortNameToTownerId.end())
		return StrCat(name, " is invalid!");

	for (const Towner &towner : Towners) {
		if (towner._ttype != it->second) continue;
		CastSpell(
		    *MyPlayer,
		    SpellID::Teleport,
		    myPlayer.position.tile,
		    towner.position,
		    /*spllvl=*/1);
		return StrCat("Moved you to ", name, ".");
	}

	return StrCat("Unable to locate ", name, "!");
}

std::string DebugCmdTalkToTowner(std::string_view name)
{
	if (name.empty()) {
		std::string ret;
		ret = StrCat("Please provide the name of a Towner: ");
		for (const auto &[name, _] : TownerShortNameToTownerId) {
			ret += ' ';
			ret.append(name);
		}
		return ret;
	}

	auto it = TownerShortNameToTownerId.find(name);
	if (it == TownerShortNameToTownerId.end())
		return StrCat(name, " is invalid!");

	if (!DebugTalkToTowner(it->second)) return StrCat("Towner not found!");
	return StrCat("Opened ", name, " talk window.");
}

} // namespace

sol::table LuaDevTownersModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	SetDocumented(table, "talk", "(name: string)", "Talk to towner.", &DebugCmdTalkToTowner);
	SetDocumented(table, "visit", "(name: string)", "Teleport to towner.", &DebugCmdVisitTowner);
	return table;
}

} // namespace devilution
#endif // _DEBUG
