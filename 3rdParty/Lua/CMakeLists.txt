include(functions/FetchContent_ExcludeFromAll_backport)

# Workaround for deprecation of older CMake versions
set(CMAKE_POLICY_VERSION_MINIMUM 3.22)

set(LUA_ENABLE_TESTING OFF)
set(LUA_BUILD_COMPILER OFF)
if(DEVILUTIONX_STATIC_LUA)
  set(LUA_ENABLE_SHARED OFF)
else()
  set(LUA_ENABLE_SHARED ON)
endif()

include(FetchContent)
FetchContent_Declare_ExcludeFromAll(Lua
    URL https://github.com/walterschell/Lua/archive/88246d621abf7b6fba9332f49229d507f020e450.tar.gz
    URL_HASH MD5=03b76927cb5341ffc53bea12c37ddcca
)
FetchContent_MakeAvailable_ExcludeFromAll(Lua)

if(CMAKE_SYSTEM_NAME MATCHES "Darwin" AND DARWIN_MAJOR_VERSION VERSION_EQUAL 8)
  # We need legacy-support from MacPorts for:
  # localtime_r gmtime_r
  find_package(MacportsLegacySupport REQUIRED)
  target_link_libraries(lua_static PRIVATE MacportsLegacySupport::MacportsLegacySupport)
elseif(ANDROID AND ("${ANDROID_ABI}" STREQUAL "armeabi-v7a" OR "${ANDROID_ABI}" STREQUAL "x86"))
  target_compile_definitions(lua_internal INTERFACE -DLUA_USE_C89)
elseif(NINTENDO_3DS OR VITA OR NINTENDO_SWITCH OR NXDK)
  target_compile_definitions(lua_static PUBLIC -DLUA_USE_C89)
elseif(IOS)
  target_compile_definitions(lua_static PUBLIC -DLUA_USE_IOS)
endif()
