option(MINGW_STDTHREADS_GENERATE_STDHEADERS "" OFF)

# Workaround for deprecation of older CMake versions
set(CMAKE_POLICY_VERSION_MINIMUM 3.22)

FetchContent_Declare_ExcludeFromAll(mingw-std-threads
  GIT_REPOSITORY https://github.com/meganz/mingw-std-threads
  GIT_TAG bee085c0a6cb32c59f0b55c7bba976fe6dcfca7f)
FetchContent_MakeAvailable_ExcludeFromAll(mingw-std-threads)

target_compile_definitions(libnatpmp_obj PRIVATE -D_WIN32_WINNT=0x601 -DSTATICLIB)
target_compile_definitions(zto_obj PRIVATE -D_WIN32_WINNT=0x601)
target_compile_definitions(zto_pic PRIVATE -D_WIN32_WINNT=0x601)
target_compile_definitions(libzt_obj PRIVATE -D_WIN32_WINNT=0x601)
target_compile_definitions(zt_pic PRIVATE -D_WIN32_WINNT=0x601)
target_compile_definitions(${libzt_LIB_NAME} PRIVATE -D_WIN32_WINNT=0x601 -DADD_EXPORTS=1)
target_compile_definitions(${libzt_LIB_NAME} PUBLIC -DADD_EXPORTS=1)
target_link_libraries(libzt_obj PRIVATE mingw_stdthreads)
target_link_libraries(${libzt_LIB_NAME} mingw_stdthreads)
