include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    URL https://github.com/diasurgical/mpqfs/archive/1dc2407a9cd97eb7aaf8a0c184c70436871b7e5a.tar.gz
    URL_HASH MD5=df58160d1bed8a6f41bb5f1d58520611
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)

if(MPQFS_FILE_BUFFER_SIZE)
  target_compile_definitions(mpqfs_obj PRIVATE "MPQFS_FILE_BUFFER_SIZE=${MPQFS_FILE_BUFFER_SIZE}")
endif()


if(DEVILUTIONX_WINDOWS_NO_WCHAR)
  target_compile_definitions(mpqfs_obj PRIVATE MPQFS_WINDOWS_NO_WCHAR)
endif()
