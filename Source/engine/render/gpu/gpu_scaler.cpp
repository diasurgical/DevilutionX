#include "engine/render/gpu/gpu_scaler.hpp"

#ifdef USE_GPU_SCALER
#ifndef USE_SDL1

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cmath>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifdef GPU_SCALER_SPV_GENERATED
#include "engine/render/gpu/gpu_shader_spv.hpp"
#else
#include "engine/render/gpu/gpu_shader_spv_prebuilt.hpp"
#endif
#include "engine/render/gpu/gpu_d3dcompile.hpp"
#include "engine/render/gpu/gpu_shaders_hlsl.hpp"
#include "engine/render/gpu/gpu_shaders_msl.hpp"
#include "engine/render/gpu/preset/preset_parser.hpp"
#include "engine/render/gpu/preset/preset_runner.hpp"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace devilution {
namespace gpu_scaler {
namespace {

struct alignas(16) FragUniforms {
	float sourceSize[4]; // (w, h, 1/w, 1/h)
	float params[4];
};

enum class Program {
	Vertex,
	Blit,
};

SDL_GPUDevice *gDevice = nullptr;
SDL_Window *gWindow = nullptr;
SDL_GPUShaderFormat gShaderFormat = SDL_GPU_SHADERFORMAT_INVALID;
ShaderFilter gFilter = ShaderFilter::None;

int gGameWidth = 0;
int gGameHeight = 0;

SDL_GPUShader *gVertexShader = nullptr;
SDL_GPUShader *gFragmentShaders[9] = {};

SDL_GPUGraphicsPipeline *gBlitPipeline = nullptr;

SDL_GPUTexture *gGameTex = nullptr;

// Runtime libretro slang preset chain (custom shader filters).
preset::Runner gPresetRunner;
std::string gPresetDisplayName;
// The preset file the running chain was built from (resolved path).
std::string gLoadedPresetPath;

SDL_GPUSampler *gNearestSampler = nullptr;
SDL_GPUSampler *gLinearSampler = nullptr;

bool gLoggedFailure = false;

// Set when the scaler hits an unrecoverable error. Session-sticky: the game
// reverts to the regular renderer instead of retrying a broken GPU path.
bool gFailed = false;
// The last window successfully claimed by the GPU device. Deliberately kept
// after Shutdown(): on Windows the HWND keeps its DXGI flip-model swapchain
// history, so display code must recreate the window before handing it back
// to the regular SDL_Renderer. Only ever compared by pointer, never
// dereferenced (the window may already be destroyed).
SDL_Window *gLastClaimedWindow = nullptr;
// Format the current blit pipeline was built for (swapchain format can change).
SDL_GPUTextureFormat gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

// SDL_Log output is invisible for a GUI-subsystem application on Windows, so
// GPU scaler diagnostics are also appended to gpu-scaler.log next to the
// executable. The file opens lazily on first use and is size-capped: when a
// write would push it past kMaxLogBytes, the current log is rotated to
// gpu-scaler.log.old (one generation kept) and a fresh file starts.
constexpr long kMaxLogBytes = 200 * 1024;

FILE *gLogFile = nullptr;
// Bytes currently in the log file, kept in step with the writes below so
// that the cap check does not have to stat the file on every line.
long gLogBytes = 0;

void InstallSdlLogCapture();

std::string LogFilePath()
{
	const char *basePath = SDL_GetBasePath();
	// SDL3 owns the returned string; it must not be freed.
	return basePath != nullptr ? std::string(basePath) + "gpu-scaler.log" : std::string("gpu-scaler.log");
}

long LogFileSize(const std::string &path)
{
	FILE *file = std::fopen(path.c_str(), "rb");
	if (file == nullptr)
		return 0;
	long size = 0;
	if (std::fseek(file, 0, SEEK_END) == 0)
		size = std::ftell(file);
	std::fclose(file);
	return size > 0 ? size : 0;
}

// Moves path aside as path.old (one generation kept). When the rename is
// refused (e.g. a viewer holding the .old file on Windows), the current
// file is removed instead so the cap holds either way.
void RotateLogFile(const std::string &path)
{
	const std::string oldPath = path + ".old";
	std::remove(oldPath.c_str());
	if (std::rename(path.c_str(), oldPath.c_str()) != 0)
		std::remove(path.c_str());
}

void LogClose()
{
	if (gLogFile != nullptr) {
		std::fclose(gLogFile);
		gLogFile = nullptr;
	}
}

FILE *LogOpenAppended()
{
	const std::string path = LogFilePath();
	// A leftover log from previous sessions must not push this session's
	// file past the cap: rotate oversized leftovers once at open.
	if (LogFileSize(path) > kMaxLogBytes)
		RotateLogFile(path);
	FILE *file = std::fopen(path.c_str(), "a");
	if (file != nullptr && std::fseek(file, 0, SEEK_END) == 0) {
		const long size = std::ftell(file);
		gLogBytes = size > 0 ? size : 0;
	} else {
		gLogBytes = 0;
	}
	return file;
}

// Starts a fresh log mid-session, keeping the current one as
// gpu-scaler.log.old.
void RotateLog()
{
	LogClose();
	RotateLogFile(LogFilePath());
	gLogBytes = 0;
	gLogFile = std::fopen(LogFilePath().c_str(), "a");
	if (gLogFile != nullptr) {
		const std::string marker = StrCat("=== gpu-scaler.log rotated (", kMaxLogBytes / 1024, " KB cap) ===\n");
		std::fwrite(marker.data(), 1, marker.size(), gLogFile);
		gLogBytes += static_cast<long>(marker.size());
		std::fflush(gLogFile);
	}
}

FILE *LogFile()
{
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		gLogFile = LogOpenAppended();
		if (gLogFile != nullptr) {
			if (gLogBytes + 64 > kMaxLogBytes)
				RotateLog(); // keep even the session header within the cap
			if (gLogFile != nullptr) {
				const std::string header = StrCat("\n=== GPU scaler session (build ", __DATE__, " ", __TIME__, ") ===\n");
				std::fwrite(header.data(), 1, header.size(), gLogFile);
				gLogBytes += static_cast<long>(header.size());
				std::fflush(gLogFile);
			}
			InstallSdlLogCapture();
		}
	}
	return gLogFile;
}

// Returns the log file for a write of `incomingBytes` more bytes, rotating
// it first when that write would push the file past the size cap. Returns
// nullptr when the log is unavailable.
FILE *LogFileForWrite(size_t incomingBytes)
{
	if (LogFile() == nullptr)
		return nullptr;
	if (gLogBytes + static_cast<long>(incomingBytes) > kMaxLogBytes)
		RotateLog();
	return gLogFile;
}

void LogFileLine(std::string_view line)
{
	if (FILE *file = LogFileForWrite(line.size() + 16); file != nullptr) {
		std::fprintf(file, "[%10u ms] %.*s\n", static_cast<unsigned>(SDL_GetTicks()), static_cast<int>(line.size()), line.data());
		std::fflush(file);
	}
}

void LogOk(std::string_view what)
{
	LogFileLine(StrCat("ok: ", what));
}

void LogFail(std::string_view what)
{
	if (!gLoggedFailure) {
		gLoggedFailure = true;
		LogError("GPU scaler: {}: {} (further GPU scaler errors will be suppressed)", what, SDL_GetError());
	}
	LogFileLine(StrCat("ERROR: ", what, " | SDL_GetError: ", SDL_GetError()));
}

// A failure that cannot be recovered from within the GPU path: the scaler is
// torn down and the game falls back to the regular renderer for this session.
void LogFailHard(std::string_view what)
{
	gFailed = true;
	LogFail(what);
	LogFileLine("GPU scaler disabled for this session (hard failure)");
}

// SDL's own log output (adapter info, validation errors, device removal,
// backend initialization) is invisible for a GUI-subsystem application on
// Windows. Chain it into the diagnostic file as well.
SDL_LogOutputFunction gPrevSdlLogFunction = nullptr;
void *gPrevSdlLogUserdata = nullptr;

void SDLCALL SdlLogCapture(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
	if (gPrevSdlLogFunction != nullptr)
		gPrevSdlLogFunction(gPrevSdlLogUserdata, category, priority, message);
	const size_t messageLen = message != nullptr ? std::strlen(message) : 0;
	if (FILE *file = LogFileForWrite(messageLen + 16); file != nullptr) {
		std::fprintf(file, "[       SDL] %s\n", message != nullptr ? message : "");
		std::fflush(file);
	}
}

void InstallSdlLogCapture()
{
	if (gPrevSdlLogFunction != nullptr)
		return;
	SDL_GetLogOutputFunction(&gPrevSdlLogFunction, &gPrevSdlLogUserdata);
	SDL_SetLogOutputFunction(SdlLogCapture, nullptr);
}

struct ShaderCode {
	const uint8_t *data = nullptr;
	size_t size = 0;
};

ShaderCode SpvFor(Program program)
{
	switch (program) {
	case Program::Vertex: return { gpu::spv::kVertex, sizeof(gpu::spv::kVertex) };
	case Program::Blit: return { gpu::spv::kBlit, sizeof(gpu::spv::kBlit) };
	}
	return { nullptr, 0 };
}

[[maybe_unused]] const char *HlslFor(Program program)
{
#ifndef _WIN32
	// Only used by the D3D12 backend.
	(void)program;
	return nullptr;
#else
	switch (program) {
	case Program::Vertex: return gpu::hlsl::kVertex;
	case Program::Blit: return gpu::hlsl::kBlit;
	}
	return nullptr;
#endif
}

const char *MslFor(Program program)
{
	switch (program) {
	case Program::Vertex: return gpu::msl::kVertex;
	case Program::Blit: return gpu::msl::kBlit;
	}
	return nullptr;
}

int SamplersFor(Program program)
{
	switch (program) {
	case Program::Vertex: return 0;
	case Program::Blit: return 1;
	}
	return 0;
}

const char *NameFor(Program program)
{
	switch (program) {
	case Program::Vertex: return "vertex";
	case Program::Blit: return "blit";
	}
	return "unknown";
}

const char *MslEntryFor(Program program)
{
	switch (program) {
	case Program::Vertex: return "fullscreenVS";
	case Program::Blit: return "blitPS";
	}
	return "main";
}

bool CreateShader(Program program, SDL_GPUShaderStage stage, SDL_GPUShader **out)
{
	SDL_GPUShaderCreateInfo info {};
	info.stage = stage;
	info.entrypoint = (gShaderFormat == SDL_GPU_SHADERFORMAT_MSL) ? MslEntryFor(program) : "main";
	info.num_samplers = (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? SamplersFor(program) : 0;
	info.num_uniform_buffers = (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? 1 : 0;

	// Must outlive the SDL_CreateGPUShader call below.
	std::vector<uint8_t> dxbcBlob;

	switch (gShaderFormat) {
	case SDL_GPU_SHADERFORMAT_SPIRV: {
		const ShaderCode code = SpvFor(program);
		info.code = code.data;
		info.code_size = code.size;
		info.format = SDL_GPU_SHADERFORMAT_SPIRV;
	} break;
	case SDL_GPU_SHADERFORMAT_MSL: {
		const char *source = MslFor(program);
		info.code = reinterpret_cast<const uint8_t *>(source);
		info.code_size = std::strlen(source);
		info.format = SDL_GPU_SHADERFORMAT_MSL;
	} break;
	case SDL_GPU_SHADERFORMAT_DXBC: {
#ifdef _WIN32
		const char *source = HlslFor(program);
		// Shader Model 5.0 first: supported by every d3dcompiler and the
		// least exotic input for GPU drivers. 5.1 only as a fallback.
		const char *target = stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vs_5_0" : "ps_5_0";
		LogFileLine(StrCat("compiling shader '", NameFor(program), "' (", target, ") via d3dcompiler"));
		dxbcBlob = gpu_d3dcompile::Compile(source, target);
		if (dxbcBlob.empty()) {
			dxbcBlob = gpu_d3dcompile::Compile(source, stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vs_5_1" : "ps_5_1");
			if (dxbcBlob.empty()) {
				LogFileLine(StrCat("ERROR: all shader targets failed for '", NameFor(program), "'"));
				return false;
			}
		}
		LogFileLine(StrCat("compiled shader '", NameFor(program), "', bytes=", dxbcBlob.size()));
		info.code = dxbcBlob.data();
		info.code_size = dxbcBlob.size();
		info.format = SDL_GPU_SHADERFORMAT_DXBC;
		break;
#else
		return false;
#endif
	}
	default:
		return false;
	}

	*out = SDL_CreateGPUShader(gDevice, &info);
	if (*out == nullptr) {
		LogFail(StrCat("shader creation failed ('", NameFor(program), "')"));
		return false;
	}
	LogOk(StrCat("shader '", NameFor(program), "' created"));
	return true;
}

SDL_GPUTexture *CreateTexture(SDL_GPUTextureFormat format, Uint32 width, Uint32 height, SDL_GPUTextureUsageFlags usage)
{
	SDL_GPUTextureCreateInfo info {};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = format;
	info.usage = usage;
	info.width = width;
	info.height = height;
	info.layer_count_or_depth = 1;
	info.num_levels = 1;
	info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	SDL_GPUTexture *texture = SDL_CreateGPUTexture(gDevice, &info);
	if (texture == nullptr)
		LogFail("texture creation failed");
	return texture;
}

SDL_GPUTextureFormat GameTextureFormat()
{
	switch (DEVILUTIONX_DISPLAY_TEXTURE_FORMAT) {
	case SDL_PIXELFORMAT_ABGR8888:
		return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	case SDL_PIXELFORMAT_ARGB8888:
	case SDL_PIXELFORMAT_XRGB8888:
	default:
		return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
	}
}

SDL_GPUGraphicsPipeline *CreatePipeline(Program fragmentProgram, SDL_GPUTextureFormat targetFormat)
{
	if (gVertexShader == nullptr)
		return nullptr;
	SDL_GPUShader *fragmentShader = gFragmentShaders[static_cast<int>(fragmentProgram)];
	if (fragmentShader == nullptr)
		return nullptr;
	if (targetFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
		return nullptr;

	SDL_GPUColorTargetDescription targetDesc {};
	targetDesc.format = targetFormat;

	SDL_GPUGraphicsPipelineCreateInfo info {};
	info.vertex_shader = gVertexShader;
	info.fragment_shader = fragmentShader;
	info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	info.multisample_state.sample_mask = 0xFFFFFFFF;
	info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
	info.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
	info.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
	info.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
	info.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
	info.depth_stencil_state.back_stencil_state = info.depth_stencil_state.front_stencil_state;
	info.target_info.color_target_descriptions = &targetDesc;
	info.target_info.num_color_targets = 1;

	SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(gDevice, &info);
	if (pipeline == nullptr)
		LogFail(StrCat("pipeline creation failed ('", NameFor(fragmentProgram), "')"));
	else
		LogOk(StrCat("pipeline '", NameFor(fragmentProgram), "' created"));
	return pipeline;
}

bool EnsureSamplers()
{
	if (gNearestSampler != nullptr)
		return true;

	SDL_GPUSamplerCreateInfo samplerInfo {};
	samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
	samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
	samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
	samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	samplerInfo.max_anisotropy = 1.0F;
	samplerInfo.min_lod = 0.0F;
	samplerInfo.max_lod = 1000.0F;
	gNearestSampler = SDL_CreateGPUSampler(gDevice, &samplerInfo);

	SDL_GPUSamplerCreateInfo linearInfo = samplerInfo;
	linearInfo.min_filter = SDL_GPU_FILTER_LINEAR;
	linearInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
	gLinearSampler = SDL_CreateGPUSampler(gDevice, &linearInfo);

	if (gNearestSampler == nullptr || gLinearSampler == nullptr) {
		LogFail("sampler creation failed");
		return false;
	}
	return true;
}

bool EnsureBlitPipeline(SDL_GPUTextureFormat format)
{
	if (gBlitPipeline != nullptr && gBlitPipelineFormat == format)
		return true;
	if (gBlitPipeline != nullptr) {
		SDL_ReleaseGPUGraphicsPipeline(gDevice, gBlitPipeline);
		gBlitPipeline = nullptr;
		gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
	}
	gBlitPipeline = CreatePipeline(Program::Blit, format);
	if (gBlitPipeline == nullptr) {
		LogFail("blit pipeline creation failed");
		return false;
	}
	gBlitPipelineFormat = format;
	LogFileLine(StrCat("blit pipeline ready, swapchain format=", static_cast<int>(format)));
	return true;
}

void ReleaseFilterResources()
{
	gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
	if (gVertexShader != nullptr) {
		SDL_ReleaseGPUShader(gDevice, gVertexShader);
		gVertexShader = nullptr;
	}
	for (auto *&shader : gFragmentShaders) {
		if (shader != nullptr) {
			SDL_ReleaseGPUShader(gDevice, shader);
			shader = nullptr;
		}
	}
	if (gBlitPipeline != nullptr) {
		SDL_ReleaseGPUGraphicsPipeline(gDevice, gBlitPipeline);
		gBlitPipeline = nullptr;
	}
}

bool EnsureShaders()
{
	if (gVertexShader != nullptr)
		return true;
	if (!CreateShader(Program::Vertex, SDL_GPU_SHADERSTAGE_VERTEX, &gVertexShader))
		return false;
	static constexpr Program kFragmentPrograms[] = {
		Program::Blit
	};
	for (const Program program : kFragmentPrograms) {
		if (!CreateShader(program, SDL_GPU_SHADERSTAGE_FRAGMENT, &gFragmentShaders[static_cast<int>(program)]))
			return false;
	}
	return true;
}

// Parses, translates, compiles and uploads the runtime preset selected in the
// options. The preset chain is fully independent of the baked-in filter
// pipelines; a failure here is a soft failure (the session falls back per the
// regular contract).
bool EnsurePresetRunner()
{
	// The selected preset file: the path stored alongside the filter value
	// is the single source of truth. The filter value itself is only an
	// index into the browser's CURRENT folder listing, so it must never be
	// resolved against a full-tree scan -- that loaded unrelated presets
	// from other folders.
	std::string path;
	const std::string &storedPath = GetOptions().Graphics.shaderFilterPreset.value;
	if (!storedPath.empty())
		path = preset::ResolvePresetPath(storedPath);
	if (path.empty()) {
		if (storedPath.empty())
			LogFileLine("preset load failed: no preset path is stored for the selected custom filter");
		else
			LogFileLine(StrCat("preset load failed: preset file not found in the shaders folder: ", storedPath));
		return false;
	}
	if (gPresetRunner.IsLoaded()) {
		if (gLoadedPresetPath == path)
			return true;
		// The selection moved to a different preset: rebuild the chain from
		// scratch instead of presenting the previously loaded one.
		LogFileLine(StrCat("preset changed: ", gLoadedPresetPath, " -> ", path));
		gPresetRunner.Unload();
		gLoadedPresetPath.clear();
	}

	preset::Preset preset;
	std::string error;
	if (!preset::ParsePreset(path, preset, error)) {
		LogFileLine(StrCat("preset parse failed: ", error));
		return false;
	}
	gPresetDisplayName = preset.displayName;

	const char *driver = SDL_GetGPUDeviceDriver(gDevice);
	if (!gPresetRunner.Load(gDevice, driver != nullptr ? driver : "", preset, error)) {
		LogFileLine(StrCat("preset load failed (", preset.displayName, "): ", error));
		gPresetRunner.Unload();
		gLoadedPresetPath.clear();
		return false;
	}
	gLoadedPresetPath = path;
	LogOk(StrCat("preset '", preset.displayName, "' loaded (", preset.passes.size(), " passes)"));
	return true;
}

bool EnsureFilterResources(ShaderFilter filter)
{
	if (!EnsureSamplers() || !EnsureShaders())
		return false;
	LogOk("samplers and shaders ready");

	if (IsCustomFilter(filter)) {
		if (!EnsurePresetRunner())
			return false;
	}

	if (gBlitPipeline == nullptr) {
		const SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(gDevice, gWindow);
		if (swapchainFormat == SDL_GPU_TEXTUREFORMAT_INVALID) {
			// The swapchain is not ready yet (claimed but never
			// presented to). Build the blit pipeline lazily on the
			// first present instead of guessing the format here.
			LogFileLine("swapchain format unavailable at init; deferring blit pipeline creation");
			return true;
		}
		if (!EnsureBlitPipeline(swapchainFormat))
			return false;
	}
	return true;
}

void ReleaseFrameResources()
{
	for (SDL_GPUTexture **texture : { &gGameTex }) {
		if (*texture != nullptr) {
			SDL_ReleaseGPUTexture(gDevice, *texture);
			*texture = nullptr;
		}
	}
	gGameWidth = 0;
	gGameHeight = 0;
}

bool EnsureFrameResources(int gameWidth, int gameHeight)
{
	if (gGameTex != nullptr && gGameWidth == gameWidth && gGameHeight == gameHeight)
		return true;
	ReleaseFrameResources();
	gGameWidth = gameWidth;
	gGameHeight = gameHeight;

	const SDL_GPUTextureFormat gameFormat = GameTextureFormat();
	gGameTex = CreateTexture(gameFormat, gameWidth, gameHeight,
	    SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
	if (gGameTex == nullptr) {
		LogFailHard("frame resource creation failed");
		ReleaseFrameResources();
		return false;
	}
	return true;
}

bool RenderPass(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, Uint32 targetWidth, Uint32 targetHeight,
    SDL_GPUGraphicsPipeline *pipeline, const FragUniforms &uniforms,
    const SDL_GPUTextureSamplerBinding *bindings, Uint32 numBindings,
    const SDL_GPUViewport *viewport)
{
	SDL_GPUColorTargetInfo colorInfo {};
	colorInfo.texture = target;
	colorInfo.clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
	colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
	colorInfo.store_op = SDL_GPU_STOREOP_STORE;
	SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmd, &colorInfo, 1, nullptr);
	if (renderPass == nullptr) {
		LogFailHard("render pass creation failed");
		return false;
	}
	SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
	if (numBindings > 0)
		SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, numBindings);
	SDL_PushGPUFragmentUniformData(cmd, 0, &uniforms, sizeof(uniforms));
	if (viewport != nullptr)
		SDL_SetGPUViewport(renderPass, viewport);
	SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
	SDL_EndGPURenderPass(renderPass);
	return true;
}

} // namespace

void PreloadD3DCompiler()
{
	gpu_d3dcompile::PreloadSystemCompiler();
}

void LogPhase(std::string_view line)
{
	LogFileLine(line);
}

bool ScalerWanted()
{
	return *GetOptions().Graphics.shaderFilter != ShaderFilter::None;
}

bool IsActive()
{
	return gDevice != nullptr && gWindow != nullptr && gFilter != ShaderFilter::None;
}

bool HasFailed()
{
	return gFailed;
}

bool WindowHasSwapchainHistory(SDL_Window *window)
{
	return window != nullptr && gLastClaimedWindow == window;
}

// Creates the GPU device and selects the shader format for its backend.
// `name` forces a specific backend (e.g. "vulkan") or auto-selects when null.
// The DEVILUTIONX_GPU_BACKEND environment variable overrides both, which is
// useful for troubleshooting a broken D3D12 or Vulkan driver.
static bool CreateDevice(const char *name)
{
	const char *backendOverride = SDL_getenv("DEVILUTIONX_GPU_BACKEND");
	if (backendOverride != nullptr && *backendOverride == '\0')
		backendOverride = nullptr;
	if (backendOverride != nullptr)
		LogFileLine(StrCat("backend override from DEVILUTIONX_GPU_BACKEND: ", backendOverride));

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN, true);
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true);
	// Ask SDL to log the adapter name and driver version; the log capture
	// installed below redirects these messages into the diagnostic file.
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN, true);
	if (backendOverride != nullptr || name != nullptr)
		SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,
		    backendOverride != nullptr ? backendOverride : name);
	gDevice = SDL_CreateGPUDeviceWithProperties(props);
	SDL_DestroyProperties(props);
	if (gDevice == nullptr)
		return false;
	const char *driver = SDL_GetGPUDeviceDriver(gDevice);
	if (driver != nullptr && std::strcmp(driver, "vulkan") == 0) {
		gShaderFormat = SDL_GPU_SHADERFORMAT_SPIRV;
	} else if (driver != nullptr && std::strcmp(driver, "direct3d12") == 0) {
#ifdef _WIN32
		gShaderFormat = SDL_GPU_SHADERFORMAT_DXBC;
#else
		LogFailHard("D3D12 backend only supported on Windows");
		Shutdown();
		return false;
#endif
	} else if (driver != nullptr && std::strcmp(driver, "metal") == 0) {
		gShaderFormat = SDL_GPU_SHADERFORMAT_MSL;
	} else {
		LogFailHard(StrCat("unsupported GPU driver: ", driver != nullptr ? driver : "unknown"));
		Shutdown();
		return false;
	}
	Log("GPU scaler: using the {} backend", driver);
	LogFileLine(StrCat("device created, backend=", driver != nullptr ? driver : "unknown"));
	return true;
}

bool Reinit(SDL_Window *window)
{
#ifdef _WIN32
	gpu_d3dcompile::InstallCrashGuard();
#endif
	if (gFailed) {
		LogFileLine("Reinit skipped: the scaler failed earlier this session");
		gFilter = ShaderFilter::None;
		return false;
	}
	gFilter = *GetOptions().Graphics.shaderFilter;
	if (gFilter == ShaderFilter::None) {
		Shutdown();
		return false;
	}
	LogFileLine(StrCat("Reinit: filter=", static_cast<int>(gFilter),
	    " (0=Off, >=100=custom preset)"));

	if (gDevice == nullptr) {
		if (!CreateDevice(/*name=*/nullptr)) {
			LogFailHard("SDL_CreateGPUDevice failed");
			gFilter = ShaderFilter::None;
			return false;
		}
	}

	if (gWindow != window) {
		if (gWindow != nullptr)
			SDL_ReleaseWindowFromGPUDevice(gDevice, gWindow);
		gWindow = window;
		bool claimed = SDL_ClaimWindowForGPUDevice(gDevice, gWindow);
#ifdef _WIN32
		if (!claimed && gShaderFormat == SDL_GPU_SHADERFORMAT_DXBC) {
			// DXGI can refuse the D3D12 swapchain creation even on a
			// freshly recreated window (driver quirk / flip-model
			// ownership rules). Retry once with the Vulkan backend
			// before giving up; the shaders ship with prebuilt
			// SPIR-V blobs, so no recompilation is needed.
			LogFileLine("claim failed on direct3d12, retrying with the vulkan backend");
			Shutdown();
			gWindow = window;
			gFilter = *GetOptions().Graphics.shaderFilter;
			if (CreateDevice(/*name=*/"vulkan"))
				claimed = SDL_ClaimWindowForGPUDevice(gDevice, gWindow);
		}
#endif
		if (!claimed) {
			LogFailHard("SDL_ClaimWindowForGPUDevice failed");
			gWindow = nullptr;
			Shutdown();
			return false;
		}
		gLastClaimedWindow = gWindow;
		const bool vsync = *GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync;
		if (!SDL_SetGPUSwapchainParameters(gDevice, gWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
		        vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE)) {
			LogFail("SDL_SetGPUSwapchainParameters failed");
		}
		LogFileLine(StrCat("window claimed, swapchain format=", static_cast<int>(SDL_GetGPUSwapchainTextureFormat(gDevice, gWindow)),
		    " vsync=", vsync ? "1" : "0"));
		// The swapchain format may have changed with the new window.
		if (gBlitPipeline != nullptr) {
			SDL_ReleaseGPUGraphicsPipeline(gDevice, gBlitPipeline);
			gBlitPipeline = nullptr;
			gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
		}
	}

	if (!EnsureFilterResources(gFilter)) {
		if (IsCustomFilter(gFilter)) {
			// A runtime preset that fails on every backend must not take the
			// GPU scaler (and the game) down: turn the filter off for this
			// session instead. Picking another preset in Settings retries the
			// load from scratch.
			LogFileLine("custom preset failed to load; Shader Filter is off for this session");
			gFilter = ShaderFilter::None;
			Shutdown();
			return false;
		}
		// A shader compile or pipeline creation failure is often
		// backend-specific (e.g. a broken d3dcompiler). Retry once on
		// the Vulkan backend before giving up on the GPU path.
		const char *driver = gDevice != nullptr ? SDL_GetGPUDeviceDriver(gDevice) : nullptr;
		if (driver != nullptr && std::strcmp(driver, "vulkan") != 0) {
			LogFileLine(StrCat("filter resource initialization failed on ", driver,
			    "; retrying once with the vulkan backend"));
			Shutdown();
			gFilter = *GetOptions().Graphics.shaderFilter;
			if (CreateDevice(/*name=*/"vulkan")) {
				gWindow = window;
				if (SDL_ClaimWindowForGPUDevice(gDevice, gWindow)) {
					gLastClaimedWindow = gWindow;
					const bool vsync = *GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync;
					if (!SDL_SetGPUSwapchainParameters(gDevice, gWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
					        vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE)) {
						LogFail("SDL_SetGPUSwapchainParameters failed");
					}
					if (EnsureFilterResources(gFilter))
						return true;
				}
			}
		}
		LogFailHard("filter resource initialization failed");
		Shutdown();
		return false;
	}
	return true;
}

void Shutdown()
{
	LogFileLine("Shutdown");
	if (gDevice == nullptr) {
		gWindow = nullptr;
		gFilter = ShaderFilter::None;
		return;
	}
	ReleaseFrameResources();
	ReleaseFilterResources();
	gPresetRunner.Unload();
	gLoadedPresetPath.clear();
	gPresetDisplayName.clear();
	if (gNearestSampler != nullptr) {
		SDL_ReleaseGPUSampler(gDevice, gNearestSampler);
		gNearestSampler = nullptr;
	}
	if (gLinearSampler != nullptr) {
		SDL_ReleaseGPUSampler(gDevice, gLinearSampler);
		gLinearSampler = nullptr;
	}
	if (gWindow != nullptr) {
		SDL_ReleaseWindowFromGPUDevice(gDevice, gWindow);
		gWindow = nullptr;
	}
	SDL_DestroyGPUDevice(gDevice);
	gDevice = nullptr;
	gShaderFormat = SDL_GPU_SHADERFORMAT_INVALID;
	gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
	gFilter = ShaderFilter::None;
	// gLastClaimedWindow is intentionally NOT cleared: the (possibly still
	// alive) window keeps its DXGI swapchain history on Windows.
}

void PresentFrame(const SDL_Surface &frame)
{
	if (!IsActive())
		return;

	// Heartbeat: lets a remote log show whether the present loop is alive
	// (UI responsive) or stalled (main thread stuck elsewhere), and counts
	// frames skipped due to an unavailable swapchain image.
	static Uint32 sLastHeartbeat = 0;
	static unsigned long sHeartbeatFrames = 0;
	static unsigned long sSkippedFrames = 0;
	static unsigned long sTotalFrames = 0;
	++sHeartbeatFrames;
	++sTotalFrames;
	{
		const Uint32 now = SDL_GetTicks();
		if (sLastHeartbeat == 0)
			sLastHeartbeat = now;
		if (now - sLastHeartbeat >= 5000) {
			LogFileLine(StrCat("heartbeat: ", sHeartbeatFrames, " frames (", sSkippedFrames,
			    " skipped, ", sTotalFrames, " total, filter=",
			    static_cast<int>(gFilter), ")"));
			sLastHeartbeat = now;
			sHeartbeatFrames = 0;
			sSkippedFrames = 0;
		}
	}

	static bool loggedFirstPresentStart = false;
	if (!loggedFirstPresentStart) {
		loggedFirstPresentStart = true;
		LogFileLine("present: first frame, acquiring command buffer and swapchain");
	}

	const ShaderFilter filter = gFilter;
	const bool isCustom = IsCustomFilter(filter);

	// Frame duration for FrameTime, measured around the whole present.
	static Uint32 sLastFrameTick = 0;
	const Uint32 nowTick = SDL_GetTicks();
	Uint64 frameDeltaUs = 0;
	if (sLastFrameTick != 0)
		frameDeltaUs = static_cast<Uint64>(nowTick - sLastFrameTick) * 1000U;
	sLastFrameTick = nowTick;

	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gDevice);
	if (cmd == nullptr) {
		LogFailHard("SDL_AcquireGPUCommandBuffer failed");
		return;
	}

	SDL_GPUTexture *swapchainTexture = nullptr;
	Uint32 swapchainWidth = 0;
	Uint32 swapchainHeight = 0;
	// Non-blocking on purpose: the blocking acquire variant can stall the
	// main thread indefinitely (minimized window, resize, driver quirk),
	// which looks like a frozen UI while audio keeps playing. Skipping a
	// frame costs nothing; the next frame retries.
	if (!SDL_AcquireGPUSwapchainTexture(cmd, gWindow, &swapchainTexture, &swapchainWidth, &swapchainHeight)
	    || swapchainTexture == nullptr) {
		// Window minimized or no swapchain image available this frame.
		++sSkippedFrames;
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}

	const int gameWidth = frame.w;
	const int gameHeight = frame.h;

	static bool loggedFirstPresent = false;
	if (!loggedFirstPresent) {
		loggedFirstPresent = true;
		LogFileLine(StrCat("first present: game=", gameWidth, "x", gameHeight,
		    " swapchain=", swapchainWidth, "x", swapchainHeight));
	}

	// The swapchain format can change (window resize, fullscreen switch).
	// Rebuild the blit pipeline for the current format when needed.
	if (!EnsureBlitPipeline(SDL_GetGPUSwapchainTextureFormat(gDevice, gWindow))) {
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}

	// Fit the game frame into the window, honoring the Integer Scaling option.
	float fitScale = std::min(static_cast<float>(swapchainWidth) / gameWidth,
	    static_cast<float>(swapchainHeight) / gameHeight);
	if (*GetOptions().Graphics.integerScaling)
		fitScale = std::floor(fitScale);

	float destScale;
	if (isCustom) {
		// Preset chains size their own outputs via the libretro scale
		// rules; the final fit is computed after the chain has run,
		// from the actual output dimensions.
		destScale = 1.0F;
	} else {
		// Builtin HQ2x/ScaleFX filters were removed; only runtime
		// presets render here.
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}

	if (!EnsureFrameResources(gameWidth, gameHeight)) {
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}
	// Upload the frame through a fresh transfer buffer created per frame.
	// The buffer is released when this function returns (RAII below); SDL
	// defers the actual destruction until the GPU is done with it. This
	// avoids overwriting a long-lived buffer that the GPU may still be
	// reading from on the previous frame's copy pass.
	SDL_GPUTransferBufferCreateInfo transferInfo {};
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferInfo.size = static_cast<Uint32>(gameWidth) * gameHeight * 4;
	SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(gDevice, &transferInfo);
	if (transfer == nullptr) {
		LogFailHard("frame transfer buffer creation failed");
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}
	struct TransferBufferReleaser {
		SDL_GPUDevice *device;
		SDL_GPUTransferBuffer *buffer;
		~TransferBufferReleaser()
		{
			SDL_ReleaseGPUTransferBuffer(device, buffer);
		}
	} transferReleaser { gDevice, transfer };

	{
		const int bytesPerRow = gameWidth * 4;
		auto *dst = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(gDevice, transfer, false));
		const auto *src = static_cast<const uint8_t *>(frame.pixels);
		if (frame.pitch == bytesPerRow) {
			std::memcpy(dst, src, static_cast<size_t>(bytesPerRow) * gameHeight);
		} else {
			for (int y = 0; y < gameHeight; ++y)
				std::memcpy(dst + static_cast<size_t>(y) * bytesPerRow, src + static_cast<size_t>(y) * frame.pitch, bytesPerRow);
		}
		SDL_UnmapGPUTransferBuffer(gDevice, transfer);

		SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
		SDL_GPUTextureTransferInfo transferSrc {};
		transferSrc.transfer_buffer = transfer;
		transferSrc.offset = 0;
		transferSrc.pixels_per_row = gameWidth;
		transferSrc.rows_per_layer = gameHeight;
		SDL_GPUTextureRegion region {};
		region.texture = gGameTex;
		region.w = gameWidth;
		region.h = gameHeight;
		region.d = 1;
		SDL_UploadToGPUTexture(copyPass, &transferSrc, &region, false);
		SDL_EndGPUCopyPass(copyPass);
	}

	FragUniforms uniforms {};
	uniforms.sourceSize[0] = static_cast<float>(gameWidth);
	uniforms.sourceSize[1] = static_cast<float>(gameHeight);
	uniforms.sourceSize[2] = 1.0F / gameWidth;
	uniforms.sourceSize[3] = 1.0F / gameHeight;

	// Texture presented by the filter chain: runtime presets produce
	// their own final texture.
	SDL_GPUTexture *presentTex = nullptr;
	Uint32 presentWidth = 0;
	Uint32 presentHeight = 0;

	if (isCustom) {
		SDL_GPUTexture *outTex = nullptr;
		Uint32 outWidth = 0;
		Uint32 outHeight = 0;
		if (!gPresetRunner.Run(cmd, gGameTex, gameWidth, gameHeight, swapchainWidth, swapchainHeight,
		        frameDeltaUs, &outTex, &outWidth, &outHeight)) {
			SDL_CancelGPUCommandBuffer(cmd);
			return;
		}
		// Fit the chain output into the window, honoring Integer Scaling.
		float outFit = std::min(static_cast<float>(swapchainWidth) / static_cast<float>(outWidth),
		    static_cast<float>(swapchainHeight) / static_cast<float>(outHeight));
		if (*GetOptions().Graphics.integerScaling)
			outFit = std::floor(outFit);
		destScale = outFit;
		presentTex = outTex;
		presentWidth = outWidth;
		presentHeight = outHeight;
	}

	// Blit the scaled image into the swapchain with letterboxing.
	int destWidth;
	int destHeight;
	destWidth = static_cast<int>(std::lround(static_cast<float>(presentWidth) * destScale));
	destHeight = static_cast<int>(std::lround(static_cast<float>(presentHeight) * destScale));
	// Rounding can overshoot the swapchain by a pixel; clamp so the
	// viewport never exceeds the render target.
	if (destWidth > static_cast<int>(swapchainWidth))
		destWidth = static_cast<int>(swapchainWidth);
	if (destHeight > static_cast<int>(swapchainHeight))
		destHeight = static_cast<int>(swapchainHeight);
	if (destWidth <= 0 || destHeight <= 0) {
		// Window smaller than the scaled image can occupy; skip.
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}
	SDL_GPUViewport viewport {};
	viewport.x = (swapchainWidth - destWidth) / 2.0F;
	viewport.y = (swapchainHeight - destHeight) / 2.0F;
	viewport.w = destWidth;
	viewport.h = destHeight;
	viewport.min_depth = 0.0F;
	viewport.max_depth = 1.0F;
	SDL_GPUTextureSamplerBinding blitBindings[1] = { { presentTex, gLinearSampler } };
	uniforms.params[0] = 0.0F;
	if (!RenderPass(cmd, swapchainTexture, swapchainWidth, swapchainHeight, gBlitPipeline, uniforms, blitBindings, 1, &viewport)) {
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}

	if (!SDL_SubmitGPUCommandBuffer(cmd))
		LogFailHard("SDL_SubmitGPUCommandBuffer failed");
}

namespace {

std::string gOsdText;

const char *FilterDisplayName(ShaderFilter filter)
{
	switch (filter) {
	case ShaderFilter::None:
		break;
	}
	if (IsCustomFilter(filter))
		return gPresetDisplayName.empty() ? "Custom" : gPresetDisplayName.c_str();
	return "Off";
}

std::string PrettyBackendName(std::string_view driver)
{
	if (driver == "vulkan")
		return "Vulkan";
	if (driver == "direct3d12")
		return "Direct3D 12";
	if (driver == "metal")
		return "Metal";
	return std::string(driver);
}

} // namespace

void ShowFilterOsd()
{
	const int value = static_cast<int>(*GetOptions().Graphics.shaderFilter);
	std::string text;
	if (value == static_cast<int>(ShaderFilter::None)) {
		// No filter active: show no label at all.
		text.clear();
	} else if (!IsActive()) {
		text = StrCat("Shader Filter: ", FilterDisplayName(static_cast<ShaderFilter>(value)), " (unavailable)");
	} else {
		text = StrCat("Shader Filter: ", FilterDisplayName(static_cast<ShaderFilter>(value)),
		    " (", PrettyBackendName(SDL_GetGPUDeviceDriver(gDevice)), ")");
	}
	if (text != gOsdText) {
		gOsdText = std::move(text);
		LogFileLine(StrCat("osd: ", gOsdText.empty() ? "(hidden)" : gOsdText.c_str()));
	}
}

std::string_view FilterOsdText()
{
	return gOsdText;
}

void DrawFilterOsd(const Surface &out)
{
	const std::string_view text = FilterOsdText();
	if (text.empty())
		return;
	// FontSizeDialog + ColorDialogWhite is the engine's standard in-game
	// dialog text combo and reads correctly under BOTH the menu palette
	// (raw glyph index 224 = white in ui_art palettes) and the game
	// palettes (gamedialogwhite.trn maps 224 -> PAL16_GRAY white ramp).
	// Plain ColorWhite maps into palette entries that are pure
	// red/green/blue/... primaries in the menu palette ("rainbow" text).
	constexpr GameFontTables OsdFontSize = FontSizeDialog;
	constexpr int Margin = 8;
	const int textWidth = GetLineWidth(text, OsdFontSize);
	const int textHeight = GetLineHeight(text, OsdFontSize);
	const Point position { out.w() - Margin - textWidth, out.h() - Margin - textHeight };
	DrawString(out, text, position, { .flags = UiFlags::ColorDialogWhite | UiFlags::FontSizeDialog });
}

} // namespace gpu_scaler
} // namespace devilution
#endif // !USE_SDL1
#endif // USE_GPU_SCALER
