#!/bin/sh

apt-get update
apt-get install bison flex curl gcc g++ make texinfo zlib1g-dev tar bzip2 gzip xz-utils unzip python3-dev m4 dos2unix nasm
apt-get install cmake

git clone https://github.com/jwt27/build-gcc.git
cd build-gcc/

./build-djgpp.sh --prefix=$HOME/.local binutils gcc-14.2.0 djgpp-cvs

mkdir ~/.local/i386-pc-msdosdjgpp/include

cd ..

source $HOME/.local/bin/i386-pc-msdosdjgpp-setenv

git clone https://github.com/AJenbo/SDL.git
cd SDL/
git checkout patch-1
autoreconf -fi
./configure --host=i386-pc-msdosdjgpp --prefix=$HOME/.local/i386-pc-msdosdjgpp --disable-shared --enable-static --enable-video-svga --enable-timer-dos --enable-uclock
make -j$(nproc)
make install
cd ..

mkdir dos-build
cd dos-build/
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_TOOLCHAIN_FILE=../CMake/platforms/djgpp.toolchain.cmake          -DTARGET_PLATFORM=dos -DNOSOUND=ON
make -j$(nproc)
