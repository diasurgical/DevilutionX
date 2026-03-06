/**
 * @file dc_init.hpp
 * @brief Dreamcast-specific initialization API
 */
#pragma once

#ifdef __DREAMCAST__

namespace devilution {
namespace dc {

/**
 * Initialize Dreamcast subsystems (video, VMU, etc.)
 * Call from main() before game initialization.
 * @return true on success
 */
bool InitDreamcast();

/**
 * Shutdown Dreamcast subsystems.
 * Call before program exit.
 */
void ShutdownDreamcast();

/**
 * Check if VMU (Visual Memory Unit) is available for saves.
 * VMU is the Dreamcast's memory card, accessed via /vmu/[port][unit]/
 *
 * @return true if a VMU was detected
 */
bool IsVmuAvailable();

/**
 * Get the VMU filesystem path (e.g., "/vmu/a1/")
 * Only valid if IsVmuAvailable() returns true.
 *
 * @return Path string to VMU root
 */
const char *GetVmuPath();

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
