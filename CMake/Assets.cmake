include(functions/copy_files)
include(functions/trim_retired_files)

if(NOT DEFINED DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY)
  set(DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets")
endif()

set(devilutionx_langs bg cs da de el es et fi fr hr hu it ja ko pl pt_BR ro ru uk sv tr zh_CN zh_TW)
if(USE_GETTEXT_FROM_VCPKG)
  # vcpkg doesn't add its own tools directory to the search path
  list(APPEND Gettext_ROOT ${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/gettext/bin)
endif()
find_package(Gettext)
if (Gettext_FOUND)
  file(MAKE_DIRECTORY "${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}")
  foreach(lang ${devilutionx_langs})
    set(_po_file "${CMAKE_CURRENT_SOURCE_DIR}/Translations/${lang}.po")
    set(_gmo_file "${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}/${lang}.gmo")
    set(_lang_target devilutionx_lang_${lang})
    add_custom_command(
      COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" -o "${_gmo_file}" "${_po_file}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT "${_gmo_file}"
      MAIN_DEPENDENCY "${_po_file}"
      VERBATIM
    )
    add_custom_target("${_lang_target}" DEPENDS "${_gmo_file}")
    list(APPEND devilutionx_lang_targets "${_lang_target}")
    list(APPEND devilutionx_lang_files "${_gmo_file}")

    if(APPLE)
      set_source_files_properties("${_gmo_file}" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
        XCODE_EXPLICIT_FILE_TYPE compiled)
      add_dependencies(libdevilutionx "${_lang_target}")
      add_dependencies(${BIN_TARGET} "${_lang_target}")
      target_sources(${BIN_TARGET} PRIVATE "${_gmo_file}")
    endif()

    if(VITA)
      list(APPEND VITA_TRANSLATIONS_LIST "FILE" "${_gmo_file}" "assets/${lang}.gmo")
    endif()
  endforeach()
endif()

set(devilutionx_assets
  ASSETS_VERSION
  arena/church.dun
  arena/circle_of_death.dun
  arena/hell.dun
  data/boxleftend.clx
  data/boxmiddle.clx
  data/boxrightend.clx
  data/charbg.clx
  data/dirtybuc.clx
  data/dirtybucp.clx
  data/healthbox.clx
  data/health.clx
  data/hintbox.clx
  data/hintboxbackground.clx
  data/hinticons.clx
  data/monstertags.clx
  data/panel8buc.clx
  data/panel8bucp.clx
  data/resistance.clx
  data/stash.clx
  data/stashnavbtns.clx
  data/talkbutton.clx
  data/xpbar.clx
  fonts/12-00.clx
  fonts/12-01.clx
  fonts/12-02.clx
  fonts/12-03.clx
  fonts/12-04.clx
  fonts/12-1f4.clx
  fonts/12-1f6.clx
  fonts/12-1f9.clx
  fonts/12-20.clx
  fonts/12-26.clx
  fonts/12-e0.clx
  fonts/22-00.clx
  fonts/22-01.clx
  fonts/22-02.clx
  fonts/22-03.clx
  fonts/22-04.clx
  fonts/22-05.clx
  fonts/22-20.clx
  fonts/24-00.clx
  fonts/24-01.clx
  fonts/24-02.clx
  fonts/24-03.clx
  fonts/24-04.clx
  fonts/24-1f4.clx
  fonts/24-1f6.clx
  fonts/24-1f9.clx
  fonts/24-20.clx
  fonts/24-26.clx
  fonts/24-e0.clx
  fonts/30-00.clx
  fonts/30-01.clx
  fonts/30-02.clx
  fonts/30-03.clx
  fonts/30-04.clx
  fonts/30-20.clx
  fonts/30-e0.clx
  fonts/42-00.clx
  fonts/42-01.clx
  fonts/42-02.clx
  fonts/42-03.clx
  fonts/42-04.clx
  fonts/42-20.clx
  fonts/46-00.clx
  fonts/46-01.clx
  fonts/46-02.clx
  fonts/46-03.clx
  fonts/46-04.clx
  fonts/46-20.clx
  fonts/black.trn
  fonts/blue.trn
  fonts/buttonface.trn
  fonts/buttonpushed.trn
  fonts/gamedialogwhite.trn
  fonts/gamedialogyellow.trn
  fonts/gamedialogred.trn
  fonts/golduis.trn
  fonts/goldui.trn
  fonts/grayuis.trn
  fonts/grayui.trn
  fonts/orange.trn
  fonts/red.trn
  fonts/tr/12-00.clx
  fonts/tr/24-00.clx
  fonts/tr/30-00.clx
  fonts/tr/42-00.clx
  fonts/tr/46-00.clx
  fonts/whitegold.trn
  fonts/white.trn
  fonts/yellow.trn
  gendata/cut2w.clx
  gendata/cut3w.clx
  gendata/cut4w.clx
  gendata/cutgatew.clx
  gendata/cutl1dw.clx
  gendata/cutportlw.clx
  gendata/cutportrw.clx
  gendata/cutstartw.clx
  gendata/cutttw.clx
  gendata/pause.trn
  levels/l1data/sklkngt.dun
  levels/l2data/bonechat.dun
  levels/towndata/automap.dun
  levels/towndata/automap.amp
  lua_internal/get_lua_function_signature.lua
  lua/devilutionx/events.lua
  lua/inspect.lua
  lua/mods/clock/init.lua
  lua/repl_prelude.lua
  plrgfx/warrior/whu/whufm.trn
  plrgfx/warrior/whu/whulm.trn
  plrgfx/warrior/whu/whuqm.trn
  txtdata/Experience.tsv
  txtdata/classes/barbarian/animations.tsv
  txtdata/classes/barbarian/attributes.tsv
  txtdata/classes/barbarian/sounds.tsv
  txtdata/classes/barbarian/sprites.tsv
  txtdata/classes/barbarian/starting_loadout.tsv
  txtdata/classes/bard/animations.tsv
  txtdata/classes/bard/attributes.tsv
  txtdata/classes/bard/sounds.tsv
  txtdata/classes/bard/sprites.tsv
  txtdata/classes/bard/starting_loadout.tsv
  txtdata/classes/monk/animations.tsv
  txtdata/classes/monk/attributes.tsv
  txtdata/classes/monk/sounds.tsv
  txtdata/classes/monk/sprites.tsv
  txtdata/classes/monk/starting_loadout.tsv
  txtdata/classes/rogue/animations.tsv
  txtdata/classes/rogue/attributes.tsv
  txtdata/classes/rogue/sounds.tsv
  txtdata/classes/rogue/sprites.tsv
  txtdata/classes/rogue/starting_loadout.tsv
  txtdata/classes/sorcerer/animations.tsv
  txtdata/classes/sorcerer/attributes.tsv
  txtdata/classes/sorcerer/sounds.tsv
  txtdata/classes/sorcerer/sprites.tsv
  txtdata/classes/sorcerer/starting_loadout.tsv
  txtdata/classes/warrior/animations.tsv
  txtdata/classes/warrior/attributes.tsv
  txtdata/classes/warrior/sounds.tsv
  txtdata/classes/warrior/sprites.tsv
  txtdata/classes/warrior/starting_loadout.tsv
  txtdata/classes/classdat.tsv
  txtdata/items/item_prefixes.tsv
  txtdata/items/item_suffixes.tsv
  txtdata/items/itemdat.tsv
  txtdata/items/unique_itemdat.tsv
  txtdata/missiles/misdat.tsv
  txtdata/missiles/missile_sprites.tsv
  txtdata/monsters/monstdat.tsv
  txtdata/monsters/unique_monstdat.tsv
  txtdata/objects/objdat.tsv
  txtdata/quests/questdat.tsv
  txtdata/sound/effects.tsv
  txtdata/spells/spelldat.tsv
  txtdata/text/textdat.tsv
  ui_art/diablo.pal
  ui_art/creditsw.clx
  ui_art/dvl_but_sml.clx
  ui_art/dvl_lrpopup.clx
  ui_art/mainmenuw.clx)

if(NOT UNPACKED_MPQS)
  list(APPEND devilutionx_assets
    data/inv/objcurs-widths.txt)
endif()

if(NOT USE_SDL1 AND NOT VITA)
  list(APPEND devilutionx_assets
    ui_art/button.png
    ui_art/directions2.png
    ui_art/directions.png
    ui_art/menu-levelup.png
    ui_art/menu.png)
endif()

if(APPLE)
  foreach(asset_file ${devilutionx_assets})
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/assets/${asset_file}")
    get_filename_component(_asset_dir "${asset_file}" DIRECTORY)
    set_source_files_properties("${src}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/${_asset_dir}"
      XCODE_EXPLICIT_FILE_TYPE compiled)
    target_sources(${BIN_TARGET} PRIVATE "${src}")
  endforeach()
else()
  # Copy assets to the build assets subdirectory. This serves two purposes:
  # - If smpq is installed, devilutionx.mpq is built from these files.
  # - If smpq is not installed, the game will load the assets directly from this directory.
  copy_files(
    FILES ${devilutionx_assets}
    SRC_PREFIX "assets/"
    OUTPUT_DIR "${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}"
    OUTPUT_VARIABLE DEVILUTIONX_OUTPUT_ASSETS_FILES)
  set(DEVILUTIONX_MPQ_FILES ${devilutionx_assets})
  if (Gettext_FOUND)
    foreach(lang ${devilutionx_langs})
      list(APPEND DEVILUTIONX_MPQ_FILES "${lang}.gmo")
    endforeach()
  endif()

  add_trim_target(devilutionx_trim_assets
    ROOT_FOLDER "${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}"
    CURRENT_FILES ${DEVILUTIONX_MPQ_FILES})
  if(devilutionx_lang_targets)
    add_dependencies(devilutionx_trim_assets ${devilutionx_lang_targets})
  endif()

  if(BUILD_ASSETS_MPQ)
    if(TARGET_PLATFORM STREQUAL "dos")
      set(DEVILUTIONX_MPQ "${CMAKE_CURRENT_BINARY_DIR}/devx.mpq")
    else()
      set(DEVILUTIONX_MPQ "${CMAKE_CURRENT_BINARY_DIR}/devilutionx.mpq")
    endif()
    add_custom_command(
      COMMENT "Building devilutionx.mpq"
      OUTPUT "${DEVILUTIONX_MPQ}"
      COMMAND ${CMAKE_COMMAND} -E remove -f "${DEVILUTIONX_MPQ}"
      COMMAND ${SMPQ} -A -M 1 -C BZIP2 -c "${DEVILUTIONX_MPQ}" ${DEVILUTIONX_MPQ_FILES}
      WORKING_DIRECTORY "${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}"
      DEPENDS ${TRIM_COMMAND_BYPRODUCT} ${DEVILUTIONX_OUTPUT_ASSETS_FILES} ${devilutionx_lang_targets} ${devilutionx_lang_files}
      VERBATIM)
    add_custom_target(devilutionx_mpq DEPENDS "${DEVILUTIONX_MPQ}")
    add_dependencies(devilutionx_mpq devilutionx_trim_assets)
    add_dependencies(libdevilutionx devilutionx_mpq)
  else()
    add_custom_target(devilutionx_copied_assets DEPENDS ${DEVILUTIONX_OUTPUT_ASSETS_FILES} ${devilutionx_lang_targets})
    add_dependencies(devilutionx_copied_assets devilutionx_trim_assets)
    add_dependencies(libdevilutionx devilutionx_copied_assets)
  endif()
endif()
