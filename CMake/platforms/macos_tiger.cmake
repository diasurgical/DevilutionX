# ASAN and UBSAN are not supported by macports gcc14 on PowerPC.
set(ASAN OFF)
set(UBSAN OFF)

# SDL2 does not build for Tiger, so we use SDL1 instead.
set(USE_SDL1 ON)

# ZeroTier is yet to be tested.
set(DISABLE_ZERO_TIER ON)

# https://trac.macports.org/ticket/71511
set(DEVILUTIONX_SYSTEM_GOOGLETEST OFF)
set(DEVILUTIONX_STATIC_GOOGLETEST OFF)
set(DEVILUTIONX_SYSTEM_BENCHMARK OFF)
set(DEVILUTIONX_STATIC_BENCHMARK OFF)
