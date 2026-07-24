# TP-3000 – zusätzlicher TPLOG-Ein-Datei-Container

## Firmwarestand

- Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_36`
- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_11.zip`
- neuer Paketstand: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_12.zip`
- Containerformat: `TP3000-LOG-CONTAINER-1`

## Ausgabedateien

Der zertifizierte Logmodus ersetzt die vorhandene Ausgabe nicht. Nach dem
Versiegeln eines Abschnitts liegen gleichzeitig vor:

```text
/LOG/260716_001.CSV
/LOG/260716_001.TPSIG
/LOG/260716_001.TPLOG
```

Die CSV und TPSIG bleiben normale, voneinander getrennte Dateien. Die TPLOG
enthält zusätzlich bytegenau dieselbe CSV und dieselbe TPSIG. Dadurch kann der
Anwender wahlweise direkt mit der CSV arbeiten, die Zweidateien-Ausgabe
archivieren oder nur die TPLOG als untrennbare Transportdatei weitergeben.

## Sicherheitsmodell

Die TPLOG führt keine zweite unabhängige Gerätesignatur ein. Die kryptografische
Authentizität stammt weiterhin aus der eingebetteten
`TP3000-LOG-SIGNATURE-2`-TPSIG:

- TPSIG signiert CSV-Dateiname, CSV-Größe und CSV-SHA-256,
- TPSIG bindet Gerätezertifikat, Justierungen, Systemkalibrierung,
  Firmwareidentität, Messkonfiguration und Recovery-Status,
- die ECDSA-P-256-Signatur bleibt unverändert.

Der TPLOG-Header und -Footer sichern die Containerstruktur mit CRC-32. Die
beiden eingebetteten Bereiche werden zusätzlich mit SHA-256 gebunden. Ein
Prüfer muss immer sowohl die Containerstruktur als auch die eingebettete TPSIG
prüfen.

## Binärformat

Alle Ganzzahlen sind Little Endian. Der Container besteht lückenlos aus:

```text
[116-Byte-Header]
[CSV-Bytes]
[TPSIG-Bytes]
[40-Byte-Footer]
```

### Header – 116 Byte

| Offset | Größe | Feld |
|---:|---:|---|
| 0 | 8 | Magic `TP3LOG1\0` |
| 8 | 2 | Version, derzeit `1` |
| 10 | 2 | Headergröße, `116` |
| 12 | 4 | Flags |
| 16 | 8 | CSV-Offset |
| 24 | 8 | CSV-Länge |
| 32 | 8 | TPSIG-Offset |
| 40 | 8 | TPSIG-Länge |
| 48 | 32 | SHA-256 der CSV-Bytes |
| 80 | 32 | SHA-256 der TPSIG-Bytes |
| 112 | 4 | CRC-32 über Headerbytes 0 bis 111 |

Flags:

- Bit 0: `RecoveredAfterUncleanShutdown`
- alle übrigen Bits müssen derzeit null sein.

### Footer – 40 Byte

| Offset im Footer | Größe | Feld |
|---:|---:|---|
| 0 | 8 | Magic `TP3END1\0` |
| 8 | 2 | Version, derzeit `1` |
| 10 | 2 | Footergröße, `40` |
| 12 | 4 | reserviert, muss null sein |
| 16 | 8 | gesamte TPLOG-Dateigröße |
| 24 | 8 | Headeroffset, derzeit `0` |
| 32 | 4 | Kopie der Header-CRC-32 |
| 36 | 4 | CRC-32 über Footerbytes 0 bis 35 |

Der Footer liegt immer exakt am Dateiende. Damit kann ein Prüfer zuerst die
letzten 40 Byte lesen und anschließend direkt zu Header, CSV und TPSIG springen.

## Erzeugung

Im zertifizierten Modus läuft der Abschluss transaktional:

1. CSV vollständig schließen und SHA-256 bilden.
2. TPSIG temporär schreiben, erneut lesen und prüfen, danach aktivieren.
3. TPLOG temporär schreiben.
4. Header, Footer, Gesamtgröße, CRC-32 und beide Abschnitts-SHA-256 erneut aus
   der temporären TPLOG prüfen.
5. Erst danach die temporäre Datei in `.TPLOG` umbenennen.
6. Journal und eingefrorenen Kontext erst entfernen, wenn CSV, TPSIG und TPLOG
   vollständig vorhanden sind.

Scheitert die TPLOG-Erzeugung, bleiben Journal und Kontext erhalten. Der
Abschluss kann dadurch nach Neustart erneut versucht werden.

## Stromausfall-Recovery

Nach einem unsauberen Neustart wird die CSV wie bisher bis zur letzten
vollständigen LF-terminierten Zeile wiederhergestellt. Danach entstehen:

- TPSIG mit `RecoveredAfterUncleanShutdown=true`,
- TPLOG mit gesetztem Headerflag Bit 0.

Beide Kennzeichnungen müssen bei der Offlineprüfung übereinstimmen.

## Webdownload

`.TPLOG` wird unter `/download` als `Nachweis` gelistet und mit dem MIME-Typ

```text
application/vnd.tp3000.tplog
```

übertragen. CSV und TPSIG bleiben separat sichtbar und herunterladbar.

## Prüf- und Extraktionswerkzeug

Das paketierte Hilfsprogramm

```text
tools/tplog_extract.py
```

prüft:

- Header-/Footer-Magic und Version,
- Header-/Footergrößen,
- CRC-32 beider Strukturblöcke,
- lückenlose Offsets und Längen,
- SHA-256 der eingebetteten CSV,
- SHA-256 der eingebetteten TPSIG,
- TPSIG-Format, `LogFileName`, `LogFileSize` und `LogFileSha256`.

Beispiel:

```text
python tools/tplog_extract.py 260716_001.TPLOG --verify-only
python tools/tplog_extract.py 260716_001.TPLOG -o extrahiert
```

Das Hilfsprogramm prüft bewusst noch nicht die ECDSA-Signatur oder die
Zertifikatskette. Diese vollständige kryptografische Prüfung bleibt Aufgabe des
TP-3000 Viewers beziehungsweise der späteren Verifier-Anwendung.

## Speicheraufteilung

- Containererzeugung und -prüfung: `FLASHMEM`
- 1024-Byte-I/O-Puffer: `DMAMEM` / RAM2
- keine vollständige CSV oder TPSIG im RAM
- kein zusätzlicher 64-MiB-SPI-/QSPI-Flash verwendet
