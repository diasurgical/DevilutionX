#pragma once
/**
 * @file clx_shadow.hpp
 *
 * @brief Separates the shadows baked into CLX sprites from the sprites themselves.
 */

#include <cstdint>
#include <optional>
#include <span>

#include "engine/clx_sprite.hpp"
#include "engine/shadow_mask.hpp"
#include "engine/shadow_rule.hpp"

namespace devilution {

/**
 * @brief Separates the shadows baked into the given sprites from the sprites.
 *
 * A shadow pixel is a black (palette index 0) pixel that lies where the shadow function
 * (see `shadow_rule.hpp`) casts a pixel of the sprite. Such pixels are removed from the sprite.
 * The shadow becomes the shadow mask of the sprite (see `shadow_mask.hpp`), stored in its frame header.
 *
 * The mask of a sprite is the whole shadow that it casts, including the part underneath the
 * sprite itself, limited to the pixels that are opaque in the input. Drawing a sprite over its
 * mask filled with black gives back the input exactly.
 *
 * Black pixels that are not explained as shadow are kept in the sprite.
 *
 * For sprite sheets, each list is treated as a separate animation (for `ShadowRuleType::FirstFrame`
 * the baseline comes from the first sprite of each list).
 *
 * @param fixes The contents of the corrections file of the sprites (see `shadow_fix.hpp`), empty if
 * there is none. Corrections are applied after the rule. Sprites whose rule does not describe a
 * shadow that can be separated (`!rule.canExtractShadow()`) keep all their pixels except for the
 * ones that the corrections mark as shadow. Corrections that do not match the sprites are ignored.
 *
 * @return The sprites without their shadows, with the same structure and dimensions as the input.
 * `std::nullopt` if there is nothing to extract (`!rule.canExtractShadow()` and no usable corrections)
 * or if a sprite is larger than `MaxShadowMaskSpriteWidth` by `MaxShadowMaskSpriteHeight`.
 */
std::optional<OwnedClxSpriteListOrSheet> ExtractClxShadows(ClxSpriteListOrSheet sprites, ShadowRule rule, std::span<const uint8_t> fixes = {});

/**
 * @brief Separates the shadows baked into the given sprites, replacing the sprites.
 *
 * Leaves the sprites as they are if there is nothing to extract (see `ExtractClxShadows`).
 */
inline void ExtractClxShadowsInPlace(OwnedClxSpriteList &sprites, ShadowRule rule, std::span<const uint8_t> fixes = {})
{
	std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(ClxSpriteListOrSheet { ClxSpriteList { sprites }.data(), 0 }, rule, fixes);
	if (result.has_value()) sprites = std::move(*result).list();
}

inline void ExtractClxShadowsInPlace(OwnedClxSpriteSheet &sprites, ShadowRule rule, std::span<const uint8_t> fixes = {})
{
	const ClxSpriteSheet sheet { sprites };
	std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(ClxSpriteListOrSheet { sheet.data(), sheet.numLists() }, rule, fixes);
	if (result.has_value()) sprites = std::move(*result).sheet();
}

} // namespace devilution
