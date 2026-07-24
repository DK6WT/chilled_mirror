# Kalibrierschein: Deutsch/English und wiederholte Seitenkennzeichnung – V0.50.1_66

## Ziel

Der im TP-3000 erzeugte Kalibrierschein kann auf der Web-Seite zwischen Deutsch und Englisch umgeschaltet werden. Die Druckausgabe soll auf jeder Folgeseite eindeutig zum Kalibrierschein gehören, eine echte Seitenangabe enthalten und auf Folgeseiten keinen Inhalt mehr am oberen Seitenrand abschneiden.

## Sprachumschaltung

Auf `/calibration-certificate-view` stehen die Schaltflächen `Deutsch` und `English` zur Verfügung. Die Auswahl gilt gemeinsam für:

- Webansicht,
- Druckvorschau und PDF-Druck,
- Überschriften und Feldbezeichnungen,
- Status-, Warn- und Erläuterungstexte,
- QR-Überschriften und QR-Begleittexte,
- Kopf- und Fußzeilen.

Die Auswahl wird im Browser unter `tp3000-cal-certificate-language` gespeichert. Messwerte, IDs, Hashes, Signaturen, Zertifikatsdaten und QR-Nutzdaten werden durch die Sprache nicht verändert.

Bei einem gebundenen externen PDF-Kalibrierschein wird nur die vom TP-3000 erzeugte Begleit- und Nachweisseite übersetzt. Das externe Original-PDF bleibt unverändert.

## Druckkopf und Druckfuß

Der Druck verwendet CSS Paged Media mit einem auf jeder A4-Seite reservierten Randbereich:

- oben: 18 mm,
- links und rechts: 14 mm,
- unten: 16 mm.

Damit beginnt auch der Inhalt jeder Folgeseite unterhalb des reservierten Kopfbereichs. Der frühere Effekt, dass ab Seite 2 etwa 12,5 mm der Überschrift abgeschnitten wurden, wird dadurch an der Ursache beseitigt.

### Seite 1

Die Kalibrierschein-Nr. steht ausschließlich im vorhandenen großen Dokumentkopf. Eine zusätzliche Wiederholung im kleinen Seitenkopf wird auf der ersten Seite mit `@page :first` unterdrückt.

### Seiten 2 bis Ende

Oben rechts steht kompakt:

- Deutsch: `Kalibrierschein-Nr.: <Nummer>`
- Englisch: `Calibration certificate No.: <number>`

### Alle Seiten

Unten rechts steht:

- Deutsch: `Seite x von y`
- Englisch: `Page x of y`

Die Seitenzählung einschließlich Gesamtseitenzahl wird vom Chromium-Drucksystem über `counter(page)` und `counter(pages)` erzeugt.

## Themen- und QR-Paginierung

Die vorhandene Themenlogik bleibt erhalten:

- vollständige Themen dürfen gemeinsam auf einer Seite stehen,
- Themen und einzelne As-Found-/As-Left-Tabellen werden möglichst nicht geteilt,
- übergroße Inhalte dürfen weiterhin sinnvoll umbrechen,
- die Offline-Prüfung mit beiden TP3C1-QR-Codes beginnt immer auf einer eigenen Seite,
- auch die QR-Seite trägt Kalibrierschein-Nr. und Seitenzahl.

## Unverändert

Unverändert bleiben insbesondere:

- `.tpdcal`, `.tphcal`, `.tpscal` und Firmwarekontext,
- TP3C1 V0.4 und TP3Q3 QR-4,
- Signaturen und Hashes,
- Messung, Filterung, Regelung und Safety,
- Inhalt und SHA-256 externer PDF-Kalibrierscheine.
