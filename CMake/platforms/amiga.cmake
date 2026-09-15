list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/threads-stub")

set(BUILD_TESTING OFF)
set(ASAN OFF)
set(UBSAN OFF)
set(NONET ON)
set(USE_SDL1 ON)
set(SDL1_VIDEO_MODE_BPP 8)

set(DEVILUTIONX_SYSTEM_BZIP2 OFF)
set(DEVILUTIONX_SYSTEM_ZLIB OFF)

list(APPEND DEVILUTIONX_PLATFORM_LINK_LIBRARIES ZLIB::ZLIB)
if(NOT WARPOS)
  list(APPEND DEVILUTIONX_PLATFORM_LINK_LIBRARIES -ldebug)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/amiga/devilutionx.info" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

# zlib uses check_function_exists() to determine if fseeko exists.
# That doesn't work properly when CMAKE_TRY_COMPILE_TARGET_TYPE
# is set to STATIC_LIBRARY, as it normally should be when cross-compiling.
# Instead, call check_symbol_exists() here before zlib has a chance to get it wrong.
include(CheckSymbolExists)
check_symbol_exists(fseeko stdio.h HAVE_FSEEKO)
