#ifdef _DEBUG
#include "lua/modules/dev/player/spells.hpp"

#include <cstdint>
#include <string>

#include <sol/sol.hpp>

#include "lua/metadoc.hpp"
#include "msg.h"
#include "spelldat.h"
#include "spells.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {
std::string DebugCmdSetSpellsLevel(uint8_t level)
{
	for (uint8_t i = static_cast<uint8_t>(SpellID::Firebolt); i < MAX_SPELLS; i++) {
		if (GetSpellBookLevel(static_cast<SpellID>(i)) != -1) {
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, i, level);
		}
	}
	if (level == 0)
		MyPlayer->learnedSpells = 0;

	return StrCat("Set all spell levels to ", level);
}
} // namespace

sol::table LuaDevPlayerSpellsModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	SetDocumented(table, "setLevel", "(level: number)", "Set spell level for all spells.", &DebugCmdSetSpellsLevel);
	return table;
}

} // namespace devilution
#endif // _DEBUG
