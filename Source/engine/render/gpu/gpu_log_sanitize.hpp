// Written by VTSTech (Nigel Todman) (veritas@vts-tech.org)
// AI Assisted by Super-Z (GLM-5.3-Flash)
// License: MIT

#pragma once

#include <string>
#include <string_view>

namespace devilution::gpu_scaler {

/** @brief Neutralizes control characters in text destined for gpu-scaler.log.
 *
 * Bytes < 0x20 (including \n, \r, \t and the ESC that starts terminal escape
 * sequences) and 0x7F (DEL) are replaced with '?'. Everything else passes
 * through unchanged. Untrusted content — preset file names, `#pragma`
 * descriptions, glslang info-log text — must go through this before being
 * written, so a hostile preset cannot forge log lines (embedded newlines)
 * or inject terminal escapes into the field-diagnosis record. `%`-signs are
 * deliberately untouched: callers pass text as arguments, never as the
 * format string. */
inline std::string SanitizeLogLine(std::string_view line)
{
	std::string out;
	out.reserve(line.size());
	for (const char c : line) {
		const auto uc = static_cast<unsigned char>(c);
		out.push_back((uc < 0x20 || uc == 0x7F) ? '?' : c);
	}
	return out;
}

} // namespace devilution::gpu_scaler
