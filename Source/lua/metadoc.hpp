#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <sol/sol.hpp>

#include "utils/str_cat.hpp"

namespace devilution {

enum class LuaUserdataMemberType : uint8_t {
	ReadonlyProperty,
	Property,
	MemberFunction,
	Constructor,
};

inline std::string LuaSignatureKey(std::string_view key)
{
	return StrCat("__sig_", key);
}

inline std::string LuaDocstringKey(std::string_view key)
{
	return StrCat("__doc_", key);
}

inline std::string LuaUserdataMemberTypeKey(std::string_view key)
{
	return StrCat("__udt_", key);
}

template <typename U, typename T>
void LuaSetDoc(sol::usertype<U> &table, std::string_view key, const char *signature, const char *doc, T &&value)
{
	table.set(key, std::forward<T>(value));
	table.set(LuaSignatureKey(key), sol::var(signature));
	table.set(LuaDocstringKey(key), sol::var(doc));
}

template <typename U, typename T>
void LuaSetDocFn(sol::usertype<U> &table, std::string_view key, const char *signature, const char *doc, T &&value)
{
	table.set_function(key, std::forward<T>(value));
	table.set(LuaSignatureKey(key), sol::var(signature));
	table.set(LuaDocstringKey(key), sol::var(doc));
	table.set(LuaUserdataMemberTypeKey(key), sol::var(static_cast<uint8_t>(LuaUserdataMemberType::MemberFunction)));
}

template <typename U, typename G>
void LuaSetDocReadonlyProperty(sol::usertype<U> &table, std::string_view key, const char *type, const char *doc, G &&getter)
{
	table.set(key, sol::readonly_property(std::forward<G>(getter)));
	table.set(LuaSignatureKey(key), sol::var(type));
	table.set(LuaDocstringKey(key), sol::var(doc));
	table.set(LuaUserdataMemberTypeKey(key), sol::var(static_cast<uint8_t>(LuaUserdataMemberType::ReadonlyProperty)));
}

template <typename U, typename G, typename S>
void LuaSetDocProperty(sol::usertype<U> &table, std::string_view key, const char *type, const char *doc, G &&getter, S &&setter)
{
	table.set(key, sol::property(std::forward<G>(getter), std::forward<S>(setter)));
	table.set(LuaSignatureKey(key), sol::var(type));
	table.set(LuaDocstringKey(key), sol::var(doc));
	table.set(LuaUserdataMemberTypeKey(key), sol::var(static_cast<uint8_t>(LuaUserdataMemberType::Property)));
}

template <typename U, typename F>
void LuaSetDocProperty(sol::usertype<U> &table, std::string_view key, const char *type, const char *doc, F U::*&&value)
{
	table.set(key, value);
	table.set(LuaSignatureKey(key), sol::var(type));
	table.set(LuaDocstringKey(key), sol::var(doc));
	table.set(LuaUserdataMemberTypeKey(key), sol::var(static_cast<uint8_t>(LuaUserdataMemberType::Property)));
}

template <typename T>
void LuaSetDoc(sol::table &table, std::string_view key, const char *signature, const char *doc, T &&value)
{
	table.set(key, std::forward<T>(value));
	table.set(LuaSignatureKey(key), signature);
	table.set(LuaDocstringKey(key), doc);
}

template <typename T>
void LuaSetDocFn(sol::table &table, std::string_view key, const char *signature, const char *doc, T &&value)
{
	table.set_function(key, std::forward<T>(value));
	table.set(LuaSignatureKey(key), signature);
	table.set(LuaDocstringKey(key), doc);
}

template <typename T>
void LuaSetDocFn(sol::table &table, std::string_view key, std::string_view signature, T &&value)
{
	table.set_function(key, std::forward<T>(value));
	table.set(LuaSignatureKey(key), signature);
}

inline std::optional<std::string> GetSignature(const sol::table &table, std::string_view key)
{
	return table.get<std::optional<std::string>>(LuaSignatureKey(key));
}

inline std::optional<std::string> GetDocstring(const sol::table &table, std::string_view key)
{
	return table.get<std::optional<std::string>>(LuaDocstringKey(key));
}

inline std::optional<std::string> GetLuaUserdataSignature(const sol::userdata &obj, std::string_view key)
{
	return obj.get<std::optional<std::string>>(LuaSignatureKey(key));
}

inline std::optional<std::string> GetLuaUserdataDocstring(const sol::userdata &obj, std::string_view key)
{
	return obj.get<std::optional<std::string>>(LuaDocstringKey(key));
}

inline std::optional<LuaUserdataMemberType> GetLuaUserdataMemberType(const sol::userdata &obj, std::string_view key, const sol::object &value)
{
	std::optional<uint8_t> result = obj.get<std::optional<uint8_t>>(LuaUserdataMemberTypeKey(key));
	if (!result.has_value()) {
		if (value.get_type() == sol::type::userdata) return LuaUserdataMemberType::Property;
		if (value.get_type() == sol::type::function && key == "new") return LuaUserdataMemberType::Constructor;
		return std::nullopt;
	}
	return static_cast<LuaUserdataMemberType>(*result);
}

} // namespace devilution
