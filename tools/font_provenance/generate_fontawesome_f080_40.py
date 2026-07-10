#!/usr/bin/env python3
"""Generate the TP-3000 AwesomeF080_40 packed font independently.

SPDX-License-Identifier: GPL-3.0-only
Copyright (C) 2026 S. Brachtl (DK6WT)

The generated C data is derived from Font Awesome 4.5.0 and remains
licensed under the SIL Open Font License 1.1. This generator does not read
or copy the historical ILI9341_fonts C tables.
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from dataclasses import dataclass
from pathlib import Path

try:
    import freetype
except ImportError as exc:  # pragma: no cover - environment dependent
    raise SystemExit(
        "ERROR: freetype-py is required. Install it with: python -m pip install -r tools/font_provenance/requirements.txt"
    ) from exc

FONT_RELATIVE = Path("third_party/font_sources/FontAwesome-4.5.0.ttf")
FONT_SHA256 = "7b5a4320fba0d4c8f79327645b4b9cc875a2ec617a557e849b813918eb733499"
POINT_SIZE = 40
DPI = 100
FONT_SYMBOL = "AwesomeF080_40"
RANGE_FIRST = 0x00
RANGE_LAST = 0x2B
LINE_SPACE = 56
CAP_HEIGHT = 48

# Existing TP-3000 byte values are retained, but they now map to glyphs
# independently rasterized from the official Font Awesome source font.
SLOT_TO_UNICODE = {
    0x10: 0xF090,  # sign-in / ENTER symbol used by icon_ok
    0x2A: 0xF0AA,  # chevron-circle-up
    0x2B: 0xF0AB,  # chevron-circle-down
}


@dataclass(frozen=True)
class Glyph:
    width: int
    height: int
    xoffset: int
    yoffset: int
    delta: int
    rows: tuple[tuple[int, ...], ...]


EMPTY_GLYPH = Glyph(0, 0, 0, 0, 0, tuple())


class BitWriter:
    def __init__(self) -> None:
        self._bytes = bytearray()
        self._current = 0
        self._bit_count = 0

    @property
    def byte_count(self) -> int:
        return len(self._bytes) + (1 if self._bit_count else 0)

    def write_bit(self, bit: int) -> None:
        if bit:
            self._current |= 1 << (7 - self._bit_count)
        self._bit_count += 1
        if self._bit_count == 8:
            self._bytes.append(self._current)
            self._current = 0
            self._bit_count = 0

    def write_number(self, value: int, bits: int) -> None:
        if bits <= 0:
            raise ValueError("bit width must be positive")
        encoded = value & ((1 << bits) - 1)
        for shift in range(bits - 1, -1, -1):
            self.write_bit((encoded >> shift) & 1)

    def pad_to_byte(self) -> None:
        while self._bit_count:
            self.write_bit(0)

    def finish(self) -> bytes:
        self.pad_to_byte()
        return bytes(self._bytes)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def bits_required_unsigned(maximum: int) -> int:
    maximum = max(0, maximum)
    bits = 1
    while maximum >= (1 << bits):
        bits += 1
    return bits


def bits_required_signed(minimum: int, maximum: int) -> int:
    minimum = min(0, minimum)
    maximum = max(0, maximum)
    bits = 2
    while minimum < -(1 << (bits - 1)):
        bits += 1
    while maximum >= (1 << (bits - 1)):
        bits += 1
    return bits


def bitmap_rows(bitmap: "freetype.Bitmap") -> tuple[tuple[int, ...], ...]:
    width = bitmap.width
    height = bitmap.rows
    pitch = bitmap.pitch
    source = bytes(bitmap.buffer)
    stride = abs(pitch)
    rows: list[tuple[int, ...]] = []

    for visual_y in range(height):
        source_y = visual_y if pitch >= 0 else (height - 1 - visual_y)
        base = source_y * stride
        pixels = []
        for x in range(width):
            byte = source[base + (x >> 3)]
            pixels.append(1 if byte & (0x80 >> (x & 7)) else 0)
        rows.append(tuple(pixels))
    return tuple(rows)


def render_glyphs(font_path: Path) -> dict[int, Glyph]:
    actual_hash = sha256_file(font_path)
    if actual_hash != FONT_SHA256:
        raise ValueError(
            f"Font SHA-256 mismatch for {font_path}: {actual_hash}; expected {FONT_SHA256}"
        )

    face = freetype.Face(str(font_path))
    face.set_char_size(0, POINT_SIZE * 64, DPI, DPI)
    flags = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO

    glyphs: dict[int, Glyph] = {}
    for slot, codepoint in SLOT_TO_UNICODE.items():
        face.load_char(chr(codepoint), flags)
        rendered = face.glyph
        bitmap = rendered.bitmap
        advance = int(round(rendered.advance.x / 64.0))
        glyphs[slot] = Glyph(
            width=bitmap.width,
            height=bitmap.rows,
            xoffset=rendered.bitmap_left,
            yoffset=rendered.bitmap_top - bitmap.rows,
            delta=advance,
            rows=bitmap_rows(bitmap),
        )
    return glyphs


def repeated_lines(glyph: Glyph, y: int) -> int:
    count = 0
    for next_y in range(y + 1, glyph.height):
        if glyph.rows[next_y] != glyph.rows[y]:
            break
        count += 1
    return count


def write_glyph(
    writer: BitWriter,
    glyph: Glyph,
    *,
    bits_width: int,
    bits_height: int,
    bits_xoffset: int,
    bits_yoffset: int,
    bits_delta: int,
) -> None:
    writer.write_number(0, 3)  # packed-font format version/reserved bits
    writer.write_number(glyph.width, bits_width)
    writer.write_number(glyph.height, bits_height)
    writer.write_number(glyph.xoffset, bits_xoffset)
    writer.write_number(glyph.yoffset, bits_yoffset)
    writer.write_number(glyph.delta, bits_delta)

    y = 0
    while y < glyph.height:
        identical = repeated_lines(glyph, y)
        if identical == 0:
            writer.write_bit(0)
        else:
            writer.write_bit(1)
            identical = min(identical, 6)
            writer.write_number(identical - 1, 3)
        for pixel in glyph.rows[y]:
            writer.write_bit(pixel)
        y += identical + 1
    writer.pad_to_byte()


def format_bytes(data: bytes, per_line: int = 10) -> str:
    if not data:
        return ""
    lines = []
    for offset in range(0, len(data), per_line):
        chunk = data[offset : offset + per_line]
        lines.append("  " + ",".join(f"0x{value:02X}" for value in chunk) + ",")
    return "\n".join(lines)


def ascii_art(slot: int, codepoint: int, glyph: Glyph) -> str:
    lines = [
        f"/* Slot 0x{slot:02X} -> U+{codepoint:04X}: "
        f"size={glyph.width}x{glyph.height}, offset={glyph.xoffset},{glyph.yoffset}, "
        f"delta={glyph.delta}",
    ]
    for row in glyph.rows:
        lines.append(("  " + "".join("*" if pixel else " " for pixel in row)).rstrip())
    lines.append("*/")
    return "\n".join(lines)


def generate_c(root: Path) -> str:
    font_path = root / FONT_RELATIVE
    rendered = render_glyphs(font_path)
    glyphs = [rendered.get(slot, EMPTY_GLYPH) for slot in range(RANGE_FIRST, RANGE_LAST + 1)]

    bits_width = bits_required_unsigned(max(g.width for g in glyphs))
    bits_height = bits_required_unsigned(max(g.height for g in glyphs))
    bits_xoffset = bits_required_signed(
        min(g.xoffset for g in glyphs), max(g.xoffset for g in glyphs)
    )
    bits_yoffset = bits_required_signed(
        min(g.yoffset for g in glyphs), max(g.yoffset for g in glyphs)
    )
    bits_delta = bits_required_unsigned(max(g.delta for g in glyphs))

    data_writer = BitWriter()
    offsets: list[int] = []
    for glyph in glyphs:
        offsets.append(len(data_writer.finish()))
        # finish() only pads; because every glyph is already padded this does not alter data.
        write_glyph(
            data_writer,
            glyph,
            bits_width=bits_width,
            bits_height=bits_height,
            bits_xoffset=bits_xoffset,
            bits_yoffset=bits_yoffset,
            bits_delta=bits_delta,
        )
    data = data_writer.finish()

    bits_index = bits_required_unsigned(len(data))
    index_writer = BitWriter()
    for offset in offsets:
        index_writer.write_number(offset, bits_index)
    index = index_writer.finish()

    art = "\n\n".join(
        ascii_art(slot, codepoint, rendered[slot])
        for slot, codepoint in SLOT_TO_UNICODE.items()
    )

    return f'''/*
 * SPDX-License-Identifier: OFL-1.1
 *
 * TP-3000 independently generated Font Awesome icon font
 * File: fonts_fontawesome_f080_data.c
 *
 * Source font: Font Awesome 4.5.0, fontawesome-webfont.ttf
 * Source SHA-256: {FONT_SHA256}
 * Font license: SIL Open Font License 1.1
 *
 * Generated by tools/font_provenance/generate_fontawesome_f080_40.py
 * at {POINT_SIZE} pt and {DPI} dpi using monochrome FreeType rasterization.
 * The generator reads only the official TTF source. It does not read or copy
 * the historical ILI9341_fonts C tables.
 *
 * Included TP-3000 byte slots:
 *   0x10 -> U+F090  sign-in / ENTER symbol
 *   0x2A -> U+F0AA  chevron-circle-up
 *   0x2B -> U+F0AB  chevron-circle-down
 *
 * The packed representation is compatible with ILI9341_t3_font_t.
 * Full license and provenance details are in LICENSES/OFL-1.1.txt,
 * LICENSES/Font-Awesome-NOTICE.txt and third_party/font_sources/README.md.
 */

#include "fonts.h"

#ifndef PROGMEM
#define PROGMEM __attribute__((section(".progmem")))
#endif

{art}

static const unsigned char {FONT_SYMBOL}_data[] PROGMEM = {{
{format_bytes(data)}
}};
/* font data size: {len(data)} bytes */

static const unsigned char {FONT_SYMBOL}_index[] PROGMEM = {{
{format_bytes(index)}
}};
/* font index size: {len(index)} bytes */

const ILI9341_t3_font_t {FONT_SYMBOL} PROGMEM = {{
  {FONT_SYMBOL}_index,
  0,
  {FONT_SYMBOL}_data,
  1,
  0,
  {RANGE_FIRST},
  {RANGE_LAST},
  0,
  0,
  {bits_index},
  {bits_width},
  {bits_height},
  {bits_xoffset},
  {bits_yoffset},
  {bits_delta},
  {LINE_SPACE},
  {CAP_HEIGHT}
}};
'''


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="repository root (default: inferred)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="output C file (default: <root>/fonts_fontawesome_f080_data.c)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify that the committed output is reproducible without rewriting it",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve() if args.output else root / "fonts_fontawesome_f080_data.c"
    generated = generate_c(root)

    if args.check:
        if not output.is_file():
            raise ValueError(f"missing generated file: {output}")
        current = output.read_text(encoding="utf-8")
        if current != generated:
            raise ValueError(f"generated output differs from committed file: {output}")
        print("Font Awesome AwesomeF080_40 regeneration: PASS")
        return 0

    output.write_text(generated, encoding="utf-8", newline="\n")
    print(f"Wrote {output}")
    print(f"SHA-256: {hashlib.sha256(generated.encode('utf-8')).hexdigest()}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
