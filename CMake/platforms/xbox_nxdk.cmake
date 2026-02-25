set(NONET ON)
set(ASAN OFF)
set(UBSAN OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/xbox_nxdk/finders")

set(DEVILUTIONX_SYSTEM_BZIP2 OFF)
set(DEVILUTIONX_SYSTEM_LIBFMT OFF)

# nxdk's zlib is built with Z_SOLO, which excludes utility functions like
# uncompress() that mpqfs needs for zlib-compressed MPQ sectors.
# Diablo/Hellfire MPQs only use PKWARE DCL implode (and bzip2 for fonts.mpq),
# so zlib decompression in mpqfs is not needed.
set(MPQFS_USE_ZLIB OFF CACHE BOOL "" FORCE)

set(BUILD_ASSETS_MPQ OFF)
set(DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pkg/assets")
set(DEVILUTIONX_WINDOWS_NO_WCHAR ON)

set(DEVILUTIONX_RESAMPLER_SPEEX OFF)
set(DEFAULT_AUDIO_BUFFER_SIZE 5120)

set(DEVILUTIONX_GAMEPAD_TYPE Xbox)

set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
