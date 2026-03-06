include(functions/devilutionx_library)

add_devilutionx_object_library(libdevilutionx_dreamcast
  posix_stubs.c
  dc_video.cpp
  dc_init.cpp
  dc_save_codec.cpp
)

target_link_libraries(libdevilutionx_dreamcast PUBLIC
  DevilutionX::SDL
  ZLIB::ZLIB
)
