#include "engine/load_cel.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#ifdef DEBUG_CEL_TO_CL2_SIZE
#include <iostream>
#endif

#include <expected.hpp>

#include "appfat.h"
#include "mpq/mpq_common.hpp"
#include "utils/status_macros.hpp"
#include "utils/str_cat.hpp"

#ifdef UNPACKED_MPQS
#include "engine/load_clx.hpp"
#else
#include "engine/load_file.hpp"
#include "utils/cel_to_clx.hpp"
#endif

namespace devilution {

tl::expected<OwnedClxSpriteListOrSheet, std::string> LoadCelListOrSheetWithStatus(const char *pszName, PointerOrValue<uint16_t> widthOrWidths)
{
	char path[MaxMpqPathSize];
	*BufCopy(path, pszName, DEVILUTIONX_CEL_EXT) = '\0';
#ifdef UNPACKED_MPQS
	return LoadClxListOrSheetWithStatus(path);
#else
	size_t size;
	ASSIGN_OR_RETURN(std::unique_ptr<uint8_t[]> data, LoadFileInMemWithStatus<uint8_t>(path, &size));
#ifdef DEBUG_CEL_TO_CL2_SIZE
	std::cout << path;
#endif
	return CelToClx(data.get(), size, widthOrWidths);
#endif
}

OwnedClxSpriteListOrSheet LoadCelListOrSheet(const char *pszName, PointerOrValue<uint16_t> widthOrWidths)
{
	tl::expected<OwnedClxSpriteListOrSheet, std::string> result = LoadCelListOrSheetWithStatus(pszName, widthOrWidths);
	if (DVL_PREDICT_FALSE(!result.has_value())) app_fatal(result.error());
	return std::move(result).value();
}

} // namespace devilution
