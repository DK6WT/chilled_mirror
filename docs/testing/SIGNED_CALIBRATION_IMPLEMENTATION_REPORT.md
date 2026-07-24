# V0.50.1_15 – Implementierungsbericht signierte Kalibrierung

## Basis

`Taupunktspiegel_StandV1_2026-07-12_V0.50.1_13_TEST_SIGNED_DATA_FOUNDATION.zip`

## Neue Dateien

- `TPsignedCalibration.h`
- `TPsignedCalibration.cpp`
- `SIGNED_CALIBRATION_WORKFLOW_TEST.md`

## Geänderte Firmwaredateien

- `TP-3000.ino`: Initialisierung nach SD-Start.
- `TP_T.h`: sichere Helfer für Gerätesignatur, Root-Prüfung und Zertifikatsexport.
- `TPidentity.ino`: Nutzung des internen Geräteschlüssels für Kalibrieranfragen; Root-Verifikation und Rekonstruktion des aktiven Gerätezertifikats.
- `TPethernet.ino`: Seite `/calibration`, Anfrage-Downloads und SD-Import-Endpunkte.
- `TPsignedData.h/.cpp`: Build-ID 0.50.1_14, echte Kalibriernachweise und getrennte Evidence-/Output-Bereitschaft.

## Sicherheitsentscheidungen

- Keine privaten Geräte- oder Root-Schlüssel werden exportiert.
- Der CALIBRATION-Rollenschlüssel ist root-zertifiziert und wird wiederverwendet.
- Jede Kalibrierung erhält eine neue Calibration-ID und neue signierte Datei.
- Abgelaufene Gültigkeit ist nicht blockierend.
- Manipulierte oder nicht zum Gerät/Kopf passende Dateien werden abgelehnt.
- Die vorher aktive gültige Datei bleibt bei einem fehlgeschlagenen Import erhalten.
- `Zertifiziert` bleibt gesperrt, bis der echte `.TPSIG`-Ausgabepfad aktiv ist.

## Vorprüfungen in der Erstellungsumgebung

- `TPsignedData.cpp`: clang++ Syntaxprüfung bestanden.
- `TPsignedCalibration.cpp`: clang++ Syntaxprüfung bestanden.
- Eingebettetes Setup- und Download-JavaScript: `node --check` bestanden.
- Keine privaten PEM-Schlüssel im Paket gefunden.

Ein vollständiger Teensyduino-Build und Hardwaretest bleiben erforderlich.


## V0.50.1_15 RAM1-Nacharbeit

Die Kalibrierfunktion blieb unverändert. Neue statische Texte und Formatvorlagen wurden in QSPI-Flash verschoben. Der exakte RAM1-Gewinn ist im lokalen Teensyduino-Linkerbericht zu bestätigen.
