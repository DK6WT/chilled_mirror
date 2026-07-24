# Source Validation – V0.50.1_53

## Basis und Ausgabe

- frisch entpackte Basis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_28.zip`
- Ausgabe: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_29.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_53`

## Statische Prüfungen

- Build-ID nur im zentralen Firmwareheader geändert.
- Neuer Pfad `/calibration-system-import` ist in Pfaddefinition, Request-Klassifizierung, Diagnosetext und Dispatch vorhanden.
- Headerdeklaration und Implementierung von `tpSignedCalibrationImportSystemFromSd()` stimmen überein.
- System-SD-Import akzeptiert nur kryptografisch gültige und aktuell gebundene `.tpscal`-Pakete.
- HTML-Template enthält 47 Formatkonvertierungen; der zugehörige `snprintf` übergibt exakt 47 Nutzargumente.
- Gerendertes HTML besitzt eindeutige Element-IDs, fünf Datei-Eingaben und fünf zugehörige Aktionsknöpfe.
- Eingebettetes JavaScript wurde mit `node --check` syntaktisch geprüft.
- Testdarstellung bei 1400 px Breite wurde vollständig gerendert; Geräte- und Kopfaktionen sind in den jeweiligen Karten ausgerichtet.
- Alte Sammelbereiche für Anfrageexport, Direktupload und alternativen SD-Import sind aus dem aktiven Template entfernt.

## Nicht durchgeführt

- vollständiger Teensyduino-Build
- Upload auf Teensy 4.1
- realer SD-Import und Browserdurchlauf auf Hardware
