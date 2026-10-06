include(functions/devilutionx_library)

add_devilutionx_object_library(libdevilutionx_vita
  network.cpp
  keyboard.cpp
  touch.cpp
)

if(NOT NONET)
  if(NOT DISABLE_TCP)
    target_link_libraries(libdevilutionx_vita PUBLIC asio)
  endif()
  if(PACKET_ENCRYPTION)
    target_sources(libdevilutionx_vita PRIVATE random.cpp)
    target_link_libraries(libdevilutionx_vita PUBLIC sodium)
  endif()
endif()

target_link_libraries(libdevilutionx_vita PUBLIC
  DevilutionX::SDL
  ScePower_stub
  SceAppUtil_stub
  SceNet_stub
  SceNetCtl_stub
  libdevilutionx_options
  libdevilutionx_text_input
)
