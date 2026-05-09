include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)
FetchContent_Declare_ExcludeFromAll(libsmackerdec
    URL https://github.com/diasurgical/libsmackerdec/archive/0aaaf8c94a097b009d844db0d44dd7cd0ff81922.tar.gz
    URL_HASH SHA256=c3a7fbc91531eee8989b1d281415adc0209d84ffc0437c6b0b4f677ae7fd3b41
)
FetchContent_MakeAvailable_ExcludeFromAll(libsmackerdec)

add_library(libsmackerdec STATIC
  ${libsmackerdec_SOURCE_DIR}/src/BitReader.cpp
  ${libsmackerdec_SOURCE_DIR}/src/FileStream.cpp
  ${libsmackerdec_SOURCE_DIR}/src/LogError.cpp
  ${libsmackerdec_SOURCE_DIR}/src/SmackerDecoder.cpp)

target_include_directories(libsmackerdec PUBLIC ${libsmackerdec_SOURCE_DIR}/include)

if(USE_SDL1)
  target_link_libraries(libsmackerdec PUBLIC ${SDL_LIBRARY})
elseif(USE_SDL3)
  target_link_libraries(libsmackerdec PUBLIC SDL3::SDL3)
  target_compile_definitions(libsmackerdec PUBLIC USE_SDL3)
elseif(TARGET SDL2::SDL2 AND NOT (DEVILUTIONX_STATIC_SDL2 AND TARGET SDL2::SDL2-static))
  target_link_libraries(libsmackerdec PUBLIC SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
  target_link_libraries(libsmackerdec PUBLIC SDL2::SDL2-static)
endif()
