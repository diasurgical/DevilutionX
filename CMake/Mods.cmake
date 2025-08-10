if(NOT DEFINED DEVILUTIONX_MODS_OUTPUT_DIRECTORY)
  set(DEVILUTIONX_MODS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mods")
endif()

set(hellfire_mod
  lua/mods/Hellfire/init.lua
  nlevels/cutl5w.clx
  nlevels/cutl6w.clx
  nlevels/l5data/cornerstone.dun
  nlevels/l5data/uberroom.dun
  txtdata/items/item_prefixes.tsv
  txtdata/items/item_suffixes.tsv
  txtdata/items/unique_itemdat.tsv
  txtdata/missiles/misdat.tsv
  txtdata/missiles/missile_sprites.tsv
  txtdata/monsters/monstdat.tsv
  txtdata/sound/effects.tsv
  txtdata/spells/spelldat.tsv
  ui_art/diablo.pal
  ui_art/hf_titlew.clx
  ui_art/supportw.clx
  ui_art/mainmenuw.clx)

if(NOT UNPACKED_MPQS)
  list(APPEND hellfire_mod
    data/inv/objcurs2-widths.txt)
endif()

foreach(asset_file ${hellfire_mod})
  set(src "${CMAKE_CURRENT_SOURCE_DIR}/mods/Hellfire/${asset_file}")
  set(dst "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/Hellfire/${asset_file}")
  list(APPEND HELLFIRE_MPQ_FILES "${asset_file}")
  list(APPEND HELLFIRE_OUTPUT_FILES "${dst}")
  add_custom_command(
    COMMENT "Copying ${asset_file}"
    OUTPUT "${dst}"
    DEPENDS "${src}"
    COMMAND ${CMAKE_COMMAND} -E copy "${src}" "${dst}"
    VERBATIM)
endforeach()

if(BUILD_ASSETS_MPQ)
  set(HELLFIRE_MPQ "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/Hellfire.mpq")
  add_custom_command(
    COMMENT "Building Hellfire.mpq"
    OUTPUT "${HELLFIRE_MPQ}"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${HELLFIRE_MPQ}"
    COMMAND ${SMPQ} -A -M 1 -C BZIP2 -c "${HELLFIRE_MPQ}" ${HELLFIRE_MPQ_FILES}
    WORKING_DIRECTORY "${DEVILUTIONX_MODS_OUTPUT_DIRECTORY}/Hellfire"
    DEPENDS ${HELLFIRE_OUTPUT_FILES}
    VERBATIM)
  add_custom_target(hellfire_mpq DEPENDS "${HELLFIRE_MPQ}")
  add_dependencies(libdevilutionx hellfire_mpq)
else()
  add_custom_target(hellfire_copied_mod_file DEPENDS ${HELLFIRE_OUTPUT_FILES})
  add_dependencies(libdevilutionx hellfire_copied_mod_file)
endif()
