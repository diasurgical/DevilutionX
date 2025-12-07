set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

if($ENV{DJGPP_PREFIX})
  set(DJGPP_PREFIX "$ENV{DJGPP_PREFIX}")
else()
  set(DJGPP_PREFIX "/opt/i386-pc-msdosdjgpp-toolchain")
endif()

set(CMAKE_C_COMPILER "${DJGPP_PREFIX}/bin/i386-pc-msdosdjgpp-gcc")
set(CMAKE_CXX_COMPILER "${DJGPP_PREFIX}/bin/i386-pc-msdosdjgpp-g++")
set(CMAKE_STRIP "${DJGPP_PREFIX}/bin/i386-pc-msdosdjgpp-strip")
set(PKG_CONFIG_EXECUTABLE "${DJGPP_PREFIX}/bin/i386-pc-msdosdjgpp-pkg-config" CACHE STRING "Path to pkg-config")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")

set(DJGPP_ROOT "${DJGPP_PREFIX}/i386-pc-msdosdjgpp")

set(CMAKE_FIND_ROOT_PATH "${DJGPP_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

link_directories("${DJGPP_ROOT}/lib")
include_directories(BEFORE SYSTEM "${DJGPP_ROOT}/sys-include" "${DJGPP_ROOT}/include")

