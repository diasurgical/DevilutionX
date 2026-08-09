include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    URL https://github.com/diasurgical/mpqfs/archive/f9dbf8abc0e5a25b3c76b2b964a707dc72b7d45b.tar.gz
    URL_HASH MD5=2994259729289fdc01c368ee31055679
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)

if(MPQFS_FILE_BUFFER_SIZE)
  target_compile_definitions(mpqfs_obj PRIVATE "MPQFS_FILE_BUFFER_SIZE=${MPQFS_FILE_BUFFER_SIZE}")
endif()


if(DEVILUTIONX_WINDOWS_NO_WCHAR)
  target_compile_definitions(mpqfs_obj PRIVATE MPQFS_WINDOWS_NO_WCHAR)
endif()
