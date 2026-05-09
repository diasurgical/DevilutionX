#!/bin/sh

# Downloads, builds, and installs SMPQ from source.
#
# Useful when developing on systems that do not have an SMPQ package.
# Compatible with Linux, *BSD, and macOS.
# Requires: cmake curl sed sudo zlib bzip2

set -ex

PARALLELISM="$(getconf _NPROCESSORS_ONLN)"
if [ "$PARALLELISM" = "undefined" ] && [ -f /usr/sbin/sysctl ]; then
	# On older OSX, such as 10.4, _NPROCESSOR_ONLN is not defined.
	if [ -z "$CC" ]; then
		# Tiger's default cc is too old to build smpq, so we default to gcc instead (e.g. from macports).
		export CC=gcc
	fi
	PARALLELISM="$(/usr/sbin/sysctl -n hw.ncpu)"
fi

STORMLIB_VERSION=e01d93cc8ae743cfe2da5450854c5d2e3a939265
STORMLIB_SRC="/tmp/StormLib-$STORMLIB_VERSION"
SMPQ_VERSION=1.7
SMPQ_SRC="/tmp/smpq-$SMPQ_VERSION"

# Download, build, and install the static version of StormLib, an SMPQ dependency, to the staging prefix.
if ! [ -d "$STORMLIB_SRC" ]; then
	curl -L -s "https://github.com/ladislav-zezula/StormLib/archive/${STORMLIB_VERSION}.tar.gz" | tar -C /tmp -xvzf -
fi

cmake -S"$STORMLIB_SRC" -B"$STORMLIB_SRC"/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/smpq-staging -DBUILD_SHARED_LIBS=OFF \
	-DWITH_STATIC=ON -DSTORM_BUILD_TESTS=OFF -DWITH_LIBTOMCRYPT=OFF
cmake --build "$STORMLIB_SRC"/build --config Release --target install -j"$PARALLELISM"

# Download, build, and install SMPQ.
if ! [ -d "$SMPQ_SRC" ]; then
	curl -L -s "https://launchpad.net/smpq/trunk/${SMPQ_VERSION}/+download/smpq_${SMPQ_VERSION}.orig.tar.gz" | tar -C /tmp -xvzf -

	# StormLib.a is C++ and must be linked with a C++ linker (e.g. via g++ instead of gcc).
	sed -i.bak -e '/^project/a\
file(GLOB_RECURSE CFILES "${CMAKE_SOURCE_DIR}/*.c")\
SET_SOURCE_FILES_PROPERTIES(${CFILES} PROPERTIES LANGUAGE CXX)' "$SMPQ_SRC"/CMakeLists.txt

	# StormLib is linked statically, so we need to add links its dynamic link dependencies to smpq itself.
	sed -i.bak -e 's|target_link_libraries(smpq ${STORMLIB_LIBRARY})|find_package(ZLIB REQUIRED)\
	find_package(BZip2 REQUIRED)\
	target_link_libraries(smpq ${STORMLIB_LIBRARY} ${ZLIB_LIBRARY} ${BZIP2_LIBRARIES})|' "$SMPQ_SRC"/CMakeLists.txt

	# Do not generate the manual.
	sed -i.bak -e 's|if(NOT CMAKE_CROSSCOMPILING)|if(FALSE)|' "$SMPQ_SRC"/CMakeLists.txt

	# Fix missing header includes.
	sed -i.bak -e '/#include <string.h>/a\
#include <stdio.h>' "$SMPQ_SRC/append.c"
	sed -i.bak -e '/#include <StormLib.h>/a\
#include <string.h>' "$SMPQ_SRC/remove.c"
fi

# The StormLib version check in SMPQ CMake is broken. We bypass it by passing the paths to StormLib explicitly.
cmake -S"$SMPQ_SRC" -B"$SMPQ_SRC"/build -DCMAKE_BUILD_TYPE=Release -DWITH_KDE=OFF -DCMAKE_PREFIX_PATH=/tmp/smpq-staging \
	-DSTORMLIB_INCLUDE_DIR=/tmp/smpq-staging/include -DSTORMLIB_LIBRARY=/tmp/smpq-staging/lib/libstorm.a
sudo cmake --build "$SMPQ_SRC"/build --config Release --target install -j"$PARALLELISM"
