include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    GIT_REPOSITORY https://github.com/diasurgical/mpqfs.git
    GIT_TAG c4e1327feb4620d42f183fed6e8aab9133719192
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)

if(MPQFS_FILE_BUFFER_SIZE)
  target_compile_definitions(mpqfs PRIVATE "MPQFS_FILE_BUFFER_SIZE=${MPQFS_FILE_BUFFER_SIZE}")
endif()
