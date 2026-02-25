include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    GIT_REPOSITORY https://github.com/diasurgical/mpqfs.git
    GIT_TAG 19f5bb3c426f4e749f3a8f859562d4ca3bca3898
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)