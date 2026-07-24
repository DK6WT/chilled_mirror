# Source Validation V0.50.1_19

Datum: 2026-07-12

Durchgeführt:

- lexikalische C/C++-Prüfung der geänderten Firmwaredateien einschließlich Raw-String-Webseiten
- JavaScript-Syntaxprüfung des eingebetteten QR-Encoders und der Kalibrierscheinseite mit Node.js
- Browserkompression/Base45 → Python-ZLIB/JSON-Rundlauf erfolgreich
- eigenständiger C++-Test: Top-Level-`ManifestSha256` wird auch dann gewählt, wenn ein eingebettetes Laborzertifikat vorher ein gleichnamiges Feld enthält
- typische kombinierte Geräte-/Kopfkalibrierdaten auf QR-Kapazität geprüft

Nicht durchgeführt:

- Arduino-/Teensyduino-Kompilierung
- Flash-/RAM-Linkerbericht
- Hardware-, Touch-, Netzwerk-, Drucker- oder Watchdogtest

Diese Punkte müssen vor Freigabe nach `BUILDING.md` nachgeholt werden.
