if(DEVILUTIONX_STATIC_SDL2)
  set(BUILD_SHARED_LIBS OFF)
  set(SDL_SHARED OFF)
  set(SDL_STATIC ON)
  if(PIE)
    set(SDL_STATIC_PIC ON)
  endif()
else()
  set(BUILD_SHARED_LIBS ON)
  set(SDL_SHARED ON)
  set(SDL_STATIC OFF)
endif()
set(SDL_TEST_ENABLED_BY_DEFAULT OFF)

include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

if(TARGET_PLATFORM STREQUAL "dos")
  set(DOS ON)
  FetchContent_Declare(SDL2
    # branch: dos-vbe-rebase
    URL https://github.com/diasurgical/SDL/archive/20b47a62a12f0c7be4052fb5b32eb21d709d6f06.tar.gz
    URL_HASH MD5=6e77a1d1a79d7f0dd6231b39515a341f
  )
else()
  FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.8/SDL2-2.32.8.tar.gz
    URL_HASH SHA256=0ca83e9c9b31e18288c7ec811108e58bac1f1bb5ec6577ad386830eac51c787e
  )
endif()
FetchContent_MakeAvailable_ExcludeFromAll(SDL2)
