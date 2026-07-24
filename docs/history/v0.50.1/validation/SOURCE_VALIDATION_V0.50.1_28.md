# Source Validation – TP-3000 Build 0.50.1_28

Statisch geprüft:

- Firmware-Build-ID ist konsistent `0.50.1_28`.
- Der QR-Präfix lautet bytegenau `tp3000://verify#` ohne Leerzeichen oder Zeilenumbruch.
- Der bestehende `TP3Q3:`-Payloadgenerator wurde nicht verändert.
- QR-Grafiken kodieren Präfix und Roh-Payload als zwei direkt aufeinanderfolgende Segmente; der Base45-Payload bleibt im Alphanumerikmodus.
- Ein- und zweiteilige QR-Codes nutzen denselben Deep-Link-Präfix.
- `.tpqr`-Downloads bleiben unveränderte Roh-Payloads ohne Präfix.
- QR-Autoversion und Kapazitätsfehler werden weiterhin durch die lokale QR-Bibliothek mit Fehlerkorrekturstufe L ermittelt; bei Überlauf des Einzelcodes greift die vorhandene Zweiteilung.
- Eingebettetes JavaScript wurde mit Node.js syntaktisch geprüft.

Nicht durchgeführt:

- vollständiger Teensyduino-Build
- Anzeige- und Kameratest auf TP-3000-Hardware
- Android-Intent-Test mit TP-3000 Verifier V0.3.3
