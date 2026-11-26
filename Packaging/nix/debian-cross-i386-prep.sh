#!/usr/bin/env bash
set -euo pipefail
set -x

FLAVOR="$(lsb_release -sc)"

if dpkg-vendor --derives-from Ubuntu; then
	if ! grep -q '^Architectures:' /etc/apt/sources.list.d/ubuntu.sources; then
		sudo sed -i '/^Suites:/a Architectures: amd64' /etc/apt/sources.list.d/ubuntu.sources
	fi
	# amd64 is listed here too: Multi-Arch: same packages (e.g. libxi6) must have the same version for
	# amd64 and i386, and the runner's mirror can lag behind archive.ubuntu.com on security updates.
	sudo tee /etc/apt/sources.list.d/i386.sources <<LIST
Types: deb deb-src
URIs: http://archive.ubuntu.com/ubuntu/
Suites: ${FLAVOR} ${FLAVOR}-updates ${FLAVOR}-backports ${FLAVOR}-security
Architectures: amd64 i386
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
LIST
fi

PACKAGES=(
  cmake git smpq gettext
  libsdl2-dev:i386 libsdl2-image-dev:i386 libsodium-dev:i386
  libpng-dev:i386 libbz2-dev:i386 libspeechd-dev:i386
)

if (( $# < 1 )) || [[ "$1" != --no-gcc ]]; then
  PACKAGES+=(g++-multilib)
fi

sudo dpkg --add-architecture i386
sudo apt-get update
if [[ $FLAVOR == "noble" ]]; then
	# https://github.com/actions/runner-images/issues/12091#issuecomment-2844588977
	sudo apt-mark hold grub-common grub-efi-amd64-bin python3 python3-apt shim-signed
fi
sudo apt-get install -y "${PACKAGES[@]}"

