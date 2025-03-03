if(NOT DEFINED DEVILUTIONX_TEST_FIXTURES_OUTPUT_DIRECTORY)
  set(DEVILUTIONX_TEST_FIXTURES_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
endif()

set(devilutionx_fixtures
  diablo/1-2588.dun
  diablo/1-743271966.dun
  diablo/2-1383137027.dun
  diablo/3-844660068.dun
  diablo/4-609325643.dun
  diablo/4-902156014.dun
  diablo/5-68685319.dun
  diablo/5-1677631846.dun
  diablo/6-1824554527.dun
  diablo/6-2034738122.dun
  diablo/6-2033265779.dun
  diablo/7-680552750.dun
  diablo/7-1607627156.dun
  diablo/8-1999936419.dun
  diablo/9-262005438.dun
  diablo/10-879635115.dun
  diablo/10-1630062353.dun
  diablo/11-384626536.dun
  diablo/12-2104541047.dun
  diablo/13-428074402.dun
  diablo/13-594689775.dun
  diablo/14-717625719.dun
  diablo/14-815743776.dun
  diablo/15-1256511996.dun
  diablo/15-1583642716.dun
  diablo/15-1583642716-changed.dun
  diablo/16-741281013.dun
  hellfire/1-401921334.dun
  hellfire/1-536340718.dun
  hellfire/2-128964898.dun
  hellfire/2-1180526547.dun
  hellfire/3-1369955278.dun
  hellfire/3-1512491184.dun
  hellfire/3-1799396623.dun
  hellfire/4-1190318991.dun
  hellfire/4-1924296259.dun
  hellfire/17-19770182.dun
  hellfire/18-1522546307.dun
  hellfire/19-125121312.dun
  hellfire/20-1511478689.dun
  hellfire/21-2122696790.dun
  hellfire/22-1191662129.dun
  hellfire/23-97055268.dun
  hellfire/24-1324803725.dun
  levels/l1data/banner1.dun
  levels/l1data/banner2.dun
  levels/l1data/rnd6.dun
  levels/l1data/skngdo.dun
  levels/l2data/blind1.dun
  levels/l2data/blood1.dun
  levels/l2data/blood2.dun
  levels/l2data/bonestr1.dun
  levels/l2data/bonestr2.dun
  levels/l3data/anvil.dun
  levels/l4data/diab1.dun
  levels/l4data/diab2a.dun
  levels/l4data/diab2b.dun
  levels/l4data/diab3a.dun
  levels/l4data/diab3b.dun
  levels/l4data/diab4a.dun
  levels/l4data/diab4b.dun
  levels/l4data/vile1.dun
  levels/l4data/warlord.dun
  levels/l4data/warlord2.dun
  memory_map/additionalMissiles.txt
  memory_map/game.txt
  memory_map/hero.txt
  memory_map/item.txt
  memory_map/itemPack.txt
  memory_map/level.txt
  memory_map/levelSeed.txt
  memory_map/lightning.txt
  memory_map/missile.txt
  memory_map/monster.txt
  memory_map/object.txt
  memory_map/player.txt
  memory_map/portal.txt
  memory_map/quest.txt
  text_render_integration_test/basic-colors.png
  text_render_integration_test/basic.png
  text_render_integration_test/horizontal_overflow.png
  text_render_integration_test/horizontal_overflow-colors.png
  text_render_integration_test/kerning_fit_spacing-colors.png
  text_render_integration_test/kerning_fit_spacing.png
  text_render_integration_test/kerning_fit_spacing__align_center-colors.png
  text_render_integration_test/kerning_fit_spacing__align_center.png
  text_render_integration_test/kerning_fit_spacing__align_center__newlines.png
  text_render_integration_test/kerning_fit_spacing__align_center__newlines_in_fmt-colors.png
  text_render_integration_test/kerning_fit_spacing__align_center__newlines_in_value-colors.png
  text_render_integration_test/kerning_fit_spacing__align_right-colors.png
  text_render_integration_test/kerning_fit_spacing__align_right.png
  text_render_integration_test/vertical_overflow.png
  text_render_integration_test/vertical_overflow-colors.png
  timedemo/WarriorLevel1to2/demo_0.dmo
  timedemo/WarriorLevel1to2/demo_0_reference_spawn_0.sv
  timedemo/WarriorLevel1to2/spawn_0.sv
  txtdata/cr.tsv
  txtdata/crlf.tsv
  txtdata/empty.tsv
  txtdata/empty_with_utf8_bom.tsv
  txtdata/lf.tsv
  txtdata/lf_no_trail.tsv
  txtdata/sample.tsv
  txtdata/utf8_bom.tsv
)

foreach(fixture ${devilutionx_fixtures})
  set(src "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${fixture}")
  set(dst "${DEVILUTIONX_TEST_FIXTURES_OUTPUT_DIRECTORY}/${fixture}")
  list(APPEND DEVILUTIONX_OUTPUT_TEST_FIXTURES_FILES "${dst}")
  add_custom_command(
    COMMENT "Copying ${fixture}"
    OUTPUT "${dst}"
    DEPENDS "${src}"
    COMMAND ${CMAKE_COMMAND} -E copy "${src}" "${dst}"
    VERBATIM)
endforeach()

add_custom_target(devilutionx_copied_fixtures DEPENDS ${DEVILUTIONX_OUTPUT_TEST_FIXTURES_FILES})
add_dependencies(test_main devilutionx_copied_fixtures)
