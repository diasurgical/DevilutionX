set(CMAKE_SYSTEM_NAME DOS)
set(CMAKE_SYSTEM_VERSION 1)

set(TARGET_PLATFORM "dos" CACHE STRING "Target platform")

# CMake's Platform/DOS.cmake assumes OpenWatcom naming, override untill (possibly) upstreamed.
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/djgpp-platform-overrides.cmake")

# Locate the DJGPP cross-compiler Different DJGPP builds use different target
# triplets (i386- or i586-pc-msdosdjgpp) and install to different prefixes, so
# we search for it rather than hardcoding. Set DJGPP_PREFIX (or put the tools
# on PATH) to point at a non-standard install.
if(DEFINED ENV{DJGPP_PREFIX})
  set(_djgpp_bin_hints "$ENV{DJGPP_PREFIX}/bin" "$ENV{DJGPP_PREFIX}")
endif()

find_program(DJGPP_GCC
  NAMES i586-pc-msdosdjgpp-gcc i386-pc-msdosdjgpp-gcc
  HINTS ${_djgpp_bin_hints}
  PATHS
    /opt/i386-pc-msdosdjgpp-toolchain/bin
    /opt/djgpp/bin
    /usr/local/djgpp/bin
    "$ENV{HOME}/.local/bin")

if(NOT DJGPP_GCC)
  message(FATAL_ERROR
    "DJGPP cross-compiler not found (looked for i586/i386-pc-msdosdjgpp-gcc on "
    "PATH and common install locations). Install DJGPP or set DJGPP_PREFIX to "
    "its install root.")
endif()

# Derive the install prefix from the compiler location: <prefix>/bin/<triplet>-gcc
get_filename_component(_djgpp_bin "${DJGPP_GCC}" DIRECTORY)
get_filename_component(DJGPP_PREFIX "${_djgpp_bin}" DIRECTORY)

# Determine the *real* target triplet from the sysroot directory that actually
# contains the DJGPP headers/libs. This is the source of truth: it is named
# after the triplet the toolchain was built with (e.g. i386-pc-msdosdjgpp),
# regardless of whether the compiler was invoked through an i586- symlink.
file(GLOB _djgpp_roots "${DJGPP_PREFIX}/*-pc-msdosdjgpp")
foreach(_root ${_djgpp_roots})
  if(IS_DIRECTORY "${_root}/sys-include")
    set(DJGPP_ROOT "${_root}")
    break()
  endif()
endforeach()

if(NOT DJGPP_ROOT)
  message(FATAL_ERROR
    "Found DJGPP compiler at ${DJGPP_GCC} but no matching sysroot "
    "(<prefix>/<triplet>-pc-msdosdjgpp/sys-include) under ${DJGPP_PREFIX}.")
endif()

get_filename_component(DJGPP_TARGET "${DJGPP_ROOT}" NAME)

set(CMAKE_C_COMPILER   "${_djgpp_bin}/${DJGPP_TARGET}-gcc")
set(CMAKE_CXX_COMPILER "${_djgpp_bin}/${DJGPP_TARGET}-g++")
set(CMAKE_STRIP        "${_djgpp_bin}/${DJGPP_TARGET}-strip")
set(PKG_CONFIG_EXECUTABLE "${_djgpp_bin}/${DJGPP_TARGET}-pkg-config" CACHE STRING "Path to pkg-config")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")

set(CMAKE_FIND_ROOT_PATH "${DJGPP_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

link_directories("${DJGPP_ROOT}/lib")
include_directories(BEFORE SYSTEM "${DJGPP_ROOT}/sys-include" "${DJGPP_ROOT}/include")
