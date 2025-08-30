set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_C_COMPILER   "$ENV{HOME}/.local/bin/i386-pc-msdosdjgpp-gcc")
set(CMAKE_CXX_COMPILER "$ENV{HOME}/.local/bin/i386-pc-msdosdjgpp-g++")

set(CMAKE_EXE_LINKER_FLAGS "-static")

set(DJGPP_ROOT "$ENV{HOME}/.local/i386-pc-msdosdjgpp")

set(CMAKE_FIND_ROOT_PATH "${DJGPP_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

link_directories("${DJGPP_ROOT}/lib")
include_directories(BEFORE SYSTEM "${DJGPP_ROOT}/sys-include" "${DJGPP_ROOT}/include")

