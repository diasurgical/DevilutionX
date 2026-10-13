# SPIRV-Cross — Khronos SPIR-V introspection and cross-compilation library.
# Used by the GPU scaler on the D3D12 backend to transpile runtime-loaded
# slang shaders (compiled to SPIR-V by glslang) into HLSL for the existing
# guarded d3dcompiler path, and for descriptor reflection.
#
# Static libraries only; CLI, tests, MSL/reflect/util targets are disabled.

include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS OFF)
set(SPIRV_CROSS_SHARED OFF)
set(SPIRV_CROSS_STATIC ON)
set(SPIRV_CROSS_CLI OFF)
set(SPIRV_CROSS_ENABLE_TESTS OFF)
set(SPIRV_CROSS_ENABLE_GLSL ON)
set(SPIRV_CROSS_ENABLE_HLSL ON)
set(SPIRV_CROSS_ENABLE_MSL OFF)
set(SPIRV_CROSS_ENABLE_CPP OFF)
set(SPIRV_CROSS_ENABLE_REFLECT OFF)
set(SPIRV_CROSS_ENABLE_C_API OFF)
set(SPIRV_CROSS_ENABLE_UTIL OFF)
set(SPIRV_CROSS_SKIP_INSTALL ON)

FetchContent_Declare(spirv_cross
  URL https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
  URL_HASH SHA256=f708982e88b763ef5b0394ed468fc2c3628f68cdab89f2e7a946cd628c04e721
)
FetchContent_MakeAvailable_ExcludeFromAll(spirv_cross)
