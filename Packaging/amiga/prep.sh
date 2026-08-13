#!/usr/bin/env bash

# exit when any command fails
set -euo pipefail

PARALLELISM="$(getconf _NPROCESSORS_ONLN)"

mkdir -p deps
cd deps

# SDL1.2
wget https://github.com/AmigaPorts/SDL-1.2/archive/AmigaOS3.tar.gz -O SDL-1.2.tar.gz
tar -xvf SDL-1.2.tar.gz
cd SDL-1.2-AmigaOS3

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
    export CFLAGS="-Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wno-error=return-mismatch -Wno-error=return-type -Wno-error=pointer-sign -Wno-error=pointer-to-int-cast -Wno-error=int-to-pointer-cast"
fi

cmake \
    -S . \
    -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DM68K_COMMON="-s -ffast-math -fomit-frame-pointer -fbbb=-" \
    -DM68K_CPU=68040 \
    -DM68K_FPU=hard \
    -DAPOLLOBLIT=OFF \
    -DGIT_TAG=1.2.15

cmake --build build -j"$PARALLELISM"

# cmake --install is broken on amigadev/crosstools
cmake -P build/cmake_install.cmake

cd ..
