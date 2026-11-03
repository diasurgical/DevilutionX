#include "lua/modules/stash.hpp"

#include <string>
#include <string_view>

#include <sol/sol.hpp>

#include "lua/metadoc.hpp"
#include "qol/stash.h"

namespace devilution {

sol::table LuaStashModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "register", "(name: string, options?: { pages?: integer, gold?: boolean }) -> boolean",
	    "Register a mod stash. It is saved to its own save file named after `name` (letters, digits, '_' and '-', "
	    "max 32 characters; names used by the engine's own save files are refused), with the same extension as "
	    "the player stash. pages = 1..100 (default 100); gold = whether it holds gold (default true). Registering "
	    "an existing name updates its options and keeps its contents. Returns false if the name or options are invalid. "
	    "Register from mod init. Which items it accepts is decided by the OnItemAllowedInStash event "
	    "(check stash.activeName() there).",
	    [](std::string_view name, sol::optional<sol::table> options) -> bool {
		    unsigned pages = DefaultStashPageCount;
		    bool gold = true;
		    if (options.has_value()) {
			    const int requestedPages = options->get_or("pages", static_cast<int>(DefaultStashPageCount));
			    pages = requestedPages > 0 ? static_cast<unsigned>(requestedPages) : 0;
			    gold = options->get_or("gold", true);
		    }
		    return LuaRegisterModStash(name, pages, gold);
	    });
	LuaSetDocFn(table, "open", "(name: string) -> boolean",
	    "Open the stash window on a registered mod stash (closing any open stash first). Its contents are loaded "
	    "from its save file the first time it is opened in a game. Returns false if no stash has that name.",
	    [](std::string_view name) -> bool {
		    return LuaOpenModStash(name);
	    });
	LuaSetDocFn(table, "activeName", "() -> string|nil",
	    "Name of the mod stash currently shown in the stash window, or nil when the player stash is shown or no "
	    "stash is open.",
	    []() -> sol::optional<std::string> {
		    const std::string_view name = LuaActiveModStashName();
		    if (name.empty())
			    return sol::nullopt;
		    return std::string(name);
	    });
	return table;
}

} // namespace devilution
