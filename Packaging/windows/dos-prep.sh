#!/usr/bin/env bash

set -euo pipefail

# only use sudo when necessary
if [ `id -u` -ne 0 ]; then
	SUDO=sudo
else
	SUDO=""
fi

# Install dependencies on Debian / Ubuntu:
if which apt-get 2>/dev/null; then
	set -x
	$SUDO apt-get update
	$SUDO apt-get install bison flex curl gcc g++ make texinfo zlib1g-dev tar bzip2 \
		gzip xz-utils unzip python3-dev m4 dos2unix nasm cmake
	{ set +x; } 2>/dev/null
fi

INSTALL_PREFIX=/opt/i386-pc-msdosdjgpp-toolchain

set -x
mkdir -p tmp/dos-prep
cd tmp/dos-prep

# Build and install DJGPP
git clone https://github.com/jwt27/build-gcc.git
cd build-gcc
$SUDO ./build-djgpp.sh --prefix="$INSTALL_PREFIX" --batch binutils gcc-14.2.0 djgpp-cvs
cd -
$SUDO rm -rf build-gcc

# Activate DJGPP environment
{ set +x; } 2>/dev/null
set +eu
source "${INSTALL_PREFIX}/bin/i386-pc-msdosdjgpp-setenv"
set -eu
set -x

# Build and install SDL2 for DOS
git clone --branch=dos-vbe https://github.com/diasurgical/SDL.git
cd SDL
autoreconf -fi
./configure --host=i386-pc-msdosdjgpp --prefix="${INSTALL_PREFIX}/i386-pc-msdosdjgpp" --disable-shared --enable-static --enable-video-svga --enable-timer-dos --enable-uclock
make -j$(nproc)
$SUDO "${INSTALL_PREFIX}/bin/i386-pc-msdosdjgpp-setenv" make install
cd -
rm -rf SDL
