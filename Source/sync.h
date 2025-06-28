/**
 * @file sync.h
 *
 * Interface of functionality for syncing game state with other players.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "msg.h"
#include "player.h"

namespace devilution {

size_t sync_all_monsters(std::byte *pbBuf, size_t dwMaxLen);
size_t OnSyncData(const TSyncHeader &header, size_t maxCmdSize, const Player &player);
void sync_init();

} // namespace devilution
