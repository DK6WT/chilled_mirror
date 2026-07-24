# Source Validation – TP-3000 V0.50.1 / Build 0.50.1_35

## Prüfgegenstand

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-15_V0.50.1_10.zip`
- neuer Paketstand: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_11.zip`
- Schwerpunkt: SHA-256- und zertifizierter SD-Logmodus
- Format: `TP3000-LOG-SIGNATURE-2`

## Durchgeführte Prüfungen

### 1. Host-Kompilierung des neuen Logkerns

`TPcertifiedLog.cpp`, `TPsignedData.cpp` und `TPsha256.cpp` wurden mit einem
Dateisystem-/Arduino-Stub unter C++17 und folgenden Warnstufen kompiliert:

```text
-Wall -Wextra -Wpedantic
```

Ergebnis: bestanden.

### 2. Normaler zertifizierter Abschluss

Geprüft wurden:

- nummerierter CSV-Pfad,
- Erzeugung der gleichnamigen `.TPSIG`,
- gültiges JSON,
- Formatkennung `TP3000-LOG-SIGNATURE-2`,
- Signaturangaben `ECDSA-P256-SHA256` und `IEEE-P1363`,
- Übereinstimmung von `LogFileSize` mit der realen CSV,
- bytegenaue Übereinstimmung von `LogFileSha256`,
- eingebettete Systemkalibrierung,
- Übereinstimmung des SHA-256 der roh eingebetteten `Context`-Bytes mit
  `ContextSha256`,
- `RecoveredAfterUncleanShutdown=false`.

Ergebnis: bestanden.

### 3. Einfacher SHA-256-Modus

Geprüft wurde die exakte GNU-kompatible Zeile:

```text
<lowercase SHA-256> *260716_001.CSV
```

Der Hash stimmt bytegenau mit der erzeugten CSV überein.

Ergebnis: bestanden.

### 4. Wiederherstellung nach unsauberem Neustart

Testdatei vor simuliertem Neustart:

```text
H\n
1\n
2\n
PARTIAL
```

Das Journal bestätigte nur `H\n1\n`. Bei der Wiederherstellung wurde die
vollständige zusätzliche Zeile `2\n` erhalten und der unvollständige Rest
`PARTIAL` entfernt. Anschließend wurde eine gültige `.TPSIG` mit
`RecoveredAfterUncleanShutdown=true` erzeugt.

Ergebnis: bestanden.

### 5. Manipulation des eingefrorenen Kontextes

Nach Anlegen eines offenen Journalzustands wurde die `.CTX` absichtlich um ein
Byte verändert. Die Wiederherstellung und Signaturerzeugung wurden erwartungsgemäß
abgewiesen.

Ergebnis: bestanden.

### 6. Rotation

Geprüft wurden Rotationserkennung bei:

- Änderung eines gebundenen PID-Wertes,
- Rückkehr zum ursprünglichen Wert,
- Kalendertagswechsel.

Ergebnis: bestanden.

### 7. Web-JavaScript

Die eingebettete `/download`-Seite wurde aus dem C++-Raw-String extrahiert.
Geprüft wurden:

- vorhandener Filter `Nachweise`,
- aktualisierter Snapshot-Hinweis,
- JavaScript-Syntax mit `node --check`.

Ergebnis: bestanden.

### 8. Versions- und Zeitbasisprüfung

Statisch bestätigt:

- Build-ID `0.50.1_35`,
- Formatkennung `TP3000-LOG-SIGNATURE-2`,
- kryptografische Zeitstempel verwenden `tpCurrentUtcUnixTime()` und nicht die
  lokale RTC-Zeit direkt.

Ergebnis: bestanden.

## Nicht durchführbar in dieser Umgebung

Folgende Prüfungen benötigen die reale Arduino-/Teensyduino-Toolchain oder
Hardware und wurden nicht als bestanden behauptet:

- vollständiger Teensy-4.1-Kompilier- und Linklauf,
- exakte FLASH-/RAM1-/RAM2-Ausgabe,
- echte SD-Karten-Latenz und FAT-Verhalten,
- realer Stromausfall während Schreib-/Rename-/Flush-Phasen,
- ECDSA-Signatur mit dem echten geräteinternen Schlüssel,
- Offline-Verifikation durch den TP-3000 Viewer,
- Langlauf mit Regelung, ADC, Safety und gleichzeitigem Webdownload.

Diese Punkte sind nach dem ersten Gerätebuild gemäß `BUILDING.md` zu prüfen.
