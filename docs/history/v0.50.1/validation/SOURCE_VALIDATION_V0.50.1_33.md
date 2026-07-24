# Source Validation – TP-3000 Build 0.50.1_33

- Verbindliche Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_8(1).zip` (`V0.50.1.8`).
- Neuer Übergabestand: `V0.50.1.9`; interne Firmwareversion weiterhin `0.50.1`, Build-ID `0.50.1_33`.
- Schwerpunkt: SD-basierter Upload externer PDF-Kalibrierscheine, transaktionale Aktivierung, Integritätsprüfung sowie kryptografische Bindung an die Systemkalibrierung.
- Kein externer SPI-/QSPI-Flash wird angesprochen oder vorausgesetzt.

## Geänderte beziehungsweise neue Programmdateien

- `TP-3000.ino`
- `TPethernet.ino`
- `TPexternalCalibration.cpp` (neu)
- `TPexternalCalibration.h` (neu)
- `TPsignedCalibration.cpp`
- `TPsignedCalibration.h`
- `TPsignedData.h`

ADC, Pt100-Messkette, Optikregelung, Peltierregelung und Safety wurden nicht fachlich geändert.

## Durchgeführte Prüfungen

- `TPexternalCalibration.cpp` mit Clang C++17 und Arduino-/SD-Stubs syntaktisch geprüft: bestanden.
- Dateisystemgestützter Host-Funktionstest mit der echten Projektimplementierung `TPsha256.cpp`: bestanden.
- Der Host-Test deckt ab:
  - Pflichtfeld- und Datumsvalidierung,
  - blockweisen Binärupload ohne vollständige PDF-Kopie im RAM,
  - unveränderte PDF-Bytes,
  - `%PDF-`-Prüfung,
  - SHA-256 und daraus abgeleitete Dokument-ID,
  - zweiten vollständigen Hashlauf nach Schließen und erneutem Öffnen,
  - Laden und Prüfen nach simuliertem Neustart,
  - exakte Systemkalibrierungsbindung,
  - identischen erneuten Upload,
  - Ablehnung abweichender Metadaten bei identischem PDF,
  - Ablehnung einer Nicht-PDF-Datei,
  - Erkennung nachträglicher PDF-Manipulation.
- Das vollständige eingebettete JavaScript des Kalibrierscheins mit Node.js syntaktisch geprüft: bestanden.
- Das vollständige eingebettete JavaScript der Seite `/calibration` mit Node.js syntaktisch geprüft: bestanden.
- C++-Quelldateien mit einem raw-string-fähigen Klammer-/Stringscanner geprüft: keine offenen Literale oder unausgeglichenen Trennzeichen.
- Formatstring-Prüfung der geänderten HTML-Templates:
  - Gültigkeitsseite: 43 Formatangaben / 43 Argumente,
  - Kalibrierverwaltung: 37 / 37,
  - Systemkalibrierungsanfrage: 28 / 28.
- Konfliktmarkerprüfung auf `<<<<<<<` und `>>>>>>>`: keine Treffer.
- SD-PDF-Grenze ist auf 6 MiB (`6.291.456` Byte) festgelegt; Browser-Chunkgröße ist 16.000 Byte und bleibt unter der vorhandenen Request-Body-Grenze.
- V1-Systemkalibrierungsanfragen und -pakete bleiben lesbar; neue Pakete verwenden V2 und Approval V4.
- Die fünf bestehenden Signaturen und ihre Reihenfolge bleiben erhalten. Bei eigener TP-3000-Kalibrierscheinquelle bleiben die sechs Fachfelder in TP3C1 unverändert; bei externer Quelle werden sie leer gebunden und durch die PDF-Referenz ersetzt.

## Nicht in dieser Umgebung ausgeführt

Ein vollständiger Teensyduino-Releasebuild, Linktest gegen alle realen Hardwarebibliotheken sowie ein End-to-End-Test auf dem TP-3000 mit Browser, SD-Karte und echtem KeyGen waren in dieser Umgebung nicht möglich. Diese Prüfungen müssen auf dem vorgesehenen Teensyduino-/Hardware-Buildsystem erfolgen.
