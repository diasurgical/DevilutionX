#include "actor_position.hpp"

#include <array>
#include <cstdint>

namespace devilution {

namespace {

enum class VelocityToUse : uint8_t {
	None,
	Full,
	NegativeFull,
	Half,
	NegativeHalf,
	Quarter,
	NegativeQuarter,
};

struct RoundedWalkVelocity {
	int16_t quarter;
	int16_t half;
	int16_t full;

	int16_t getVelocity(VelocityToUse velocityToUse) const
	{
		switch (velocityToUse) {
		case VelocityToUse::Quarter:
			return quarter;
		case VelocityToUse::NegativeQuarter:
			return -quarter;
		case VelocityToUse::Half:
			return half;
		case VelocityToUse::NegativeHalf:
			return -half;
		case VelocityToUse::Full:
			return full;
		case VelocityToUse::NegativeFull:
			return -full;
		default:
			return 0;
		}
	}
};

/** @brief Maps from walk animation length to monster velocity (longer/slower animation means less velocity). */
constexpr RoundedWalkVelocity WalkVelocityForFrames[24] = {
	// clang-format off
	// Quarter, Half, Full
	{ 256, 512, 1024 },
	{ 128, 256,  512 },
	{  85, 170,  341 },
	{  64, 128,  256 },
	{  51, 102,  204 },
	{  42,  85,  170 },
	{  36,  73,  146 },
	{  32,  64,  128 },
	{  28,  56,  113 },
	{  26,  51,  102 },
	{  23,  46,   93 },
	{  21,  42,   85 },
	{  19,  39,   78 },
	{  18,  36,   73 },
	{  17,  34,   68 },
	{  16,  32,   64 },
	{  15,  30,   60 },
	{  14,  28,   57 },
	{  13,  26,   54 },
	{  12,  25,   51 },
	{  12,  24,   48 },
	{  11,  23,   46 },
	{  11,  22,   44 },
	{  10,  21,   42 }
	// clang-format on
};

struct WalkParameter {
	VelocityToUse VelocityX;
	VelocityToUse VelocityY;
	DisplacementOf<int16_t> getVelocity(int8_t numberOfFrames) const
	{
		const RoundedWalkVelocity &walkVelocity = WalkVelocityForFrames[numberOfFrames - 1];
		auto velocity = DisplacementOf<int16_t> {
			walkVelocity.getVelocity(VelocityX),
			walkVelocity.getVelocity(VelocityY),
		};
		return velocity;
	}
};

constexpr std::array<const WalkParameter, 8> WalkParameters { {
	// clang-format off
	// Direction      VelocityX,                   VelocityY
	{ /* South     */ VelocityToUse::None,         VelocityToUse::Half },
	{ /* SouthWest */ VelocityToUse::NegativeHalf, VelocityToUse::Quarter },
	{ /* West      */ VelocityToUse::NegativeFull, VelocityToUse::None },
	{ /* NorthWest */ VelocityToUse::NegativeHalf, VelocityToUse::NegativeQuarter },
	{ /* North     */ VelocityToUse::None,         VelocityToUse::NegativeHalf },
	{ /* NorthEast */ VelocityToUse::Half,         VelocityToUse::NegativeQuarter },
	{ /* East      */ VelocityToUse::Full,         VelocityToUse::None },
	{ /* SouthEast */ VelocityToUse::Half,         VelocityToUse::Quarter }
	// clang-format on
} };

} // namespace

DisplacementOf<int8_t> ActorPosition::CalculateWalkingOffset(Direction dir, const AnimationInfo &animInfo) const
{
	const DisplacementOf<int16_t> offset = CalculateWalkingOffsetShifted4(dir, animInfo);
	return { static_cast<int8_t>(offset.deltaX >> 4), static_cast<int8_t>(offset.deltaY >> 4) };
}

DisplacementOf<int16_t> ActorPosition::CalculateWalkingOffsetShifted4(Direction dir, const AnimationInfo &animInfo) const
{
	const int16_t velocityProgress = static_cast<int16_t>(animInfo.getAnimationProgress()) * animInfo.numberOfFrames / AnimationInfo::baseValueFraction;
	const WalkParameter &walkParameter = WalkParameters[static_cast<size_t>(dir)];
	const DisplacementOf<int16_t> velocity = walkParameter.getVelocity(animInfo.numberOfFrames);
	DisplacementOf<int16_t> offset = (velocity * velocityProgress);
	return offset;
}

DisplacementOf<int16_t> ActorPosition::CalculateWalkingOffsetShifted8(Direction dir, const AnimationInfo &animInfo) const
{
	DisplacementOf<int16_t> offset = CalculateWalkingOffsetShifted4(dir, animInfo);
	offset.deltaX <<= 4;
	offset.deltaY <<= 4;
	return offset;
}

DisplacementOf<int16_t> ActorPosition::GetWalkingVelocityShifted4(Direction dir, const AnimationInfo &animInfo) const
{
	const WalkParameter &walkParameter = WalkParameters[static_cast<size_t>(dir)];
	return walkParameter.getVelocity(animInfo.numberOfFrames);
}

DisplacementOf<int16_t> ActorPosition::GetWalkingVelocityShifted8(Direction dir, const AnimationInfo &animInfo) const
{
	DisplacementOf<int16_t> velocity = GetWalkingVelocityShifted4(dir, animInfo);
	velocity.deltaX <<= 4;
	velocity.deltaY <<= 4;
	return velocity;
}

} // namespace devilution
