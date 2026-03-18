#!/usr/bin/env python3
"""
Generate the external Chinese font binary used by src/graphics/fonts/ChineseFont.cpp.

Output layout:
  uint32 magic   = 0x43484631 ("CHF1")
  uint32 version = 1
  uint32 count
  uint32 reserved = 0
  uint8  key_table[count][4]
  uint8  bitmap_table[count][32]

Usage:
  python3 bin/generate_chinese_font_bin.py
  python3 bin/generate_chinese_font_bin.py --output /tmp/chinese_font.bin
  python3 bin/generate_chinese_font_bin.py --input src/graphics/fonts/ChineseFont.cpp --output bin/chinese_font.bin
"""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path


MAGIC = 0x43484631  # "CHF1"
VERSION = 1
KEY_SIZE = 4
BITMAP_SIZE = 32


ENTRY_RE = re.compile(
    r'\{\s*"((?:\\.|[^"\\])*)"\s*,\s*\{([^}]*)\}\s*\},?',
    re.MULTILINE,
)


def extract_font_block(source_text: str) -> str:
    start_marker = "const ChineseFont chineseFont[] = {"
    start = source_text.find(start_marker)
    if start < 0:
        raise ValueError("Cannot find 'const ChineseFont chineseFont[] = {' in source file")

    start = source_text.find("{", start)
    if start < 0:
        raise ValueError("Cannot locate start of chineseFont array body")

    end = source_text.find("\n};", start)
    if end < 0:
        raise ValueError("Cannot locate end of chineseFont array body")

    return source_text[start + 1 : end]


def parse_entries(source_text: str) -> list[tuple[str, bytes]]:
    block = extract_font_block(source_text)
    entries: list[tuple[str, bytes]] = []

    for match in ENTRY_RE.finditer(block):
        utf8_text = match.group(1)
        bitmap_text = match.group(2)
        byte_values = [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{1,2})", bitmap_text)]

        if len(byte_values) != BITMAP_SIZE:
            raise ValueError(
                f"Glyph '{utf8_text}' has {len(byte_values)} bytes, expected {BITMAP_SIZE}"
            )

        entries.append((utf8_text, bytes(byte_values)))

    if not entries:
        raise ValueError("No chineseFont entries were parsed")

    return entries


def make_utf8_key(text: str) -> bytes:
    raw = text.encode("utf-8")
    if len(raw) > KEY_SIZE - 1:
        raw = raw[: KEY_SIZE - 1]
    return raw.ljust(KEY_SIZE, b"\x00")


def build_binary(entries: list[tuple[str, bytes]]) -> bytes:
    header = struct.pack("<IIII", MAGIC, VERSION, len(entries), 0)
    key_table = b"".join(make_utf8_key(text) for text, _ in entries)
    bitmap_table = b"".join(bitmap for _, bitmap in entries)
    return header + key_table + bitmap_table


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate chinese_font.bin from ChineseFont.cpp")
    parser.add_argument(
        "--input",
        default="src/graphics/fonts/ChineseFont.cpp",
        help="Path to ChineseFont.cpp",
    )
    parser.add_argument(
        "--output",
        default="bin/chinese_font.bin",
        help="Output path for generated binary",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    source_text = input_path.read_text(encoding="utf-8")
    entries = parse_entries(source_text)
    blob = build_binary(entries)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(blob)

    print(f"Generated {output_path}")
    print(f"Glyphs: {len(entries)}")
    print(f"Bytes: {len(blob)}")
    print(f"Header: magic=0x{MAGIC:08x} version={VERSION}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
