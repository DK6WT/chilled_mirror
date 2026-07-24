# TP-3000 V0.50.1 – Releasevalidierung

## Durchgeführt

### Paketstruktur

- Build-Metadaten auf `0.50.1_87` / `24.07.2026` vereinheitlicht
- Firmwaremodule auf der Sketch-Hauptebene belassen
- historische Dokumente nach `docs/` verschoben
- keine verschachtelten ZIPs, Firmwarebinaries, Objektdateien oder privaten Schlüssel im öffentlichen Baum
- GSL1680-Herstellerfirmware entfernt und durch Beschaffungsanleitung ersetzt

### Statische Quellenprüfung

- alle Text-/Quelldateien lesbar und ohne NUL-Bytes
- lokale Includes und wesentliche Projektdateien vorhanden
- keine verbleibende veraltete Webanzeige `0.50.1_47`
- Release-Metadaten in `TPsignedData.h` konsistent
- keine externe Flash-/PSRAM-Initialisierung in diesem Release

### Bibliotheken und Lizenzen

- lokale Bibliotheksversionen aus `library.properties` erfasst
- zu jeder lokalen Bibliothek eine Lizenzdatei vorhanden
- Bosch-BMP5-Unterkomponente separat als BSD-3-Clause erfasst
- micro-ecc als BSD-2-Clause erfasst
- pako 1.0.11 einschließlich MIT-/zlib-Aufteilung dokumentiert
- Project Nayuki und QRCode for JavaScript als MIT-Komponenten dokumentiert
- Font- und Bildherkunftsnachweise erhalten
- Fontquell-Hashes, UTF-8-Zuordnungen und bytegleiche Regeneration von Droid Sans Mono und Font Awesome erfolgreich geprüft
- GSL1680-Weitergabebeschränkung im Audit technisch erzwungen

### Automatischer Audit

`tools/release_audit.py` prüft den öffentlichen Quellbaum auf:

- erwartete Version und Build-ID
- erforderliche Kern-, Dokumentations- und Lizenzdateien
- unerlaubte Herstellerfirmware
- private Schlüssel, Binaries, Archive und Messdaten
- lokale Bibliothekslizenzen
- saubere Hauptebene
- Textdateien mit NUL-Bytes

## Nicht durchgeführt

Mangels vollständiger Teensyduino-Toolchain und Zielgerät nicht durchgeführt:

- Arduino-/Teensyduino-Kompilier- und Linklauf
- reale Flash-, RAM1- und RAM2-Speicherstatistik
- Upload und Boot auf Teensy 4.1
- Watchdog-, CrashReport- und Firmwarehash-Endtest
- ADC1-/ADC2-, Referenz-, Pt100-, Optik- und Peltier-Hardwaretest
- DHCP-/Static-IP-/Ethernet-Aus-Ein-Test
- SD-Entnahme, Dateifehler und alle vier Log-Integritätsmodi
- vollständiger Zertifikats-/KeyGen-End-to-End-Workflow
- QR-Scan beider TFT- und Webteile mit dem aktuellen Viewer
- Drucktest des deutschen und englischen Kalibrierscheins
- LED-Autoadaption über reale Temperaturzyklen und mehrere Kopf-SNs

## Verbindliche Abnahme auf dem Entwicklungsrechner

1. Öffentlichen Releasebaum lokal um die originale GSL1680-Paneldatei ergänzen.
2. vollständigen Releasebuild für Teensy 4.1 erstellen und komplette Memory Usage archivieren.
3. Gerät sauber starten; Build-ID `_87`, Firmware-SHA-256 und Watchdogverhalten prüfen.
4. Messung und Regelung mit realer Hardware testen.
5. Ethernet, Web, SD, Schnittstellen und Logging testen.
6. Geräte-, Kopf-, System- und Firmwarezertifikate einschließlich historischer Firmwareabweichung prüfen.
7. beide TP3C1-QR-Teile auf TFT, Web und Ausdruck scannen.
8. deutsche und englische Kalibrierscheine drucken beziehungsweise als PDF erzeugen.
9. erst nach erfolgreicher Abnahme optional ein Hersteller-Firmwarezertifikat für den exakten Buildhash ausstellen.

Die historischen Prüfberichte unter `docs/history/v0.50.1/validation/` dokumentieren die Zwischenstände, ersetzen aber nicht diese Schlussabnahme.
