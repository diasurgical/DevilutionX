%define debug_package %{nil}

Name:           devilutionx
Version:        1.5.5
Release:        %{?_release}%{!?_release:1}%{?dist}
Summary:        Open source implementation of the Diablo 1 game engine
License:        Unlicense
URL:            https://github.com/diasurgical/DevilutionX

# FIXED FULLY-VENDORED RELEASE LINK:
Source0:        https://github.com/diasurgical/DevilutionX/releases/download/%{version}/devilutionx-src-fully-vendored.tar.xz
Source1:        https://github.com/diasurgical/devilutionx-assets/releases/latest/download/spawn.mpq

BuildRequires:  cmake >= 3.22
BuildRequires:  gcc-c++
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
Hellfire. Note that this package only includes the engine binaries. You must provide
your own legal copy of the original DIABDAT.MPQ asset file to play.

%prep
# Targets the root extraction folder name inside the vendored archive format
%autosetup -n devilutionx-src-full-%{version}

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DDISCORD_INTEGRATION=OFF \
    -DBUILD_TESTING=OFF \
    -DSDL_PIPEWIRE=OFF \
    -DCPACK=ON
%cmake_build

%install
%cmake_install
install -Dm0644 %{SOURCE1} %{buildroot}%{_datadir}/diasurgical/devilutionx/spawn.mpq

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
%{_datadir}/diasurgical/devilutionx/spawn.mpq

%changelog
* Sun Sep 20 2026 sonik.bhoom <sonik.bhoom@users.noreply.github.com> - 1.5.5-2
- Added Fedora audio development dependencies and disabled the incompatible SDL2 PipeWire backend.
- Disabled Discord integration because the fully-vendored source archive omitted the Discord SDK.
- Packaged the Diablo shareware spawn.mpq asset in the system data directory.
- Build and runtime validation completed with GitHub Copilot assistance.