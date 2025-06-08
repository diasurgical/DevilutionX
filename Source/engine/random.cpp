#include "engine/random.hpp"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>

namespace devilution {

/** Current game seed */
uint32_t sglGameSeed;

/** Borland C/C++ psuedo-random number generator needed for vanilla compatibility */
std::linear_congruential_engine<uint32_t, 0x015A4E35, 1, 0> diabloGenerator;

/** Xoshiro pseudo-random number generator to provide less predictable seeds */
xoshiro128plusplus seedGenerator;

uint32_t xoshiro128plusplus::next()
{
	const uint32_t result = std::rotl(s[0] + s[3], 7) + s[0];

	const uint32_t t = s[1] << 9;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;

	s[3] = std::rotl(s[3], 11);

	return result;
}

uint64_t xoshiro128plusplus::timeSeed()
{
	auto now = std::chrono::system_clock::now();
	auto nano = std::chrono::nanoseconds(now.time_since_epoch());
	return static_cast<uint64_t>(nano.count());
}

void xoshiro128plusplus::copy(state &dst, const state &src)
{
	memcpy(dst, src, sizeof(dst));
}

xoshiro128plusplus ReserveSeedSequence(int initialSeed)
{
	if (initialSeed != -1)
		seedGenerator = xoshiro128plusplus((uint64_t)initialSeed);

	xoshiro128plusplus reserved = seedGenerator;
	seedGenerator.jump();
	return reserved;
}

uint32_t GenerateSeed()
{
	return seedGenerator.next();
}

void SetRndSeed(uint32_t seed)
{
	diabloGenerator.seed(seed);
	sglGameSeed = seed;
}

uint32_t GetLCGEngineState()
{
	return sglGameSeed;
}

void DiscardRandomValues(unsigned count)
{
	while (count != 0) {
		GenerateRandomNumber();
		count--;
	}
}

uint32_t GenerateRandomNumber()
{
	sglGameSeed = diabloGenerator();
	return sglGameSeed;
}

int32_t AdvanceRndSeed()
{
	const int32_t seed = static_cast<int32_t>(GenerateRandomNumber());
	// since abs(INT_MIN) is undefined behavior, handle this value specially
	return seed == std::numeric_limits<int32_t>::min() ? std::numeric_limits<int32_t>::min() : std::abs(seed);
}

int32_t GenerateRnd(int32_t v)
{
	if (v <= 0)
		return 0;
	if (v <= 0x7FFF) // use the high bits to correct for LCG bias
		return (AdvanceRndSeed() >> 16) % v;
	return AdvanceRndSeed() % v;
}

bool FlipCoin(unsigned frequency)
{
	// Casting here because GenerateRnd takes a signed argument when it should take and yield unsigned.
	return GenerateRnd(static_cast<int32_t>(frequency)) == 0;
}

} // namespace devilution
