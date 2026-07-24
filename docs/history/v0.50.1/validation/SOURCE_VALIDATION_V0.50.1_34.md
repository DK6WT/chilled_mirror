# Source Validation – TP-3000 Build 0.50.1_34

## Stand

- Basis: `Taupunktspiegel_StandV1_2026-07-15_V0.50.1_9.zip` / Build `0.50.1_33`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-15_V0.50.1_10.zip`
- interne Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_34`

## Geprüfte Änderungen

- alle Funktionen von `TPexternalCalibration.cpp` in `FLASHMEM`
- externe PDF-Pfade, Feldnamen, Meldungen und Formate in `PROGMEM`
- reine externe PDF-Zustände und Arbeitspuffer in `DMAMEM`/RAM2
- aktiver Systemkalibrierungsstatus und Systemkalibrierungswerte in RAM2
- große externe PDF-Webpuffer nicht mehr auf dem RAM1-Stack
- explizite Initialisierung persistent verwendeter DMAMEM-Zustände

## Ausgeführte Prüfungen

1. `TPexternalCalibration.cpp` mit Clang als C++17 syntaktisch geprüft.
2. Host-Dateisystemtest des vollständigen PDF-Ablaufs bestanden:
   - gültiger blockweiser PDF-Upload
   - Größenprüfung
   - PDF-Kennung
   - zweifache SHA-256-Prüfung
   - Archiv- und Metadatendateien
   - aktiver Zeiger
   - Neustartprüfung
   - Ablehnung veränderter oder widersprüchlicher Dateien
3. Vergleichender Objektbau mit simulierten `.flashmem`, `.progmem` und `.dmabuffers` durchgeführt; die erwarteten Abschnittsverschiebungen sind vorhanden.
4. Klammer-, String- und Kommentarstruktur der drei geänderten Quelldateien lexikalisch geprüft.
5. Formatparameter von externem JSON- und HTML-Template geprüft: sieben beziehungsweise fünf Konvertierungen stimmen mit den Aufrufen überein.
6. JavaScript-Syntax der Kalibrierverwaltungs- und Kalibrierscheinlogik mit Node.js geprüft. Die Browserlogik wurde durch die RAM-Optimierung nicht verändert.
7. Konfliktmarker und unbeabsichtigte Binär-/Buildausgaben im Quellverzeichnis geprüft.

## Nicht in dieser Umgebung möglich

Ein vollständiger Teensyduino-1.62-Build für Teensy 4.1 konnte mangels installierter Teensy-Toolchain nicht ausgeführt werden. Die gemeldeten Zielwerte für RAM1 und RAM2 sind deshalb belastbare Abschnittsabschätzungen, aber keine behaupteten Linker-Endwerte. Vor Gerätefreigabe muss das ZIP auf dem vorgesehenen Buildrechner kompiliert und die neue Speicherstatistik dokumentiert werden.
