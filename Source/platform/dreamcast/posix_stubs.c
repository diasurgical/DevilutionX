/**
 * @file posix_stubs.c
 * @brief POSIX function stubs for KOS/Dreamcast
 *
 * KOS's newlib is missing some POSIX functions that external libraries expect.
 * This file provides stub implementations.
 */

#ifdef __DREAMCAST__

#include <stdio.h>
#include <unistd.h>

// POSIX Thread Safety Stubs
// KOS doesn't implement these, but libfmt uses them.
// Since we are effectively single-threaded for I/O, empty stubs are safe.

/**
 * @brief Lock a stdio stream (stub - KOS is single-threaded by default)
 */
void flockfile(FILE *file)
{
	(void)file;
}

/**
 * @brief Unlock a stdio stream (stub - KOS is single-threaded by default)
 */
void funlockfile(FILE *file)
{
	(void)file;
}

#endif /* __DREAMCAST__ */
