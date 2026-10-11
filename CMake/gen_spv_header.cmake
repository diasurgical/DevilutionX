# Compiles the Vulkan GLSL shaders of the GPU scaler to SPIR-V and emits a
# single C++ header with the embedded blobs.
#
# Usage:
#   cmake -DGLSLANG=<glslangValidator> -DSRC_DIR=<shaders/vulkan dir>
#         -DOUTPUT=<header path> -P gen_spv_header.cmake

set(SHADER_PAIRS
  "fullscreen.vert:kVertex"
  "hqx_index.frag:kHqxIndex"
  "hqx_blend.frag:kHqxBlend"
  "scalefx_pass0.frag:kSfxPass0"
  "scalefx_pass1.frag:kSfxPass1"
  "scalefx_pass2.frag:kSfxPass2"
  "scalefx_pass3.frag:kSfxPass3"
  "scalefx_pass4.frag:kSfxPass4"
  "blit.frag:kBlit"
)

set(tmpdir "${OUTPUT}.tmp")
file(REMOVE_RECURSE "${tmpdir}")
file(MAKE_DIRECTORY "${tmpdir}")

set(content "// GENERATED FILE - do not edit by hand.
// Regenerated from Source/engine/render/gpu/shaders/vulkan by
// CMake/gen_spv_header.cmake (glslangValidator -> SPIR-V).
// SPIR-V for the SDL_GPU Vulkan backend of the GPU scaler.
//
// The byte arrays are wrapped in clang-format off/on markers:
// generated data columns are exempt from clang-format (16 bytes per row).

#pragma once

#include <cstdint>

namespace devilution::gpu::spv {
")

foreach(PAIR IN LISTS SHADER_PAIRS)
  string(REPLACE ":" ";" PAIR_LIST "${PAIR}")
  list(GET PAIR_LIST 0 SHADER_FILE)
  list(GET PAIR_LIST 1 ARRAY_NAME)
  set(SPV_FILE "${tmpdir}/${ARRAY_NAME}.spv")
  execute_process(
    COMMAND "${GLSLANG}" -V "${SRC_DIR}/${SHADER_FILE}" -o "${SPV_FILE}"
    RESULT_VARIABLE RES
    OUTPUT_VARIABLE OUT
    ERROR_VARIABLE ERR
  )
  if(NOT RES EQUAL 0)
    message(FATAL_ERROR "glslangValidator failed for ${SHADER_FILE}:\n${OUT}${ERR}")
  endif()
  file(READ "${SPV_FILE}" HEX HEX)
  string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," BYTES "${HEX}")
  # Wrap the flat "0xNN," stream into rows of 16 bytes (5 chars each = 80 cols).
  string(LENGTH "${BYTES}" byte_len)
  set(rows "")
  set(offset 0)
  while(offset LESS byte_len)
    math(EXPR remain "${byte_len} - ${offset}")
    if(remain GREATER 80)
      set(take 80)
    else()
      set(take ${remain})
    endif()
    string(SUBSTRING "${BYTES}" ${offset} ${take} row)
    string(APPEND rows "\t${row}\n")
    math(EXPR offset "${offset} + 80")
  endwhile()
  string(APPEND content "// ${SHADER_FILE}\n// clang-format off\nstatic constexpr std::uint8_t ${ARRAY_NAME}[] = {\n${rows}};\n// clang-format on\n\n")
endforeach()

string(APPEND content "} // namespace devilution::gpu::spv\n")

# Avoid touching the file when content is unchanged (prevents rebuild churn).
if(EXISTS "${OUTPUT}")
  file(READ "${OUTPUT}" old_content)
else()
  set(old_content "")
endif()
if(NOT content STREQUAL old_content)
  file(WRITE "${OUTPUT}" "${content}")
endif()
file(REMOVE_RECURSE "${tmpdir}")
