#!/usr/bin/env bash
# Written by VTSTech (Nigel Todman) (veritas@vts-tech.org)
# AI Assisted by Super-Z (GLM-5.3-Flash)
# License: MIT

# ============================================================
#  install_shaders.sh - download the libretro slang shader
#  pack into shaders/ next to the game binary.
#  Linux equivalent of install_shaders.cmd.
#
#  Equivalent manual commands:
#    git clone https://github.com/libretro/slang-shaders shaders
#    cd shaders
#    git lfs pull
#
#  Requires: git and git-lfs. If either is missing the script
#  prints the install command for your distribution.
# ============================================================

# The game looks for shaders/ next to the executable, and this
# script ships beside the binary - so resolve to the script's
# own directory, not the caller's working directory.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)" || exit 1
cd "$SCRIPT_DIR" || exit 1

print_install_hint() {
    echo "Install them with your distribution's package manager:"
    if command -v apt-get >/dev/null 2>&1; then
        echo "  sudo apt-get install git git-lfs"
    elif command -v dnf >/dev/null 2>&1; then
        echo "  sudo dnf install git git-lfs"
    elif command -v pacman >/dev/null 2>&1; then
        echo "  sudo pacman -S --needed git git-lfs"
    elif command -v zypper >/dev/null 2>&1; then
        echo "  sudo zypper install git git-lfs"
    elif command -v apk >/dev/null 2>&1; then
        echo "  sudo apk add git git-lfs"
    else
        echo "  (unknown distribution - see https://git-lfs.com)"
    fi
}

echo "Checking for git / git-lfs ..."
MISSING=""
command -v git >/dev/null 2>&1 || MISSING="$MISSING git"
command -v git-lfs >/dev/null 2>&1 || MISSING="$MISSING git-lfs"
if [ -n "$MISSING" ]; then
    echo "[ERROR] This script needs the following, but not found:$MISSING"
    print_install_hint
    echo "Then re-run this script."
    exit 1
fi
git --version
git-lfs --version

# Make sure the LFS smudge filter is configured for this user
# (harmless if it was already set up).
git lfs install >/dev/null 2>&1 || echo "[WARN] git lfs install failed - continuing, LFS assets are pulled explicitly below"

echo "Wiping any existing shaders/ ..."
rm -rf ./shaders

echo "Downloading libretro slang shaders ..."
if ! git clone https://github.com/libretro/slang-shaders shaders; then
    echo "[ERROR] git clone failed - check your network connection and re-run this script."
    exit 1
fi

echo "Downloading PNG assets ..."
if ! (cd shaders && git lfs pull); then
    echo "[WARN] git lfs pull failed - presets that use PNG lookup textures"
    echo "       will not load. Re-run this script once the network is back."
fi

echo "Done - shaders/ is ready next to the game binary."
exit 0
