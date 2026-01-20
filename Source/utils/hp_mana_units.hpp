#pragma once

namespace devilution {

constexpr int HpManaFracBits = 6;
constexpr int HpManaScale = 1 << HpManaFracBits;

constexpr int HpManaToFrac(int whole)
{
	return whole * HpManaScale;
}

constexpr int HpManaToWhole(int frac)
{
	return frac / HpManaScale;
}

constexpr int HpManaFromParts(int whole, int frac)
{
	return HpManaToFrac(whole) + frac;
}

} // namespace devilution
