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
 * @brief Identity of an active mod, used for multiplayer save/join compatibility checks.
 */
struct ModIdentifier {
	/** Mod name as listed in the active mod list (for error messages only). */
	std::string name;
	/** SHA-256 of the raw mod MPQ bytes, or all-zero for provenance-whitelisted built-ins. */
	std::array<uint8_t, 32> hash;
	/** True if this mod is exempt from compatibility checks (provenance or hash whitelist). */
	bool whitelisted;
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
 */
void RegisterPackedModIdentifier(std::string_view name, const char *mpqPath);

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
