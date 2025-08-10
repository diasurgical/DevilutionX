include(functions/FetchContent_ExcludeFromAll_backport)

# Workaround for deprecation of older CMake versions
set(CMAKE_POLICY_VERSION_MINIMUM 3.22)

if(NOT DISABLE_LTO)
  # Force CMake to raise an error if INTERPROCEDURAL_OPTIMIZATION
  # is enabled and compiler does not support IPO
  set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
endif()

if(DEVILUTIONX_STATIC_LIBPNG)
  set(PNG_LIBRARY png_static)
  set(PNG_STATIC ON)
  set(PNG_SHARED OFF)
else()
  set(PNG_LIBRARY png)
  set(PNG_STATIC OFF)
  set(PNG_SHARED ON)
endif()
set(PNG_TESTS OFF)
set(PNG_EXECUTABLES OFF)
set(SKIP_INSTALL_ALL ON)

if(APPLE AND "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
  set(PNG_ARM_NEON "on" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare_ExcludeFromAll(libpng
  URL https://github.com/glennrp/libpng/archive/0a158f3506502dfa23edfc42790dfaed82efba17.tar.gz
  URL_HASH MD5=6d705417242732e8e081bff752c98c18
)
FetchContent_MakeAvailable_ExcludeFromAll(libpng)

target_include_directories(${PNG_LIBRARY} INTERFACE
  $<BUILD_INTERFACE:${libpng_SOURCE_DIR}>
  $<BUILD_INTERFACE:${libpng_BINARY_DIR}>)

add_library(PNG::PNG ALIAS ${PNG_LIBRARY})
