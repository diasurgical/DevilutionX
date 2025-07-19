include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

find_package(Patch REQUIRED)

set(SOL2_ENABLE_INSTALL OFF)
set(SOL2_BUILD_LUA OFF)
set(SOL2_LUA_VERSION "${LUA_VERSION_STRING}")

FetchContent_Declare_ExcludeFromAll(sol2
    URL https://github.com/ThePhD/sol2/archive/c1f95a773c6f8f4fde8ca3efe872e7286afe4444.tar.gz
    URL_HASH MD5=845ce869733636632d857d8517ac3eb5
          # https://github.com/ThePhD/sol2/pull/1712
    PATCH_COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0001-Reduce-cmake_minimum_required-to-3.22.patch || true
          # https://github.com/ThePhD/sol2/pull/1722
          COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0002-Fix-array-index-out-of-bounds-in-stack_field.hpp.patch || true
          # https://github.com/ThePhD/sol2/pull/1676
          COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0003-Change-end-to-sen-in-usertype_container.hpp.patch || true
          # https://github.com/ThePhD/sol2/pull/1704
          COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0004-Fix-missing-Lua-Lua-target-when-using-system-Lua.patch || true
          # https://github.com/ThePhD/sol2/pull/1705
          COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0005-fix-INTERFACE_LINK_LIBRARIES-property-for-lualib.patch || true
          # https://github.com/ThePhD/sol2/pull/1716
          COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0006-Overload-stateless_reference_equals-and-stateless_re.patch || true
          # https://github.com/ThePhD/sol2/pull/1716
          COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0007-Add-Nerixyz-s-test-and-fix-for-C-17.patch || true
          # https://github.com/ThePhD/sol2/pull/1674
          COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0008-Faster-track-for-lightuserdata_value.patch || true
          # https://github.com/ThePhD/sol2/pull/1722
          COMMAND "${Patch_EXECUTABLE}" -p0 -N < ${CMAKE_CURRENT_LIST_DIR}/patches/0009-Work-around-1581.patch || true
)
FetchContent_MakeAvailable_ExcludeFromAll(sol2)

target_include_directories(sol2 SYSTEM BEFORE INTERFACE ${CMAKE_CURRENT_LIST_DIR}/sol_config)
target_compile_definitions(sol2 INTERFACE SOL_NO_EXCEPTIONS=1)
