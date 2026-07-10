#!/usr/bin/env python3
"""Verify TP-3000 embedded-font provenance and reproducibility.

SPDX-License-Identifier: GPL-3.0-only
Copyright (C) 2026 S. Brachtl (DK6WT)
"""
from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

DROID_SIZES = (8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 48, 60, 72, 96)
EXPECTED_FILES = {
    "third_party/font_sources/DroidSansMono-AOSP.ttf": "db19a1fdaba41cc4a2fec0330e5c15e71c6dd68a3ef074f4f28268828b45c862",
    "third_party/font_sources/FontAwesome-4.5.0.ttf": "7b5a4320fba0d4c8f79327645b4b9cc875a2ec617a557e849b813918eb733499",
    "third_party/font_sources/FontAwesome-4.5.0.otf": "7ed24c05432403117372891543f0cb6a7922100919e7ae077c1f3faf67658dc2",
    "third_party/font_sources/FontAwesome-4.5.0-README.md": "9f1435a68f2e88e768dced82f9cb070291fbb8f2445e03d3298d208d973d4209",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def strip_comments(text: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def extract_array(text: str, symbol: str, suffix: str) -> bytes:
    match = re.search(
        rf"static\s+const\s+unsigned\s+char\s+{re.escape(symbol)}_{suffix}\[\]"
        rf"\s*(?:PROGMEM\s*)?=\s*\{{(.*?)\}};",
        text,
        re.S,
    )
    if not match:
        raise ValueError(f"missing array {symbol}_{suffix}")
    body = strip_comments(match.group(1))
    return bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", body))


def extract_descriptor(text: str, symbol: str) -> tuple[int, ...]:
    match = re.search(
        rf"const\s+ILI9341_t3_font_t\s+{re.escape(symbol)}\s*(?:PROGMEM\s*)?="
        rf"\s*\{{(.*?)\}};",
        text,
        re.S,
    )
    if not match:
        raise ValueError(f"missing descriptor {symbol}")
    values = [part.strip() for part in strip_comments(match.group(1)).split(",") if part.strip()]
    return tuple(int(part, 0) for part in values[3:])


def run_generator(root: Path, filename: str, label: str) -> None:
    generator = root / "tools/font_provenance" / filename
    if not generator.is_file():
        raise ValueError(f"missing generator: {generator}")
    result = subprocess.run(
        [sys.executable, str(generator), "--root", str(root), "--check"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0:
        raise ValueError(f"{label} regeneration check failed:\n" + result.stdout.rstrip())
    print(result.stdout.rstrip())



def verify_textbox_utf8_mapping(root: Path) -> None:
    """Verify the shared UTF-8-to-local-font-byte mapper and both consumers."""
    path = root / "TPtft.cpp"
    if not path.is_file():
        raise ValueError("missing TPtft.cpp UTF-8 mapping implementation")
    text = path.read_text(encoding="utf-8", errors="strict")

    helper_match = re.search(
        r"static\s+bool\s+tpUtf8ToFontByte\s*\([^)]*\)\s*\{(.*?)\n\}",
        text,
        re.S,
    )
    if not helper_match:
        raise ValueError("missing shared tpUtf8ToFontByte() helper in TPtft.cpp")
    helper = helper_match.group(1)

    expected_two_byte = {
        (0xC2, 0xB0): 0xB0,  # degree
        (0xCE, 0xA9): 0xB1,  # omega
        (0xC3, 0xA4): 0xB2,  # ä
        (0xC3, 0xB6): 0xB3,  # ö
        (0xC3, 0xBC): 0xB4,  # ü
        (0xC3, 0x84): 0xB6,  # Ä
        (0xC3, 0x96): 0xB7,  # Ö
        (0xC3, 0x9C): 0xB8,  # Ü
        (0xC3, 0x9F): 0xB9,  # ß
        (0xC2, 0xB5): 0xBA,  # micro sign
        (0xCE, 0xBC): 0xBA,  # Greek mu
        (0xC2, 0xB1): 0xBB,  # plus-minus
        (0xCE, 0x94): 0xBC,  # delta
    }
    for (first, second), local in expected_two_byte.items():
        pattern = (
            rf"p\[0\]\s*==\s*0x{first:02X}\s*&&\s*"
            rf"p\[1\]\s*==\s*0x{second:02X}.*?"
            rf"\*out\s*=\s*0x{local:02X}\s*;.*?"
            rf"\*used\s*=\s*2\s*;"
        )
        if not re.search(pattern, helper, re.S):
            raise ValueError(
                f"missing shared UTF-8 mapping {first:02X} {second:02X} -> {local:02X}"
            )

    expected_three_byte = {
        (0xE2, 0x84, 0xA6): 0xB1,  # ohm sign
        (0xE2, 0x89, 0xA4): 0xBD,  # less-than or equal
        (0xE2, 0x89, 0xA5): 0xBE,  # greater-than or equal
    }
    for (first, second, third), local in expected_three_byte.items():
        pattern = (
            rf"p\[0\]\s*==\s*0x{first:02X}.*?"
            rf"p\[1\]\s*==\s*0x{second:02X}\s*&&\s*"
            rf"p\[2\]\s*==\s*0x{third:02X}.*?"
            rf"\*out\s*=\s*0x{local:02X}\s*;.*?"
            rf"\*used\s*=\s*3\s*;"
        )
        if not re.search(pattern, helper, re.S):
            raise ValueError(
                f"missing shared UTF-8 mapping {first:02X} {second:02X} "
                f"{third:02X} -> {local:02X}"
            )

    direct_match = re.search(
        r"void\s+tpTftPrintUtf8\s*\([^)]*\)\s*\{(.*?)\n\}", text, re.S
    )
    textbox_match = re.search(
        r"void\s+TextBox::print\s*\([^)]*\)\s*\{(.*?)\n\}", text, re.S
    )
    if not direct_match or "tpUtf8ToFontByte" not in direct_match.group(1):
        raise ValueError("tpTftPrintUtf8() does not use the shared UTF-8 mapper")
    if not textbox_match or "tpUtf8ToFontByte" not in textbox_match.group(1):
        raise ValueError("TextBox::print() does not use the shared UTF-8 mapper")

    print("Shared TFT/TextBox UTF-8 mappings: PASS")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="repository root (default: inferred)",
    )
    parser.add_argument(
        "--skip-regeneration",
        action="store_true",
        help="skip freetype-py based deterministic regeneration checks",
    )
    args = parser.parse_args()
    root = args.root.resolve()

    for relative, expected in EXPECTED_FILES.items():
        path = root / relative
        if not path.is_file():
            raise ValueError(f"missing source font: {relative}")
        actual = sha256_file(path)
        if actual != expected:
            raise ValueError(f"SHA-256 mismatch for {relative}: {actual}")
    print("Source-font hashes: PASS")

    obsolete = root / "fonts.c"
    if obsolete.exists():
        raise ValueError("obsolete fonts.c is still present; remove it to avoid duplicate font definitions")

    droid_path = root / "fonts_droidsansmono_data.c"
    awesome_path = root / "fonts_fontawesome_f080_data.c"
    if not droid_path.is_file() or not awesome_path.is_file():
        raise ValueError("missing generated font data file")

    droid_text = droid_path.read_text(encoding="utf-8", errors="strict")
    awesome_text = awesome_path.read_text(encoding="utf-8", errors="strict")

    found_droid_sizes = set(int(size) for size in re.findall(r"DroidSansMono_(\d+)", droid_text))
    if found_droid_sizes != set(DROID_SIZES):
        raise ValueError(f"unexpected Droid Sans Mono sizes: {sorted(found_droid_sizes)}")

    for size in DROID_SIZES:
        symbol = f"DroidSansMono_{size}"
        descriptor = extract_descriptor(droid_text, symbol)
        if descriptor[2:6] != (32, 126, 176, 190):
            raise ValueError(f"unexpected character ranges for {symbol}: {descriptor[2:6]}")
        data = extract_array(droid_text, symbol, "data")
        index = extract_array(droid_text, symbol, "index")
        if not data or not index:
            raise ValueError(f"empty generated arrays for {symbol}")

    awesome = "AwesomeF080_40"
    descriptor = extract_descriptor(awesome_text, awesome)
    if descriptor[2:6] != (0, 43, 0, 0):
        raise ValueError(f"unexpected character ranges for {awesome}: {descriptor[2:6]}")
    extract_array(awesome_text, awesome, "data")
    extract_array(awesome_text, awesome, "index")
    found_awesome_sizes = set(re.findall(r"AwesomeF080_(\d+)", awesome_text))
    if found_awesome_sizes != {"40"}:
        raise ValueError(f"unexpected AwesomeF080 sizes: {sorted(found_awesome_sizes)}")
    print("Embedded font symbols and ranges: PASS")
    verify_textbox_utf8_mapping(root)

    if args.skip_regeneration:
        print("Font regeneration checks: SKIPPED")
    else:
        run_generator(root, "generate_droidsansmono.py", "Droid Sans Mono")
        run_generator(root, "generate_fontawesome_f080_40.py", "Font Awesome")

    print("Font provenance verification completed successfully.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
