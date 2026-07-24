# Source Validation – TP-3000 V0.50.1_45

## Zielstand

- Basis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_20.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_21.zip`
- Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_45`

## Geprüft

- Appendix-Marker und Feldvertrag sind im Quelltext vorhanden.
- Zentrale Datei `/CALIBRATION/APPLICABILITY.TPS` bleibt maßgeblich.
- Aktive und archivierte Systemdateien werden vor der Signaturprüfung am Appendix-Marker getrennt.
- Browser-Endpunkte erhalten weiterhin reines JSON ohne angehängten Textblock; der RAM2-Webpuffer enthält zusätzlich 512 Byte Reserve zum Einlesen des physischen Appendix.
- Appendix-Schreiben erfolgt über Temp-/Backup-Datei mit Rückleseprüfung.
- Web-Kalibrierschein lädt den Status über `/calibration-applicability.json`.
- `Gültig bis` wird nur bei vorhandenem Anwendbarkeitsende durchgestrichen.
- `Anwendbar bis`, Status und Grund stehen unmittelbar darunter.
- Druck-CSS setzt die zusätzlichen Statusangaben ausdrücklich auf Schwarz.
- Der Hosttest bildet zusätzlich das Anhängen, Trennen und die SHA-256-Prüfung eines transportablen Appendix nach; manipulierte Appendix-Daten werden abgewiesen.
- Bestehende Hosttests für Anwendbarkeit, dynamische Seitenausgabe und kopfgebundene Zertifikatauswahl laufen erfolgreich.
- Der zusammengesetzte JavaScript-Inhalt der Kalibrierscheinseite besteht `node --check`.

## Nicht ausgeführt

- vollständiger Teensyduino-Linkerbuild,
- realer SD-Schreib-/Stromausfalltest auf Teensy 4.1,
- Browser-Drucktest mit realem historischen Zertifikat,
- physischer Export-/Reimporttest einer `.tpscal` mit Anhang.
