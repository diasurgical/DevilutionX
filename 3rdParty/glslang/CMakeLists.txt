# glslang — Khronos reference GLSL/ES frontend used by the GPU scaler to
# compile runtime-loaded libretro slang shaders (.slang) to SPIR-V in-process
# (the same frontend RetroArch uses for its slang pipeline).
#
# Static libraries only; binaries, tests, SPIRV-Tools integration and
# install rules are all disabled to keep configure/build time reasonable.
# Link targets: glslang::glslang, glslang::SPIRV (and optionally
# glslang::glslang-default-resource-limits for apps that need resource caps).

include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

set(BUILD_EXTERNAL OFF)
set(BUILD_WERROR OFF)
set(GLSLANG_TESTS OFF)
set(GLSLANG_ENABLE_INSTALL OFF)
set(ENABLE_SPIRV ON)
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_GLSLANG_JS OFF)
set(ENABLE_HLSL ON)
set(ENABLE_OPT OFF)
set(ENABLE_SPVREMAPPER OFF)
set(ENABLE_PCH ON)
set(BUILD_SHARED_LIBS OFF)

FetchContent_Declare(glslang
  URL https://github.com/KhronosGroup/glslang/archive/refs/tags/16.6.0.tar.gz
  URL_HASH SHA256=9c09b901149c729df745057dafa815278aaa101b84d2b6e14f16a42de52f97f2
)
FetchContent_MakeAvailable_ExcludeFromAll(glslang)
