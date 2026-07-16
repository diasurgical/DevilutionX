include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)
FetchContent_Declare_ExcludeFromAll(asio
    URL https://github.com/diasurgical/asio/archive/2300b7a0684cf8c390b78ca67368242985f502f6.tar.gz
    URL_HASH MD5=8d868bb241e96f7eb078e695f177098c
)
FetchContent_MakeAvailable_ExcludeFromAll(asio)

add_library(asio STATIC ${CMAKE_CURRENT_LIST_DIR}/asio_handle_exception.cpp)
target_compile_definitions(asio PUBLIC ASIO_NO_EXCEPTIONS)
target_include_directories(asio PUBLIC ${asio_SOURCE_DIR}/asio/include ${CMAKE_CURRENT_LIST_DIR})

if(NINTENDO_3DS OR NINTENDO_SWITCH)
  include(asio_defs REQUIRED)
endif()
