# TP-3000 V0.50.1_41 – Kopfgebundene Zertifikatsauswahl

## Ziel

Die wirksame Systemkalibrierung ist nicht nur an den Gerätetyp gebunden. Maßgeblich ist die aktuell konfigurierte Kombination aus:

- Kopftyp, z. B. `STP-3002`
- Kopf-Seriennummer, z. B. `K12345`
- aktuell wirksamer Gerätejustierung
- aktuell eingestellten und signiert nachgewiesenen Kopfjustierungswerten

Ein Zertifikat eines anderen Kopfes darf weder als aktuell angezeigt noch für zertifizierte Logs verwendet werden.

## Automatische Auswahl

Beim Start, nach Importen und nach einer Änderung des Kopfes durchsucht die Firmware das Systemkalibrierungsarchiv. Ein Kandidat wird nur berücksichtigt, wenn:

1. die Systemsignatur gültig ist,
2. `ValidFrom <= Jetzt <= ValidUntil` gilt,
3. Kopftyp und Kopf-SN exakt dem Setup entsprechen,
4. Gerätejustierungs-ID und Gerätejustierungsmanifest exakt zur aktuellen Gerätejustierung passen,
5. die gebundene Kopfjustierung aus dem Archiv erneut signaturgeprüft werden kann,
6. die Kopfjustierung zeitlich gültig ist und zu den aktuellen Kopfwerten passt,
7. bei externer Quelle Metadaten, PDF-Kopf, Dateigröße und SHA-256 stimmen.

Mehrere passende Pakete werden deterministisch sortiert nach:

1. neuestem `ValidFrom`,
2. neuestem Kalibrierdatum,
3. lexikografisch größtem Systemmanifest als Gleichstandbrecher.

Ein neuerer, aber beschädigter PDF-Kandidat wird übersprungen; ein älteres vollständig gültiges Paket kann weiterhin ausgewählt werden.

## Transaktionale Aktivierung

Die passende Kopfjustierung und die passende Systemkalibrierung werden als Paar aktiviert:

1. Archivdateien erneut prüfen,
2. beide Dateien in temporäre aktive Dateien kopieren,
3. temporäre Dateien erneut prüfen,
4. bisher aktive Dateien als Backup umbenennen,
5. beide neuen Dateien aktivieren,
6. beide Pakete erneut laden und die komplette Bindung prüfen,
7. nur bei Erfolg Backups löschen; bei Fehler beide alten Dateien wiederherstellen.

Dadurch kann ein SD-Fehler niemals still eine neue Systemkalibrierung mit einer unpassenden Kopfjustierung kombinieren.

## Weboberfläche

Die Zertifikatsübersicht zeigt beispielsweise:

```text
Typ / verwendeter Kopf: TP-3000 / STP-3002 K12345
```

Die aktuelle Ansicht und die Zeitraumssuche sind auf genau diesen Kopf begrenzt. Jeder Treffer besitzt `Anzeigen`; bei vorhandenem exakt gebundenem externen Original-PDF zusätzlich `PDF`.

Existiert kein aktuell gültiges Paket, wird dies ausdrücklich für den ausgewählten Kopf gemeldet. Zertifizierte CSV- und TPLOG-Modi bleiben dann gesperrt.

## Speicherplatzierung

- Auswahl-, Archiv-, Datei- und Kryptoprüfpfade: `FLASHMEM`
- wiederverwendete System-/Kopf-/PDF-Arbeitsstrukturen: RAM2 (`DMAMEM`)
- geprüfter externer PDF-Cache: RAM2
- neue feste Fehlermeldungen und Webtexte: Program-Flash
- keine vollständigen Zertifikats- oder PDF-Dateien im RAM

Der Host-Objektvergleich zur Arbeitsbasis ergibt ungefähr:

- RAM2: `+3.056 Byte`
- `FLASHMEM`: `+6.688 Byte`
- Program-Flash für konstante Texte: `+240 Byte`
- keine relevante neue RAM1-Rodata; wiederverwendete RAM2-Strukturen reduzieren zusätzlich den Laufzeit-Stackbedarf.

## Unverändert

Kanonische Kalibrierbytes, Zertifikate, ECDSA-Signaturen, TP3C1/TP3Q3, TPSIG, TPLOG, Skalierungen und Feldreihenfolgen wurden nicht geändert.
