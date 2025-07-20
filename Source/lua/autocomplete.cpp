#ifdef _DEBUG
#include "lua/autocomplete.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <sol/sol.hpp>
#include <sol/utility/to_string.hpp>

#include "appfat.h"
#include "engine/assets.hpp"
#include "lua/lua_global.hpp"
#include "lua/metadoc.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

namespace devilution {

namespace {

std::string_view GetLastToken(std::string_view text)
{
	if (text.empty())
		return {};
	size_t i = text.size();
	while (i > 0 && text[i - 1] != ' ' && text[i - 1] != '(' && text[i - 1] != ',')
		--i;
	return text.substr(i);
}

struct ValueInfo {
	bool callable = false;
	std::string signature;
	std::string docstring;
};

sol::protected_function LoadLuaFunctionSignatureGetter(sol::state &lua)
{
	tl::expected<AssetData, std::string> src = LoadAsset("lua_internal\\get_lua_function_signature.lua");
	if (!src.has_value()) {
		app_fatal(src.error());
	}
	const sol::object obj = SafeCallResult(lua.safe_script(std::string_view(src.value())), /*optional=*/false);
	if (obj.get_type() != sol::type::function) {
		app_fatal("Lua: expected a function");
	}
	return obj.as<sol::protected_function>();
}

std::string GetNativeLuaFunctionSignature(const sol::object &fn)
{
	sol::state &lua = GetLuaState();
	constexpr std::string_view LuaFunctionSignatureGetterKey = "__DEVILUTIONX_GET_LUA_SIGNATURE";
	sol::object getter = lua[LuaFunctionSignatureGetterKey];
	if (getter.get_type() == sol::type::lua_nil) {
		getter = lua[LuaFunctionSignatureGetterKey] = LoadLuaFunctionSignatureGetter(lua);
	}
	const sol::object obj = SafeCallResult(getter.as<sol::protected_function>()(fn), /*optional=*/false);
	if (obj.get_type() != sol::type::string) {
		app_fatal(StrCat("Lua: Expected a string, got ", sol::utility::to_string(obj)));
	}
	return obj.as<std::string>();
}

std::string GetFunctionSignature(const sol::object &value)
{
	value.push(value.lua_state());
	const bool isC = lua_iscfunction(value.lua_state(), -1) != 0;
	lua_pop(value.lua_state(), 1);
	return isC ? "(...)" : GetNativeLuaFunctionSignature(value);
}

void RemoveFirstArgumentFromFunctionSignature(std::string &signature)
{
	if (signature == "(...)") return;
	size_t firstArgEnd = signature.find_first_of(",)");
	if (firstArgEnd == std::string::npos) return;
	++firstArgEnd;
	if (firstArgEnd == signature.size()) {
		signature = "()";
		return;
	}
	if (signature[firstArgEnd] == ' ') ++firstArgEnd;
	signature.replace(0, firstArgEnd, "(");
}

ValueInfo GetValueInfo(const sol::table &table, std::string_view key, const sol::object &value)
{
	ValueInfo info;
	if (std::optional<std::string> signature = GetSignature(table, key); signature.has_value()) {
		info.signature = *std::move(signature);
	}
	if (std::optional<std::string> docstring = GetDocstring(table, key); docstring.has_value()) {
		info.docstring = *std::move(docstring);
	}
	if (value.get_type() == sol::type::function) {
		info.callable = true;
		if (info.signature.empty()) info.signature = GetFunctionSignature(value);
		return info;
	}
	if (!value.is<sol::table>()) return info;
	const auto valueAsTable = value.as<sol::table>();
	const auto metatable = valueAsTable.get<std::optional<sol::object>>(sol::metatable_key);
	if (!metatable || !metatable->is<sol::table>())
		return info;
	const auto metatableTbl = metatable->as<sol::table>();
	const auto callFn = metatableTbl.get<std::optional<sol::object>>(sol::meta_function::call);
	info.callable = callFn.has_value();
	if (info.callable && info.signature.empty()) {
		if (info.signature.empty()) {
			info.signature = GetFunctionSignature(*callFn);
			// Remove the first argument (the table passed to `__call`):
			RemoveFirstArgumentFromFunctionSignature(info.signature);
		}
	}
	return info;
}

ValueInfo GetValueInfoForUserdata(const sol::userdata &obj, std::string_view key, const sol::object &value, std::optional<LuaUserdataMemberType> memberType)
{
	ValueInfo info;
	if (value.get_type() == sol::type::userdata) {
		info.callable = false;
		return info;
	}

	if (std::optional<std::string> signature = GetLuaUserdataSignature(obj, key); signature.has_value()) {
		info.signature = *std::move(signature);
	}
	if (std::optional<std::string> docstring = GetLuaUserdataDocstring(obj, key); docstring.has_value()) {
		info.docstring = *std::move(docstring);
	}
	if (memberType.has_value()) {
		info.callable = *memberType == LuaUserdataMemberType::MemberFunction;
	} else {
		info.callable = value.get_type() == sol::type::function;
	}
	return info;
}

struct UserdataQuery {
	const sol::userdata *obj;
	bool colonAccess;
};

void SuggestionsFromTable(const sol::table &table, std::string_view prefix,
    size_t maxSuggestions, ankerl::unordered_dense::set<LuaAutocompleteSuggestion> &out,
    std::optional<UserdataQuery> userdataQuery = std::nullopt)
{
	for (const auto &[key, value] : table) {
		if (key.get_type() == sol::type::string) {
			std::string keyStr = key.as<std::string>();
			if (!keyStr.starts_with(prefix) || keyStr.size() == prefix.size())
				continue;
			if (keyStr.starts_with("__") && !prefix.starts_with("__"))
				continue;
			// sol-internal keys -- we don't have fonts for these so skip them.
			if (keyStr.find("♻") != std::string::npos
			    || keyStr.find("☢") != std::string::npos
			    || keyStr.find("🔩") != std::string::npos)
				continue;
			ValueInfo info;
			std::optional<LuaUserdataMemberType> memberType;
			if (userdataQuery.has_value()) {
				memberType = GetLuaUserdataMemberType(*userdataQuery->obj, keyStr, value);
				const bool requiresColonAccess = memberType.has_value()
				    ? *memberType == LuaUserdataMemberType::MemberFunction
				    : value.get_type() == sol::type::function;
				if (userdataQuery->colonAccess != requiresColonAccess) {
					continue;
				}
				info = GetValueInfoForUserdata(*userdataQuery->obj, keyStr, value, memberType);
			} else {
				info = GetValueInfo(table, keyStr, value);
			}
			std::string completionText = keyStr.substr(prefix.size());
			LuaAutocompleteSuggestion suggestion { std::move(keyStr), std::move(completionText) };
			if (info.callable) {
				suggestion.completionText.append("()");
				suggestion.cursorAdjust = -1;
			}
			if (!info.signature.empty()) {
				if (memberType.has_value() && memberType != LuaUserdataMemberType::MemberFunction) {
					StrAppend(suggestion.displayText, ": ");
				}
				StrAppend(suggestion.displayText, info.signature);
			}
			if (!info.docstring.empty()) {
				std::string_view firstLine = info.docstring;
				if (const size_t newlinePos = firstLine.find('\n'); newlinePos != std::string_view::npos) {
					firstLine = firstLine.substr(0, newlinePos);
				}
				StrAppend(suggestion.displayText, " - ", firstLine);
			}
			out.insert(std::move(suggestion));
			if (out.size() == maxSuggestions)
				break;
		}
	}
	const auto fallback = table.get<std::optional<sol::object>>(sol::metatable_key);
	if (fallback.has_value() && fallback->get_type() == sol::type::table) {
		SuggestionsFromTable(fallback->as<sol::table>(), prefix, maxSuggestions, out);
	}
}

void SuggestionsFromUserdata(UserdataQuery query, std::string_view prefix,
    size_t maxSuggestions, ankerl::unordered_dense::set<LuaAutocompleteSuggestion> &out)
{
	const auto &meta = query.obj->get<std::optional<sol::object>>(sol::metatable_key);
	if (meta.has_value() && meta->get_type() == sol::type::table) {
		SuggestionsFromTable(meta->as<sol::table>(), prefix, maxSuggestions, out, query);
	}
}

bool IsAlnum(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool IsIdentifierChar(char c)
{
	return IsAlnum(c) || c == '_';
}

bool IsIdentifierOrExprChar(char c)
{
	return IsIdentifierChar(c) || c == '-' || c == '+' || c == '*' || c == '/' || c == '=';
}

} // namespace

void GetLuaAutocompleteSuggestions(std::string_view text, size_t cursorPos, const sol::environment &lua,
    size_t maxSuggestions, std::vector<LuaAutocompleteSuggestion> &out)
{
	out.clear();
	const std::string_view textPrefix = text.substr(0, cursorPos);
	if (textPrefix.empty()) return;
	const std::string_view textSuffix = text.substr(cursorPos);
	if (!textSuffix.empty()) {
		const char c = textSuffix[0];
		if (IsIdentifierOrExprChar(c) || (c == ' ' && textSuffix.size() > 1)) return;
	}
	if (textPrefix.size() >= 2 && textPrefix.back() == ' ' && IsIdentifierChar(textPrefix[textPrefix.size() - 2])) {
		return;
	}
	std::string_view token = GetLastToken(textPrefix);
	const char prevChar = token.data() == textPrefix.data() ? '\0' : *(token.data() - 1);
	if (prevChar == '(' || prevChar == ',') return;
	const size_t dotPos = token.find_last_of(".:");
	const std::string_view prefix = token.substr(dotPos + 1);
	const char completionChar = dotPos != std::string_view::npos ? token[dotPos] : '\0';
	token.remove_suffix(token.size() - (dotPos == std::string_view::npos ? 0 : dotPos));

	ankerl::unordered_dense::set<LuaAutocompleteSuggestion> suggestions;
	const auto addSuggestions = [&](const sol::table &table) {
		SuggestionsFromTable(table, prefix, maxSuggestions, suggestions);
	};

	if (token.empty()) {
		if (prevChar == '.') return;
		addSuggestions(lua);
		const auto fallback = lua.get<std::optional<sol::object>>("_G");
		if (fallback.has_value() && fallback->get_type() == sol::type::table) {
			addSuggestions(fallback->as<sol::table>());
		}
	} else {
		std::optional<sol::object> obj = lua;
		for (const std::string_view partDot : SplitByChar(token, '.')) {
			for (const std::string_view part : SplitByChar(partDot, ':')) {
				obj = obj->as<sol::table>().get<std::optional<sol::object>>(part);
				if (!obj.has_value() || !(obj->get_type() == sol::type::table || obj->get_type() == sol::type::userdata)) {
					return;
				}
			}
		}
		if (obj->get_type() == sol::type::table) {
			addSuggestions(obj->as<sol::table>());
		} else if (obj->get_type() == sol::type::userdata) {
			const sol::userdata &data = obj->as<sol::userdata>();
			SuggestionsFromUserdata(UserdataQuery { .obj = &data, .colonAccess = completionChar == ':' },
			    prefix, maxSuggestions, suggestions);
		}
	}

	out.insert(out.end(), suggestions.begin(), suggestions.end());
	c_sort(out);
}

} // namespace devilution
#endif // _DEBUG
