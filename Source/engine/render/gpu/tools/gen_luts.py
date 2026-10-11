#!/usr/bin/env python3
"""Generate Source/engine/render/gpu/gpu_luts.hpp from the libretro hqx LUT PNGs.

The hqx shader family (Maxim Stepin's algorithm, LGPL 2.1, GLSL port by
Cameron Zemek / Jules Blok) uses a 256 x (16*S*S) RGBA8 lookup texture whose
texels are integer weight counts (0..16). The PNGs contain few distinct RGBA
tuples, so we embed them palette-indexed to keep the generated source small.
The GPU scaler expands them to RGBA8 textures once at init time.

Usage: gen_luts.py <hqx-resources-dir> <output-header>
"""
import sys
from PIL import Image

MAX_ENTRIES = 256 * 256  # hq4x is 256 x (16*4*4) = 65536 entries


def emit(f, name, path):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    assert w == 256, f"{name}: expected width 256, got {w}"
    px = list(im.getdata())
    palette = []
    index = []
    for p in px:
        # clamp weights to sane hqx range
        q = tuple(min(max(c, 0), 16) for c in p)
        if q not in palette:
            palette.append(q)
        index.append(palette.index(q))
    assert len(palette) <= 256
    print(f"{name}: {w}x{h}, {len(palette)} palette entries")
    f.write(f"// {name}: {w}x{h} LUT, {len(palette)} distinct weight vectors\n")
    # Generated data is exempt from clang-format (upstream clang-format CI
    # would otherwise repack the tables); markers mirror the committed file.
    f.write("// clang-format off\n")
    f.write(f"constexpr LutData {name} = {{\n    {w}, {h}, {len(palette)},\n    {{\n")
    for p in palette:
        f.write(f"        {{{p[0]}, {p[1]}, {p[2]}, {p[3]}}},\n")
    f.write("    },\n    {\n")
    for row in range(h):
        for col0 in range(0, w, 32):
            f.write("        ")
            f.write("".join(f"{index[row * w + col]}," for col in range(col0, min(col0 + 32, w))))
            f.write("\n")
    f.write("    }\n")
    f.write("};\n")
    f.write("// clang-format on\n\n")


def main():
    src = sys.argv[1]
    out = sys.argv[2]
    with open(out, "w") as f:
        f.write("""// GENERATED FILE - do not edit by hand.
// Regenerate with Source/engine/render/gpu/tools/gen_luts.py
//
// hqx lookup tables for the GPU scaler. Data derived from the hqx LUT
// resources shipped with the libretro slang-shaders repository:
//   Copyright (C) 2003 Maxim Stepin
//   Copyright (C) 2010 Cameron Zemek
//   Copyright (C) 2014 Jules Blok
//   License: GNU Lesser General Public License, version 2.1 or later.
// Each texel is a palette index; palette entries are integer blend weights
// (the shader normalizes them by their sum).

#pragma once

#include <cstdint>

namespace devilution::gpu::luts {

struct LutData {
        int width;
        int height;
        int paletteSize;
        std::uint8_t palette[256][4];
        std::uint8_t indices[256 * 256];
};

""")
        emit(f, "HQ2x", f"{src}/hq2x.png")
        emit(f, "HQ3x", f"{src}/hq3x.png")
        emit(f, "HQ4x", f"{src}/hq4x.png")
        f.write("} // namespace devilution::gpu::luts\n")


if __name__ == "__main__":
    main()
