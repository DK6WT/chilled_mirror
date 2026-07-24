# Source Validation – V0.50.1_51

## Geprüfter Stand

- Basis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_26.zip`
- Ausgabe: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_27.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_51`

## Statische Prüfungen

Bestanden:

- Build-ID und Builddatum in `TPsignedData.h` eindeutig,
- `/TP3000_BOOT.TXT` als einziger früher Diagnosepfad vorhanden,
- vollständiger CrashReport wird vor neuer Breadcrumb-Nutzung auf SD geschrieben,
- bei fehlgeschlagenem SD-Schreiben wird der gespeicherte CrashReport nicht durch eine serielle Ausgabe gelöscht,
- Watchdog-Resetstatus wird nach sicherem SD-Eintrag quittiert, damit der Diagnose-Wiederanlauf nur einmal erfolgt,
- Firmwarehash wird nur bei gespeichertem CrashReport oder erkanntem Watchdog-Reset übersprungen,
- Recovery-Status ist `TP_FW_INTEGRITY_ERROR`; zertifizierungsabhängige Funktionen bleiben damit gesperrt,
- Breadcrumb 1 enthält die Bootstufe, Breadcrumb 2 den letzten vollständig gelesenen Hash-Offset,
- Klammer-/Blockstruktur der geänderten C++-/INO-Dateien statisch geprüft,
- keine unbeabsichtigten Änderungen außerhalb der dokumentierten Quell- und Übergabedateien.

## Nicht ausgeführt

- vollständiger Teensyduino-Build,
- Flash-/RAM-Belegungsmessung,
- Hardwaretest mit realem Teensy 4.1 und SD-Karte,
- Auswertung eines real erzeugten `TP3000_BOOT.TXT`.

Diese Punkte müssen am Gerät noch bestätigt werden.
