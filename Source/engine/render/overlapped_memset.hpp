#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>

#include "utils/attributes.h"

namespace devilution {

/**
 * @brief Copies up to 32 bytes without invoking memcpy.
 *
 * Uses overlapping loads and stores to cover the buffer with two fixed-size
 * operations per size tier, reading all source bytes into temporaries before
 * any write so the overlap is safe.
 *
 * @note n > 32 leaves a gap in the middle uninitialized.
 */
DVL_ALWAYS_INLINE void CopyBytesUpTo32(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, int n)
{
	assert(n > 0);
	assert(n <= 32);
	if constexpr (sizeof(void *) == 8) {
		if (n >= 16) {
			uint64_t a, b, c, d;
			memcpy(&a, src, 8);
			memcpy(&b, src + 8, 8);
			memcpy(&c, src + n - 16, 8);
			memcpy(&d, src + n - 8, 8);
			memcpy(dst, &a, 8);
			memcpy(dst + 8, &b, 8);
			memcpy(dst + n - 16, &c, 8);
			memcpy(dst + n - 8, &d, 8);
			return;
		}
		if (n >= 8) {
			uint64_t a, b;
			memcpy(&a, src, 8);
			memcpy(&b, src + n - 8, 8);
			memcpy(dst, &a, 8);
			memcpy(dst + n - 8, &b, 8);
			return;
		}
	} else {
		if (n >= 16) {
			uint32_t a, b, c, d, e, f, g, h;
			memcpy(&a, src, 4);
			memcpy(&b, src + 4, 4);
			memcpy(&c, src + 8, 4);
			memcpy(&d, src + 12, 4);
			memcpy(&e, src + n - 16, 4);
			memcpy(&f, src + n - 12, 4);
			memcpy(&g, src + n - 8, 4);
			memcpy(&h, src + n - 4, 4);
			memcpy(dst, &a, 4);
			memcpy(dst + 4, &b, 4);
			memcpy(dst + 8, &c, 4);
			memcpy(dst + 12, &d, 4);
			memcpy(dst + n - 16, &e, 4);
			memcpy(dst + n - 12, &f, 4);
			memcpy(dst + n - 8, &g, 4);
			memcpy(dst + n - 4, &h, 4);
			return;
		}
		if (n >= 8) {
			uint32_t a, b, c, d;
			memcpy(&a, src, 4);
			memcpy(&b, src + 4, 4);
			memcpy(&c, src + n - 8, 4);
			memcpy(&d, src + n - 4, 4);
			memcpy(dst, &a, 4);
			memcpy(dst + 4, &b, 4);
			memcpy(dst + n - 8, &c, 4);
			memcpy(dst + n - 4, &d, 4);
			return;
		}
	}
	if (n >= 4) {
		uint32_t a, b;
		memcpy(&a, src, 4);
		memcpy(&b, src + n - 4, 4);
		memcpy(dst, &a, 4);
		memcpy(dst + n - 4, &b, 4);
		return;
	}
	dst[0] = src[0];
	if (n >= 2) {
		uint16_t a;
		memcpy(&a, src + n - 2, 2);
		memcpy(dst + n - 2, &a, 2);
	}
}

/**
 * @brief Fills a block of memory with a specified byte value without invoking memset.
 *
 * This function provides an ultra-fast, branch-optimized alternative to standard
 * memset for small buffers. By leveraging fixed-size, constant-expression memcpy
 * calls, it forces the compiler to emit inline, architectural store instructions
 * (e.g., movq, movl) directly, bypassing PLT/library overhead entirely.
 *
 * To minimize branching and completely eliminate loops, it utilizes an
 * "overlapping store" strategy (writing identical data from both ends of the buffer).
 *
 * @note CRITICAL: This function is strictly optimized and mathematically bounded
 * for buffers between 0 and 32 bytes. Passing a size greater than 32 will
 * leave an uninitialized memory gap in the middle of the destination buffer.
 *
 * @param dst   Pointer to the destination memory block.
 * @param n     The number of bytes to fill (Must be between 0 and 32 inclusive).
 * @param value The byte value to broadcast across the memory block.
 */
DVL_ALWAYS_INLINE void FillBytesUpTo32(uint8_t *dst, int n, uint8_t value)
{
	assert(n > 0);
	assert(n <= 32);
	if constexpr (sizeof(void *) == 8) {
		const uint64_t fill64 = static_cast<uint64_t>(value) * 0x0101010101010101ULL;
		if (n >= 16) {
			memcpy(dst, &fill64, 8);
			memcpy(dst + 8, &fill64, 8);
			memcpy(dst + n - 16, &fill64, 8);
			memcpy(dst + n - 8, &fill64, 8);
			return;
		}
		if (n >= 8) {
			memcpy(dst, &fill64, 8);
			memcpy(dst + n - 8, &fill64, 8);
			return;
		}
	} else {
		const uint32_t fill32 = static_cast<uint32_t>(value) * 0x01010101U;
		if (n >= 16) {
			memcpy(dst, &fill32, 4);
			memcpy(dst + 4, &fill32, 4);
			memcpy(dst + 8, &fill32, 4);
			memcpy(dst + 12, &fill32, 4);
			memcpy(dst + n - 16, &fill32, 4);
			memcpy(dst + n - 12, &fill32, 4);
			memcpy(dst + n - 8, &fill32, 4);
			memcpy(dst + n - 4, &fill32, 4);
			return;
		}
		if (n >= 8) {
			memcpy(dst, &fill32, 4);
			memcpy(dst + 4, &fill32, 4);
			memcpy(dst + n - 8, &fill32, 4);
			memcpy(dst + n - 4, &fill32, 4);
			return;
		}
	}

	const uint32_t fill32 = static_cast<uint32_t>(value) * 0x01010101U;
	if (n >= 4) {
		memcpy(dst, &fill32, 4);
		memcpy(dst + n - 4, &fill32, 4);
		return;
	}
	dst[0] = value;
	if (n >= 2) {
		const uint16_t fill16 = static_cast<uint16_t>(value) * 0x0101U;
		memcpy(dst + n - 2, &fill16, 2);
	}
}

/**
 * @brief Fills a block of memory with a specified byte value without invoking memset.
 *
 * This function provides an ultra-fast, branch-optimized alternative to standard
 * memset for small buffers. By leveraging fixed-size, constant-expression memcpy
 * calls, it forces the compiler to emit inline, architectural store instructions
 * (e.g., movq, movl) directly, bypassing PLT/library overhead entirely.
 *
 * To minimize branching and completely eliminate loops, it utilizes an
 * "overlapping store" strategy (writing identical data from both ends of the buffer).
 *
 * @note CRITICAL: This function is strictly optimized and mathematically bounded
 * for buffers between 0 and 64 bytes. Passing a size greater than 64 will
 * leave an uninitialized memory gap in the middle of the destination buffer.
 *
 * @param dst   Pointer to the destination memory block.
 * @param n     The number of bytes to fill (Must be between 0 and 64 inclusive).
 * @param value The byte value to broadcast across the memory block.
 */
DVL_ALWAYS_INLINE void FillBytesUpTo64(uint8_t *dst, int n, uint8_t value)
{
	assert(n > 0);
	assert(n <= 64);
	if constexpr (sizeof(void *) == 8) {
		const uint64_t fill64 = static_cast<uint64_t>(value) * 0x0101010101010101ULL;
		if (n >= 32) {
			memcpy(dst, &fill64, 8);
			memcpy(dst + 8, &fill64, 8);
			memcpy(dst + 16, &fill64, 8);
			memcpy(dst + 24, &fill64, 8);
			memcpy(dst + n - 32, &fill64, 8);
			memcpy(dst + n - 24, &fill64, 8);
			memcpy(dst + n - 16, &fill64, 8);
			memcpy(dst + n - 8, &fill64, 8);
			return;
		}
		if (n >= 16) {
			memcpy(dst, &fill64, 8);
			memcpy(dst + 8, &fill64, 8);
			memcpy(dst + n - 16, &fill64, 8);
			memcpy(dst + n - 8, &fill64, 8);
			return;
		}
		if (n >= 8) {
			memcpy(dst, &fill64, 8);
			memcpy(dst + n - 8, &fill64, 8);
			return;
		}
	} else {
		const uint32_t fill32 = static_cast<uint32_t>(value) * 0x01010101U;
		if (n >= 32) {
			memcpy(dst, &fill32, 4);
			memcpy(dst + 4, &fill32, 4);
			memcpy(dst + 8, &fill32, 4);
			memcpy(dst + 12, &fill32, 4);
			memcpy(dst + 16, &fill32, 4);
			memcpy(dst + 20, &fill32, 4);
			memcpy(dst + 24, &fill32, 4);
			memcpy(dst + 28, &fill32, 4);
			memcpy(dst + n - 32, &fill32, 4);
			memcpy(dst + n - 28, &fill32, 4);
			memcpy(dst + n - 24, &fill32, 4);
			memcpy(dst + n - 20, &fill32, 4);
			memcpy(dst + n - 16, &fill32, 4);
			memcpy(dst + n - 12, &fill32, 4);
			memcpy(dst + n - 8, &fill32, 4);
			memcpy(dst + n - 4, &fill32, 4);
			return;
		}
		if (n >= 16) {
			memcpy(dst, &fill32, 4);
			memcpy(dst + 4, &fill32, 4);
			memcpy(dst + 8, &fill32, 4);
			memcpy(dst + 12, &fill32, 4);
			memcpy(dst + n - 16, &fill32, 4);
			memcpy(dst + n - 12, &fill32, 4);
			memcpy(dst + n - 8, &fill32, 4);
			memcpy(dst + n - 4, &fill32, 4);
			return;
		}
		if (n >= 8) {
			memcpy(dst, &fill32, 4);
			memcpy(dst + 4, &fill32, 4);
			memcpy(dst + n - 8, &fill32, 4);
			memcpy(dst + n - 4, &fill32, 4);
			return;
		}
	}

	const uint32_t fill32 = static_cast<uint32_t>(value) * 0x01010101U;

	if (n >= 4) {
		memcpy(dst, &fill32, 4);
		memcpy(dst + n - 4, &fill32, 4);
		return;
	}

	dst[0] = value;
	if (n >= 2) {
		const uint16_t fill16 = static_cast<uint16_t>(value) * 0x0101U;
		memcpy(dst + n - 2, &fill16, 2);
	}
}

} // namespace devilution
