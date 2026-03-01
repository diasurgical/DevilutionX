#pragma once

#if defined(__APPLE__) && DARWIN_MAJOR_VERSION >= 9
#include <Availability.h>
#if (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500) \
    || (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 130000)
#define DVL_NO_FILESYSTEM
#endif
#elif defined(NXDK) || defined(__DREAMCAST__) || (defined(_MSVC_LANG) && _MSVC_LANG < 201703L) \
    || (defined(WINVER) && WINVER <= 0x0500 && (!defined(_WIN32_WINNT) || _WIN32_WINNT == 0))
#define DVL_NO_FILESYSTEM
#endif

#ifndef DVL_NO_FILESYSTEM
#if defined(__has_include)
#if __has_include(<filesystem>)
#define DVL_HAS_FILESYSTEM
#include <filesystem> // IWYU pragma: export
#elif __has_include(<experimental/filesystem>)
#define DVL_HAS_FILESYSTEM
#include <experimental/filesystem> // IWYU pragma: export
#define filesystem experimental::filesystem
#endif
#endif
#endif
