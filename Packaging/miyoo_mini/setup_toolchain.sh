#!/bin/sh
set -eu

TOOLCHAIN_VERSION="v0.0.3"
GCC_VERSION="13.3.0"
GCC_SHA512="ed5f2f4c6ed2c796fcf2c93707159e9dbd3ddb1ba063d549804dd68cdabbb6d550985ae1c8465ae9a336cfe29274a6eb0f42e21924360574ebd8e5d5c7c9a801"

MIYOO_TOOLCHAIN="/opt/miyoomini-toolchain"
MIYOO_SYSROOT="${MIYOO_TOOLCHAIN}/arm-linux-gnueabihf/sysroot"

MODERN_TOOLCHAIN="/opt/miyoomini-gcc13"
MODERN_BINUTILS="${MODERN_TOOLCHAIN}/binutils"

main() {
	install_dependencies
	install_toolchain
	install_modern_gcc
	install_modern_binutils
}

install_dependencies() {
	apt-get -y update
	apt-get -y install \
		bc \
		binutils-arm-linux-gnueabihf \
		bison \
		build-essential \
		bzip2 \
		bzr \
		cmake \
		cmake-curses-gui \
		cpio \
		flex \
		gettext \
		git \
		libgmp-dev \
		libmpc-dev \
		libmpfr-dev \
		libncurses5-dev \
		make \
		rsync \
		scons \
		smpq \
		tree \
		unzip \
		wget \
		xz-utils \
		zip
}

install_toolchain() {
	TOOLCHAIN_TAR="miyoomini-toolchain.tar.xz"
	TOOLCHAIN_ARCH="$(uname -m)"

	if [ "$TOOLCHAIN_ARCH" = "aarch64" ]; then
		TOOLCHAIN_REPO="miyoomini-toolchain-buildroot-aarch64"
	else
		TOOLCHAIN_REPO="miyoomini-toolchain-buildroot"
	fi

	TOOLCHAIN_URL="https://github.com/shauninman/${TOOLCHAIN_REPO}/releases/download/${TOOLCHAIN_VERSION}/${TOOLCHAIN_TAR}"

	cd /opt
	wget "$TOOLCHAIN_URL"

	echo "extracting remote toolchain ${TOOLCHAIN_VERSION} (${TOOLCHAIN_ARCH})"

	tar xf "./${TOOLCHAIN_TAR}"
	rm -f "./${TOOLCHAIN_TAR}"
}

install_modern_gcc() {
	if [ -x "${MODERN_TOOLCHAIN}/bin/arm-linux-gnueabihf-g++" ]; then
		echo "GCC ${GCC_VERSION} Miyoo toolchain already installed"
		return
	fi

	WORK_DIR="/tmp/miyoomini-gcc-${GCC_VERSION}"
	SOURCE_DIR="${WORK_DIR}/gcc-${GCC_VERSION}"
	BUILD_DIR="${WORK_DIR}/build"
	GCC_TAR="gcc-${GCC_VERSION}.tar.xz"
	GCC_URL="https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/${GCC_TAR}"

	rm -rf "$WORK_DIR"
	mkdir -p "$WORK_DIR"

	cd "$WORK_DIR"

	echo "downloading GCC ${GCC_VERSION}"
	wget "$GCC_URL"

	echo "${GCC_SHA512}  ${GCC_TAR}" | sha512sum -c -

	tar xf "$GCC_TAR"
	mkdir "$BUILD_DIR"
	cd "$BUILD_DIR"

	"${SOURCE_DIR}/configure" \
		--target=arm-linux-gnueabihf \
		--prefix="$MODERN_TOOLCHAIN" \
		--with-sysroot="$MIYOO_SYSROOT" \
		--with-build-sysroot="$MIYOO_SYSROOT" \
		--disable-bootstrap \
		--disable-multilib \
		--disable-nls \
		--enable-languages=c,c++ \
		--disable-libsanitizer \
		--disable-libquadmath \
		--disable-libgomp

	make -j"$(getconf _NPROCESSORS_ONLN)" \
		all-gcc \
		all-target-libgcc \
		all-target-libstdc++-v3

	make \
		install-gcc \
		install-target-libgcc \
		install-target-libstdc++-v3

	rm -rf "$WORK_DIR"

	echo "installed GCC ${GCC_VERSION} Miyoo toolchain"
}

install_modern_binutils() {
	# GCC 13 emits architecture extensions unsupported by the old
	# Miyoo binutils 2.32. Use Ubuntu 24.04's ARM binutils instead.
	mkdir -p "$MODERN_BINUTILS"

	ln -sf /usr/bin/arm-linux-gnueabihf-as \
		"${MODERN_BINUTILS}/as"

	ln -sf /usr/bin/arm-linux-gnueabihf-ld \
		"${MODERN_BINUTILS}/ld"
}

main
