# Externe PDF-Kalibrierscheine – Build 0.50.1_33

## Bedienung

Der Upload befindet sich auf der bestehenden Web-Seite `/calibration`.

Pflichtangaben:

- Kalibrierschein-Nr. / Zeichen: 1 bis 32 druckbare Zeichen
- Gültig von: Kalenderdatum
- Gültig bis: Kalenderdatum, nicht vor „Gültig von“
- PDF-Datei: 5 Byte bis maximal 6 MiB

Die PDF-Datei wird nicht in Text umgewandelt und nicht neu geschrieben. Der Browser sendet binäre Blöcke; die Firmware schreibt diese direkt auf die SD-Karte.

## SD-Struktur

```text
/CALIBRATION/EXTERNAL/UPLOAD.tmp
/CALIBRATION/EXTERNAL/<DOCUMENT-ID>.pdf
/CALIBRATION/EXTERNAL/<DOCUMENT-ID>.json
/CALIBRATION/EXTERNAL/ACTIVE.json
/CALIBRATION/EXTERNAL/ACTIVE.tmp
/CALIBRATION/EXTERNAL/ACTIVE.bak
```

`DOCUMENT-ID` sind die ersten 16 Bytes des vollständigen PDF-SHA-256 in hexadezimaler Schreibweise. Das vollständige SHA-256 bleibt zusätzlich in den Metadaten erhalten.

## Prüf- und Aktivierungsfolge

1. Metadaten und Größe prüfen.
2. Temporäre Datei `UPLOAD.tmp` neu anlegen.
3. Ersten Block auf `%PDF-` prüfen.
4. Alle Blöcke in exakter Offset-Reihenfolge schreiben und gleichzeitig SHA-256 bilden.
5. Datei vollständig schließen.
6. Datei erneut öffnen, Größe und SHA-256 nochmals über alle Bytes bestimmen.
7. Geprüfte Originaldatei unter ihrer Dokument-ID archivieren.
8. Dokumentbezogene JSON-Metadaten schreiben, ohne einen bestehenden abweichenden Datensatz zu überschreiben.
9. `ACTIVE.json` über temporäre Datei und Backup wechseln.
10. Aktiven Zeiger einschließlich PDF-Größe und PDF-SHA-256 zurücklesen und erst danach das Backup löschen.

Beim Neustart wird die aktive PDF erneut vollständig gehasht. Eine nachträglich veränderte oder fehlende PDF verliert dadurch sofort ihren Aktivstatus.

## Systemkalibrierung

Die V2-Systemkalibrierungsanfrage enthält die Metadaten des aktiven externen PDFs. Im KeyGen wird anschließend eine der beiden Quellen gewählt:

- `TP3000`: eigener digitaler TP-3000-Kalibrierschein mit den sechs Fachangaben.
- `EXTERNAL_PDF`: externer PDF-Kalibrierschein; die sechs Fachangaben müssen leer sein.

Bei `EXTERNAL_PDF` bindet die Signatur folgende Werte:

- Dokument-ID
- Kalibrierschein-Nr. / Zeichen
- Gültig von / bis
- Originaldateiname
- Dateigröße
- vollständiger SHA-256

Wird später ein anderes PDF aktiviert, bleibt die signierte Systemkalibrierung gespeichert, ist aber wegen der abweichenden PDF-Bindung nicht mehr aktiv/verwendungsbereit.

## Abgrenzung

Build 0.50.1_33 verwendet ausschließlich die SD-Karte. Es gibt weder einen Treiber noch reservierte Rohadressen für den später vorgesehenen externen 64-MiB-Flash.
