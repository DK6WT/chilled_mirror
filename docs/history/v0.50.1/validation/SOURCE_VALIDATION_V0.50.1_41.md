# Source Validation V0.50.1_41

## Prüfgegenstand

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_16.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_17.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_41`

## Geänderte Quellbereiche

- `TPsignedCalibration.cpp`
  - kopfbezogene Archivsuche und deterministische Auswahl
  - erneute Signatur-/Bindungsprüfung historischer Kopf- und Systempakete
  - transaktionale gemeinsame Aktivierung von Kopfjustierung und Systemkalibrierung
  - exakte Prüfung gebundener historischer PDFs
  - Filterung der Zeitraumssuche nach Kopftyp und Kopf-SN
  - Arbeitsstrukturen in RAM2, nicht zeitkritische Pfade in `FLASHMEM`
- `TPexternalCalibration.cpp/.h`
  - exakte Prüfung eines archivierten PDFs anhand vollständiger gebundener Metadaten und SHA-256
  - Laufzeitcache in RAM2
- `TPethernet.ino`
  - Anzeige `TP-3000 / <Kopftyp> K<Seriennummer>`
  - aktives Zertifikat und Suchtreffer auf den ausgewählten Kopf begrenzt
  - eindeutige Leermeldung und unveränderte `Anzeigen`-/`PDF`-Aktionen
  - Cache-Invalidierung nach Kopfänderung im Web-Setup
- `TPheadCalibration.ino`, `TPmenu_Pages.ino`
  - Cache-Invalidierung nach Kopfänderung bzw. Übernahme neuer Kopfwerte
- `TPsignedData.h`
  - Build-ID `0.50.1_41`

## Hostprüfungen

Bestanden:

1. C++-Syntaxprüfung mit Teensy-kompatiblen Host-Stubs:
   - `TPsignedCalibration.cpp`
   - `TPexternalCalibration.cpp`
2. JavaScript-Syntaxprüfung der eingebetteten Zertifikatsauswahlseite mit `node --check`.
3. Hostmodell `tools/test_head_bound_certificate_selection.py`:
   - exakter Filter nach Kopftyp und Kopf-SN
   - Überschneidungsregel `ValidFrom <= SearchEnd && ValidUntil >= SearchStart`
   - Auswahl nach `ValidFrom`, Kalibrierdatum und Manifest
   - Überspringen eines neueren Kandidaten mit ungültigem externem PDF
   - kein Treffer für einen anderen Kopf
4. Statische Quellassertionen für Kopfbindung, Auswahlreihenfolge, gemeinsame Aktivierung, PDF-Prüfung und Webfelder.
5. Kritischer Invariantenvergleich zur Arbeitsbasis:
   - `scWriteSystemValuesV1/V2` bytegleich
   - `scWriteApprovalV1/V2/V3/V4` bytegleich
   - eingebettete TP3C1-Encoderbibliothek bytegleich
   - `TPcertifiedLog.cpp/.h`, `TPsignedData.cpp`, `TPsha256.cpp/.h` bytegleich

## Speicherplatzierung – Host-Objektvergleich

Vergleich mit identischen `clang++ -O0`-Flags zur Arbeitsbasis:

- `TPsignedCalibration.cpp`
  - RAM2/`.dmabuffers`: `+2.784 Byte`
  - `FLASHMEM`: `+5.536 Byte`
  - Program-Flash: `+240 Byte`
  - normale String-Rodata: `-4 Byte`
- `TPexternalCalibration.cpp`
  - RAM2/`.dmabuffers`: `+272 Byte`
  - `FLASHMEM`: `+1.152 Byte`

Gesamtabschätzung:

- RAM2: etwa `+3.056 Byte`
- `FLASHMEM`: etwa `+6.688 Byte`
- keine relevante neue globale RAM1-Belegung; wiederverwendete RAM2-Arbeitsstrukturen reduzieren zusätzlich große lokale Strukturen im Archivsuchpfad.

Die endgültigen Teensy-Linkerwerte können nur der vollständige Teensyduino-Build liefern.

## Sicherheits- und Funktionsprüfung

- Ein Zertifikat eines anderen Kopftyps oder einer anderen Kopf-SN wird nicht als aktuell geliefert und nicht in der Suche angezeigt.
- Zertifizierte Logmodi verwenden weiterhin `tpSignedCalibrationSystemReady()` und bleiben ohne exakt passendes, aktuell gültiges Paket gesperrt.
- Geräte-, Kopf- und Systempakete werden vor Auswahl erneut signaturgeprüft.
- Ein extern gebundenes PDF wird über Dokument-ID, Nummer, Gültigkeitsdaten, Originaldateiname, Größe, `%PDF-` und SHA-256 geprüft.
- Ein beschädigter neuerer Kandidat verhindert nicht die Auswahl eines älteren vollständig gültigen Pakets.
- Kopf- und Systempaket werden als Paar mit temporären Dateien und Backups aktiviert; bei Fehler wird der vorherige Zustand wiederhergestellt.

## Noch ausstehend

- vollständiger Teensyduino-Build
- FLASH-/RAM1-/RAM2-Linkerwerte auf Teensy 4.1
- Hardwaretest mit mindestens zwei Köpfen und je einer gleichzeitig gültigen Systemkalibrierung
- Test von Kopfwechsel über Web und TFT, inklusive Sperre der zertifizierten Logmodi ohne passenden Treffer
- Test eines gebundenen externen PDFs und eines absichtlich veränderten Archiv-PDFs
