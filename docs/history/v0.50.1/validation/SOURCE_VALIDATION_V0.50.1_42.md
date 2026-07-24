# Source Validation V0.50.1_42

## Prüfgegenstand

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_17.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_18.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_42`

## Geänderte Quellbereiche

- `TPethernet.ino`
  - Neustart des 120-ms-Antwortbudgets nach vollständigem Geräte-/Kopf-/Systemimport
  - gleicher Schutz beim SD-Import und beim Abschluss externer PDF-Uploads
  - Infoanzeige auf Build `0.50.1_42` aktualisiert
- `TPsignedData.h`
  - Build-ID `0.50.1_42`

## Statische Prüfungen

Bestanden:

1. Jeder lange Import speichert zuerst das Ergebnis, ruft danach `client.restartBudget()` auf und schreibt erst anschließend die HTTP-Antwort.
2. Erfolgs- und Fehlerpfad verwenden dasselbe neu gestartete Antwortbudget.
3. Keine Änderung an `TPsignedCalibration.cpp`, `TPexternalCalibration.cpp`, kanonischen Bytes, Signaturprüfung, Archivierung oder Aktivierungslogik.
4. Build-ID in `TPsignedData.h` und Web-Info ist konsistent.
5. Paket nach Erstellung erneut entpackt und die geänderten Stellen sowie die ZIP-Integrität geprüft.

## Noch ausstehend

- vollständiger Teensyduino-Build
- Hardwaretest: neue `.tphcal` über `/calibration` hochladen und konkrete Erfolgs-/Fehlermeldung empfangen
- Kontrolle, ob der vor dem Update mit `Failed to fetch` quittierte Import bereits aktiviert wurde
