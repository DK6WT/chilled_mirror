# Source Validation V0.50.1_77

## Gegenstand

Validierter Quellstand: `Taupunktspiegel_StandV1_2026-07-19_V0.50.1_77.zip`
Direkte Basis: `V0.50.1_76`
Interne Version: `0.50.1`
Build-ID: `0.50.1_77`

## Durchgeführte statische Prüfungen

- Quellbaum umfasst 283 Dateien; gegenüber V0.50.1_76 wurden zwei Dokumentationsdateien ergänzt und sieben vorhandene Dateien geändert.
- Änderungen gegen V0.50.1_76 als SHA-256-Dateidifferenz kontrolliert; keine Datei entfernt.
- Ordner `libraries/` mit 48 Dateien binär gegen die Basis verglichen: vollständig bytegleich.
- Build-ID und Dokumentation auf `0.50.1_77` abgeglichen.
- Textquelldateien auf eingebettete NUL-Bytes und versehentliche Escape-Sequenzen geprüft.
- `TPledAdaptation.cpp` mit einem Arduino-/SD-Hoststub unter G++17 erfolgreich syntaktisch geprüft.
- Ausführbarer Host-Verhaltenstest erfolgreich: kein eigener Medienzugriff bei aktivem Logging, sofortiger Fallback bei Loggerstatus `Keine Karte`, Prüfung exakt nach 300000 ms ohne Logging, vorgezogene Auto-Cal-Prüfung mit neuem Raster sowie fünf Minuten Wartezeit nach dem Stoppen des Loggings.
- LED-SD-Logik zusätzlich statisch auf Loggerstatus, Fünf-Minuten-Raster, Auto-Cal-Synchronisationspunkt und unveränderten Fallback geprüft.

## Abgrenzung

In dieser Umgebung steht keine vollständige Teensyduino-Toolchain für einen realen Teensy-4.1-Build und kein TP-3000-Prüfgerät zur Verfügung. Die statischen Prüfungen ersetzen daher weder den Arduino-IDE-Kompilierungslauf noch den Hardwaretest.

## Empfohlene Freigabeprüfung

1. Vollständiger Teensyduino-Build mit Memory-Usage-Protokoll.
2. Logging aktiv: SD entfernen und Fallback nach Loggerfehler prüfen; keine parallele LED-Medienabfrage.
3. Logging aus: SD-Entnahme ohne Auto-Cal und Erkennung spätestens nach fünf Minuten prüfen.
4. Auto-Cal vor Ablauf des Rasters starten und sofortige Prüfung sowie Neustart des Fünf-Minuten-Rasters kontrollieren.
5. Auto-Cal-Intervalle 10 und 15 min, Stromrampe und Erhalt des letzten gültigen Ankers prüfen.
