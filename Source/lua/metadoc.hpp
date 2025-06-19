#pragma once

#include <sol/sol.hpp>

#include <string_view>

#include "utils/str_cat.hpp"

namespace devilution {

inline std::string LuaSignatureKey(std::string_view key)
{
	return StrCat("__sig_", key);
}

inline std::string LuaDocstringKey(std::string_view key)
{
	return StrCat("__doc_", key);
}

template <typename U, typename T>
void SetDocumented(sol::usertype<U> &table, std::string_view key, std::string_view signature, std::string_view doc, T &&value)
{
	table[key] = std::forward<T>(value);
	// TODO: figure out a way to set signature and docstring.
}

template <typename U, typename G, typename S>
void SetDocumented(sol::usertype<U> &table, std::string_view key, std::string_view signature, std::string_view doc, G &&getter, S &&setter)
{
	table[key] = sol::property(std::forward<G>(getter), std::forward<S>(setter));
	// TODO: figure out a way to set signature and docstring.
}

template <typename T>
void SetDocumented(sol::table &table, std::string_view key, std::string_view signature, std::string_view doc, T &&value)
{
	table[key] = std::forward<T>(value);
	table[LuaSignatureKey(key)] = signature;
	table[LuaDocstringKey(key)] = doc;
}

template <typename T>
void SetWithSignature(sol::table &table, std::string_view key, std::string_view signature, T &&value)
{
	table[key] = std::forward<T>(value);
	table[LuaSignatureKey(key)] = signature;
}

inline std::optional<std::string> GetSignature(const sol::table &table, std::string_view key)
{
	return table.get<std::optional<std::string>>(LuaSignatureKey(key));
}

inline std::optional<std::string> GetDocstring(const sol::table &table, std::string_view key)
{
	return table.get<std::optional<std::string>>(LuaDocstringKey(key));
}

} // namespace devilution
