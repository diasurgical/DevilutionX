#pragma once
/**
 * @file shadow_rule.hpp
 *
 * @brief Describes how the shadow baked into a sprite was generated.
 *
 * The original sprites have their shadow drawn into them as black (palette index 0) pixels.
 * Every shadow was cast by the same function, measured from a baseline row `B`:
 *
 *   v  = (B - y) / 3
 *   ys = B - 2 - v
 *   xs = x - (3 * v) / 4
 *
 * where `(x, y)` is a pixel of the sprite at or above the baseline, `(xs, ys)` is the shadow
 * pixel it casts, and both divisions are integer divisions.
 *
 * The sprites only differ in how the baseline was chosen. That choice is the shadow rule.
 */

#include <algorithm>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "utils/parse_int.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

enum class ShadowRuleType : uint8_t {
	/** The sprite has black pixels but no known rule reproduces them, or it was never analysed. */
	Unknown,
	/** The sprite has no shadow. */
	None,
	/** The baseline is the lowest opaque row of the first frame of the animation (per direction). */
	FirstFrame,
	/** The baseline is the lowest opaque row of each frame. */
	EachFrame,
	/**
	 * The sprite has a shadow, but it cannot be told apart from the black pixels of the sprite itself:
	 * part of the sprite is black and lies inside its own shadow. The shadow stays part of the sprite,
	 * unless the sprite file has shadow corrections (see `shadow_fix.hpp`) that say where it is.
	 */
	Baked,
};

/**
 * Notes on how well the shadow rule reproduces the shadow of the sprite.
 *
 * They do not change how the shadow is extracted.
 */
enum class ShadowRuleFlags : uint8_t {
	// clang-format off
	None        = 0,
	/** Some black pixels of the sprite are neither a shadow that the rule casts nor plausibly part of the sprite itself. */
	Unexplained = 1 << 0,
	/** The rule casts a shadow onto some pixels that are transparent in the sprite. */
	Stray       = 1 << 1,
	// clang-format on
};

[[nodiscard]] constexpr ShadowRuleFlags operator|(ShadowRuleFlags a, ShadowRuleFlags b)
{
	return static_cast<ShadowRuleFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

[[nodiscard]] constexpr bool HasAnyOf(ShadowRuleFlags flags, ShadowRuleFlags test)
{
	return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
}

struct ShadowRule {
	ShadowRuleType type = ShadowRuleType::Unknown;

	/**
	 * @brief Number of rows added to the baseline.
	 *
	 * Used by flying monsters, whose shadow is detached from the body. Only valid with `FirstFrame`.
	 */
	uint8_t baselineOffset = 0;

	ShadowRuleFlags flags = ShadowRuleFlags::None;

	constexpr bool operator==(const ShadowRule &other) const = default;

	/** @brief Whether the sprite has a shadow that can be separated from it. */
	[[nodiscard]] constexpr bool canExtractShadow() const
	{
		return type == ShadowRuleType::FirstFrame || type == ShadowRuleType::EachFrame;
	}
};

/**
 * @brief Parses a shadow rule as written in the TSV data files.
 *
 * A rule is one of: an empty string (same as `Unknown`), `Unknown`, `None`, `Baked`, `FirstFrame`,
 * `EachFrame`, and `FirstFrame+N` where `N` is the baseline offset (1 to 255).
 *
 * It can be followed by the tags `Unexplained` and `Stray` (see `ShadowRuleFlags`),
 * each preceded by `|`, e.g. `FirstFrame|Stray`.
 */
inline std::expected<ShadowRule, std::string> ParseShadowRule(std::string_view value)
{
	constexpr char Separator = '|';
	const size_t ruleEnd = std::min(value.find(Separator), value.size());
	const std::string_view ruleStr = value.substr(0, ruleEnd);

	ShadowRule result;
	constexpr std::string_view FirstFrame = "FirstFrame";
	if (ruleStr.empty() || ruleStr == "Unknown") {
		result.type = ShadowRuleType::Unknown;
	} else if (ruleStr == "None") {
		result.type = ShadowRuleType::None;
	} else if (ruleStr == "Baked") {
		result.type = ShadowRuleType::Baked;
	} else if (ruleStr == "EachFrame") {
		result.type = ShadowRuleType::EachFrame;
	} else if (ruleStr == FirstFrame) {
		result.type = ShadowRuleType::FirstFrame;
	} else if (ruleStr.starts_with(FirstFrame) && ruleStr.size() > FirstFrame.size() + 1 && ruleStr[FirstFrame.size()] == '+') {
		const std::string_view offsetStr = ruleStr.substr(FirstFrame.size() + 1);
		const char *end = nullptr;
		const ParseIntResult<uint8_t> offset = ParseInt<uint8_t>(offsetStr, 1, 255, &end);
		if (!offset.has_value() || end != offsetStr.data() + offsetStr.size())
			return std::unexpected(StrCat("Unknown shadow rule: ", ruleStr));
		result.type = ShadowRuleType::FirstFrame;
		result.baselineOffset = *offset;
	} else {
		return std::unexpected(StrCat("Unknown shadow rule: ", ruleStr));
	}

	for (size_t pos = ruleEnd; pos < value.size();) {
		const size_t tagBegin = pos + 1;
		const size_t tagEnd = std::min(value.find(Separator, tagBegin), value.size());
		const std::string_view tag = value.substr(tagBegin, tagEnd - tagBegin);
		if (tag == "Unexplained") {
			result.flags = result.flags | ShadowRuleFlags::Unexplained;
		} else if (tag == "Stray") {
			result.flags = result.flags | ShadowRuleFlags::Stray;
		} else {
			return std::unexpected(StrCat("Unknown shadow rule tag: ", tag));
		}
		pos = tagEnd;
	}
	return result;
}

} // namespace devilution
