include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

FetchContent_Declare_ExcludeFromAll(magic_enum
    URL https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.7.tar.gz
    URL_HASH MD5=5afd218c48c3f7cf094889a182842a50
)
FetchContent_MakeAvailable_ExcludeFromAll(magic_enum)
