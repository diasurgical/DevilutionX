#pragma once

#include <string>
#include <vector>

#include "engine/render/gpu/preset/preset_types.hpp"

namespace devilution {
namespace gpu_scaler {
namespace preset {

/**
 * @brief Parses a .slangp preset file (libretro slang preset format).
 *
 * Handles: shaders/shaderN chains, filter_linearN, wrap_modeN, scale rules,
 * srgb/float framebuffers, aliases, frame_count_mod, mipmap_input, lookup
 * textures (with linear/wrap/mipmap variants), parameter overrides,
 * feedback_pass and histories. Values may be quoted; '#' comments (full-line
 * and trailing) are stripped.
 *
 * @return true on success; on failure `out` is left untouched and `error`
 * contains a human-readable reason.
 */
bool ParsePreset(const std::string &path, Preset &out, std::string &error, int referenceDepth);

bool ParsePreset(const std::string &path, Preset &out, std::string &error);

/**
 * @brief Scans `rootDir` recursively (bounded depth) for .slangp presets.
 *
 * Returns entries sorted by relative path (deterministic menu ordering).
 * The scan is not cached; call sparingly (startup and when the settings
 * menu opens).
 */
std::vector<PresetEntry> ScanPresets(const std::string &rootDir);

/**
 * @brief Lists subdirectories and .slangp presets directly inside
 * rootDir/relativeDir ("" = the root itself).
 *
 * Used by the settings UI for folder navigation. Preset relativePath values
 * are relative to rootDir (forward slashes); displayName is prettified.
 * Subdirectory names are plain names without separators; the special shader
 * cache folder and hidden directories are not listed.
 */
PresetDirListing ListPresetDir(const std::string &rootDir, const std::string &relativeDir);

/**
 * @brief The runtime shader folder: "shaders" next to the executable.
 * End users drop .slangp presets (and any subfolder layout) here.
 */
std::string ShaderRootDir();

/**
 * @brief Directory for compiled SPIR-V cache blobs: ShaderRootDir() + "/cache".
 */
std::string ShaderCacheDir();

/**
 * @brief Resolves a preset relative path against the shader root and returns
 * the absolute path, or an empty string when the file does not exist.
 */
std::string ResolvePresetPath(const std::string &relativePath);

/**
 * @brief Resolves a file path referenced by a preset (shader pass, LUT
 * texture or #reference target) and returns the path to use.
 *
 * Redistributed shader packs commonly keep presets in a subfolder of the
 * pack while the referenced files sit in a sibling folder of the pack root
 * (e.g. koko-aio\Presets-4.1\*.slangp referencing shaders-ng\*.slang one
 * level up). Absolute references pass through; relative references are tried
 * against the preset's own folder first, then against each ancestor folder
 * (bounded), then against the shader root. The first existing file wins;
 * when nothing exists the preset-relative join is returned so error messages
 * show the conventional path.
 */
std::string ResolvePresetFilePath(const std::string &presetDir, const std::string &referencedPath);

} // namespace preset
} // namespace gpu_scaler
} // namespace devilution
