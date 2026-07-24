# TP-3000 bauen und über USB installieren

Diese Anleitung beschreibt das öffentliche TP-3000-Quellpaket **0.50.0** mit
dem Firmwaredatum **11.07.2026** und die aktuell dokumentierte
Arduino-/Teensy-Konfiguration.

## 1. Entwicklungsumgebung

- Arduino IDE: **2.3.10**
- Teensyduino / Teensy-Boardpaket: **1.62.0**
- PJRC-Produktbezeichnung: **Teensyduino 1.62**
- Zielboard: **Teensy 4.1**
- CPU Speed: **600 MHz**
- Optimize: **Faster**
- USB Type: **Serial**
- Keyboard Layout: **US English**

Für die Installation des Teensy-Boardpakets wird in der Arduino IDE unter
`Datei -> Voreinstellungen -> Zusätzliche Boardverwalter-URLs` folgende URL
verwendet:

```text
https://www.pjrc.com/teensy/package_teensy_index.json
```

Der lokale Sketchbook-Pfad und der angezeigte USB-Port sind rechnerabhängig und
nicht Teil der festen Build-Konfiguration.

## 2. Projekt vorbereiten

1. Das vollständige TP-3000-Quellpaket in einen lokalen Ordner entpacken.
2. In der Arduino IDE die Datei `TP-3000.ino` öffnen.
3. Die Ordner `ProtoCentral_ADS1262_32-bit_precision_ADC_Library`,
   `RTC_RV3129_Arduino_Library` und `SparkFun_BMP581_Arduino_Library` aus dem
   mitgelieferten Verzeichnis `libraries/` unverändert nach
   `<Sketchbook>\libraries\` kopieren und Arduino IDE neu starten. Einen
   beliebigen `libraries/`-Unterordner neben dem Sketch durchsucht die IDE für
   Includes in spitzen Klammern nicht automatisch.
4. `WDT_T4` bleibt im Projektordner: Der Sketch bindet
   `libraries/WDT_T4/Watchdog_t4.h` direkt ein.
5. Die zum verwendeten Display gehörende GSL1680-Herstellerfirmware lokal unter
   `external/GSL1680/gslX680_311_5_F.h` installieren.

## 2.1 Optional: Fonts neu erzeugen oder prüfen

Für einen normalen Firmware-Build ist Python nicht erforderlich, weil die
erzeugten C-Dateien bereits im Quellpaket liegen.

Zur unabhängigen Neuerzeugung beziehungsweise Prüfung:

```text
python -m pip install -r tools/font_provenance/requirements.txt
python tools/font_provenance/generate_droidsansmono.py
python tools/font_provenance/generate_droidsansmono.py --check
python tools/font_provenance/generate_fontawesome_f080_40.py
python tools/font_provenance/generate_fontawesome_f080_40.py --check
python tools/font_provenance/verify_font_provenance.py
```

Der Droid-Generator liest ausschließlich
`third_party/font_sources/DroidSansMono-AOSP.ttf`, erzeugt alle 18 Größen und
baut den erweiterten Bereich `0xB0` bis `0xBE` einschließlich des aus dem
normalen Prozentzeichen abgeleiteten kleinen Prozentzeichens auf. Der
Font-Awesome-Generator liest ausschließlich die geprüfte
`FontAwesome-4.5.0.ttf` und erzeugt nur die drei benötigten Icons bei 40 pt /
100 dpi.

Der eingecheckte Stand wurde mit FreeType 2.13.2 erstellt. Eine andere
FreeType-Version kann einzelne Randpixel anders rastern. Nach einer
Neuerzeugung sollten die verwendeten Droid-Größen und die ENTER-/UP-/DOWN-
Icons auf dem realen TFT optisch geprüft werden.

## 3. GSL1680-Panel-Firmware

Die Panel-Firmware ist nicht Teil der TP-3000-GPL-Lizenzierung und ihre
Weitergaberechte sind nicht abschließend dokumentiert. Sie ist deshalb im
öffentlichen Repository und Quell-ZIP **nicht enthalten**. `.gitignore` schließt
den Pfad aus.

Die Originaldatei für das konkrete EastRising-/BuyDisplay-Panel muss direkt vom
Hersteller bezogen und unverändert hier abgelegt werden:

```text
external/GSL1680/gslX680_311_5_F.h
```

SHA-256 der auf der vorhandenen TP-3000-Hardware getesteten Referenzdatei:

```text
ea756ce8d96631337fa09abe22c4e95aaad86cb706b561a52dfd143e751edd96
```

Die Prüfsumme identifiziert nur die getestete Fassung. Für ein anderes Panel
kann eine andere offizielle Herstellerdatei erforderlich sein.

Die Datei darf nicht umbenannt, umformatiert oder inhaltlich bearbeitet werden.
Insbesondere:

- das 8051-Schlüsselwort `code` nicht entfernen,
- die im Original auskommentierten Seiten `0xE0` bis `0xE6` nicht aktivieren,
- keine konvertierte oder zuvor bearbeitete Ersatzdatei verwenden.

`GSL1680Firmware.h` ordnet `code` ausschließlich während des Includes dem
Makro `PROGMEM` zu. Die Herstellerdatei bleibt bytegenau unverändert, während
die Firmwaretabelle im Teensy-Flash abgelegt wird.

Ohne die lokale Datei stoppt der Build mit einer verständlichen Fehlermeldung.
Ein kompiliertes HEX/BIN-Abbild enthält die Herstellerdaten und wird deshalb
nicht als Bestandteil dieses öffentlichen Quellpakets mitgeliefert. Weitere
Angaben stehen in `external/GSL1680/README.md`.

## 4. Board-Einstellungen

In `Werkzeuge` folgende Werte einstellen:

```text
Board:            Teensy 4.1
CPU Speed:        600 MHz
Optimize:         Faster
USB Type:         Serial
Keyboard Layout:  US English
```

Als Port den aktuell erkannten Teensy auswählen. Die Portbezeichnung ist auf
jedem Computer unterschiedlich.

## 5. Kompilieren

In der Arduino IDE `Sketch -> Überprüfen/Kompilieren` ausführen.

Für dieses öffentliche Paket wurde ohne die absichtlich fehlende
Panel-Firmware kein vollständiger Teensy-Build erzeugt. Nach lokaler Installation
der Datei sollten mit aktivierter ausführlicher Ausgabe archiviert werden:

- vollständige Bibliotheksauswahl und Versionsliste,
- Compiler- und Linkerwarnungen,
- FLASH-, RAM1- und RAM2-Ausgabe,
- erzeugte HEX-/EHEX-Datei und deren Prüfsumme.

## 6. Über USB installieren oder aktualisieren

1. Teensy 4.1 über USB mit dem Computer verbinden.
2. In der Arduino IDE den erkannten Teensy-Port auswählen.
3. `Sketch -> Hochladen` ausführen.
4. Falls die IDE meldet, dass kein Teensy gefunden wurde, einmal die
   **PROGRAM MODE**-Taste auf dem Teensy drücken.
5. Nach erfolgreicher Übertragung startet der TP-3000 mit der neuen Firmware.

Der TP-3000 verwendet keine Signaturprüfung, keinen gesperrten Bootloader und
keinen geheimen Installationsschlüssel. Der Anwender kann eine selbst
kompilierte oder veränderte GPL-Firmware auf normalem Weg über USB installieren.

Für veränderte Firmware besteht keine Funktions-, Messgenauigkeits-, Safety-
oder Supportgarantie. Vor dem Einsatz müssen insbesondere Messpfad,
Peltierregelung, Alarmfunktionen und Safety-Abschaltungen geprüft werden.

## 7. Bibliotheken

Projektlokal enthalten:

- ProtoCentral ADS1262/ADS1263 Library **2.0.0** — MIT
- RTC RV3129 Arduino Library **1.0.0** — MIT
- SparkFun BMP581 Arduino Library **1.0.1** — MIT
- Bosch BMP5 Sensor API innerhalb der SparkFun-Bibliothek — BSD-3-Clause
- WDT_T4 / Watchdog_t4 **0.1** von Antonio Brewer — MIT

TP-3000 bevorzugt die mitgelieferte Datei
`libraries/WDT_T4/Watchdog_t4.h`. Ist sie nicht vorhanden, wird ersatzweise nach
einer global installierten `Watchdog_t4.h` gesucht; fehlt auch diese, bleibt der
Sketch kompilierbar, jedoch ohne Hardware-Watchdog.

Aus Teensyduino / Arduino verwendet, aber nicht im Projekt dupliziert:

- RA8875 **0.7.11** — GPL-3.0-or-later
- SPI, SD, SdFat, NativeEthernet, FNET, Wire, Metro, EEPROM, Time
- Teensy-Core und Toolchain

Der erwartete RA8875-Pfad der aktuellen Boards-Manager-Installation lautet:

```text
Arduino15/packages/teensy/hardware/avr/1.62.0/libraries/RA8875
```

Weitere Herkunfts- und Lizenzangaben stehen in `README.md`,
`THIRD_PARTY_NOTICES.md`, `libraries/README.md` und im Ordner `LICENSES/`.

## 8. Nicht enthaltene Hardwareunterlagen

Dieses Repository enthält ausschließlich die freigegebene Firmware und die für
ihren Build erforderlichen Softwaredateien.

Nicht mitgeliefert werden insbesondere Schaltpläne, Leiterplattenlayouts,
Gerber- und Fertigungsdaten, vollständige Stücklisten, Bestückungs- und
Produktionsunterlagen, mechanische Konstruktionen, STEP-/STL-/Gehäusedateien
sowie interne Prüf-, Abgleich- und Fertigungsunterlagen.

Diese Hardware- und Fertigungsunterlagen sind **nicht Bestandteil der
GPL-3.0-only-Veröffentlichung**, bleiben proprietär und werden durch die GNU GPL
nicht lizenziert.
