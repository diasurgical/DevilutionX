#!/usr/bin/env bash
set -euo pipefail
set -x

BUILD_DIR="${1-build}"

mkdir -p "${BUILD_DIR}/package"

PKG_PATH=("${BUILD_DIR}/_CPack_Packages/Linux/7Z/"devilutionx-*/)
PKG_PATH="${PKG_PATH[@]}"
PKG_PATH="${PKG_PATH%/}"

cp "${PKG_PATH}/bin/devilutionx" "${BUILD_DIR}/package/devilutionx"
if [[ -f "${PKG_PATH}/lib/discord_game_sdk.so" ]]; then
	cp "${PKG_PATH}/lib/discord_game_sdk.so" "${BUILD_DIR}/package/"
	cat <<'SH' > "${BUILD_DIR}/package/devilutionx.sh"
#!/bin/sh
BASEDIR="$(dirname "$(realpath "$0")")"
LD_LIBRARY_PATH="$BASEDIR" "$BASEDIR"/devilutionx "$@"
SH
	chmod +x "${BUILD_DIR}/package/devilutionx.sh"
fi

cp "${BUILD_DIR}/devilutionx" "${BUILD_DIR}/package/devilutionx"
cp "${BUILD_DIR}/devilutionx.mpq" "${BUILD_DIR}/package/devilutionx.mpq"
mkdir -p "${BUILD_DIR}/package/mods"
cp "${BUILD_DIR}/mods/Hellfire.mpq" "${BUILD_DIR}/package/mods/Hellfire.mpq"

if which dpkg 2>/dev/null; then
	cp "${BUILD_DIR}/"devilutionx*.deb "${BUILD_DIR}/package/devilutionx.deb"
fi
if which rpmbuild; then
	cp "${BUILD_DIR}/"devilutionx*.rpm "${BUILD_DIR}/package/devilutionx.rpm"
fi

cp ./Packaging/nix/README.txt "${BUILD_DIR}/package/README.txt"
cp ./Packaging/resources/LICENSE.CC-BY.txt "${BUILD_DIR}/package/LICENSE.CC-BY.txt"
cp ./Packaging/resources/LICENSE.OFL.txt "${BUILD_DIR}/package/LICENSE.OFL.txt"
cd "${BUILD_DIR}/package/" && find -type f | xargs tar -cavf ../../devilutionx.tar.xz
