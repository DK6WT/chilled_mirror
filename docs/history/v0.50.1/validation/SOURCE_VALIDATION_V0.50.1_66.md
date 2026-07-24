# Source Validation – TP-3000 V0.50.1_66

## Identität

- interne Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_66`
- Arbeitsbasis: frisch entpackte `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_65.zip`
- passendes KeyGen: V0.7.13, unverändert

## Statische Prüfungen

Bestanden:

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_66`.
- Die frühere feste HTML-Fußzeile `printCertificateNumber` ist vollständig entfernt.
- Der dynamische Druckstil enthält A4-Seitenränder sowie `@top-right`, `@bottom-right`, `counter(page)`, `counter(pages)` und `@page :first`.
- Der Sprachumschalter enthält genau `Deutsch` und `English`.
- Die gewählte Sprache wird fehlertolerant im Browser gespeichert.
- Der JavaScript-Teil des Kalibrierscheins besteht `node --check`.
- C++-Raw-String-Begrenzer der drei Kalibrierscheinsegmente sind vollständig und eindeutig.

## Chromium-/Skia-Ende-zu-Ende-Layouttest

Mit einem synthetischen vollständigen Systemkalibrierschein wurden deutsche und englische Webansicht sowie A4-PDF erzeugt.

Bestanden:

- deutsche und englische Überschriften, Tabellenfelder, Status- und Begleittexte werden umgeschaltet,
- Seite 1 enthält keine zusätzliche kompakte `Kalibrierschein-Nr.` im Seitenkopf,
- Seiten 2 und 3 enthalten die Kalibrierschein-Nr. oben rechts,
- alle Seiten enthalten `Seite x von 3` beziehungsweise `Page x of 3` unten rechts,
- die erste Inhaltsüberschrift auf Folgeseiten liegt vollständig unterhalb des Seitenkopfes,
- die QR-Prüfung beginnt auf einer eigenen Seite,
- Themenblöcke bleiben in der Testbelegung vollständig gruppiert.

Testumgebung: System-Chromium mit CSS-Paged-Media-Seitenrandboxen, PDF-Erzeugung über Playwright/Skia.

## Paketprüfung

Bestanden:

- ZIP-Strukturtest,
- bytegleicher Vergleich nach frischem Entpacken.

Der SHA-256 des finalen ZIP-Pakets wird im Übergabetext dokumentiert.

## Noch offen

- vollständiger Teensyduino-Build,
- reale Memory-Usage-Ausgabe,
- Hardwaretest auf Teensy 4.1,
- realer Drucktest aus dem am TP-3000 verwendeten Browser und Druckertreiber.
