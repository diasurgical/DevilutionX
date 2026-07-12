#!/usr/bin/env bash

# Builds a pinned i686-w64-mingw32 cross-toolchain suitable for targeting
# Windows 9x, plus the libraries needed by the devilutionx win9x build.
#
# Distro MinGW GCC packages (>= 13, e.g. Ubuntu >= 24.04) cannot target
# Windows 9x: their libgcc/libstdc++ are compiled at the default
# `_WIN32_WINNT` (Windows 10), which makes the win32 thread model use
# Vista-only kernel32 APIs (SRWLock, CONDITION_VARIABLE). Those end up in
# the import table of every statically linked C++ binary, and the
# Windows 9x loader refuses to start a program with unresolved kernel32
# imports. It also breaks compilation of `<mutex>` at `_WIN32_WINNT < 0x0600`.
#
# GCC still ships the legacy, 9x-safe threading shim behind
# `#if _WIN32_WINNT < 0x0600` in gthr-win32.h, so we build the toolchain
# ourselves with the target libraries pinned to `_WIN32_WINNT=0x0400`.
# libstdc++'s configure then disables the (Vista-dependent) C++11 thread
# primitives, exactly like the GCC <= 12 toolchains this build used to rely
# on, while keeping an up-to-date compiler.
#
# Installs to /opt/mingw9x by default; override with MINGW9X_PREFIX.
# CMake/platforms/mingw9x.toolchain.cmake picks the toolchain up from there
# (CROSS_PREFIX).
#
# Host build dependencies (Ubuntu):
#   build-essential flex bison curl xz-utils bzip2 pkg-config patch

BINUTILS_VERS=2.46.1
GCC_VERS=16.1.0
MINGW_VERS=14.0.0
ZLIB_VERS=1.3.1
SDLDEV_VERS=1.2.15

# exit when any command fails
set -euo pipefail

# i586 (plain Pentium) baseline: Windows 98-era CPUs may lack the cmov
# instructions that an i686 triplet's code generation assumes.
TARGET=i586-w64-mingw32
PREFIX="${MINGW9X_PREFIX:-/opt/mingw9x}"
JOBS="$(nproc)"
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"

# API level for the target runtime libraries (libgcc, libstdc++):
# 0x0400 = Windows 95 / NT4. This is what keeps Vista-only APIs out of the
# static runtime and makes libstdc++ configure select the pre-Vista
# threading support.
TARGET_LIB_CPPFLAGS="-DWINVER=0x0400 -D_WIN32_WINNT=0x0400"

# only use sudo when the prefix is not writable
SUDO=""
if ! mkdir -p "${PREFIX}" 2>/dev/null || [ ! -w "${PREFIX}" ]; then
    SUDO=sudo
    sudo mkdir -p "${PREFIX}"
fi

rm -rf tmp-mingw9x-prep
mkdir -p tmp-mingw9x-prep
cd tmp-mingw9x-prep

echo "== Downloading sources"
curl --no-progress-meter -OL "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERS}.tar.xz"
curl --no-progress-meter -OL "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERS}/gcc-${GCC_VERS}.tar.xz"
curl --no-progress-meter -o "mingw-w64-v${MINGW_VERS}.tar.bz2" -L \
    "https://downloads.sourceforge.net/project/mingw-w64/mingw-w64/mingw-w64-release/mingw-w64-v${MINGW_VERS}.tar.bz2"
curl --no-progress-meter -OL "https://github.com/madler/zlib/releases/download/v${ZLIB_VERS}/zlib-${ZLIB_VERS}.tar.gz"
curl --no-progress-meter -OL "https://www.libsdl.org/release/SDL-devel-${SDLDEV_VERS}-mingw32.tar.gz"
for archive in *.tar.*; do tar -xf "$archive"; done

# GMP/MPFR/MPC, built in-tree by the GCC build (checksum-verified).
(cd "gcc-${GCC_VERS}" && ./contrib/download_prerequisites)

# Patch GCC for pre-Vista targets (see mingw9x-gcc.patch):
#  - libstdc++'s <chrono> tzdb assumes Vista+ APIs (GetDynamicTimeZoneInformation,
#    GetModuleHandleExA) whenever <windows.h> is available; restrict its
#    Windows-specific code to _WIN32_WINNT >= 0x0600 so that pre-Vista target
#    libraries fall back to the generic $TZ-based code path instead.
#  - libgcc's gthread runtime statically imports TryEnterCriticalSection and
#    GetThreadId, which do not exist on Windows 9x (the 9x loader rejects any
#    binary importing them); resolve them dynamically via GetProcAddress and
#    degrade gracefully when they are unavailable.
patch -d "gcc-${GCC_VERS}" -p1 < "${SCRIPT_DIR}/mingw9x-gcc.patch"

echo "== Building binutils ${BINUTILS_VERS}"
mkdir binutils-build
(
    cd binutils-build
    "../binutils-${BINUTILS_VERS}/configure" \
        --target="${TARGET}" \
        --prefix="${PREFIX}" \
        --with-sysroot="${PREFIX}" \
        --disable-multilib \
        --disable-nls \
        --disable-werror
    make -j"${JOBS}" MAKEINFO=true
    $SUDO make install MAKEINFO=true
)

export PATH="${PREFIX}/bin:${PATH}"

echo "== Installing mingw-w64 ${MINGW_VERS} headers"
mkdir mingw-headers-build
(
    cd mingw-headers-build
    "../mingw-w64-v${MINGW_VERS}/mingw-w64-headers/configure" \
        --host="${TARGET}" \
        --prefix="${PREFIX}/${TARGET}" \
        --with-sysroot="${PREFIX}" \
        --with-default-msvcrt=msvcrt \
        --with-default-win32-winnt=0x0400
    $SUDO make install
)
# GCC looks for target headers in <sysroot>/mingw/include.
$SUDO ln -sfn "${TARGET}" "${PREFIX}/mingw"

echo "== Building GCC ${GCC_VERS} (compiler)"
mkdir gcc-build
(
    cd gcc-build
    CFLAGS_FOR_TARGET="-O2 ${TARGET_LIB_CPPFLAGS}" \
    CXXFLAGS_FOR_TARGET="-O2 ${TARGET_LIB_CPPFLAGS}" \
    "../gcc-${GCC_VERS}/configure" \
        --target="${TARGET}" \
        --prefix="${PREFIX}" \
        --with-sysroot="${PREFIX}" \
        --enable-languages=c,c++ \
        --enable-threads=win32 \
        --disable-sjlj-exceptions \
        --with-dwarf2 \
        --with-default-msvcrt=msvcrt \
        --disable-multilib \
        --disable-shared \
        --disable-libgomp \
        --disable-nls \
        --disable-werror
    make -j"${JOBS}" all-gcc
    $SUDO make install-gcc
)

echo "== Building mingw-w64 ${MINGW_VERS} CRT"
mkdir mingw-crt-build
(
    cd mingw-crt-build
    "../mingw-w64-v${MINGW_VERS}/mingw-w64-crt/configure" \
        --host="${TARGET}" \
        --prefix="${PREFIX}/${TARGET}" \
        --with-sysroot="${PREFIX}" \
        --with-default-msvcrt=msvcrt \
        --enable-lib32 \
        --disable-lib64
    make -j"${JOBS}"
    $SUDO make install
)

echo "== Building GCC ${GCC_VERS} (runtime libraries)"
(
    cd gcc-build
    make -j"${JOBS}"
    $SUDO make install
)

echo "== Building zlib ${ZLIB_VERS}"
(
    cd "zlib-${ZLIB_VERS}"
    CC="${TARGET}-gcc" AR="${TARGET}-ar" RANLIB="${TARGET}-ranlib" \
        ./configure --prefix="${PREFIX}/${TARGET}" --static
    make -j"${JOBS}"
    $SUDO make install
)

echo "== Installing prebuilt SDL ${SDLDEV_VERS}"
$SUDO cp -r SDL-*/include/* "${PREFIX}/${TARGET}/include"
$SUDO cp -r SDL-*/lib/* "${PREFIX}/${TARGET}/lib"
$SUDO cp -r SDL-*/bin/* "${PREFIX}/${TARGET}/bin"

# Fixup pkgconfig prefix:
find "${PREFIX}/${TARGET}/lib/pkgconfig/" -name '*.pc' -exec \
    $SUDO sed -i "s|^prefix=.*|prefix=${PREFIX}/${TARGET}|" '{}' \;

# Fixup CMake prefix:
find "${PREFIX}/${TARGET}" -name '*.cmake' -exec \
    $SUDO sed -i "s|/opt/local/${TARGET}|${PREFIX}/${TARGET}|" '{}' \;

echo "== Creating pkg-config wrapper"
$SUDO tee "${PREFIX}/bin/${TARGET}-pkg-config" > /dev/null <<EOF
#!/bin/sh
export PKG_CONFIG_LIBDIR="${PREFIX}/${TARGET}/lib/pkgconfig"
exec pkg-config "\$@"
EOF
$SUDO chmod +x "${PREFIX}/bin/${TARGET}-pkg-config"

echo "== Smoke-testing the toolchain for Windows 9x compatibility"
cat > smoke.cpp <<'EOF'
// <mutex> must be compilable at the Windows 98 API level (std::lock_guard
// is used with SDL mutexes by sdl_audiolib).
#include <mutex>
#include <cstdio>
#include <string>
struct S { S() { std::puts("init"); } };
S& get() { static S s; return s; }  // static-local guard uses libgcc gthreads
int main() { get(); std::string s = "hello"; std::printf("%s\n", s.c_str()); }
EOF
"${TARGET}-g++" -O2 -static -DWINVER=0x0500 -D_WIN32_WINDOWS=0x0500 -D_WIN32_WINNT=0 smoke.cpp -o smoke.exe
# None of these exist in the Windows 9x kernel32.dll; the 9x loader refuses
# to start a program that imports them.
BAD_IMPORTS='SRWLock|ConditionVariable|InitOnce|TryEnterCriticalSection|GetTickCount64|GetThreadId|GetModuleHandleEx|EncodePointer|FlsAlloc'
if "${TARGET}-objdump" -p smoke.exe | grep -E "${BAD_IMPORTS}"; then
    echo "ERROR: toolchain produces binaries with post-9x kernel32 imports (see above)"
    exit 1
fi
echo "OK: no post-9x kernel32 imports"
# The 9x loader also rejects binaries that expect a subsystem newer than 4.x.
# (no `grep -q` here: with pipefail it would report the SIGPIPE'd objdump)
if ! "${TARGET}-objdump" -p smoke.exe | grep 'MajorSubsystemVersion[[:space:]]*4$' > /dev/null; then
    echo "ERROR: PE subsystem version is not 4.x:"
    "${TARGET}-objdump" -p smoke.exe | grep SubsystemVersion
    exit 1
fi
echo "OK: PE subsystem version 4.x"

echo "== Done. Toolchain installed to ${PREFIX}"
