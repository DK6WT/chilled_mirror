#!/usr/bin/env python3
"""Generate TP-3000 Droid Sans Mono packed fonts independently.

SPDX-License-Identifier: GPL-3.0-only
Copyright (C) 2026 S. Brachtl (DK6WT)

The generated C data is derived exclusively from the Apache-2.0-licensed
DroidSansMono-AOSP.ttf source. This generator does not read or copy the
historical ILI9341_fonts C tables.
"""
from __future__ import annotations

import argparse
import hashlib
import math
import sys
from dataclasses import dataclass
from pathlib import Path

try:
    import freetype
except ImportError as exc:  # pragma: no cover - environment dependent
    raise SystemExit(
        "ERROR: freetype-py is required. Install it with: "
        "python -m pip install -r tools/font_provenance/requirements.txt"
    ) from exc

FONT_RELATIVE = Path("third_party/font_sources/DroidSansMono-AOSP.ttf")
FONT_SHA256 = "db19a1fdaba41cc4a2fec0330e5c15e71c6dd68a3ef074f4f28268828b45c862"
DPI = 100
SIZES = (8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 48, 60, 72, 96)
RANGE1_FIRST = 0x20
RANGE1_LAST = 0x7E
RANGE2_FIRST = 0xB0
RANGE2_LAST = 0xBE
SMALL_PERCENT_SCALE = 0.75
SMALL_PERCENT_THRESHOLD = 0.40

# Keep the established TP-3000 byte values B0-B5 unchanged and extend the
# second packed-font range with additional German and metrology characters.
SLOT_TO_UNICODE = {
    0xB0: 0x00B0,  # degree sign
    0xB1: 0x03A9,  # Greek capital omega, used as ohm symbol
    0xB2: 0x00E4,  # ä
    0xB3: 0x00F6,  # ö
    0xB4: 0x00FC,  # ü
    # 0xB5 is generated from the freshly rasterized U+0025 percent sign.
    0xB6: 0x00C4,  # Ä
    0xB7: 0x00D6,  # Ö
    0xB8: 0x00DC,  # Ü
    0xB9: 0x00DF,  # ß
    0xBA: 0x00B5,  # µ micro sign
    0xBB: 0x00B1,  # ± plus-minus sign
    0xBC: 0x0394,  # Δ Greek capital delta
    0xBD: 0x2264,  # ≤ less-than or equal
    0xBE: 0x2265,  # ≥ greater-than or equal
}

# Retain the established font line metrics so existing TP-3000 layouts do not
# move merely because the packed glyph data is regenerated.
LINE_METRICS = {
    8: (12, 8),
    9: (15, 9),
    10: (15, 10),
    11: (16, 11),
    12: (19, 12),
    13: (20, 13),
    14: (21, 14),
    16: (25, 16),
    18: (28, 18),
    20: (31, 20),
    24: (37, 24),
    28: (45, 28),
    32: (50, 31),
    40: (64, 40),
    48: (77, 48),
    60: (96, 59),
    72: (115, 71),
    96: (154, 95),
}


@dataclass(frozen=True)
class Glyph:
    width: int
    height: int
    xoffset: int
    yoffset: int
    delta: int
    rows: tuple[tuple[int, ...], ...]


class BitWriter:
    def __init__(self) -> None:
        self._bytes = bytearray()
        self._current = 0
        self._bit_count = 0

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
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


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


def render_codepoint(face: "freetype.Face", codepoint: int) -> Glyph:
    if face.get_char_index(codepoint) == 0:
        raise ValueError(f"source font does not contain U+{codepoint:04X}")
    flags = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO
    face.load_char(chr(codepoint), flags)
    rendered = face.glyph
    bitmap = rendered.bitmap
    rows = bitmap_rows(bitmap)
    delta = int(round(rendered.advance.x / 64.0))
    if not any(pixel for row in rows for pixel in row):
        return Glyph(0, 0, 0, 0, delta, tuple())
    return Glyph(
        width=bitmap.width,
        height=bitmap.rows,
        xoffset=rendered.bitmap_left,
        yoffset=rendered.bitmap_top - bitmap.rows,
        delta=delta,
        rows=rows,
    )


def scale_binary_bitmap(
    rows: tuple[tuple[int, ...], ...],
    new_width: int,
    new_height: int,
    threshold: float,
) -> tuple[tuple[int, ...], ...]:
    """Area-resample a monochrome bitmap and threshold it deterministically."""
    if not rows or not rows[0]:
        return tuple()
    old_height = len(rows)
    old_width = len(rows[0])
    output: list[tuple[int, ...]] = []

    for dest_y in range(new_height):
        y0 = dest_y * old_height / new_height
        y1 = (dest_y + 1) * old_height / new_height
        out_row: list[int] = []
        for dest_x in range(new_width):
            x0 = dest_x * old_width / new_width
            x1 = (dest_x + 1) * old_width / new_width
            total_area = (x1 - x0) * (y1 - y0)
            on_area = 0.0
            for source_y in range(math.floor(y0), math.ceil(y1)):
                if source_y < 0 or source_y >= old_height:
                    continue
                overlap_y = max(0.0, min(y1, source_y + 1) - max(y0, source_y))
                for source_x in range(math.floor(x0), math.ceil(x1)):
                    if source_x < 0 or source_x >= old_width:
                        continue
                    overlap_x = max(0.0, min(x1, source_x + 1) - max(x0, source_x))
                    if rows[source_y][source_x]:
                        on_area += overlap_x * overlap_y
            out_row.append(1 if on_area / total_area >= threshold else 0)
        output.append(tuple(out_row))
    return tuple(output)


def make_small_percent(percent: Glyph) -> Glyph:
    if percent.width <= 0 or percent.height <= 0:
        raise ValueError("rendered percent sign is empty")
    width = max(1, int(round(percent.width * SMALL_PERCENT_SCALE)))
    height = max(1, int(round(percent.height * SMALL_PERCENT_SCALE)))
    rows = scale_binary_bitmap(percent.rows, width, height, SMALL_PERCENT_THRESHOLD)
    # Keep the normal percent baseline and monospaced advance, but center the
    # reduced glyph horizontally in the same cell.
    xoffset = (percent.delta - width) // 2
    return Glyph(
        width=width,
        height=height,
        xoffset=xoffset,
        yoffset=percent.yoffset,
        delta=percent.delta,
        rows=rows,
    )


def render_size(font_path: Path, size: int) -> list[Glyph]:
    face = freetype.Face(str(font_path))
    face.set_char_size(0, size * 64, DPI, DPI)

    glyphs: list[Glyph] = []
    for codepoint in range(RANGE1_FIRST, RANGE1_LAST + 1):
        glyphs.append(render_codepoint(face, codepoint))

    regular_percent = render_codepoint(face, 0x0025)
    for slot in range(RANGE2_FIRST, RANGE2_LAST + 1):
        if slot == 0xB5:
            glyphs.append(make_small_percent(regular_percent))
        else:
            glyphs.append(render_codepoint(face, SLOT_TO_UNICODE[slot]))
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
    writer.write_number(0, 3)
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


def pack_glyphs(glyphs: list[Glyph]) -> tuple[bytes, bytes, tuple[int, int, int, int, int, int]]:
    bits_width = bits_required_unsigned(max(glyph.width for glyph in glyphs))
    bits_height = bits_required_unsigned(max(glyph.height for glyph in glyphs))
    bits_xoffset = bits_required_signed(
        min(glyph.xoffset for glyph in glyphs), max(glyph.xoffset for glyph in glyphs)
    )
    bits_yoffset = bits_required_signed(
        min(glyph.yoffset for glyph in glyphs), max(glyph.yoffset for glyph in glyphs)
    )
    bits_delta = bits_required_unsigned(max(glyph.delta for glyph in glyphs))

    data_writer = BitWriter()
    offsets: list[int] = []
    for glyph in glyphs:
        offsets.append(len(data_writer.finish()))
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
    return data, index, (
        bits_index,
        bits_width,
        bits_height,
        bits_xoffset,
        bits_yoffset,
        bits_delta,
    )


def format_bytes(data: bytes, per_line: int = 10) -> str:
    lines = []
    for offset in range(0, len(data), per_line):
        chunk = data[offset : offset + per_line]
        lines.append("  " + ",".join(f"0x{value:02X}" for value in chunk) + ",")
    return "\n".join(lines)


def generate_font_block(font_path: Path, size: int) -> str:
    glyphs = render_size(font_path, size)
    data, index, widths = pack_glyphs(glyphs)
    bits_index, bits_width, bits_height, bits_xoffset, bits_yoffset, bits_delta = widths
    line_space, cap_height = LINE_METRICS[size]
    symbol = f"DroidSansMono_{size}"
    return f'''static const unsigned char {symbol}_data[] PROGMEM = {{
{format_bytes(data)}
}};
/* font data size: {len(data)} bytes */

static const unsigned char {symbol}_index[] PROGMEM = {{
{format_bytes(index)}
}};
/* font index size: {len(index)} bytes */

const ILI9341_t3_font_t {symbol} PROGMEM = {{
  {symbol}_index,
  0,
  {symbol}_data,
  1,
  0,
  {RANGE1_FIRST},
  {RANGE1_LAST},
  {RANGE2_FIRST},
  {RANGE2_LAST},
  {bits_index},
  {bits_width},
  {bits_height},
  {bits_xoffset},
  {bits_yoffset},
  {bits_delta},
  {line_space},
  {cap_height}
}};
'''


def generate_c(root: Path) -> str:
    font_path = root / FONT_RELATIVE
    actual_hash = sha256_file(font_path)
    if actual_hash != FONT_SHA256:
        raise ValueError(
            f"Font SHA-256 mismatch for {font_path}: {actual_hash}; expected {FONT_SHA256}"
        )

    blocks = "\n\n".join(generate_font_block(font_path, size) for size in SIZES)
    mapping_lines = [
        " *   0xB0 -> U+00B0  degree sign",
        " *   0xB1 -> U+03A9  Greek capital omega / ohm",
        " *   0xB2 -> U+00E4  ä",
        " *   0xB3 -> U+00F6  ö",
        " *   0xB4 -> U+00FC  ü",
        " *   0xB5 -> reduced U+0025 percent sign, generated in this script",
        " *   0xB6 -> U+00C4  Ä",
        " *   0xB7 -> U+00D6  Ö",
        " *   0xB8 -> U+00DC  Ü",
        " *   0xB9 -> U+00DF  ß",
        " *   0xBA -> U+00B5  µ",
        " *   0xBB -> U+00B1  ±",
        " *   0xBC -> U+0394  Δ",
        " *   0xBD -> U+2264  ≤",
        " *   0xBE -> U+2265  ≥",
    ]
    mapping = "\n".join(mapping_lines)
    return f'''/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * TP-3000 independently generated Droid Sans Mono packed font data
 * File: fonts_droidsansmono_data.c
 *
 * Source font: Droid Sans Mono from the Android Open Source Project
 * Source SHA-256: {FONT_SHA256}
 * Font license: Apache License 2.0
 * Designer: Steve Matteson
 *
 * Generated by tools/font_provenance/generate_droidsansmono.py
 * at 100 dpi using monochrome FreeType rasterization. The generator reads
 * only the official AOSP TTF source. It does not read or copy historical
 * ILI9341_fonts C tables.
 *
 * First range: 0x20-0x7E (ASCII)
 * TP-3000 second range:
{mapping}
 *
 * The packed representation is compatible with ILI9341_t3_font_t.
 * Full license and provenance details are in LICENSES/Apache-2.0.txt,
 * LICENSES/Droid-Sans-Mono-NOTICE.txt and third_party/font_sources/README.md.
 */

#include "fonts.h"

#ifndef PROGMEM
#define PROGMEM __attribute__((section(".progmem")))
#endif

{blocks}

/* End of independently generated Droid Sans Mono font data. */
'''


def write_pbm(path: Path, rows: tuple[tuple[int, ...], ...], scale: int = 4) -> None:
    width = len(rows[0]) if rows else 1
    height = len(rows) if rows else 1
    expanded_width = width * scale
    expanded_height = height * scale
    lines = ["P1", f"{expanded_width} {expanded_height}"]
    for row in rows or ((0,),):
        expanded = []
        for pixel in row:
            expanded.extend([str(pixel)] * scale)
        line = " ".join(expanded)
        lines.extend([line] * scale)
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def write_previews(root: Path, preview_dir: Path) -> None:
    font_path = root / FONT_RELATIVE
    preview_dir.mkdir(parents=True, exist_ok=True)
    for size in (12, 14, 16, 20, 24, 28, 60):
        glyphs = render_size(font_path, size)
        special = glyphs[(RANGE1_LAST - RANGE1_FIRST + 1) :]
        cell_width = max(glyph.delta for glyph in special)
        top = max(glyph.height + max(0, glyph.yoffset) for glyph in special)
        bottom = max(max(0, -glyph.yoffset) for glyph in special)
        canvas_height = top + bottom
        canvas_width = cell_width * len(special)
        canvas = [[0 for _ in range(canvas_width)] for _ in range(canvas_height)]
        baseline = top
        for index, glyph in enumerate(special):
            x0 = index * cell_width + glyph.xoffset
            y0 = baseline - glyph.height - glyph.yoffset
            for y, row in enumerate(glyph.rows):
                for x, pixel in enumerate(row):
                    if pixel and 0 <= y0 + y < canvas_height and 0 <= x0 + x < canvas_width:
                        canvas[y0 + y][x0 + x] = 1
        write_pbm(preview_dir / f"DroidSansMono_{size}_specials.pbm", tuple(tuple(row) for row in canvas))


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
        help="output C file (default: <root>/fonts_droidsansmono_data.c)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify that the committed output is reproducible without rewriting it",
    )
    parser.add_argument(
        "--preview-dir",
        type=Path,
        help="optional directory for PBM preview sheets of the extended range",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve() if args.output else root / "fonts_droidsansmono_data.c"
    generated = generate_c(root)

    if args.check:
        if not output.is_file():
            raise ValueError(f"missing generated file: {output}")
        current = output.read_text(encoding="utf-8")
        if current != generated:
            raise ValueError(f"generated output differs from committed file: {output}")
        print("Droid Sans Mono regeneration: PASS")
    else:
        output.write_text(generated, encoding="utf-8", newline="\n")
        print(f"Wrote {output}")
        print(f"SHA-256: {hashlib.sha256(generated.encode('utf-8')).hexdigest()}")

    if args.preview_dir:
        write_previews(root, args.preview_dir.resolve())
        print(f"Wrote previews to {args.preview_dir.resolve()}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
