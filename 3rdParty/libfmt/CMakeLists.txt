include(functions/FetchContent_ExcludeFromAll_backport)

if(NOT WIN32 AND NOT APPLE AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
  # Enable POSIX extensions such as `readlink` and `ftruncate`.
  add_definitions(-D_POSIX_C_SOURCE=200809L)
endif()

# Disable fmt/os.h functionality.
# We do not use it and it is not supported on some systems.
set(FMT_OS OFF)

if(DEVILUTIONX_STATIC_LIBFMT)
  set(BUILD_SHARED_LIBS OFF)
else()
  set(BUILD_SHARED_LIBS ON)
endif()
include(FetchContent)
if(DREAMCAST)
  set(DREAMCAST_FMT_PATCH "${CMAKE_CURRENT_LIST_DIR}/fmt-12.0.0-sh4-long-double.patch")
  if(NOT EXISTS "${DREAMCAST_FMT_PATCH}")
    message(FATAL_ERROR "Dreamcast fmt patch not found: ${DREAMCAST_FMT_PATCH}")
  endif()
  find_program(DREAMCAST_PATCH_EXECUTABLE patch REQUIRED)
  FetchContent_Declare_ExcludeFromAll(libfmt
    URL https://github.com/fmtlib/fmt/releases/download/12.0.0/fmt-12.0.0.zip
    URL_HASH SHA256=1c32293203449792bf8e94c7f6699c643887e826f2d66a80869b4f279fb07d25
    PATCH_COMMAND "${DREAMCAST_PATCH_EXECUTABLE}" -p1 -N -i "${DREAMCAST_FMT_PATCH}"
  )
else()
  FetchContent_Declare_ExcludeFromAll(libfmt
    URL https://github.com/fmtlib/fmt/releases/download/12.0.0/fmt-12.0.0.zip
    URL_HASH SHA256=1c32293203449792bf8e94c7f6699c643887e826f2d66a80869b4f279fb07d25
  )
endif()
FetchContent_MakeAvailable_ExcludeFromAll(libfmt)

# We do not use locale-specific features of libfmt and disabling them reduces the size.
target_compile_definitions(fmt PUBLIC FMT_USE_LOCALE=0)

if(DEVILUTIONX_WINDOWS_NO_WCHAR)
  target_compile_definitions(fmt PUBLIC FMT_USE_WRITE_CONSOLE)
endif()

# Reduces the overall binary size by 8 KiB.
if(TARGET_PLATFORM STREQUAL "rg99")
  target_compile_definitions(fmt PUBLIC FMT_BUILTIN_TYPES=0)
endif()

# https://github.com/fmtlib/fmt/issues/4189
if(NINTENDO_3DS OR NINTENDO_SWITCH OR VITA)
  target_compile_definitions(fmt PUBLIC FMT_USE_FALLBACK_FILE=1)
endif()
