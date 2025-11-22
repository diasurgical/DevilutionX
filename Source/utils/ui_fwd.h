#pragma once

#include <cstdint>

#include "engine/rectangle.hpp"
#include "utils/attributes.h"

namespace devilution {

extern DVL_API_FOR_TEST uint16_t gnScreenWidth;
extern DVL_API_FOR_TEST uint16_t gnScreenHeight;
extern DVL_API_FOR_TEST uint16_t gnViewportHeight;

uint16_t GetScreenWidth();
uint16_t GetScreenHeight();
uint16_t GetViewportHeight();

/** @brief Returns the UI (Menus, Messages, Help) can use. Currently this is 640x480 like vanilla. */
const Rectangle &GetUIRectangle();

void AdjustToScreenGeometry(Size windowSize);
float GetDpiScalingFactor();
/**
 * @brief Set the screen to fullscreen or windowe if fullsc
 */
void SetFullscreenMode();
bool SpawnWindow(const char *lpWindowName);
#ifndef USE_SDL1
void ReinitializeTexture();
void ReinitializeIntegerScale();
#endif
void ReinitializeRenderer();
void ResizeWindow();
void UiErrorOkDialog(std::string_view caption, std::string_view text, bool error = true);

} // namespace devilution
