include(Definitions)
include(functions/devilutionx_library)
include(functions/genex)

set(libdevilutionx_SRCS
  appfat.cpp
  automap.cpp
  capture.cpp
  codec.cpp
  control.cpp
  cursor.cpp
  dead.cpp
  debug.cpp
  diablo.cpp
  doom.cpp
  engine.cpp
  error.cpp
  gamemenu.cpp
  gmenu.cpp
  help.cpp
  hwcursor.cpp
  init.cpp
  interfac.cpp
  inv.cpp
  itemdat.cpp
  items.cpp
  lighting.cpp
  loadsave.cpp
  menu.cpp
  minitext.cpp
  misdat.cpp
  missiles.cpp
  monstdat.cpp
  monster.cpp
  movie.cpp
  msg.cpp
  multi.cpp
  nthread.cpp
  objdat.cpp
  objects.cpp
  options.cpp
  pack.cpp
  pfile.cpp
  player.cpp
  playerdat.cpp
  plrmsg.cpp
  portal.cpp
  quests.cpp
  restrict.cpp
  sha.cpp
  spelldat.cpp
  spells.cpp
  stores.cpp
  sync.cpp
  textdat.cpp
  tmsg.cpp
  towners.cpp
  track.cpp

  controls/axis_direction.cpp
  controls/controller.cpp
  controls/controller_buttons.cpp
  controls/controller_motion.cpp
  controls/devices/joystick.cpp
  controls/devices/kbcontroller.cpp
  controls/game_controls.cpp
  controls/menu_controls.cpp
  controls/modifier_hints.cpp
  controls/plrctrls.cpp

  DiabloUI/button.cpp
  DiabloUI/credits.cpp
  DiabloUI/credits_lines.cpp
  DiabloUI/diabloui.cpp
  DiabloUI/dialogs.cpp
  DiabloUI/hero/selhero.cpp
  DiabloUI/mainmenu.cpp
  DiabloUI/multi/selconn.cpp
  DiabloUI/multi/selgame.cpp
  DiabloUI/progress.cpp
  DiabloUI/scrollbar.cpp
  DiabloUI/selok.cpp
  DiabloUI/selstart.cpp
  DiabloUI/selyesno.cpp
  DiabloUI/settingsmenu.cpp
  DiabloUI/support_lines.cpp
  DiabloUI/text_input.cpp
  DiabloUI/title.cpp

  dvlnet/abstract_net.cpp
  dvlnet/base.cpp
  dvlnet/cdwrap.cpp
  dvlnet/frame_queue.cpp
  dvlnet/loopback.cpp
  dvlnet/packet.cpp

  engine/actor_position.cpp
  engine/animationinfo.cpp
  engine/assets.cpp
  engine/backbuffer_state.cpp
  engine/direction.cpp
  engine/dx.cpp
  engine/events.cpp
  engine/load_cel.cpp
  engine/load_cl2.cpp
  engine/load_clx.cpp
  engine/load_pcx.cpp
  engine/palette.cpp
  engine/path.cpp
  engine/random.cpp
  engine/sound_position.cpp
  engine/surface.cpp
  engine/trn.cpp

  engine/render/automap_render.cpp
  engine/render/clx_render.cpp
  engine/render/dun_render.cpp
  engine/render/primitive_render.cpp
  engine/render/scrollrt.cpp
  engine/render/text_render.cpp

  items/validation.cpp

  levels/crypt.cpp
  levels/drlg_l1.cpp
  levels/drlg_l2.cpp
  levels/drlg_l3.cpp
  levels/drlg_l4.cpp
  levels/gendung.cpp
  levels/setmaps.cpp
  levels/themes.cpp
  levels/town.cpp
  levels/trigs.cpp

  panels/charpanel.cpp
  panels/info_box.cpp
  panels/mainpanel.cpp
  panels/spell_book.cpp
  panels/spell_icons.cpp
  panels/spell_list.cpp

  platform/locale.cpp

  qol/autopickup.cpp
  qol/chatlog.cpp
  qol/floatingnumbers.cpp
  qol/itemlabels.cpp
  qol/monhealthbar.cpp
  qol/stash.cpp
  qol/xpbar.cpp

  storm/storm_net.cpp
  storm/storm_svid.cpp

  utils/cel_to_clx.cpp
  utils/cl2_to_clx.cpp
  utils/console.cpp
  utils/display.cpp
  utils/file_util.cpp
  utils/format_int.cpp
  utils/language.cpp
  utils/logged_fstream.cpp
  utils/parse_int.cpp
  utils/paths.cpp
  utils/pcx_to_clx.cpp
  utils/sdl_bilinear_scale.cpp
  utils/sdl_thread.cpp
  utils/str_cat.cpp
  utils/str_case.cpp
  utils/surface_to_clx.cpp
  utils/utf8.cpp)

if(SUPPORTS_MPQ)
  list(APPEND libdevilutionx_DEPS libmpq)
  list(APPEND libdevilutionx_SRCS
    mpq/mpq_reader.cpp
    mpq/mpq_sdl_rwops.cpp
    mpq/mpq_writer.cpp)
endif()

if(SUPPORTS_MPQ OR NOT NONET)
  list(APPEND libdevilutionx_DEPS PKWare)
  list(APPEND libdevilutionx_SRCS encrypt.cpp)
endif()

if(IOS)
  list(APPEND libdevilutionx_SRCS platform/ios/ios_paths.m)
endif()

if(USE_SDL1)
  list(APPEND libdevilutionx_SRCS utils/sdl2_to_1_2_backports.cpp)
  if(APPLE)
    enable_language(OBJC)
    list(APPEND libdevilutionx_SRCS platform/macos_sdl1/SDL_filesystem.m)
  endif()
endif()

if(NOT DISABLE_DEMOMODE)
  list(APPEND libdevilutionx_SRCS engine/demomode.cpp)
endif()

if(NOSOUND)
  list(APPEND libdevilutionx_SRCS
    effects_stubs.cpp
    engine/sound_stubs.cpp)
else()
  list(APPEND libdevilutionx_SRCS
    effects.cpp
    engine/sound.cpp
    utils/push_aulib_decoder.cpp
    utils/soundsample.cpp)
endif()

if(NOT NONET)
  if(NOT DISABLE_TCP)
    list(APPEND libdevilutionx_SRCS
      dvlnet/tcp_client.cpp
      dvlnet/tcp_server.cpp)
  endif()
  if(NOT DISABLE_ZERO_TIER)
    list(APPEND libdevilutionx_SRCS
      dvlnet/protocol_zt.cpp
      dvlnet/zerotier_native.cpp
      dvlnet/zerotier_lwip.cpp)
  endif()
endif()

if(NOT USE_SDL1)
  list(APPEND libdevilutionx_SRCS
    controls/devices/game_controller.cpp
    controls/touch/event_handlers.cpp
    controls/touch/gamepad.cpp
    controls/touch/renderers.cpp)
endif()

if(DISCORD_INTEGRATION)
  list(APPEND libdevilutionx_SRCS
    discord/discord.cpp
  )
endif()

add_devilutionx_library(libdevilutionx OBJECT ${libdevilutionx_SRCS})
target_include_directories(libdevilutionx PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Use file GENERATE instead of configure_file because configure_file
# does not support generator expressions.
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(is_multi_config)
  set(CONFIG_PATH $<CONFIG>/config.h)
  target_include_directories(libdevilutionx PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
  set(CONFIG_PATH config.h)
endif()
file(GENERATE OUTPUT ${CONFIG_PATH} CONTENT
"#pragma once
#define PROJECT_NAME \"${PROJECT_NAME}\"
#define PROJECT_VERSION \"${PROJECT_VERSION_WITH_SUFFIX}\"
#define PROJECT_VERSION_MAJOR ${PROJECT_VERSION_MAJOR}
#define PROJECT_VERSION_MINOR ${PROJECT_VERSION_MINOR}
#define PROJECT_VERSION_PATCH ${PROJECT_VERSION_PATCH}
")

if(DISCORD_INTEGRATION)
  target_compile_definitions(libdevilutionx PRIVATE DISCORD)
  target_link_libraries(libdevilutionx PRIVATE discord discord_game_sdk)
endif()

target_link_libraries(libdevilutionx PUBLIC
  Threads::Threads
  DevilutionX::SDL
  fmt::fmt
  libsmackerdec
  simpleini::simpleini
  tl
  hoehrmann_utf8
  ${libdevilutionx_DEPS}
)

if(NOT USE_SDL1)
  target_link_libraries(libdevilutionx PUBLIC SDL2::SDL2_image)
endif()

if(NOT NONET)
  if(NOT DISABLE_TCP)
    target_link_libraries(libdevilutionx PUBLIC asio)
  endif()
  if(PACKET_ENCRYPTION)
    target_link_libraries(libdevilutionx PUBLIC sodium)
  endif()
endif()

if(NOT NOSOUND)
  target_link_libraries(libdevilutionx PUBLIC SDL_audiolib::SDL_audiolib)
endif()

if(NOT NONET AND NOT DISABLE_ZERO_TIER)
  if(NOT ANDROID)
    target_link_libraries(libdevilutionx PUBLIC zt-static)
  else()
    target_link_libraries(libdevilutionx PUBLIC zt-shared)
  endif()
endif()

foreach(path ${DEVILUTIONX_PLATFORM_SUBDIRECTORIES})
  add_subdirectory(${path})
endforeach()

target_link_libraries(libdevilutionx PUBLIC ${DEVILUTIONX_PLATFORM_LINK_LIBRARIES})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9 AND NOT AMIGA)
    target_link_libraries(libdevilutionx PUBLIC stdc++fs)
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Assumes libc++ (clang) is used rather than libstdc++ (gcc).
  # This is not always true but these are ancient clang versions anyway.
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7)
    target_link_libraries(libdevilutionx PUBLIC c++experimental)
  elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(libdevilutionx PUBLIC c++fs)
  endif()
endif()
