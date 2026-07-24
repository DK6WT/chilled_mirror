# TP-3000 V0.50.1 bauen

## Zielumgebung

- Board: **Teensy 4.1**
- vorgesehene Referenzumgebung: Arduino IDE 2.3.x mit Teensyduino 1.62.x
- Hauptsketch: `TP-3000.ino`
- Release-Metadaten: `TPsignedData.h`

Andere Toolchainstände können funktionieren, müssen aber wegen Änderungen an Core, NativeEthernet/FNET, SD/SdFat, RA8875 und Linkerskript separat geprüft werden.

## 1. Quellbaum vorbereiten

Das Repository als zusammenhängenden Sketchordner auschecken oder entpacken. Die Dateien `TP-3000.ino` und alle weiteren Firmwaremodule müssen gemeinsam auf der obersten Sketch-Ebene bleiben. Die buildbezogenen Dokumente liegen dagegen unter `docs/` und werden nicht kompiliert.

## 2. Lokale Bibliotheken

Der Ordner `libraries/` enthält die für diesen Stand mitgeführten Versionen:

- ProtoCentral ADS1262 Library 2.0.0
- RTC RV3129 Library 1.0.0
- SparkFun BMP581 Library 1.0.1 einschließlich Bosch BMP5 API
- TP3000 micro-ecc 1.0.0-tp3000.1
- WDT_T4 0.1

Arduino IDE durchsucht einen beliebigen `libraries/`-Unterordner neben dem Sketch nicht automatisch für Includes in spitzen Klammern. Vor dem Build deshalb diese vier Ordner unverändert in den Bibliotheksordner des verwendeten Sketchbooks kopieren:

```text
<Sketchbook>\libraries\ProtoCentral_ADS1262_32-bit_precision_ADC_Library
<Sketchbook>\libraries\RTC_RV3129_Arduino_Library
<Sketchbook>\libraries\SparkFun_BMP581_Arduino_Library
<Sketchbook>\libraries\TP3000_micro_ecc
```

Den Sketchbook-Pfad zeigt Arduino IDE unter **Datei → Voreinstellungen → Sketchbook-Speicherort** an. Nach dem Kopieren die IDE neu starten. Diese manuell installierten Bibliotheken haben Vorrang vor gleichnamigen anderen Installationen.

`WDT_T4` wird vom Hauptsketch direkt über `libraries/WDT_T4/Watchdog_t4.h` eingebunden und muss nicht separat installiert werden. `libraries/README.md` beschreibt Versionen und Herkunft.

Weitere Abhängigkeiten kommen aus Arduino/Teensyduino, insbesondere Arduino Core, EEPROM, Entropy, Metro, NativeEthernet/FNET, RA8875, SD/SdFat, SPI, TimeLib und Wire.

## 3. GSL1680-Panel-Firmware lokal ergänzen

Die panelspezifische Herstellerdatei wird nicht öffentlich verteilt. Für das getestete Displaymodul wird lokal benötigt:

```text
external/GSL1680/gslX680_311_5_F.h
```

Die Datei muss aus dem offiziellen EastRising-/BuyDisplay-Paket für das tatsächlich verwendete Panel stammen und unverändert kopiert werden. Die getestete Referenzfassung ist in `external/GSL1680/README.md` mit Dateikopf und SHA-256 dokumentiert.

Nicht verändern:

- Arrayname oder Datentyp
- das 8051-Schlüsselwort `code`
- die auskommentierten Seiten `0xE0` bis `0xE6`
- Dateiname und Pfad

Der Wrapper `GSL1680Firmware.h` ordnet `code` nur während des Includes `PROGMEM` zu. Die Herstellerdatei selbst bleibt unverändert.

## 4. Kompilieren

1. `TP-3000.ino` in der Arduino IDE öffnen.
2. Board **Teensy 4.1** auswählen.
3. die auf dem Zielgerät verwendeten USB-, CPU- und Optimierungsoptionen einstellen.
4. vollständigen Verify-/Compile-Lauf ausführen.
5. die komplette Teensy-Memory-Usage archivieren.

Der Release wurde in der Bereinigungsumgebung statisch geprüft, aber dort stand keine vollständige Arduino-/Teensyduino-Toolchain zur Verfügung. Deshalb enthält das Paket bewusst keine Behauptung über einen erfolgreichen finalen Linklauf und keine vorgebauten Firmwareabbilder.

## 5. Vor dem Upload prüfen

- angezeigte Version: `0.50.1`
- Build-ID: `0.50.1_87`
- Datum: `24.07.2026`
- keine unerwarteten Warnungen aus dem eigenen Projektcode
- RAM1/RAM2/Flash-Werte gegen den letzten bekannten Hardwarebuild vergleichen
- Hersteller-Firmwarezertifikat nach jedem neuen Binärbuild neu erzeugen, sofern diese Funktion verwendet wird

## 6. Hardware-Abnahme

Die verbindliche Abnahmeliste steht in `docs/release/VALIDATION_V0.50.1.md`. Besonders kritisch sind:

- Watchdog und Boot-Hash
- Pt100-/Referenzpfad und SFOCAL
- Optik-Auto-Cal und Peltier-Safety
- DHCP, statische IP und Webserver
- SD-Logging in allen vier Integritätsmodi
- TFT-QR Code 1/2 und 2/2
- Geräte-, Kopf-, System- und Firmwarezertifikate
- Kalibrierscheindruck und externe PDF-Bindung
- LED-Autoadaption mit und ohne SD

## 7. Öffentliche Artefakte

Nicht öffentlich verteilen:

- `external/GSL1680/gslX680_311_5_F.h`
- daraus erzeugte HEX-/BIN-Abbilder
- private Root-, Labor- oder Geräteschlüssel
- reale Provisionierungs- und Zertifikatsdateien, sofern sie vertrauliche Produktionsdaten enthalten

Der öffentliche Quellbaum wird mit folgendem Audit geprüft:

```text
python tools/release_audit.py .
```
