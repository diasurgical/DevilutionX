// Written by VTSTech (Nigel Todman) (veritas@vts-tech.org)
// AI Assisted by Super-Z (GLM-5.3-Flash)
// License: MIT

#include "engine/render/gpu/gpu_scaler.hpp"

#ifdef USE_GPU_SCALER
#ifndef USE_SDL1

// volk.h must come first: it pulls in <vulkan/vulkan.h> with VK_NO_PROTOTYPES
// (defined by the slv-volk target that libslang-vk exposes) and provides every
// entry point as a runtime-loaded function pointer, so neither the scaler nor
// the game needs a Vulkan SDK or a link-time loader dependency. The exe's volk
// copy is initialized HERE (volkInitialize + volkLoad* below): with
// GPU_SCALER_SHARED=ON libslang-vk is a DLL with its own separate volk copy,
// so Device::create's initialization inside the DLL does not reach this copy —
// in the static build the two used to be the same table.
#include <volk.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <slangvk/core/preset_parser.hpp>
#include <slangvk/runner.hpp>

#include "config.h"
#include "engine/render/gpu/gpu_log_sanitize.hpp"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

namespace devilution {
namespace gpu_scaler {

// Forward declaration of the public API teardown: the anonymous-namespace
// helpers below route soft preset failures through it.
void Shutdown();

namespace {

ShaderFilter gFilter = ShaderFilter::None;

SDL_Window *gWindow = nullptr;

// Set when the scaler hits an unrecoverable error. Session-sticky: the game
// reverts to the regular renderer instead of retrying a broken GPU path.
bool gFailed = false;
bool gLoggedFailure = false;

// The Vulkan context. The libslang-vk Device owns the instance/device/queue
// (created through libslang-vk's volk runtime loader); the swapchain and its
// views/images are host-side because the presentation protocol (timed acquire,
// present after the library's synchronous render) belongs to the host, per
// the library's SwapchainFrame contract.
std::optional<slv::Device> gDevice;
VkSurfaceKHR gSurface = VK_NULL_HANDLE;
VkSwapchainKHR gSwapchain = VK_NULL_HANDLE;
std::vector<VkImage> gSwapchainImages;
std::vector<VkImageView> gSwapchainViews;
VkFormat gSwapchainFormat = VK_FORMAT_UNDEFINED;
VkExtent2D gSwapchainExtent { 0, 0 };
VkSemaphore gAcquireSemaphore = VK_NULL_HANDLE;
// Set when the next Present must rebuild the swapchain first (out-of-date or
// suboptimal acquire/present, resize, present-mode change).
bool gSwapchainDirty = false;
bool gVsync = false;

// The loaded slang preset chain (parse + translate + glslang compile + the
// Vulkan pass runner all live inside libslang-vk).
std::optional<slv::Preset> gPreset;
std::optional<slv::Chain> gChain;
std::string gPresetDisplayName;
// The preset file the running chain was built from (resolved absolute path).
std::string gLoadedPresetPath;
// FEAT-01: the file stat snapshot taken when the chain was built. A non-zero
// mtime enables the changed-on-disk reload check in EnsurePresetLoaded; zero
// means the file could not be statted at load time and the chain never
// auto-reloads.
Uint64 gLoadedPresetMtimeNs = 0;
Uint64 gLoadedPresetFileSize = 0;
// FEAT-03: why the last preset load attempt failed (full text; the OSD shows
// its first line). Deliberately survives the failure Shutdown so the OSD can
// name the reason; overwritten by the next attempt, cleared on success.
std::string gPresetLastError;

// The chain input image (the converted RGBA game frame, uploaded per present).
std::optional<slv::Image> gGameImage;
// Tightly packed copy of the SDL_Surface frame: slv::Image::update requires
// pitch == width * 4, which the game surface does not promise.
std::vector<uint8_t> gStaging;
int gGameWidth = 0;
int gGameHeight = 0;

// Geometry of the last actually-presented frame: the game size it was
// uploaded for and the swapchain rect the final pass was drawn into. The
// mouse-coordinate mapping (MapWindowToGame / MapGameToWindow) derives the
// window <-> game transform from it. Cleared with the swapchain.
slv::Rect gLastPresentRect {};

// Sizes the chain was last (re)built or resized for, so Present only calls
// Chain::resize when something actually changed.
uint32_t gChainInputWidth = 0;
uint32_t gChainInputHeight = 0;
uint32_t gChainOutputWidth = 0;
uint32_t gChainOutputHeight = 0;
// Swapchain format the running chain's pipelines were created for (a rebuild
// that changes the surface format forces a chain reload).
VkFormat gChainOutputFormat = VK_FORMAT_UNDEFINED;

uint64_t gFrameCount = 0;

// FEAT-01: clears the loaded-chain identity wherever the chain goes away.
void ClearLoadedPreset()
{
	gLoadedPresetPath.clear();
	gLoadedPresetMtimeNs = 0;
	gLoadedPresetFileSize = 0;
	gChainOutputFormat = VK_FORMAT_UNDEFINED;
}

// ARCH-01: the window-presentation hand-off protocol, in one place.
//
// On Windows, DXGI permits only one flip-model swapchain per HWND, so the SDL
// window alternates ownership between SDL_Renderer (D3D11/GL) and the Vulkan
// swapchain of the scaler. A window presented to by EITHER path keeps that
// swapchain history even after its owner releases it, and the next owner of
// the OTHER kind fails with "Access is denied" (0x80070005) until the window
// is destroyed and recreated (display.cpp RecreateMainRenderWindow).
//
// Protocol (three participants):
//   - gpu_scaler.cpp (here): records the ID of every window it successfully
//     claims (creates a Vulkan surface on) and answers
//     WindowHasSwapchainHistory() for display.cpp. The ID is deliberately
//     NOT cleared on Shutdown(): the (possibly still alive) window keeps its
//     DXGI swapchain history.
//   - display.cpp: before handing the window across paths, recreates it when
//     WindowHasSwapchainHistory() is true (scaler -> renderer direction) or
//     when the renderer owned it (renderer -> scaler direction).
//   - dx.cpp: the failure funnel that routes any scaler error back to the
//     stock renderer for the session.
//
// The history is tracked as explicit state keyed by the SDL window ID, not
// by the SDL_Window pointer: SDL never reuses a window ID within a process,
// so a destroyed window's recorded ID can never match a NEW window that
// happened to be allocated at the recycled address (the ABA false-positive
// the previous pointer-only approach allowed — it forced an unnecessary
// window recreation and could not distinguish "this window had a swapchain"
// from "some window at this address did"). The recorded ID is only ever
// compared, never dereferenced (the window may already be destroyed).
Uint32 gLastClaimedWindowId = 0;

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

// ROB-06: every mutation of gLogFile / gLogBytes (open, rotate, write) must
// hold this mutex. SDL log capture routes SDL's own messages here, and those
// can originate on SDL-internal threads (GPU, audio, validation), so the
// capture callback races the main-thread LogFileLine writes. Without the
// mutex the worst case was use-after-close of the FILE* or a double rotation
// — exactly when users need the log most (an SDL-internal error storm).
// Cheap at this call rate; the file is only touched for diagnostics.
std::mutex gLogMutex;

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

// --- log-file internals (LogClose / LogOpenAppended / RotateLog / LogFile /
// LogFileForWrite): the caller must hold gLogMutex. ---

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
				const std::string header = StrCat("\n=== GPU scaler session (build ", __DATE__, " ", __TIME__, ", exe ", PROJECT_VERSION, ") ===\n");
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
	// SEC-04: log text includes untrusted strings (preset paths, pragma
	// descriptions, glslang info logs). Control bytes would let a hostile
	// preset forge log lines or inject terminal escapes. Sanitized outside
	// the lock (pure string work), then written under the lock so the
	// rotation state cannot change between the cap check and the write.
	const std::string safe = SanitizeLogLine(line);
	const std::lock_guard<std::mutex> lock(gLogMutex);
	if (FILE *file = LogFileForWrite(line.size() + 16); file != nullptr) {
		std::fprintf(file, "[%10u ms] %.*s\n", static_cast<unsigned>(SDL_GetTicks()), static_cast<int>(safe.size()), safe.data());
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

// A failure that cannot be recovered from within the Vulkan path: the scaler
// is torn down and the game falls back to the regular renderer for this
// session.
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
	// Chain to the previous output function BEFORE taking gLogMutex: the
	// previous handler is arbitrary user code and must never observe (or
	// recurse into) our log lock, and SDL-internal messages can originate
	// on SDL's own threads (ROB-06) — the write below is what needs the
	// mutual exclusion against the main-thread LogFileLine writers.
	if (gPrevSdlLogFunction != nullptr)
		gPrevSdlLogFunction(gPrevSdlLogUserdata, category, priority, message);
	const size_t messageLen = message != nullptr ? std::strlen(message) : 0;
	const std::lock_guard<std::mutex> lock(gLogMutex);
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

// Pixel format of the converted game frame: the same mapping the previous
// SDL_GPU path used for its game texture. The frame arrives in the display
// texture format configured for the build.
VkFormat GameTextureVkFormat()
{
	switch (DEVILUTIONX_DISPLAY_TEXTURE_FORMAT) {
	case SDL_PIXELFORMAT_ABGR8888:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case SDL_PIXELFORMAT_ARGB8888:
	case SDL_PIXELFORMAT_XRGB8888:
	default:
		return VK_FORMAT_B8G8R8A8_UNORM;
	}
}

// Timeout for the swapchain image acquire: non-blocking on purpose. A
// blocking acquire can stall the main thread indefinitely (minimized window,
// resize, driver quirk), which looks like a frozen UI while audio keeps
// playing. A timed-out acquire just skips the frame; the next frame retries.
constexpr uint64_t kAcquireTimeoutNs = 50'000'000; // 50 ms

// Destroys the swapchain and its image views. Must be called while gDevice
// holds a valid device. Waits for the device to go idle first so no pending
// frame still references the views.
void DestroySwapchain()
{
	if (gDevice && gDevice->device() != VK_NULL_HANDLE)
		vkDeviceWaitIdle(gDevice->device());
	for (const VkImageView view : gSwapchainViews) {
		if (view != VK_NULL_HANDLE)
			vkDestroyImageView(gDevice->device(), view, nullptr);
	}
	gSwapchainViews.clear();
	gSwapchainImages.clear();
	if (gSwapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(gDevice->device(), gSwapchain, nullptr);
		gSwapchain = VK_NULL_HANDLE;
	}
	gSwapchainFormat = VK_FORMAT_UNDEFINED;
	gSwapchainExtent = { 0, 0 };
	// The cached present rect describes the destroyed swapchain; the mouse
	// mapping must not keep using it.
	gLastPresentRect = slv::Rect {};
}

// Releases everything that is bound to the current window (swapchain, views,
// acquire semaphore, surface) while keeping the parked libslang-vk device
// alive for reuse by the next Reinit (ROB-26 hand-back semantics).
void ReleaseWindowResources()
{
	if (!gDevice || gDevice->device() == VK_NULL_HANDLE) {
		gSurface = VK_NULL_HANDLE;
		return;
	}
	if (gAcquireSemaphore != VK_NULL_HANDLE) {
		vkDestroySemaphore(gDevice->device(), gAcquireSemaphore, nullptr);
		gAcquireSemaphore = VK_NULL_HANDLE;
	}
	DestroySwapchain();
	if (gSurface != VK_NULL_HANDLE) {
		// SDL_Vulkan_DestroySurface wraps vkDestroySurfaceKHR with the same
		// instance the surface was created on.
		SDL_Vulkan_DestroySurface(gDevice->instance(), gSurface, nullptr);
		gSurface = VK_NULL_HANDLE;
	}
	gWindow = nullptr;
}

void RecreateAcquireSemaphore()
{
	if (!gDevice || gDevice->device() == VK_NULL_HANDLE)
		return;
	if (gAcquireSemaphore != VK_NULL_HANDLE)
		vkDestroySemaphore(gDevice->device(), gAcquireSemaphore, nullptr);
	gAcquireSemaphore = VK_NULL_HANDLE;
	VkSemaphoreCreateInfo semInfo {};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if (vkCreateSemaphore(gDevice->device(), &semInfo, nullptr, &gAcquireSemaphore) != VK_SUCCESS) {
		// The next acquire will fail and surface the state; counted as a
		// skipped frame by the caller.
		LogFail("acquire semaphore recreation failed");
	}
}

// (Re)creates the swapchain for the current window surface. Lifted from the
// validated slangvk spike backend and extended with the present-mode
// fallback ladder the SDL_GPU path used (IMMEDIATE -> MAILBOX -> FIFO).
bool CreateSwapchain(std::string &error)
{
	const VkPhysicalDevice physicalDevice = gDevice->physicalDevice();

	VkSurfaceCapabilitiesKHR caps {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, gSurface, &caps);

	// A minimized window reports a zero extent; keep the old swapchain and
	// let Present skip frames until the window is back.
	if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
		error = "surface extent is zero (window minimized?)";
		return false;
	}
	VkExtent2D newExtent = caps.currentExtent;
	if (newExtent.width == std::numeric_limits<uint32_t>::max()
	    || newExtent.height == std::numeric_limits<uint32_t>::max()) {
		int pixelWidth = 0;
		int pixelHeight = 0;
		SDL_GetWindowSizeInPixels(gWindow, &pixelWidth, &pixelHeight);
		newExtent.width = static_cast<uint32_t>(std::max(pixelWidth, 1));
		newExtent.height = static_cast<uint32_t>(std::max(pixelHeight, 1));
		newExtent.width = std::clamp(newExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
		newExtent.height = std::clamp(newExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	}

	// Surface format: prefer an 8-bit UNORM format with the non-linear SRGB
	// color space, matching the SDR composition of the previous SDL_GPU
	// swapchain path.
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, gSurface, &formatCount, nullptr);
	if (formatCount == 0) {
		error = "no supported surface formats";
		return false;
	}
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, gSurface, &formatCount, formats.data());
	VkSurfaceFormatKHR chosen = formats[0];
	for (const VkSurfaceFormatKHR &candidate : formats) {
		if (candidate.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			continue;
		if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM || candidate.format == VK_FORMAT_R8G8B8A8_UNORM) {
			chosen = candidate;
			break;
		}
		if (chosen.format == VK_FORMAT_UNDEFINED)
			chosen = candidate;
	}

	// Present mode: FIFO is always supported and is the vsync choice. For
	// immediate presentation prefer IMMEDIATE (tearing allowed), then
	// MAILBOX, matching the SDL_GPU present-mode fallback ladder.
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, gSurface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, gSurface, &presentModeCount, presentModes.data());
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (!gVsync) {
		const bool hasImmediate = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end();
		const bool hasMailbox = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end();
		if (hasImmediate)
			presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		else if (hasMailbox)
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	}

	uint32_t imageCount = caps.minImageCount + 1;
	if (imageCount < 3)
		imageCount = 3; // triple buffering when the surface allows it
	if (caps.maxImageCount != 0 && imageCount > caps.maxImageCount)
		imageCount = caps.maxImageCount;

	VkSwapchainCreateInfoKHR info {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = gSurface;
	info.minImageCount = imageCount;
	info.imageFormat = chosen.format;
	info.imageColorSpace = chosen.colorSpace;
	info.imageExtent = newExtent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = caps.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = presentMode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = gSwapchain;

	VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(gDevice->device(), &info, nullptr, &newSwapchain) != VK_SUCCESS) {
		error = "vkCreateSwapchainKHR failed";
		return false;
	}

	// Keep the old swapchain alive only until the new one exists.
	const VkSwapchainKHR oldSwapchain = gSwapchain;
	gSwapchain = newSwapchain;

	uint32_t actualImageCount = 0;
	vkGetSwapchainImagesKHR(gDevice->device(), gSwapchain, &actualImageCount, nullptr);
	std::vector<VkImage> newImages(actualImageCount);
	vkGetSwapchainImagesKHR(gDevice->device(), gSwapchain, &actualImageCount, newImages.data());
	std::vector<VkImageView> newViews(actualImageCount, VK_NULL_HANDLE);
	for (uint32_t i = 0; i < actualImageCount; ++i) {
		VkImageViewCreateInfo viewInfo {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = newImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = chosen.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(gDevice->device(), &viewInfo, nullptr, &newViews[i]) != VK_SUCCESS) {
			newViews.resize(i); // only destroy the views that were created
			for (const VkImageView view : newViews)
				vkDestroyImageView(gDevice->device(), view, nullptr);
			gSwapchain = oldSwapchain;
			vkDestroySwapchainKHR(gDevice->device(), newSwapchain, nullptr);
			error = "swapchain image view creation failed";
			return false;
		}
	}

	if (oldSwapchain != VK_NULL_HANDLE) {
		for (const VkImageView view : gSwapchainViews)
			vkDestroyImageView(gDevice->device(), view, nullptr);
		vkDestroySwapchainKHR(gDevice->device(), oldSwapchain, nullptr);
	}
	gSwapchainImages = std::move(newImages);
	gSwapchainViews = std::move(newViews);
	gSwapchainFormat = chosen.format;
	gSwapchainExtent = newExtent;
	gSwapchainDirty = false;
	return true;
}

// Rebuilds the swapchain when it is dirty (first use, resize, present-mode
// change) or when the surface reports a new extent. Returns false on a
// rebuild failure; `minimized` reports the transient zero-extent state.
bool EnsureSwapchainCurrent(bool &minimized, std::string &error)
{
	minimized = false;
	error.clear();
	if (!gDevice || gSurface == VK_NULL_HANDLE) {
		error = "no Vulkan surface";
		return false;
	}
	VkSurfaceCapabilitiesKHR caps {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gDevice->physicalDevice(), gSurface, &caps);
	if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
		minimized = true; // nothing to do this frame; keep the old swapchain
		return true;
	}
	if (!gSwapchainDirty && gSwapchain != VK_NULL_HANDLE
	    && caps.currentExtent.width == gSwapchainExtent.width
	    && caps.currentExtent.height == gSwapchainExtent.height)
		return true;
	if (!CreateSwapchain(error)) {
		return false;
	}
	// The window size drives both the chain output size and the present
	// rect; resetting the tracking forces the Chain::resize call below.
	gChainOutputWidth = 0;
	gChainOutputHeight = 0;
	return true;
}

// Creates the input image the game frame is uploaded into every present.
bool EnsureGameImage(int width, int height, std::string &error)
{
	if (gGameImage && gGameWidth == width && gGameHeight == height)
		return true;
	gGameImage.reset();
	gStaging.clear();
	gStaging.shrink_to_fit();
	gGameWidth = 0;
	gGameHeight = 0;

	slv::Image::CreateInfo info {};
	info.desc.width = static_cast<uint32_t>(width);
	info.desc.height = static_cast<uint32_t>(height);
	info.desc.format = GameTextureVkFormat();
	// The default usage already covers what the runner needs: sampled by the
	// chain, updated per frame through the staging path, blitted into the
	// history rings / mip-capable copies.
	auto image = slv::Image::create(*gDevice, info);
	if (!image) {
		error = StrCat("input image creation failed: ", image.error().message);
		return false;
	}
	gGameImage.emplace(std::move(*image));
	gGameWidth = width;
	gGameHeight = height;
	return true;
}

// Releases the chain input image and its staging buffer.
void ReleaseFrameResources()
{
	gGameImage.reset();
	gStaging.clear();
	gStaging.shrink_to_fit();
	gGameWidth = 0;
	gGameHeight = 0;
}

// Mirrors libslang-vk's PresetDisplayName (underscore -> space, collapsed,
// trimmed) so the OSD label matches the settings list entry for the same
// file without parsing the preset.
std::string PresetDisplayNameFromPath(std::string_view storedPath)
{
	std::string stem { storedPath };
	const size_t slash = stem.find_last_of("/\\");
	if (slash != std::string::npos)
		stem.erase(0, slash + 1);
	const size_t dot = stem.find_last_of('.');
	if (dot != std::string::npos)
		stem.resize(dot);
	std::replace(stem.begin(), stem.end(), '_', ' ');
	stem.erase(std::unique(stem.begin(), stem.end(), [](char a, char b) { return a == ' ' && b == ' '; }), stem.end());
	const size_t first = stem.find_first_not_of(' ');
	if (first == std::string::npos)
		return stem;
	if (first > 0)
		stem.erase(0, first);
	while (!stem.empty() && stem.back() == ' ')
		stem.pop_back();
	return stem;
}

// Parses, translates, compiles and loads the runtime preset selected in the
// options through libslang-vk (Preset::load does all three; Chain::create
// builds the Vulkan pass runtime). A failure here is a soft failure (the
// session falls back per the regular contract) and its reason is kept for
// the OSD (FEAT-03).
bool EnsurePresetLoaded()
{
	// The selected preset file: the path stored alongside the filter value
	// is the single source of truth. The filter value itself is only an
	// index into the browser's CURRENT folder listing, so it must never be
	// resolved against a full-tree scan -- that loaded unrelated presets
	// from other folders.
	std::string path;
	const std::string &storedPath = GetOptions().Graphics.shaderFilterPreset.value;
	if (!storedPath.empty())
		path = slv::preset::ResolvePresetPath(storedPath);
	if (path.empty()) {
		// FEAT-03: remember why the attempt failed so the OSD can surface it.
		if (storedPath.empty())
			gPresetLastError = "no preset path is stored for the selected custom filter";
		else
			gPresetLastError = StrCat("preset file not found in the shaders folder: ", storedPath);
		LogFileLine(StrCat("preset load failed: ", gPresetLastError));
		return false;
	}
	if (gChain) {
		if (gLoadedPresetPath == path) {
			// FEAT-01: re-selecting the same preset rechecks the stat snapshot
			// taken at load time; a changed mtime or size reloads the chain, so
			// pack edits and failed-load retries land without detouring through
			// another preset. A stat that fails here (file briefly unavailable)
			// keeps the running chain.
			SDL_PathInfo info;
			if (gLoadedPresetMtimeNs == 0 || !SDL_GetPathInfo(path.c_str(), &info)
			    || (info.modify_time == static_cast<SDL_Time>(gLoadedPresetMtimeNs)
				&& info.size == gLoadedPresetFileSize))
				return true;
			LogFileLine(StrCat("preset file changed on disk; reloading: ", path));
		} else {
			// The selection moved to a different preset: rebuild the chain from
			// scratch instead of presenting the previously loaded one.
			LogFileLine(StrCat("preset changed: ", gLoadedPresetPath, " -> ", path));
		}
		gChain.reset();
		gPreset.reset();
		ClearLoadedPreset();
	}

	gPresetDisplayName = PresetDisplayNameFromPath(storedPath);

	// The chain output format/size target the swapchain: pass FBOs keep their
	// preset-derived sizes, only the presentation draw touches the swapchain.
	slv::ChainOptions chainOptions;
	chainOptions.outputWidth = gSwapchainExtent.width;
	chainOptions.outputHeight = gSwapchainExtent.height;
	chainOptions.outputFormat = gSwapchainFormat;

	auto preset = slv::Preset::load(*gDevice, std::filesystem::path(path), {});
	if (!preset) {
		gPresetLastError = preset.error().message;
		LogFileLine(StrCat("preset load failed (", gPresetDisplayName, "): ", gPresetLastError));
		gPreset.reset();
		ClearLoadedPreset();
		return false;
	}
	gPreset.emplace(std::move(*preset));

	auto chain = slv::Chain::create(*gDevice, *gPreset, chainOptions);
	if (!chain) {
		gPresetLastError = chain.error().message;
		LogFileLine(StrCat("preset load failed (", gPresetDisplayName, "): ", gPresetLastError));
		gPreset.reset();
		ClearLoadedPreset();
		return false;
	}
	gChain.emplace(std::move(*chain));
	gChainOutputFormat = chainOptions.outputFormat;
	gLoadedPresetPath = path;
	// FEAT-01: snapshot the file's mtime/size at load time - the anchor the
	// changed-on-disk reload check compares against.
	SDL_PathInfo info;
	gLoadedPresetMtimeNs = 0;
	gLoadedPresetFileSize = 0;
	if (SDL_GetPathInfo(path.c_str(), &info)) {
		gLoadedPresetMtimeNs = static_cast<Uint64>(info.modify_time);
		gLoadedPresetFileSize = info.size;
	}
	gChainInputWidth = 0;
	gChainInputHeight = 0;
	gChainOutputWidth = 0;
	gChainOutputHeight = 0;
	gPresetLastError.clear();
	LogOk(StrCat("preset '", gPresetDisplayName, "' loaded (", gPreset->passCount(), " passes)"));
	return true;
}

// The soft-failure exit used when the selected preset cannot load: turn the
// filter off for this session (picking another preset in Settings retries
// the load from scratch) and keep the failure reason for the OSD.
void FailPresetForSession(std::string_view reason)
{
	LogFileLine(StrCat("preset failed to load; Shader Filter is off for this session (", reason, ")"));
	gFilter = ShaderFilter::None;
	Shutdown();
}

// Creates the libslang-vk device: instance with the platform surface
// extensions SDL requires, a device with swapchain support, and the
// runtime-loaded Vulkan loader (volk). No Vulkan SDK at build or link time.
bool CreateVulkanDevice(SDL_Window *window)
{
	(void)window; // SDL3 lists the platform surface extensions without a window
	uint32_t extensionCount = 0;
	const char *const *extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
	if (extensionNames == nullptr || extensionCount == 0) {
		LogFileLine(StrCat("SDL_Vulkan_GetInstanceExtensions failed: ", SDL_GetError()));
		return false;
	}

	// DEVILUTIONX_GPU_DEBUG=1 enables the Khronos validation layer (when
	// installed): a state the release path would pass through silently then
	// surfaces as a validation error and the scaler's graceful fallback
	// instead of a process abort. Field instrument for silent first-frame
	// crashes.
	const char *debugEnv = SDL_getenv("DEVILUTIONX_GPU_DEBUG");
	const bool debugDevice = debugEnv != nullptr && *debugEnv == '1';
	if (debugDevice)
		LogFileLine("GPU debug mode enabled via DEVILUTIONX_GPU_DEBUG=1 (Vulkan validation layers)");
	// The previous SDL_GPU path honored DEVILUTIONX_GPU_BACKEND to force a
	// backend driver; this backend is Vulkan-only through libslang-vk.
	const char *backendEnv = SDL_getenv("DEVILUTIONX_GPU_BACKEND");
	if (backendEnv != nullptr && *backendEnv != '\0')
		LogFileLine("DEVILUTIONX_GPU_BACKEND is ignored: the scaler presents through libslang-vk (Vulkan only)");

	slv::DeviceOptions options;
	options.appName = PROJECT_NAME;
	options.minApiVersion = VK_API_VERSION_1_1;
	options.validationLayers = debugDevice;
	options.instanceExtensions.assign(extensionNames, extensionNames + extensionCount);
	options.deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	auto device = slv::Device::create(options);
	if (!device) {
		LogFileLine(StrCat("libslang-vk device creation failed: ", device.error().message));
		return false;
	}
	gDevice.emplace(std::move(*device));

	// Repopulate volk's global table for this TU's WSI calls. There are TWO
	// volk copies when GPU_SCALER_SHARED=ON: one inside libslangvk.dll
	// (initialized by Device::create) and the exe's own static copy linked
	// from slv-volk. The exe's copy starts completely NULL in the DLL build
	// (in the static build Device::create initialized the single shared
	// table, which is why this used to work without a volkInitialize here) —
	// and volkLoadInstance dereferences this copy's vkGetInstanceProcAddr
	// global through its stub, so calling it before volkInitialize() is a
	// guaranteed null-call crash (field crash 2026-10-18, nonstatic DLL
	// build, right after the "shader root" log line). volkInitialize is
	// idempotent: it merely refcounts LoadLibrary("vulkan-1.dll") and
	// re-fetches vkGetInstanceProcAddr when the table is already live.
	if (volkInitialize() != VK_SUCCESS) {
		LogFileLine("volkInitialize failed: the Vulkan loader (vulkan-1.dll) is not loadable");
		return false;
	}
	volkLoadInstance(gDevice->instance());
	volkLoadDevice(gDevice->device());

	const VkPhysicalDeviceProperties &props = gDevice->properties();
	Log("GPU scaler: libslang-vk Vulkan backend ready (device: {})", props.deviceName);
	LogFileLine(StrCat("device created: ", props.deviceName));
	return true;
}

// Creates the Vulkan surface for the current window and verifies that the
// library's graphics queue family can present to it.
bool CreateWindowSurface()
{
	if (!SDL_Vulkan_CreateSurface(gWindow, gDevice->instance(), nullptr, &gSurface)) {
		LogFileLine(StrCat("SDL_Vulkan_CreateSurface failed: ", SDL_GetError()));
		return false;
	}
	// The library picks a graphics queue family on its own; presentation from
	// that family is required for this path (on desktop drivers the graphics
	// family is present-capable, but verify instead of assuming).
	VkBool32 presentSupported = VK_FALSE;
	vkGetPhysicalDeviceSurfaceSupportKHR(gDevice->physicalDevice(), gDevice->graphicsFamily(), gSurface, &presentSupported);
	if (!presentSupported) {
		LogFileLine("the graphics queue family does not support presenting to this surface");
		SDL_Vulkan_DestroySurface(gDevice->instance(), gSurface, nullptr);
		gSurface = VK_NULL_HANDLE;
		return false;
	}
	return true;
}

// Computes the letterbox rect the final pass is drawn into, honoring the
// Integer Scaling option. A zero rect delegates the fit-and-center math to
// the library (reference-host math).
slv::Rect ComputePresentRect()
{
	slv::Rect rect {};
	if (!gChain || !gPreset || gPreset->passCount() == 0)
		return rect;
	auto size = gChain->passOutputSize(gPreset->passCount() - 1);
	if (!size)
		return rect;
	const uint32_t outWidth = size->first;
	const uint32_t outHeight = size->second;
	if (outWidth == 0 || outHeight == 0 || gSwapchainExtent.width == 0 || gSwapchainExtent.height == 0)
		return rect;

	float fitScale = std::min(static_cast<float>(gSwapchainExtent.width) / static_cast<float>(outWidth),
	    static_cast<float>(gSwapchainExtent.height) / static_cast<float>(outHeight));
	if (*GetOptions().Graphics.integerScaling) {
		const float floored = std::floor(fitScale);
		// Round 9: integer scaling is an UPSCALE constraint. Flooring a
		// DOWNSCALE ratio (< 1) collapses the scale to zero and silently
		// blacks out every frame; keep the fractional fit instead.
		if (floored >= 1.0F)
			fitScale = floored;
	}

	int destWidth = static_cast<int>(std::lround(static_cast<float>(outWidth) * fitScale));
	int destHeight = static_cast<int>(std::lround(static_cast<float>(outHeight) * fitScale));
	// Rounding can overshoot the swapchain by a pixel; clamp so the rect
	// never exceeds the target (same guard as the previous path).
	destWidth = std::min(destWidth, static_cast<int>(gSwapchainExtent.width));
	destHeight = std::min(destHeight, static_cast<int>(gSwapchainExtent.height));
	if (destWidth <= 0 || destHeight <= 0)
		return rect;

	rect.x = (static_cast<int32_t>(gSwapchainExtent.width) - destWidth) / 2;
	rect.y = (static_cast<int32_t>(gSwapchainExtent.height) - destHeight) / 2;
	rect.width = static_cast<uint32_t>(destWidth);
	rect.height = static_cast<uint32_t>(destHeight);
	return rect;
}

} // namespace

void LogPhase(std::string_view line)
{
	LogFileLine(line);
}

void InitShaderRootDir()
{
	static bool initialized = false;
	if (initialized)
		return;
	initialized = true;
	// Point the library's shader-root sandbox at the runtime shaders folder
	// next to the executable (the layout the runtime preset browser and the
	// docs promise). Idempotent; called from Reinit and from the settings
	// browser.
	const char *basePath = SDL_GetBasePath();
	if (basePath != nullptr) {
		const std::filesystem::path root = std::filesystem::path(basePath) / "shaders";
		slv::preset::SetShaderRootDir(root.generic_string());
	} else {
		// No base path: keep the library's default ("shaders", relative to
		// the working directory) and let preset resolution report failures.
		LogFileLine("SDL_GetBasePath failed; shader root stays relative ('shaders')");
	}
	LogFileLine(StrCat("shader root: ", slv::preset::ShaderRootDir()));
}

bool ScalerWanted()
{
	return *GetOptions().Graphics.shaderFilter != ShaderFilter::None;
}

bool IsActive()
{
	return gDevice.has_value() && gWindow != nullptr && gChain.has_value()
	    && gFilter != ShaderFilter::None;
}

bool HasFailed()
{
	return gFailed;
}

bool WindowHasSwapchainHistory(SDL_Window *window)
{
	return window != nullptr && gLastClaimedWindowId != 0 && SDL_GetWindowID(window) == gLastClaimedWindowId;
}

bool Reinit(SDL_Window *window)
{
	if (gFailed) {
		LogFileLine("Reinit skipped: the scaler failed earlier this session");
		gFilter = ShaderFilter::None;
		return false;
	}
	gFilter = *GetOptions().Graphics.shaderFilter;
	if (!IsCustomFilter(gFilter)) {
		// ShaderFilter::None, or a stale value left over from an older build
		// with built-in filters: nothing for the scaler to own.
		Shutdown();
		return false;
	}
	LogFileLine(StrCat("Reinit: filter=", static_cast<int>(gFilter), " (custom preset)"));
	InitShaderRootDir();

	gVsync = *GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync;

	if (!gDevice.has_value()) {
		if (!CreateVulkanDevice(window)) {
			LogFailHard("libslang-vk device creation failed");
			gFilter = ShaderFilter::None;
			return false;
		}
	}

	if (gWindow != window || gSurface == VK_NULL_HANDLE) {
		ReleaseWindowResources();
		gWindow = window;
		if (!CreateWindowSurface()) {
			LogFailHard("Vulkan surface creation failed");
			gWindow = nullptr;
			Shutdown();
			return false;
		}
		gLastClaimedWindowId = SDL_GetWindowID(gWindow);
		LogFileLine(StrCat("window claimed, vsync=", gVsync ? "1" : "0"));
	}

	bool minimized = false;
	std::string error;
	if (!EnsureSwapchainCurrent(minimized, error)) {
		LogFailHard(StrCat("swapchain creation failed: ", error));
		Shutdown();
		return false;
	}

	if (gSwapchain == VK_NULL_HANDLE) {
		// The window has no surface extent yet (minimized at init): the
		// swapchain and the chain are built lazily on the first present that
		// finds a valid extent.
		LogFileLine("swapchain deferred: the window has no surface extent yet");
		return true;
	}

	if (!EnsurePresetLoaded()) {
		FailPresetForSession(gPresetLastError);
		return false;
	}
	return true;
}

void ApplyFrameRateControl()
{
	if (!IsActive())
		return;
	const bool vsync = *GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync;
	if (vsync != gVsync) {
		gVsync = vsync;
		// The present mode is part of the swapchain: rebuild on the next
		// present. A failed rebuild resurfaces through the regular failure
		// contract; the toggle itself never fails the option change.
		gSwapchainDirty = true;
	}
	LogFileLine(StrCat("FrameRateControl changed: vsync=", gVsync ? "1" : "0"));
}

void Shutdown()
{
	LogFileLine("Shutdown");
	gChain.reset();
	gPreset.reset();
	ClearLoadedPreset();
	gPresetDisplayName.clear();
	ReleaseFrameResources();
	ReleaseWindowResources();
	gDevice.reset();
	gFilter = ShaderFilter::None;
	// gLastClaimedWindowId is intentionally NOT cleared: the (possibly still
	// alive) window keeps its DXGI swapchain history on Windows. Window IDs
	// are never reused, so the stale ID cannot match a future window.
}

void HandbackWindow(SDL_Window *window)
{
	LogFileLine("HandbackWindow: handing the window back to the regular renderer (device teardown deferred)");
	gChain.reset();
	gPreset.reset();
	ClearLoadedPreset();
	gPresetDisplayName.clear();
	ReleaseFrameResources();
	ReleaseWindowResources();
	(void)window;
	gFilter = ShaderFilter::None;
	// ROB-26: gDevice intentionally left ALIVE and parked, and
	// gLastClaimedWindowId intentionally kept (the hand-back flow still needs
	// WindowHasSwapchainHistory to recreate the DXGI-poisoned window). The
	// parked device holds no chain/frame/swapchain resources from here on.
	// Destroyed by the next Shutdown() (quit / dx_cleanup) or reused by the
	// next Reinit(); IsActive() is already false, so every game path falls
	// back to the regular renderer immediately.
}

void PresentFrame(const SDL_Surface &frame)
{
	if (!IsActive())
		return;

	// Heartbeat: lets a remote log show whether the present loop is alive
	// (UI responsive) or stalled (main thread stuck elsewhere), and counts
	// frames skipped due to an unavailable swapchain image plus frames
	// skipped by the zero-destination guard (round 9: that skip class was
	// SILENT - the field log looked healthy while the screen stayed black).
	static Uint32 sLastHeartbeat = 0;
	static unsigned long sHeartbeatFrames = 0;
	static unsigned long sSkippedFrames = 0;
	static unsigned long sDestSkipFrames = 0;
	static unsigned long sTotalFrames = 0;
	++sHeartbeatFrames;
	++sTotalFrames;
	{
		const Uint32 now = SDL_GetTicks();
		if (sLastHeartbeat == 0)
			sLastHeartbeat = now;
		if (now - sLastHeartbeat >= 5000) {
			LogFileLine(StrCat("heartbeat: ", sHeartbeatFrames, " frames (", sSkippedFrames,
			    " skipped, ", sDestSkipFrames, " dest-skipped, ", sTotalFrames,
			    " total, filter=", static_cast<int>(gFilter), ")"));
			sLastHeartbeat = now;
			sHeartbeatFrames = 0;
			sSkippedFrames = 0;
			sDestSkipFrames = 0;
		}
	}

	static bool loggedFirstPresentStart = false;
	if (!loggedFirstPresentStart) {
		loggedFirstPresentStart = true;
		LogFileLine("present: first frame, acquiring swapchain image");
	}

	// Frame duration for FrameTime, measured around the whole present.
	static Uint32 sLastFrameTick = 0;
	const Uint32 nowTick = SDL_GetTicks();
	Uint64 frameDeltaUs = 0;
	if (sLastFrameTick != 0)
		frameDeltaUs = static_cast<Uint64>(nowTick - sLastFrameTick) * 1000U;
	sLastFrameTick = nowTick;

	// Lazy completion path: Reinit can return with no swapchain when the
	// window had no surface extent yet (minimized at init). Build both the
	// swapchain and the chain on the first present that finds a valid one.
	if (!gChain.has_value()) {
		bool minimized = false;
		std::string error;
		if (!EnsureSwapchainCurrent(minimized, error)) {
			LogFailHard(StrCat("swapchain creation failed: ", error));
			return;
		}
		if (minimized || gSwapchain == VK_NULL_HANDLE) {
			++sSkippedFrames;
			return;
		}
		if (!EnsurePresetLoaded()) {
			FailPresetForSession(gPresetLastError);
			return;
		}
	}

	bool minimized = false;
	std::string error;
	if (!EnsureSwapchainCurrent(minimized, error)) {
		LogFailHard(StrCat("swapchain rebuild failed: ", error));
		return;
	}
	if (minimized) {
		++sSkippedFrames;
		return;
	}
	// A swapchain rebuild can change the surface format (rare; driver reset,
	// window dragged to an adapter with different capabilities). The chain's
	// pipelines target the format they were created with: reload the preset.
	if (gSwapchainFormat != gChainOutputFormat) {
		LogFileLine(StrCat("swapchain format changed; reloading preset chain"));
		gChain.reset();
		gPreset.reset();
		ClearLoadedPreset();
		if (!EnsurePresetLoaded()) {
			FailPresetForSession(gPresetLastError);
			return;
		}
	}

	const int gameWidth = frame.w;
	const int gameHeight = frame.h;

	static bool loggedFirstPresent = false;
	if (!loggedFirstPresent) {
		loggedFirstPresent = true;
		LogFileLine(StrCat("first present: game=", gameWidth, "x", gameHeight,
		    " swapchain=", gSwapchainExtent.width, "x", gSwapchainExtent.height));
	}

	if (!EnsureGameImage(gameWidth, gameHeight, error)) {
		// EnsureGameImage already produced the reason; hard-fail the session
		// per the failure contract.
		LogFailHard(error);
		return;
	}
	// Upload the frame through a tightly packed staging copy: the surface
	// pitch is not promised to equal width * 4. The library stages the copy
	// into the input image synchronously.
	{
		const size_t tightlyPacked = static_cast<size_t>(gameWidth) * static_cast<size_t>(gameHeight) * 4;
		gStaging.resize(tightlyPacked);
		const size_t bytesPerRow = static_cast<size_t>(gameWidth) * 4;
		auto *dst = gStaging.data();
		const auto *src = static_cast<const uint8_t *>(frame.pixels);
		if (frame.pixels == nullptr) {
			LogFailHard("frame pixels unavailable");
			return;
		}
		if (static_cast<size_t>(frame.pitch) == bytesPerRow) {
			std::memcpy(dst, src, tightlyPacked);
		} else {
			for (int y = 0; y < gameHeight; ++y)
				std::memcpy(dst + static_cast<size_t>(y) * bytesPerRow,
				    src + static_cast<size_t>(y) * static_cast<size_t>(frame.pitch), bytesPerRow);
		}
		if (!gGameImage->update(gStaging.data(), gStaging.size())) {
			LogFailHard("frame upload failed");
			return;
		}
	}

	// Keep the chain's output target sized to the current swapchain extent
	// (libslang-vk M4.05 API: resize() stores the output size only; pass FBOs,
	// the history ring and the mip copy are re-derived by render() from the
	// per-frame InputFrame size, so an input-size-only change is a no-op here).
	if (gChainInputWidth != static_cast<uint32_t>(gameWidth)
	    || gChainInputHeight != static_cast<uint32_t>(gameHeight)
	    || gChainOutputWidth != gSwapchainExtent.width
	    || gChainOutputHeight != gSwapchainExtent.height) {
		auto resized = gChain->resize(gSwapchainExtent.width, gSwapchainExtent.height);
		if (!resized) {
			LogFailHard(StrCat("chain resize failed: ", resized.error().message));
			return;
		}
		gChainInputWidth = static_cast<uint32_t>(gameWidth);
		gChainInputHeight = static_cast<uint32_t>(gameHeight);
		gChainOutputWidth = gSwapchainExtent.width;
		gChainOutputHeight = gSwapchainExtent.height;
	}

	// Acquire (timed, non-blocking): a timed-out or unavailable acquire skips
	// the frame — the same contract the previous path had for unavailable
	// swapchain images (minimized window, resize, driver quirk).
	uint32_t imageIndex = 0;
	const VkResult acquired = vkAcquireNextImageKHR(gDevice->device(), gSwapchain, kAcquireTimeoutNs,
	    gAcquireSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
		gSwapchainDirty = true;
		++sSkippedFrames;
		return;
	}
	if (acquired == VK_TIMEOUT || acquired == VK_NOT_READY) {
		// No image became available in time (minimized window); skip.
		++sSkippedFrames;
		return;
	}
	if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
		LogFailHard(StrCat("vkAcquireNextImageKHR failed (", static_cast<int>(acquired), ")"));
		return;
	}
	if (acquired == VK_SUBOPTIMAL_KHR)
		gSwapchainDirty = true; // rebuild after this frame

	slv::SwapchainFrame target;
	target.swapchainImageView = gSwapchainViews[imageIndex];
	target.format = gSwapchainFormat;
	target.width = gSwapchainExtent.width;
	target.height = gSwapchainExtent.height;
	target.outputRect = ComputePresentRect();
	// Cache the rect the frame is presented into: MapWindowToGame /
	// MapGameToWindow derive the window <-> game transform from it.
	gLastPresentRect = target.outputRect;
	target.acquireSemaphore = gAcquireSemaphore;

	slv::InputFrame input;
	input.view = gGameImage->view();
	input.image = gGameImage->image();
	input.format = GameTextureVkFormat();
	input.width = static_cast<uint32_t>(gameWidth);
	input.height = static_cast<uint32_t>(gameHeight);
	input.rotation = slv::Rotation::None; // orientation is a shader-side concern

	slv::FrameInfo frameInfo;
	frameInfo.frameCount = gFrameCount++;
	frameInfo.frameDeltaUs = static_cast<float>(frameDeltaUs);
	frameInfo.frameDirection = 1;

	auto rendered = gChain->render(input, target, frameInfo);
	if (!rendered) {
		LogFailHard(StrCat("preset run failed: ", rendered.error().message));
		// The acquire semaphore protocol may be out of step after a failed
		// render (signaled without a wait); rebuild it so the next acquire is
		// well-formed.
		RecreateAcquireSemaphore();
		return;
	}

	VkPresentInfoKHR present {};
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.swapchainCount = 1;
	present.pSwapchains = &gSwapchain;
	present.pImageIndices = &imageIndex;
	// Chain::render is synchronous (fenced), so the present needs no wait
	// semaphore (the documented SwapchainFrame contract of the library).
	const VkResult presented = vkQueuePresentKHR(gDevice->graphicsQueue(), &present);
	if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
		gSwapchainDirty = true;
	} else if (presented != VK_SUCCESS) {
		LogFailHard(StrCat("vkQueuePresentKHR failed (", static_cast<int>(presented), ")"));
		return;
	}
}

bool MapWindowToGame(float *x, float *y)
{
	if (x == nullptr || y == nullptr || !IsActive() || gWindow == nullptr)
		return false;
	if (gGameWidth <= 0 || gGameHeight <= 0 || gLastPresentRect.width == 0
	    || gLastPresentRect.height == 0 || gSwapchainExtent.width == 0
	    || gSwapchainExtent.height == 0)
		return false;
	int windowWidth = 0;
	int windowHeight = 0;
	SDL_GetWindowSize(gWindow, &windowWidth, &windowHeight);
	if (windowWidth <= 0 || windowHeight <= 0)
		return false;

	// SDL delivers mouse events in window points; the swapchain and the
	// present rect live in window pixels.
	const float pixelX = *x * static_cast<float>(gSwapchainExtent.width) / static_cast<float>(windowWidth);
	const float pixelY = *y * static_cast<float>(gSwapchainExtent.height) / static_cast<float>(windowHeight);
	*x = (pixelX - static_cast<float>(gLastPresentRect.x)) * static_cast<float>(gGameWidth) / static_cast<float>(gLastPresentRect.width);
	*y = (pixelY - static_cast<float>(gLastPresentRect.y)) * static_cast<float>(gGameHeight) / static_cast<float>(gLastPresentRect.height);
	return true;
}

bool MapGameToWindow(float *x, float *y)
{
	if (x == nullptr || y == nullptr || !IsActive() || gWindow == nullptr)
		return false;
	if (gGameWidth <= 0 || gGameHeight <= 0 || gLastPresentRect.width == 0
	    || gLastPresentRect.height == 0 || gSwapchainExtent.width == 0
	    || gSwapchainExtent.height == 0)
		return false;
	int windowWidth = 0;
	int windowHeight = 0;
	SDL_GetWindowSize(gWindow, &windowWidth, &windowHeight);
	if (windowWidth <= 0 || windowHeight <= 0)
		return false;

	const float densityX = static_cast<float>(gSwapchainExtent.width) / static_cast<float>(windowWidth);
	const float densityY = static_cast<float>(gSwapchainExtent.height) / static_cast<float>(windowHeight);
	*x = (static_cast<float>(gLastPresentRect.x) + *x * static_cast<float>(gLastPresentRect.width) / static_cast<float>(gGameWidth)) / densityX;
	*y = (static_cast<float>(gLastPresentRect.y) + *y * static_cast<float>(gLastPresentRect.height) / static_cast<float>(gGameHeight)) / densityY;
	return true;
}

namespace {

std::string gOsdText;

// FEAT-03: the OSD carries the first line of the last preset load failure
// (the full text stays in gpu-scaler.log), capped so the bottom-right label
// cannot walk off the screen on long paths.
std::string OsdErrorLine(const std::string &error)
{
	std::string line = error.substr(0, error.find('\n'));
	constexpr size_t kMaxOsdErrorChars = 64;
	if (line.size() > kMaxOsdErrorChars) {
		line.resize(kMaxOsdErrorChars - 3);
		line += "...";
	}
	return line;
}

const char *FilterDisplayName(ShaderFilter filter)
{
	if (IsCustomFilter(filter))
		return gPresetDisplayName.empty() ? "Custom" : gPresetDisplayName.c_str();
	return "Off";
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
		// FEAT-03: name the failure instead of a bare "(unavailable)" - the
		// first line of the last preset load error; the full text stays in
		// gpu-scaler.log.
		if (IsCustomFilter(static_cast<ShaderFilter>(value)) && !gPresetLastError.empty())
			text = StrCat("Shader: ", FilterDisplayName(static_cast<ShaderFilter>(value)), " (failed: ",
			    OsdErrorLine(gPresetLastError), ")");
		else
			text = StrCat("Shader: ", FilterDisplayName(static_cast<ShaderFilter>(value)), " (unavailable)");
	} else {
		text = StrCat("Shader: ", FilterDisplayName(static_cast<ShaderFilter>(value)), " (Vulkan)");
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
