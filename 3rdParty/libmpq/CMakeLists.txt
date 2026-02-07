if(NOT TARGET ZLIB::ZLIB AND NOT ZLIB_FOUND)
  find_package(ZLIB REQUIRED)
endif()

if(NOT TARGET BZip2::BZip2 AND NOT BZip2_FOUND AND NOT BZIP2_FOUND)
  find_package(BZip2 REQUIRED)
endif()

include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)
FetchContent_Declare_ExcludeFromAll(libmpq
    URL https://github.com/diasurgical/libmpq/archive/7c2924d4553513eba1a70bbdb558198dd8c2726a.tar.gz
    URL_HASH MD5=315c88c02b45851cdfee8460322de044
)
FetchContent_MakeAvailable_ExcludeFromAll(libmpq)

add_library(libmpq STATIC
  ${libmpq_SOURCE_DIR}/libmpq/common.c
  ${libmpq_SOURCE_DIR}/libmpq/explode.c
  ${libmpq_SOURCE_DIR}/libmpq/extract.c
  ${libmpq_SOURCE_DIR}/libmpq/huffman.c
  ${libmpq_SOURCE_DIR}/libmpq/mpq.c
  ${libmpq_SOURCE_DIR}/libmpq/wave.c
)

target_include_directories(libmpq PUBLIC ${libmpq_SOURCE_DIR})
target_include_directories(libmpq PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(libmpq PRIVATE ZLIB::ZLIB BZip2::BZip2)

if(LIBMPQ_FILE_BUFFER_SIZE)
  target_compile_definitions(libmpq PRIVATE "LIBMPQ_FILE_BUFFER_SIZE=${LIBMPQ_FILE_BUFFER_SIZE}")
endif()

if(DEVILUTIONX_WINDOWS_NO_WCHAR)
  target_compile_definitions(libmpq PRIVATE LIBMPQ_WINDOWS_NO_WCHAR)
endif()
