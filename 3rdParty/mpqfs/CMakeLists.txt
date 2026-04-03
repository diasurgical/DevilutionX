include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    GIT_REPOSITORY https://github.com/diasurgical/mpqfs.git
    GIT_TAG 8887bf1940d3ba74d73950d85bb79bb738b78896
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)

if(MPQFS_FILE_BUFFER_SIZE)
  target_compile_definitions(mpqfs PRIVATE "MPQFS_FILE_BUFFER_SIZE=${MPQFS_FILE_BUFFER_SIZE}")
endif()
