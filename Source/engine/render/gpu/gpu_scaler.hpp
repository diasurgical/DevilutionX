// Written by VTSTech (Nigel Todman) (veritas@vts-tech.org)
// AI Assisted by Super-Z (GLM-5.3-Flash)
// License: MIT

#pragma once

#include "options.h"

#include "engine/point.hpp"

#ifndef USE_SDL1

#include <cstdint>
#include <string_view>

struct SDL_Surface;
struct SDL_Window;

namespace devilution {

class Surface;

/**
 * @brief Vulkan shader scaler that executes libretro slang presets through
 * the libslang-vk library.
 *
 * Replaces the SDL_Renderer presentation path when a shader filter is
 * selected in the graphics options. The game still renders into its 8-bit
 * paletted backbuffer as usual; the converted RGBA frame is uploaded to a
 * Vulkan image every frame and pushed through the preset's shader chain
 * (parsing, .slang -> GLSL translation, glslang SPIR-V compilation, pass
 * execution with history/feedback/LUT semantics) before the final pass is
 * drawn into the swapchain image with letterboxing. All slang and Vulkan
 * machinery lives in libslang-vk; this module is the host adapter: window
 * claim, frame upload, presentation, OSD, diagnostics and the session
 * fallback contract.
 */
namespace gpu_scaler {

/**
 * @brief Returns true when a shader filter is selected in the options,
 * regardless of whether the scaler initialized successfully. Callers use
 * this to keep the backbuffer setup (upscale-style) consistent.
 */
bool ScalerWanted();

/**
 * @brief Points the libslang-vk shader-root sandbox at the runtime shaders
 * folder next to the executable. Idempotent; called by Reinit and by the
 * settings preset browser before any scan, resolve or load.
 */
void InitShaderRootDir();

/**
 * @brief Returns true when the GPU scaler owns presentation for the main
 * window (device created, window claimed and filter initialized).
 */
bool IsActive();

/**
 * @brief Returns true when the GPU scaler hit an unrecoverable error this
 * session. The caller should tear it down and revert to the regular
 * renderer; Reinit() will keep refusing until the process restarts.
 */
bool HasFailed();

/**
 * @brief Returns true when the GPU device successfully claimed the given
 * window at some point this session (even if the scaler has since been shut
 * down). On Windows the underlying HWND keeps its DXGI flip-model swapchain
 * history, which prevents other presentation APIs from creating their own
 * swapchain on it; recreate the window before handing it back to the
 * regular SDL_Renderer (see RecreateMainRenderWindow in display.cpp).
 */
bool WindowHasSwapchainHistory(SDL_Window *window);

/**
 * @brief (Re)initializes the GPU scaler for the given window.
 * @return true on success; on failure the caller must fall back to the
 * regular SDL_Renderer path.
 */
bool Reinit(SDL_Window *window);

/** @brief Releases all GPU scaler resources. Safe to call anytime. */
void Shutdown();

/**
 * @brief Hands the window back to the regular renderer WITHOUT destroying the
 * GPU device: releases the swapchain claim and every filter/preset resource,
 * marks the scaler inactive (IsActive() goes false immediately), and PARKS
 * the device for deferred destruction.
 *
 * Used by the presentation hand-back path (Shaders -> Off and fallbacks).
 * Destroying the device synchronously inside that switch — Vulkan teardown
 * immediately followed by window recreation and a D3D11 reclaim on Windows —
 * is the field-crashing call sequence (ROB-26): the GPU backend priority is
 * D3D12 > Vulkan on Windows, so this stack only ever started running once the
 * forced-Vulkan retry (ROB-18) made the scaler work there, which is why the
 * unload path appeared "newly broken" when it had in fact never worked.
 *
 * The parked device is destroyed by the next Shutdown() (quit / dx_cleanup),
 * or transparently reused by the next Reinit() (re-selecting a shader just
 * re-claims a fresh window on it and reloads filter resources).
 */
void HandbackWindow(SDL_Window *window);

/**
 * @brief Applies the current FrameRateControl setting (Vertical Sync vs
 * None) to the live GPU swapchain. ROB-08: the FrameRateControl option had
 * no change callback, so toggling vsync while the scaler was active
 * changed nothing until restart or filter change, while the SDL_Renderer
 * path re-applied it on every ReinitializeRenderer. Registered as the
 * option's change callback; a no-op unless the scaler is active (owns the
 * present). A failed SDL_SetGPUSwapchainParameters keeps the old present
 * mode and is logged, never fatal.
 */
void ApplyFrameRateControl();

/**
 * @brief Presents the given RGBA frame (game resolution) with the active
 * shader filter. Blocks for vsync when configured.
 */
void PresentFrame(const SDL_Surface &frame);

/**
 * @brief Maps window coordinates (as delivered by SDL mouse events: window
 * points) to game backbuffer coordinates while the GPU scaler owns the
 * window.
 *
 * The scaler presents through its own Vulkan swapchain and deliberately has
 * no SDL_Renderer, so SDL's logical-presentation event conversion is
 * unavailable and raw window coordinates would reach the game as if they
 * were game coordinates - highlighting and picking up at the wrong spot
 * whenever the presented frame is scaled or letterboxed (resolution below
 * the window pixel size, shader chain upscaling, OS display scaling, where
 * window points are a fraction of window pixels).
 *
 * Uses the last presented frame's size and present rect; returns false and
 * leaves the inputs unchanged when the scaler is inactive or no frame has
 * been presented yet.
 */
bool MapWindowToGame(float *x, float *y);

/**
 * @brief Inverse of MapWindowToGame: maps game backbuffer coordinates to
 * window coordinates (points, the space SDL_WarpMouseInWindow expects).
 */
bool MapGameToWindow(float *x, float *y);

/**
 * @brief Appends a timestamped line to the GPU scaler diagnostic log
 * (gpu-scaler.log next to the executable). Used by load/present phase
 * instrumentation to diagnose remote hangs and crashes.
 */
void LogPhase(std::string_view line);

/**
 * @brief Refreshes the persistent filter OSD label confirming which shader
 * filter the presentation path is actually running (e.g. "Shader:
 * HQ2x (Vulkan)"). Call after the renderer (re)initialization has settled.
 * The label is visible in the bottom-right corner of the game frame only
 * while a filter is selected; with the filter Off no label is shown. This
 * call only updates its text,
 * which changes when the selected filter, its availability, or the active
 * GPU backend changes. Every text change is also logged to gpu-scaler.log.
 */
void ShowFilterOsd();

/**
 * @brief The current OSD label, or an empty view before the first
 * initialization. Consumed by DrawFilterOsd.
 */
std::string_view FilterOsdText();

/**
 * @brief Draws the persistent filter OSD label (if any) into the 8-bit
 * game frame, right-aligned in the bottom-right corner, in dialog-size
 * white, so it flows through the scaler (or the regular renderer) like
 * the rest of the image. A no-op before the first ShowFilterOsd call.
 */
void DrawFilterOsd(const Surface &out);

} // namespace gpu_scaler

} // namespace devilution

#endif // !USE_SDL1
