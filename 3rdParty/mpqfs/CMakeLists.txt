include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    URL https://github.com/diasurgical/mpqfs/archive/ee29f596bd01822ae0edf01bcf3564cb9e6ea781.tar.gz
    URL_HASH MD5=bb5503213e0f22f00fdd8c350fe40b19
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)

if(MPQFS_FILE_BUFFER_SIZE)
  target_compile_definitions(mpqfs_obj PRIVATE "MPQFS_FILE_BUFFER_SIZE=${MPQFS_FILE_BUFFER_SIZE}")
endif()


if(DEVILUTIONX_WINDOWS_NO_WCHAR)
  target_compile_definitions(mpqfs_obj PRIVATE MPQFS_WINDOWS_NO_WCHAR)
endif()
