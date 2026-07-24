# Source Validation – Firmware 0.50.1 / Build 0.50.1_46

## Grundlage

- frisch entpackte Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_21.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_22.zip`
- interne Firmwareversion: `0.50.1`
- neue Build-ID: `0.50.1_46`

## Geänderte Programmdateien

- `TPethernet.ino`
- `TPsignedData.h`

Zusätzlich wurden TP3C1-V0.3-Spezifikation, Implementierungsbericht und Hosttests ergänzt.

## Durchgeführte Prüfungen

- eingebetteten TP3C1-JavaScript-Encoder aus der Firmware extrahiert
- JavaScript-Syntax mit Node.js geprüft
- V3-Geräte-, V3-Kopf- und V2-Systempaket synthetisch aufgebaut
- Schema 1.1 und RequiredFeatures `0x3F` geprüft
- Approval-Felder 14/15 in Geräte-, Kopf- und Systemblock geprüft
- TPC1-Rohdaten und Scope-Konsistenz geprüft
- Transportkennungen `0x31` / `0x32`, Längen und CRC-Struktur geprüft
- V2-Gerätejustierung wird erwartungsgemäß abgelehnt
- inkonsistenter TPC1-Scope wird erwartungsgemäß abgelehnt
- ZLIB-Rundlauf und genau zwei Teile geprüft
- QR-Erzeugung mit derselben Browserbibliothek und Fehlerkorrektur M geprüft

## Größenprüfung

Normaler synthetischer Test:

- Envelope 1266 Byte
- ZLIB 375 Byte
- Binärteile 206 / 205 Byte
- QR 61 × 61 Module

Harter Maximaltest mit je 40 TPC1-Punkten in Geräte-, Kopf- und Systemkalibrierung sowie maximal langen Fachtexten:

- Envelope 6762 Byte
- ZLIB 3170 Byte
- Binärteile 1603 / 1603 Byte
- Deep Links 2465 / 2465 Zeichen
- QR 153 × 153 Module, Version 34, EC M
- ca. 0,714 mm je Modul bei 115 mm Außenmaß einschließlich Ruhezone

## Nicht durchgeführt

- vollständiger Teensyduino-Build
- Flashen auf das reale TP-3000
- Browser-/Drucktest mit den echten V3-Dateien des Gerätes
- End-to-End-Scan mit einer auf TP3C1 V0.3 angepassten Android-App
- kryptografische Prüfung der fünf Signaturen durch die Android-App

Der im aktiven Dateibestand verfügbare Viewer-Quellstand `TP3000.Viewer_0.35.zip` enthält noch keinen TP3C1-Decoder. Für die App-Anpassung wird der tatsächlich aktuelle TP3C1-App-Quellstand benötigt; ein veralteter Viewer wurde bewusst nicht als vermeintlich aktuelle App neu paketiert.
