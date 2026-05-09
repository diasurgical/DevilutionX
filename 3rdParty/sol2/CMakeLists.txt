include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

set(SOL2_ENABLE_INSTALL OFF)
set(SOL2_BUILD_LUA OFF)
set(SOL2_LUA_VERSION "${LUA_VERSION_STRING}")

FetchContent_Declare_ExcludeFromAll(sol2
    # branch: devilutionx
    # This is upstream c1f95a773c6f8f4fde8ca3efe872e7286afe4444 with the following PRs merged on top:
    # https://github.com/ThePhD/sol2/pull/1712
    # https://github.com/ThePhD/sol2/pull/1722
    # https://github.com/ThePhD/sol2/pull/1676
    # https://github.com/ThePhD/sol2/pull/1704
    # https://github.com/ThePhD/sol2/pull/1705
    # https://github.com/ThePhD/sol2/pull/1716
    # https://github.com/ThePhD/sol2/pull/1674
    # https://github.com/ThePhD/sol2/pull/1722
    URL https://github.com/diasurgical/sol2/archive/832ac772c2cd3d9620d447e9e77897f7b5e806e3.tar.gz
    URL_HASH MD5=06c9c0f3242ee803f50e8fd210fdfe4e
)
FetchContent_MakeAvailable_ExcludeFromAll(sol2)

target_include_directories(sol2 SYSTEM BEFORE INTERFACE ${CMAKE_CURRENT_LIST_DIR}/sol_config)
target_compile_definitions(sol2 INTERFACE SOL_NO_EXCEPTIONS=1)
