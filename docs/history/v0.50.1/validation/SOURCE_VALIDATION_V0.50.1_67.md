# Source Validation - TP-3000 V0.50.1_67

## Identität

- interne Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_67`
- Arbeitsbasis: V0.50.1_66, hervorgegangen aus der verbindlichen Arbeitsversion V0.50.1_65
- passendes KeyGen: V0.7.13, unverändert

## Statische Prüfungen

Bestanden:

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_67`.
- Der Dokumentkopf verwendet eine nicht umbrechende rechte Grid-Spalte.
- `#docId` ist im A4-Druck auf eine Zeile festgelegt.
- Die Druckgröße beider TP3C1-Codes ist exakt `112 mm`.
- Der Prüfverfahren-Zusatz liegt innerhalb des QR-Themenblocks als `.qrFoot`.
- Der QR-Themenblock bleibt mit `break-before: page` auf einer eigenen Seite.
- Der JavaScript-Inhalt wurde in Chromium ausgeführt; deutsche und englische Umschaltung funktionierten ohne Seitenfehler.
- Die C++-Raw-String-Begrenzer der Kalibrierscheinsegmente sind vollständig.

## Chromium-/Skia-A4-Layouttest

Mit einem synthetischen vollständigen Systemkalibrierschein, aktivem Firmwareänderungs-Warnstatus und der realistisch langen Nummer `SCAL-G00001-K30001-20260718-B03D60AA` wurden Deutsch und Englisch als A4-PDF ausgegeben und anschließend gerendert.

Bestanden:

- die Nummer im großen Kopf von Seite 1 bleibt in Deutsch und Englisch einzeilig,
- der vollständige Systemkalibrierungsblock beginnt in beiden Sprachen auf Seite 1,
- deutsche Überschriften und Warntexte sind vollständig sichtbar,
- Folgeseiten behalten die kompakte Kalibrierschein-Nr. oben rechts,
- alle Seiten tragen `Seite x von y` beziehungsweise `Page x of y`,
- die QR-Prüfung beginnt auf einer eigenen Seite,
- beide 112-mm-QR-Flächen, beide technischen Beschriftungen und der vollständige Prüfverfahren-Zusatz befinden sich auf derselben QR-Seite,
- es entsteht keine leere zusätzliche Seite für den Prüfverfahren-Zusatz.

Testumgebung: System-Chromium, Playwright/Skia, A4 mit CSS Paged Media. Die erzeugten Test-PDFs hatten in beiden Sprachen jeweils drei Seiten.

## Paketprüfung

Bestanden:

- ZIP-Strukturtest,
- CRC-Test aller ZIP-Einträge,
- bytegleicher Vergleich nach frischem Entpacken.

Der SHA-256 des finalen ZIP-Pakets wird im Übergabetext dokumentiert.

## Noch offen

- vollständiger Teensyduino-Build,
- reale Memory-Usage-Ausgabe,
- Hardwaretest auf Teensy 4.1,
- Drucktest mit dem tatsächlich verwendeten Browser und Druckertreiber sowie echten QR-Codes.
