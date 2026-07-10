# TP-3000 – Chilled-Mirror Dew-Point Hygrometer

**Open-source firmware for an experimental high-resolution chilled-mirror
dew-point hygrometer based on Teensy 4.1, ADS1263, a Bosch BMP585 pressure
sensor and a 5-inch RA8875/GSL1680 touch display.**

TP-3000 ist die Firmware eines experimentellen Taupunktspiegel-Hygrometers.
Sie erfasst die Spiegel- und Umgebungstemperatur ratiometrisch, erkennt die
Betauung optisch und regelt die Spiegeltemperatur bidirektional über ein
Peltier-Element. Anzeige, Kalibrierung, Logging, Diagnose, Safety und optionale
Schnittstellen sind in einem eigenständigen Embedded-System zusammengeführt.

> **Entwicklungsstatus:** funktionsfähiger Engineering-Prototyp in aktiver
> Hardwareerprobung. Diese Veröffentlichung enthält keine Zusicherung einer
> Kalibrierung, Baumusterprüfung oder Sicherheitszertifizierung.

**Version:** `0.50.0`  
**Stand:** `11.07.2026`  
**Pakettyp:** öffentliches Quellpaket / GitHub-Quellstand

## Hinweis zum alternativen Hauptbildschirm

In diesem Quellstand sind bereits erste Arbeiten an einem optionalen
alternativen Hauptbildschirm enthalten. Dieser soll im zentralen
Messwertbereich drei Zeilen statt der bisherigen zwei darstellen:
relative Feuchte, Taupunkt und Umgebungstemperatur.

Die grundlegende Funktion und erste Layout-Ansätze sind bereits angelegt. Die
optische Gestaltung, Abstände und endgültige Anordnung entsprechen jedoch noch
nicht dem vorgesehenen Endstand. Der bisherige Hauptbildschirm bleibt erhalten;
der alternative Hauptbildschirm ist derzeit als Entwicklungsstand zu betrachten.

## Version 0.50.0

Mit diesem Stand wird das gesamte Projekt auf die dreiteilige Versionsnummer
`MAJOR.MINOR.PATCH` umgestellt. Die früheren internen Vxx-Arbeitsnummern werden
für neue Versionen nicht mehr verwendet.

Der TP-3000-Projektcode wird unter **GNU GPL Version 3 only**
(`GPL-3.0-only`) verteilt. Fremdkomponenten behalten ihre jeweils dokumentierten
Lizenzen.

Die Setup-Seite **Lizenzen / Marken** beziehungsweise **Licenses / Trademarks**
ist mit UP und DOWN in Schritten von jeweils zwei Zeilen scrollbar. Sie zeigt
neben Projektlizenz, Herkunft und Gewährleistungsausschluss auch das Recht zur
Weitergabe und Änderung unter GPL-3.0-only, die öffentliche Quellcodeadresse
`github.com/DK6WT/chilled_mirror` sowie den Hinweis zur optionalen
ALMEMO-Anbindung und zur Marke.

### Status der GSL1680-Panel-Firmware

Die panelspezifische EastRising-/BuyDisplay-Firmware
`external/GSL1680/gslX680_311_5_F.h` ist in diesem öffentlichen Quellpaket
**nicht enthalten**. Ihre eigenständigen Weitergaberechte sind nicht
abschließend dokumentiert; sie ist kein Bestandteil der TP-3000-GPL-
Lizenzierung.

Ein Nutzer bezieht die zum eigenen Display gehörende Originaldatei direkt vom
Displayhersteller und legt sie lokal unter dem genannten Pfad ab. `.gitignore`
schließt die Datei von Commits aus. Die auf der vorhandenen Hardware getestete
Referenzdatei hatte folgende SHA-256-Prüfsumme:

```text
ea756ce8d96631337fa09abe22c4e95aaad86cb706b561a52dfd143e751edd96
```

Der Wrapper `GSL1680Firmware.h` lässt die Herstellerdatei unverändert und ordnet
das dort verwendete 8051-Schlüsselwort `code` nur während des Includes dem
Makro `PROGMEM` zu. Ohne lokal installierte Panel-Firmware stoppt der Build mit
einer eindeutigen Fehlermeldung. Ein kompiliertes Abbild würde die
Herstellerdaten enthalten und wird deshalb nicht mit diesem öffentlichen
Quellpaket verteilt.

Die lokalen Build-Bibliotheken bleiben im Ordner `libraries/` erhalten. Die
RA8875-Displaybibliothek wird aus dem installierten Teensyduino-Boardpaket
verwendet und nicht als doppelte Projektkopie verteilt. Das dokumentierte
AI-generierte Earth-Startbild zeigt die **Western Hemisphere**.

## Projektursprung und Entwicklungslinie

Die Bedien-, Display- und Menügrundstruktur des TP-3000 wurde aus dem Projekt
**LJ2000M T_2.06c GSL1680** weiterentwickelt. Das ursprüngliche Projekt war ein
menügeführtes RF-Leistungs- und SWR-Messgerät.

1. **2014–2016 – LJ2000M:** Entwicklung durch Loftur E. Jonasson,
   TF3LJ / VE2LJX.
2. **2017–2021 – Display- und Touch-Anpassung:** Weiterentwicklung durch
   J.G. Holstein, unter anderem für RA8875 und GSL1680.
3. **2025–2026 – TP-3000:** grundlegender Umbau zur Taupunktspiegel-Firmware
   durch S. Brachtl, DK6WT.

Der TP-3000-Umbau umfasst unter anderem den ADS1263-Messpfad, Pt100- und
Referenzkalibrierung, Taupunkt-/Feuchteberechnung, optische Spiegelregelung,
Peltierregelung, Safety, Logging, Ethernet sowie die deutsch/englische
Benutzeroberfläche. Die ursprünglichen Urheber- und Änderungshinweise bleiben
in den abgeleiteten Dateien erhalten.

## Hauptfunktionen

- Teensy 4.1 mit 600 MHz als zentrale Echtzeitplattform
- ADS1263 ADC1 für die ratiometrische Erfassung von Spiegel-Pt100,
  Umgebung-Pt100 und zwei Referenzwiderständen
- aktive Stromumkehr an beiden Pt100- und Referenzmessungen zur Unterdrückung
  parasitärer Thermospannungen
- Callendar-Van-Dusen-Auswertung mit einstellbarem R0 und optionaler
  Zweipunktkorrektur je Pt100-Kanal
- ADS1263 ADC2, Transimpedanzverstärker, Dunkelwertkorrektur und geregelte
  IR-LED für die optische Betauungsdetektion
- bidirektionale PID-Regelung des Peltierstroms über DRV8873 mit
  MCP3202-Stromüberwachung und Lüftersteuerung
- Sättigungsdampfdruck über Wasser und Eis nach Hardy/Wexler auf ITS-90-Basis
  sowie Druckkorrektur über den WMO-Enhancement-Faktor in `double`-Präzision
- getrennte schnelle Pfade für Regelung und Safety, eigene Ring- und
  Mittelwertpuffer sowie konfigurierbare Ausgabefilter für SD, USB, RS232 und
  Web
- RV-3129-Echtzeituhr und BMP585-Luftdrucksensor über den separaten
  `Wire2`-Bus; der BMP585 wird über die SparkFun-BMP581/Bosch-BMP5-API
  angesprochen
- RA8875-TFT mit kapazitivem GSL1680-Touchscreen und deutsch/englischer
  Menüführung
- SD-Logging, USB, RS232 und Ethernet mit integrierter Weboberfläche
- optionale serielle ALMEMO-Anbindung und Messwertausgabe im ALMEMO-V6-Format
  für WinControl
- A/B-gesicherte EEPROM-Einstellungen, Diagnosefunktionen, Alarme, Watchdog
  und mehrstufige Safety-Abschaltungen

## Zielhardware

- Teensy 4.1
- ADS1263
- RA8875-basiertes 800 × 480 TFT
- GSL1680 kapazitiver Touchcontroller
- DRV8873 H-Brücke
- MCP3202
- AD5683R
- RV-3129 RTC
- BMP585 — angesprochen über die SparkFun-BMP581/Bosch-BMP5-API

Die konkrete Pinbelegung und die projektspezifischen Hardwareannahmen stehen in
`TP-3000.ino` und den jeweiligen Treiberdateien.


## Nicht enthaltene Hardwareunterlagen

Dieses Repository veröffentlicht ausschließlich die TP-3000-Firmware und die
für ihren Build erforderlichen Softwaredateien. Schaltpläne,
Leiterplattenlayouts, Gerber- und Fertigungsdaten, vollständige Stücklisten,
Bestückungsunterlagen, mechanische Konstruktionen, STEP-/STL-Dateien,
Gehäusedaten sowie interne Produktions- und Abgleichunterlagen werden nicht
mitgeliefert.

Diese Hardware- und Fertigungsunterlagen sind **nicht Bestandteil der
GPL-3.0-only-Veröffentlichung**, bleiben proprietär und werden durch die GNU GPL
nicht lizenziert. Die Firmware kann unabhängig davon vom Anwender normal über
USB auf den Teensy 4.1 geladen und aktualisiert werden; es gibt keine
Signaturprüfung oder technische Sperre für selbst kompilierte Firmware.

## Build-Umgebung

Die aktuelle Arbeitsumgebung verwendet:

- Arduino IDE **2.3.10**
- Teensyduino / Teensy-Boardpaket **1.62.0**
- Teensy 4.1 mit **600 MHz**
- Optimierung **Faster**
- USB-Typ **Serial**
- Keyboard Layout **US English**

PJRC führt die Produktbezeichnung als **Teensyduino 1.62**; der Arduino-
Boards-Manager installiert sie als Paketversion **1.62.0**. Öffne
`TP-3000.ino` aus diesem Verzeichnis. Die vollständige Installations- und
USB-Update-Anleitung steht in [`BUILDING.md`](BUILDING.md).

### GSL1680-Firmware für den lokalen Build

Das öffentliche Repository enthält die Panel-Firmware nicht. Die passende
Originaldatei muss direkt vom Displayhersteller bezogen und lokal hier abgelegt
werden:

```text
external/GSL1680/gslX680_311_5_F.h
```

Die Datei bleibt bytegenau unverändert. Das 8051-Schlüsselwort `code` wird
während des Includes über den Wrapper `GSL1680Firmware.h` auf `PROGMEM`
abgebildet; die im Original auskommentierten Seiten `0xE0` bis `0xE6` bleiben
auskommentiert. Weitere Details stehen in `external/GSL1680/README.md`.

### Mitgelieferte lokale Bibliotheken

Die folgenden Bibliotheken werden laut vollständiger Arduino-Buildausgabe direkt
aus dem projektlokalen Ordner `libraries/` eingebunden:

- `libraries/ProtoCentral_ADS1262_32-bit_precision_ADC_Library` — Version 2.0.0, MIT
- `libraries/RTC_RV3129_Arduino_Library` — Version 1.0.0, MIT
- `libraries/SparkFun_BMP581_Arduino_Library` — Version 1.0.1, MIT
- `libraries/WDT_T4` — Version 0.1, MIT

Die Bibliotheksordner enthalten die jeweils erforderlichen Quellen, Metadaten,
README- und Lizenzdateien. Details stehen in `libraries/README.md` und
`THIRD_PARTY_NOTICES.md`.

### Hardware-Watchdog

Die MIT-lizenzierte Bibliothek `WDT_T4 / Watchdog_t4` von Antonio Brewer
(`tonton81`) ist unter `libraries/WDT_T4/` mit ihrer ursprünglichen `LICENSE`
enthalten. TP-3000 bindet bevorzugt die projektlokale Datei ein:

```cpp
#include "libraries/WDT_T4/Watchdog_t4.h"
```

Fehlt die projektlokale Kopie, wird ersatzweise nach einer global installierten
`Watchdog_t4.h` gesucht. Fehlt auch diese, bleibt der Sketch kompilierbar, der
Hardware-Watchdog ist dann jedoch deaktiviert.

### Externe RA8875-Displaybibliothek

TP-3000 verwendet **RA8875 0.7.11** von Max MC Costa / sumotoy aus der
Teensyduino-Installation, nicht aus dem TP-3000-Projektordner. Bei der aktuellen
Boards-Manager-Installation liegt der erwartete Pfad unter:

```text
Arduino15/packages/teensy/hardware/avr/1.62.0/libraries/RA8875
```

Lizenz: **GPL-3.0-or-later**. Die Urheber- und Lizenzangaben stehen in
`LICENSES/RA8875-NOTICE.txt`; der GPL-v3-Text liegt in
`LICENSES/GPL-3.0.txt`. Die Bibliothek selbst wird in diesem Paket nicht erneut
verteilt.

### Zusätzlich aus Teensyduino / Arduino erforderlich

Der Sketch verwendet außerdem SPI, SD, SdFat, NativeEthernet, FNET, Wire,
Metro, EEPROM, Time und den Teensy-Core aus der installierten
Teensyduino-/Arduino-Umgebung. Diese Komponenten werden im TP-3000-Ordner nicht
dupliziert und behalten ihre eigenen Urheber- und Lizenzbedingungen.

Die exakten Einzelversionsnummern und die aktuelle Flash-/RAM-Ausgabe sollen vor
einem öffentlichen Release aus einer ausführlichen 0.50.0-Kompilierung mit
Arduino IDE 2.3.10 und Teensyduino 1.62.0 archiviert werden. Diese reine
Für diese Paket- und Fontaktualisierung wurde kein neuer vollständiger Teensy-Build erzeugt.

## Quellstruktur

- `TP-3000.ino`: Hauptprogramm, Initialisierung und Scheduler
- `ads1263.ino`: Pt100-, Referenz- und Optikmessung
- `Taupunkt_Regelung.ino`: optische Regelung und Peltier-Sollwert
- `TPdisplay.ino`: Haupt-, Diagnose- und ADC-Info-Anzeige
- `TPmenu*.ino`: Setup- und Kalibrierungsmenüs
- `TPsdLog.ino`, `TPserial.ino`, `TPusbSerial.ino`, `TPethernet.ino`: Datenausgabe
- `TPsafety.ino`, `TPalarm.ino`: Schutz- und Alarmfunktionen
- `TPtft.*`, `TPtouchscreen.ino`: RA8875-/Touch-Bedienebene
- `GSL1680.*`, `GSL1680Firmware.h`: Touchcontroller-Treiber und lokale Firmware-Einbindung
- `external/GSL1680/README.md`: lokaler Bezug und Ausschluss der Panel-Firmware aus dem öffentlichen Paket
- `fonts_droidsansmono_data.c`: unabhängig erzeugtes Droid Sans Mono (Apache-2.0)
- `fonts_fontawesome_f080_data.c`: unabhängig erzeugter Font Awesome `AwesomeF080_40` mit drei Icons (OFL-1.1)
- `fonts.h`: gemeinsame GPL-3.0-only-Symboldeklarationen
- `earth.c`: AI-generiertes Startbild in RGB565
- `assets/earth_blue_marble_ai_800x480.png`: dokumentierte 800-x-480-Quelle für `earth.c`

## Entfernte, nicht benötigte Bestandteile

Für den aktiven Build nicht benötigte Ordner und Dateien wurden entfernt,
darunter:

- Chrono und LightChrono
- JPEGDecoder
- das vollständige ILI9341-Fontarchiv
- GSL1680-Beispielsketch
- das unbenutzte alternative `gslfw.h`
- Archiv-, Patch- und IDE-Hilfsdateien
- Beispiele, Hardwaredaten und CI-Dateien der mitgelieferten Bibliotheken
- die doppelte, vom geprüften Build nicht verwendete Projektkopie von RA8875
- unbenutzte ADS1263-Test- und Altpfade
- Arial- und Arial-Bold-Fontdaten aus dem früheren gemeinsamen Fontbestand
- den unbenutzten Font-Awesome-Bereich `AwesomeF000` aus dem früheren gemeinsamen Fontbestand


## Font-Herkunft, Lizenzen und Verifikation

Die Fontdaten sind nach Lizenz und Erzeugungsweg getrennt:

- `fonts_droidsansmono_data.c` enthält ausschließlich unabhängig erzeugte
  Droid-Sans-Mono-Daten unter Apache-2.0.
- `fonts_fontawesome_f080_data.c` enthält ausschließlich den unabhängig
  erzeugten Font-Awesome-Iconfont `AwesomeF080_40` unter OFL-1.1.
- `fonts.h` enthält nur die gemeinsamen Symboldeklarationen unter
  GPL-3.0-only.

**Droid Sans Mono** wird durch
`tools/font_provenance/generate_droidsansmono.py` direkt aus der geprüften
AOSP-Datei `third_party/font_sources/DroidSansMono-AOSP.ttf` erzeugt. Der
Generator prüft den SHA-256 der TTF, rendert alle 18 vorhandenen Größen bei
100 dpi monochrom und schreibt ein `ILI9341_t3_font_t`-kompatibles
Packed-Format. Er liest weder das historische `ILI9341_fonts`-Archiv noch
dessen C-Tabellen.

Der erste Zeichenbereich bleibt ASCII `0x20` bis `0x7E`. Der zweite Bereich
wurde auf `0xB0` bis `0xBE` erweitert:

```text
0xB0 °    0xB1 Ω    0xB2 ä    0xB3 ö    0xB4 ü
0xB5 kleines %      0xB6 Ä    0xB7 Ö    0xB8 Ü
0xB9 ß    0xBA µ    0xBB ±    0xBC Δ    0xBD ≤    0xBE ≥
```

Die bisherigen Bytewerte `0xB0` bis `0xB5` bleiben unverändert. Das kleine
Prozentzeichen wird nicht aus einer alten Tabelle kopiert, sondern in jedem
Generatorlauf aus dem frisch gerasterten normalen Prozentzeichen abgeleitet,
auf 75 % verkleinert, in der Monospace-Zelle zentriert und an derselben
Grundlinie ausgerichtet. `TextBox::print()` übersetzt die entsprechenden
UTF-8-Zeichen einschließlich `Ä`, `Ö`, `Ü`, `ß`, `µ`/`μ`, `±`, `Δ`, `≤` und
`≥` in die lokalen Fontbytes. Bestehende Texte und Layouts werden dadurch
nicht automatisch geändert; die Zeichen stehen für neue oder überarbeitete
Texte zur Verfügung.

**Font Awesome 4.5.0** wird durch
`tools/font_provenance/generate_fontawesome_f080_40.py` direkt aus
`third_party/font_sources/FontAwesome-4.5.0.ttf` erzeugt. TP-3000 benötigt nur
`AwesomeF080_40` und nur drei Zeichen:

```text
Byte 0x10 -> U+F090  sign-in / ENTER
Byte 0x2A -> U+F0AA  chevron-circle-up
Byte 0x2B -> U+F0AB  chevron-circle-down
```

Beide Generatoren lesen ausschließlich die eindeutig lizenzierten
Originalfonts und erzeugen Daten, Indizes und Deskriptoren selbst. Das frühere
`ILI9341_fonts`-Archiv ist damit für keine verteilten Fontdaten mehr Quelle oder
Build-Abhängigkeit; es kann höchstens als historische optische Referenz dienen.

Reproduzierbarkeit:

```text
python -m pip install -r tools/font_provenance/requirements.txt
python tools/font_provenance/generate_droidsansmono.py --check
python tools/font_provenance/generate_fontawesome_f080_40.py --check
python tools/font_provenance/verify_font_provenance.py
```

Optional können PBM-Vorschauen des erweiterten Droid-Zeichenbereichs erzeugt
werden:

```text
python tools/font_provenance/generate_droidsansmono.py --check \
  --preview-dir font-preview
```

Der eingecheckte Stand wurde mit FreeType 2.13.2 erzeugt. Andere
FreeType-/Hinting-Versionen können einzelne Randpixel anders rastern. Deshalb
sollten die häufig verwendeten Größen 12, 14, 16, 18, 20, 24, 28 und 60 sowie
die drei Touch-Icons einmal auf dem realen TFT geprüft werden. Die Lizenz- und
Herkunftskette bleibt davon unberührt.

Weitere Einzelheiten stehen in:

- `third_party/font_sources/README.md`
- `tools/font_provenance/README.md`
- `LICENSES/ILI9341-FONTS-NOTICE.txt`
- `LICENSES/ILI9341-CONVERTER-NOTICE.txt`
- `LICENSES/Droid-Sans-Mono-NOTICE.txt`
- `LICENSES/Font-Awesome-NOTICE.txt`

## Lizenz

Der TP-3000-Projektcode und die aus LJ2000M abgeleiteten Projektdateien stehen
unter **GNU GPL Version 3 only** (`GPL-3.0-only`). Der vollständige
GPL-v3-Lizenztext befindet sich in `LICENSE`. Fremdkomponenten behalten ihre
jeweiligen Lizenzen; kompatible `-or-later`-Komponenten werden für diese
Distribution unter Version 3 verwendet.

Mitgeführte Fremdkomponenten behalten ihre eigenen Lizenzen. Eine genaue
Zuordnung steht in `THIRD_PARTY_NOTICES.md`; die benötigten Lizenztexte liegen
unter `LICENSES/` beziehungsweise in den jeweiligen Bibliotheksordnern.

Bei Weitergabe des Quell- oder Binärpakets müssen insbesondere erhalten bleiben:

1. Urheber- und Änderungshinweise in den Dateien,
2. `LICENSE`, `THIRD_PARTY_NOTICES.md` und der Ordner `LICENSES/`,
3. der korrespondierende Quellcode für GPL-pflichtige Bestandteile,
4. Hinweise auf vorgenommene Änderungen.

## Optionale ALMEMO-/WinControl-Anbindung und Markenhinweis

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
Dies ist keine Aussage über eine allgemeine oder vollständige
ALMEMO-Kompatibilität.

Es wird kein ALMEMO-, WinControl-, AHLBORN- oder akrobit-Logo verwendet. Die
vollständige zweisprachige Erklärung steht in
`LICENSES/ALMEMO-TRADEMARK-NOTICE.txt` und wird in gekürzter Form auch auf der
scrollbaren Geräte-Seite **Lizenzen / Marken** angezeigt.

## Earth-Startbild und externe GSL1680-Firmware

Das frühere, nicht eindeutig identifizierbare `earth.c`-Bild wurde vollständig
ersetzt. Das aktuelle Startbild ist als AI-generierte Darstellung gekennzeichnet
und wurde mit OpenAI image generation erstellt. Als visuelles Quellenmaterial
diente die von NASA Earth Observatory veröffentlichte Darstellung **The Blue
Marble (2002): True-color global imagery at 1km resolution, Western
Hemisphere**.

Das Ergebnis ist kein originales NASA-Bild und wurde von NASA weder geprüft noch
genehmigt oder unterstützt. Die Kennzeichnung `GPL-3.0-only` gilt für die
TP-3000-spezifische Quelldarstellung, Bearbeitung, Konvertierung und Integration
sowie für etwaige schutzfähige eigene Bildbestandteile; das NASA-Ausgangsmaterial
wird dadurch nicht neu lizenziert. Vollständige Quellen-, Credit-, AI- und
Nutzungshinweise stehen in `THIRD_PARTY_NOTICES.md` und
`LICENSES/EARTH-IMAGE-NOTICE.txt`.

Aktuelle Prüfsummen:

```text
assets/earth_blue_marble_ai_800x480.png
bf5044d5b65629b0413efc9fe09e8b62ac408bc5b674b9cb37626331b44e73b6

earth.c
4f7b90e2b18c192c9ae3b47ce960d0ed223b34d26c0bdd01a4609a4acd60e32f
```

Die eigenständige Weitergabeberechtigung der panelspezifischen
GSL1680-Firmwaretabelle ist nicht abschließend geklärt. Sie ist deshalb aus
diesem öffentlichen Quellpaket ausgeschlossen. TP-3000 erteilt daran keine
Lizenz und behauptet insbesondere nicht, dass sie unter GPL steht. Bezug und
lokale Installation sind in `external/GSL1680/README.md` beschrieben.

## Danksagung

Ein besonderer Dank gilt **Jim Morris (`wolfmanjm`)** und
**Skallwar/ESTBLC**. Beide reagierten offen und hilfreich auf Fragen zur
Herkunft und Lizenzierung des GSL1680-Treibercodes. Skallwar stellte im Juli
2026 klar, dass sein Projekt auf `wolfmanjm/GSL1680` basiert und daher von
Anfang an unter GPLv3 hätte stehen müssen; die öffentliche Repository-Lizenz
wurde entsprechend korrigiert. Dadurch ist die Treiber-Rechtekette nun
konsistent als GPLv3 dokumentiert.

## Gewährleistung

Die Firmware wird ohne Gewährleistung bereitgestellt. Sie steuert Heiz-/Kühl-
und Leistungshardware; Inbetriebnahme und Betrieb erfordern unabhängige
Hardware-Schutzmaßnahmen und eine eigene Sicherheitsprüfung.
