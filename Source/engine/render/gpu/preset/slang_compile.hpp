#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/render/gpu/preset/slang_translate.hpp"

namespace devilution {
namespace gpu_scaler {
namespace preset {

struct PassSpirv {
	std::vector<uint8_t> vertex;   // SPIR-V bytecode for the vertex stage.
	std::vector<uint8_t> fragment; // SPIR-V bytecode for the fragment stage.
};

/**
 * @brief Compiles one translated .slang pass to SPIR-V for the Vulkan flavor
 * of the SDL3 GPU conventions, using the in-process glslang frontend.
 *
 * Results are cached on disk (see CachePathFor) keyed by a hash of the
 * translated sources; precompiled `<name>.vert.spv`/`<name>.frag.spv` files
 * sitting next to the .slang source always win over everything else.
 *
 * @param translation Translated stage sources and metadata.
 * @param slangPath Absolute path of the source .slang (for .spv fast-path lookup).
 * @param cacheDir Directory for cache blobs (may be empty to disable caching).
 */
bool CompilePass(const SlangTranslation &translation, const std::string &slangPath, const std::string &cacheDir,
    PassSpirv &out, std::string &error);

/** @brief The cache blob paths for a translated .slang pass (content-addressed). */
void CachePathFor(const SlangTranslation &translation, const std::string &slangPath, const std::string &cacheDir,
    std::string &vertexPath, std::string &fragmentPath);

/** @brief Precompiled SPIR-V file paths next to a .slang source. */
void SidecarSpvPathsFor(const std::string &slangPath, std::string &vertexPath, std::string &fragmentPath);

} // namespace preset
} // namespace gpu_scaler
} // namespace devilution
