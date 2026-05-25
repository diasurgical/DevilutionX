#include "levels/parse_dungeon_type.hpp"

namespace devilution {

tl::expected<dungeon_type, std::string> ParseDungeonType(std::string_view value)
{
	if (value.empty()) return DTYPE_NONE;
	if (value == "DTYPE_TOWN") return DTYPE_TOWN;
	if (value == "DTYPE_CATHEDRAL") return DTYPE_CATHEDRAL;
	if (value == "DTYPE_CATACOMBS") return DTYPE_CATACOMBS;
	if (value == "DTYPE_CAVES") return DTYPE_CAVES;
	if (value == "DTYPE_HELL") return DTYPE_HELL;
	if (value == "DTYPE_NEST") return DTYPE_NEST;
	if (value == "DTYPE_CRYPT") return DTYPE_CRYPT;
	return tl::make_unexpected("Unknown enum value");
}

} // namespace devilution
