# Source Validation V0.50.1_43

## Basis und Ziel

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_18.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_19.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_43`

## Geänderte Quelldateien

- `TPethernet.ino`
  - dynamischer nicht blockierender Seitentransfer für `/identity`, `/validity` und `/calibration`
  - dynamische Seiten ohne ETag und mit `no-store`
  - eigener Diagnosecode `ETH_REQ_VALIDITY_PAGE`
- `TPsignedData.h`
  - Build-ID auf `0.50.1_43`
- `CHANGELOG.md`
- `BUILDING.md`

## Durchgeführte Hostprüfungen

- C/C++-Lexikprüfung auf ausgeglichene Klammern unter Berücksichtigung von Kommentaren, Zeichenketten und Raw-Strings: bestanden.
- JavaScript-Syntaxprüfung aller aus Raw-HTML-Seiten extrahierten `<script>`-Blöcke mit `node --check`: 3 von 3 bestanden.
- Source-Contract-Prüfung:
  - alle drei dynamischen Seiten verwenden `ethBeginGeneratedPageTransfer()`;
  - Routen übergeben den originalen `EthernetClient`, Requestheader und `keepOpen`;
  - dynamische Seiten können nicht über ETag mit 304 beantwortet werden;
  - statische Seiten behalten ETag-Unterstützung.
- Änderungsumfang gegen die frisch entpackte Arbeitsbasis geprüft: funktional geändert wurden nur `TPethernet.ino` und die Build-ID in `TPsignedData.h`; kryptografische Encoder und Datenformate sind bytegleich.
- ZIP-Inhaltsprüfung, vollständige Testentpackung und Wiederholung der Source-Contract- sowie JavaScript-Prüfungen aus dem fertigen Paket: bestanden.

## Noch ausstehend

- vollständiger Teensyduino-Build für Teensy 4.1
- reale Erstaufruftests von `/identity`, `/validity` und `/calibration`
- FLASH-/RAM1-/RAM2-Messwerte
- Hardware-Regressionsprüfung von Messung, Regelung und Safety während einer großen Seitenausgabe
