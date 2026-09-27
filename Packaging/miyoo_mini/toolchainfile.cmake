set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(MIYOO_SYSROOT
	"/opt/miyoomini-toolchain/arm-linux-gnueabihf/sysroot")

set(MIYOO_GCC_PREFIX
	"/opt/miyoomini-gcc13")

set(MIYOO_BINUTILS
	"${MIYOO_GCC_PREFIX}/binutils")

set(CMAKE_SYSROOT "${MIYOO_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${MIYOO_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${MIYOO_SYSROOT}")

set(CMAKE_C_COMPILER
	"${MIYOO_GCC_PREFIX}/bin/arm-linux-gnueabihf-gcc")

set(CMAKE_CXX_COMPILER
	"${MIYOO_GCC_PREFIX}/bin/arm-linux-gnueabihf-g++")

set(CMAKE_AR
	"/usr/bin/arm-linux-gnueabihf-ar")

set(CMAKE_RANLIB
	"/usr/bin/arm-linux-gnueabihf-ranlib")

set(CMAKE_STRIP
	"/usr/bin/arm-linux-gnueabihf-strip")

set(MIYOO_ARCH_FLAGS
	"-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve")

# -B selects modern ARM binutils without changing the runtime/sysroot.
set(CMAKE_C_FLAGS_INIT
	"-B${MIYOO_BINUTILS}/ ${MIYOO_ARCH_FLAGS}")

set(CMAKE_CXX_FLAGS_INIT
	"-B${MIYOO_BINUTILS}/ ${MIYOO_ARCH_FLAGS}")

# libstdc++ and libgcc were built against the Miyoo sysroot and are linked
# statically so they do not need to be shipped with OnionOS/MiniUI.
set(CMAKE_EXE_LINKER_FLAGS_INIT
	"-B${MIYOO_BINUTILS}/ -static-libstdc++ -static-libgcc")
