set(ASAN OFF)
set(UBSAN OFF)
set(NONET ON)
set(DEVILUTIONX_SYSTEM_SDL_IMAGE OFF)
set(BUILD_TESTING OFF)
set(PREFILL_PLAYER_NAME ON)
set(NOEXIT ON)

# 16-bit ABGR1555 is a native texture format of the PSP GPU.
set(DEVILUTIONX_DISPLAY_TEXTURE_FORMAT SDL_PIXELFORMAT_ABGR1555)

# Use lower resampling quality for PSP performance.
set(DEFAULT_AUDIO_RESAMPLING_QUALITY 2)

# Per-pixel lighting is too slow on the PSP.
set(DEFAULT_PER_PIXEL_LIGHTING false)
