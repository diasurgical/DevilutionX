/**
 * @file local_coop_assets.hpp
 *
 * Asset management for local co-op HUD sprites and graphics.
 */
#pragma once

#include <optional>

#include "engine/clx_sprite.hpp"

namespace devilution {

/**
 * @brief Local co-op HUD sprite assets.
 *
 * This namespace provides access to all sprites used by the local co-op HUD,
 * including health/mana bars, panel backgrounds, and UI elements.
 */
namespace LocalCoopAssets {

/**
 * @brief Initialize local co-op HUD assets.
 *
 * Loads all sprites and creates color variants (red, blue, yellow) for bars.
 * Safe to call multiple times - will only initialize once.
 */
void Init();

/**
 * @brief Free local co-op HUD assets.
 *
 * Releases all loaded sprites to free memory.
 * Should be called during shutdown or when switching modes.
 */
void Free();

/**
 * @brief Check if assets are currently loaded.
 * @return true if assets have been initialized, false otherwise
 */
bool IsLoaded();

/**
 * @brief Get health box sprite (border around health bar).
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetHealthBox();

/**
 * @brief Get health bar sprite (red).
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetHealth();

/**
 * @brief Get health bar sprite (blue, for mana shield).
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetHealthBlue();

/**
 * @brief Get left end cap for panel boxes.
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetBoxLeft();

/**
 * @brief Get middle section for panel boxes.
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetBoxMiddle();

/**
 * @brief Get right end cap for panel boxes.
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetBoxRight();

/**
 * @brief Get character panel background sprite.
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetCharBg();

/**
 * @brief Get grayscale bar sprite (base sprite for colored variants).
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetBarSprite();

/**
 * @brief Get red bar sprite (for health bars).
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetBarSpriteRed();

/**
 * @brief Get blue bar sprite (for mana bars).
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetBarSpriteBlue();

/**
 * @brief Get yellow bar sprite (for experience bars).
 * @return Optional sprite, nullopt if not loaded
 */
const OptionalOwnedClxSpriteList &GetBarSpriteYellow();

} // namespace LocalCoopAssets

} // namespace devilution
