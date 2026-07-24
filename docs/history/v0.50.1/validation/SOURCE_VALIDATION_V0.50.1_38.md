# Source Validation – TP-3000 V0.50.1_38

## Prüfbasis

- Ausgangspaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_13.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_14.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_38`

## Geprüfte Änderungen

- vorgeschaltete Kalibrierzertifikat-Auswahlseite,
- aktuelles Zertifikat mit eigenem **Anzeigen**-Knopf,
- Zeitraumssuche mit inklusiver Überschneidungsregel,
- Trefferliste mit **Anzeigen** je Zertifikat,
- bedingter **PDF**-Knopf je exakt gebundenem externen Original-PDF,
- inhaltsadressiertes SD-Archiv für Gerätejustierung, Kopfjustierung und Systemkalibrierung,
- historische Zertifikatsansicht mit den damals gebundenen Justierungen,
- vollständige Prüfung archivierter PDF-Metadaten und PDF-Bytes beim Öffnen.

## Hostseitig ausgeführte Prüfungen

### JavaScript

Die beiden aus `TPethernet.ino` extrahierten Skripte wurden mit Node.js geprüft:

```text
node --check _check_index.js   → bestanden
node --check _check_viewer.js  → bestanden
```

### C++-Syntax

Mit lokalen Arduino-/SD-/TimeLib-Stubs und dem eingebetteten TP3000-micro-ecc-Include-Pfad:

```text
TPsignedCalibration.cpp  → g++ -std=c++17 -Wall -Wextra -Wpedantic -fsyntax-only: bestanden
TPexternalCalibration.cpp → g++ -std=c++17 -Wall -Wextra -Wpedantic -fsyntax-only: bestanden
```

### Funktionstest externes PDF-Archiv

Der Hosttest hat ein PDF hochgeladen und anschließend geprüft:

- archiviertes PDF vorhanden,
- exakte Dokument-ID auflösbar,
- archivierte Metadaten lesbar,
- SHA-256 identisch,
- Kalibrierschein-Nr. identisch,
- unbekannte Dokument-ID wird abgewiesen.

Ergebnis: **bestanden**.

### Statische Archiv-/Suchprüfung

Geprüft wurden:

- inklusive Überlappungsgrenzen,
- Nichttreffer unmittelbar vor und nach dem Suchzeitraum,
- alle neuen Webrouten,
- **Anzeigen** und bedingter **PDF**-Knopf,
- historische System-, Geräte- und Kopfbindung,
- manifestadressierte Archivpfade,
- vorhandene TP3C1-/TP3Q3-Formatkennungen.

Ergebnis: **bestanden**.

### Unveränderte QR-Encoder

Die eingebetteten JavaScript-Blöcke wurden gegen die Basis V0.50.1_37 bytegenau verglichen:

```text
ethCalibrationQrLibrary
SHA-256: 1cbc21fad9289a31906d447a2faba7d0cebfda780a89d29ba4c67f2c945abf6b
Ergebnis: identisch

ethTp3c1EncoderLibrary
SHA-256: 391093347eb28e68f47b64d80d0e1d6c087d481cda11ef62a2971b4575c5994c
Ergebnis: identisch
```

## Paketprüfung

Das erzeugte ZIP-Paket wurde zusätzlich geprüft:

- `unzip -t`: bestanden,
- frisches Entpacken in ein leeres Verzeichnis: bestanden,
- erneute JavaScript-Syntaxprüfung beider Seiten: bestanden,
- erneute statische Routen-/Suchprüfung: bestanden,
- SHA-256-Vergleich der geänderten Kerndateien zwischen Arbeitsbaum und frisch entpacktem Paket: identisch.

## Noch ausstehend

In dieser Umgebung konnten nicht ausgeführt werden:

- vollständiger Teensyduino-/Arduino-Linker-Build,
- endgültige FLASH-/RAM1-/RAM2-Ausgabe,
- Test mit realem Teensy 4.1 und echter SD-Karte,
- Browser-/PDF-Test auf dem realen Gerät.

Diese Punkte sind vor einer Produktionsfreigabe nachzuholen.
