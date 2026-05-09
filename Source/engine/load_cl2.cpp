#include "engine/load_cl2.hpp"

#include <cstdint>
#include <memory>
#include <utility>

#include <expected.hpp>

#include "mpq/mpq_common.hpp"
#include "utils/status_macros.hpp"
#include "utils/str_cat.hpp"

#ifdef UNPACKED_MPQS
#include "engine/load_clx.hpp"
#else
#include "engine/load_file.hpp"
#include "utils/cl2_to_clx.hpp"
#endif

namespace devilution {

tl::expected<OwnedClxSpriteListOrSheet, std::string> LoadCl2ListOrSheetWithStatus(const char *pszName, PointerOrValue<uint16_t> widthOrWidths)
{
	char path[MaxMpqPathSize];
	*BufCopy(path, pszName, DEVILUTIONX_CL2_EXT) = '\0';
#ifdef UNPACKED_MPQS
	return LoadClxListOrSheetWithStatus(path);
#else
	size_t size;
	ASSIGN_OR_RETURN(std::unique_ptr<uint8_t[]> data, LoadFileInMemWithStatus<uint8_t>(path, &size));
	return Cl2ToClx(std::move(data), size, widthOrWidths);
#endif
}

OwnedClxSpriteListOrSheet LoadCl2ListOrSheet(const char *pszName, PointerOrValue<uint16_t> widthOrWidths)
{
	tl::expected<OwnedClxSpriteListOrSheet, std::string> result = LoadCl2ListOrSheetWithStatus(pszName, widthOrWidths);
	if (!result.has_value()) app_fatal(result.error());
	return std::move(result).value();
}

} // namespace devilution
