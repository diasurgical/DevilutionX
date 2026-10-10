#pragma once
/**
 * @file pch.hpp
 *
 * @brief Precompiled header shared by every devilutionX object library.
 *
 * Header parsing dominates our build: roughly 78% of the time spent compiling
 * `Source/` goes into headers rather than into our own code. Precompiling the
 * external headers that nearly every translation unit pulls in cuts the
 * affected targets roughly in half.
 *
 * It is built once by `libdevilutionx_pch` and reused everywhere else via
 * `target_precompile_headers(... REUSE_FROM ...)`. That target is deliberately
 * kept free of the optional third-party defines (asio/sol/mpqfs) that only some
 * libraries get: GCC accepts a PCH whose macro state is a *subset* of the
 * consumer's, but rejects one built with a macro the consumer lacks. Reusing
 * libdevilutionx's own PCH instead fails for exactly that reason.
 *
 * This file deliberately contains **no devilutionX headers**. Anything listed
 * here is baked into the PCH, so touching it forces all ~250 translation units
 * to rebuild. Restricting the contents to third-party headers means the PCH is
 * invalidated only when a dependency or the toolchain changes, never by
 * ordinary work on the game. Measured against including our own hot headers as
 * well, this gives up only a few percent of the total win.
 *
 * Do not include this file directly: it is injected by
 * `target_precompile_headers`, and every source file must keep listing the
 * headers it actually uses so the build still works with `DEVILUTIONX_PCH=OFF`.
 *
 * The third-party includes below are guarded with `__has_include`. The target
 * that builds the PCH (`libdevilutionx_pch`) always has all of them available,
 * so the precompiled image is complete. The guards only matter on the fallback
 * path: if a compiler ever rejects the PCH, it re-parses this header in a
 * target that may not have those include directories, and we want that to
 * degrade to a slower build rather than a broken one. Every translation unit
 * still includes what it uses, so nothing here changes what compiles.
 */

// NOLINTBEGIN(misc-include-cleaner)
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#ifdef USE_SDL3
#if __has_include(<SDL3/SDL.h>)
#include <SDL3/SDL.h>
#endif
#elif __has_include(<SDL.h>)
#include <SDL.h>
#endif

#if __has_include(<ankerl/unordered_dense.h>)
#include <ankerl/unordered_dense.h>
#endif
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#endif
// NOLINTEND(misc-include-cleaner)
