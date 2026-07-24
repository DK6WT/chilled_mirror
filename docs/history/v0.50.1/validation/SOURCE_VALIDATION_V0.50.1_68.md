# Source Validation – V0.50.1_68

## Basis und Ziel

- Basis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_67.zip`
- Ziel: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_68.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_68`

## Durchgeführte Prüfungen

- Gemeinsame HTML-Darstellungsfunktion für `null`, nicht vorhandene, leere und nur aus Leerzeichen bestehende Werte geprüft.
- Deutsch- und Englisch-Testschein mit absichtlich leeren optionalen Feldern in Chromium/Skia als A4-PDF erzeugt.
- Zertifikatsreferenz, Bearbeiter, Anschrift, alle sechs kalibriertechnischen Fachfelder sowie optionale Firmwarezertifikatsfelder zeigten `—`.
- Beide Testausgaben bestanden weiterhin aus drei A4-Seiten.
- Eingebettetes JavaScript extrahiert und mit Node.js syntaktisch geprüft.
- Entfernten redundanten Systemkalibrierungs-Hinweis statisch geprüft.
- Build-ID in `TPsignedData.h` auf `0.50.1_68` geprüft.
- ZIP-CRC-Prüfung und bytegleicher Vergleich nach frischem Entpacken durchgeführt.

## Nicht durchgeführt

- vollständiger Teensyduino-Kompilier- und Linkerlauf
- reale FLASH-/RAM1-/RAM2-Messung
- Hardwaretest am Teensy 4.1
- Drucktest mit dem realen Zielbrowser und Druckertreiber

## Abgrenzung

Der exakt im Screenshot markierte Vorschau-Erklärungstext stammt nicht aus dem Firmwarequellpaket. Seine Entfernung erfordert den aktuellen, bearbeitbaren KeyGen-V0.7.13-Quellstand.
