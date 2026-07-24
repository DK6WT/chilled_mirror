# TP-3000 – zertifizierter SD-Logmodus

## Firmwarestand

- Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_35`
- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-15_V0.50.1_10.zip`
- Datum: 16.07.2026
- Speicherziel: interne SD-Karte; kein zusätzlicher SPI-/QSPI-Flash

## Betriebsarten

### Aus

Unverändertes bisheriges Tageslogging mit `YYMMDD.CSV` beziehungsweise
`YYMMDDD.CSV`.

### SHA-256

Jeder Abschnitt erhält einen nummerierten, nach Abschluss unveränderlichen
Dateinamen:

```text
/LOG/YYMMDD_001.CSV
/LOG/YYMMDD_001.CSV.sha256
```

Die Begleitdatei enthält eine GNU-kompatible SHA-256-Zeile.

### Zertifiziert

```text
/LOG/YYMMDD_001.CSV
/LOG/YYMMDD_001.TPSIG
```

Die `.TPSIG` verwendet `TP3000-LOG-SIGNATURE-2` und enthält:

- exakten CSV-Dateinamen, Dateigröße und SHA-256,
- Startzeit, Zeit des letzten bestätigten Flushs und Versiegelungszeit,
- Geräte-SN und Geräte-Key-ID,
- Gerätezertifikat,
- aktive signierte Gerätejustierung,
- aktive signierte Kopfjustierung,
- aktive signierte Systemkalibrierung,
- sichtbare Firmwareidentität mit Status `DEVELOPMENT`, solange kein
  Hersteller-Firmwaremanifest vorliegt,
- messrelevante Konfiguration,
- SHA-256 des eingefrorenen Gesamtkontextes,
- Kennzeichen `RecoveredAfterUncleanShutdown`,
- ECDSA-P-256-Gerätesignatur als 64 Byte `R || S` / IEEE-P1363.

Der private Geräteschlüssel wird über die bereits vorhandene
`deviceIdentitySignHash()`-Schnittstelle verwendet und nicht exportiert.

## Freigabebedingungen

Der Modus `Zertifiziert` ist nur auswählbar, wenn folgende Nachweise vorhanden
und aktuell kryptografisch gültig sind:

1. Gerätezertifikat,
2. signierte Gerätejustierung,
3. signierte Kopfjustierung,
4. an beide Justierungen gebundene signierte Systemkalibrierung.

Ein Hersteller-Firmwaremanifest bleibt im Entwicklungsstand noch informativ und
blockiert die Auswahl nicht. Die `.TPSIG` weist diesen Zustand ausdrücklich als
`DEVELOPMENT` aus.

## Eingefrorener Kontext

Vor dem ersten CSV-Byte eines neuen Abschnitts wird eine temporäre `.CTX`-Datei
angelegt. Sie enthält die zu diesem Abschnitt gehörenden Nachweise und die
Messkonfiguration. Ihr SHA-256 wird im Journal gespeichert und in der
Gerätesignatur gebunden.

Vor dem Erzeugen der `.TPSIG` wird die `.CTX` erneut vollständig gehasht. Fehlt
sie oder weicht der Hash ab, wird keine Logsignatur ausgegeben. Nach erfolgreicher
und geprüfter `.TPSIG` wird die `.CTX` entfernt; für die Offline-Prüfung werden
nur CSV und `.TPSIG` benötigt.

## CRC-Journal und Stromausfall

Während ein Integritätsabschnitt geöffnet ist, liegt folgendes Journal auf SD:

```text
/LOG/TPLOG.HST
```

Temporäre und Sicherungsnamen:

```text
/LOG/TPLOG.TMP
/LOG/TPLOG.BAK
```

Das Journal enthält unter anderem:

- CSV-Pfad und Abschnittsnummer,
- Integritätsmodus und Diagnoseformat,
- Startdatum und Startzeit,
- zuletzt bestätigte Dateigröße und UTC-Zeit,
- Manifest-Hashes aller gebundenen Nachweise,
- Hash der Messkonfiguration,
- Hash des eingefrorenen Kontextes,
- CRC-32 über den vollständigen Journaldatensatz.

Die bestätigte Dateigröße wird erst nach erfolgreichem CSV-Flush und Schließen
der Datei fortgeschrieben.

Nach einem unsauberen Neustart:

1. letztes gültiges Journal laden,
2. CSV ab der zuletzt bestätigten Position lesen,
3. vollständige, mit `LF` abgeschlossene Zeilen behalten,
4. unvollständiges Dateiende abschneiden,
5. Kontext-SHA-256 erneut prüfen,
6. CSV hashen und Abschnitt versiegeln,
7. `.TPSIG` mit `RecoveredAfterUncleanShutdown=true` erzeugen.

Die Journaldateien werden nach erfolgreicher Versiegelung vor der temporären
Kontextdatei entfernt. Dadurch kann ein Stromausfall beim Aufräumen kein Journal
zurücklassen, dessen Kontext bereits gelöscht wurde.

## Abschnittswechsel

Ein neuer Abschnitt wird begonnen, wenn sich einer der folgenden Werte ändert:

- Integritätsmodus,
- Kalendertag oder gültige/ungültige Zeitbasis,
- normales oder diagnostisches CSV-Format,
- Gerätezertifikat,
- Gerätejustierung,
- Kopf/Kopfjustierung,
- Systemkalibrierung einschließlich externer PDF-Bindung,
- Firmwareidentität beziehungsweise späteres Firmwaremanifest,
- messrelevante Konfiguration.

Noch nicht auf SD geschriebene Pufferwerte werden bei einem solchen
Provenienzwechsel bewusst verworfen. Dadurch werden nie Werte unterschiedlicher
Nachweiszustände in derselben signierten CSV gemischt.

## Webdownload

Unter `/download` stehen die Filter zur Verfügung:

- Messdaten,
- Seriellog,
- Nachweise.

Der Nachweisfilter zeigt `.TPSIG` und `.sha256`. Bei einem Snapshot der aktuell
beschriebenen CSV im SHA-256- oder zertifizierten Modus wird zuerst der RAM-Puffer
geschrieben, der Abschnitt versiegelt und danach die unveränderliche Datei
geladen. Das weitere Logging beginnt anschließend automatisch in einem neuen
nummerierten Abschnitt.

## Speicheraufteilung

- `TPcertifiedLog.cpp`: nicht zeitkritische Funktionen in `FLASHMEM`.
- Journalzustand und Kontext-Arbeitspuffer: `DMAMEM` / RAM2.
- CSV-, Kontext-, Kalibrier- und Zertifikatsdaten werden blockweise verarbeitet;
  eine vollständige CSV wird nicht in den RAM geladen.
- `File`- und `TpSha256`-Objekte bleiben als lokale C++-Objekte im regulären RAM.

Die endgültige RAM1-/RAM2-/FLASH-Belegung muss mit dem Teensy-4.1-Linker des
realen Projekts gemessen werden.

## Geänderte beziehungsweise neue Quelldateien

- `TPcertifiedLog.h` – neue öffentliche Logschnittstelle
- `TPcertifiedLog.cpp` – Segmentierung, Kontext, Journal, Hash und `.TPSIG`
- `TPsdLog.ino` – Integration in CSV-Flush, Rotation und Web-Snapshot
- `TPethernet.ino` – Nachweisfilter, Downloads und Snapshot-Versiegelung
- `TPsignedCalibration.h/.cpp` – streamingfähige Ausgabe aktiver Pakete
- `TPsignedData.h/.cpp` – Build-ID, V2-Formatkennung und Freigabe
- `TP-3000.ino` – Startreihenfolge für Nachweise vor Log-Recovery
- `SIGNED_DATA_FORMATS_V1.md` – Dokumentation von V1 und aktivem V2
