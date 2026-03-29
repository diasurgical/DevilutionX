include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    GIT_REPOSITORY https://github.com/diasurgical/mpqfs.git
    GIT_TAG 9423bd48d04c8a0c51d3df6ce328e66785e3b441
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)

if(MPQFS_FILE_BUFFER_SIZE)
  target_compile_definitions(mpqfs PRIVATE "MPQFS_FILE_BUFFER_SIZE=${MPQFS_FILE_BUFFER_SIZE}")
endif()