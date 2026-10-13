#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace devilution {
namespace gpu_scaler {
namespace preset {

/** @brief std140 block member types we support in slang UBO/push blocks. */
enum class UniformType : uint8_t {
	Float,
	Vec2,
	Vec3,
	Vec4,
	Int,
	IVec2,
	IVec3,
	IVec4,
	Uint,
	UVec2,
	UVec3,
	UVec4,
	Bool,
	Mat4,
};

struct UniformMember {
	std::string name;
	UniformType type = UniformType::Float;
	int arraySize = 0;   // 0 = scalar member; > 0 = array with this element count.
	uint32_t offset = 0; // std140 byte offset inside the block.
	uint32_t size = 0;   // Total std140 byte size (arrays included).
};

/** @brief One rewritten uniform block (the original UBO or push-constant block). */
struct UniformBlock {
	std::string instanceName;           // Instance identifier as used by the shader ("" if none).
	uint32_t binding = 0;               // Binding assigned after the rewrite (dense, 0..1).
	std::vector<UniformMember> members; // std140 reflected layout.
};

/** @brief A texture the shader samples, with its assigned dense binding. */
struct TextureRef {
	std::string name;     // "Source", "Original", "OriginalHistory1", alias or LUT name.
	uint32_t binding = 0; // Dense binding in set 2 (fragment).
};

struct ParamInfo {
	std::string name;
	std::string description;
	float defaultValue = 0.0F;
	float minimum = 0.0F;
	float maximum = 1.0F;
	float step = 0.001F;
};

struct StageTranslation {
	bool present = false;
	std::string glsl; // Final GLSL source for this stage.
	std::vector<UniformBlock> uniformBlocks;
	std::vector<TextureRef> textures;
	bool usesMvp = false;
	bool usesFrameCount = false;
	bool usesFrameDirection = false;
	bool usesFrameTime = false;
	bool usesPassIndex = false;
	// Field diagnostics: raw sampling-style usage counts of the stage GLSL.
	int texelFetchCount = 0;
	int textureGatherCount = 0;
};

struct SlangTranslation {
	StageTranslation vertex;
	StageTranslation fragment;
	std::vector<ParamInfo> parameters; // #pragma parameter metadata.
	std::string pragmaName;            // #pragma name
	std::string pragmaFormat;          // #pragma format
	// Field diagnostics: content fingerprints (raw file, include-resolved merge).
	int sourceSize = 0;
	unsigned sourceHash = 0;
	int mergedSize = 0;
	unsigned mergedHash = 0;
	std::string error;

	bool ok() const { return error.empty() && (vertex.present || fragment.present); }
};

/**
 * @brief Translates one .slang source file into per-stage GLSL ready for
 * glslang Vulkan compilation, with descriptors rewritten to the SDL3 GPU
 * conventions (vertex UBOs set 1, fragment textures set 2, fragment UBOs
 * set 3) and the push-constant block re-homed as a std140 UBO.
 *
 * @param shaderPath Absolute path of the .slang file (include base + diagnostics).
 * @param knownTextureNames Names the shaders may reference besides the builtins:
 *   preset lookup texture names and pass aliases.
 * @param passIndex Index of this pass in the chain (for PassOutput# causality).
 * @param passCount Number of passes in the chain.
 */
bool TranslateSlang(const std::string &shaderPath, const std::unordered_set<std::string> &knownTextureNames,
    int passIndex, int passCount, SlangTranslation &out,
    const std::vector<ParamInfo> &presetParameters = {});

} // namespace preset
} // namespace gpu_scaler
} // namespace devilution
