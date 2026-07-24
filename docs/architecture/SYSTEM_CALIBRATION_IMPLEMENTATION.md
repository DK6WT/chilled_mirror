# TP-3000 Systemkalibrierung – Implementierung 0.50.1_26

## Fachliches Modell

- `Gerätejustierung`: signierte Referenz- und Kanalkorrekturwerte des Grundgeräts.
- `Kopfjustierung`: signierte Kopf-, Pt100- und Regelparameter des angeschlossenen Sensorkopfs.
- `Systemkalibrierung`: gemeinsamer Kalibriernachweis für genau eine Kombination aus Geräte-SN, Kopftyp und Kopf-SN.

## Anfrage `.tpscalreq`

Die Anfrage wird vom Geräteschlüssel signiert und ist 72 Stunden gültig. Sie enthält zusätzlich zu Geräteidentität und Zeitstempel:

- Kopftyp und Kopf-SN
- Calibration-ID der aktiven Gerätejustierung
- SHA-256-Manifest der aktiven Gerätejustierung
- Calibration-ID der aktiven Kopfjustierung
- SHA-256-Manifest der aktiven Kopfjustierung

Eine Anfrage kann nur erzeugt werden, wenn Gerätezertifikat, Gerätejustierung und Kopfjustierung gültig aktiv sind.

## Signierte `.tpscal`

Das KeyGen signiert den gemeinsamen Kalibriernachweis als Hersteller-Root oder autorisiertes Kalibrierlabor. Die echte Kalibrierung verwendet eindeutig `AS_FOUND`, `AS_LEFT`, `AS_FOUND_AS_LEFT_NO_ADJUSTMENT` oder `BEFORE_AFTER_ADJUSTMENT`. Das frühere `AS_FOUND_AS_LEFT` bleibt als Altformat lesbar.

## Web-Upload

`POST /calibration-system-upload` nimmt höchstens 16.383 Byte entgegen. Die Datei wird zunächst im RAM vollständig gelesen, anschließend kryptografisch geprüft und nur bei passender Geräte-/Kopf- und Justierungsbindung aktiviert.

Aktive Datei: `/CALIBRATION/ACTIVE_SYSTEM.tpscal`

Der Austausch verwendet `.tmp` und `.bak`; bei fehlgeschlagenem Rename wird die bisherige aktive Datei wiederhergestellt.

## Web-Anzeige

`/validity` zeigt den separaten Abschnitt `Systemkalibrierung`. `/calibration` bietet Export der `.tpscalreq`, direkte Dateiauswahl und Upload der `.tpscal`.

## Bewusste Abgrenzung dieses Schritts

- genau eine aktive Systemkalibrierung
- noch keine Liste mehrerer Zertifikate
- noch keine Auswahl einer älteren Geräte-/Kopfkombination
- der bestehende Kalibrierschein-/QR-Ausdruck wird in einem separaten Folgeschritt vollständig auf die neue Systemkalibrierung umgestellt

## Nachweisstatus für den späteren zertifizierten Logmodus

Die Bereitschaftsprüfung verlangt nun zusätzlich die aktive, an beide Justierungen gebundene Systemkalibrierung. Der `.TPSIG`-Ausgabepfad bleibt weiterhin bewusst gesperrt, bis er separat implementiert und geprüft ist.
