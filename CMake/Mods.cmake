include(functions/copy_files)
include(functions/trim_retired_files)

if(NOT DEFINED DEVILUTIONX_MODS_OUTPUT_DIRECTORY)
  set(DEVILUTIONX_MODS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mods")
endif()

set(hellfire_mod
  lua/mods/Hellfire/init.lua
  nlevels/cutl5w.clx
  nlevels/cutl6w.clx
  nlevels/l5data/cornerstone.dun
  nlevels/l5data/uberroom.dun
  txtdata/classes/sorcerer/starting_loadout.tsv
  txtdata/classes/classdat.tsv
  txtdata/items/item_prefixes.tsv
  txtdata/items/item_suffixes.tsv
  txtdata/items/unique_itemdat.tsv
  txtdata/missiles/misdat.tsv
  txtdata/missiles/missile_sprites.tsv
  txtdata/monsters/monstdat.tsv
  txtdata/sound/effects.tsv
  txtdata/spells/spelldat.tsv
  txtdata/towners/quest_dialog.tsv
  txtdata/towners/towners.tsv
  ui_art/diablo.pal
  ui_art/hf_titlew.clx
  ui_art/supportw.clx
  ui_art/mainmenuw.clx)

if(NOT UNPACKED_MPQS)
  list(APPEND hellfire_mod
    data/inv/objcurs2-widths.txt)
endif()

if(APPLE)
  foreach(asset_file ${hellfire_mod})
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/mods/Hellfire/${asset_file}")
    get_filename_component(_asset_dir "${asset_file}" DIRECTORY)
    set_source_files_properties("${src}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/mods/Hellfire/${_asset_dir}"
      XCODE_EXPLICIT_FILE_TYPE compiled)
    target_sources(${BIN_TARGET} PRIVATE "${src}")
  endforeach()
else()
  copy_files(
    FILES ${hellfire_mod}
    SRC_PREFIX "mods/Hellfire/"
    OUTPUT_DIR "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/Hellfire"
    OUTPUT_VARIABLE HELLFIRE_OUTPUT_FILES)
  set(HELLFIRE_MPQ_FILES ${hellfire_mod})
  add_trim_target(hellfire_trim_assets
    ROOT_FOLDER "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/Hellfire"
    CURRENT_FILES ${HELLFIRE_MPQ_FILES})

  if(BUILD_ASSETS_MPQ)
    set(HELLFIRE_MPQ "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/Hellfire.mpq")
    add_custom_command(
      COMMENT "Building Hellfire.mpq"
      OUTPUT "${HELLFIRE_MPQ}"
      COMMAND ${CMAKE_COMMAND} -E remove -f "${HELLFIRE_MPQ}"
      COMMAND ${SMPQ} -A -M 1 -C BZIP2 -c "${HELLFIRE_MPQ}" ${HELLFIRE_MPQ_FILES}
      WORKING_DIRECTORY "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/Hellfire"
      DEPENDS ${TRIM_COMMAND_BYPRODUCT} ${HELLFIRE_OUTPUT_FILES}
      VERBATIM)
    add_custom_target(hellfire_mpq DEPENDS "${HELLFIRE_MPQ}")
    add_dependencies(hellfire_mpq hellfire_trim_assets)
    add_dependencies(libdevilutionx hellfire_mpq)
  else()
    add_custom_target(hellfire_copied_assets DEPENDS ${HELLFIRE_OUTPUT_FILES})
    add_dependencies(hellfire_copied_assets hellfire_trim_assets)
    add_dependencies(libdevilutionx hellfire_copied_assets)
  endif()
endif()
