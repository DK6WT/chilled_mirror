# Source Validation – TP-3000 Build 0.50.1_32

- Basis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_9.zip`, Build `0.50.1_31`.
- Neuer Build: `0.50.1_32`, interne Version weiterhin `0.50.1`.
- Geänderte Programmdateien: `TPethernet.ino` und `TPsignedData.h`.
- Alle ADC-, Regelungs-, Safety-, Kalibrierparser-, Zertifikats- und Identitätsdateien sind gegenüber der Basis unverändert.

## Prüfungen

- Vollständiges eingebettetes Kalibrierschein-JavaScript mit Node.js syntaktisch geprüft: bestanden.
- Layoutfunktion der Fachfelder getestet: vier zweispaltige Felder und zwei Vollbreitenfelder: bestanden.
- Datenquelle der Fachfelder: ausschließlich `system.Approval`: geprüft.
- TP3C1-Encoderblock SHA-256 vor/nach Änderung identisch:
  `883a2836459ad108497638e15aabf2e17179f5752b75577dc439e7bf356a6031`.
- Referenz-SHA-256 für Envelope, ZLIB und beide Transportteile unverändert und korrekt.
- A4-Drucktest mit vollständigen Überschriften, Beschriftungen, Prüftext und zwei 115-mm-QR-Codes: genau eine Seite.
- QR-Symbole bleiben Version 20, EC M und 97 × 97 Module; nur die physische Druckbreite wurde geändert.
- ZIP-Integrität und erneutes Entpacken werden beim Erstellen des Übergabepakets geprüft.

Ein vollständiger Teensyduino-Build und ein Hardware-/Samsung-/Android-End-to-End-Test sind in dieser Umgebung nicht möglich.
