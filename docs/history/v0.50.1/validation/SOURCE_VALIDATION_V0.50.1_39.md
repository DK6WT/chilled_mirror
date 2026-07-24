# Source Validation – TP-3000 V0.50.1 / Build 0.50.1_39

## Basis und Ziel

- Basis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_14.zip`
- Ziel: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_15.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_39`

## Reproduziertes Fehlerbild

Die historische Ansicht verwendet SD-Dateizugriffe und ECDSA-/SHA-256-Prüfungen, bevor die JSON-Antwort geschrieben wird. Der bisherige `EthBoundedWriter` maß sein 120-ms-Budget ab Requestbeginn. War die Vorprüfung länger, brach bereits das erste Antwortschreiben ab. Der Browser erhielt daher keinen HTTP-Fehlertext, sondern `Failed to fetch`.

## Geprüfte Korrekturen

- `EthBoundedWriter::restartBudget()` vorhanden.
- Budgetneustart nach historischer Systemkalibrierungsprüfung.
- Budgetneustart nach historischer Geräte-/Kopfjustierungsprüfung.
- Budgetneustart nach Zeitraumssuche.
- Budgetneustart nach historischem externem PDF-Metadatenabruf.
- Budgetneustart vor historischem externem PDF-Download.
- Historische Geräte- und Kopfjustierung werden im Browser sequenziell geladen.
- Aktuelle Geräte-/Kopfjustierung darf weiterhin parallel aus dem aktiven Speicher geladen werden.

## Hostseitige Prüfungen

- JavaScript der Zertifikatsauswahl mit `node --check`: bestanden.
- JavaScript der vollständigen Zertifikatsansicht mit `node --check`: bestanden.
- Diff gegen V0.50.1_38: Änderungen nur in `TPethernet.ino`, Build-ID und Dokumentation.
- `TPsignedCalibration.cpp`, `TPexternalCalibration.cpp`, `TPcertifiedLog.cpp`, TP3C1-/TP3Q3-Encoder und Signaturformate: bytegenau unverändert.
- Fertiges ZIP mit `unzip -t`: bestanden.
- Frisch entpacktes ZIP gegen Arbeitsbaum verglichen: bestanden.

## Noch ausstehend

- vollständiger Teensyduino-Build,
- FLASH-/RAM1-/RAM2-Linkerbericht,
- Hardwaretest mit realer SD-Karte und historischem Zertifikat,
- Browserprüfung des historischen externen PDF-Knopfs.
