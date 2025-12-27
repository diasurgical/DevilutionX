#include "lua/modules/audio.hpp"

#include <magic_enum/magic_enum.hpp>
#include <sol/sol.hpp>

#include "effects.h"
#include "lua/metadoc.hpp"
#include "sound_effect_enums.h"

namespace devilution {

namespace {

bool IsValidSfx(int16_t psfx)
{
	return psfx >= 0 && psfx <= static_cast<int16_t>(SfxID::LAST);
}

void RegisterSfxIDEnum(sol::state_view &lua)
{
	constexpr auto enumValues = magic_enum::enum_values<SfxID>();
	sol::table enumTable = lua.create_table();
	for (const auto enumValue : enumValues) {
		const std::string_view name = magic_enum::enum_name(enumValue);
		if (!name.empty() && name != "LAST" && name != "None") {
			enumTable[name] = static_cast<int16_t>(enumValue);
		}
	}
	// Add LAST and None explicitly
	enumTable["LAST"] = static_cast<int16_t>(SfxID::LAST);
	enumTable["None"] = static_cast<int16_t>(SfxID::None);
	lua["SfxID"] = enumTable;
}

} // namespace

sol::table LuaAudioModule(sol::state_view &lua)
{
	RegisterSfxIDEnum(lua);
	sol::table table = lua.create_table();
	LuaSetDocFn(table,
	    "playSfx", "(id: number)",
	    [](int16_t psfx) { if (IsValidSfx(psfx)) PlaySFX(static_cast<SfxID>(psfx)); });
	LuaSetDocFn(table,
	    "playSfxLoc", "(id: number, x: number, y: number)",
	    [](int16_t psfx, int x, int y) { if (IsValidSfx(psfx)) PlaySfxLoc(static_cast<SfxID>(psfx), { x, y }); });
	// Expose SfxID enum through the module table
	table["SfxID"] = lua["SfxID"];
	return table;
}

} // namespace devilution
