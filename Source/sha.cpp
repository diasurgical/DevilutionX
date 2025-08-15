#include "sha.h"

#include <cstdint>
#include <cstring>

namespace devilution {

// NOTE: Diablo's "SHA1" is different from actual SHA1 in that it uses arithmetic
// right shifts (sign bit extension).

namespace {

/**
 * Diablo-"SHA1" circular left shift, portable version.
 */
uint32_t SHA1CircularShift(uint32_t word, size_t bits)
{
	// The SHA-like algorithm as originally implemented treated word as a signed value and used arithmetic right shifts
	//  (sign-extending). This results in the high 32-`bits` bits being set to 1.
	if ((word & (1 << 31)) != 0)
		return (0xFFFFFFFF << bits) | (word >> (32 - bits));
	return (word << bits) | (word >> (32 - bits));
}

void SHA1ProcessMessageBlock(SHA1Context *context)
{
	std::uint32_t w[80];

	memcpy(w, context->buffer, BlockSize * sizeof(uint32_t));
	for (int i = 16; i < 80; i++) {
		w[i] = w[i - 16] ^ w[i - 14] ^ w[i - 8] ^ w[i - 3];
	}

	std::uint32_t a = context->state[0];
	std::uint32_t b = context->state[1];
	std::uint32_t c = context->state[2];
	std::uint32_t d = context->state[3];
	std::uint32_t e = context->state[4];

	for (int i = 0; i < 20; i++) {
		const std::uint32_t temp = SHA1CircularShift(a, 5) + ((b & c) | ((~b) & d)) + e + w[i] + 0x5A827999;
		e = d;
		d = c;
		c = SHA1CircularShift(b, 30);
		b = a;
		a = temp;
	}

	for (int i = 20; i < 40; i++) {
		const std::uint32_t temp = SHA1CircularShift(a, 5) + (b ^ c ^ d) + e + w[i] + 0x6ED9EBA1;
		e = d;
		d = c;
		c = SHA1CircularShift(b, 30);
		b = a;
		a = temp;
	}

	for (int i = 40; i < 60; i++) {
		const std::uint32_t temp = SHA1CircularShift(a, 5) + ((b & c) | (b & d) | (c & d)) + e + w[i] + 0x8F1BBCDC;
		e = d;
		d = c;
		c = SHA1CircularShift(b, 30);
		b = a;
		a = temp;
	}

	for (int i = 60; i < 80; i++) {
		const std::uint32_t temp = SHA1CircularShift(a, 5) + (b ^ c ^ d) + e + w[i] + 0xCA62C1D6;
		e = d;
		d = c;
		c = SHA1CircularShift(b, 30);
		b = a;
		a = temp;
	}

	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
}

} // namespace

void SHA1Result(SHA1Context &context, uint32_t messageDigest[SHA1HashSize])
{
	memcpy(messageDigest, context.state, sizeof(context.state));
}

void SHA1Calculate(SHA1Context &context, const uint32_t data[BlockSize])
{
	memcpy(&context.buffer[0], data, BlockSize * sizeof(uint32_t));
	SHA1ProcessMessageBlock(&context);
}

} // namespace devilution
