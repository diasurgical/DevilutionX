/**
 * @file enum_traits.h
 *
 * Base template for 'enum_traits' which allow us to access static information about an enum.
 */
#pragma once

#include <cstddef>
#include <type_traits>

#include "utils/attributes.h"

namespace devilution {

template <typename T>
struct enum_size {
	constexpr static const std::size_t value = static_cast<std::size_t>(T::LAST) + 1;
};

template <typename T>
class enum_values {
public:
	class Iterator {
		typename std::underlying_type<T>::type m_value;

	public:
		Iterator(typename std::underlying_type<T>::type value)
		    : m_value(value)
		{
		}

		[[nodiscard]] DVL_ALWAYS_INLINE const T operator*() const
		{
			return static_cast<T>(m_value);
		}

		DVL_ALWAYS_INLINE void operator++()
		{
			m_value++;
		}

		[[nodiscard]] DVL_ALWAYS_INLINE bool operator!=(Iterator rhs) const
		{
			return m_value != rhs.m_value;
		}
	};
};

template <typename T>
[[nodiscard]] DVL_ALWAYS_INLINE typename enum_values<T>::Iterator begin(enum_values<T>)
{
	return typename enum_values<T>::Iterator(static_cast<typename std::underlying_type<T>::type>(T::FIRST));
}

template <typename T>
[[nodiscard]] DVL_ALWAYS_INLINE typename enum_values<T>::Iterator end(enum_values<T>)
{
	return typename enum_values<T>::Iterator(static_cast<typename std::underlying_type<T>::type>(T::LAST) + 1);
}

template <typename>
struct is_flags_enum : std::false_type {
};

#define use_enum_as_flags(Type)                   \
	template <>                                   \
	struct is_flags_enum<Type> : std::true_type { \
	};

template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value && is_flags_enum<EnumType>::value, bool> = true>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr EnumType operator|(EnumType lhs, EnumType rhs)
{
	using T = std::underlying_type_t<EnumType>;
	return static_cast<EnumType>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value && is_flags_enum<EnumType>::value, bool> = true>
DVL_ALWAYS_INLINE constexpr EnumType operator|=(EnumType &lhs, EnumType rhs)
{
	lhs = lhs | rhs;
	return lhs;
}

template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value && is_flags_enum<EnumType>::value, bool> = true>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr EnumType operator&(EnumType lhs, EnumType rhs)
{
	using T = std::underlying_type_t<EnumType>;
	return static_cast<EnumType>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value && is_flags_enum<EnumType>::value, bool> = true>
DVL_ALWAYS_INLINE constexpr EnumType operator&=(EnumType &lhs, EnumType rhs)
{
	lhs = lhs & rhs;
	return lhs;
}

template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value && is_flags_enum<EnumType>::value, bool> = true>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr EnumType operator~(EnumType value)
{
	using T = std::underlying_type_t<EnumType>;
	return static_cast<EnumType>(~static_cast<T>(value));
}

template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value && is_flags_enum<EnumType>::value, bool> = true>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool HasAnyOf(EnumType lhs, EnumType test)
{
	return (lhs & test) != static_cast<EnumType>(0); // Some flags enums may not use a None value outside this check so we don't require an EnumType::None definition here.
}

template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value && is_flags_enum<EnumType>::value, bool> = true>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool HasAllOf(EnumType lhs, EnumType test)
{
	return (lhs & test) == test;
}

template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value && is_flags_enum<EnumType>::value, bool> = true>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool HasNoneOf(EnumType lhs, EnumType test)
{
	return !HasAnyOf(lhs, test);
}

} // namespace devilution
