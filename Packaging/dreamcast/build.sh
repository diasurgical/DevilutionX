#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-dreamcast"
CD_ROOT="${SCRIPT_DIR}/cd_root"

ELF_NAME="devilutionx.elf"
STRIPPED_NAME="devilutionx-stripped.elf"
BIN_NAME="1ST_READ.BIN"

KOS_BASE="${KOS_BASE:-/opt/toolchains/dc/kos}"
KOS_ENV="${KOS_ENV:-${KOS_BASE}/environ.sh}"
MKDCDISC="${MKDCDISC:-/opt/toolchains/dc/antiruins/tools/mkdcdisc}"

if [ -f "${KOS_ENV}" ]; then
    # shellcheck disable=SC1090
    # Temporarily disable -u because KOS environ may have unbound variables
    set +u
    source "${KOS_ENV}"
    set -u
else
    echo "Error: KOS environ not found at ${KOS_ENV}"
    exit 1
fi

if [ ! -x "${MKDCDISC}" ]; then
    echo "Error: mkdcdisc not found at ${MKDCDISC}"
    exit 1
fi

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

cmake_args=(
    -S "${ROOT_DIR}"
    -B "${BUILD_DIR}"
    -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/CMake/platforms/dreamcast.toolchain.cmake"
)

if command -v ninja >/dev/null 2>&1; then
    cmake_args+=(-G Ninja)
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

if [ ! -f "${BUILD_DIR}/${ELF_NAME}" ]; then
    echo "Error: ${BUILD_DIR}/${ELF_NAME} not found after build"
    exit 1
fi

sh-elf-strip -s "${BUILD_DIR}/${ELF_NAME}" -o "${BUILD_DIR}/${STRIPPED_NAME}"

mkdir -p "${CD_ROOT}"
cp "${BUILD_DIR}/${STRIPPED_NAME}" "${CD_ROOT}/${BIN_NAME}"
cp "${BUILD_DIR}/devilutionx.mpq" "${CD_ROOT}/devilutionx.mpq"

"${MKDCDISC}" -e "${CD_ROOT}/${BIN_NAME}" \
    -D "${CD_ROOT}" \
    -o "${SCRIPT_DIR}/devilutionx-playable.cdi" \
    -n "DevilutionX"

echo "CDI: ${SCRIPT_DIR}/devilutionx-playable.cdi"
