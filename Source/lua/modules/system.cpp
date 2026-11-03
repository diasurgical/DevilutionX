#include "lua/modules/system.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <sol/sol.hpp>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "control/control_text_prompt.hpp"
#include "lua/lua_global.hpp"
#include "lua/metadoc.hpp"

namespace devilution {

sol::table LuaSystemModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "get_ticks", "() -> integer", "Returns the number of milliseconds since the game started.",
	    []() { return static_cast<int>(SDL_GetTicks()); });

	LuaSetDocFn(table, "promptText", "(prompt: string, maxLength: integer, onFinish: function)",
	    "Open a modal text-entry box showing `prompt`. onFinish(text) is called with the entered text when the "
	    "player presses Enter, or onFinish(nil) on Escape. maxLength is in bytes (at most 255). The text is not "
	    "filtered; validate it in onFinish. Opening a prompt cancels any open one.",
	    [](std::string_view prompt, int maxLength, const sol::protected_function &onFinish) {
		    OpenTextPrompt(prompt, static_cast<size_t>(std::max(maxLength, 1)),
		        [onFinish](std::optional<std::string> text) {
			        if (text.has_value())
				        SafeCallResult(onFinish(*text), /*optional=*/true);
			        else
				        SafeCallResult(onFinish(sol::lua_nil), /*optional=*/true);
		        });
	    });

	return table;
}

} // namespace devilution
