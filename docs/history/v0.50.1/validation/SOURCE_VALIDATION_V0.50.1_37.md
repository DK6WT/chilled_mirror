# Source Validation – TP-3000 V0.50.1 / Build 0.50.1_37

## Prüfgegenstand

- Basis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_12.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_13.zip`
- neue Auswahl: Aus / SHA-256 / CSV zertifiziert / TPLOG zertifiziert

## Durchgeführte Hostprüfungen

`TPcertifiedLog.cpp`, `TPsignedData.cpp` und `TPsha256.cpp` wurden mit dem vorhandenen
Arduino-/SD-Hoststub unter C++17 mit `-Wall -Wextra -Wpedantic` kompiliert. Ergebnis: ohne
Warnungen.

Geprüft und bestanden:

1. CSV-zertifiziert erzeugt `.CSV` und `.TPSIG`, aber keine `.TPLOG`.
2. TPLOG-zertifiziert erzeugt nach Abschluss nur `.TPLOG`; CSV und TPSIG sind im Container
   vorhanden, auf der SD aber nicht mehr separat.
3. `tools/tplog_extract.py --verify-only` bestätigt Header/Footer, CRC-32, Abschnittsgrenzen
   und beide SHA-256-Werte.
4. SHA-256 erzeugt `.CSV` plus `.CSV.sha256` und keine zertifizierten Dateien.
5. Moduswechsel nach einer TPLOG verwendet den nächsten Sequenzwert (`_002` statt erneuter
   Nutzung von `_001`).
6. Rotation bei Konfigurations- und Datumsänderung bleibt aktiv.
7. Das JavaScript der Setupseite wurde aus dem eingebetteten Raw-String extrahiert und mit
   `node --check` geprüft.
8. TFT- und Webvalidierung sperren beide zertifizierten Werte 2 und 3 gemeinsam, solange die
   Nachweiskette nicht vollständig ist.
9. Der Snapshotpfad übernimmt im TPLOG-Modus den von der Versiegelung gelieferten
   `.TPLOG`-Dateinamen.

## Nicht als bestanden behauptet

- vollständiger Teensyduino-Kompilier- und Linklauf,
- endgültige FLASH-/RAM1-/RAM2-Werte,
- reale SD-Langlauf- und Stromausfalltests auf Hardware,
- ECDSA-Verifikation mit echtem Geräte- und Produktionsschlüssel,
- Viewer-Unterstützung für TPLOG.
