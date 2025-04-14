if(DEVILUTIONX_STATIC_SDL_IMAGE)
  set(BUILD_SHARED_LIBS OFF)
else()
  set(BUILD_SHARED_LIBS ON)
endif()

set(SDL2IMAGE_AVIF OFF)
set(SDL2IMAGE_BMP OFF)
set(SDL2IMAGE_GIF OFF)
set(SDL2IMAGE_JPG OFF)
set(SDL2IMAGE_LBM OFF)
set(SDL2IMAGE_PCX OFF)
set(SDL2IMAGE_PNM OFF)
set(SDL2IMAGE_QOI OFF)
set(SDL2IMAGE_SVG OFF)
set(SDL2IMAGE_TGA OFF)
set(SDL2IMAGE_TIF OFF)
set(SDL2IMAGE_WEBP OFF)
set(SDL2IMAGE_XCF OFF)
set(SDL2IMAGE_XPM OFF)
set(SDL2IMAGE_XV OFF)

include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)
FetchContent_Declare_ExcludeFromAll(SDL_image
  URL https://github.com/libsdl-org/SDL_image/archive/refs/tags/release-2.8.8.tar.gz
  URL_HASH MD5=552ef2c342a80616d3a19fe0bd07dc3a
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL_image)

add_library(SDL2::SDL2_image ALIAS SDL2_image)
