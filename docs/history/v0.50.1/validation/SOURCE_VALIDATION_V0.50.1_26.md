# Source Validation 0.50.1_26

- Firmwareidentität: `0.50.1`, Build-ID `0.50.1_26`.
- `TPsignedCalibration.cpp` mit Clang 17 und den vorhandenen Arduino-/SD-/TimeLib-Stubs syntaxgeprüft: ohne Fehler.
- `TPethernet.ino` mit Clang 17 und den vorhandenen Stubs syntaxgeprüft: ohne Fehler.
- Eingebetteter JavaScript-Teil des Kalibrierscheins extrahiert und mit Node.js `--check` geprüft: ohne Fehler.
- TPC1-Testdaten für Scope 4 / Stage 3, Scope 5 / Stage 1+2 und Alt-Scope 3 im Webdecoder erfolgreich dekodiert.
- Ein vollständiger Teensyduino-Build ist in dieser Umgebung nicht erfolgt.
