set(BUILD_TESTING OFF)
set(ASAN OFF)
set(UBSAN OFF)
set(NONET ON)
set(USE_SDL1 ON)
set(SDL1_VIDEO_MODE_BPP 8)

set(DEVILUTIONX_SYSTEM_BZIP2 OFF)
set(DEVILUTIONX_SYSTEM_ZLIB OFF)

# Lower the optimization level to O2 because there are issues with O3.
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

# `fseeko` fails to link on Amiga.
add_definitions(-Dfseeko=fseek)

list(APPEND DEVILUTIONX_PLATFORM_LINK_LIBRARIES ZLIB::ZLIB)
if(NOT WARPOS)
  list(APPEND DEVILUTIONX_PLATFORM_LINK_LIBRARIES -ldebug)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/amiga/devilutionx.info" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
