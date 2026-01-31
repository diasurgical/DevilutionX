cmake_minimum_required(VERSION 3.22)

foreach(_policy CMP0135 CMP0141)
  if(POLICY ${_policy})
    cmake_policy(SET ${_policy} NEW)
    set(CMAKE_POLICY_DEFAULT_${_policy} NEW)
  endif()
endforeach()

# Projects added via `add_subdirectory` or `FetchContent` may have a lower
# `cmake_minimum_required` than we set here. Set policies that we require
# to their new value so that they still apply.
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist")
  message("-- Detected a source distribution with the required FetchContent dependencies and devilutionx.mpq included")
  set(SRC_DIST ON)
  add_subdirectory(dist)
endif()

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(WARNING [[In-source build detected, please eg. create a new directory and use `cmake ..`]])
endif()

include(CMakeDependentOption)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/finders")
include(functions/genex)

# Options required by `VcPkgManifestFeatures`, which must be included before the `project` call.
option(USE_SDL1 "Use SDL1.2 instead of SDL2" OFF)
option(USE_SDL3 "Use SDL3 instead of SDL2" OFF)
option(NONET "Disable network support" OFF)
cmake_dependent_option(PACKET_ENCRYPTION "Encrypt network packets" ON "NOT NONET" OFF)
# The gettext[tools] package takes a very long time to install
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake$")
  option(USE_GETTEXT_FROM_VCPKG "Add vcpkg dependency for gettext[tools] for compiling translations" OFF)
endif()
option(BUILD_TESTING "Build tests." ON)

# These must be included after the options above but before the `project` call.
include(VcPkgManifestFeatures)

# Set up the `project` before the rest of the options so that:
#
# 1. Properties such as `TARGET_SUPPORTS_SHARED_LIBS` are defined.
# 2. Toolchain file is evaluated, required for `Platforms.cmake`,
#    which can override the options.
file(STRINGS "VERSION" VERSION_STR)
if(NOT "${VERSION_STR}" STREQUAL "")
  string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+).*" VERSION_PREFIX ${VERSION_STR})
  set(VERSION_NUM ${CMAKE_MATCH_1})
endif()

if(NOT VERSION_SUFFIX)
  # For versions with a suffix, e.g. "1.5.0-dev", include
  # the build type and the git hash.
  set(VERSION_SUFFIX "-$<CONFIG>")
  if(VERSION_PREFIX MATCHES "-")
    if(NOT GIT_COMMIT_HASH)
      include(functions/git)
      get_git_commit_hash(GIT_COMMIT_HASH)
    endif()
    if(GIT_COMMIT_HASH)
      set(VERSION_SUFFIX "${VERSION_SUFFIX}-${GIT_COMMIT_HASH}")
    endif()
  else()
    # For versions without a suffix, e.g. "1.5.0" rather than "1.5.0-dev",
    # only include the build type if it is a debug build.
    set(VERSION_SUFFIX "$<$<CONFIG:Debug>:$<CONFIG>>")
  endif()
endif()

project(DevilutionX
  VERSION ${VERSION_NUM}
  LANGUAGES C CXX)
set(PROJECT_VERSION_WITH_SUFFIX "${VERSION_PREFIX}${VERSION_SUFFIX}")

# Platform definitions can override options and we want `cmake_dependent_option` to see the effects.
# Note that a few options are still defined before this because they're needed by `VcPkgManifestFeatures.cmake`.
include(Platforms)

# This built-in CMake module adds a BUILD_TESTING option (ON by default).
# Must be included in the top-level `CMakeLists.txt` after calling `project`.
# Because we must include `VcPkgManifestFeatures` before the `project` call,
# we add a BUILD_TESTING option ourselves above as well.
include(CTest)

# Debugging / profiling options
DEBUG_OPTION(ASAN "Enable address sanitizer")
DEBUG_OPTION(UBSAN "Enable undefined behaviour sanitizer")
option(TSAN "Enable thread sanitizer (not compatible with ASAN=ON)" OFF)
DEBUG_OPTION(DEBUG "Enable debug mode in engine")
option(GPERF "Build with GPerfTools profiler" OFF)
cmake_dependent_option(GPERF_HEAP_FIRST_GAME_ITERATION "Save heap profile of the first game iteration" OFF "GPERF" OFF)
option(ENABLE_CODECOVERAGE "Instrument code for code coverage (only enabled with BUILD_TESTING)" OFF)

# Packaging options
RELEASE_OPTION(CPACK "Configure CPack")
option(MACOSX_STANDALONE_APP_BUNDLE "Generate a portable app bundle to use on other devices (requires sudo)" OFF)
option(WIN_NSIS "Generate an NSIS installer" OFF)

# Network options
cmake_dependent_option(DISABLE_TCP "Disable TCP multiplayer option" OFF "NOT NONET" ON)
cmake_dependent_option(DISABLE_ZERO_TIER "Disable ZeroTier multiplayer option" OFF "NOT NONET" ON)

if(USE_SDL1 AND USE_SDL3)
  message(FATAL_ERROR "USE_SDL1 and USE_SDL3 cannot be set at the same time")
endif()

# Graphics options
if(NOT USE_SDL1)
if(USE_SDL3)
  set(_texture_format_default "SDL_PIXELFORMAT_XRGB8888")
else()
  set(_texture_format_default "SDL_PIXELFORMAT_RGB888")
endif()
  set(DEVILUTIONX_DISPLAY_TEXTURE_FORMAT "${_texture_format_default}" CACHE STRING "Texture format for DevilutionX textures when using the GPU renderer")
  mark_as_advanced(DEVILUTIONX_DISPLAY_TEXTURE_FORMAT)
endif()

if(USE_SDL1)
  # SDL_image in SDL1 does not support PNG, making PCX the only option.
  set(DEVILUTIONX_SCREENSHOT_FORMAT "DEVILUTIONX_SCREENSHOT_FORMAT_PCX")
else()
  set(DEVILUTIONX_SCREENSHOT_FORMAT "DEVILUTIONX_SCREENSHOT_FORMAT_PNG" CACHE STRING "Screenshot format")
  set_property(CACHE DEVILUTIONX_SCREENSHOT_FORMAT PROPERTY STRINGS "DEVILUTIONX_SCREENSHOT_FORMAT_PNG;DEVILUTIONX_SCREENSHOT_FORMAT_PCX")
  mark_as_advanced(DEVILUTIONX_SCREENSHOT_FORMAT)
endif()

# Sound options
option(NOSOUND "Disable sound support" OFF)
option(DEVILUTIONX_RESAMPLER_SPEEX "Build with Speex resampler" ON)
cmake_dependent_option(DEVILUTIONX_RESAMPLER_SDL "Build with SDL resampler" ON "NOT USE_SDL1" OFF)
if(DEVILUTIONX_RESAMPLER_SPEEX)
  list(APPEND _resamplers Speex)
endif()
if(DEVILUTIONX_RESAMPLER_SDL)
  list(APPEND _resamplers SDL)
endif()
list(GET _resamplers 0 _default_resampler)
set(DEVILUTIONX_DEFAULT_RESAMPLER ${_default_resampler} CACHE STRING "Default resampler")
set_property(CACHE DEVILUTIONX_DEFAULT_RESAMPLER PROPERTY STRINGS ${_resamplers})

# Optimization / link options
option(DISABLE_LTO "Disable link-time optimization (by default enabled in release mode)" OFF)
option(PIE "Generate position-independent code" OFF)
cmake_dependent_option(DEVILUTIONX_DISABLE_RTTI "Disable RTTI" ON "NONET" OFF)
cmake_dependent_option(DEVILUTIONX_DISABLE_EXCEPTIONS "Disable exceptions" ON "DISABLE_ZERO_TIER" OFF)
RELEASE_OPTION(DEVILUTIONX_STATIC_CXX_STDLIB "Link C++ standard library statically (if available)")
option(DEVILUTIONX_PROFILE_GENERATE "Build a binary that generates the profile for PGO" OFF)
option(DEVILUTIONX_PROFILE_USE "Build with PGO using the given profile file" OFF)
set(DEVILUTIONX_PROFILE_DIR "" CACHE STRING "Directory where the profile is stored")

include(MoldLinker)

# Memory / performance trade-off options
option(UNPACKED_MPQS "Expect MPQs to be unpacked and the data converted with devilutionx-mpq-tools" OFF)
option(UNPACKED_SAVES "Uses unpacked save files instead of MPQ .sv/.hsv files" OFF)
option(DISABLE_STREAMING_MUSIC "Disable streaming music (to work around broken platform implementations)" OFF)
mark_as_advanced(DISABLE_STREAMING_MUSIC)
option(DISABLE_STREAMING_SOUNDS "Disable streaming sounds (to work around broken platform implementations)" OFF)
mark_as_advanced(DISABLE_STREAMING_SOUNDS)
set(STREAM_ALL_AUDIO_MIN_FILE_SIZE "" CACHE STRING "If set, stream all the audio files larger than this size")
mark_as_advanced(STREAM_ALL_AUDIO_MIN_FILE_SIZE)
option(DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT "Whether to use a lookup table for transparency blending with black. This improves performance of blending transparent black overlays, such as quest dialog background, at the cost of 128 KiB of RAM." ON)
mark_as_advanced(DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT)

# Additional features
option(DISABLE_DEMOMODE "Disable demo mode support" OFF)
option(DISCORD_INTEGRATION "Build with Discord SDK for rich presence support" OFF)
option(SCREEN_READER_INTEGRATION "Build with screen reader support" OFF)
mark_as_advanced(SCREEN_READER_INTEGRATION)

# If both UNPACKED_MPQS and UNPACKED_SAVES are enabled, we completely remove MPQ support.
if(UNPACKED_MPQS AND UNPACKED_SAVES)
  set(SUPPORTS_MPQ OFF)
else()
  set(SUPPORTS_MPQ ON)
endif()

# By default, devilutionx.mpq and mods/Hellfire.mpq are built only if smpq is installed and MPQ support is enabled.
if(SUPPORTS_MPQ AND NOT UNPACKED_MPQS)
  if(BUILD_ASSETS_MPQ OR (CPACK STREQUAL "ON" AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")))
    find_program(SMPQ smpq REQUIRED)
  elseif(NOT DEFINED BUILD_ASSETS_MPQ AND NOT SRC_DIST)
    find_program(SMPQ smpq)
  endif()
  if(SMPQ)
    set(_has_smpq ON)
  else()
    set(_has_smpq OFF)
  endif()
  option(BUILD_ASSETS_MPQ "If true, assets are packaged into devilutionx.mpq and mods/Hellfire to Hellfire.mpq." ${_has_smpq})
else()
  set(BUILD_ASSETS_MPQS OFF)
endif()

# === Option overrides ===
# TSAN is not compatible with ASAN.
if(TSAN)
  set(ASAN OFF)
endif()

if(MSVC AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT DISABLE_LTO)
  # Work around MSVC + CMake bug when LTO is enabled.
  # See https://github.com/diasurgical/devilutionX/issues/3778
  # and https://gitlab.kitware.com/cmake/cmake/-/issues/23035
  set(BUILD_TESTING OFF)
endif()

# Note: `CMAKE_CROSSCOMPILING` is only available after the `project` call.
if(CMAKE_CROSSCOMPILING)
  set(BUILD_TESTING OFF)
endif()

if(DISABLE_DEMOMODE)
  # Testing requires demomode.
  set(BUILD_TESTING OFF)
endif()

if(BUILD_TESTING)
  # When tests are enabled, we build a shared devilutionx_so library, which needs to be PIC to link.
  set(PIE ON)
endif()

# Recalculate the dependent options that are defined before `include(Platforms)`:
if(NONET)
  # PACKET_ENCRYPTION is defined before `Platforms.cmake` is included.
  # This means that if a `Platforms.cmake` sets NONET to OFF, PACKET_ENCRYPTION will not automatically
  # reflect that.
  set(PACKET_ENCRYPTION OFF)
endif()
# === End of option overrides ===

if(PIE)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

if(DEVILUTIONX_DISABLE_RTTI)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  elseif(MSVC)
    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  endif()
endif()

if(DEVILUTIONX_DISABLE_EXCEPTIONS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
  elseif(MSVC)
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  endif()
endif()

# Remove unused symbols in non-debug mode.
# This is useful even with LTO (-84 KiB with MinSizeRel).
#
# PS4 toolchain crashes in `create-fself` when linking with these flags, so we exclude it:
# https://github.com/PacBrew/ps4-openorbis/issues/8
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT PS4)
  add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-ffunction-sections;-fdata-sections>")
  if(APPLE)
    add_link_options("$<$<NOT:$<CONFIG:Debug>>:LINKER:-dead_strip>")
  else()
    add_link_options("$<$<NOT:$<CONFIG:Debug>>:LINKER:--gc-sections,--as-needed>")
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # -fipa-pta and -fdevirtualize-at-ltrans improve performance.
  add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-fipa-pta;-fdevirtualize-at-ltrans>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options("$<$<BOOL:${DEVILUTIONX_PROFILE_GENERATE}>:-fprofile-generate>")
  add_link_options("$<$<BOOL:${DEVILUTIONX_PROFILE_GENERATE}>:-fprofile-generate>")
  add_compile_options("$<$<BOOL:${DEVILUTIONX_PROFILE_USE}>:-fprofile-use>")
  add_link_options("$<$<BOOL:${DEVILUTIONX_PROFILE_USE}>:-fprofile-use>")
  add_compile_options("$<$<BOOL:${DEVILUTIONX_PROFILE_DIR}>:-fprofile-dir=${DEVILUTIONX_PROFILE_DIR};-fprofile-prefix-path=${CMAKE_CURRENT_BINARY_DIR}>")
  add_link_options("$<$<BOOL:${DEVILUTIONX_PROFILE_DIR}>:-fprofile-dir=${DEVILUTIONX_PROFILE_DIR};-fprofile-prefix-path=${CMAKE_CURRENT_BINARY_DIR}>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # u8path() function is deprecated but there is no sensible alternative and it might even get un-deprecated.
  add_definitions(-D_SILENCE_CXX20_U8PATH_DEPRECATION_WARNING)
  if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.25")
    # This enables Edit & Continue support, see https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio#edit-and-continue-for-cmake-projects
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug>:EditAndContinue>") # Sets /ZI compiler option, see https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_DEBUG_INFORMATION_FORMAT.html
    add_link_options("$<$<CONFIG:Debug>:/INCREMENTAL>")
  endif()
endif()

# Not a genexp because CMake doesn't support it
# https://gitlab.kitware.com/cmake/cmake/-/issues/20546
if(NOT DISABLE_LTO)
  # LTO if supported:
  include(CheckIPOSupported)
  check_ipo_supported(RESULT is_ipo_supported OUTPUT lto_error)
  if(is_ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
  endif()
endif()

if(GPERF)
  set(DEVILUTIONX_STATIC_CXX_STDLIB OFF)

  if(GPERF_HEAP_FIRST_GAME_ITERATION)
    set(GPERF_HEAP_MAIN ON)
  endif()

  # Compile with information about file and line numbers for everything
  # even in non-Debug build types.
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-g2>")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Use the more size-efficient `-gmlt` option on clang.
    add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-gmlt>")
  endif()
endif()

set(CMAKE_CXX_STANDARD 20)

# On some platforms, such as DJGPP,
# `-std=c++20` defines `__STRICT_ANSI__` which disables
# all POSIX extensions, so we need to use `-std=gnu++20` instead.
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clang-tidy
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
if(NOT TARGET_PLATFORM STREQUAL "dos")
  find_package(Threads REQUIRED)
endif()

# Dependencies must be included after Platforms.
include(Dependencies)

add_subdirectory(Source)

set(BIN_TARGET devilutionx)
if(NINTENDO_3DS)
  set(BIN_TARGET ${BIN_TARGET}.elf)
elseif(TARGET_PLATFORM STREQUAL "dos")
  set(BIN_TARGET devx)
endif()

if(ANDROID)
  add_library(${BIN_TARGET} SHARED Source/main.cpp)
elseif(UWP_LIB)
  set(BIN_TARGET libdevilutionx)
else()
  add_executable(${BIN_TARGET}
    WIN32
    MACOSX_BUNDLE
    Source/main.cpp
    Packaging/windows/devilutionx.exe.manifest
    Packaging/windows/devilutionx.rc
    Packaging/apple/LaunchScreen.storyboard)

    if(CMAKE_STRIP AND NOT DEVILUTIONX_DISABLE_STRIP)
      add_custom_command(
        TARGET ${BIN_TARGET} POST_BUILD
        COMMAND $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:${CMAKE_STRIP}>
        ARGS $<TARGET_FILE:${BIN_TARGET}>)
    endif()
endif()

if(TARGET_PLATFORM STREQUAL "dos")
    # Allow multiple definitions for math stubs in DJGPP
    set_target_properties(${BIN_TARGET} PROPERTIES
        LINK_FLAGS "-Wl,--allow-multiple-definition -static"
    )
    target_link_libraries(${BIN_TARGET} PRIVATE m)
endif()

if(NOT UWP_LIB)
  target_link_dependencies(${BIN_TARGET} PRIVATE libdevilutionx)
endif()

if(GPERF)
  target_link_libraries(${BIN_TARGET} PUBLIC ${GPERFTOOLS_LIBRARIES})
endif()

# Must be included after `BIN_TARGET` and `libdevilutionx` are defined.
include(Assets)
include(Mods)

if(EMSCRIPTEN)
  target_link_options(${BIN_TARGET} PRIVATE --preload-file assets)
endif()

if(NOT USE_SDL1 AND NOT UWP_LIB)
  target_link_libraries(${BIN_TARGET} PUBLIC ${SDL2_MAIN})
endif()

if(BUILD_TESTING)
  include(Tests)
endif()

include(functions/set_relative_file_macro)
set_relative_file_macro(${BIN_TARGET})

if(APPLE)
  set(MACOSX_BUNDLE_GUI_IDENTIFIER com.diasurgical.devilutionx)
  set(MACOSX_BUNDLE_COPYRIGHT Unlicense)
  set(MACOSX_BUNDLE_BUNDLE_NAME devilutionx)
  set(MACOSX_BUNDLE_DISPLAY_NAME DevilutionX)
  set(MACOSX_BUNDLE_INFO_STRING ${PROJECT_VERSION})
  set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
  set(MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}")
  if(IOS)
    set(MACOSX_BUNDLE_REQUIRED_PLATFORM IPhoneOS)
    set_target_properties(${BIN_TARGET} PROPERTIES XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "9.0")
  elseif(DARWIN_MAJOR_VERSION GREATER_EQUAL 17)
    set(MACOSX_BUNDLE_REQUIRED_PLATFORM Carbon)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13.0")
  endif()
  if(DARWIN_MAJOR_VERSION VERSION_LESS 9)
    # Finder on OSX Tiger can only handle icns files with up to 128x128 icons.
    set(_icon_file AppIcon_128)
  else()
    set(_icon_file AppIcon)
  endif()
  target_sources(${BIN_TARGET} PRIVATE "Packaging/apple/${_icon_file}.icns")
  set_source_files_properties("./Packaging/apple/${_icon_file}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(${BIN_TARGET} PROPERTIES MACOSX_BUNDLE_ICON_FILE "${_icon_file}.icns")
  set_target_properties(${BIN_TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/apple/Info.plist")

  install (TARGETS ${BIN_TARGET} DESTINATION ./)

  if(MACOSX_STANDALONE_APP_BUNDLE)
      install(CODE "
        include(BundleUtilities)
        fixup_bundle(${CMAKE_BINARY_DIR}/${MACOSX_BUNDLE_BUNDLE_NAME}.app \"\" \"\")
        "
        COMPONENT Runtime)
  endif()

  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  if(COREFOUNDATION_LIBRARY)
    target_link_libraries(libdevilutionx PUBLIC "${COREFOUNDATION_LIBRARY}")
    target_compile_definitions(libdevilutionx PRIVATE USE_COREFOUNDATION)
  endif()

  set(MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}")
  set(CPACK On)
endif()

if(NINTENDO_SWITCH)
  nx_generate_nacp (${BIN_TARGET}.nacp
    NAME    "DevilutionX"
    AUTHOR  "Devilution Team"
    VERSION "${PROJECT_VERSION}"
  )

  file(MAKE_DIRECTORY "${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}")
  nx_create_nro(${BIN_TARGET}
    NACP  ${BIN_TARGET}.nacp
    ICON  "${PROJECT_SOURCE_DIR}/Packaging/switch/icon.jpg"
    ROMFS ${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}
  )
endif()

if(VITA)
  set(VITA_APP_NAME "devilutionX")
  set(VITA_TITLEID  "DVLX00001")
  set(VITA_VERSION  "01.00")
  set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")
  set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")
  vita_create_self(devilutionx.self devilutionx UNSAFE)
  if(BUILD_ASSETS_MPQ OR SRC_DIST)
    vita_create_vpk(devilutionx.vpk ${VITA_TITLEID} devilutionx.self
      VERSION ${VITA_VERSION}
      NAME ${VITA_APP_NAME}
      FILE Packaging/vita/sce_sys sce_sys
      FILE ${DEVILUTIONX_MPQ} devilutionx.mpq
      FILE ${HELLFIRE_MPQ} mods/Hellfire.mpq
    )
  else()
    vita_create_vpk(devilutionx.vpk ${VITA_TITLEID} devilutionx.self
      VERSION ${VITA_VERSION}
      NAME ${VITA_APP_NAME}
      FILE Packaging/vita/sce_sys sce_sys
      FILE assets assets
      ${VITA_TRANSLATIONS_LIST}
    )
  endif()

endif()

if(PS4)
  add_custom_command(
    TARGET devilutionx_mpq POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DEVILUTIONX_MPQ} "${PROJECT_SOURCE_DIR}/Packaging/ps4/")
  add_custom_command(
    TARGET hellfire_mpq POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/Packaging/ps4/mods"
    COMMAND ${CMAKE_COMMAND} -E copy ${HELLFIRE_MPQ} "${PROJECT_SOURCE_DIR}/Packaging/ps4/mods/")
  add_self(${BIN_TARGET})
  add_pkg(${BIN_TARGET} "${PROJECT_SOURCE_DIR}/Packaging/ps4"
    "DVLX00001" "DevilutionX" "${PROJECT_VERSION}")
endif()

if(NINTENDO_3DS)
  set(APP_TITLE       "DevilutionX")
  set(APP_DESCRIPTION "DevilutionX port for 3DS")
  set(APP_AUTHOR      "Diasurgical Team")
  set(APP_ICON        "${PROJECT_SOURCE_DIR}/Packaging/ctr/icon.png")
  set(APP_BANNER      "${PROJECT_SOURCE_DIR}/Packaging/ctr/banner.png")
  set(APP_AUDIO       "${CMAKE_BINARY_DIR}/banner_audio.wav")
  set(APP_RSF         "${PROJECT_SOURCE_DIR}/Packaging/ctr/template.rsf")
  set(APP_VERSION ${PROJECT_VERSION})

  find_program(FFMPEG ffmpeg)
  if(FFMPEG)
    add_custom_command(OUTPUT ${APP_AUDIO}
      COMMAND ${FFMPEG} -y -ss 3.3 -t 3 -i "${PROJECT_SOURCE_DIR}/Packaging/resources/shareware-startup.wav" -af "afade=t=in:st=0:d=0.1,afade=t=out:st=2.9:d=0.1" ${APP_AUDIO}
      DEPENDS ${PROJECT_SOURCE_DIR}/Packaging/resources/shareware-startup.wav
      VERBATIM)
  else()
    add_custom_command(OUTPUT ${APP_AUDIO}
      COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Packaging/ctr/audio_silent.wav ${APP_AUDIO}
      DEPENDS ${PROJECT_SOURCE_DIR}/Packaging/ctr/audio_silent.wav
      VERBATIM)
  endif()

  include(Tools3DS)
  add_3dsx_target(${BIN_TARGET})
  add_cia_target(${BIN_TARGET} ${APP_RSF} ${APP_BANNER} ${APP_AUDIO})
endif()

if(NXDK)
  target_link_libraries(${BIN_TARGET} PRIVATE "${NXDK_DIR}/lib/libnxdk_automount_d.lib")
  target_link_options(${BIN_TARGET} PRIVATE "-include:_automount_d_drive")

  set(_nxdk_pkg_dir "${CMAKE_BINARY_DIR}/pkg")
  set(_xbe_path "${_nxdk_pkg_dir}/default.xbe")
  add_custom_command(
    OUTPUT "${_xbe_path}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_nxdk_pkg_dir}"
    COMMAND "${NXDK_DIR}/tools/cxbe/cxbe" "-OUT:${_xbe_path}" -TITLE:DevilutionX "-Logo:${PROJECT_SOURCE_DIR}/Packaging/xbox_nxdk/xbe_logo.pgm" "${CMAKE_BINARY_DIR}/${BIN_TARGET}.exe"
    DEPENDS "${BIN_TARGET}"
  )
  add_custom_target(nxdk_xbe DEPENDS "${_xbe_path}")
endif()

if(CPACK AND (APPLE OR BUILD_ASSETS_MPQ OR SRC_DIST))
  if(WIN32)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      set(SDL2_WIN32_DLLS_DIR "${CMAKE_BINARY_DIR}")
    else()
      set(SDL2_WIN32_DLLS_DIR "${SDL2_EXEC_PREFIX}")
    endif()
    set(SDL2_WIN32_LICENSES_DIR "${PROJECT_SOURCE_DIR}/Packaging/resources")

    file(GLOB SDL2_WIN32_ALL_DLLS
      LIST_DIRECTORIES false
      "${SDL2_WIN32_DLLS_DIR}/*.dll")
    file(GLOB SDL2_WIN32_ALL_LICENSES
      LIST_DIRECTORIES false
      "${SDL2_WIN32_LICENSES_DIR}/LICENSE*.txt"
      "${SDL2_WIN32_LICENSES_DIR}/README*.txt")

    set(CPACK_PACKAGE_NAME ${project_name})
    if(WIN_NSIS)
      set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}_${PROJECT_VERSION}_Installer")
      set(CPACK_GENERATOR "NSIS")
      set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
      set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/Packaging/windows/icon.ico")
      set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/Packaging/windows/icon.ico")
      set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.md")
      set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
      set(CPACK_PACKAGE_EXECUTABLES "devilutionx" "DevilutionX")
      set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${BIN_TARGET}")
    else()
      set(CPACK_PACKAGE_FILE_NAME "devilutionx")
      set(CPACK_GENERATOR "ZIP")
    endif()
    set(CPACK_STRIP_FILES TRUE)
    install(TARGETS ${BIN_TARGET} DESTINATION .)
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/windows/README.txt"
      DESTINATION "."
    )
    install(FILES "${DEVILUTIONX_MPQ}"
      DESTINATION "."
    )
    install(FILES "${HELLFIRE_MPQ}"
      DESTINATION "mods"
    )

    foreach(_SDL2_WIN32_DLL_PATH ${SDL2_WIN32_ALL_DLLS} ${WIN32_INSTALL_DLLS})
      install(FILES "${_SDL2_WIN32_DLL_PATH}"
        DESTINATION "."
      )
    endforeach()

    foreach(_SDL2_WIN32_LICENSE_PATH ${SDL2_WIN32_ALL_LICENSES})
      install(FILES "${_SDL2_WIN32_LICENSE_PATH}"
        DESTINATION "LICENSE"
      )
    endforeach()

    if(DISCORD_SHARED_LIB)
      install(FILES "${DISCORD_SHARED_LIB}"
        DESTINATION "."
      )
    endif()

    if(SCREEN_READER_INTEGRATION)
      install(FILES "${Tolk_BINARY_DIR}/libTolk.dll"
        DESTINATION "."
      )
    endif()

  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(TOLOWER ${PROJECT_NAME} project_name)
    set(CPACK_PACKAGE_NAME ${project_name})
    set(CPACK_GENERATOR "7Z")
    # Common *nix files
    set(CPACK_STRIP_FILES TRUE)
    install(TARGETS ${BIN_TARGET} DESTINATION bin)
    set(desktop_file "${PROJECT_SOURCE_DIR}/Packaging/nix/${project_name}.desktop")
    set(desktop_file_hellfire "${PROJECT_SOURCE_DIR}/Packaging/nix/${project_name}-hellfire.desktop")

    find_program(DFI desktop-file-install)
    if(DFI)
      execute_process(COMMAND ${DFI} --dir=${CMAKE_BINARY_DIR} ${desktop_file})
      set(desktop_file "${CMAKE_BINARY_DIR}/${project_name}.desktop")
      execute_process(COMMAND ${DFI} --dir=${CMAKE_BINARY_DIR} ${desktop_file_hellfire})
      set(desktop_file_hellfire "${CMAKE_BINARY_DIR}/${project_name}-hellfire.desktop")
    endif()

    install(FILES "${desktop_file}"
      DESTINATION "share/applications"
    )
    install(FILES "${desktop_file_hellfire}"
      DESTINATION "share/applications"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/nix/README.txt"
      DESTINATION "share/diasurgical/${project_name}"
    )
    install(FILES "${DEVILUTIONX_MPQ}"
      DESTINATION "share/diasurgical/${project_name}"
    )
    install(FILES "${HELLFIRE_MPQ}"
      DESTINATION "share/diasurgical/${project_name}/mods"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/resources/icon_flat.png"
      DESTINATION "share/icons/hicolor/512x512/apps"
      RENAME "${project_name}.png"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/resources/hellfire.png"
      DESTINATION "share/icons/hicolor/512x512/apps"
      RENAME "${project_name}-hellfire.png"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/nix/devilutionx.metainfo.xml"
      DESTINATION "share/metainfo"
      RENAME "${project_name}.metainfo.xml"
    )
    if(DISCORD_SHARED_LIB)
      install(FILES "${DISCORD_SHARED_LIB}" DESTINATION "lib")
    endif()

    # -G DEB
    set(CPACK_PACKAGE_CONTACT "anders@jenbo.dk")
    if(USE_SDL1)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl1.2debian")
    else()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libsdl2-image-2.0-0")
    endif()
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # -G RPM
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

    find_program(RPMBUILD rpmbuild)
    if(RPMBUILD)
      list(APPEND CPACK_GENERATOR "RPM")
    endif()
    find_program(DPKG dpkg)
    if(DPKG)
      list(APPEND CPACK_GENERATOR "DEB")
    endif()

  elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "devilutionx")
    set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK "ON")
    set(CPACK_STRIP_FILES TRUE)
    set(CPACK_GENERATOR "DragNDrop")
  endif()

  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
  include(CPack)
endif()

resolve_target_link_dependencies()
if(UWP_LIB)
  get_target_property(_linked_objects libdevilutionx LINKED_OBJECTS)
  target_sources(libdevilutionx PRIVATE ${_linked_objects})
endif()
