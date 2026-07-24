# Source fonts for the TP-3000 packed font tables

This directory contains the exact licensed outline-font files used by the two
reproducible generators. The firmware does not parse these files at run time.

## Droid Sans Mono

- File: `DroidSansMono-AOSP.ttf`
- Source: Android Open Source Project,
  `platform/frameworks/base/data/fonts/DroidSansMono.ttf`
- Family: Droid Sans Mono, Regular
- Designer: Steve Matteson
- License: Apache License 2.0
- Git blob ID: `b7bf5b4aa8ad5a8c8036653734d83006c6a376ee`
- SHA-256:
  `db19a1fdaba41cc4a2fec0330e5c15e71c6dd68a3ef074f4f28268828b45c862`

The AOSP notice is retained in `AOSP-NOTICE.txt`; the complete Apache-2.0 text
is in `../../LICENSES/Apache-2.0.txt`.

`../../fonts_droidsansmono_data.c` is generated directly from this TTF by:

```text
python tools/font_provenance/generate_droidsansmono.py
```

The generator emits all 18 TP-3000 sizes, ASCII and the extended range
`0xB0-0xBE`. The small percent sign is generated from the normal percent glyph
in the same run.

## Font Awesome 4.5.0

- Files: `FontAwesome-4.5.0.ttf`, `FontAwesome-4.5.0.otf`
- Source: official Font Awesome tag `v4.5.0`
- Creator: Dave Gandy / Font Awesome
- Font license: SIL Open Font License 1.1
- TTF Git blob ID: `26dea7951a73079223b50653c455c5adf46a4648`
- OTF Git blob ID: `3ed7f8b48ad9bfab52eb03822fefcd6b77d2e680`
- TTF SHA-256:
  `7b5a4320fba0d4c8f79327645b4b9cc875a2ec617a557e849b813918eb733499`
- OTF SHA-256:
  `7ed24c05432403117372891543f0cb6a7922100919e7ae077c1f3faf67658dc2`
- Release README SHA-256:
  `9f1435a68f2e88e768dced82f9cb070291fbb8f2445e03d3298d208d973d4209`

The release README is retained unmodified as `FontAwesome-4.5.0-README.md`.
That documentation file is CC BY 3.0 Unported; its central attribution and
license-URI notice is in `../../LICENSES/CC-BY-3.0-NOTICE.txt`. The TTF/OTF
files and generated glyph data are separately covered by the complete OFL-1.1
text in `../../LICENSES/OFL-1.1.txt`.

`../../fonts_fontawesome_f080_data.c` is generated directly from the TTF by:

```text
python tools/font_provenance/generate_fontawesome_f080_40.py
```

Only the three Font Awesome glyphs used by TP-3000 are emitted.

## Packed-table policy

The outline fonts in this directory are the preferred source form for the font
artwork. Both generator scripts verify the exact input hash and do not read or
copy historical ILI9341_fonts C files. The generated output is compatible with
`ILI9341_t3_font_t` and retains the license of its respective source font.
