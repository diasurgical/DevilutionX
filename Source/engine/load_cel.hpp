#pragma once

#include <cstdint>
#include <string>

#include <expected.hpp>

#include "engine/clx_sprite.hpp"
#include "utils/pointer_value_union.hpp"
#include "utils/status_macros.hpp"

#ifdef UNPACKED_MPQS
#define DEVILUTIONX_CEL_EXT ".clx"
#else
#define DEVILUTIONX_CEL_EXT ".cel"
#endif

namespace devilution {

tl::expected<OwnedClxSpriteListOrSheet, std::string> LoadCelListOrSheetWithStatus(const char *pszName, PointerOrValue<uint16_t> widthOrWidths);

OwnedClxSpriteListOrSheet LoadCelListOrSheet(const char *pszName, PointerOrValue<uint16_t> widthOrWidths);

inline OwnedClxSpriteList LoadCel(const char *pszName, uint16_t width)
{
	return LoadCelListOrSheet(pszName, PointerOrValue<uint16_t> { width }).list();
}

inline OptionalOwnedClxSpriteList LoadOptionalCel(const char *pszName, uint16_t width)
{
	tl::expected<OwnedClxSpriteListOrSheet, std::string> result = LoadCelListOrSheetWithStatus(pszName, PointerOrValue<uint16_t> { width });
	if (!result.has_value())
		return std::nullopt;
	return (*std::move(result)).list();
}

inline tl::expected<OwnedClxSpriteList, std::string> LoadCelWithStatus(const char *pszName, uint16_t width)
{
	ASSIGN_OR_RETURN(OwnedClxSpriteListOrSheet result, LoadCelListOrSheetWithStatus(pszName, PointerOrValue<uint16_t> { width }));
	return std::move(result).list();
}

inline OwnedClxSpriteList LoadCel(const char *pszName, const uint16_t *widths)
{
	return LoadCelListOrSheet(pszName, PointerOrValue<uint16_t> { widths }).list();
}

inline OptionalOwnedClxSpriteList LoadOptionalCel(const char *pszName, const uint16_t *widths)
{
	tl::expected<OwnedClxSpriteListOrSheet, std::string> result = LoadCelListOrSheetWithStatus(pszName, PointerOrValue<uint16_t> { widths });
	if (!result.has_value())
		return std::nullopt;
	return (*std::move(result)).list();
}

inline tl::expected<OwnedClxSpriteList, std::string> LoadCelWithStatus(const char *pszName, const uint16_t *widths)
{
	ASSIGN_OR_RETURN(OwnedClxSpriteListOrSheet result, LoadCelListOrSheetWithStatus(pszName, PointerOrValue<uint16_t> { widths }));
	return std::move(result).list();
}

inline OwnedClxSpriteSheet LoadCelSheet(const char *pszName, uint16_t width)
{
	return LoadCelListOrSheet(pszName, PointerOrValue<uint16_t> { width }).sheet();
}

} // namespace devilution
