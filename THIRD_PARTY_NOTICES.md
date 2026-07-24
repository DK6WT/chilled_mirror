# Third-Party Notices und Herkunftsnachweise

Dieses Dokument ergänzt die Dateikopf-Hinweise. Es ist eine technische
Herkunfts- und Lizenzübersicht und keine Rechtsberatung.

## 1. LJ2000M T_2.06c GSL1680

TP-3000 verwendet beziehungsweise verändert Teile der Bedien-, Display-,
EEPROM-, Touchscreen- und Menüarchitektur von **LJ2000M T_2.06c GSL1680**.

Urheberhinweise aus den Originaldateien:

- Copyright (C) 2014, 2015 und 2016 Loftur E. Jonasson, TF3LJ / VE2LJX
  (je nach Quelldatei)
- Copyright (C) 2017-2021 J.G. Holstein
- Lizenz: GNU GPL Version 3 oder später

Die ursprünglichen LJ2000M-Beiträge behalten diese `-or-later`-Freigabe. Für
die kombinierte TP-3000-Distribution wird ausdrücklich GNU GPL Version 3 only
gewählt.

Wesentliche Zuordnung:

| TP-3000-Datei | LJ2000M-Ursprung |
|---|---|
| `TP-3000.ino` | `LJ2000M_2.06c_GSL1680.ino` |
| `TP_T.h` | `PSWR_T.h` |
| `TPdisplay.ino` | `PSWRdisplay.ino` |
| `TPmenu*.ino` | `PSWRmenu.ino` und spätere TP-3000-Erweiterungen |
| `TPprintFunc.ino` | `PSWRprintFunc.ino` |
| `TPtft.cpp`, `TPtft.h` | `PSWRtft.cpp`, `PSWRtft.h` |
| `TPtouchscreen.ino` | `PSWRtouchscreen.ino` und `buttons.ino` |
| `TPusbSerial.ino` | `PSWRusbSerial.ino` |
| `EEPROMAnything.h` | `_EEPROMAnything.h` |
| `fonts_droidsansmono_data.c`, `fonts_fontawesome_f080_data.c`, `fonts.h` | unabhängig erzeugte Fontdaten plus gemeinsame TP-3000-Deklarationen |
| `earth.c` | `earth.c` |

## 2. GSL1680-Touchscreen-Treiber

Dateien: `GSL1680.cpp`, `GSL1680.h`, `GSL1680Firmware.h`

Der mitgeführte Treiber basiert auf `Skallwar/GSL1680` (ESTBLC), das wiederum
`wolfmanjm/GSL1680` als Quelle nennt, und wurde für Teensy/Wire1 sowie die
TP-3000-Quellstruktur angepasst.

Aktuelle Upstream-Repositories:

- https://github.com/Skallwar/GSL1680 — GNU GPL Version 3
- https://github.com/wolfmanjm/GSL1680 — GNU GPL Version 3

Skallwar erläuterte im Juli 2026, dass sein Projekt als Copy-and-paste-Basis von
`wolfmanjm/GSL1680` begann und deshalb von Anfang an GPLv3 hätte sein müssen.
Er korrigierte die öffentliche Repository-Lizenz entsprechend. Urheberrechte an
Beiträgen anderer Mitwirkender verbleiben bei den jeweiligen Autoren.

Für die TP-3000-Treiberfassung gilt:

```text
SPDX-License-Identifier: GPL-3.0-only
```

Lizenz- und Herkunftsdokumente:

- `LICENSES/GPL-3.0.txt`
- `LICENSES/GSL1680-NOTICE.txt`
- https://github.com/Skallwar/GSL1680
- https://github.com/wolfmanjm/GSL1680

`LICENSES/Apache-2.0.txt` bleibt für andere Paketbestandteile, insbesondere
Droid Sans Mono, erhalten; die Apache-Lizenz wird nicht mehr dem GSL1680-
Treibercode zugeordnet.

**Dank:** Das TP-3000-Projekt dankt Jim Morris (`wolfmanjm`) und
Skallwar/ESTBLC für die offene und hilfreiche Klärung. Dadurch kann die
Treiber-Rechtekette konsistent als GPLv3 dokumentiert werden.

## 3. GSL1680-Panel-Firmware – nicht im öffentlichen Paket

Die panelspezifische GSL1680-Firmwaretabelle ist vom GPL-lizenzierten
Treibercode getrennt zu betrachten. Ihre eigenständige Weitergabeberechtigung
und der ursprüngliche Rechteinhaber sind nicht sicher dokumentiert.

Dieses öffentliche Quellpaket enthält die Herstellerdatei nicht:

```text
external/GSL1680/gslX680_311_5_F.h
```

Die auf der TP-3000-Hardware getestete Referenzfassung war durch
`IC: GSL1680f`, `DTAE: Aug-04-2017`, `VER: 1.1` und folgende
SHA-256-Prüfsumme gekennzeichnet:

```text
ea756ce8d96631337fa09abe22c4e95aaad86cb706b561a52dfd143e751edd96
```

Die Prüfsumme dient nur zur Identifikation der getesteten lokalen Datei. Ein
Nutzer bezieht die zu seinem konkreten Display gehörende Originaldatei direkt
von EastRising/BuyDisplay und kopiert sie unverändert unter dem Originalnamen an
den genannten lokalen Pfad. `.gitignore` verhindert versehentliche Commits.

Die Originaldatei wird **1:1 verwendet**. Das Schlüsselwort `code` wird nicht
aus der Herstellerdatei entfernt und die Seiten `0xE0` bis `0xE6` bleiben wie
im Original auskommentiert. Der Wrapper `GSL1680Firmware.h` ordnet `code` nur
während des Includes dem Makro `PROGMEM` zu, ohne die Herstellerdatei zu
verändern.

Die Datei ist **nicht Bestandteil der TP-3000-GPL-Lizenzierung**. TP-3000
erteilt daran keine Lizenz und behauptet nicht, dass sie unter GPL steht. Ein
mit der lokalen Datei kompiliertes HEX-/BIN-Abbild enthält ebenfalls die
Herstellerdaten und wird daher nicht mit diesem öffentlichen Quellpaket
verteilt. Bezug und Installation sind in `external/GSL1680/README.md`
dokumentiert.

Das EastRising-Datenblatt zum Modul `ER-TFTM050A2-3-3661` nennt den GSL1680X als
Touchcontroller und verweist für zugehörige Interface-Dokumente und Democode auf
die Herstellerseite.

## 4. Schriftarten

Dateien und Prüfhilfen:

- `fonts_droidsansmono_data.c` — unabhängig erzeugtes Droid Sans Mono,
  Apache-2.0
- `fonts_fontawesome_f080_data.c` — unabhängig erzeugter Font-Awesome-Teil,
  OFL-1.1
- `fonts.h` — gemeinsame Deklarationen, GPL-3.0-only
- `third_party/font_sources/`
- `tools/font_provenance/generate_droidsansmono.py`
- `tools/font_provenance/generate_fontawesome_f080_40.py`
- `tools/font_provenance/verify_font_provenance.py`

### 4.1 Droid Sans Mono

- Designer: Steve Matteson
- Quelle: Android Open Source Project, `DroidSansMono.ttf`
- Lizenz: Apache License 2.0
- mitgelieferte Quelldatei:
  `third_party/font_sources/DroidSansMono-AOSP.ttf`
- SHA-256:
  `db19a1fdaba41cc4a2fec0330e5c15e71c6dd68a3ef074f4f28268828b45c862`

`tools/font_provenance/generate_droidsansmono.py` erzeugt alle 18 Größen direkt
aus dieser TTF bei 100 dpi mit monochromer FreeType-Rasterung. Der Generator
prüft vorab den Quellhash und liest keine historische `font_DroidSansMono.c`.
Die Ausgabe liegt ausschließlich in `fonts_droidsansmono_data.c`.

Der ASCII-Bereich ist `0x20` bis `0x7E`. Der zweite Bereich `0xB0` bis `0xBE`
enthält Gradzeichen, Omega, kleine und große deutsche Umlaute, ß, Mikro,
Plus/Minus, Delta sowie ≤ und ≥. Das kleine Prozentzeichen an `0xB5` wird in
jedem Lauf aus dem frisch gerasterten normalen Prozentzeichen abgeleitet und
nicht aus einer früheren Bitmap übernommen.

### 4.2 Font Awesome

- Creator: Dave Gandy / Font Awesome
- Quelle: offizieller Release-Stand Font Awesome `v4.5.0`
- Font-Lizenz: SIL Open Font License 1.1
- unverändert mitgelieferte Upstream-README: CC BY 3.0 Unported
- zentrale Hinweise: `LICENSES/Font-Awesome-NOTICE.txt`, `LICENSES/CC-BY-3.0-NOTICE.txt`
- mitgelieferte Quelldateien:
  `FontAwesome-4.5.0.ttf` und `FontAwesome-4.5.0.otf`
- TTF SHA-256:
  `7b5a4320fba0d4c8f79327645b4b9cc875a2ec617a557e849b813918eb733499`
- OTF SHA-256:
  `7ed24c05432403117372891543f0cb6a7922100919e7ae077c1f3faf67658dc2`

TP-3000 verwendet ausschließlich `AwesomeF080_40` und darin nur U+F090,
U+F0AA und U+F0AB an den vorhandenen Bytepositionen 0x10, 0x2A und 0x2B.
`tools/font_provenance/generate_fontawesome_f080_40.py` erzeugt Daten, Index
und Deskriptor unmittelbar aus der geprüften offiziellen TTF. Es liest keine
historische `font_AwesomeF080.c`.

### 4.3 Packed-Format und historische Referenz

Beide Generatoren schreiben selbst ein mit `ILI9341_t3_font_t` kompatibles
Packed-Format. Die Generatoren sind GPL-3.0-only; die erzeugten
fontabgeleiteten Daten behalten Apache-2.0 beziehungsweise OFL-1.1.

Reproduzierbarkeit:

```text
python -m pip install -r tools/font_provenance/requirements.txt
python tools/font_provenance/generate_droidsansmono.py --check
python tools/font_provenance/generate_fontawesome_f080_40.py --check
python tools/font_provenance/verify_font_provenance.py
```

Das frühere `ILI9341_fonts-master.zip` ist nicht Bestandteil des öffentlichen
Pakets und wird von keinem Generator gelesen. Seine fehlende allgemeine
Top-Level-Lizenz ist damit für die verteilten TP-3000-Fontdaten keine
Lizenzgrundlage und keine verbleibende Provenienzabhängigkeit. Das historische
Archiv kann unabhängig davon als optische Vergleichsreferenz betrachtet
werden.

Lizenz- und Herkunftsdokumente:

- `LICENSES/Apache-2.0.txt`
- `LICENSES/OFL-1.1.txt`
- `LICENSES/GPL-3.0.txt`
- `LICENSES/ILI9341-FONTS-NOTICE.txt`
- `LICENSES/ILI9341-CONVERTER-NOTICE.txt`
- `LICENSES/Droid-Sans-Mono-NOTICE.txt`
- `LICENSES/Font-Awesome-NOTICE.txt`

## 5. AI-generiertes Earth-Startbild `earth.c`

Dateien:

- `earth.c` – 800 x 480 RGB565-Daten für die Firmware
- `assets/earth_blue_marble_ai_800x480.png` – dokumentierte PNG-Quelle
- `LICENSES/EARTH-IMAGE-NOTICE.txt` – vollständiger Herkunfts- und Nutzungshinweis

Das frühere, aus dem LJ2000M-Altbestand übernommene und nicht eindeutig
identifizierbare Earth-Bild wurde entfernt. Das neue Bild wurde am 14.06.2026
mit **OpenAI image generation** für den TP-3000 erzeugt und anschließend auf
800 x 480 Pixel skaliert sowie unverändert in RGB565 umgewandelt.

Aktuelle SHA-256-Prüfsummen:

```text
assets/earth_blue_marble_ai_800x480.png
bf5044d5b65629b0413efc9fe09e8b62ac408bc5b674b9cb37626331b44e73b6

earth.c
4f7b90e2b18c192c9ae3b47ce960d0ed223b34d26c0bdd01a4609a4acd60e32f
```

Als visuelles Quellenmaterial beziehungsweise Referenz wurde verwendet:

- **NASA Earth Observatory**
- **The Blue Marble (2002): True-color global imagery at 1km resolution**
- Variante: Western Hemisphere
- Quelle: https://science.nasa.gov/earth/earth-observatory/the-blue-marble-true-color-global-imagery-at-1km-resolution/

Credits des NASA-Ausgangsmaterials:

- NASA Goddard Space Flight Center
- Image by Reto Stöckli (land surface, shallow water, clouds)
- Enhancements by Robert Simmon (ocean color, compositing, 3D globes, animation)
- Data and technical support: MODIS Land Group; MODIS Science Data Support Team;
  MODIS Atmosphere Group; MODIS Ocean Group
- Additional data: USGS EROS Data Center (topography); USGS Terrestrial Remote
  Sensing Flagstaff Field Center (Antarctica); Defense Meteorological Satellite
  Program (city lights)

NASA beschreibt die Blue-Marble-Ausgangsbilder als frei verfügbar für
Bildung, Wissenschaft, Museen und die Öffentlichkeit. Ergänzend gelten die
NASA Images and Media Usage Guidelines:

https://www.nasa.gov/nasa-brand-center/images-and-media/

Dort wird verlangt beziehungsweise empfohlen, NASA als Quelle des verwendeten
Ausgangsmaterials zu nennen, keine Unterstützung eines Produkts durch NASA zu
suggerieren und AI-generierte Ergebnisse als solche zu kennzeichnen. Entsprechend
gilt für das TP-3000-Bild:

- Das TP-3000-Bild ist **AI-generiert** und wird OpenAI image generation
  zugeschrieben, nicht NASA.
- Die NASA-Blue-Marble-Darstellung wird ausschließlich als verwendetes
  Quellenmaterial beziehungsweise visuelle Referenz offengelegt.
- NASA hat das Ergebnis nicht geprüft, genehmigt oder unterstützt.
- Das Bild enthält kein NASA-Logo, keine NASA-Wortmarke und kein NASA-Siegel.

Nach den OpenAI Europe Terms of Use besitzt der Nutzer gegenüber OpenAI und im
gesetzlich zulässigen Umfang das erzeugte Output. Die Kennzeichnung
`GPL-3.0-only` gilt für die TP-3000-spezifische Quelldarstellung, Auswahl,
Bearbeitung, Skalierung, RGB565-Konvertierung und Integration sowie für etwaige
schutzfähige eigene Bestandteile des erzeugten Bildes. NASA-Ausgangsmaterial
wird dadurch nicht neu lizenziert und bleibt den jeweils geltenden
NASA-Nutzungsrichtlinien unterworfen; gemeinfreie Bestandteile bleiben, soweit
anwendbar, gemeinfrei. Dies ist keine Zusicherung, dass AI-Ausgaben einzigartig
sind oder keine Rechte Dritter berühren.

## 6. Displaybibliothek und mitgelieferte lokale Build-Bibliotheken

### RA8875 0.7.11 – externe Teensyduino-Abhängigkeit

- Autor: Max MC Costa / sumotoy
- Copyright-Hinweis im Quellkopf: Copyright (C) 2014 egidio massimo costa
- Ursprung: https://github.com/sumotoy/RA8875
- Lizenz: GNU GPL Version 3 oder später (`GPL-3.0-or-later`)
- Projekt-Notice: `LICENSES/RA8875-NOTICE.txt`
- Lizenztext: `LICENSES/GPL-3.0.txt`

TP-3000 verwendet die mit dem installierten Teensyduino-Boardpaket gelieferte
Bibliothek. Für die aktuelle Paketversion 1.62.0 lautet der erwartete Pfad:

```text
Arduino15/packages/teensy/hardware/avr/1.62.0/libraries/RA8875
```

Die zuvor im Projektordner mitgeführte Kopie unter `libraries/RA8875` wurde vom
geprüften Build nicht ausgewählt und deshalb als unnötige Dublette entfernt.
Die RA8875-Quellen werden mit diesem TP-3000-Paket nicht erneut verteilt. Die
aktuelle Arbeitsumgebung verwendet Arduino IDE 2.3.10 und Teensyduino 1.62.0.
RA8875 0.7.11 bleibt als verwendeter Bibliotheksstand dokumentiert.

Die GPL-3.0-or-later der Displaybibliothek ist mit der Projektlizenz
GPL-3.0-only vereinbar; für die TP-3000-Distribution wird Version 3 gewählt. Beim Weitergeben eines vollständigen Binärprodukts
sind die jeweils einschlägigen GPL-Pflichten und der korrespondierende Quellcode
zu berücksichtigen.

### ProtoCentral ADS1262 Library 2.0.0

- Pfad: `libraries/ProtoCentral_ADS1262_32-bit_precision_ADC_Library`
- Autor: ProtoCentral Electronics
- Ursprung: https://github.com/Protocentral/ProtoCentral_ads1262
- Lizenz: MIT
- Lizenzdateien im Bibliotheksordner

TP-3000 verwendet daraus `ads1262.h` beziehungsweise die zugehörigen
Registerdefinitionen und Quellbestandteile für den ADS1263-Treiberpfad.

### RTC RV3129 Arduino Library 1.0.0

- Pfad: `libraries/RTC_RV3129_Arduino_Library`
- Autor: Jacob English; Fork der SparkFun RV-1805-C3 Library
- Ursprung: https://github.com/OUIDEAS/SparkFun_RV-3129_Arduino_Library
- Lizenz: MIT
- Lizenzdatei: `libraries/RTC_RV3129_Arduino_Library/LICENSE.md`
- TP-3000-Patch: Die gebündelte Kopie ist in `src/RV3129.h` und
  `src/RV3129.cpp` dokumentiert für stabilen 24h-Betrieb geändert. `begin()`
  erzwingt nicht mehr 12h, Stundenregister werden bei 12h/24h-Umwandlungen
  maskiert und 00:xx/12:xx werden korrekt behandelt. Die ursprüngliche
  MIT-Lizenz und Attribution bleiben erhalten.

### SparkFun BMP581 Arduino Library 1.0.1

- Pfad: `libraries/SparkFun_BMP581_Arduino_Library`
- Autor: SparkFun Electronics
- Ursprung: https://github.com/sparkfun/SparkFun_BMP581_Arduino_Library
- Lizenz: MIT
- Lizenzdatei: `libraries/SparkFun_BMP581_Arduino_Library/LICENSE.md`

Die Unterkomponente `src/bmp5_api/` stammt von Bosch Sensortec GmbH und steht
unter BSD-3-Clause. Der vollständige Lizenztext liegt in:

```text
libraries/SparkFun_BMP581_Arduino_Library/src/bmp5_api/LICENSE
```

Die drei Sensorbibliothekskopien wurden nur um Beispiele, Hardwaredaten,
CI-/GitHub-Metadaten und andere für den Build nicht benötigte Dateien gekürzt.
Ihre aktiven Quelltexte wurden bei dieser Bereinigung nicht funktional geändert.

### WDT_T4 / Watchdog_t4 0.1

- Pfad: `libraries/WDT_T4`
- Autor: Antonio Brewer (`tonton81`)
- Ursprung: https://github.com/tonton81/WDT_T4
- Lizenz: MIT
- Lizenzdatei: `libraries/WDT_T4/LICENSE`

TP-3000 bevorzugt diese projektlokale Kopie für den Hardware-Watchdog und fällt
bei Bedarf auf eine global installierte `Watchdog_t4.h` zurück. Nicht benötigte
Upstream-Beispiele und CI-/GitHub-Metadaten sind in der gebündelten Kopie nicht
enthalten; die benötigten Quell-, README-, Metadaten- und MIT-Lizenzdateien
bleiben erhalten.

## 7. Externe Toolchain-Abhängigkeiten

Die aktuelle Arbeitsumgebung verwendet Arduino IDE 2.3.10 und das
Teensyduino-Boardpaket 1.62.0. PJRC bezeichnet die Veröffentlichung als
Teensyduino 1.62.

Der Sketch verwendet aus dieser extern installierten Umgebung insbesondere:

- Teensy-Core und Compiler-/Linker-Toolchain,
- SPI, Wire und EEPROM,
- SD und SdFat,
- NativeEthernet und FNET,
- Metro und Time,
- RA8875 0.7.11.


WDT_T4 wird nicht aus der externen Toolchain benötigt, weil eine
MIT-lizenzierte Kopie im Projektordner mitverteilt wird.

Die oben genannten Teensyduino-/Arduino-Komponenten sind nicht im
TP-3000-Projektordner dupliziert. Sie behalten
ihre jeweiligen Urheber- und Lizenzbedingungen in der installierten
Teensyduino-/Arduino-Distribution. Die Entwicklungswerkzeuge selbst werden
ebenfalls nicht mit dem TP-3000-Quellpaket verteilt.

Vor einem öffentlichen Release werden die exakten Einzelversionsnummern, die
ausgewählten Bibliothekspfade sowie die FLASH-/RAM-Ausgabe aus einer
ausführlichen 0.50.0-Kompilierung archiviert.

## 8. Optionale ALMEMO-/WinControl-Anbindung und Markenhinweis

ALMEMO® ist eine von der AHLBORN Mess- und Regelungstechnik GmbH verwendete
Produktbezeichnung und Marke. AMR WinControl (im Gerät kurz `WinControl`) wird
von der akrobit software GmbH entwickelt und für ALMEMO-Systeme über AHLBORN
vertrieben.

TP-3000 ist ein unabhängiges Projekt und steht in keiner geschäftlichen oder
organisatorischen Verbindung zu AHLBORN oder akrobit. Das Projekt wird von
diesen Unternehmen weder unterstützt, gesponsert, freigegeben, geprüft noch
zertifiziert.

Die Bezeichnungen ALMEMO® und AMR WinControl werden ausschließlich zur
sachlichen Beschreibung optionaler Schnittstellenfunktionen verwendet: der
seriellen Anbindung an ALMEMO-Messgeräte sowie der Messwertausgabe im
ALMEMO-V6-Format zur Nutzung mit AMR WinControl über RS232 oder Ethernet/TCP.
Es werden keine Rechte an den Bezeichnungen oder Marken ALMEMO oder WinControl
beansprucht. Es wird kein ALMEMO-, WinControl-, AHLBORN- oder akrobit-Logo
verwendet. Die Beschreibung ist keine Aussage über eine allgemeine oder
vollständige ALMEMO-Kompatibilität.

Die vollständige zweisprachige Erklärung steht in:

```text
LICENSES/ALMEMO-TRADEMARK-NOTICE.txt
```

Der Hinweis wird zusätzlich in gekürzter Form auf der am Gerät scrollbaren
Seite `Lizenzen / Marken` beziehungsweise `Licenses / Trademarks` und auf der
entsprechenden Web-Seite angezeigt.

## 9. Keine sonstige Markenfreigabe

Die Nennung weiterer Hersteller, Projekte und Autoren dient ausschließlich der
Herkunfts- oder Verwendungsangabe. Sie bedeutet keine Unterstützung,
Freigabe oder Zertifizierung des TP-3000 durch die genannten Rechteinhaber.
