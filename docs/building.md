# Building from Source

DevilutionX provides a source package with the release (`devilutionx-src.tar.xz`) that contains
all the dependencies that must be vendored, the version information, and `devilutionx.mpq`.
This is the version most appropriate for packaging DevilutionX for Linux distributions.
For other use cases, use the git repository.

<details><summary>Linux</summary>

Note that ```pkg-config``` is an optional dependency for finding libsodium, although we have a fallback if necessary.

### Installing dependencies on Debian and Ubuntu

```
sudo apt-get install cmake g++ libsdl2-dev libsodium-dev libpng-dev libbz2-dev libgtest-dev libgmock-dev libbenchmark-dev libsdl2-image-dev libfmt-dev
```

### If you want to build the translations (optional)

```
sudo apt-get install gettext poedit
```

### If you want to build the devilutionX.mpq File (optional)

```
sudo apt-get install smpq
```

### Installing dependencies on Fedora

```
sudo dnf install cmake gcc-c++ glibc-devel libstdc++-static SDL2-devel SDL2_image-devel libsodium-devel libpng-devel bzip2-devel gmock-devel gtest-devel google-benchmark-devel libasan libubsan fmt-devel
```

### Compiling

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(getconf _NPROCESSORS_ONLN)
```

### Cross-compiling for arm64 (aarch64) on Debian or Ubuntu

First, set up the dependencies for cross-compilation:

```bash
Packaging/nix/debian-cross-aarch64-prep.sh
```

Then, build DevilutionX using the cross-compilation CMake toolchain file:

```bash
cmake -S. -Bbuild-aarch64-rel \
  -DCMAKE_TOOLCHAIN_FILE=../CMake/platforms/aarch64-linux-gnu.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCPACK=ON \
  -DDEVILUTIONX_SYSTEM_LIBFMT=OFF
cmake --build build-aarch64-rel -j $(getconf _NPROCESSORS_ONLN) --target package
```

</details>

<details><summary>macOS</summary>

Make sure you have [Homebrew](https://brew.sh/) installed, then run:

```bash
brew bundle install
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(sysctl -n hw.physicalcpu)
```

</details>
<details><summary>iOS</summary>

Make sure you have [Homebrew](https://brew.sh/) installed, then run:

```bash
brew install cmake
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=../CMake/Platforms/ios.toolchain.cmake  -DENABLE_BITCODE=0 -DPLATFORM=OS64
cmake --build build -j $(sysctl -n hw.physicalcpu) --config Release
cd build
rm -rf Payload
mkdir -p Payload
mv devilutionx.app Payload
zip -r devilutionx.ipa Payload
```

For testing with the Simulator instead run the following:

```bash
cmake -S. -Bbuild -G Xcode -DCMAKE_TOOLCHAIN_FILE=../CMake/Platforms/ios.toolchain.cmake -DPLATFORM=SIMULATOR64
```

Then open the generated Xcode project and run things from there.
</details>
<details><summary>FreeBSD</summary>

### Installing dependencies

```
pkg install cmake sdl2 libsodium libpng bzip2 googletest
```

### Compiling

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(sysctl -n hw.ncpu)
```

</details>
<details><summary>NetBSD</summary>

### Installing dependencies

```
pkgin install cmake SDL2 libsodium libpng bzip2 googletest
```

### Compiling

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(sysctl -n hw.ncpu)
```

</details>

<details><summary>OpenBSD</summary>

### Installing dependencies

```
pkg_add cmake sdl2 libsodium png bzip2 gmake gtest
```

### Compiling

```bash
cmake -S. -Bbuild -DCMAKE_MAKE_PROGRAM=gmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(sysctl -n hw.ncpuonline)
```

</details>

<details><summary>Windows via MinGW</summary>

<details><summary>Installing Windows Subsystem for Linux</summary>

Note: We currently recommend using Ubuntu 24.04 for the MinGW build. The following instructions will install the recommended version of Ubuntu on WSL.

If you are building on Windows and do not have WSL already setup this will install WSL and Ubuntu (Requires Windows 10 2004 or higher or Windows 11)

In an Administrator Command Prompt or Powershell

```wsl --install -d Ubuntu-24.04```

Reboot

Wait for Command Prompt to pop up and say installing when complete enter your new Linux password

You can launch WSL anytime by typing wsl or ubuntu in a Command Prompt or Powershell or in the Start Menu launch the Ubuntu App

### Setup git and clone DevilutionX

In a WSL terminal run these commands to get the source code for DevilutionX

```
sudo apt-get install git
git clone https://github.com/diasurgical/devilutionx
cd devilutionx
```

</details>

### Installing dependencies on WSL, Debian and Ubuntu

#### MinGW build tools

```bash
# Install MinGW build tools
sudo apt-get update
sudo apt-get install cmake git libz-mingw-w64-dev mingw-w64 mingw-w64-tools smpq wget
```

<details><summary>MinGW 32-bit</summary>

The 32-bit build depends on the 32-bit MinGW Development Libraries for [SDL2](https://www.libsdl.org/download-2.0.php) and [libsodium](https://github.com/jedisct1/libsodium/releases) as well as headers for [zlib](https://zlib.net/zlib-1.2.12.tar.gz). These dependencies will need to be placed in the appropriate subfolders under `/usr/i686-w64-mingw32`.

When linking zlib, libpng will always prefer dynamically linking with `libz.dll.a` if it can be found. We recommend renaming or deleting `libz.dll.a` to force libpng to use static linkage. This will prevent errors about missing dlls when you attempt to run the game.

These can be done automatically by running [`Packaging/windows/mingw-prep.sh`](/Packaging/windows/mingw-prep.sh).

Note: If your `i686-w64-mingw32` directory is not in `/usr` (e.g. when on
Debian), the mingw-prep scripts and the CMake command won't work. You need
adjust the mingw-prep scripts and pass `-DCROSS_PREFIX=/path` to CMake to set
the path to the parent of the `i686-w64-mingw32` directory.

```bash
# Download the 32-bit development libraries for SDL2 and libsodium
# as well as the headers for zlib and place them in subfolders under
# /usr/i686-w64-mingw32
Packaging/windows/mingw-prep.sh
```

### Compiling

By compiling the `package` target, the build will produce the `devilutionx.zip` archive which should contain all the dlls necessary to run the game. If you encounter any errors suggesting a dll is missing, try extracting the dlls from the zip archive.

```bash
# Configure the project to disable unit tests,
# statically link bzip2 and libsodium,
# and enable Discord integration
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=../CMake/platforms/mingwcc.toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DDEVILUTIONX_SYSTEM_BZIP2=OFF \
    -DDEVILUTIONX_STATIC_LIBSODIUM=ON -DDISCORD_INTEGRATION=ON

# Build the "package" target which produces devilutionx.zip
# containing all the necessary dlls to run the game
cmake --build build -j $(getconf _NPROCESSORS_ONLN) --target package
```

</details>

<details open><summary>MinGW 64-bit</summary>

The 64-bit build depends on the 64-bit MinGW Development Libraries of [SDL2](https://www.libsdl.org/download-2.0.php) and [libsodium](https://github.com/jedisct1/libsodium/releases) as well as headers for [zlib](https://zlib.net/zlib-1.2.12.tar.gz). These dependencies will need to be placed in the appropriate subfolders under `/usr/x86_64-w64-mingw32`.

When linking zlib, libpng will always prefer dynamically linking with `libz.dll.a` if it can be found. We recommend renaming or deleting `libz.dll.a` to force libpng to use static linkage. This will prevent errors about missing dlls when you attempt to run the game.

These can be done automatically by running [`Packaging/windows/mingw-prep64.sh`](/Packaging/windows/mingw-prep64.sh).

Note: If your `x86_64-w64-mingw32` directory is not in `/usr` (e.g. when
on Debian), the mingw-prep scripts and the CMake command won't work. You need
adjust the mingw-prep scripts and pass `-DCROSS_PREFIX=/path` to CMake to set
the path to the parent of the `x86_64-w64-mingw32` directory.

```bash
# Download the 64-bit development libraries for SDL2 and libsodium
# as well as the headers for zlib and place them in subfolders under
# /usr/x86_64-w64-mingw32
Packaging/windows/mingw-prep64.sh
```

### Compiling

```bash
# Configure the project to disable unit tests,
# statically link bzip2 and libsodium,
# and enable Discord integration
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=../CMake/platforms/mingwcc64.toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DDEVILUTIONX_SYSTEM_BZIP2=OFF \
    -DDEVILUTIONX_STATIC_LIBSODIUM=ON -DDISCORD_INTEGRATION=ON

# Build the "package" target which produces devilutionx.zip
# containing all the necessary dlls to run the game
cmake --build build -j $(getconf _NPROCESSORS_ONLN) --target package
```

</details>

</details>

<details><summary>Windows via Visual Studio</summary>

### Installing dependencies

Make sure to install the workload `Desktop development with C++` and the individual components `C++ CMake tools for Windows` and `Windows SDK` for Visual Studio.
*Note: `Windows SDK` component should match your Windows build version.*

Install [Git for Windows](https://gitforwindows.org/)

Run the following commands in Command Prompt or Powershell

~~~ bat
git clone https://github.com/microsoft/vcpkg
cd vcpkg
bootstrap-vcpkg.bat
vcpkg integrate install
~~~

If you need additional instructions for vcpkg you can find the documentation [here](https://github.com/microsoft/vcpkg#quick-start-windows).

### If you want to build the devilutionX.mpq File (optional)

In order to build devilutionx.mpq, install smpq from https://launchpad.net/smpq/trunk/1.6/+download/SMPQ-1.6-x86_64.exe.
The location of this tool will need to be [added to the system's PATH environment variable](https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/).

### Compiling

* **Through CMake Presets in Visual Studio (Recommended)**

1. Go to `File -> Open -> Folder` and select the devilutionx root folder. CMake will automatically detect the project and load the CMakePresets.json file. This may take several minutes.
2. Select a configuration from the configuration dropdown (e.g., `x64 Debug`).
3. Select `devilutionx.exe` from the Startup Item dropdown.
4. Select `Build devilutionx.exe` from the `Build` menu.

* **Through GCC/WSL in Visual Studio**

1. Ensure the WSL environment has the build pre-requisites for both devilutionX (see "Installing Dependencies on Debian and Ubuntu" under the "Linux" section above) and [WSL remote development](https://docs.microsoft.com/en-us/cpp/linux/connect-to-your-remote-linux-computer?view=msvc-160#connect-to-wsl).
2. Select the `WSL-GCC-x64-Debug` configuration.
3. Select `Build devilution` from the `Build` menu.

* **Through cmake-gui**

1. Input the path to devilutionx source directory at `Where is the source code:` field.
2. Input the path where the binaries would be placed at `Where to build the binaries:` field. If you want to place them inside source directory it's preferable to do so inside directory called `build` to avoid the binaries being added to the source tree.
3. It's recommended to input `Win32` in `Optional Platform for Generator`, otherwise it will default to x64 build.
4. In case you're using `vcpkg` select `Specify toolchain file for cross-compiling` and select the file `scripts/buildsystems/vcpkg.cmake` from `vcpkg` directory otherwise just go with `Use default native compilers`.
5. In case you need to select any paths to dependencies manually do this right in cmake-gui window.
6. Press `Generate` and open produced `.sln` file using Visual Studio.
7. Use build/debug etc. commands inside Visual Studio Solution like with any normal Visual Studio project.

</details>

<details><summary>Android</summary>

### Installing dependencies

Install [Android Studio](https://developer.android.com/studio)
After first launch configuration, go to "Configure -> SDK Manager -> SDK Tools".
Select "NDK (Side by side)" and "CMake" checkboxes and click "OK".

### Compiling

Click "Open Existing Project" and choose "android-project" folder in DevilutionX root folder.
Wait until Gradle sync is completed.
In Android Studio, go to "Build -> Make Project" or use the shortcut Ctrl+F9
You can find the compiled APK in `/android-project/app/build/outputs/apk/`
</details>

<details><summary>Nintendo Switch</summary>

### Installing dependencies

https://devkitpro.org/wiki/Getting_Started

- Install (dkp-)pacman: https://devkitpro.org/wiki/devkitPro_pacman

- Install required packages with (dkp-)pacman:

```
sudo (dkp-)pacman -S --needed - < Packaging/switch/packages.txt
```

- Install smpq (if building from git or a source archive without devilutionx.mpq)
  DevilutionX requires some core assets to render UI elements and fonts even if game data is not available. While some
  platforms can load this from the filesystem as loose files the switch build currently only supports bundling the mpq
  archive inside the nro. If you're building DevilutionX on a supported platform a prebuilt binary may be available from
  your package distribution system (e.g. `sudo apt install smpq` or `yum install smpq`), on windows you can
  [download the latest version from Launchpad.net](https://launchpad.net/smpq/+download), and unix based OSes can build
  from source using [tools/build_and_install_smpq.sh](../tools/build_and_install_smpq.sh)

### Compiling

```bash
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_ASSETS_MPQ=ON
cmake --build build -j $(getconf _NPROCESSORS_ONLN)
```

The nro-file will be generated in the build folder. Test with an emulator (RyuJinx) or real hardware.

[Nintendo Switch manual](manual/platforms/switch.md)
</details>

<details><summary>Nintendo 3DS</summary>

### Installing dependencies

https://devkitpro.org/wiki/Getting_Started

- Install (dkp-)pacman: https://devkitpro.org/wiki/devkitPro_pacman

- Install required packages with (dkp-)pacman:

```
sudo (dkp-)pacman -S \
    devkitARM general-tools 3dstools libctru \
    citro3d 3ds-sdl 3ds-libpng 3ds-bzip2 \
    3ds-cmake 3ds-pkg-config picasso 3dslink
```

- Download or compile [bannertool](https://github.com/diasurgical/bannertool/releases) and [makerom](https://github.com/jakcron/Project_CTR/releases)
    - Copy binaries to: `/opt/devkitpro/tools/bin/`

### Compiling

_If you are compiling using MSYS2, you will need to run `export MSYS2_ARG_CONV_EXCL=-D` before compiling.
Otherwise, MSYS will sanitize file paths in compiler flags which will likely lead to errors in the build._

```bash
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/3DS.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(getconf _NPROCESSORS_ONLN)
```

The output files will be generated in the build folder.

[Nintendo 3DS manual](/docs/manual/platforms/3ds.md)
</details>

<details><summary>PlayStation Vita</summary>

### Compiling

```bash
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=${VITASDK}/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

[PlayStation Vita manual](/docs/manual/platforms/vita.md)
</details>


<details><summary>Haiku</summary>

### Installing dependencies on 32 bit Haiku

```
pkgman install cmake_x86 devel:libsdl2_x86 devel:libsodium_x86 devel:libpng_x86 devel:bzip2_x86
```

### Installing dependencies on 64 bit Haiku

```
pkgman install cmake libsdl2_devel libsodium_devel libpng16_devel bzip2_devel gtest_devel gettext getconf
```

### Compiling on 32 bit Haiku

```bash
setarch x86 # Switch to secondary compiler toolchain (GCC8+)
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(getconf _NPROCESSORS_ONLN)
```

### Compiling on 64 bit Haiku

No setarch required, as there is no secondary toolchain on x86_64, and the primary is GCC8+

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(getconf _NPROCESSORS_ONLN)
```

</details>

<details><summary>OpenDingux / RetroFW</summary>

DevilutionX uses buildroot-based toolchains to build packages for OpenDingux and RetroFW.

For OpenDingux / RetroFW builds, `mksquashfs` needs to be installed on your machine.

To build, run the following command:

~~~ bash
TOOLCHAIN=<path/to/toolchain> Packaging/OpenDingux/build.sh <platform>
~~~

Replace `<platform>` with one of: `lepus`, `retrofw`, `rg99`, `rg350`, or `gkd350h`.

For example:

~~~ bash
TOOLCHAIN=/opt/gcw0-toolchain Packaging/OpenDingux/build.sh rg350
~~~

You can download the prebuilt toolchains for `x86_64` hosts here:

* OpenDingux: https://github.com/OpenDingux/buildroot/releases
* RetroFW: https://github.com/Poligraf/retrofw_buildroot_gcc11/releases

Remember to run `./relocate-sdk.sh` in the toolchain directory after unpacking it.

Alternatively, if you do not set `TOOLCHAIN`, the script will
download and compile a partial buildroot toolchain for you
(stored at `$HOME/buildroot-$PLATFORM-devilutionx`).
This requires 8 GiB+ disk space and takes a while.

End-user manuals are available [here](manual/platforms) and
in the package help section.

</details>

<details><summary>Clockwork PI GameShell</summary>

You can either call

~~~ bash
Packaging/cpi-gamesh/build.sh
~~~

to install dependencies and build the code.

Or you create a new directory under `/home/cpi/apps/Menu` and copy [the file](../Packaging/cpi-gamesh/__init__.py) there. After restarting the UI, you can download and compile the game directly from the device itself. See [the readme](../Packaging/cpi-gamesh/readme.md) for more details.
</details>

<details><summary>Amiga via Docker</summary>

### Build the container from the repo root

~~~ bash
docker build -f Packaging/amiga/Dockerfile -t devilutionx-amiga .
~~~

### Build DevilutionX Amiga binary

~~~ bash
docker run -u "$(id -u "$USER"):$(id -g "$USER")" --rm -v "${PWD}:/work" devilutionx-amiga
~~~

The command above builds DevilutionX in release mode.
For other build options, you can run the container interactively:

~~~ bash
docker run -u "$(id -u "$USER"):$(id -g "$USER")" -ti --rm -v "${PWD}:/work" devilutionx-amiga bash
~~~

See the `CMD` in `Packaging/amiga/Dockerfile` for reference.

To actually start DevilutionX, increase the stack size to 50KiB in Amiga.
You can do this by selecting the DevilutionX icon, then hold right mouse button and
select Icons -> Information in the top menu.
</details>

<details><summary>Emscripten</summary>

Emscripten port is a work in progress. It builds but does not do more than that currently.

To build, install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html), then run:

~~~ bash
emcmake cmake -S. -Bbuild-em -DCMAKE_BUILD_TYPE=Release
cmake --build build-em -j $(getconf _NPROCESSORS_ONLN)
~~~

To then run it:

~~~ bash
cd build-em
emrun index.html
~~~

</details>

<details><summary>Xbox One/Series</summary>

### Dependencies

* Windows 10
* CMake
* Git
* Visual Studio 2022 with the following packages installed:
    * C++ (v143) Universal Windows Platform tools
    * Windows 11 SDK (10.0.26100.0)
    * MSVC v143 - VS 2022 C++ x64/x86 build tools

_Note: Visual Studio Community Edition can be used._

### Building

Add the following to the PATH:

* CMake
* GIT
* VsDevCmd.bat

Run:

```
Packaging/xbox-one/build.bat
```

[Xbox One/Series manual](manual/platforms/xbox-one.md)
</details>

<details><summary>Miyoo Mini</summary>

Building for Miyoo Mini must be run from inside the [Toolchain Docker image](https://github.com/MiyooMini/union-toolchain).
Executing `Packaging/miyoo_mini/build.sh` will create the folder `build-miyoo-mini/SDROOT` which has the correct structure to be used with
OnionOS Port Collection.
</details>

<details><summary>macOS 10.4 Tiger</summary>

For macOS Tiger, DevilutionX can be compiled using the compiler and libraries from [MacPorts](https://www.macports.org/).

For PowerPC, you can use precompiled dependencies from here:

http://macports-tiger-ppc.glebm.com/

After installing MacPorts, run:

~~~ bash
# Some packages may require you to manually deactivate certain ports during installation.
# Remember to reactivate them after installing.
sudo port install curl curl-ca-bundle gcc14 cmake \
  libsdl12 libsdl_image libsodium bzip2 zlib lua54

# Set GCC 14 as the default GCC:
sudo port select --set gcc mp-gcc14
~~~

<!-- The following packages have issues so we use the vendored versions:
     libfmt11 google-benchmark gtest -->

Then, build DevilutionX:

~~~ bash
CC=gcc cmake -S. -Bbuild-rel -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCPACK=ON -DMACOSX_STANDALONE_APP_BUNDLE=ON
cmake --build build-rel -j "$(sysctl -n hw.ncpu)"

# `sudo` is required to produce a bundle with all the shared libraries.
sudo cmake --build build-rel --target package -j "$(sysctl -n hw.ncpu)"
~~~

To run tools from the `tools/` directory (only needed for development), you also need Python:

~~~ bash
sudo port install python312
sudo port select --set python python312
sudo port select --set python3 python312
~~~

</details>

<details><summary>DOS</summary>

You can build for DOS from Linux using DJGPP.

First, install / compile the dependencies (only needs to be done once):

~~~ bash
Packaging/windows/dos-prep.sh
~~~

Then, build DevilutionX:

~~~ bash
cmake -S. -Bbuild-dos -DCMAKE_TOOLCHAIN_FILE=CMake/platforms/djcpp.toolchain.cmake -DTARGET_PLATFORM="dos" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-dos -j $(getconf _NPROCESSORS_ONLN)
~~~

</details>

<details><summary><b>CMake build options</b></summary>

### General

- `-DCMAKE_BUILD_TYPE=Release` changed build type to release and optimize for distribution.
- `-DNONET=ON` disable network support, this also removes the need for the ASIO and Sodium.
- `-DUSE_SDL1=ON` build for SDL v1 instead of v2, not all features are supported under SDL v1, notably upscaling.
- `-DCMAKE_TOOLCHAIN_FILE=../CMake/platforms/linux_i386.toolchain..cmake` generate 32bit builds on 64bit platforms (remember to use the `linux32` command if on Linux).

### Debug builds

- `-DDEBUG=OFF` disable debug mode of the Diablo engine.
- `-DASAN=OFF` disable address sanitizer.
- `-DUBSAN=OFF` disable undefined behavior sanitizer.

</details>
