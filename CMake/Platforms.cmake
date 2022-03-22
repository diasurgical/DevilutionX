if(WIN32)
  include(platforms/windows)
endif()

if(UNIX)
  include(platforms/linux)
endif()

if(APPLE)
  include(platforms/macos)
endif()

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
set_property(CACHE TARGET_PLATFORM PROPERTY STRINGS host retrofw rg350 gkd350h cpigamesh)
if(TARGET_PLATFORM STREQUAL "retrofw")
  include(platforms/retrofw)
elseif(TARGET_PLATFORM STREQUAL "rg350")
  include(platforms/rg350)
elseif(TARGET_PLATFORM STREQUAL "gkd350h")
  include(platforms/gkd350h)
elseif(TARGET_PLATFORM STREQUAL "cpigamesh")
  include(platforms/cpigamesh)
elseif(TARGET_PLATFORM STREQUAL "lepus")
  include(platforms/lepus)
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
