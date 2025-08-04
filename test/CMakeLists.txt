include(GoogleTest)

add_library(libdevilutionx_so SHARED)
set_target_properties(libdevilutionx_so PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_dependencies(libdevilutionx_so PUBLIC libdevilutionx)
set_target_properties(libdevilutionx_so PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(test_main OBJECT main.cpp)
target_link_dependencies(test_main PUBLIC libdevilutionx_so GTest::gtest GTest::gmock)

set(tests
  animationinfo_test
  appfat_test
  automap_test
  cursor_test
  dead_test
  diablo_test
  drlg_common_test
  drlg_l1_test
  drlg_l2_test
  drlg_l3_test
  drlg_l4_test
  effects_test
  inv_test
  items_test
  math_test
  missiles_test
  pack_test
  player_test
  quests_test
  scrollrt_test
  stores_test
  tile_properties_test
  timedemo_test
  writehero_test
)
set(standalone_tests
  codec_test
  crawl_test
  data_file_test
  file_util_test
  format_int_test
  ini_test
  palette_blending_test
  parse_int_test
  path_test
  vision_test
  random_test
  rectangle_test
  static_vector_test
  str_cat_test
  utf8_test
)
if(NOT USE_SDL1)
  list(APPEND standalone_tests text_render_integration_test)
endif()
set(benchmarks
  clx_render_benchmark
  crawl_benchmark
  dun_render_benchmark
  light_render_benchmark
  palette_blending_benchmark
  path_benchmark
)

include(Fixtures.cmake)

foreach(test_target ${tests} ${standalone_tests} ${benchmarks})
  add_executable(${test_target} "${test_target}.cpp")
  set_target_properties(${test_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  if(GPERF)
    target_link_libraries(${test_target} PUBLIC ${GPERFTOOLS_LIBRARIES})
  endif()
endforeach()

foreach(test_target ${tests} ${standalone_tests})
  gtest_discover_tests(${test_target})
endforeach()

foreach(test_target ${tests})
  target_link_libraries(${test_target} PRIVATE test_main)
endforeach()

foreach(test_target ${standalone_tests})
  target_link_libraries(${test_target} PRIVATE GTest::gtest_main)
  target_include_directories(${test_target} PRIVATE "${PROJECT_SOURCE_DIR}/Source")
endforeach()

foreach(target ${benchmarks})
  target_link_libraries(${target} PRIVATE benchmark::benchmark benchmark::benchmark_main)
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/Source")
endforeach()

add_library(app_fatal_for_testing OBJECT app_fatal_for_testing.cpp)
target_sources(app_fatal_for_testing INTERFACE $<TARGET_OBJECTS:app_fatal_for_testing>)

add_library(language_for_testing OBJECT language_for_testing.cpp)
target_sources(language_for_testing INTERFACE $<TARGET_OBJECTS:language_for_testing>)

target_link_dependencies(codec_test PRIVATE libdevilutionx_codec app_fatal_for_testing)
target_link_dependencies(clx_render_benchmark
  PRIVATE
  DevilutionX::SDL
  tl
  app_fatal_for_testing
  language_for_testing
  libdevilutionx_clx_render
  libdevilutionx_load_clx
  libdevilutionx_log
  libdevilutionx_surface
)
target_link_dependencies(crawl_test PRIVATE libdevilutionx_crawl)
target_link_dependencies(crawl_benchmark PRIVATE libdevilutionx_crawl)
target_link_dependencies(data_file_test PRIVATE libdevilutionx_txtdata app_fatal_for_testing language_for_testing)
target_link_dependencies(dun_render_benchmark PRIVATE libdevilutionx_so)
target_link_dependencies(file_util_test PRIVATE libdevilutionx_file_util app_fatal_for_testing)
target_link_dependencies(format_int_test PRIVATE libdevilutionx_format_int language_for_testing)
target_link_dependencies(ini_test PRIVATE libdevilutionx_ini app_fatal_for_testing)
target_link_dependencies(light_render_benchmark PRIVATE libdevilutionx_light_render DevilutionX::SDL libdevilutionx_surface libdevilutionx_paths app_fatal_for_testing)
target_link_dependencies(palette_blending_test PRIVATE libdevilutionx_palette_blending DevilutionX::SDL libdevilutionx_strings GTest::gmock app_fatal_for_testing)
target_link_dependencies(palette_blending_benchmark PRIVATE libdevilutionx_palette_blending DevilutionX::SDL app_fatal_for_testing)
target_link_dependencies(parse_int_test PRIVATE libdevilutionx_parse_int)
target_link_dependencies(path_test PRIVATE libdevilutionx_pathfinding libdevilutionx_direction app_fatal_for_testing)
target_link_dependencies(vision_test PRIVATE libdevilutionx_vision)
target_link_dependencies(path_benchmark PRIVATE libdevilutionx_pathfinding app_fatal_for_testing)
target_link_dependencies(random_test PRIVATE libdevilutionx_random)
target_link_dependencies(static_vector_test PRIVATE libdevilutionx_random app_fatal_for_testing)
target_link_dependencies(str_cat_test PRIVATE libdevilutionx_strings)
if(DEVILUTIONX_SCREENSHOT_FORMAT STREQUAL DEVILUTIONX_SCREENSHOT_FORMAT_PNG AND NOT USE_SDL1)
  target_link_dependencies(text_render_integration_test
    PRIVATE
    DevilutionX::SDL
    GTest::gmock
    GTest::gtest
    tl
    app_fatal_for_testing
    language_for_testing
    libdevilutionx_primitive_render
    libdevilutionx_strings
    libdevilutionx_surface
    libdevilutionx_surface_to_png
    libdevilutionx_text_render
  )
endif()
target_link_dependencies(utf8_test PRIVATE libdevilutionx_utf8)

target_include_directories(writehero_test PRIVATE ../3rdParty/PicoSHA2)
