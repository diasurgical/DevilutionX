#!/usr/bin/env bash
set -euo pipefail
set -x

FLAVOR="$(lsb_release -sc)"

if dpkg-vendor --derives-from Ubuntu; then
	if ! grep -q '^Architectures:' /etc/apt/sources.list.d/ubuntu.sources; then
		sudo sed -i '/^Suites:/a Architectures: amd64' /etc/apt/sources.list.d/ubuntu.sources
	fi
	# amd64 also comes from archive.ubuntu.com: Multi-Arch: same packages (e.g. libxi6) must have the same
	# version for amd64 and arm64, and the runner's mirror can lag behind ports.ubuntu.com on security updates.
	sudo tee /etc/apt/sources.list.d/arm64.sources <<LIST
Types: deb deb-src
URIs: http://ports.ubuntu.com/
Suites: ${FLAVOR} ${FLAVOR}-updates ${FLAVOR}-backports ${FLAVOR}-security
Architectures: arm64
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: ${FLAVOR} ${FLAVOR}-updates ${FLAVOR}-backports ${FLAVOR}-security
Architectures: amd64
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
LIST
fi


PACKAGES=(
  cmake git smpq gettext dpkg-cross libc-dev-arm64-cross
  libsdl2-dev:arm64 libsdl2-image-dev:arm64 libsodium-dev:arm64
  libpng-dev:arm64 libbz2-dev:arm64
  libspeechd-dev:arm64
)

if (( $# < 1 )) || [[ "$1" != --no-gcc ]]; then
  PACKAGES+=(crossbuild-essential-arm64)
fi


sudo dpkg --add-architecture arm64
sudo apt-get update
if [[ $FLAVOR == "noble" ]]; then
	# https://github.com/actions/runner-images/issues/12091#issuecomment-2844588977
	sudo apt-mark hold grub-common grub-efi-amd64-bin python3 python3-apt shim-signed
fi
sudo apt-get install -y "${PACKAGES[@]}"
