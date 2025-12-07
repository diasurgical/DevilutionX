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
  URL https://github.com/libsdl-org/SDL/archive/02c4478f9364ac9ba8c0b157c01e47c2f059552c.tar.gz
  URL_HASH SHA256=1c8e9369ef6c67b6ca4102cc957846966815de56924b24a8a28feeac344a506a
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL3)
