# Source Validation – TP-3000 V0.50.1_44

## Geprüfter Stand

- Basis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_19.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_20.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_44`

## Durchgeführte Host-/Quellprüfungen

- `tools/test_calibration_applicability_state.py`
  - kanonische Bildung des Datensatz-SHA-256
  - erster/ältester Ende-Datensatz bleibt maßgeblich
  - unvollständige letzte Zeile wird nach simuliertem Stromausfall ignoriert
  - veränderte vollständige Zeile wird fail-closed abgewiesen
  - Quellvertrag für SD-Pfad, Formatkennung, Ereigniscodes, Auswahlfilter, Transaktionssperre, Web-JSON und Statusbezeichnungen
  - zentrale Änderungsmeldung aus `tpMainConfigSave()` und `tpMetrologyConfigSave()`
- `tools/test_head_bound_certificate_selection.py`
  - kopfgebundene Auswahl, PDF-Fallback, Zeitraumüberlappung und deterministische Sortierung
- `tools/test_dynamic_page_transfer.py`
  - nicht blockierender Seitentransfer für `/identity`, `/validity` und `/calibration`
- JavaScript-Syntaxprüfung der eingebetteten Zertifikatsauswahlseite mit Node.js `--check`
- Klammer-/Delimiterprüfung der geänderten C++-/INO-Dateien
- Vergleich gegen frisch entpackte V0.50.1_19-Basis: nur die dokumentierten Quell-, Versions-, Test- und Dokumentationsdateien wurden verändert

Alle genannten Host-/Quellprüfungen waren erfolgreich.

## Nicht durchgeführt

- kein vollständiger Teensyduino-/Teensy-4.1-Build in dieser Umgebung
- keine realen FLASH-/RAM1-/RAM2-Zahlen
- kein Hardwaretest mit SD-Karte, Stromausfall und Kopfwechsel
- kein Browser-/Netzwerktest am realen Gerät

Diese Prüfungen sind vor Freigabe gemäß `BUILDING.md` nachzuholen.
