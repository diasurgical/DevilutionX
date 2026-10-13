#pragma once

#ifdef USE_GPU_SCALER
#ifndef USE_SDL1

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "engine/render/gpu/preset/preset_types.hpp"

struct SDL_Window;

namespace devilution {
namespace gpu_scaler {
namespace preset {

struct PassRuntime;
struct LutRuntime;
struct UniformBlockRuntime;

/**
 * @brief Loads and executes one libretro slang preset (.slangp) on the
 * SDL3 GPU device of the GPU scaler.
 *
 * The runner owns every GPU resource of the preset chain (pipelines, shaders,
 * pass targets, history frames, lookup textures, samplers) and exposes a
 * single Run() call per presented frame. Any load failure is reported
 * through the error string; run-time failures are expected to be handled by
 * the GPU scaler's session fallback contract.
 */
class Runner {
public:
	Runner() = default;
	~Runner();
	Runner(const Runner &) = delete;
	Runner &operator=(const Runner &) = delete;

	/**
	 * @brief Parses, translates, compiles and uploads the preset.
	 *
	 * @param device SDL_GPU device (must stay alive until Unload).
	 * @param backend "vulkan" compiles SPIR-V; "direct3d12" transpiles the
	 *     SPIR-V to HLSL and compiles DXBC with the guarded d3dcompiler.
	 * @param preset The parsed preset (preset_parser).
	 * @param error Filled with a human-readable reason on failure.
	 */
	bool Load(SDL_GPUDevice *device, std::string_view backend, const Preset &preset, std::string &error);

	/** @brief Releases all GPU resources. Safe to call anytime. */
	void Unload();

	[[nodiscard]] bool IsLoaded() const { return device_ != nullptr && !passes_.empty(); }

	/**
	 * @brief Executes the shader chain for one frame.
	 *
	 * @param gameTex RGBA chain input (gameW x gameH), already uploaded.
	 * @param swapchainWidth/Height Present target size (drives viewport-scale
	 *     passes and FinalViewportSize).
	 * @param frameDeltaUs Duration of the previous frame in microseconds.
	 * @param outTexture Receives the final pass output texture (owned by the
	 *     runner; valid until the next Run or Unload).
	 * @return false when the frame must be skipped (transient condition;
	 *     the GPU scaler's failure handling stays in charge of hard errors).
	 */
	bool Run(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *gameTex, int gameWidth, int gameHeight,
	    Uint32 swapchainWidth, Uint32 swapchainHeight, Uint64 frameDeltaUs, SDL_GPUTexture **outTexture,
	    Uint32 *outWidth, Uint32 *outHeight);

	/** @brief Highest OriginalHistory# index referenced by any pass (0 = none). */
	[[nodiscard]] int HistoryDepth() const { return historyDepth_; }

private:
	bool EnsureSamplers();
	SDL_GPUSampler *SamplerFor(FilterMode filter, WrapMode wrap, bool mipmapped);
	bool EnsureVertexBuffer();
	bool EnsureTargets(int gameWidth, int gameHeight, Uint32 swapchainWidth, Uint32 swapchainHeight);
	void ReleaseTargets();
	bool LoadLut(LutRuntime &lut, std::string &error);
	void PushBlockUniforms(SDL_GPUCommandBuffer *cmd, bool fragmentStage, const UniformBlockRuntime &block,
	    int passIndex, float inputWidth, float inputHeight);
	void FillBlockBytes(const UniformBlockRuntime &block, int passIndex, float inputWidth, float inputHeight,
	    bool fragmentStage, std::vector<uint8_t> &out) const;

	SDL_GPUDevice *device_ = nullptr;
	Preset preset_;
	std::vector<PassRuntime *> passes_;
	std::vector<LutRuntime *> luts_;

	// Sampler cache keyed by (filter, wrap, mipmap-mode).
	SDL_GPUSampler *samplers_[2][3][2] = {};

	SDL_GPUBuffer *vertexBuffer_ = nullptr;

	int historyDepth_ = 0;                  // Max referenced OriginalHistory# index.
	std::vector<SDL_GPUTexture *> history_; // Ring of chain-input copies.
	int historySlot_ = 0;

	int targetGameWidth_ = 0;
	int targetGameHeight_ = 0;
	Uint32 targetSwapchainWidth_ = 0;
	Uint32 targetSwapchainHeight_ = 0;

	Uint64 frameCounter_ = 0;
	Uint64 frameDeltaUs_ = 0;
	Uint32 swapchainWidth_ = 0;
	Uint32 swapchainHeight_ = 0;
};

} // namespace preset
} // namespace gpu_scaler
} // namespace devilution

#endif // !USE_SDL1
#endif // USE_GPU_SCALER
