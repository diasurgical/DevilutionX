#pragma once
/**
 * @file load_shadow_fixes.hpp
 *
 * @brief Loads the shadow corrections of a sprite file (see `shadow_fix.hpp`).
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "engine/assets.hpp"
#include "engine/shadow_fix.hpp"
#include "mpq/mpq_common.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

/** @brief The contents of a shadow corrections file. Empty if the sprite file has none. */
struct ShadowFixData {
	std::unique_ptr<uint8_t[]> data;
	size_t size = 0;

	[[nodiscard]] operator std::span<const uint8_t>() const
	{
		return { data.get(), size };
	}
};

/**
 * @brief Loads the corrections stored next to a sprite file, if there are any.
 *
 * Most sprite files have none. The file is looked up like any other asset, so a mod can
 * provide corrections for its own sprites or replace the bundled ones.
 *
 * @param spritePath the path of the sprite file without its extension, e.g. `objects\cauldren`.
 */
inline ShadowFixData LoadShadowFixes(std::string_view spritePath)
{
	char path[MaxMpqPathSize];
	if (spritePath.size() + sizeof(ShadowFixExtension) > sizeof(path)) return {};
	*BufCopy(path, spritePath, ShadowFixExtension) = '\0';
	AssetRef ref = FindAsset(path);
	if (!ref.ok()) return {};
	const size_t size = ref.size();
	AssetHandle handle = OpenAsset(std::move(ref));
	if (!handle.ok()) return {};
	ShadowFixData result { std::unique_ptr<uint8_t[]> { new uint8_t[size] }, size };
	if (size > 0 && !handle.read(result.data.get(), size)) return {};
	return result;
}

} // namespace devilution
