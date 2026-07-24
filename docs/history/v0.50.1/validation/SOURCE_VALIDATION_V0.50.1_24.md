# Source Validation 0.50.1_24

Durchgeführte statische Prüfungen:

- Build-ID in Firmware und Web-Infoseite auf `0.50.1_24`.
- V3-Formatkennungen und V1-/V2-Abwärtsprüfung kontrolliert.
- Kanonische Reihenfolge der neuen Kalibrierfelder mit TP3000KeyTool V0.7.4 verglichen.
- JavaScript des Kalibrierscheins mit Node.js syntaktisch geprüft.
- `TPC1`-Kalibriermesswertdecoder mit As-Found-/As-Left-Testdaten geprüft.
- SHA-256-Dokumentidentität zwischen JavaScript und Referenzberechnung abgeglichen.
- `TP3Q2`-Kompression/Base45-Hülle testweise erzeugt und mit ZLIB/JSON wieder dekodiert.
- Ein- und zweiteilige QR-Darstellung sowie Drucksperre bis zum vollständigen Aufbau statisch geprüft.

Nicht durchgeführt: Teensyduino-Kompilierung und Hardwaretest, weil die erforderliche Toolchain in der Erstellungsumgebung nicht installiert ist. Diese Prüfung ist vor dem Einsatz verbindlich nachzuholen.

Zusätzlich ausgeführt: QR-Kapazität mit 20 As-Found-/As-Left-Messpunkten und großer Testbelegung. Der kombinierte Testcode wurde erfolgreich aufgebaut. Ergebnis: ohne Befund.

- Datumswechsel-Fix: kein aktuelles Tagesdatum als Ersatz für ein fehlendes Referenzdatum.
- Geräte-Aktivvergleich basiert nur auf wirksamen Justierungswerten.
