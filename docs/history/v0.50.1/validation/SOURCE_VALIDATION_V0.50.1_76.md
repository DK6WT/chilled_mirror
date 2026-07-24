# Source Validation V0.50.1_76

## Gegenstand

Validierter Quellstand: `Taupunktspiegel_StandV1_2026-07-19_V0.50.1_76.zip`
Direkte Basis: `V0.50.1_75`
Interne Version: `0.50.1`
Build-ID: `0.50.1_76`

## Durchgeführte statische Prüfungen

- Quellbaum umfasst 281 Dateien; ZIP-Struktur und vollständige Entpackbarkeit geprüft.
- Änderungen gegen V0.50.1_75 als Dateidifferenz kontrolliert.
- Ordner `libraries/` mit 48 Dateien binär gegen die Basis verglichen: unverändert.
- Relevante Warnstellen für Selbstkopie, Pfadkürzung, TFT-/Webformatierung und unbenutzte eigene Helfer kontrolliert.
- Textquelldateien auf versehentlich eingebettete NUL-Bytes geprüft.
- Build-ID und Dokumentation auf `0.50.1_76` abgeglichen.

## Abgrenzung

In dieser Umgebung steht keine vollständige Teensyduino-Toolchain für einen realen Teensy-4.1-Build und kein TP-3000-Prüfgerät zur Verfügung. Deshalb ersetzen diese Prüfungen weder den Arduino-IDE-Kompilierungslauf noch den Hardwaretest. Fremdbibliothekswarnungen wurden bewusst nicht verändert.

## Empfohlene Freigabeprüfung

1. Vollständiger Build mit derselben Teensyduino-/Boardkonfiguration wie beim Warnprotokoll.
2. Prüfen, dass nur noch bewusst akzeptierte Fremdbibliotheks- beziehungsweise Toolchainmeldungen verbleiben.
3. Starttest, Laden bestehender aktueller A/B-Konfiguration und Testmigration mit einem gesicherten alten V5–V8-EEPROM-Abbild.
4. SD-Test für externen Kalibrierschein und Firmwarezertifikatsuche.
5. TFT- und Webprüfung der Gültigkeits- und Importmeldungen.
