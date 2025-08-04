include(Definitions)
include(functions/devilutionx_library)
include(functions/genex)

set(libdevilutionx_SRCS
  appfat.cpp
  automap.cpp
  capture.cpp
  control.cpp
  cursor.cpp
  dead.cpp
  debug.cpp
  diablo.cpp
  diablo_msg.cpp
  doom.cpp
  gamemenu.cpp
  gmenu.cpp
  help.cpp
  hwcursor.cpp
  interfac.cpp
  inv.cpp
  loadsave.cpp
  menu.cpp
  minitext.cpp
  misdat.cpp
  missiles.cpp
  movie.cpp
  msg.cpp
  nthread.cpp
  pfile.cpp
  plrmsg.cpp
  portal.cpp
  restrict.cpp
  sync.cpp
  textdat.cpp
  tmsg.cpp
  towners.cpp
  track.cpp

  controls/axis_direction.cpp
  controls/controller_motion.cpp
  controls/controller.cpp
  controls/devices/joystick.cpp
  controls/devices/kbcontroller.cpp
  controls/game_controls.cpp
  controls/keymapper.cpp
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
  DiabloUI/title.cpp
  DiabloUI/text_input.cpp

  dvlnet/abstract_net.cpp
  dvlnet/base.cpp
  dvlnet/cdwrap.cpp
  dvlnet/frame_queue.cpp
  dvlnet/loopback.cpp
  dvlnet/packet.cpp

  engine/actor_position.cpp
  engine/animationinfo.cpp
  engine/backbuffer_state.cpp
  engine/dx.cpp
  engine/events.cpp
  engine/palette.cpp
  engine/sound_position.cpp
  engine/trn.cpp

  engine/render/automap_render.cpp
  engine/render/scrollrt.cpp

  items/validation.cpp

  levels/reencode_dun_cels.cpp
  levels/setmaps.cpp
  levels/themes.cpp
  levels/tile_properties.cpp
  levels/town.cpp
  levels/trigs.cpp

  lua/autocomplete.cpp
  lua/lua_global.cpp
  lua/modules/audio.cpp
  lua/modules/hellfire.cpp
  lua/modules/dev.cpp
  lua/modules/dev/display.cpp
  lua/modules/dev/items.cpp
  lua/modules/dev/level.cpp
  lua/modules/dev/level/map.cpp
  lua/modules/dev/level/warp.cpp
  lua/modules/dev/monsters.cpp
  lua/modules/dev/player.cpp
  lua/modules/dev/player/gold.cpp
  lua/modules/dev/player/spells.cpp
  lua/modules/dev/player/stats.cpp
  lua/modules/dev/quests.cpp
  lua/modules/dev/search.cpp
  lua/modules/dev/towners.cpp
  lua/modules/i18n.cpp
  lua/modules/items.cpp
  lua/modules/log.cpp
  lua/modules/player.cpp
  lua/modules/render.cpp
  lua/modules/towners.cpp
  lua/repl.cpp

  monsters/validation.cpp

  panels/charpanel.cpp
  panels/console.cpp
  panels/info_box.cpp
  panels/mainpanel.cpp
  panels/spell_book.cpp
  panels/spell_icons.cpp
  panels/spell_list.cpp

  platform/locale.cpp

  portals/validation.cpp

  qol/autopickup.cpp
  qol/chatlog.cpp
  qol/floatingnumbers.cpp
  qol/itemlabels.cpp
  qol/monhealthbar.cpp
  qol/stash.cpp
  qol/xpbar.cpp

  quests/validation.cpp

  storm/storm_net.cpp
  storm/storm_svid.cpp
  utils/display.cpp
  utils/language.cpp
  utils/sdl_bilinear_scale.cpp
  utils/sdl_thread.cpp
  utils/surface_to_clx.cpp
  utils/timer.cpp)

# These files are responsible for most of the runtime in Debug mode.
# Apply some optimizations to them even in Debug mode to get reasonable performance.
#
# They also perform better with -O2 rather than -O3 even in Release mode.
set(_optimize_in_debug_srcs
  engine/render/clx_render.cpp
  engine/render/dun_render.cpp
  engine/render/text_render.cpp
  utils/cel_to_clx.cpp
  utils/cl2_to_clx.cpp
  utils/pcx_to_clx.cpp)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(${_optimize_in_debug_srcs} PROPERTIES COMPILE_OPTIONS "-O2;--param=max-vartrack-size=900000000")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set_source_files_properties(${_optimize_in_debug_srcs} PROPERTIES COMPILE_OPTIONS "-O2")
endif()

# We need to define all the object libraries first
# because our workaround for transitive dependency support
# requires targets to exist when calling `target_link_dependencies`
# (see object_libraries.cmake).

add_devilutionx_object_library(libdevilutionx_assets
  engine/assets.cpp
)
target_link_dependencies(libdevilutionx_assets PUBLIC
  DevilutionX::SDL
  fmt::fmt
  tl
  libdevilutionx_headless_mode
  libdevilutionx_game_mode
  libdevilutionx_mpq
  libdevilutionx_paths
  libdevilutionx_sdl2_to_1_2_backports
  libdevilutionx_strings
  ${DEVILUTIONX_PLATFORM_ASSETS_LINK_LIBRARIES}
)

add_devilutionx_object_library(libdevilutionx_cel_to_clx
  utils/cel_to_clx.cpp
)
target_link_dependencies(libdevilutionx_cel_to_clx
  PRIVATE
  libdevilutionx_endian_write
)

add_devilutionx_object_library(libdevilutionx_cl2_to_clx
  utils/cl2_to_clx.cpp
)
target_link_dependencies(libdevilutionx_cl2_to_clx
  PRIVATE
  libdevilutionx_endian_write
)

add_devilutionx_object_library(libdevilutionx_clx_render
  engine/render/clx_render.cpp
)
target_link_dependencies(libdevilutionx_clx_render PUBLIC
  DevilutionX::SDL
  fmt::fmt
  libdevilutionx_light_render
  libdevilutionx_palette_blending
  libdevilutionx_strings
)

add_devilutionx_object_library(libdevilutionx_codec
  codec.cpp
  sha.cpp
)
target_link_dependencies(libdevilutionx_codec PRIVATE
  DevilutionX::SDL
  libdevilutionx_log
)

add_devilutionx_object_library(libdevilutionx_controller_buttons
  controls/controller_buttons.cpp
)
target_link_dependencies(libdevilutionx_controller_buttons
  DevilutionX::SDL
)

add_devilutionx_object_library(libdevilutionx_control_mode
  controls/control_mode.cpp
)
target_link_dependencies(libdevilutionx_control_mode PUBLIC
  libdevilutionx_controller_buttons
)

add_devilutionx_object_library(libdevilutionx_padmapper
  controls/padmapper.cpp
)
target_link_dependencies(libdevilutionx_padmapper PUBLIC
  libdevilutionx_controller_buttons
  libdevilutionx_options
)

add_devilutionx_object_library(libdevilutionx_paths
  utils/paths.cpp
)
target_link_dependencies(libdevilutionx_paths PUBLIC
  DevilutionX::SDL
  libdevilutionx_file_util
  libdevilutionx_log
  libdevilutionx_sdl2_to_1_2_backports
)

add_devilutionx_object_library(libdevilutionx_pcx_to_clx
  utils/pcx_to_clx.cpp
)
target_link_dependencies(libdevilutionx_pcx_to_clx
  PUBLIC
  DevilutionX::SDL
  libdevilutionx_assets
  libdevilutionx_endian_write
)

add_devilutionx_object_library(libdevilutionx_primitive_render
  engine/render/primitive_render.cpp
)
target_link_dependencies(libdevilutionx_primitive_render
  PUBLIC
  libdevilutionx_palette_blending
  libdevilutionx_surface
)

add_devilutionx_object_library(libdevilutionx_crawl
  crawl.cpp
)
target_link_dependencies(libdevilutionx_crawl PUBLIC
  tl
)

add_devilutionx_object_library(libdevilutionx_direction
  engine/direction.cpp
)

add_devilutionx_object_library(libdevilutionx_dun_render
  engine/render/dun_render.cpp
)
target_link_libraries(libdevilutionx_dun_render
  PUBLIC
  DevilutionX::SDL
  libdevilutionx_light_render
  libdevilutionx_surface
  PRIVATE
  libdevilutionx_options
)

add_library(libdevilutionx_endian_write INTERFACE)
target_link_libraries(libdevilutionx_endian_write INTERFACE
  DevilutionX::SDL
)

add_devilutionx_object_library(libdevilutionx_surface
  engine/surface.cpp
)
target_link_dependencies(libdevilutionx_surface PUBLIC
  DevilutionX::SDL
)

add_devilutionx_object_library(libdevilutionx_file_util
  utils/file_util.cpp
)
target_link_dependencies(libdevilutionx_file_util PRIVATE
  DevilutionX::SDL
  libdevilutionx_log
  ${DEVILUTIONX_PLATFORM_FILE_UTIL_LINK_LIBRARIES}
)

add_devilutionx_object_library(libdevilutionx_format_int
  utils/format_int.cpp
)
target_link_dependencies(libdevilutionx_format_int PUBLIC
  libdevilutionx_strings
)

add_devilutionx_object_library(libdevilutionx_game_mode
  game_mode.cpp
)
target_link_dependencies(libdevilutionx_game_mode PRIVATE
  tl
  libdevilutionx_options
)

add_devilutionx_object_library(libdevilutionx_gendung
  levels/crypt.cpp
  levels/drlg_l1.cpp
  levels/drlg_l2.cpp
  levels/drlg_l3.cpp
  levels/drlg_l4.cpp
  levels/gendung.cpp
)
target_link_dependencies(libdevilutionx_gendung PUBLIC
  DevilutionX::SDL
  fmt::fmt
  tl
  libdevilutionx_assets
  libdevilutionx_items
  libdevilutionx_monster
  libdevilutionx_random
)

add_devilutionx_object_library(libdevilutionx_headless_mode
  headless_mode.cpp
)

add_devilutionx_object_library(libdevilutionx_init
  init.cpp
)
target_link_dependencies(libdevilutionx_init PUBLIC
  libdevilutionx_assets
  libdevilutionx_config
  libdevilutionx_mpq
  libdevilutionx_options
)

add_devilutionx_object_library(libdevilutionx_load_cel
  engine/load_cel.cpp
)
target_link_dependencies(libdevilutionx_load_cel
  PUBLIC
  tl
  PRIVATE
  libdevilutionx_strings
)
if(SUPPORTS_MPQ)
  target_link_dependencies(libdevilutionx_load_cel PRIVATE
    libdevilutionx_mpq
    libdevilutionx_cel_to_clx
  )
else()
  target_link_dependencies(libdevilutionx_load_cel PRIVATE
    libdevilutionx_load_clx
  )
endif()

add_devilutionx_object_library(libdevilutionx_load_cl2
  engine/load_cl2.cpp
)
target_link_dependencies(libdevilutionx_load_cl2
  PUBLIC
  tl
  libdevilutionx_endian_write
  PRIVATE
  libdevilutionx_strings
)
if(SUPPORTS_MPQ)
  target_link_dependencies(libdevilutionx_load_cl2 PUBLIC
    libdevilutionx_mpq
    libdevilutionx_cl2_to_clx
  )
else()
  target_link_dependencies(libdevilutionx_load_cl2 PRIVATE
    libdevilutionx_load_clx
  )
endif()

add_devilutionx_object_library(libdevilutionx_load_clx
  engine/load_clx.cpp
)
target_link_dependencies(libdevilutionx_load_clx
  PUBLIC
  tl
  PRIVATE
  libdevilutionx_assets
)

add_devilutionx_object_library(libdevilutionx_load_pcx
  engine/load_pcx.cpp
)
target_link_dependencies(libdevilutionx_load_pcx
  PRIVATE
  DevilutionX::SDL
  libdevilutionx_sdl2_to_1_2_backports
  libdevilutionx_log
  libdevilutionx_strings
)
if(SUPPORTS_MPQ)
  target_link_dependencies(libdevilutionx_load_pcx PUBLIC
    libdevilutionx_assets
    libdevilutionx_pcx_to_clx
  )
else()
  target_link_dependencies(libdevilutionx_load_pcx PRIVATE
    libdevilutionx_load_clx
  )
endif()

add_devilutionx_object_library(libdevilutionx_light_render
  engine/render/light_render.cpp
)

add_devilutionx_object_library(libdevilutionx_lighting
  lighting.cpp
)
target_link_dependencies(libdevilutionx_lighting PUBLIC
  DevilutionX::SDL
  fmt::fmt
  tl
  libdevilutionx_vision
)

add_devilutionx_object_library(libdevilutionx_logged_fstream
  utils/logged_fstream.cpp
)
target_link_dependencies(libdevilutionx_logged_fstream PUBLIC
  libdevilutionx_file_util
  libdevilutionx_log
)

add_devilutionx_object_library(libdevilutionx_items
  itemdat.cpp
  items.cpp
)
target_link_dependencies(libdevilutionx_items PUBLIC
  DevilutionX::SDL
  tl
  libdevilutionx_headless_mode
  libdevilutionx_sound
  libdevilutionx_spells
  libdevilutionx_stores
  libdevilutionx_strings
)

add_devilutionx_object_library(libdevilutionx_ini
  utils/ini.cpp
)
target_link_dependencies(libdevilutionx_ini PUBLIC
  fmt::fmt
  tl
  unordered_dense::unordered_dense
  libdevilutionx_strings
  libdevilutionx_utf8
)

# We use an INTERFACE library rather than an OBJECT library
# because `libdevilutionx_log` does not have any sources.
add_library(libdevilutionx_log INTERFACE)
target_include_directories(libdevilutionx_log INTERFACE
  ${PROJECT_SOURCE_DIR}/Source)
target_link_libraries(libdevilutionx_log INTERFACE
  DevilutionX::SDL
  fmt::fmt
)
target_sources(libdevilutionx_log INTERFACE $<TARGET_OBJECTS:libdevilutionx_strings>)

add_devilutionx_object_library(libdevilutionx_level_objects
  objdat.cpp
  objects.cpp
)
target_link_dependencies(libdevilutionx_level_objects PUBLIC
  DevilutionX::SDL
  unordered_dense::unordered_dense
  tl
  libdevilutionx_direction
  libdevilutionx_headless_mode
  libdevilutionx_monster
  libdevilutionx_options
  libdevilutionx_player
  libdevilutionx_random
  libdevilutionx_txtdata
)

add_devilutionx_object_library(libdevilutionx_monster
  monstdat.cpp
  monster.cpp
)
target_link_dependencies(libdevilutionx_monster
  PUBLIC
  DevilutionX::SDL
  tl
  libdevilutionx_game_mode
  libdevilutionx_headless_mode
  libdevilutionx_sound
  libdevilutionx_txtdata
  PRIVATE
  libdevilutionx_cl2_to_clx
)

add_devilutionx_object_library(libdevilutionx_palette_blending
  utils/palette_blending.cpp
)
target_link_dependencies(libdevilutionx_palette_blending PUBLIC
  DevilutionX::SDL
  fmt::fmt
  libdevilutionx_strings
)

add_devilutionx_object_library(libdevilutionx_parse_int
  utils/parse_int.cpp
)
target_link_dependencies(libdevilutionx_parse_int PUBLIC
  tl
)

if(SUPPORTS_MPQ)
  add_devilutionx_object_library(libdevilutionx_mpq
    mpq/mpq_common.cpp
    mpq/mpq_reader.cpp
    mpq/mpq_sdl_rwops.cpp
    mpq/mpq_writer.cpp
  )
  target_link_dependencies(libdevilutionx_mpq PUBLIC
    DevilutionX::SDL
    fmt::fmt
    tl
    libmpq
    libdevilutionx_file_util
    libdevilutionx_logged_fstream
    libdevilutionx_pkware_encrypt
    libdevilutionx_strings
  )
else()
  add_library(libdevilutionx_mpq INTERFACE)
endif()

add_devilutionx_object_library(libdevilutionx_multiplayer
  multi.cpp
  pack.cpp
)
target_link_dependencies(libdevilutionx_multiplayer PUBLIC
  libdevilutionx_config
  libdevilutionx_items
)

add_devilutionx_object_library(libdevilutionx_options
  options.cpp
)
target_link_dependencies(libdevilutionx_options PUBLIC
  DevilutionX::SDL
  fmt::fmt
  tl
  unordered_dense::unordered_dense
  libdevilutionx_controller_buttons
  libdevilutionx_control_mode
  libdevilutionx_logged_fstream
  libdevilutionx_quick_messages
  libdevilutionx_strings
  libdevilutionx_ini
)

add_devilutionx_object_library(libdevilutionx_pathfinding
  engine/path.cpp
)
target_link_dependencies(libdevilutionx_pathfinding PUBLIC
  tl
  libdevilutionx_crawl
  libdevilutionx_direction
)

if(SUPPORTS_MPQ OR NOT NONET)
  add_devilutionx_object_library(libdevilutionx_pkware_encrypt
    encrypt.cpp
  )
  target_link_dependencies(libdevilutionx_pkware_encrypt PUBLIC
    DevilutionX::SDL
    PKWare
  )
else()
  add_library(libdevilutionx_pkware_encrypt INTERFACE)
endif()

add_devilutionx_object_library(libdevilutionx_player
  player.cpp
  playerdat.cpp
)
target_link_dependencies(libdevilutionx_player
  PUBLIC
  DevilutionX::SDL
  fmt::fmt
  tl
  unordered_dense::unordered_dense
  libdevilutionx_game_mode
  PRIVATE
  libdevilutionx_load_cl2
  libdevilutionx_strings
)

add_devilutionx_object_library(libdevilutionx_quests
  quests.cpp
)
target_link_dependencies(libdevilutionx_quests PUBLIC
  libdevilutionx_surface
  libdevilutionx_gendung
)

add_devilutionx_object_library(libdevilutionx_random
  engine/random.cpp
)

add_devilutionx_object_library(libdevilutionx_quick_messages
  quick_messages.cpp
)

add_devilutionx_object_library(libdevilutionx_spells
  spelldat.cpp
  spells.cpp
)
target_link_dependencies(libdevilutionx_spells PUBLIC
  tl
  libdevilutionx_player
  libdevilutionx_txtdata
)

add_devilutionx_object_library(libdevilutionx_text_render
  engine/render/text_render.cpp
)
target_link_dependencies(libdevilutionx_text_render
  PUBLIC
  libdevilutionx_surface
  PRIVATE
  fmt::fmt
  unordered_dense::unordered_dense
  libdevilutionx_clx_render
  libdevilutionx_game_mode
  libdevilutionx_load_cel
  libdevilutionx_load_clx
  libdevilutionx_load_pcx
  libdevilutionx_log
  libdevilutionx_primitive_render
  libdevilutionx_ticks
  libdevilutionx_utf8
)

add_devilutionx_object_library(libdevilutionx_ticks
  engine/ticks.cpp
)
target_link_dependencies(libdevilutionx_ticks PRIVATE
  DevilutionX::SDL
)

add_devilutionx_object_library(libdevilutionx_txtdata
  data/file.cpp
  data/parser.cpp
  data/record_reader.cpp
)
target_link_dependencies(libdevilutionx_txtdata PUBLIC
  fmt::fmt
  tl
  libdevilutionx_assets
  libdevilutionx_parse_int
  libdevilutionx_strings
)

add_devilutionx_object_library(libdevilutionx_utf8
  utils/utf8.cpp
)
target_link_dependencies(libdevilutionx_utf8 PRIVATE
  SheenBidi::SheenBidi
)

if(NOSOUND)
  add_devilutionx_object_library(libdevilutionx_sound
    effects_stubs.cpp
    engine/sound_stubs.cpp
  )
else()
  add_devilutionx_object_library(libdevilutionx_sound
    effects.cpp
    engine/sound.cpp
    utils/push_aulib_decoder.cpp
    utils/soundsample.cpp
  )
  target_link_dependencies(libdevilutionx_sound PUBLIC
    DevilutionX::SDL
    SDL_audiolib::SDL_audiolib
    fmt::fmt
    tl
    unordered_dense::unordered_dense
    libdevilutionx_options
    libdevilutionx_random
    libdevilutionx_sdl2_to_1_2_backports
  )
endif()

add_devilutionx_object_library(libdevilutionx_stores
  stores.cpp
)
target_link_dependencies(libdevilutionx_stores PUBLIC
  DevilutionX::SDL
  fmt::fmt
  tl
  libdevilutionx_clx_render
  libdevilutionx_options
  libdevilutionx_sound
  libdevilutionx_strings
)

add_devilutionx_object_library(libdevilutionx_strings
  utils/str_cat.cpp
  utils/str_case.cpp
)
target_link_dependencies(libdevilutionx_strings PRIVATE
  fmt::fmt)

add_devilutionx_object_library(libdevilutionx_utils_console
  utils/console.cpp
)

add_devilutionx_object_library(libdevilutionx_vision
  vision.cpp
)
target_link_dependencies(libdevilutionx_vision PUBLIC
  tl
)

if(USE_SDL1)
  add_devilutionx_library(libdevilutionx_sdl2_to_1_2_backports STATIC
    utils/sdl2_to_1_2_backports.cpp
  )
  target_link_dependencies(libdevilutionx_sdl2_to_1_2_backports PRIVATE
    libdevilutionx_utils_console
    fmt::fmt
  )
  target_link_libraries(DevilutionX::SDL INTERFACE
    libdevilutionx_sdl2_to_1_2_backports
  )
  if(APPLE)
    enable_language(OBJC)
    target_sources(libdevilutionx_sdl2_to_1_2_backports PRIVATE
      platform/macos_sdl1/SDL_filesystem.m)
    target_link_libraries(libdevilutionx_sdl2_to_1_2_backports PRIVATE
      "-framework Foundation")
  endif()
else()
  add_library(libdevilutionx_sdl2_to_1_2_backports INTERFACE)
endif()

if(IOS)
  list(APPEND libdevilutionx_SRCS platform/ios/ios_paths.m)
endif()

if(NOT DISABLE_DEMOMODE)
  list(APPEND libdevilutionx_SRCS engine/demomode.cpp)
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

if(SCREEN_READER_INTEGRATION)
  list(APPEND libdevilutionx_SRCS
    utils/screen_reader.cpp
  )
endif()

if(DEVILUTIONX_SCREENSHOT_FORMAT STREQUAL DEVILUTIONX_SCREENSHOT_FORMAT_PCX)
  list(APPEND libdevilutionx_SRCS
    utils/surface_to_pcx.cpp
  )
endif()

if(DEVILUTIONX_SCREENSHOT_FORMAT STREQUAL DEVILUTIONX_SCREENSHOT_FORMAT_PNG)
  add_devilutionx_object_library(libdevilutionx_surface_to_png
    utils/surface_to_png.cpp
  )
  target_link_dependencies(libdevilutionx_surface_to_png
    PUBLIC
    DevilutionX::SDL
    SDL2::SDL2_image
    tl
    libdevilutionx_surface
  )
endif()

add_devilutionx_object_library(libdevilutionx ${libdevilutionx_SRCS})
target_include_directories(libdevilutionx PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_dependencies(libdevilutionx PUBLIC
  Threads::Threads
  DevilutionX::SDL
  fmt::fmt
  libsmackerdec
  ${LUA_LIBRARIES}
  sol2::sol2
  tl
  unordered_dense::unordered_dense
  libdevilutionx_assets
  libdevilutionx_clx_render
  libdevilutionx_codec
  libdevilutionx_config
  libdevilutionx_controller_buttons
  libdevilutionx_control_mode
  libdevilutionx_crawl
  libdevilutionx_direction
  libdevilutionx_dun_render
  libdevilutionx_surface
  libdevilutionx_file_util
  libdevilutionx_format_int
  libdevilutionx_game_mode
  libdevilutionx_gendung
  libdevilutionx_headless_mode
  libdevilutionx_ini
  libdevilutionx_init
  libdevilutionx_items
  libdevilutionx_level_objects
  libdevilutionx_light_render
  libdevilutionx_lighting
  libdevilutionx_monster
  libdevilutionx_mpq
  libdevilutionx_multiplayer
  libdevilutionx_options
  libdevilutionx_padmapper
  libdevilutionx_palette_blending
  libdevilutionx_parse_int
  libdevilutionx_pathfinding
  libdevilutionx_pkware_encrypt
  libdevilutionx_player
  libdevilutionx_primitive_render
  libdevilutionx_quests
  libdevilutionx_quick_messages
  libdevilutionx_random
  libdevilutionx_sound
  libdevilutionx_spells
  libdevilutionx_stores
  libdevilutionx_strings
  libdevilutionx_text_render
  libdevilutionx_txtdata
  libdevilutionx_ticks
  libdevilutionx_utf8
  libdevilutionx_utils_console
)
if(DEVILUTIONX_SCREENSHOT_FORMAT STREQUAL DEVILUTIONX_SCREENSHOT_FORMAT_PNG)
  target_link_dependencies(libdevilutionx PUBLIC libdevilutionx_surface_to_png)
endif()

# Use file GENERATE instead of configure_file because configure_file
# does not support generator expressions.
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
add_library(libdevilutionx_config INTERFACE)
if(is_multi_config)
  set(CONFIG_PATH $<CONFIG>/config.h)
  target_include_directories(libdevilutionx_config INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
  set(CONFIG_PATH config.h)
  target_include_directories(libdevilutionx_config INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
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

if(SCREEN_READER_INTEGRATION)
  if(WIN32)
    target_compile_definitions(libdevilutionx PRIVATE Tolk)
    target_link_libraries(libdevilutionx PUBLIC Tolk)
  else()
    target_include_directories(libdevilutionx PUBLIC ${Speechd_INCLUDE_DIRS})
    target_link_libraries(libdevilutionx PUBLIC speechd)
  endif()
endif()

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

target_link_dependencies(libdevilutionx PUBLIC ${DEVILUTIONX_PLATFORM_LINK_LIBRARIES})

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
