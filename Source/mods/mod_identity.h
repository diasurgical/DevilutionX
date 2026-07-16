/**
 * @file mod_identity.h
 *
 * SHA-256 identifiers for active mods, used by the multiplayer compatibility checks
 * (see `todo/mod-check.md`). Only packed (MPQ) mods carry a content hash; loose-dir
 * mods are handled by the loose-asset integrity check instead.
 */
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "utils/attributes.h"

namespace devilution {

/**
 * @brief Declarative metadata a mod may ship as a `manifest.ini` at the root of its MPQ.
 *
 * The manifest is read directly from the mod's own archive, so is it self part of the
 * identifie, a release can only ever vouch for its past releases.
 */
struct ModManifest {
	/** Human-readable name shown in the mod menu. */
	std::string name;
	/** Description shown in the mod menu. */
	std::string description;
	/** Free-form version string (e.g. "1.2.0"). */
	std::string version;
	/** Author / attribution. */
	std::string author;
	/** Homepage or project URL, for a future online mod archive. */
	std::string homepage;
	/** License identifier or short text, for a future online mod archive. */
	std::string license;
	/**
	 * Save-file extension this mod uses, without the leading dot (e.g. "hsv"). Opt-in: a mod
	 * that omits this will default to "msv". The last active mod that sets one wins.
	 */
	std::string saveExtension;
	/**
	 * Four-character cosmetic branding id (e.g. "HRTL") shown in the multiplayer game browser.
	 * The last active mod that sets one wins.
	 */
	std::string programId;
	/**
	 * Names (MPQ filename ids) of other mods this mod requires; also constrains load order so
	 * required mods load first.
	 */
	std::vector<std::string> requiredMods;
	/**
	 * SHA-256 identifiers of previous releases whose saves this version can load.
	 * A required identifier `X` is satisfied by this mod if its own hash equals `X` or `X`
	 * appears in this list.
	 */
	std::vector<std::array<uint8_t, 32>> compatibleWith;
};

/**
 * @brief Identity of an active mod, used for multiplayer save/join compatibility checks.
 */
struct ModIdentifier {
	/** Mod name as listed in the active mod list (the MPQ filename id; for error messages and lookup). */
	std::string name;
	/** SHA-256 of the raw mod MPQ bytes, or all-zero for provenance-whitelisted built-ins. */
	std::array<uint8_t, 32> hash;
	/** True if this mod is exempt from compatibility checks (provenance or hash whitelist). */
	bool whitelisted;
	/** Parsed `manifest.ini` from the mod's own archive; default-constructed when absent. */
	ModManifest manifest;
};

/**
 * @brief Active packed/built-in mod identifiers, in mod load order.
 *
 * Loose-dir-only mods are intentionally absentthey, they are instead
 * gated by the loose-asset integrity check.
 */
extern DVL_API_FOR_TEST std::vector<ModIdentifier> ActiveModIdentifiers;

/** @brief Empties `ActiveModIdentifiers`. Called at the start of every mod reload. */
void ClearModIdentifiers();

/**
 * @brief Computes the SHA-256 of a file's raw bytes using a chunked read.
 * @param path absolute path to the file
 * @param[out] hashOut filled with the 32-byte digest on success (untouched on failure)
 * @return true on success; false if the file could not be opened or read
 */
bool ComputeFileSha256(const char *path, std::array<uint8_t, 32> &hashOut);

/**
 * @brief Records a packed mod's identifier, hashing its MPQ file bytes.
 *
 * The entry is marked whitelisted if its hash is in the hardcoded approved list.
 * If the file cannot be hashed the mod is still recorded (with a zero hash) so the
 * active list stays positionally aligned with the mod load order.
 *
 * @return reference to the just-appended entry (valid until the next mutation of
 * `ActiveModIdentifiers`), so the caller can attach a parsed manifest to it.
 */
ModIdentifier &RegisterPackedModIdentifier(std::string_view name, const char *mpqPath);

/**
 * @brief Parses a mod `manifest.ini` (INI text read from the mod's own MPQ).
 *
 * All fields are optional; unknown keys are ignored and a malformed file yields an
 * empty manifest. `compatible` identifiers that are not 64 hex characters are skipped.
 */
[[nodiscard]] ModManifest ParseModManifest(std::string_view manifestIni);

/**
 * @brief Decodes a lowercase/uppercase 64-character hex string into a 32-byte hash.
 * @return true on success; false if the input is not exactly 64 hex digits.
 */
bool HexToModHash(std::string_view hex, std::array<uint8_t, 32> &out);

/**
 * @brief Whether a mod satisfies a required save identifier per the compat rule.
 *
 * True if the mod's own hash equals `required`, or `required` appears in the mod's
 * manifest `compatibleWith` list.
 */
[[nodiscard]] bool SatisfiesRequiredIdentifier(const ModIdentifier &mod, std::span<const uint8_t, 32> required);

/**
 * @brief Records a built-in mod (one that resolves purely from core archives) as
 * provenance-whitelisted, with a zero hash.
 */
void RegisterBuiltinModIdentifier(std::string_view name);

/** @brief Returns the lowercase hex string of a 32-byte hash. */
[[nodiscard]] std::string ModHashToHex(std::span<const uint8_t, 32> hash);

/** @brief Returns true if `hash` is in the hardcoded approved mod whitelisted. */
[[nodiscard]] bool IsHashWhitelisted(std::span<const uint8_t, 32> hash);

/**
 * @brief Save-file extension declared by the active mods, or empty if mods are active.
 *
 * The last active mod (in load order) that sets `saveExtension` wins.
 * The returned view points into `ActiveModIdentifiers` and is valid until the next
 * mod reload.
 */
[[nodiscard]] std::string_view GetActiveModSaveExtension();

/** @brief A mod name paired with the names of the mods it declares as required. */
struct ModDependency {
	std::string name;
	std::vector<std::string> requiredMods;
};

/**
 * @brief Orders mods so every mod's required mods load before it (stable topological sort).
 *
 * Ties preserve the input order. A required mod that is not in the input, and any
 * dependency cycle, is logged as an error; the involved mods are still emitted in their
 * original relative order so a broken manifest never silently drops a mod.
 */
[[nodiscard]] std::vector<std::string> OrderModsByDependencies(const std::vector<ModDependency> &mods);

} // namespace devilution
