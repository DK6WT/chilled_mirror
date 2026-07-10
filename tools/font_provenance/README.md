# Font generation and provenance verification

Both TP-3000 packed-font files are generated directly from licensed outline
fonts included under `third_party/font_sources/`. Neither generator reads or
copies historical ILI9341_fonts C tables.

Install the required Python package:

```text
python -m pip install -r tools/font_provenance/requirements.txt
```

## Droid Sans Mono

Generate all 18 sizes:

```text
python tools/font_provenance/generate_droidsansmono.py
```

Reproducibility check without rewriting the committed file:

```text
python tools/font_provenance/generate_droidsansmono.py --check
```

The generator reads only `DroidSansMono-AOSP.ttf`, verifies its SHA-256 and
creates:

```text
fonts_droidsansmono_data.c
```

Generation parameters:

```text
sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 48, 60, 72, 96
100 dpi
FreeType monochrome rasterization
ASCII 0x20-0x7E
TP-3000 range 0xB0-0xBE
ILI9341_t3_font_t-compatible packed output
```

The small percent sign at 0xB5 is generated from the freshly rasterized normal
percent sign, area-resampled to 75%, centered and baseline-aligned. Optional PBM
preview sheets can be created with:

```text
python tools/font_provenance/generate_droidsansmono.py --check \
  --preview-dir font-preview
```

## Font Awesome icon font

Generate the only required `AwesomeF080_40` subset:

```text
python tools/font_provenance/generate_fontawesome_f080_40.py
```

Check it:

```text
python tools/font_provenance/generate_fontawesome_f080_40.py --check
```

Included slots:

```text
0x10 -> U+F090  sign-in / ENTER
0x2A -> U+F0AA  chevron-circle-up
0x2B -> U+F0AB  chevron-circle-down
```

## Complete provenance check

```text
python tools/font_provenance/verify_font_provenance.py
```

This checks source-font hashes, generated symbols and character ranges, the
`TextBox::print()` UTF-8-to-local-byte mappings, and byte-for-byte regeneration
of both committed C files.

The committed output was generated with FreeType 2.13.2. A different FreeType
version may rasterize a few edge pixels differently; in that case the result is
still derived from the same licensed source font but will not be byte-identical
to the committed output.

The generator scripts are GPL-3.0-only. Their licenses do not replace the
Apache-2.0 or OFL-1.1 licenses of the generated font-derived data.
