# TP-3000 V0.50.1_68 – leere Felder und bereinigter Hinweistext

## Ziel

Ein Kalibrierschein soll keine optisch defekten Leerstellen enthalten. Optionale Felder, für die im signierten Datensatz kein Inhalt vorhanden ist, werden deshalb in der Webansicht und im A4-Ausdruck einheitlich mit dem Gedankenstrich `—` dargestellt.

## Darstellung

Die gemeinsame Hilfsfunktion behandelt folgende Werte als leer:

- `null`
- nicht vorhanden / `undefined`
- leere Zeichenkette
- Zeichenketten, die ausschließlich aus Leerzeichen bestehen

Die Darstellung betrifft unter anderem:

- Zertifikatsaussteller, Bearbeiter und Anschrift
- Zertifikatsreferenz
- Labor-/Schlüsselangaben, sofern optional leer
- Referenznormale, Rückführbarkeit, Messunsicherheit und Umgebungsbedingungen
- Kalibrierverfahren und Akkreditierungsangaben
- optionale Firmwarezertifikat-ID und Hersteller-Root-ID
- optionale Angaben externer PDF-Kalibrierscheine

Pflichtfelder werden weiterhin durch die vorhandenen Import- und Signaturprüfungen validiert. Der Gedankenstrich ist keine Ersatzvalidierung.

## Datenintegrität

Die Änderung findet ausschließlich beim Aufbau des HTML statt. Signierte JSON-Pakete, kanonische Bytefolgen, Manifest-SHA-256, ECDSA-Signaturen, TP3C1 und TP3Q3 werden nicht verändert.

## Bereinigter Hinweistext

Der redundante Absatz unter den Systemkalibrierungs-Steuerelementen der Firmware-Webseite wurde entfernt. Die Vorschau und die vorhandenen Upload-, Download- und SD-Funktionen bleiben unverändert.

## Abgrenzung

Der im KeyGen angezeigte eigenständige Vorschau-Erklärungstext gehört nicht zum Firmwarequellpaket und ist deshalb nicht Bestandteil dieser Firmwareänderung.
