# Attribution, license and packaging audit – TP-3000 0.50.0

Scope: public-source candidate prepared on **2026-07-11** from the freshly
unpacked archive:

```text
Taupunktspiegel_StandV1_2026-07-03_V0.50.0_21.zip
SHA-256: 09574ea1a0daed8ffc241b32b4ba3ebb56fdc7dbcd7116b5ac169d024c06b128
```

Firmware-visible version and date remain **0.50.0 / 11.07.2026**. This is a
best-effort engineering audit and not legal advice.

## Completed

- Confirmed the TP-3000 project selection as `GPL-3.0-only` and retained the
  complete GNU GPL version 3 text.
- Corrected the GSL1680 driver chain: Skallwar/GSL1680 and
  wolfmanjm/GSL1680 are treated as GPLv3 upstreams; the obsolete
  `Apache-2.0 AND GPL-3.0-only` expression was removed from the driver files and
  package documentation.
- Preserved attribution to Skallwar/ESTBLC, wolfmanjm and upstream contributors.
- Confirmed that the panel-specific file
  `external/GSL1680/gslX680_311_5_F.h` is absent from the public tree and is
  excluded by `.gitignore`.
- Recorded the tested vendor-file SHA-256
  `ea756ce8d96631337fa09abe22c4e95aaad86cb706b561a52dfd143e751edd96`
  only as a local identification reference.
- Confirmed that no HEX, BIN, ELF, object file, nested archive, CSV/log file or
  duplicate panel firmware table is included.
- Corrected the wrapper documentation: the vendor keyword `code` is mapped to
  `PROGMEM`, not to an empty macro.
- Confirmed Arduino IDE 2.3.10 and Teensyduino board package 1.62.0 as the
  documented development environment.
- Confirmed the Earth visual-reference variant as Western Hemisphere, clarified
  that GPL-3.0-only covers the TP-3000-specific representation and protectable
  original elements without relicensing NASA source material, and synchronized
  the current Earth checksums.
- Retained the bilingual ALMEMO/WinControl trademark notice and replaced broad
  compatibility wording with a precise description of the serial input and
  ALMEMO-V6-format output. Corrected the public attribution: ALMEMO is associated
  with AHLBORN; AMR WinControl is developed by akrobit and distributed for
  ALMEMO systems through AHLBORN.
- Confirmed bundled local library metadata:
  - ProtoCentral ADS1262 2.0.0 — MIT
  - RTC RV3129 1.0.0 — MIT
  - SparkFun BMP581 1.0.1 — MIT
  - Bosch BMP5 API — BSD-3-Clause
  - WDT_T4 / Watchdog_t4 0.1 — MIT, with original `LICENSE`
- Retained RA8875 0.7.11 attribution as GPL-3.0-or-later and the expected
  Teensyduino path `avr/1.62.0/libraries/RA8875`.
- Checked the TFT license-page line lengths and retained two-line UP/DOWN
  scrolling.
- Bundled the exact AOSP Droid Sans Mono source font (Apache-2.0) and the
  exact Font Awesome 4.5.0 TTF/OTF source fonts (OFL-1.1), with hashes and
  license texts. The unchanged upstream Font Awesome release README is
  separately documented as CC BY 3.0 Unported in
  `LICENSES/CC-BY-3.0-NOTICE.txt`.
- Split the packed font data by license: `fonts_droidsansmono_data.c` contains
  independently generated Droid Sans Mono data (Apache-2.0),
  `fonts_fontawesome_f080_data.c` contains independently generated Font Awesome
  data (OFL-1.1), and `fonts.h` is the GPL-3.0-only declaration layer.
- Replaced the historical Font Awesome tables with an independently generated
  `AwesomeF080_40` subset. The GPL-3.0-only generator reads only the verified
  official TTF and emits the three glyphs actually used by TP-3000 at their
  existing byte positions. No historical `font_AwesomeF080.c` data, index,
  descriptor, comment or wrapper is read or copied.
- Added `generate_droidsansmono.py`, which regenerates all 18 Droid sizes,
  ASCII and TP-3000 extended characters directly from the verified AOSP TTF.
  The small-percent glyph is derived from the freshly rendered normal percent
  sign in the same generator run.
- Removed the 17 unused Font Awesome sizes and unused glyphs. Neither generated
  font data file now reads or copies historical ILI9341_fonts C tables.
- Removed the accidental internal audit artifact `T_calls.txt`, corrected the
  WDT_T4 package description, and synchronized the font verifier with the shared
  UTF-8 conversion helper used by both direct TFT output and `TextBox::print()`.

## Functional-change check

The publication preparation changes license comments, documentation, the
Droid/Font-Awesome raster data and the TextBox UTF-8 character mapping. Intended
visible changes are the independently rasterized fonts and availability of the
extended Droid characters. It does not intentionally change measurement,
calibration, control, safety, logging, serial-protocol or Ethernet data-path
behavior.

## Public firmware exclusion

This repository does not contain:

```text
external/GSL1680/gslX680_311_5_F.h
```

Users must obtain the correct original file for their exact panel directly from
the display manufacturer and install it locally. A firmware image compiled with
that local file contains the vendor data; no such compiled image is included in
this source package.

## Remaining verification limitation

A complete Teensy build was not performed for this public tree because the
panel-specific vendor firmware is intentionally absent. After local installation
of that file, a release builder should archive the verbose compiler/library
selection, warnings, memory output and resulting firmware checksum.

Both distributed packed-font files are independently generated from the
included, clearly licensed source fonts. The historical font archive is not
distributed, is not read by either generator and is no longer part of the
provenance chain. Exact source hashes, generators and verification commands are
included. External build tools and Teensyduino components retain their own
licenses and are not redistributed here, except for the explicitly bundled
local libraries.
