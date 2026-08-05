# Wrapper around CMake's bundled FindSDL module.
#
# The bundled module runs its own `find_package(Threads)` and appends
# `CMAKE_THREAD_LIBS_INIT` to the SDL libraries. AmigaOS has no POSIX threads
# library — SDL 1.2 uses native AmigaOS tasks — and `m68k-amigaos-g++` rejects
# both `-pthread` and `-lpthreads`, so strip the thread flag back out.
#
# This has to live in a wrapper rather than at the call site because
# dependencies built from source (notably SDL_audiolib) call `find_package(SDL)`
# themselves and propagate the flag through their own link interface.

include("${CMAKE_ROOT}/Modules/FindSDL.cmake")

if(AMIGA)
  foreach(_devilutionx_sdl_var SDL_LIBRARY SDL_LIBRARIES SDL_LIBRARY_TEMP)
    if(${_devilutionx_sdl_var})
      list(FILTER ${_devilutionx_sdl_var} EXCLUDE REGEX "pthread")
    endif()
  endforeach()
  unset(_devilutionx_sdl_var)

  # The module also bakes the thread flag into the imported target's link
  # interface, which is what dependencies actually consume.
  if(TARGET SDL::SDL)
    get_target_property(_devilutionx_sdl_iface SDL::SDL INTERFACE_LINK_LIBRARIES)
    if(_devilutionx_sdl_iface)
      list(FILTER _devilutionx_sdl_iface EXCLUDE REGEX "pthread")
      set_target_properties(SDL::SDL PROPERTIES INTERFACE_LINK_LIBRARIES "${_devilutionx_sdl_iface}")
    endif()
    unset(_devilutionx_sdl_iface)
  endif()
endif()
