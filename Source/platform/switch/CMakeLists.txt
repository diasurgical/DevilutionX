include(functions/devilutionx_library)

add_devilutionx_object_library(libdevilutionx_switch
  romfs.cpp
  network.cpp
  keyboard.cpp
  docking.cpp
  asio/pause.c
  asio/net/if.c
  asio/sys/signal.c
)

if(NOT NONET)
  if(NOT DISABLE_TCP)
    target_link_libraries(libdevilutionx_switch PUBLIC asio)
  endif()
  if(PACKET_ENCRYPTION)
    target_sources(libdevilutionx_switch PRIVATE random.cpp)
    target_link_libraries(libdevilutionx_switch PUBLIC sodium)
  endif()
endif()

target_link_libraries(libdevilutionx_switch PUBLIC
  DevilutionX::SDL
  libdevilutionx_log
)
