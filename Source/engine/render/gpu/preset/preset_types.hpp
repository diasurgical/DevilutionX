#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace devilution {
namespace gpu_scaler {
namespace preset {

/** @brief Texture addressing mode for a preset pass input or lookup texture. */
enum class WrapMode : uint8_t {
	ClampToEdge,
	Repeat,
	MirroredRepeat,
};

/** @brief Texture filtering mode for a preset pass input or lookup texture. */
enum class FilterMode : uint8_t {
	Nearest,
	Linear,
};

/** @brief Reference frame for a pass output size, following the libretro preset spec. */
enum class ScaleType : uint8_t {
	Source,
	Viewport,
	Absolute,
};

/** @brief Per-axis output size rule of a preset pass. */
struct ScaleRule {
	ScaleType type = ScaleType::Source;
	float scale = 1.0F;
};

/** @brief One shader pass of a preset (one .slang file). */
struct PassDef {
	std::string shaderPath; // Relative to the preset file directory.
	FilterMode filter = FilterMode::Nearest;
	WrapMode wrap = WrapMode::ClampToEdge;
	ScaleRule scaleX;
	ScaleRule scaleY;
	bool srgbFramebuffer = false;
	bool floatFramebuffer = false;
	bool mipmapInput = false;
	std::string alias;          // Optional alias name for this pass output.
	uint32_t frameCountMod = 0; // FrameCount is pre-wrapped by this modulo.
	std::string pragmaFormat;   // Optional #pragma format override from the .slang.
};

/** @brief A lookup texture declared by a preset. */
struct TextureDef {
	std::string name; // Name the shaders reference (sampler2D NAME).
	std::string path; // Relative to the preset file directory.
	FilterMode filter = FilterMode::Nearest;
	WrapMode wrap = WrapMode::ClampToEdge;
	bool mipmap = false;
};

/** @brief A shader parameter value (from the preset file; the .slang default applies otherwise). */
struct ParamOverride {
	std::string name;
	float value = 0.0F;
};

/** @brief A parsed .slangp preset: a chain of shader passes plus resources. */
struct Preset {
	std::string filePath;     // Resolved absolute path of the .slangp file.
	std::string dir;          // Directory of the .slangp file (with trailing separator).
	std::string displayName;  // Human-readable name (file stem).
	std::string relativePath; // Relative to the shader root directory, forward slashes.
	std::vector<PassDef> passes;
	std::vector<TextureDef> textures;
	std::vector<ParamOverride> parameters;
	int feedbackPass = -1; // Pass whose output is fed back as feedback input.
	int histories = 1;     // Chain input history depth (OriginalHistory#).
};

/** @brief One .slangp file discovered in the runtime shader folder. */
struct PresetEntry {
	std::string displayName;  // Prettified file stem; shown in the settings list.
	std::string relativePath; // Relative to the shader root directory, forward slashes.
};

/** @brief One directory level of the runtime shader folder (settings UI browsing). */
struct PresetDirListing {
	std::vector<std::string> subdirs; // Sorted subdirectory names (this level only).
	std::vector<PresetEntry> presets; // .slangp files directly in this directory, sorted.
};

} // namespace preset
} // namespace gpu_scaler
} // namespace devilution
