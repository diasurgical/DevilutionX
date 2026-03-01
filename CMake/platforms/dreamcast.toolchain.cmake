# DevilutionX Dreamcast Toolchain
# We extend the official KOS toolchain to add project-specific settings.

if(NOT DEFINED ENV{KOS_CMAKE_TOOLCHAIN})
    message(FATAL_ERROR "KOS_CMAKE_TOOLCHAIN not defined. Please source environ.sh.")
endif()

# 1. Load the official KOS toolchain
# This sets up compilers, flags (-m4-single), and system paths correctly.
include("$ENV{KOS_CMAKE_TOOLCHAIN}")

# 2. Set DevilutionX specific platform flag
# This triggers loading CMake/platforms/dreamcast.cmake
set(DREAMCAST ON)

# 3. Add our include paths
# KOS ports often hide headers in subdirectories
list(APPEND CMAKE_INCLUDE_PATH
    "$ENV{KOS_PORTS}/include/zlib"
    "$ENV{KOS_PORTS}/include/bzlib"
    "$ENV{KOS_PORTS}/include/SDL"
)

# 4. Force libraries (The "Nuclear Option" for sub-projects)
# Standard find modules struggle with KOS layout, so we force them.
set(ZLIB_INCLUDE_DIR "$ENV{KOS_PORTS}/include/zlib" CACHE PATH "ZLIB Include Dir" FORCE)
set(ZLIB_LIBRARY "$ENV{KOS_PORTS}/lib/libz.a" CACHE FILEPATH "ZLIB Library" FORCE)
set(BZIP2_INCLUDE_DIR "$ENV{KOS_PORTS}/include/bzlib" CACHE PATH "BZip2 Include Dir" FORCE)
set(BZIP2_LIBRARIES "$ENV{KOS_PORTS}/lib/libbz2.a" CACHE FILEPATH "BZip2 Library" FORCE)

# 5. Force SDL1 - MUST set SDL_LIBRARY without -lpthreads (KOS has built-in threading)
set(SDL_INCLUDE_DIR "$ENV{KOS_PORTS}/include/SDL" CACHE PATH "SDL Include Dir" FORCE)
set(SDL_LIBRARY "$ENV{KOS_PORTS}/lib/libSDL.a" CACHE FILEPATH "SDL Library" FORCE)

# 6. Fix libfmt and magic_enum compilation
# Disable long double support because sh4-gcc's long double (64-bit) confuses libfmt
# Disable magic_enum asserts because GCC 15's stricter parsing breaks them in comma expressions
add_compile_definitions(FMT_USE_LONG_DOUBLE=0 FMT_USE_FLOAT128=0 MAGIC_ENUM_NO_ASSERT)

# 7. KOS doesn't have pthreads as a separate library - threading is built into libkallisti
set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "" FORCE)
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(Threads_FOUND TRUE)

# 8. Remove host system library paths that sneak in
set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "" CACHE STRING "" FORCE)

# 8b. Ignore host system paths (homebrew, etc.) to prevent finding wrong libraries
set(CMAKE_IGNORE_PATH "/opt/homebrew;/opt/homebrew/include;/opt/homebrew/lib;/usr/local;/usr/local/include;/usr/local/lib" CACHE STRING "" FORCE)

# 8c. Force using bundled magic_enum (not system/homebrew version)
set(DEVILUTIONX_SYSTEM_MAGIC_ENUM OFF CACHE BOOL "" FORCE)

# 9. Override Link Rule to use Grouping
# KOS toolchain already adds -T, -nodefaultlibs, and -L paths via <LINK_FLAGS>
# We just need to add --start-group/--end-group for circular dependency resolution
# NOTE: kos_add_romdisk() in CMakeLists.txt handles -lromdiskbase with --whole-archive
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER> <FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -lkallisti -lc -lgcc -lm -lstdc++ -Wl,--end-group")
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_C_COMPILER> <FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -lkallisti -lc -lgcc -lm -lstdc++ -Wl,--end-group")

# 10. Skip compiler checks (saves time and avoids romdisk symbol issues)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
