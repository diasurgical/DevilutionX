/**
 * @file random.hpp
 *
 * Contains convenience functions for random number generation
 *
 * This includes specific engine/distribution functions for logic that needs to be compatible with the base game.
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <random>

namespace devilution {

class DiabloGenerator {
private:
	/** Borland C/C++ psuedo-random number generator needed for vanilla compatibility */
	std::linear_congruential_engine<uint32_t, 0x015A4E35, 1, 0> lcg;

public:
	/**
	 * @brief Set the state of the RandomNumberEngine used by the base game to the specific seed
	 * @param seed New engine state
	 */
	DiabloGenerator(uint32_t seed)
	{
		lcg.seed(seed);
	}

	/**
	 * @brief Advance the global RandomNumberEngine state by the specified number of rounds
	 *
	 * Only used to maintain vanilla compatibility until logic requiring reproducible random number generation is isolated.
	 * @param count How many values to discard
	 */
	void discardRandomValues(unsigned count)
	{
		lcg.discard(count);
	}

	/**
	 * @brief Generates a random non-negative integer (most of the time) using the vanilla RNG
	 *
	 * This advances the engine state then interprets the new engine state as a signed value and calls std::abs to try
	 * discard the high bit of the result. This usually returns a positive number but may very rarely return -2^31.
	 *
	 * This function is only used when the base game wants to store the seed used to generate an item or level, however
	 * as the returned value is transformed about 50% of values do not reflect the actual engine state. It would be more
	 * appropriate to use GetLCGEngineState() in these cases but that may break compatibility with the base game.
	 *
	 * @return A random number in the range [0,2^31) or -2^31
	 */
	int32_t advanceRndSeed()
	{
		const int32_t seed = static_cast<int32_t>(lcg());
		// since abs(INT_MIN) is undefined behavior, handle this value specially
		return seed == std::numeric_limits<int32_t>::min() ? std::numeric_limits<int32_t>::min() : std::abs(seed);
	}

	/**
	 * @brief Generates a random integer less than the given limit using the vanilla RNG
	 *
	 * If v is not a positive number this function returns 0 without calling the RNG.
	 *
	 * Limits between 32768 and 65534 should be avoided as a bug in vanilla means this function always returns a value
	 * less than 32768 for limits in that range.
	 *
	 * This can very rarely return a negative value in the range (-v, -1] due to the bug in AdvanceRndSeed()
	 *
	 * @see AdvanceRndSeed()
	 * @param v The upper limit for the return value
	 * @return A random number in the range [0, v) or rarely a negative value in (-v, -1]
	 */
	int32_t generateRnd(int32_t v)
	{
		if (v <= 0)
			return 0;
		if (v <= 0x7FFF) // use the high bits to correct for LCG bias
			return (advanceRndSeed() >> 16) % v;
		return advanceRndSeed() % v;
	}

	/**
	 * @brief Generates a random boolean value using the vanilla RNG
	 *
	 * This function returns true 1 in `frequency` of the time, otherwise false. For example the default frequency of 2
	 * represents a 50/50 chance.
	 *
	 * @param frequency odds of returning a true value
	 * @return A random boolean value
	 */
	bool flipCoin(unsigned frequency)
	{
		// Casting here because GenerateRnd takes a signed argument when it should take and yield unsigned.
		return generateRnd(static_cast<int32_t>(frequency)) == 0;
	}

	/**
	 * @brief Picks one of the elements in the list randomly.
	 *
	 * @param values The values to pick from
	 * @return A random value from the 'values' list.
	 */
	template <typename T>
	const T pickRandomlyAmong(const std::initializer_list<T> &values)
	{
		const auto index { std::max<int32_t>(generateRnd(static_cast<int32_t>(values.size())), 0) };

		return *(values.begin() + index);
	}

	/**
	 * @brief Generates a random non-negative integer
	 *
	 * Effectively the same as GenerateRnd but will never return a negative value
	 * @param v upper limit for the return value
	 * @return a value between 0 and v-1 inclusive, i.e. the range [0, v)
	 */
	inline int32_t randomIntLessThan(int32_t v)
	{
		return std::max<int32_t>(generateRnd(v), 0);
	}

	/**
	 * @brief Randomly chooses a value somewhere within the given range
	 * @param min lower limit, minimum possible value
	 * @param max upper limit, either the maximum possible value for a closed range (the default behaviour) or one greater than the maximum value for a half-open range
	 * @param halfOpen whether to use the limits as a half-open range or not
	 * @return a randomly selected integer
	 */
	inline int32_t randomIntBetween(int32_t min, int32_t max, bool halfOpen = false)
	{
		return randomIntLessThan(max - min + (halfOpen ? 0 : 1)) + min;
	}
};

// Based on fmix32 implementation from MurmurHash3 created by Austin Appleby in 2008
// https://github.com/aappleby/smhasher/blob/61a0530f28277f2e850bfc39600ce61d02b518de/src/MurmurHash3.cpp#L68
// and adapted from https://prng.di.unimi.it/splitmix64.c written in 2015 by Sebastiano Vigna
//
// See also:
//  Guy L. Steele, Doug Lea, and Christine H. Flood. 2014.
//  Fast splittable pseudorandom number generators. SIGPLAN Not. 49, 10 (October 2014), 453–472.
//  https://doi.org/10.1145/2714064.2660195
class SplitMix32 {
	uint32_t state;

public:
	SplitMix32(uint32_t state)
	    : state(state)
	{
	}

	uint32_t next()
	{
		uint32_t z = (state += 0x9e3779b9);
		z = (z ^ (z >> 16)) * 0x85ebca6b;
		z = (z ^ (z >> 13)) * 0xc2b2ae35;
		return z ^ (z >> 16);
	}

	void generate(uint32_t *begin, const uint32_t *end)
	{
		while (begin != end) {
			*begin = next();
			++begin;
		}
	}
};

// Adapted from https://prng.di.unimi.it/splitmix64.c written in 2015 by Sebastiano Vigna
//
// See also:
//  Guy L. Steele, Doug Lea, and Christine H. Flood. 2014.
//  Fast splittable pseudorandom number generators. SIGPLAN Not. 49, 10 (October 2014), 453–472.
//  https://doi.org/10.1145/2714064.2660195
class SplitMix64 {
	uint64_t state;

public:
	SplitMix64(uint64_t state)
	    : state(state)
	{
	}

	uint64_t next()
	{
		uint64_t z = (state += 0x9e3779b97f4a7c15);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
		z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
		return z ^ (z >> 31);
	}

	void generate(uint64_t *begin, const uint64_t *end)
	{
		while (begin != end) {
			*begin = next();
			++begin;
		}
	}
};

/** Adapted from https://prng.di.unimi.it/xoshiro128plusplus.c written in 2019 by David Blackman and Sebastiano Vigna */
class xoshiro128plusplus {
public:
	typedef uint32_t state[4];

	xoshiro128plusplus() { seed(); }
	xoshiro128plusplus(const state &s) { copy(this->s, s); }
	xoshiro128plusplus(uint64_t initialSeed) { seed(initialSeed); }
	xoshiro128plusplus(uint32_t initialSeed) { seed(initialSeed); }

	uint32_t next();

	/* This is the jump function for the generator. It is equivalent
	   to 2^64 calls to next(); it can be used to generate 2^64
	   non-overlapping subsequences for parallel computations. */
	void jump()
	{
		static constexpr uint32_t JUMP[] = { 0x8764000b, 0xf542d2d3, 0x6fa035c3, 0x77f2db5b };

		uint32_t s0 = 0;
		uint32_t s1 = 0;
		uint32_t s2 = 0;
		uint32_t s3 = 0;
		for (const uint32_t entry : JUMP)
			for (int b = 0; b < 32; b++) {
				if (entry & UINT32_C(1) << b) {
					s0 ^= s[0];
					s1 ^= s[1];
					s2 ^= s[2];
					s3 ^= s[3];
				}
				next();
			}

		s[0] = s0;
		s[1] = s1;
		s[2] = s2;
		s[3] = s3;
	}

	void save(state &s) const
	{
		copy(s, this->s);
	}

private:
	state s;

	void seed(uint64_t value)
	{
		uint64_t seeds[2];
		SplitMix64 seedSequence { value };
		seedSequence.generate(seeds, seeds + 2);

		s[0] = static_cast<uint32_t>(seeds[0] >> 32);
		s[1] = static_cast<uint32_t>(seeds[0]);
		s[2] = static_cast<uint32_t>(seeds[1] >> 32);
		s[3] = static_cast<uint32_t>(seeds[1]);
	}

	void seed(uint32_t value)
	{
		SplitMix32 seedSequence { value };
		seedSequence.generate(s, s + 4);
	}

	void seed()
	{
		seed(timeSeed());

#if !(defined(WINVER) && WINVER <= 0x0500 && (!defined(_WIN32_WINNT) || _WIN32_WINNT == 0))
		static std::random_device rd;
		std::uniform_int_distribution<uint32_t> dist;
		for (uint32_t &cell : s)
			cell ^= dist(rd);
#endif
	}

	static uint64_t timeSeed();
	static void copy(state &dst, const state &src);
};

/**
 * @brief Returns a copy of the global seed generator and fast-forwards the global seed generator to avoid collisions
 */
xoshiro128plusplus ReserveSeedSequence();

/**
 * @brief Advances the global seed generator state and returns the new value
 */
uint32_t GenerateSeed();

/**
 * @brief Set the state of the RandomNumberEngine used by the base game to the specific seed
 * @param seed New engine state
 */
void SetRndSeed(uint32_t seed);

/**
 * @brief Returns the current state of the RandomNumberEngine used by the base game
 *
 * This is only exposed to allow for debugging vanilla code and testing. Using this engine for new code is discouraged
 * due to the poor randomness and bugs in the implementation that need to be retained for compatibility.
 *
 * @return The current engine state
 */
uint32_t GetLCGEngineState();

/**
 * @brief Advance the global RandomNumberEngine state by the specified number of rounds
 *
 * Only used to maintain vanilla compatibility until logic requiring reproducible random number generation is isolated.
 * @param count How many values to discard
 */
void DiscardRandomValues(unsigned count);

/**
 * @brief Advances the global RandomNumberEngine state and returns the new value
 */
uint32_t GenerateRandomNumber();

/**
 * @brief Generates a random non-negative integer (most of the time) using the vanilla RNG
 *
 * This advances the engine state then interprets the new engine state as a signed value and calls std::abs to try
 * discard the high bit of the result. This usually returns a positive number but may very rarely return -2^31.
 *
 * This function is only used when the base game wants to store the seed used to generate an item or level, however
 * as the returned value is transformed about 50% of values do not reflect the actual engine state. It would be more
 * appropriate to use GetLCGEngineState() in these cases but that may break compatibility with the base game.
 *
 * @return A random number in the range [0,2^31) or -2^31
 */
[[nodiscard]] int32_t AdvanceRndSeed();

/**
 * @brief Generates a random integer less than the given limit using the vanilla RNG
 *
 * If v is not a positive number this function returns 0 without calling the RNG.
 *
 * Limits between 32768 and 65534 should be avoided as a bug in vanilla means this function always returns a value
 * less than 32768 for limits in that range.
 *
 * This can very rarely return a negative value in the range (-v, -1] due to the bug in AdvanceRndSeed()
 *
 * @see AdvanceRndSeed()
 * @param v The upper limit for the return value
 * @return A random number in the range [0, v) or rarely a negative value in (-v, -1]
 */
int32_t GenerateRnd(int32_t v);

/**
 * @brief Generates a random boolean value using the vanilla RNG
 *
 * This function returns true 1 in `frequency` of the time, otherwise false. For example the default frequency of 2
 * represents a 50/50 chance.
 *
 * @param frequency odds of returning a true value
 * @return A random boolean value
 */
bool FlipCoin(unsigned frequency = 2);

/**
 * @brief Picks one of the elements in the list randomly.
 *
 * @param values The values to pick from
 * @return A random value from the 'values' list.
 */
template <typename T>
const T PickRandomlyAmong(const std::initializer_list<T> &values)
{
	const auto index { std::max<int32_t>(GenerateRnd(static_cast<int32_t>(values.size())), 0) };

	return *(values.begin() + index);
}

/**
 * @brief Generates a random non-negative integer
 *
 * Effectively the same as GenerateRnd but will never return a negative value
 * @param v upper limit for the return value
 * @return a value between 0 and v-1 inclusive, i.e. the range [0, v)
 */
inline int32_t RandomIntLessThan(int32_t v)
{
	return std::max<int32_t>(GenerateRnd(v), 0);
}

/**
 * @brief Randomly chooses a value somewhere within the given range
 * @param min lower limit, minimum possible value
 * @param max upper limit, either the maximum possible value for a closed range (the default behaviour) or one greater than the maximum value for a half-open range
 * @param halfOpen whether to use the limits as a half-open range or not
 * @return a randomly selected integer
 */
inline int32_t RandomIntBetween(int32_t min, int32_t max, bool halfOpen = false)
{
	return RandomIntLessThan(max - min + (halfOpen ? 0 : 1)) + min;
}

} // namespace devilution
