#pragma once

#include <expected>
#include <string>

#include "clx_sprite.hpp"
#include "utils/status_macros.hpp"

namespace devilution {

OwnedClxSpriteListOrSheet LoadClxListOrSheet(const char *path);

std::expected<OwnedClxSpriteListOrSheet, std::string> LoadClxListOrSheetWithStatus(const char *path);

inline OwnedClxSpriteList LoadClx(const char *path)
{
	return LoadClxListOrSheet(path).list();
}

inline std::expected<OwnedClxSpriteList, std::string> LoadClxWithStatus(const char *path)
{
	ASSIGN_OR_RETURN(OwnedClxSpriteListOrSheet result, LoadClxListOrSheetWithStatus(path));
	return std::move(result).list();
}

inline OwnedClxSpriteSheet LoadClxSheet(const char *path)
{
	return LoadClxListOrSheet(path).sheet();
}

OptionalOwnedClxSpriteListOrSheet LoadOptionalClxListOrSheet(const char *path);

inline OptionalOwnedClxSpriteList LoadOptionalClx(const char *path)
{
	OptionalOwnedClxSpriteListOrSheet result = LoadOptionalClxListOrSheet(path);
	if (!result)
		return std::nullopt;
	return std::move(*result).list();
}

} // namespace devilution
