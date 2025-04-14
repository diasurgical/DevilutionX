include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(bzip2
    GIT_REPOSITORY https://sourceware.org/git/bzip2
    GIT_REPOSITORY https://gitlab.com/bzip2/bzip2
    GIT_TAG bzip2-1.0.8
)
FetchContent_MakeAvailable_ExcludeFromAll(bzip2)

if(DEVILUTIONX_STATIC_BZIP2)
  set(_lib_type STATIC)
else()
  set(_lib_type SHARED)
endif()
add_library(BZip2 ${_lib_type}
  ${bzip2_SOURCE_DIR}/blocksort.c
  ${bzip2_SOURCE_DIR}/bzlib.c
  ${bzip2_SOURCE_DIR}/compress.c
  ${bzip2_SOURCE_DIR}/crctable.c
  ${bzip2_SOURCE_DIR}/decompress.c
  ${bzip2_SOURCE_DIR}/huffman.c
  ${bzip2_SOURCE_DIR}/randtable.c
  ${bzip2_SOURCE_DIR}/bzlib.h
  ${bzip2_SOURCE_DIR}/bzlib_private.h
  ${CMAKE_CURRENT_LIST_DIR}/bz_internal_error.c
)
target_compile_definitions(BZip2 PRIVATE -DBZ_NO_STDIO)
target_include_directories(BZip2 PUBLIC ${bzip2_SOURCE_DIR})

add_library(BZip2::BZip2 ALIAS BZip2)
