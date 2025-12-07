
# copy_files(
#   FILES <file...>
#   OUTPUT_DIR <output_dir>
#   [SRC_PREFIX <src_prefix>]
#   [OUTPUT_VARIABLE <output_variable>]
# )
function(copy_files)
  set(options)
  set(oneValueArgs SRC_PREFIX OUTPUT_DIR OUTPUT_VARIABLE)
  set(multiValueArgs FILES)
  cmake_parse_arguments(PARSE_ARGV 0 arg
      "${options}" "${oneValueArgs}" "${multiValueArgs}"
  )
  if(arg_UNPARSED_ARGUMENTS)
     message(FATAL "unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(OUTPUT_DIR IN_LIST arg_KEYWORDS_MISSING_VALUES)
    message(FATAL "OUTPUT_DIR is required")
  endif()

  foreach(path ${arg_FILES})
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${arg_SRC_PREFIX}${path}")
    set(dst "${arg_OUTPUT_DIR}/${path}")
    list(APPEND _out_paths "${dst}")
    add_custom_command(
      COMMENT "Copying ${arg_SRC_PREFIX}${path}"
      OUTPUT "${dst}"
      DEPENDS "${src}"
      COMMAND ${CMAKE_COMMAND} -E copy "${src}" "${dst}"
      VERBATIM
    )
  endforeach()
  if(arg_OUTPUT_VARIABLE)
    set(${arg_OUTPUT_VARIABLE} ${_out_paths} PARENT_SCOPE)
  endif()

endfunction()
