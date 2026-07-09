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
 * The manifest is read directly from the mod's own archive (never through the
 * override-capable `FindAsset` pipeline), so it is covered by the same SHA-256 that
 * identifies the mod — a release can only ever vouch for its own past, never re-scope
 * an old identifier (see `todo/mod-check.md` §2).
 *
 * Only `compatibleWith` is consumed today (Phase 2). The remaining fields define the
 * schema up front so later phases can wire them in without a file rename or a second
 * parse pass; each has an outstanding task in `todo/mod-check.md`.
 */
struct ModManifest {
	/** Human-readable name shown in the mod menu. Display only — the canonical mod id remains the MPQ filename. */
	std::string name;
	/** One-line description shown in the mod menu. */
	std::string description;
	/** Free-form version string (e.g. "1.2.0"), shown in the mod menu. */
	std::string version;
	/** Author / attribution, for a future online mod archive. */
	std::string author;
	/** Homepage or project URL, for a future online mod archive. */
	std::string homepage;
	/** License identifier or short text, for a future online mod archive. */
	std::string license;
	/**
	 * Save-file extension this mod uses (e.g. ".hsv"). Last active mod that sets one wins.
	 * Not yet consumed — see the save-extension task in `todo/mod-check.md`.
	 */
	std::string saveExtension;
	/**
	 * Four-character program id (e.g. "DXMD") for the multiplayer game-list icon. Last active
	 * mod that sets one wins. Not yet consumed — see the programid task in `todo/mod-check.md`.
	 */
	std::string programId;
	/**
	 * Names (MPQ filename ids) of other mods this mod requires; also constrains load order.
	 * Not yet consumed — see the required-mods task in `todo/mod-check.md`.
	 */
	std::vector<std::string> requiredMods;
	/**
	 * SHA-256 identifiers of previous releases whose saves this version can load (design §2).
	 * A required identifier `X` is satisfied by this mod iff its own hash equals `X` or `X`
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
 * Rebuilt from scratch on every mod reload (`LuaReloadActiveMods` -> `LoadModArchives`),
 * so it is never stale. Loose-dir-only mods are intentionally absent — they carry no
 * identifier and are gated by the loose-asset integrity check.
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
 * The entry is marked whitelisted iff its hash is in the hardcoded approved list.
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
 * @brief Whether a mod satisfies a required save identifier per the compat rule (design §2).
 *
 * True iff the mod's own hash equals `required`, or `required` appears in the mod's
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

/**
 * @brief Returns true if `hash` is in the hardcoded approved (whitelisted) list.
 *
 * The approved list is populated per design §7 (whitelist). Whitelisting is by hash
 * only — never by name, which would be spoofable (see `todo/mod-check.md` §3/§7).
 */
[[nodiscard]] bool IsHashWhitelisted(std::span<const uint8_t, 32> hash);

} // namespace devilution
