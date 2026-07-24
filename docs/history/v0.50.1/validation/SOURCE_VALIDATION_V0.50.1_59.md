# Source Validation V0.50.1_59

Geprüft wurde der frisch aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_34.zip` entpackte und ausschließlich für Build `0.50.1_59` geänderte Quellstand.

## Statische Prüfungen

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_59`.
- Der Druckschein besitzt eine nur im Druck sichtbare, feste Fußzeile `printCertificateNumber`.
- Die Fußzeile wird aus derselben `CalibrationId` wie die sichtbare Dokumentkennung gespeist.
- Status, Zertifikatsaussteller, Systemkalibrierung, Kalibrierergebnisse, Gerätejustierung, Kopfjustierung und kalibriertechnische Angaben tragen einen Themenblock mit `break-inside: avoid-page` und `page-break-inside: avoid`.
- Einzelne As-Found-/As-Left-Ergebnistabellen sind als `calResult` separat zusammengehalten.
- Überschriften werden nicht allein am Seitenende abgetrennt.
- Die Offline-Prüfung verwendet unabhängig von der QR-Teilezahl `break-before: page` und `page-break-before: always`.
- Der untere Seiteninnenabstand wurde vergrößert, damit die wiederholte Kalibrierschein-Nr. keine Inhalte überlagert.

## JavaScript-Prüfung

Die im Firmwarequelltext getrennt gespeicherten Bestandteile der Kalibrierscheinseite wurden in der tatsächlichen Auslieferungsreihenfolge zusammengesetzt. `node --check` meldete keinen Syntaxfehler.

## Druck- und PDF-Test

Die endgültigen Druckregeln wurden mit Chromium/Skia in einem A4-Testschein mit realistischen Abschnittshöhen ausgeführt und anschließend als PDF gerendert:

- 3 A4-Seiten erzeugt,
- Seite 1 enthält vollständige Systemkalibrierung und vollständige Kalibrierergebnisse,
- Seite 2 enthält vollständige Gerätejustierung, Kopfjustierung und kalibriertechnische Angaben,
- Seite 3 enthält ausschließlich die Offline-Prüfung mit beiden 115-mm-QR-Blöcken,
- die Kalibrierschein-Nr. ist per Textprüfung auf allen drei Seiten vorhanden,
- visuelle Prüfung der gerenderten Seiten: keine abgeschnittenen Überschriften, keine geteilten Tabellen, keine Überlagerung durch die Fußzeile.

Der Testschein ist nur eine Layoutfixture; kryptografische QR-Inhalte und reale Gerätedaten wurden dadurch nicht verändert.

## Paketprüfung

- ZIP-Struktur und CRC wurden geprüft.
- Der Stand wurde erneut frisch entpackt und bytegleich mit dem Arbeitsverzeichnis verglichen.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage sowie Drucktest direkt vom TP-3000 mit realen Kalibrierdaten.
