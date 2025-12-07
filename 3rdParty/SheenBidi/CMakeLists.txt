include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

set(BUILD_TESTING OFF)
set(ASAN OFF)
set(UBSAN OFF)

FetchContent_Declare_ExcludeFromAll(SheenBidi
  URL https://github.com/Tehreer/SheenBidi/archive/refs/tags/v2.9.0.tar.gz
  URL_HASH MD5=2c22cfad93a52afa7bd25ad56a3a4089
)
FetchContent_MakeAvailable_ExcludeFromAll(SheenBidi)
