# TP-3000 – Kalibrierzertifikat-Auswahl und SD-Archiv

## Stand

- Quellbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_13.zip`
- neuer Paketstand: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_14.zip`
- interne Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_38`

## Bedienablauf

Der bisherige direkte Aufruf des Kalibrierscheins führt nun zuerst auf die Auswahlseite:

- `/calibration-certificate`

Die Seite enthält:

1. **Aktuelles Kalibrierzertifikat** mit kompaktem Status und eigenem Knopf **Anzeigen**.
2. **Kalibrierzertifikate suchen** mit den Pflichtfeldern **Zeitraum von** und **Zeitraum bis**.
3. Eine Trefferliste, in der jedes Zertifikat seinen eigenen Knopf **Anzeigen** erhält.
4. Ist ein externes Original-PDF eindeutig gebunden und auf der SD vorhanden, erscheint direkt hinter **Anzeigen** zusätzlich **PDF**.

Die eigentliche vollständige Zertifikatsansicht liegt unter:

- `/calibration-certificate-view`
- historischer Datensatz: `/calibration-certificate-view?id=<SYSTEM-MANIFEST-SHA256>`

Der PDF-Knopf öffnet das exakt gebundene externe Original-PDF inline:

- `/calibration-external-pdf?id=<PDF-DOCUMENT-ID>&view=1`

## Zeitraumssuche

Die Eingaben sind reine Kalenderdaten. Ein Zertifikat wird gefunden, wenn sich sein Gültigkeitszeitraum mit dem Suchzeitraum überschneidet:

```text
ValidFrom <= SearchEnd && ValidUntil >= SearchStart
```

Die Grenzen zählen mit. Ein Zertifikat, das genau am ersten oder letzten Suchtag gültig ist, wird daher gefunden.

Die Treffer werden nach `Gültig von`, danach nach Kalibrierdatum und schließlich nach Manifest-ID sortiert; jeweils neueste zuerst. Die Weboberfläche zeigt höchstens 24 Treffer und weist auf weitere Treffer hin.

## Anzeige eines Zertifikats

**Anzeigen** öffnet die TP-3000-Zertifikatsansicht mit:

- Zertifikatsaussteller und Dokumentdaten,
- Systemkalibrierung,
- Kalibrierergebnissen,
- der exakt gebundenen Gerätejustierung,
- der exakt gebundenen Kopfjustierung,
- kalibriertechnischen Angaben bei einem TP-3000-eigenen Kalibrierschein,
- Hinweis und Metadaten zum externen PDF-Kalibrierzertifikat bei externer Quelle,
- TP3C1-QR-Codes zur Offline-Prüfung,
- TP3Q3-Download der vollständigen QR-Daten.

Bei einer historischen Systemkalibrierung werden Geräte- und Kopfjustierung nicht aus den aktuell aktiven Dateien übernommen. Stattdessen liest die Firmware die in der signierten Systemkalibrierung enthaltenen Manifest-SHA-256-Werte und lädt genau diese archivierten Pakete.

## SD-Archiv

Die signierten Originalpakete werden inhaltsadressiert über ihren Manifest-SHA-256 abgelegt:

```text
/CALIBRATION/ARCHIVE/DEVICE/<64-HEX-MANIFEST>.tpdcal
/CALIBRATION/ARCHIVE/HEAD/<64-HEX-MANIFEST>.tphcal
/CALIBRATION/ARCHIVE/SYSTEM/<64-HEX-MANIFEST>.tpscal
```

Beim Aktivieren neuer Pakete gilt:

1. bisher aktives Paket in das Archiv übernehmen,
2. Archivkopie schließen und erneut lesen,
3. Signatur und Manifest erneut prüfen,
4. neues Paket aktivieren,
5. neues aktives Paket ebenfalls archivieren,
6. bei einem Fehler den vorherigen aktiven Stand wiederherstellen.

Beim ersten Start dieses Builds werden vorhandene aktive Geräte-, Kopf- und Systempakete ebenfalls in das neue Archiv übernommen.

Bereits vor diesem Build verworfene ältere Pakete können nicht rückwirkend rekonstruiert werden. Die vollständige Historie beginnt daher mit den beim ersten Start noch vorhandenen aktiven Paketen und allen danach importierten Paketen.

## Externe PDF-Kalibrierzertifikate

Externe PDFs bleiben im bestehenden SD-Archiv. Der PDF-Knopf wird nur angeboten, wenn:

- die Systemkalibrierung eine externe PDF-Dokument-ID bindet,
- Metadaten- und PDF-Datei für diese Dokument-ID vorhanden sind,
- die PDF-Größe plausibel ist.

Beim Öffnen werden Metadaten und Original-PDF vollständig geprüft:

- Dokument-ID,
- `%PDF-`-Dateikopf,
- Dateigröße,
- SHA-256 des Original-PDFs.

Damit öffnet **PDF** nicht irgendeinen aktuell aktiven Kalibrierschein, sondern exakt das von der ausgewählten Systemkalibrierung referenzierte Original.

## Integrität und unveränderte Formate

Historische `.tpdcal`, `.tphcal` und `.tpscal` werden bei jeder Ausgabe erneut kryptografisch geprüft. Die Manifest-ID im Dateinamen muss mit dem verifizierten Manifest des Inhalts übereinstimmen.

Nicht verändert wurden:

- Zertifikats- und Kalibrierformate,
- kanonische Signaturbytes,
- ECDSA P-256 / SHA-256,
- Feldreihenfolgen und Skalierungen,
- TP3C1-Encoder und TP3Q3-Transport,
- QR-Ableitungsregeln.

Die neue Funktion ergänzt ausschließlich Archivierung, Suche, Auswahl und historische Anzeige.
