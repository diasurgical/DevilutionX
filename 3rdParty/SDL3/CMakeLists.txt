if(DEVILUTIONX_STATIC_SDL3)
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

FetchContent_Declare(SDL3
  URL https://github.com/libsdl-org/SDL/archive/f173fd28f04cb64ae054d6a97edb5d33925f539b.tar.gz
  URL_HASH SHA256=f7501d84c1a7f168567c002f4e1db4f220c4a34c51f7fa7d199962d0ed5fb42c
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL3)
