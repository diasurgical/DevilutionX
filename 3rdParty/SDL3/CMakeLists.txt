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
  URL https://github.com/libsdl-org/SDL/archive/6665ebaa2e9ccad6bc426ab3694a4c067dda5d91.tar.gz
  URL_HASH SHA256=ca8249a85555da575b108e0b233ff616c9e52b91b1934ae983f341b383a85d6f
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL3)
