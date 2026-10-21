#!/usr/bin/env python3
"""Compiles shadow corrections from their text form to the `.shadowfix` files that the game loads.

The binary format is described in `Source/engine/shadow_fix.hpp`. The text form has one file per sprite
file, at the path of the sprite file with the extension `.shadowfix.txt`:

    # Comments and empty lines are ignored.
    shadowfix 1
    list <number of sprites>
    frame <index> <width> <height> <guard>
    <y> <x> <length> <B|S>
    ...

`list` starts a sprite list (a sprite file is a single list or, for sprites that have directions, one
list per direction, in order). `frame` starts the corrections of a sprite of the current list; sprites
that are not mentioned have none. `width` and `height` are the size of the sprite. `guard` is the
hexadecimal hash of the sprite's size and black pixels that the game checks before applying the
corrections (`ShadowFixGuard`); the tool that makes the corrections writes it, since this script does
not see the sprites.

Each remaining line is a run of `length` pixels starting at column `x` of row `y` (rows count from the
top of the sprite): `B` marks pixels that the shadow rule takes for shadow and that belong to the sprite,
`S` marks pixels that it keeps in the sprite and that are shadow.

Usage: compile_shadow_fixes.py --output-dir <dir> [--root <dir>] <file.shadowfix.txt>...

Each output file is written to the output directory at the path of its input relative to `--root`
(default: the current directory), without the `.txt` extension.
"""

import argparse
import pathlib
import struct
import sys

VERSION = 1
MAX_SPAN_WIDTH = 0x7F
END_OF_ROW = 0x80


class ParseError(Exception):
    pass


def parse(text):
    """Returns the lists of the file: for each list, the number of sprites and {frame: (height, guard, runs)}."""
    lists = []
    current = None
    seen_header = False
    for number, line in enumerate(text.splitlines(), 1):
        fields = line.split('#', 1)[0].split()
        if not fields:
            continue
        try:
            if not seen_header:
                if fields != ['shadowfix', str(VERSION)]:
                    raise ParseError(f'expected "shadowfix {VERSION}"')
                seen_header = True
            elif fields[0] == 'list':
                (count,) = fields[1:]
                lists.append((int(count), {}))
                current = None
            elif fields[0] == 'frame':
                index, width, height, guard = fields[1:]
                if not lists:
                    raise ParseError('frame before the first list')
                count, frames = lists[-1]
                if not 0 <= int(index) < count:
                    raise ParseError(f'the list has {count} sprites')
                if int(index) in frames:
                    raise ParseError('frame given twice')
                if not (1 <= int(width) <= 256 and 1 <= int(height) <= 255):
                    raise ParseError('invalid sprite size')
                current = frames[int(index)] = (int(height), int(guard, 16), [], int(width))
            else:
                y, x, length, kind = fields
                if current is None:
                    raise ParseError('run before the first frame of the list')
                if kind not in ('B', 'S') or int(length) < 1 or int(x) < 0 or int(x) + int(length) > current[3] or not 0 <= int(y) < current[0]:
                    raise ParseError('invalid run')
                current[2].append((int(y), int(x), int(length), kind))
        except (ValueError, ParseError) as error:
            raise ParseError(f'line {number}: {error}: {line.strip()}') from None
    if not seen_header or not lists:
        raise ParseError('no lists')
    return lists


def encode_mask(runs, height):
    """Encodes runs of (y, x, length) as the shadow mask of a sprite of the given height."""
    if not runs:
        return b''
    rows = {}
    for y, x, length in runs:
        rows.setdefault(y, []).append((x, length))
    lowest, highest = max(rows), min(rows)
    out = bytearray((height - 1 - lowest, lowest - highest + 1))
    for y in range(lowest, highest - 1, -1):
        spans = []
        end = -1
        for x, length in sorted(rows.get(y, [])):
            if x < end:
                raise ParseError(f'overlapping runs in row {y}')
            end = x + length
            # Adjacent runs become one span; long ones are split.
            if spans and spans[-1][0] + spans[-1][1] == x:
                x, length = spans[-1][0], spans[-1][1] + length
                spans.pop()
            spans.append((x, length))
        row = bytearray()
        for x, length in spans:
            while length > 0:
                width = min(length, MAX_SPAN_WIDTH)
                row += bytes((x, width))
                x += width
                length -= width
        if not row:
            row += bytes((0, 0))
        row[-1] |= END_OF_ROW
        out += row
    return bytes(out)


def compile_fixes(text):
    """Returns the binary form of the corrections given in their text form."""
    lists = parse(text)
    out = bytearray(b'SHFX' + struct.pack('<BBH', VERSION, 0, len(lists)))
    for count, frames in lists:
        out += struct.pack('<H', count)
        for index in range(count):
            if index not in frames or not frames[index][2]:
                out += struct.pack('<H', 0)
                continue
            height, guard, runs, _ = frames[index]
            body = encode_mask([run[:3] for run in runs if run[3] == 'B'], height)
            shadow = encode_mask([run[:3] for run in runs if run[3] == 'S'], height)
            record = struct.pack('<IH', guard, len(body)) + body + shadow
            if len(record) > 0xFFFF:
                raise ParseError(f'the corrections of frame {index} are too large')
            out += struct.pack('<H', len(record)) + record
    return bytes(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output-dir', required=True, type=pathlib.Path)
    parser.add_argument('--root', default='.', type=pathlib.Path)
    parser.add_argument('inputs', nargs='+', type=pathlib.Path)
    args = parser.parse_args()
    for path in args.inputs:
        if not path.name.endswith('.shadowfix.txt'):
            sys.exit(f'{path}: expected a .shadowfix.txt file')
        try:
            data = compile_fixes(path.read_text())
        except ParseError as error:
            sys.exit(f'{path}: {error}')
        output = args.output_dir / path.resolve().relative_to(args.root.resolve()).with_suffix('')
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)


if __name__ == '__main__':
    main()
