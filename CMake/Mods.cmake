include(functions/copy_files)
include(functions/trim_retired_files)

if(NOT DEFINED DEVILUTIONX_MODS_OUTPUT_DIRECTORY)
  set(DEVILUTIONX_MODS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mods")
endif()
if(NOT DEFINED DEVILUTIONX_MODS_MPQ_DIRECTORY)
  set(DEVILUTIONX_MODS_MPQ_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mods")
endif()

set(hellfire_mod
    changelog.txt
  manifest.ini
  data/diabsmall.cel
  data/hf_logo3.cel
  lua/mods/hf/init.lua
  missiles/blueg.trn
  monsters/dmage/dmagew.cl2     
  monsters/golem/fire.trn       
  monsters/skelaxe/black.trn    
  monsters/skelaxe/old_pearl.trn
  monsters/skelaxe/pearl.trn    
  nlevels/cutl5w.clx
  nlevels/cutl6w.clx
  nlevels/l5data/cornerstone.dun
  nlevels/l5data/uberroom.dun   
  plrgfx/barbarian.trn
  plrgfx/bard.trn
  plrgfx/summoner.trn
  plrgfx/test.trn
  txtdata/classes/classdat.tsv
  txtdata/classes/barbarian/attributes.tsv
  txtdata/classes/barbarian/sprites.tsv
  txtdata/classes/bard/attributes.tsv
  txtdata/classes/bard/sprites.tsv
  txtdata/classes/bard/starting_loadout.tsv
  txtdata/classes/monk/attributes.tsv
  txtdata/classes/monk/sprites.tsv
  txtdata/classes/rogue/attributes.tsv
  txtdata/classes/rogue/sprites.tsv
  txtdata/classes/sorcerer/attributes.tsv
  txtdata/classes/sorcerer/sprites.tsv
  txtdata/classes/sorcerer/starting_loadout.tsv
  txtdata/classes/summoner/animations.tsv
  txtdata/classes/summoner/attributes.tsv
  txtdata/classes/summoner/sounds.tsv
  txtdata/classes/summoner/sprites.tsv
  txtdata/classes/summoner/starting_loadout.tsv
  txtdata/classes/test/animations.tsv
  txtdata/classes/test/attributes.tsv
  txtdata/classes/test/sounds.tsv
  txtdata/classes/test/sprites.tsv
  txtdata/classes/test/starting_loadout.tsv
  txtdata/classes/warrior/attributes.tsv
  txtdata/classes/warrior/sprites.tsv
  txtdata/classes/warrior/starting_loadout.tsv
  txtdata/items/item_prefixes.tsv
  txtdata/items/item_suffixes.tsv
  txtdata/items/unique_itemdat.tsv
  txtdata/missiles/misdat.tsv
  txtdata/missiles/missile_sprites.tsv
  txtdata/monsters/monstdat.tsv
  txtdata/sound/effects.tsv
  txtdata/sound/effects-unused.tsv
  txtdata/spells/spelldat.tsv
  txtdata/towners/quest_dialog.tsv
  txtdata/towners/towners.tsv
  ui_art/diablo.pal
  ui_art/heros.pcx
  ui_art/hf_logo2.pcx
  ui_art/hf_titlew.clx
  ui_art/mainmenuw.clx
  ui_art/supportw.clx)

if(NOT UNPACKED_MPQS)
  list(APPEND hellfire_mod
    data/inv/objcurs2-widths.txt)
endif()

copy_files(
  FILES ${hellfire_mod}
  SRC_PREFIX "mods/hf/"
  OUTPUT_DIR "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/hf"
  OUTPUT_VARIABLE HELLFIRE_OUTPUT_FILES)
set(HELLFIRE_MPQ_FILES ${hellfire_mod})
add_trim_target(hellfire_trim_assets
  ROOT_FOLDER "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/hf"
  CURRENT_FILES ${HELLFIRE_MPQ_FILES})

if(BUILD_MOD_MPQS)
  set(HELLFIRE_MPQ "${DEVILUTIONX_MODS_MPQ_DIRECTORY}/hf.mpq")
  add_custom_command(
    COMMENT "Building hf.mpq"
    OUTPUT "${HELLFIRE_MPQ}"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${HELLFIRE_MPQ}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEVILUTIONX_MODS_MPQ_DIRECTORY}"
    COMMAND ${SMPQ} -A -M 1 -C BZIP2 -c "${HELLFIRE_MPQ}" ${HELLFIRE_MPQ_FILES}
    WORKING_DIRECTORY "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/hf"
    DEPENDS ${TRIM_COMMAND_BYPRODUCT} ${HELLFIRE_OUTPUT_FILES}
    VERBATIM)
  add_custom_target(hellfire_mpq DEPENDS "${HELLFIRE_MPQ}")
  add_dependencies(hellfire_mpq hellfire_trim_assets)
  add_dependencies(libdevilutionx hellfire_mpq)
  if(APPLE)
    set_source_files_properties("${HELLFIRE_MPQ}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/mods"
      XCODE_EXPLICIT_FILE_TYPE compiled)
    target_sources(${BIN_TARGET} PRIVATE "${HELLFIRE_MPQ}")
  endif()
else()
  add_custom_target(hellfire_copied_assets DEPENDS ${HELLFIRE_OUTPUT_FILES})
  add_dependencies(hellfire_copied_assets hellfire_trim_assets)
  add_dependencies(libdevilutionx hellfire_copied_assets)
endif()
