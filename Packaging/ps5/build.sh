#!/usr/bin/env bash

set -e
SCRIPTDIR="${BASH_SOURCE[0]}"
SCRIPTDIR="$(dirname "${SCRIPTDIR}")"

if [ -z "${PS5_PAYLOAD_SDK}" ]; then
    export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
fi

source "${PS5_PAYLOAD_SDK}/toolchain/prospero.sh"

${CMAKE} -DCMAKE_BUILD_TYPE=Release \
	 -DDISCORD_INTEGRATION=OFF \
	 -DBUILD_TESTING=OFF \
	 -DASAN=OFF \
	 -DUBSAN=OFF \
	 -DDISABLE_LTO=ON \
	 -DNOEXIT=ON \
	 -DNONET=OFF \
	 -DBUILD_ASSETS_MPQ=ON \
	 -DDEVILUTIONX_SYSTEM_SDL_IMAGE=OFF \
	 -B build-ps5 \
	 -S "${SCRIPTDIR}/../../"
${MAKE} -C build-ps5 -j $(getconf _NPROCESSORS_ONLN)

rm -rf build-ps5/DevilutionX
mkdir -p build-ps5/DevilutionX/mods

cp -r "${SCRIPTDIR}/sce_sys" build-ps5/DevilutionX/
cp "${SCRIPTDIR}/homebrew.js" build-ps5/DevilutionX/
cp "${SCRIPTDIR}/README.md" build-ps5/DevilutionX/
cp build-ps5/devilutionx.mpq build-ps5/DevilutionX/
cp build-ps5/mods/Hellfire.mpq build-ps5/DevilutionX/mods/Hellfire.mpq
cp build-ps5/devilutionx build-ps5/DevilutionX/devilutionx.elf

# Let github actions do this?
cd build-ps5
rm -f devilutionx-ps5.zip
zip -r devilutionx-ps5.zip DevilutionX
