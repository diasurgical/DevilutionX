#include "lua/modules/render.hpp"

#include <sol/sol.hpp>

#include "engine/dx.h"
#include "engine/render/text_render.hpp"
#include "lua/metadoc.hpp"
#include "utils/display.h"

namespace devilution {

sol::table LuaRenderModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "string", "(text: string, x: integer, y: integer)",
	    "Renders a string at the given coordinates",
	    [](std::string_view text, int x, int y) { DrawString(GlobalBackBuffer(), text, { x, y }); });
	LuaSetDocFn(table, "screen_width", "()",
	    "Returns the screen width", []() { return gnScreenWidth; });
	LuaSetDocFn(table, "screen_height", "()",
	    "Returns the screen height", []() { return gnScreenHeight; });
	return table;
}

} // namespace devilution
