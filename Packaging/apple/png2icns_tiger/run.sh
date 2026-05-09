#!/bin/bash

set -euo pipefail

declare -r INPUT=../../resources/icon.png
declare -ra SIZES=(16 32 128)
declare -r OUTPUT=../AppIcon_128.icns

declare -rA DEPENDENCY_SOURCES=(
  [convert]=imagemagick
  [cmake]=cmake
)

check_deps() {
  local -a missing_deps=()
  local path
  for dep in "${!DEPENDENCIES[@]}"; do
    if path="$(which "$dep")"; then
      echo >&2 "Using $dep from $path"
    else
      missing_deps+=("$dep")
    fi
  done
  if (( ${#missing_deps[@]} )); then
    echo >&2 "Error: Missing dependencies"
    for dep in "${missing_deps[@]}"; do
      echo >&2 '* '"Please install \"${dep}\", provided by ${DEPENDENCY_SOURCES[$dep]} on Debian/Ubuntu"
    done
    exit 1
  fi
}

main() {
  cd "$(dirname "$0")"
  check_deps

  set -x
  mkdir -p tmp
  { set +x; } 2> /dev/null

  local path
  local -a FILES=()
  for s in "${SIZES[@]}"; do
    path="tmp/output_${s}.png"
    FILES+=("$path")

    set -x
    convert "$INPUT" -resize "${s}x${s}" "$path"
    { set +x; } 2> /dev/null
  done

  set -x
  cmake -S. -Bbuild-rel -DCMAKE_BUILD_TYPE=Release
  cmake --build build-rel
  build-rel/png2icns_tiger "$OUTPUT" "${FILES[@]}"
  rm -rf tmp
}

main "$@"
