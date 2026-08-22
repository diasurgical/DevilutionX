#pragma once

namespace devilution {

/// Screen refresh interval in nanoseconds. Set by display initialization.
extern int refreshDelay;

/**
 * @brief CPU sleep-based frame limiter.
 *
 * Sleeps until the next frame deadline to maintain a consistent frame rate.
 * Only active when the frame rate control option is set to CPUSleep.
 * This is independent of vsync (which is renderer-owned).
 */
void LimitFrameRate();

} // namespace devilution
