# Source Validation – TP-3000 V0.50.1 / Build 0.50.1_36

## Prüfgegenstand

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_11.zip`
- neuer Paketstand: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_12.zip`
- bestehende Ausgabe: `.CSV` + `.TPSIG`
- zusätzliche Ausgabe: `.TPLOG`
- Containerformat: `TP3000-LOG-CONTAINER-1`

## Durchgeführte Prüfungen

### 1. Host-Kompilierung des Logkerns

`TPcertifiedLog.cpp`, `TPsignedData.cpp` und `TPsha256.cpp` wurden mit einem
Arduino-/SD-Dateisystemstub unter C++17 kompiliert:

```text
-Wall -Wextra -Wpedantic
```

Ergebnis: bestanden, keine Warnungen.

### 2. Normaler zertifizierter Abschluss

Nach einem kurzen Lauf wurden gleichzeitig erzeugt:

```text
260716_001.CSV
260716_001.TPSIG
260716_001.TPLOG
```

Geprüft wurden:

- TPLOG-Header-Magic `TP3LOG1\0`, Version 1 und Headergröße 116 Byte,
- TPLOG-Footer-Magic `TP3END1\0`, Version 1 und Footergröße 40 Byte,
- Little-Endian-Offets und lückenlose Abschnittsgrenzen,
- Header- und Footer-CRC-32,
- Gesamtlänge aus dem Footer,
- SHA-256 der eingebetteten CSV,
- SHA-256 der eingebetteten TPSIG,
- bytegenaue Identität zwischen separater und eingebetteter CSV,
- bytegenaue Identität zwischen separater und eingebetteter TPSIG,
- TPSIG-Format `TP3000-LOG-SIGNATURE-2`,
- Übereinstimmung von `LogFileSize` und `LogFileSha256` mit der CSV,
- Recovery-Flag im normalen Abschluss nicht gesetzt.

Ergebnis: bestanden.

### 3. SHA-256-Modus

Im einfachen SHA-256-Modus wurde weiterhin ausschließlich die
GNU-kompatible `<CSV>.sha256` erzeugt. Es entstanden weder `.TPSIG` noch
`.TPLOG`.

Ergebnis: bestanden.

### 4. Stromausfall-Recovery

Die simulierte offene Datei enthielt:

```text
H\n
1\n
2\n
PARTIAL
```

Nach Neustart blieb `H\n1\n2\n` erhalten; `PARTIAL` wurde entfernt. Danach
wurden CSV, TPSIG und TPLOG erzeugt. Geprüft wurden:

- `RecoveredAfterUncleanShutdown=true` in TPSIG,
- TPLOG-Headerflag Bit 0 gesetzt,
- eingebettete CSV bytegenau identisch mit der wiederhergestellten CSV.

Ergebnis: bestanden.

### 5. Rotation

Die bestehende Rotationserkennung bei Änderung der gebundenen PID-Konfiguration
und bei Kalendertagswechsel wurde erneut ausgeführt.

Ergebnis: bestanden.

### 6. Prüf- und Extraktionswerkzeug

`tools/tplog_extract.py` wurde geprüft auf:

- Python-Syntax mit `python -m py_compile`,
- erfolgreiche Struktur- und Hashprüfung,
- bytegenaue Extraktion von CSV und TPSIG,
- Ablehnung einer TPLOG nach absichtlicher Änderung eines CSV-Payloadbytes.

Ergebnis: bestanden.

### 7. Webdownload

Statisch bestätigt:

- `.TPLOG` wird als sicherer Logdateiname akzeptiert,
- `.TPLOG` wird unter `Nachweise` eingeordnet,
- MIME-Typ `application/vnd.tp3000.tplog`,
- Dateidatumsreparatur anhand des Namens gilt auch für TPSIG/TPLOG/SHA256,
- Infoseite zeigt TPSIG- und TPLOG-Ausgabe als aktiv.

Das eingebettete JavaScript der `/download`-Seite wurde extrahiert und mit
`node --check` geprüft.

Ergebnis: bestanden.

### 8. Versions- und Formatprüfung

Statisch bestätigt:

- Firmwareversion `0.50.1`,
- Build-ID `0.50.1_36`,
- `TP3000-LOG-SIGNATURE-2` unverändert,
- neuer zusätzlicher Bezeichner `TP3000-LOG-CONTAINER-1`,
- Containercode als `FLASHMEM`,
- 1024-Byte-I/O-Puffer als `DMAMEM`/RAM2.

Ergebnis: bestanden.

## Nicht durchführbar in dieser Umgebung

Nicht als bestanden behauptet werden:

- vollständiger Arduino-/Teensyduino-Kompilier- und Linklauf,
- endgültige FLASH-/RAM1-/RAM2-Ausgabe,
- reale SD-Karten-Latenz während der zusätzlichen CSV-Kopie in TPLOG,
- Stromausfall exakt während TPLOG-Write, Verify oder Rename,
- Langlauf mit großen CSV-Dateien,
- ECDSA-Verifikation mit echtem Geräteschlüssel und Produktionszertifikat,
- vollständige TPLOG-Unterstützung im TP-3000 Viewer,
- gleichzeitige Regelung, Safety, Logging und Webdownload auf Hardware.

Diese Punkte sind nach dem ersten Gerätebuild gemäß `BUILDING.md` zu prüfen.
