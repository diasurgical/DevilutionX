#include "lua/modules/audio.hpp"

#include <sol/sol.hpp>

#include "effects.h"
#include "lua/metadoc.hpp"

namespace devilution {

namespace {

bool IsValidSfx(int16_t psfx)
{
	return psfx >= 0 && psfx <= static_cast<int16_t>(SfxID::LAST);
}

} // namespace

sol::table LuaAudioModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table,
	    "playSfx", "(id: number)",
	    [](int16_t psfx) { if (IsValidSfx(psfx)) PlaySFX(static_cast<SfxID>(psfx)); });
	LuaSetDocFn(table,
	    "playSfxLoc", "(id: number, x: number, y: number)",
	    [](int16_t psfx, int x, int y) { if (IsValidSfx(psfx)) PlaySfxLoc(static_cast<SfxID>(psfx), { x, y }); });
	return table;
}

} // namespace devilution
