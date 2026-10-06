#!/usr/bin/env bash
set -euo pipefail
set -x

PACKAGES=(
  rpm pkg-config cmake git smpq gettext libsdl2-dev libsdl2-image-dev libsodium-dev
  libpng-dev libbz2-dev libspeechd-dev
)

if (( $# < 1 )) || [[ "$1" != --no-gcc ]]; then
  PACKAGES+=(g++)
fi

sudo apt-get update
sudo apt-get install -y "${PACKAGES[@]}"

