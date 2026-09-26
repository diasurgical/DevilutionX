%define debug_package %{nil}
%bcond_with shareware

Name:           devilutionx
Version:        1.5.5
Release:        %{?_release}%{!?_release:5}%{?dist}
Summary:        Open source implementation of the Diablo 1 game engine
%if %{with shareware}
License:        LicenseRef-Sustainable-Use AND LicenseRef-Discord-Game-SDK AND LicenseRef-Shareware-Asset
%else
License:        LicenseRef-Sustainable-Use AND LicenseRef-Discord-Game-SDK
%endif
URL:            https://github.com/diasurgical/DevilutionX

# FIXED FULLY-VENDORED RELEASE LINK:
Source0:        https://github.com/diasurgical/DevilutionX/releases/download/%{version}/devilutionx-src-fully-vendored.tar.xz
Source2:        https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip
%if %{with shareware}
# Optional game data: enable only after confirming redistribution rights.
Source1:        https://github.com/diasurgical/devilutionx-assets/releases/download/v5/spawn.mpq
%global spawn_mpq_sha256 64427cd7c1ba904eaa2e0031c16a6b136d0ecef9abc888c5ff8344b459356e38
%endif

BuildRequires:  cmake >= 3.22
BuildRequires:  gcc-c++
BuildRequires:  patch
BuildRequires:  unzip
BuildRequires:  libstdc++-static
BuildRequires:  SDL2-devel
BuildRequires:  SDL2_image-devel
BuildRequires:  zlib-devel
BuildRequires:  libsodium-devel
BuildRequires:  bzip2-devel
BuildRequires:  alsa-lib-devel
BuildRequires:  pipewire-devel
BuildRequires:  pulseaudio-libs-devel
# Graphics extension dependencies required by the vendored SDL2 compilation block
BuildRequires:  libXext-devel
BuildRequires:  libX11-devel
BuildRequires:  libXrandr-devel
BuildRequires:  libXi-devel
BuildRequires:  libXcursor-devel
BuildRequires:  libXinerama-devel

%description
DevilutionX is a modern open-source engine recreation for Diablo 1 and its expansion,
Hellfire. The engine source is licensed under the Sustainable Use License, which
limits use and distribution to non-commercial purposes. The Discord Game SDK has
separate terms; review those terms before redistributing this build. No original
commercial game data is included. The optional shareware MPQ is excluded by default.
Enabling it does not grant redistribution permission: confirm the asset's terms and
your rights before building or distributing with --with shareware.

%prep
# Targets the root extraction folder name inside the vendored archive format
%autosetup -n devilutionx-src-full-%{version}
mkdir -p dist/discordsrc-src
unzip -q %{SOURCE2} -d dist/discordsrc-src
patch -d dist/discordsrc-src -p1 < 3rdParty/discord/fixes.patch

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DDEVILUTIONX_SYSTEM_SDL2=ON \
    -DDEVILUTIONX_SYSTEM_SDL_IMAGE=ON \
    -DFETCHCONTENT_SOURCE_DIR_DISCORDSRC="%{_builddir}/devilutionx-src-full-%{version}/dist/discordsrc-src" \
    -DDISCORD_INTEGRATION=ON \
    -DBUILD_TESTING=OFF \
    -DSDL_PIPEWIRE=ON \
    -DCPACK=ON \
    -DDEBUG=OFF
%cmake_build

%install
%cmake_install
%if %{with shareware}
echo "%{spawn_mpq_sha256}  %{SOURCE1}" | sha256sum -c -
install -Dm0644 %{SOURCE1} %{buildroot}%{_datadir}/diasurgical/devilutionx/spawn.mpq
%endif
if [ "%{_libdir}" != "/usr/lib" ] && [ -f %{buildroot}/usr/lib/discord_game_sdk.so ]; then
    mkdir -p %{buildroot}%{_libdir}
    mv %{buildroot}/usr/lib/discord_game_sdk.so %{buildroot}%{_libdir}/
fi

%files
%license LICENSE.md
%doc README.md
%{_bindir}/devilutionx
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/*/apps/*.png
%{_datadir}/metainfo/*.xml
# TRACK THE INSTALLED GAME ASSETS:
%{_datadir}/diasurgical/devilutionx/README.txt
%{_datadir}/diasurgical/devilutionx/devilutionx.mpq
%if %{with shareware}
%{_datadir}/diasurgical/devilutionx/spawn.mpq
%endif
%{_libdir}/discord_game_sdk.so

%changelog
* Fri Sep 25 2026 sonik.bhoom <sonik.bhoom@users.noreply.github.com> - 1.5.5-5
- Replace the inaccurate Unlicense declaration with custom license references.
- Exclude shareware data by default and add an opt-in, pinned, hash-verified asset source.
- Document non-commercial restrictions and that the build switch does not grant asset rights.

* Mon Sep 21 2026 sonik.bhoom <sonik.bhoom@users.noreply.github.com> - 1.5.5-4
- Use Fedora system SDL2 and SDL2_image packages instead of the vendored SDL2 sources.
- Enable SDL2 PipeWire support with Fedora 43 development libraries.
- Verify successful RPM build and working runtime audio.

* Mon Sep 21 2026 sonik.bhoom <sonik.bhoom@users.noreply.github.com> - 1.5.5-3
- Added Discord SDK source extraction and applied the bundled compatibility patch during preparation.
- Enabled Discord integration and packaged discord_game_sdk.so for the target library directory.
- Added CPack component handling for 64-bit RPM builds.

* Sun Sep 20 2026 sonik.bhoom <sonik.bhoom@users.noreply.github.com> - 1.5.5-2
- Added Fedora audio development dependencies and disabled the incompatible SDL2 PipeWire backend.
- Enabled Discord integration by unpacking the Discord SDK into the vendored source tree.
- Applied the bundled Discord SDK compatibility patch during package preparation.
- Packaged the Diablo shareware spawn.mpq asset in the system data directory.
- Build and runtime validation completed with GitHub Copilot assistance.