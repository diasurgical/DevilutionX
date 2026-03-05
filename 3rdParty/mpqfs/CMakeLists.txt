include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    URL https://github.com/diasurgical/mpqfs/archive/27144129a5d8f84ddbe058a7c5d7f2c8a724af67.tar.gz
    URL_HASH MD5=c137f8eb7f5e097c63f9b46769ff2b5c
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)
