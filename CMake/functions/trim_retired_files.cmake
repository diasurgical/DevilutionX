set(SCRIPT_CONTENT [=[
include(functions/trim_retired_files)
trim_retired_files("${ROOT_FOLDER}" "${CURRENT_FILES}" "${OUTPUT_FILE}")
]=])

function(trim_retired_files root_folder current_files output_file)
  file(
    GLOB_RECURSE retired_files
    RELATIVE "${root_folder}"
    "${root_folder}/*")

  list(REMOVE_ITEM retired_files ${current_files})
  list(LENGTH retired_files retired_file_count)
  foreach(retired_file ${retired_files})
    file(REMOVE "${root_folder}/${retired_file}")
  endforeach()

  if(${retired_file_count} GREATER 0 OR NOT EXISTS ${output_file})
    file(TOUCH ${output_file})
  endif()
endfunction(trim_retired_files)

function(add_trim_target arg_TARGET_NAME)
  set(oneValueArgs
    ROOT_FOLDER
    BYPRODUCT
    SCRIPT_PATH)

  set(multiValueArgs CURRENT_FILES)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "${oneValueArgs}" "${multiValueArgs}")

  if(NOT arg_ROOT_FOLDER)
    message(FATAL_ERROR "add_trim_command: missing required parameter ROOT_FOLDER")
  endif()
  if(NOT arg_OUTPUT OR NOT arg_BYPRODUCT OR NOT arg_SCRIPT_PATH)
    cmake_path(GET arg_ROOT_FOLDER FILENAME root_filename)
    if(NOT arg_BYPRODUCT)
      set(arg_BYPRODUCT "${CMAKE_CURRENT_BINARY_DIR}/${root_filename}.rm")
    endif()
    if(NOT arg_SCRIPT_PATH)
      get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
      if(is_multi_config)
        set(arg_SCRIPT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${root_filename}_$<CONFIG>.cmake")
      else()
        set(arg_SCRIPT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${root_filename}.cmake")
      endif()
    endif()
  endif()

  file(GENERATE OUTPUT "${arg_SCRIPT_PATH}" CONTENT "${SCRIPT_CONTENT}")
  add_custom_target("${arg_TARGET_NAME}"
    COMMENT "Trimming ${arg_ROOT_FOLDER}"
    BYPRODUCTS "${arg_BYPRODUCT}"
    COMMAND ${CMAKE_COMMAND}
      -D "CMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}"
      -D "ROOT_FOLDER=${arg_ROOT_FOLDER}"
      -D "CURRENT_FILES=${arg_CURRENT_FILES}"
      -D "OUTPUT_FILE=${arg_BYPRODUCT}"
      -P "${arg_SCRIPT_PATH}"
    VERBATIM)

  set(TRIM_COMMAND_BYPRODUCT "${arg_BYPRODUCT}" PARENT_SCOPE)
endfunction(add_trim_target)
