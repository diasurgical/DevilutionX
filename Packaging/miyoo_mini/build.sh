#!/usr/bin/env bash
set -euo pipefail

declare -r PACKAGING_DIR=`cd -- "$(dirname "$0")" >/dev/null 2>&1; pwd -P`
declare -r CFLAGS="-O3 -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve -Wall"
declare -r LDFLAGS="-lSDL -lmi_sys -lmi_gfx -s -lSDL -lSDL_image"
declare -r BUILD_DIR="build-miyoo-mini"
declare -r MIYOO_CUSTOM_SDL_REPO="https://github.com/Brocky/SDL-1.2-miyoo-mini.git"
declare -r MIYOO_CUSTOM_SDL_BRANCH="miniui-miyoomini"

main() {
	# ensure we are in devilutionx root
	cd "$PACKAGING_DIR/../.."

	rm -f "$BUILD_DIR/CMakeCache.txt"
	cmake_configure -DCMAKE_BUILD_TYPE=Release
	cmake_build
	package_onion
	package_miniui
}

cmake_configure() {
	cmake -S. -B"$BUILD_DIR" \
		-DTARGET_PLATFORM=miyoo_mini \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE="${PACKAGING_DIR}/toolchainfile.cmake" \
		-DBUILD_TESTING=OFF \
		-DDISABLE_DEMOMODE=ON \
		"$@"
}

cmake_build() {
	cmake --build "$BUILD_DIR" -j $(getconf _NPROCESSORS_ONLN)
}

build_custom_sdl() {
	# make clean folder for custom SDL build
	rm -rf $BUILD_DIR/CustomSDL
	mkdir  $BUILD_DIR/CustomSDL

	# clone the repo and build the lib
	cd $BUILD_DIR/CustomSDL
	git clone $MIYOO_CUSTOM_SDL_REPO --branch $MIYOO_CUSTOM_SDL_BRANCH --single-branch .

	PATH="/opt/miyoomini-toolchain/usr/bin:${PATH}:/opt/miyoomini-toolchain/usr/arm-linux-gnueabihf/sysroot/bin" \
	CROSS_COMPILE=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf- \
	PREFIX=/opt/miyoomini-toolchain/usr/arm-linux-gnueabihf/sysroot/usr \
	UNION_PLATFORM=miyoomini \
	./make.sh

	# change back to devilutionx root
	cd "$PACKAGING_DIR/../.."
	cp -rfL "$BUILD_DIR/CustomSDL/build/.libs/libSDL-1.2.so.0" "$BUILD_DIR/OnionOS/Roms/PORTS/Games/Diablo (DevilutionX)/lib/libSDL-1.2.so.0"
}

prepare_onion_skeleton() {
	mkdir -p $BUILD_DIR/OnionOS

	# Copy basic skeleton
	cp -rf  Packaging/miyoo_mini/skeleton_OnionOS/* $BUILD_DIR/OnionOS

	# ensure lib dir for custom SDL
	mkdir -p "$BUILD_DIR/OnionOS/Roms/PORTS/Games/Diablo (DevilutionX)/lib"
}

package_onion() {
	prepare_onion_skeleton
	build_custom_sdl
	# copy asset mpqs
	local -r out="${BUILD_DIR}/OnionOS/Roms/PORTS/Games/Diablo (DevilutionX)"
	cp -f $BUILD_DIR/devilutionx.mpq "${out}/devilutionx.mpq"
	mkdir -p "${out}/mods"
	cp -f $BUILD_DIR/mods/hf.mpq "${out}/mods/hf.mpq"
	# copy executable
	cp -f $BUILD_DIR/devilutionx "${out}/devilutionx"

	rm -f $BUILD_DIR/onion.zip

	cd $BUILD_DIR/OnionOS
	zip -r ../devilutionx-miyoo-mini-onion-os.zip .
	cd "$PACKAGING_DIR/../.."
}

prepare_miniui_skeleton() {
	mkdir -p $BUILD_DIR/MiniUI

	# copy basic skeleton
	cp -rf  Packaging/miyoo_mini/skeleton_MiniUI/* $BUILD_DIR/MiniUI

	# ensure devilutionx asset dir
	mkdir -p $BUILD_DIR/MiniUI/Diablo/assets
}

package_miniui() {
	prepare_miniui_skeleton
	# copy assets
	cp -rf $BUILD_DIR/assets/* $BUILD_DIR/MiniUI/Diablo/assets
	# copy executable
	cp -f $BUILD_DIR/devilutionx $BUILD_DIR/MiniUI/Diablo/devilutionx

	rm -f $BUILD_DIR/miniui.zip

	cd $BUILD_DIR/MiniUI
	zip -r ../devilutionx-miyoo-mini-miniui.zip .
	cd "$PACKAGING_DIR/../.."
}

main
