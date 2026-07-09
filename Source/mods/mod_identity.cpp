#include "mods/mod_identity.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <picosha2.h>

#include "utils/file_util.h"
#include "utils/ini.hpp"
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

ModIdentifier &RegisterPackedModIdentifier(std::string_view name, const char *mpqPath)
{
	ModIdentifier identifier;
	identifier.name = std::string(name);
	identifier.hash = {};
	if (!ComputeFileSha256(mpqPath, identifier.hash)) {
		LogError("Failed to hash mod archive: {}", mpqPath);
	}
	identifier.whitelisted = IsHashWhitelisted(identifier.hash);
	ActiveModIdentifiers.push_back(std::move(identifier));
	return ActiveModIdentifiers.back();
}

bool HexToModHash(std::string_view hex, std::array<uint8_t, 32> &out)
{
	if (hex.size() != 2 * out.size())
		return false;
	const auto nibble = [](char c) -> int {
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return -1;
	};
	for (size_t i = 0; i < out.size(); ++i) {
		const int hi = nibble(hex[i * 2]);
		const int lo = nibble(hex[i * 2 + 1]);
		if (hi < 0 || lo < 0)
			return false;
		out[i] = static_cast<uint8_t>((hi << 4) | lo);
	}
	return true;
}

ModManifest ParseModManifest(std::string_view manifestIni)
{
	ModManifest manifest;
	tl::expected<Ini, std::string> ini = Ini::parse(manifestIni);
	if (!ini.has_value()) {
		LogError("Failed to parse mod manifest: {}", ini.error());
		return manifest;
	}

	constexpr std::string_view Section = "mod";
	manifest.name = std::string(ini->getString(Section, "name"));
	manifest.description = std::string(ini->getString(Section, "description"));
	manifest.version = std::string(ini->getString(Section, "version"));
	manifest.author = std::string(ini->getString(Section, "author"));
	manifest.homepage = std::string(ini->getString(Section, "homepage"));
	manifest.license = std::string(ini->getString(Section, "license"));
	manifest.saveExtension = std::string(ini->getString(Section, "saveExtension"));
	manifest.programId = std::string(ini->getString(Section, "programId"));

	for (const Ini::Value &value : ini->get(Section, "requires")) {
		if (!value.value.empty())
			manifest.requiredMods.push_back(value.value);
	}
	for (const Ini::Value &value : ini->get(Section, "compatible")) {
		std::array<uint8_t, 32> hash;
		if (HexToModHash(value.value, hash)) {
			manifest.compatibleWith.push_back(hash);
		} else {
			LogError("Ignoring invalid compatible identifier in mod manifest: {}", value.value);
		}
	}
	return manifest;
}

bool SatisfiesRequiredIdentifier(const ModIdentifier &mod, std::span<const uint8_t, 32> required)
{
	if (std::equal(mod.hash.begin(), mod.hash.end(), required.begin()))
		return true;
	for (const std::array<uint8_t, 32> &compatible : mod.manifest.compatibleWith) {
		if (std::equal(compatible.begin(), compatible.end(), required.begin()))
			return true;
	}
	return false;
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

std::string_view GetActiveModSaveExtension()
{
	std::string_view result;
	for (const ModIdentifier &mod : ActiveModIdentifiers) {
		std::string_view ext = mod.manifest.saveExtension;
		if (ext.empty())
			continue;
		if (ext.front() == '.')
			ext.remove_prefix(1);
		if (!ext.empty())
			result = ext; // last active mod that declares one wins
	}
	return result;
}

std::vector<std::string> OrderModsByDependencies(const std::vector<ModDependency> &mods)
{
	std::vector<std::string> ordered;
	ordered.reserve(mods.size());

	// A mod is emitted once all of its (present) required mods have been emitted. We repeat
	// passes over the not-yet-emitted mods until a full pass emits nothing new: the remainder
	// is then unsatisfiable (a cycle, or a required mod missing from the active set).
	std::vector<bool> emitted(mods.size(), false);
	std::unordered_set<std::string_view> emittedNames;
	std::unordered_set<std::string_view> presentNames;
	for (const ModDependency &mod : mods)
		presentNames.insert(mod.name);

	bool progress = true;
	while (progress) {
		progress = false;
		for (size_t i = 0; i < mods.size(); ++i) {
			if (emitted[i])
				continue;
			bool ready = true;
			for (const std::string &required : mods[i].requiredMods) {
				if (emittedNames.find(required) != emittedNames.end())
					continue;
				// A required mod that is not part of the active set can never be satisfied;
				// don't block on it here (it is reported once the loop stalls below).
				if (presentNames.find(required) != presentNames.end()) {
					ready = false;
					break;
				}
			}
			if (!ready)
				continue;
			ordered.push_back(mods[i].name);
			emittedNames.insert(mods[i].name);
			emitted[i] = true;
			progress = true;
		}
	}

	// Anything still unemitted is part of a cycle. Report it and append in original order so
	// the mod is still loaded (best-effort) rather than silently dropped.
	for (size_t i = 0; i < mods.size(); ++i) {
		if (emitted[i])
			continue;
		LogError("Mod '{}' has an unsatisfiable required-mod dependency (cycle); loading anyway.", mods[i].name);
		ordered.push_back(mods[i].name);
	}

	// Report any required mods that are missing from the active set.
	for (const ModDependency &mod : mods) {
		for (const std::string &required : mod.requiredMods) {
			if (presentNames.find(required) == presentNames.end())
				LogError("Mod '{}' requires '{}', which is not enabled.", mod.name, required);
		}
	}

	return ordered;
}

} // namespace devilution
