#include "parse_sfx_id.hpp"

#include <magic_enum/magic_enum.hpp>

namespace devilution {

tl::expected<SfxID, std::string> ParseSfxId(std::string_view value)
{
	const std::optional<SfxID> enumValueOpt = magic_enum::enum_cast<SfxID>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value.");
}

} // namespace devilution
