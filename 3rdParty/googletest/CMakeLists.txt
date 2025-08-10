include(functions/FetchContent_ExcludeFromAll_backport)

FetchContent_Declare_ExcludeFromAll(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
  URL_HASH MD5=7e11f6cfcf6498324ac82d567dcb891e
)

set(INSTALL_GTEST OFF)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable_ExcludeFromAll(googletest)
