# Source Validation 0.50.1_27

- Firmwareidentität: `0.50.1`, Build-ID `0.50.1_27`.
- Direkte Upload-Routen für `.tpdcal`, `.tphcal` und `.tpscal` syntaktisch geprüft.
- Geräte- und Kopfjustierungen werden vor dem transaktionalen Austausch vollständig kryptografisch geprüft und mit den wirksamen Justierungswerten verglichen.
- Der Kalibrierschein lädt Gerätejustierung, Kopfjustierung und Systemkalibrierung, zeigt die System-Messpunkte und verwendet die `SCAL-...`-ID als Dokument-ID.
- TP3Q3-Rundlauf mit ZLIB, Base45, D/H/S-Dokumenthash sowie Ein-/Zwei-Teil-Transport geprüft.
- Eingebettetes Browser-JavaScript mit Node.js syntaktisch geprüft.
- `TPsignedCalibration.cpp` und `TPethernet.ino` mit Host-Stubs und `g++ -fsyntax-only` geprüft.
- A4-Druckansicht in Chromium/Playwright mit Systemkalibriertabelle, zweispaltigen Justierungsbereichen und zentriertem Inhalt gerendert; Ergebnis: 2 Seiten im Testdatensatz.
- Vollständiger Teensyduino-Build ist auf dem vorgesehenen Windows-/Teensyduino-System auszuführen.
