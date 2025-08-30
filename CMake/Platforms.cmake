if(HAIKU)
  include(platforms/haiku)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|DragonFly|NetBSD")
  if(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    add_definitions(-D_NETBSD_SOURCE)
  else()
    add_definitions(-D_BSD_SOURCE)
    set(UBSAN OFF)
  endif()
  set(ASAN OFF)
  add_definitions(-DO_LARGEFILE=0 -Dstat64=stat -Dlstat64=lstat -Dlseek64=lseek -Doff64_t=off_t -Dfstat64=fstat -Dftruncate64=ftruncate)
endif()

set(TARGET_PLATFORM host CACHE STRING "Target platform")
set_property(CACHE TARGET_PLATFORM PROPERTY STRINGS host retrofw rg99 rg350 gkd350h cpigamesh miyoo_mini windows9x windowsXP)
if(TARGET_PLATFORM STREQUAL "retrofw")
  include(platforms/retrofw)
elseif(TARGET_PLATFORM STREQUAL "rg99")
  include(platforms/rg99)
elseif(TARGET_PLATFORM STREQUAL "rg350")
  include(platforms/rg350)
elseif(TARGET_PLATFORM STREQUAL "gkd350h")
  include(platforms/gkd350h)
elseif(TARGET_PLATFORM STREQUAL "cpigamesh")
  include(platforms/cpigamesh)
elseif(TARGET_PLATFORM STREQUAL "lepus")
  include(platforms/lepus)
elseif(TARGET_PLATFORM STREQUAL "miyoo_mini")
  include(platforms/miyoo_mini)
elseif(TARGET_PLATFORM STREQUAL "windows9x")
  include(platforms/windows9x)
elseif(TARGET_PLATFORM STREQUAL "windowsXP")
  include(platforms/windowsXP)
elseif(TARGET_PLATFORM STREQUAL "dos")
  include(platforms/dos)
elseif(WIN32)
  include(platforms/windows)
endif()

if(NINTENDO_SWITCH)
  include(platforms/switch)
endif()

if(AMIGA)
  include(platforms/amiga)
endif()

if(NINTENDO_3DS)
  include(platforms/n3ds)
endif()

if(VITA)
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  include(platforms/vita)
endif()

if(PS4)
  include(platforms/ps4)
endif()

if(ANDROID)
  include(platforms/android)
endif()

if(IOS)
  include(platforms/ios)
endif()

if(EMSCRIPTEN)
  include(platforms/emscripten)
endif()

if(UWP_LIB)
  include(platforms/uwp_lib)
endif()

if(NXDK)
  include(platforms/xbox_nxdk)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  # Some notable Darwin (macOS kernel) versions are:
  #   8.x == macOS 10.4 (Tiger)
  #   9.x == macOS 10.5 (Leopard)
  #
  # Importantly, a lot of the APIs first appeared in version 9, including
  # the feature availability API (the <Availability.h> header).
  #
  # For Darwin 8 and below, we have to rely on the kernel version
  # to detect available APIs.
  string(REGEX REPLACE "^([0-9]+)\\.([0-9]+).*$" "\\1" DARWIN_MAJOR_VERSION "${CMAKE_SYSTEM_VERSION}")
  string(REGEX REPLACE "^([0-9]+)\\.([0-9]+).*$" "\\2" DARWIN_MINOR_VERSION "${CMAKE_SYSTEM_VERSION}")

  if(DARWIN_MAJOR_VERSION VERSION_EQUAL 8)
    include(platforms/macos_tiger)
  endif()

  # For older macOS, we assume MacPorts because Homebrew only supports newer version
  if(DARWIN_MAJOR_VERSION VERSION_LESS 11)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/platforms/macports/finders")

    # On MacPorts, libfmt is in a subdirectory:
    list(APPEND CMAKE_MODULE_PATH "/opt/local/lib/libfmt11/cmake")
  endif()
endif()
