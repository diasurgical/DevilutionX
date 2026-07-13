// This is copyrighted software. More information is at the end of this file.
#pragma once
#include "aulib_config.h"
#include <cstdio>
#include <format>
#include <string>
#include <utility>

namespace aulib {
namespace log {

template <typename... Args>
void debug([[maybe_unused]] std::format_string<Args...> fmt_str,
           [[maybe_unused]] Args &&...args) {
#if AULIB_DEBUG
  const std::string message = std::format(fmt_str, std::forward<Args>(args)...);
  std::fprintf(stderr, "SDL_audiolib debug: %s", message.c_str());
#endif
}

template <typename... Args>
void debugLn([[maybe_unused]] std::format_string<Args...> fmt_str,
             [[maybe_unused]] Args &&...args) {
#if AULIB_DEBUG
  const std::string message = std::format(fmt_str, std::forward<Args>(args)...);
  std::fprintf(stderr, "SDL_audiolib debug: %s\n", message.c_str());
#endif
}

template <typename... Args>
void warn(std::format_string<Args...> fmt_str, Args &&...args) {
  const std::string message = std::format(fmt_str, std::forward<Args>(args)...);
  std::fprintf(stderr, "SDL_audiolib warning: %s", message.c_str());
}

template <typename... Args>
void warnLn(std::format_string<Args...> fmt_str, Args &&...args) {
  const std::string message = std::format(fmt_str, std::forward<Args>(args)...);
  std::fprintf(stderr, "SDL_audiolib warning: %s\n", message.c_str());
}

template <typename... Args>
void info(std::format_string<Args...> fmt_str, Args &&...args) {
  const std::string message = std::format(fmt_str, std::forward<Args>(args)...);
  std::printf("SDL_audiolib info: %s", message.c_str());
}

template <typename... Args>
void infoLn(std::format_string<Args...> fmt_str, Args &&...args) {
  const std::string message = std::format(fmt_str, std::forward<Args>(args)...);
  std::printf("SDL_audiolib info: %s\n", message.c_str());
}

} // namespace log
} // namespace aulib

/*

Copyright (C) 2022 Nikos Chantziaras.

This file is part of SDL_audiolib.

SDL_audiolib is free software: you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

SDL_audiolib is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License
along with SDL_audiolib. If not, see <http://www.gnu.org/licenses/>.

*/
