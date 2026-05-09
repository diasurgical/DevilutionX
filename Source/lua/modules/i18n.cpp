#include "lua/modules/i18n.hpp"

#include <string_view>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "lua/metadoc.hpp"
#include "utils/language.h"

namespace devilution {

sol::table LuaI18nModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "language_code", "()",
	    "Returns the current language code", GetLanguageCode);
	LuaSetDocFn(table, "translate", "(text: string)",
	    "Translates the given string", [](const char *key) { return LanguageTranslate(key); });
	LuaSetDocFn(table, "plural_translate", "(singular: string, plural: string, count: integer)",
	    "Returns a singular or plural translation for the given keys and count", [](const char *singular, std::string_view plural, int count) {
		    return fmt::format(fmt::runtime(LanguagePluralTranslate(singular, plural, count)), count);
	    });
	LuaSetDocFn(table, "particular_translate", "(context: string, text: string)",
	    "Returns the translation for the given context identifier and key.", LanguageParticularTranslate);
	LuaSetDocFn(table, "is_small_font_tall", "()",
	    "Whether the language's small font is tall (16px)", IsSmallFontTall);
	return table;
}

} // namespace devilution
