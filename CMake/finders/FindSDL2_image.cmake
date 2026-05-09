find_package(SDL2_image CONFIG)

if(TARGET SDL2_image::SDL2_image)
  if(NOT TARGET SDL2::SDL2_image)
    add_library(SDL2::SDL2_image ALIAS SDL2_image::SDL2_image)
  endif()
else()
  find_package(PkgConfig)
  pkg_check_modules(PC_SDL2_image QUIET SDL2_image>=2.0.0)

  find_path(SDL2_image_INCLUDE_DIR SDL_image.h
            HINTS ${PC_SDL2_image_INCLUDEDIR} ${PC_SDL2_image_INCLUDE_DIRS})
          
  find_library(SDL2_image_LIBRARY
               NAMES SDL2_image libSDL2_image
               HINTS ${PC_SDL2_image_LIBDIR} ${PC_SDL2_image_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(SDL2_image DEFAULT_MSG
                                    SDL2_image_INCLUDE_DIR SDL2_image_LIBRARY)

  if(SDL2_image_FOUND AND NOT TARGET SDL2::SDL2_image)
    add_library(SDL2::SDL2_image UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2_image PROPERTIES
                          IMPORTED_LOCATION ${SDL2_image_LIBRARY}
                          INTERFACE_INCLUDE_DIRECTORIES ${SDL2_image_INCLUDE_DIR})
  endif()

  if(SDL2_image_FOUND)
    mark_as_advanced(SDL2_image_INCLUDE_DIR SDL2_image_LIBRARY)
    set(SDL2_image_LIBRARIES ${SDL2_image_LIBRARY})
    set(SDL2_image_INCLUDE_DIRS ${SDL2_image_INCLUDE_DIR})
  endif()
endif()
