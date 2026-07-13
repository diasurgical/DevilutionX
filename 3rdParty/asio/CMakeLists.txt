include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)
FetchContent_Declare_ExcludeFromAll(asio
    URL https://github.com/diasurgical/asio/archive/112f011842f4ae713b158a0fc57009f95c761a21.tar.gz
    URL_HASH MD5=fb5a399c09bceac6431758f5f772db34
)
FetchContent_MakeAvailable_ExcludeFromAll(asio)

add_library(asio STATIC ${CMAKE_CURRENT_LIST_DIR}/asio_handle_exception.cpp)
target_compile_definitions(asio PUBLIC ASIO_NO_EXCEPTIONS)
target_include_directories(asio PUBLIC ${asio_SOURCE_DIR}/asio/include ${CMAKE_CURRENT_LIST_DIR})

if(NINTENDO_3DS OR NINTENDO_SWITCH)
  include(asio_defs REQUIRED)
endif()
