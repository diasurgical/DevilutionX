#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace devilution {

constexpr size_t MaxMpqPathSize = 256;

using MpqFileHash = std::array<std::uint32_t, 3>;

#if !defined(UNPACKED_MPQS) || !defined(UNPACKED_SAVES)
MpqFileHash CalculateMpqFileHash(std::string_view filename);
#endif

} // namespace devilution