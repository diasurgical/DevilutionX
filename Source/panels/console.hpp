#ifdef _DEBUG
#pragma once

#include <string_view>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#include "engine/rectangle.hpp"

namespace devilution {

void InitConsole();
bool IsConsoleOpen();
void OpenConsole();
bool ConsoleHandleEvent(const SDL_Event &event);
void DrawConsole();
Rectangle GetConsoleRect();
void RunInConsole(std::string_view code);
void PrintToConsole(std::string_view text);
void PrintWarningToConsole(std::string_view text);

} // namespace devilution
#endif // _DEBUG
