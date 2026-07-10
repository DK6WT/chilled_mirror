# TP-3000 source package record

- Project version: **0.50.0**
- Firmware date: **2026-07-11**
- Publication-preparation date: **2026-07-11**
- Package type: **public source tree / GitHub upload candidate**
- Suggested archive name: `TP-3000_0.50.0_SOURCE.zip`
- Prepared from internal snapshot: `Taupunktspiegel_StandV1_2026-07-03_V0.50.0_21.zip`
- Input archive SHA-256: `09574ea1a0daed8ffc241b32b4ba3ebb56fdc7dbcd7116b5ac169d024c06b128`
- Project license: `GPL-3.0-only`
- Historical base: LJ2000M T_2.06c GSL1680
- Detailed provenance: `THIRD_PARTY_NOTICES.md`
- License and trademark texts: `LICENSE`, `LICENSES/` and bundled library folders

This record describes the source tree prepared for a first GitHub upload. It is
a best-effort engineering record and not legal advice.

## Version scheme

The firmware-visible version remains `0.50.0`. Historical development snapshot
names such as `_21` are retained only in provenance and `CHANGELOG.md`; they are
not part of the public semantic version.

## GPLv3-only selection

Project-owned, LJ2000M-derived and GSL1680 driver integration files use:

```text
SPDX-License-Identifier: GPL-3.0-only
```

The complete GNU GPL version 3 text is in `LICENSE` and
`LICENSES/GPL-3.0.txt`. Third-party components retain their own licenses.
Components offered as GPL-3.0-or-later, such as RA8875, are used under version 3
for this distribution.

## GSL1680 driver and panel firmware

The bundled driver is based on Skallwar/GSL1680 and wolfmanjm/GSL1680 and is
adapted for Teensy Wire1. Both upstream repositories are currently GPLv3. The
TP-3000 driver files are documented as:

```text
GPL-3.0-only
```

The separate EastRising/BuyDisplay panel firmware is not included. The absent
local-only path is:

```text
external/GSL1680/gslX680_311_5_F.h
```

The tested vendor file had SHA-256:

```text
ea756ce8d96631337fa09abe22c4e95aaad86cb706b561a52dfd143e751edd96
```

Its redistribution rights are not conclusively documented. Users obtain the
firmware matching their panel directly from the display manufacturer and place
it locally under the original filename. `.gitignore` excludes the file. No
compiled HEX/BIN image containing the vendor data is included.

## Trademark wording

ALMEMO® is a product name and trademark used by AHLBORN Mess- und
Regelungstechnik GmbH. AMR WinControl is developed by akrobit software GmbH and
distributed for ALMEMO systems through AHLBORN. Both names are used only to
describe optional interface functions: an optional serial connection to ALMEMO
measuring instruments and measurement output in ALMEMO V6 format for use with
AMR WinControl over RS232 or Ethernet/TCP. This is not a statement of general or
complete ALMEMO compatibility. TP-3000 is independent from AHLBORN and akrobit
and is not supported, sponsored, endorsed, approved, tested or certified by
either company. The complete bilingual notice is in:

```text
LICENSES/ALMEMO-TRADEMARK-NOTICE.txt
```

## Bundled local libraries

- ProtoCentral ADS1262 library 2.0.0 — MIT
- RTC RV3129 Arduino Library 1.0.0 — MIT
- SparkFun BMP581 Arduino Library 1.0.1 — MIT
- Bosch BMP5 API inside the SparkFun library — BSD-3-Clause
- WDT_T4 / Watchdog_t4 0.1 by Antonio Brewer — MIT

The WDT_T4 copy is included under `libraries/WDT_T4/` together with its original
MIT `LICENSE` file. TP-3000 prefers this bundled copy and falls back to an
externally installed `Watchdog_t4.h` when necessary.

RA8875 0.7.11 is supplied externally by Teensyduino and remains licensed
GPL-3.0-or-later. The expected Boards Manager path is:

```text
Arduino15/packages/teensy/hardware/avr/1.62.0/libraries/RA8875
```

## Runtime assets and fonts

The AI-generated Earth startup image used NASA Earth Observatory's Blue Marble
(2002), Western Hemisphere, as visual reference material. It is not an original
NASA image and is not endorsed by NASA. The GPL-3.0-only designation applies to
the TP-3000-specific source representation, processing, conversion and
integration, and does not relicense NASA source material. Current asset checksums
are recorded in `README.md` and `LICENSES/EARTH-IMAGE-NOTICE.txt`.

The font data is separated by license. `fonts_droidsansmono_data.c` is
independently generated from the verified AOSP Droid Sans Mono TTF under
Apache-2.0. `fonts_fontawesome_f080_data.c` is independently generated from the
verified Font Awesome 4.5.0 TTF under OFL-1.1. `fonts.h` contains only
GPL-3.0-only declarations.

The Droid generator emits all 18 sizes, ASCII and TP-3000 range 0xB0-0xBE. The
small-percent glyph is generated from the normal percent glyph in the same run.
The Font Awesome subset contains only U+F090, U+F0AA and U+F0AB at the existing
TP-3000 byte positions. Neither generator reads or copies historical
ILI9341_fonts C tables.

The exact licensed source fonts are included under `third_party/font_sources/`.
Source checksums, license assignments and reproducibility commands are recorded
in `THIRD_PARTY_NOTICES.md`, `LICENSES/` and `tools/font_provenance/`.

## Publication checks completed

- panel firmware file absent,
- no nested ZIP/archive, HEX, BIN, ELF or object file present,
- no CSV/log measurement data present,
- GSL1680 driver notices changed from the obsolete Apache/GPL expression to
  GPL-3.0-only,
- WDT_T4 documented as bundled MIT code; removed examples are no longer claimed as included,
- source-font hashes and independent regeneration of both packed-font files verified,
- public-package wording synchronized across README, build instructions,
  notices, TFT and web license pages,
- accidental internal audit artifact `T_calls.txt` removed,
- centralized UTF-8 mapping and both font generators verified with the bundled provenance checker,
- package reopened and checked after creation.

## Build limitation of the public tree

A complete hardware build cannot be performed from the public tree alone,
because the panel-specific vendor firmware is intentionally absent. After the
user installs that local file, build with Arduino IDE 2.3.10 and Teensyduino
1.62.0 and archive the verbose library selection, warnings and FLASH/RAM output.
