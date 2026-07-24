# Third-Party Notices und Herkunftsnachweise

Dieses Dokument erfasst die im TP-3000-V0.50.1-Quellrelease enthaltenen oder zum Build benötigten Fremdkomponenten. Der TP-3000-Projektcode steht unter `GPL-3.0-only`; Fremdkomponenten behalten ihre eigenen Lizenzen. Volltexte liegen in `LICENSES/` oder direkt im jeweiligen Komponentenordner.

## 1. LJ2000M T_2.06c

Der TP-3000 basiert historisch auf LJ2000M T_2.06c.

- ursprüngliche Autoren: Loftur E. Jonasson und J.G. Holstein
- ursprüngliche Lizenz: GNU GPL Version 3 oder später
- TP-3000-Projektlizenz für den kombinierten und weiterentwickelten Stand: GNU GPL Version 3 only
- vollständiger Projektlizenztext: `LICENSE` und `LICENSES/GPL-3.0.txt`

Bestehende Copyright-Hinweise bleiben in den abgeleiteten Quelldateien erhalten. Neue TP-3000-Module nennen zusätzlich S. Brachtl (DK6WT).

## 2. GSL1680-Touchscreen-Treiber

Die Dateien `GSL1680.cpp`, `GSL1680.h` und `GSL1680Firmware.h` basieren auf beziehungsweise integrieren GPL-kompatiblen GSL1680-Treibercode.

- Lizenz des im Repository enthaltenen Treibercodes: `GPL-3.0-only`
- Herkunft und Anpassungen: `LICENSES/GSL1680-NOTICE.txt`

### Panelspezifische Firmwaretabelle

Die Datei `gslX680_311_5_F.h` ist eine davon getrennte Herstellerkomponente für das konkrete EastRising-/BuyDisplay-Panel. Da ihre Weitergaberechte nicht eindeutig dokumentiert sind, ist sie **nicht Bestandteil des öffentlichen Repositories oder Release-ZIPs**.

Der Nutzer muss die Originaldatei direkt aus dem Herstellerpaket beziehen und lokal unter folgendem Pfad ablegen:

```text
external/GSL1680/gslX680_311_5_F.h
```

Details, Referenz-Hash und unveränderte Einbindung: `external/GSL1680/README.md` und `LICENSES/GSL1680-NOTICE.txt`.

Ein kompiliertes Firmwareabbild enthält diese Tabelle ebenfalls. Solche HEX-/BIN-Abbilder werden deshalb nicht mit dem öffentlichen Quellrelease verteilt.

## 3. Projektlokale Arduino-Bibliotheken

### ProtoCentral ADS1262 Library 2.0.0

- Pfad: `libraries/ProtoCentral_ADS1262_32-bit_precision_ADC_Library/`
- Softwarelizenz: MIT
- Originaltexte: `LICENSE`, `LICENSE.md`
- Hinweis: upstream nennt separate Hardwareunterlagen unter CERN-OHL-P v2 und Dokumentation unter CC-BY-SA-4.0; im TP-3000-Paket sind keine ProtoCentral-Hardwaredesigns enthalten

### RTC RV3129 Arduino Library 1.0.0

- Pfad: `libraries/RTC_RV3129_Arduino_Library/`
- Code: MIT
- Originaltext: `LICENSE.md`

### SparkFun BMP581 Arduino Library 1.0.1

- Pfad: `libraries/SparkFun_BMP581_Arduino_Library/`
- SparkFun-Wrapper: MIT
- Bosch BMP5 API unter `src/bmp5_api/`: BSD-3-Clause
- Originaltexte: `LICENSE.md`, `src/bmp5_api/LICENSE`
- zusätzlicher Hinweis: `LICENSES/BOSCH-BMP5-NOTICE.txt`

Die generische SparkFun-Lizenzdatei erwähnt eine Analog-Devices-SLA. Im tatsächlich gebündelten BMP581-Quellbaum wurden keine Analog-Devices-Dateien gefunden.

### micro-ecc 1.0.0

- Pfad: `libraries/TP3000_micro_ecc/`
- Copyright: Kenneth MacKay
- Lizenz: BSD-2-Clause
- Originaltext: `libraries/TP3000_micro_ecc/LICENSE.txt`
- Hinweis: `LICENSES/MICRO-ECC-NOTICE.txt`

Die TP-3000-Integration konfiguriert nur P-256 und die benötigten ECDSA-Funktionen und ergänzt Teensy-4.x-Codeplatzierung. Die kryptografischen Algorithmen wurden nicht inhaltlich neu lizenziert.

### WDT_T4 0.1

- Pfad: `libraries/WDT_T4/`
- Copyright: Antonio Brewer
- Lizenz: MIT
- Originaltext: `libraries/WDT_T4/LICENSE`

Eine vollständige Tabelle steht in `libraries/README.md`.

## 4. pako 1.0.11 und zlib

Der browserseitige TP3C1-Encoder liefert `pako_deflate.min.js` als `PROGMEM`-Raw-String in `TPethernet.ino` aus und verwendet `pako.deflate(..., {level: 9})`.

- Projekt: pako / nodeca
- Version: 1.0.11
- JavaScript-Hülle: MIT
- portierter `lib/zlib`-Kern: zlib License
- vollständiger Hinweis: `LICENSES/PAKO-NOTICE.txt`
- zlib-Lizenztext: `LICENSES/Zlib.txt`

## 5. QR-Code-Komponenten

### QRCode for JavaScript

Der Browser-Kalibrierschein enthält eine angepasste Fassung von QRCode for JavaScript.

- Copyright: Kazuhiko Arase, 2009
- Lizenz: MIT
- eingebettet in: `TPethernet.ino`
- Hinweis: `LICENSES/QRCode-for-JavaScript-NOTICE.txt`

### Project Nayuki QR Code generator

Der native TFT-QR-Generator in `TPqrCode.cpp/.h` verwendet einen reduzierten, für den TP3C1-Pfad angepassten Algorithmus von Project Nayuki.

- Copyright: Project Nayuki
- Lizenz der Grundlage: MIT
- Hinweis: `LICENSES/Nayuki-QR-NOTICE.txt`

`QR Code` ist eine eingetragene Marke von DENSO WAVE INCORPORATED. Die Bezeichnung wird nur technisch beschreibend verwendet.

## 6. Schriftarten

### Droid Sans Mono

- Quelldatei: `third_party/font_sources/DroidSansMono-AOSP.ttf`
- abgeleitete Firmwaredaten: `fonts_droidsansmono_data.c`
- Designer: Steve Matteson
- Ursprung: Android Open Source Project
- Lizenz: Apache License 2.0
- Hinweise: `LICENSES/Droid-Sans-Mono-NOTICE.txt`, `third_party/font_sources/AOSP-NOTICE.txt`

### Font Awesome 4.5.0

- Quelldateien: `third_party/font_sources/FontAwesome-4.5.0.ttf` und `.otf`
- abgeleitete Firmwaredaten: `fonts_fontawesome_f080_data.c`
- Creator: Dave Gandy / Font Awesome
- Fontlizenz: SIL Open Font License 1.1
- mitgelieferte unveränderte Upstream-README: CC BY 3.0 Unported
- Hinweise: `LICENSES/Font-Awesome-NOTICE.txt`, `LICENSES/CC-BY-3.0-NOTICE.txt`

Die Generatoren unter `tools/font_provenance/` erzeugen die gebündelten Bitmaps reproduzierbar aus den dokumentierten Quelldateien. Die früher als Referenz betrachteten ILI9341-Artefakte sind nicht Bestandteil der aktuellen Fontgenerierung; die historischen Herkunftshinweise bleiben in `LICENSES/ILI9341-*.txt` erhalten.

## 7. Earth-Startbild

- Firmwaredaten: `earth.c`
- dokumentierte PNG-Quelle: `assets/earth_blue_marble_ai_800x480.png`
- Erzeugung: OpenAI image generation am 14.06.2026
- visuelle Referenz: NASA Blue Marble / NASA Earth Observatory
- vollständiger Herkunfts-, Kennzeichnungs- und Nutzungshinweis: `LICENSES/EARTH-IMAGE-NOTICE.txt`

Das Bild wird als AI-generiert gekennzeichnet. Die NASA-Referenz begründet keine Unterstützung oder Zertifizierung des TP-3000 durch NASA.

## 8. Externe Build-Abhängigkeiten aus Arduino/Teensyduino

Nicht im Repository gebündelt sind unter anderem:

- Arduino Core und Standardbibliotheken
- Teensy Core
- RA8875
- NativeEthernet und FNET
- SD und SdFat
- EEPROM, Entropy, Metro, SPI, TimeLib und Wire

Diese Komponenten werden aus der installierten Arduino-/Teensyduino-Umgebung geladen und behalten ihre jeweiligen Upstream-Lizenzen. Der RA8875-Hinweis liegt zusätzlich in `LICENSES/RA8875-NOTICE.txt`.

## 9. ALMEMO und WinControl

Die Bezeichnungen ALMEMO und WinControl werden ausschließlich zur sachlichen Beschreibung optionaler Schnittstellen verwendet.

- ALMEMO ist eine von AHLBORN Mess- und Regelungstechnik verwendete Produktbezeichnung/Marke.
- AMR WinControl wird von akrobit entwickelt und für ALMEMO-Systeme über AHLBORN vertrieben.
- TP-3000 ist ein unabhängiges Projekt ohne Verbindung, Unterstützung, Freigabe, Prüfung oder Zertifizierung durch AHLBORN oder akrobit.
- Es werden keine Logos dieser Unternehmen verwendet.

Vollständiger Hinweis: `LICENSES/ALMEMO-TRADEMARK-NOTICE.txt`.

## 10. Volltexte und zentrale Kopien

`LICENSES/` enthält unter anderem:

- GPL-3.0
- MIT
- BSD-2-Clause
- BSD-3-Clause
- Apache-2.0
- CC-BY-SA-4.0
- OFL-1.1
- zlib License
- komponentenspezifische Herkunfts- und Markenhinweise

Bei Abweichungen ist die komponentenlokale Original-Lizenzdatei zusammen mit dem jeweiligen Quellheader maßgeblich.
