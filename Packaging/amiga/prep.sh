#!/usr/bin/env bash

# exit when any command fails
set -euo pipefail

#set compiler params
export TARGET='m68k-amigaos'
export SYSROOT=/opt/$TARGET
export M68K_CPU=68040
export M68K_FPU=hard
export M68K_CPU_FPU="-m${M68K_CPU} -m${M68K_FPU}-float"
export M68K_COMMON="-s -ffast-math -fomit-frame-pointer -fbbb=-"
export M68K_CFLAGS="${M68K_CPU_FPU} ${M68K_COMMON}"
export M68K_CXXFLAGS="${M68K_CPU_FPU} ${M68K_COMMON}"

PARALLELISM="$(getconf _NPROCESSORS_ONLN)"

mkdir -p deps
mkdir -p ${SYSROOT}/usr/lib
mkdir -p ${SYSROOT}/usr/include
cd deps

# SDL1.2
wget https://github.com/AmigaPorts/libSDL12/archive/master.tar.gz -O SDL-1.2.tar.gz
tar -xvf SDL-1.2.tar.gz
cd libSDL12-master
# GCC 14+ promotes several legacy-C conformance warnings to hard errors by
# default (implicit-function-declaration, implicit-int, int-conversion,
# incompatible-pointer-types, return-mismatch, etc). This codebase predates
# that change and trips all of them; downgrade back to warnings so it still
# builds on newer GCC. Some of these warning names don't exist before GCC 14
# at all, so -Wno-error=<name> is a hard "unrecognized option" error on
# older GCC rather than a no-op -- gate this on the actual compiler version
# rather than applying it unconditionally.
# Appended at end-of-file rather than a specific line, since this Makefile
# is fetched from the upstream master branch and its exact contents can
# change; Make variable assignments apply regardless of where they appear.
GCC_MAJOR_VERSION="$(m68k-amigaos-gcc -dumpversion | cut -d. -f1)"
if [ "$GCC_MAJOR_VERSION" -ge 14 ]; then
	printf '\nGCCFLAGS += -Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wno-error=return-mismatch -Wno-error=return-type -Wno-error=pointer-sign -Wno-error=pointer-to-int-cast -Wno-error=int-to-pointer-cast\n' >> Makefile
fi
make PREFX=${SYSROOT} PREF=${SYSROOT} -j"$PARALLELISM"
mkdir -p ${SYSROOT}/usr/lib
mkdir -p ${SYSROOT}/usr/include
cp -fvr libSDL.a ${SYSROOT}/usr/lib/
cp -fvr include/* ${SYSROOT}/usr/include/
cd ..
