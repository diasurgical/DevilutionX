# Not automatically defined by sh-elf-gcc
add_compile_definitions(__DREAMCAST__)

set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "" FORCE)

set(ASAN OFF)
set(UBSAN OFF)
set(BUILD_TESTING OFF)
set(NONET ON)
set(USE_SDL1 ON)
set(NOEXIT ON)
set(PREFILL_PLAYER_NAME ON)
set(DISABLE_DEMOMODE ON)
set(DEVILUTIONX_SYSTEM_LIBSODIUM OFF)
set(DEVILUTIONX_SYSTEM_SDL_IMAGE OFF)
set(UNPACKED_MPQS OFF)

# CD reads via MPQ are too slow for real-time audio
set(DISABLE_STREAMING_SOUNDS ON)
set(DEFAULT_AUDIO_BUFFER_SIZE 2048)
set(DEFAULT_AUDIO_SAMPLE_RATE 22050)
set(DEFAULT_AUDIO_RESAMPLING_QUALITY 0)

set(DEFAULT_PER_PIXEL_LIGHTING 0)
set(DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT ON CACHE BOOL "" FORCE)

# GPF SDL supports 8bpp with hardware palette - lets SDL handle the
# palette-to-RGB conversion internally via PVR DMA, avoiding the
# manual 8bpp->16bpp conversion in dc_video.cpp entirely.
set(SDL1_VIDEO_MODE_BPP 8)
set(SDL1_VIDEO_MODE_FLAGS SDL_DOUBLEBUF|SDL_HWSURFACE)

set(DEFAULT_WIDTH 640)
set(DEFAULT_HEIGHT 480)

# VMU saves use flat files with zlib compression
set(UNPACKED_SAVES ON)

set(DEVILUTIONX_GAMEPAD_TYPE Generic)

# GPF SDL button IDs (from SDL_dreamcast.h SDL_DC_button enum)
set(JOY_BUTTON_A 2)
set(JOY_BUTTON_B 1)
set(JOY_BUTTON_X 5)
set(JOY_BUTTON_Y 6)
set(JOY_BUTTON_START 3)

set(JOY_HAT_DPAD_UP_HAT 0)
set(JOY_HAT_DPAD_DOWN_HAT 0)
set(JOY_HAT_DPAD_LEFT_HAT 0)
set(JOY_HAT_DPAD_RIGHT_HAT 0)
set(JOY_HAT_DPAD_UP 1)
set(JOY_HAT_DPAD_DOWN 4)
set(JOY_HAT_DPAD_LEFT 8)
set(JOY_HAT_DPAD_RIGHT 2)

set(JOY_AXIS_LEFTX 0)
set(JOY_AXIS_LEFTY 1)

list(APPEND DEVILUTIONX_PLATFORM_SUBDIRECTORIES platform/dreamcast)
list(APPEND DEVILUTIONX_PLATFORM_LINK_LIBRARIES libdevilutionx_dreamcast)

# FindZLIB may not create this target during cross-compilation.
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB STATIC IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_LOCATION "${ZLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
    )
endif()

# FindBZip2 may not create this target during cross-compilation
if(NOT TARGET BZip2::BZip2)
    add_library(BZip2::BZip2 STATIC IMPORTED)
    set_target_properties(BZip2::BZip2 PROPERTIES
        IMPORTED_LOCATION "${BZIP2_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${BZIP2_INCLUDE_DIR}"
    )
endif()
