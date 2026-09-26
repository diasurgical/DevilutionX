#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

#include "utils/attributes.h"

namespace devilution {

namespace fixed_point_detail {

// Picks a signed integer type at least twice as wide as `T`, keyed by size rather than by exact
// type (e.g. `int` and `int32_t` are not always the same type, but should be treated the same way).
template <typename T, bool = (sizeof(T) <= sizeof(std::int16_t))>
struct DoubleWidthImpl {
	using Type = std::int32_t;
};

template <typename T>
struct DoubleWidthImpl<T, false> {
	using Type = std::int64_t;
};

template <typename T>
using DoubleWidth = typename DoubleWidthImpl<T>::Type;

} // namespace fixed_point_detail

/**
 * @brief A fixed point number with `FractionalBits` fractional bits, backed by `StorageT`.
 *
 * Used throughout the engine for values such as hit points and mana, where the
 * fractional part allows regeneration/damage to accumulate sub-point precision.
 *
 * Arithmetic on `FixedPoint` mimics C's usual arithmetic conversions: combining two
 * `FixedPoint`s (or a `FixedPoint` and an `int`) with `+`, `-`, `*`, or `/` promotes to
 * whatever storage type the equivalent expression on the raw storage types would have
 * (e.g. `Fixed10_6 + Fixed10_6` is backed by `int16_t + int16_t`, i.e. `int`, exactly like
 * adding two `int16_t` values promotes to `int`). Compound assignment (`+=`, `*=`, ...)
 * instead keeps the left-hand side's type, narrowing the promoted result back down, just
 * like `int16_t x; x += y;` does in C.
 */
template <typename StorageT, unsigned FractionalBits>
class FixedPoint {
public:
	using StorageType = StorageT;
	static constexpr unsigned FractionalBitsV = FractionalBits;

	FixedPoint() = default;

	/**
	 * @brief Converts a fixed point value with the same fractional bits but a different storage type.
	 *
	 * Widening (or same-size) conversions are implicit, mirroring how `int16_t` converts to
	 * `int32_t` implicitly in C. Narrowing conversions must be requested explicitly.
	 */
	template <typename OtherStorageT>
	DVL_ALWAYS_INLINE explicit(sizeof(OtherStorageT) > sizeof(StorageT)) constexpr FixedPoint(FixedPoint<OtherStorageT, FractionalBits> other)
	    : raw_(static_cast<StorageT>(other.raw()))
	{
	}

	[[nodiscard]] DVL_ALWAYS_INLINE static constexpr FixedPoint fromRaw(StorageT raw)
	{
		FixedPoint result;
		result.raw_ = raw;
		return result;
	}

	[[nodiscard]] DVL_ALWAYS_INLINE static constexpr FixedPoint fromInt(int whole)
	{
		return fromRaw(static_cast<StorageT>(whole << FractionalBits));
	}

	[[nodiscard]] DVL_ALWAYS_INLINE constexpr StorageT raw() const
	{
		return raw_;
	}

	[[nodiscard]] DVL_ALWAYS_INLINE constexpr StorageT whole() const
	{
		return static_cast<StorageT>(raw_ >> FractionalBits);
	}

	[[nodiscard]] DVL_ALWAYS_INLINE constexpr StorageT fractional() const
	{
		return static_cast<StorageT>(raw_ & ((1 << FractionalBits) - 1));
	}

	/**
	 * @brief Adds another fixed point value, narrowing the (possibly promoted) result back to this type.
	 */
	template <typename OtherStorageT>
	DVL_ALWAYS_INLINE constexpr FixedPoint &operator+=(FixedPoint<OtherStorageT, FractionalBits> other)
	{
		raw_ = static_cast<StorageT>(raw_ + other.raw());
		return *this;
	}

	template <typename OtherStorageT>
	DVL_ALWAYS_INLINE constexpr FixedPoint &operator-=(FixedPoint<OtherStorageT, FractionalBits> other)
	{
		raw_ = static_cast<StorageT>(raw_ - other.raw());
		return *this;
	}

	DVL_ALWAYS_INLINE constexpr FixedPoint &operator*=(int factor)
	{
		raw_ = static_cast<StorageT>(raw_ * factor);
		return *this;
	}

	DVL_ALWAYS_INLINE constexpr FixedPoint &operator/=(int factor)
	{
		raw_ = static_cast<StorageT>(raw_ / factor);
		return *this;
	}

	/**
	 * @brief Multiplies by another fixed point value, rescaling the result and narrowing it back to this type.
	 */
	template <typename OtherStorageT>
	DVL_ALWAYS_INLINE constexpr FixedPoint &operator*=(FixedPoint<OtherStorageT, FractionalBits> factor)
	{
		using Promoted = decltype(raw_ * factor.raw());
		using Wide = fixed_point_detail::DoubleWidth<Promoted>;
		const Wide product = static_cast<Wide>(raw_) * static_cast<Wide>(factor.raw());
		raw_ = static_cast<StorageT>(product >> FractionalBits);
		return *this;
	}

	template <typename OtherStorageT>
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator==(FixedPoint<OtherStorageT, FractionalBits> other) const { return raw_ == other.raw(); }
	template <typename OtherStorageT>
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator!=(FixedPoint<OtherStorageT, FractionalBits> other) const { return raw_ != other.raw(); }
	template <typename OtherStorageT>
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator<(FixedPoint<OtherStorageT, FractionalBits> other) const { return raw_ < other.raw(); }
	template <typename OtherStorageT>
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator<=(FixedPoint<OtherStorageT, FractionalBits> other) const { return raw_ <= other.raw(); }
	template <typename OtherStorageT>
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator>(FixedPoint<OtherStorageT, FractionalBits> other) const { return raw_ > other.raw(); }
	template <typename OtherStorageT>
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator>=(FixedPoint<OtherStorageT, FractionalBits> other) const { return raw_ >= other.raw(); }

private:
	StorageT raw_;
};

template <typename StorageT1, typename StorageT2, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr auto operator+(FixedPoint<StorageT1, FractionalBits> a, FixedPoint<StorageT2, FractionalBits> b)
{
	auto raw = a.raw() + b.raw();
	return FixedPoint<decltype(raw), FractionalBits>::fromRaw(raw);
}

template <typename StorageT1, typename StorageT2, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr auto operator-(FixedPoint<StorageT1, FractionalBits> a, FixedPoint<StorageT2, FractionalBits> b)
{
	auto raw = a.raw() - b.raw();
	return FixedPoint<decltype(raw), FractionalBits>::fromRaw(raw);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr auto operator*(FixedPoint<StorageT, FractionalBits> a, int factor)
{
	auto raw = a.raw() * factor;
	return FixedPoint<decltype(raw), FractionalBits>::fromRaw(raw);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr auto operator*(int factor, FixedPoint<StorageT, FractionalBits> a)
{
	return a * factor;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr auto operator/(FixedPoint<StorageT, FractionalBits> a, int factor)
{
	auto raw = a.raw() / factor;
	return FixedPoint<decltype(raw), FractionalBits>::fromRaw(raw);
}

/**
 * @brief Multiplies two fixed point values, rescaling the result.
 *
 * The result is backed by whatever storage type multiplying the two raw storage types would
 * naturally promote to (see the class documentation), computed using an even wider intermediate
 * so the pre-rescale product itself doesn't overflow.
 */
template <typename StorageT1, typename StorageT2, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr auto operator*(FixedPoint<StorageT1, FractionalBits> a, FixedPoint<StorageT2, FractionalBits> b)
{
	using Promoted = decltype(a.raw() * b.raw());
	using Wide = fixed_point_detail::DoubleWidth<Promoted>;
	const Wide product = static_cast<Wide>(a.raw()) * static_cast<Wide>(b.raw());
	return FixedPoint<Promoted, FractionalBits>::fromRaw(static_cast<Promoted>(product >> FractionalBits));
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr auto operator-(FixedPoint<StorageT, FractionalBits> a)
{
	auto raw = -a.raw();
	return FixedPoint<decltype(raw), FractionalBits>::fromRaw(raw);
}

/**
 * @brief Compares against a plain `int`, treated as a whole number of units (as if by `fromInt`).
 */
template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator==(FixedPoint<StorageT, FractionalBits> a, int whole)
{
	return a == FixedPoint<StorageT, FractionalBits>::fromInt(whole);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator==(int whole, FixedPoint<StorageT, FractionalBits> a)
{
	return a == whole;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator!=(FixedPoint<StorageT, FractionalBits> a, int whole)
{
	return !(a == whole);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator!=(int whole, FixedPoint<StorageT, FractionalBits> a)
{
	return !(a == whole);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator<(FixedPoint<StorageT, FractionalBits> a, int whole)
{
	return a < FixedPoint<StorageT, FractionalBits>::fromInt(whole);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator<(int whole, FixedPoint<StorageT, FractionalBits> a)
{
	return a > whole;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator<=(FixedPoint<StorageT, FractionalBits> a, int whole)
{
	return a <= FixedPoint<StorageT, FractionalBits>::fromInt(whole);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator<=(int whole, FixedPoint<StorageT, FractionalBits> a)
{
	return a >= whole;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator>(FixedPoint<StorageT, FractionalBits> a, int whole)
{
	return a > FixedPoint<StorageT, FractionalBits>::fromInt(whole);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator>(int whole, FixedPoint<StorageT, FractionalBits> a)
{
	return a < whole;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator>=(FixedPoint<StorageT, FractionalBits> a, int whole)
{
	return a >= FixedPoint<StorageT, FractionalBits>::fromInt(whole);
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator>=(int whole, FixedPoint<StorageT, FractionalBits> a)
{
	return a <= whole;
}

using Fixed10_6 = FixedPoint<int16_t, 6>;
using Fixed26_6 = FixedPoint<int32_t, 6>;

template <typename T>
concept FixedPointType = std::same_as<T, FixedPoint<typename T::StorageType, T::FractionalBitsV>>;

template <typename T>
concept Fixed6Type = FixedPointType<T> && T::FractionalBitsV == 6;

} // namespace devilution
