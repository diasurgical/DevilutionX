# On Debian/Ubuntu, this requires libpng-dev and libicns-dev
cmake_minimum_required(VERSION 3.22)
project(png2icns_tiger LANGUAGES C)
add_executable(png2icns_tiger png2icns_tiger.c)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(ICNS REQUIRED IMPORTED_TARGET libicns)
target_link_libraries(png2icns_tiger PRIVATE PkgConfig::PNG PkgConfig::ICNS)
