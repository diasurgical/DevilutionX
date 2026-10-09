include(functions/FetchContent_ExcludeFromAll_backport)

include(FetchContent)

FetchContent_Declare_ExcludeFromAll(mpqfs
    URL https://github.com/diasurgical/mpqfs/archive/de99526f869cf647fde8a2229131c880eab442e9.tar.gz
    URL_HASH MD5=f8c67245857a1d532d7a8884e176556f
)
FetchContent_MakeAvailable_ExcludeFromAll(mpqfs)

if(MPQFS_FILE_BUFFER_SIZE)
  target_compile_definitions(mpqfs_obj PRIVATE "MPQFS_FILE_BUFFER_SIZE=${MPQFS_FILE_BUFFER_SIZE}")
endif()


if(DEVILUTIONX_WINDOWS_NO_WCHAR)
  target_compile_definitions(mpqfs_obj PRIVATE MPQFS_WINDOWS_NO_WCHAR)
endif()
