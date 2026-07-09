#include "mods/mod_identity.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <picosha2.h>

#include "utils/file_util.h"
#include "utils/log.hpp"

namespace devilution {

std::vector<ModIdentifier> ActiveModIdentifiers;

namespace {

/**
 * @brief Hardcoded list of approved third-party mod hashes (design §7).
 *
 * Whitelisting is by hash only. A whitelisted mod's *update* needs a new entry per
 * release, because the hash covers the whole archive (identifiers are per-release by
 * construction). Never whitelist by name — a local `mods/<name>.mpq` can shadow a
 * built-in, so name matching would be trivially spoofable.
 */
constexpr std::array<std::array<uint8_t, 32>, 0> ApprovedModHashes = {};

} // namespace

void ClearModIdentifiers()
{
	ActiveModIdentifiers.clear();
}

bool ComputeFileSha256(const char *path, std::array<uint8_t, 32> &hashOut)
{
	FILE *file = OpenFile(path, "rb");
	if (file == nullptr)
		return false;

	picosha2::hash256_one_by_one hasher;
	std::vector<uint8_t> buffer(32768);
	size_t bytesRead;
	while ((bytesRead = std::fread(buffer.data(), 1, buffer.size(), file)) > 0) {
		hasher.process(buffer.begin(), buffer.begin() + bytesRead);
	}
	const bool readError = std::ferror(file) != 0;
	std::fclose(file);
	if (readError)
		return false;

	hasher.finish();
	hasher.get_hash_bytes(hashOut.begin(), hashOut.end());
	return true;
}

void RegisterPackedModIdentifier(std::string_view name, const char *mpqPath)
{
	ModIdentifier identifier;
	identifier.name = std::string(name);
	identifier.hash = {};
	if (!ComputeFileSha256(mpqPath, identifier.hash)) {
		LogError("Failed to hash mod archive: {}", mpqPath);
	}
	identifier.whitelisted = IsHashWhitelisted(identifier.hash);
	ActiveModIdentifiers.push_back(std::move(identifier));
}

void RegisterBuiltinModIdentifier(std::string_view name)
{
	ModIdentifier identifier;
	identifier.name = std::string(name);
	identifier.hash = {};
	identifier.whitelisted = true; // provenance: resolves purely from core archives
	ActiveModIdentifiers.push_back(std::move(identifier));
}

std::string ModHashToHex(std::span<const uint8_t, 32> hash)
{
	return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

bool IsHashWhitelisted(std::span<const uint8_t, 32> hash)
{
	for (const std::array<uint8_t, 32> &approved : ApprovedModHashes) {
		if (std::equal(approved.begin(), approved.end(), hash.begin()))
			return true;
	}
	return false;
}

} // namespace devilution
