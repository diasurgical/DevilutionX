#pragma once

#include <concepts>
#include <cstdint>

#include "utils/attributes.h"

namespace devilution {

namespace fixed_point_detail {

template <typename T>
struct DoubleWidthImpl;

template <>
struct DoubleWidthImpl<int16_t> {
	using Type = int32_t;
};

template <>
struct DoubleWidthImpl<int32_t> {
	using Type = int64_t;
};

template <typename T>
using DoubleWidth = typename DoubleWidthImpl<T>::Type;

} // namespace fixed_point_detail

/**
 * @brief A fixed point number with `FractionalBits` fractional bits, backed by `StorageT`.
 *
 * Used throughout the engine for values such as hit points and mana, where the
 * fractional part allows regeneration/damage to accumulate sub-point precision.
 */
template <typename StorageT, unsigned FractionalBits>
class FixedPoint {
public:
	using StorageType = StorageT;
	static constexpr unsigned FractionalBitsV = FractionalBits;

	FixedPoint() = default;

	/**
	 * @brief Widens (or narrows) a fixed point value with the same fractional bits but a different storage type.
	 */
	template <typename OtherStorageT>
	DVL_ALWAYS_INLINE explicit constexpr FixedPoint(FixedPoint<OtherStorageT, FractionalBits> other)
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

	DVL_ALWAYS_INLINE constexpr FixedPoint &operator+=(FixedPoint other)
	{
		raw_ = static_cast<StorageT>(raw_ + other.raw_);
		return *this;
	}

	DVL_ALWAYS_INLINE constexpr FixedPoint &operator-=(FixedPoint other)
	{
		raw_ = static_cast<StorageT>(raw_ - other.raw_);
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
	 * @brief Multiplies by another fixed point value of the same type, rescaling the result.
	 */
	DVL_ALWAYS_INLINE constexpr FixedPoint &operator*=(FixedPoint factor)
	{
		using Wide = fixed_point_detail::DoubleWidth<StorageT>;
		const Wide product = static_cast<Wide>(raw_) * static_cast<Wide>(factor.raw_);
		raw_ = static_cast<StorageT>(product >> FractionalBits);
		return *this;
	}

	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator==(FixedPoint other) const { return raw_ == other.raw_; }
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator!=(FixedPoint other) const { return raw_ != other.raw_; }
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator<(FixedPoint other) const { return raw_ < other.raw_; }
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator<=(FixedPoint other) const { return raw_ <= other.raw_; }
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator>(FixedPoint other) const { return raw_ > other.raw_; }
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool operator>=(FixedPoint other) const { return raw_ >= other.raw_; }

private:
	StorageT raw_;
};

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr FixedPoint<StorageT, FractionalBits> operator+(FixedPoint<StorageT, FractionalBits> a, FixedPoint<StorageT, FractionalBits> b)
{
	a += b;
	return a;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr FixedPoint<StorageT, FractionalBits> operator-(FixedPoint<StorageT, FractionalBits> a, FixedPoint<StorageT, FractionalBits> b)
{
	a -= b;
	return a;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr FixedPoint<StorageT, FractionalBits> operator*(FixedPoint<StorageT, FractionalBits> a, int factor)
{
	a *= factor;
	return a;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr FixedPoint<StorageT, FractionalBits> operator*(int factor, FixedPoint<StorageT, FractionalBits> a)
{
	a *= factor;
	return a;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr FixedPoint<StorageT, FractionalBits> operator/(FixedPoint<StorageT, FractionalBits> a, int factor)
{
	a /= factor;
	return a;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr FixedPoint<StorageT, FractionalBits> operator*(FixedPoint<StorageT, FractionalBits> a, FixedPoint<StorageT, FractionalBits> b)
{
	a *= b;
	return a;
}

template <typename StorageT, unsigned FractionalBits>
[[nodiscard]] DVL_ALWAYS_INLINE constexpr FixedPoint<StorageT, FractionalBits> operator-(FixedPoint<StorageT, FractionalBits> a)
{
	return FixedPoint<StorageT, FractionalBits>::fromRaw(static_cast<StorageT>(-a.raw()));
}

using Fixed10_6 = FixedPoint<int16_t, 6>;
using Fixed26_6 = FixedPoint<int32_t, 6>;

template <typename T>
concept FixedPointType = std::same_as<T, FixedPoint<typename T::StorageType, T::FractionalBitsV>>;

template <typename T>
concept Fixed6Type = FixedPointType<T> && T::FractionalBitsV == 6;

} // namespace devilution
