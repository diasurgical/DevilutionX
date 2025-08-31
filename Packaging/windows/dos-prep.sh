#!/usr/bin/env bash

set -euo pipefail

INSTALL_PREFIX=/opt/i386-pc-msdosdjgpp-toolchain

# only use sudo when necessary
if [ `id -u` -ne 0 ]; then
	SUDO=sudo
else
	SUDO=""
fi

# Install dependencies on Debian / Ubuntu:
install_system_deps() {
	if which apt-get 2>/dev/null; then
		set -x
		$SUDO apt-get update
		$SUDO apt-get install bison flex curl gcc g++ make texinfo zlib1g-dev tar bzip2 \
			gzip xz-utils unzip python3-dev m4 dos2unix nasm cmake
		{ set +x; } 2>/dev/null
	fi
}

# Build and install DJGPP
build_and_install_djgpp() {
	git clone https://github.com/jwt27/build-gcc.git
	cd build-gcc
	$SUDO ./build-djgpp.sh --prefix="$INSTALL_PREFIX" --batch binutils gcc-14.2.0 djgpp-cvs
	cd -
	$SUDO rm -rf build-gcc
}

main() {
	set -x
	mkdir -p tmp/dos-prep
	cd tmp/dos-prep
	install_system_deps
	build_and_install_djgpp
}

main "$@"
