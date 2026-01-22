#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path


_ESCAPE_RE = re.compile(r"\\(n|t|r|\\|\"|[0-7]{1,3}|x[0-9a-fA-F]{2})")


def _unescape(value: str) -> str:
    def repl(match: re.Match[str]) -> str:
        escape = match.group(1)
        if escape == "n":
            return "\n"
        if escape == "t":
            return "\t"
        if escape == "r":
            return "\r"
        if escape == "\\":
            return "\\"
        if escape == '"':
            return '"'
        if escape.startswith("x"):
            return chr(int(escape[1:], 16))
        return chr(int(escape, 8))

    return _ESCAPE_RE.sub(repl, value)


def _parse_quoted(rest: str) -> str:
    rest = rest.strip()
    if not (rest.startswith('"') and rest.endswith('"')):
        raise ValueError(f"Invalid PO string: {rest!r}")
    return _unescape(rest[1:-1])


def parse_po(path: Path) -> dict[str, str]:
    messages: list[tuple[str | None, str, str | None, dict[int, str], set[str]]] = []

    msgctxt: str | None = None
    msgid: str | None = None
    msgid_plural: str | None = None
    msgstr: dict[int, str] = {}
    flags: set[str] = set()
    active: tuple[str, int | None] | None = None

    def flush() -> None:
        nonlocal msgctxt, msgid, msgid_plural, msgstr, flags, active
        if msgid is None:
            msgctxt = None
            msgid_plural = None
            msgstr = {}
            flags = set()
            active = None
            return

        messages.append((msgctxt, msgid, msgid_plural, dict(msgstr), set(flags)))

        msgctxt = None
        msgid = None
        msgid_plural = None
        msgstr = {}
        flags = set()
        active = None

    with path.open("r", encoding="utf-8", errors="replace", newline="") as file:
        for raw_line in file:
            line = raw_line.rstrip("\n")

            if not line.strip():
                flush()
                continue

            if line.startswith("#,"):
                for flag in line[2:].split(","):
                    flag = flag.strip()
                    if flag:
                        flags.add(flag)
                continue

            if line.startswith("#"):
                continue

            if line.startswith("msgctxt"):
                msgctxt = _parse_quoted(line[len("msgctxt") :])
                active = ("msgctxt", None)
                continue

            if line.startswith("msgid_plural"):
                msgid_plural = _parse_quoted(line[len("msgid_plural") :])
                active = ("msgid_plural", None)
                continue

            if line.startswith("msgid"):
                msgid = _parse_quoted(line[len("msgid") :])
                active = ("msgid", None)
                continue

            if line.startswith("msgstr["):
                close = line.find("]")
                index = int(line[len("msgstr[") : close])
                msgstr[index] = _parse_quoted(line[close + 1 :])
                active = ("msgstr", index)
                continue

            if line.startswith("msgstr"):
                msgstr[0] = _parse_quoted(line[len("msgstr") :])
                active = ("msgstr", 0)
                continue

            if line.lstrip().startswith('"'):
                value = _parse_quoted(line)
                if active is None:
                    continue
                kind, index = active
                if kind == "msgctxt":
                    msgctxt = (msgctxt or "") + value
                elif kind == "msgid":
                    msgid = (msgid or "") + value
                elif kind == "msgid_plural":
                    msgid_plural = (msgid_plural or "") + value
                elif kind == "msgstr":
                    assert index is not None
                    msgstr[index] = msgstr.get(index, "") + value
                continue

    flush()

    catalog: dict[str, str] = {}
    for msgctxt, msgid, msgid_plural, msgstrs, flags in messages:
        if "fuzzy" in flags:
            continue

        if msgid_plural is not None:
            key = msgid + "\x00" + msgid_plural
            max_index = max(msgstrs.keys(), default=0)
            value = "\x00".join(msgstrs.get(i, "") for i in range(max_index + 1))
        else:
            key = msgid
            value = msgstrs.get(0, "")

        if msgctxt:
            key = msgctxt + "\x04" + key

        catalog[key] = value

    catalog.setdefault("", "")
    return catalog


def write_mo(catalog: dict[str, str], out_file: Path) -> None:
    entries = sorted(catalog.items(), key=lambda kv: kv[0])
    ids = [key.encode("utf-8") for key, _ in entries]
    strs = [value.encode("utf-8") for _, value in entries]

    count = len(entries)
    header_size = 7 * 4
    table_size = count * 8
    originals_offset = header_size
    translations_offset = originals_offset + table_size
    string_offset = translations_offset + table_size

    offsets_ids: list[tuple[int, int]] = []
    offsets_strs: list[tuple[int, int]] = []
    pool = bytearray()

    for value in ids:
        offsets_ids.append((len(value), string_offset + len(pool)))
        pool.extend(value)
        pool.append(0)

    for value in strs:
        offsets_strs.append((len(value), string_offset + len(pool)))
        pool.extend(value)
        pool.append(0)

    out_file.parent.mkdir(parents=True, exist_ok=True)
    with out_file.open("wb") as file:
        file.write(
            struct.pack(
                "<Iiiiiii",
                0x950412DE,  # magic
                0,  # version
                count,
                originals_offset,
                translations_offset,
                0,  # hash table size
                0,  # hash table offset
            )
        )
        for length, offset in offsets_ids:
            file.write(struct.pack("<II", length, offset))
        for length, offset in offsets_strs:
            file.write(struct.pack("<II", length, offset))
        file.write(pool)


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile a .po file into a GNU .mo/.gmo file.")
    parser.add_argument("input", type=Path, help="Input .po file")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .mo/.gmo file")
    args = parser.parse_args()

    catalog = parse_po(args.input)
    write_mo(catalog, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

