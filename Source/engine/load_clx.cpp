#include "engine/load_clx.hpp"

#include <cstddef>
#include <cstdint>

#include <expected.hpp>

#include "appfat.h"
#include "engine/assets.hpp"
#include "engine/load_file.hpp"

namespace devilution {

OptionalOwnedClxSpriteListOrSheet LoadOptionalClxListOrSheet(const char *path)
{
	AssetRef ref = FindAsset(path);
	if (!ref.ok())
		return std::nullopt;
	const size_t size = ref.size();
	std::unique_ptr<uint8_t[]> data { new uint8_t[size] };
	{
		AssetHandle handle = OpenAsset(std::move(ref));
		if (!handle.ok() || !handle.read(data.get(), size))
			return std::nullopt;
	}
	return OwnedClxSpriteListOrSheet::FromBuffer(std::move(data), size);
}

tl::expected<OwnedClxSpriteListOrSheet, std::string> LoadClxListOrSheetWithStatus(const char *path)
{
	size_t size;
	tl::expected<std::unique_ptr<uint8_t[]>, std::string> data = LoadFileInMemWithStatus<uint8_t>(path, &size);
	if (!data.has_value()) return tl::make_unexpected(std::move(data).error());
	return OwnedClxSpriteListOrSheet::FromBuffer(std::move(data).value(), size);
}

OwnedClxSpriteListOrSheet LoadClxListOrSheet(const char *path)
{
	tl::expected<OwnedClxSpriteListOrSheet, std::string> result = LoadClxListOrSheetWithStatus(path);
	if (!result.has_value()) app_fatal(result.error());
	return std::move(result).value();
}

} // namespace devilution
