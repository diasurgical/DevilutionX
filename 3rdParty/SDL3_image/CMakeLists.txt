include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

# PNG support must be guaranteed even in cross builds (mingw etc.), where no
# system libpng for the target triplet exists: vendor SDL_image's own
# libpng + zlib and link png_static. The GitHub source tarball does NOT
# contain the external/ submodule sources, so fetch the git repository with
# just the two needed submodules instead. Static libpng (PNG_SHARED=OFF)
# avoids LOAD_PNG_DYNAMIC, which would dlopen a runtime DLL that is never
# packaged. Without any of this, SDL3_image silently builds without LOAD_PNG:
# IMG_Load on a valid PNG fails with "Unsupported image format" (breaks LUT
# texture loading) and IMG_SavePNG fails (breaks screenshots).
set(SDLIMAGE_VENDORED ON)
set(SDLIMAGE_PNG_SHARED OFF)

if(DEVILUTIONX_STATIC_SDL_IMAGE)
  set(BUILD_SHARED_LIBS OFF)
else()
  set(BUILD_SHARED_LIBS ON)
endif()

set(SDLIMAGE_BACKEND_STB OFF)
set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_BMP OFF)
set(SDLIMAGE_GIF OFF)
set(SDLIMAGE_JPG OFF)
set(SDLIMAGE_JXL OFF)
set(SDLIMAGE_LBM OFF)
set(SDLIMAGE_PCX OFF)
set(SDLIMAGE_PNG ON)
set(SDLIMAGE_PNM OFF)
set(SDLIMAGE_QOI OFF)
set(SDLIMAGE_SVG OFF)
set(SDLIMAGE_TGA OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_XCF OFF)
set(SDLIMAGE_XPM OFF)
set(SDLIMAGE_XV OFF)
set(SDLIMAGE_AVIF_SAVE OFF)
set(SDLIMAGE_JPG_SAVE OFF)
set(SDLIMAGE_PNG_SAVE ON)

FetchContent_Declare_ExcludeFromAll(SDL_image
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
  GIT_TAG release-3.2.4
  GIT_SUBMODULES "external/libpng;external/zlib"
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL_image)
