# TP-3000 V0.50.1_43 – dynamische Web-Seiten vollständig beim ersten Aufruf

## Fehlerbild

Große dynamisch erzeugte Seiten konnten beim ersten Aufruf unvollständig im Browser erscheinen. Sichtbar war beispielsweise unter `/calibration` nur der Anfang der Überschrift `Jus`; beim zweiten Aufruf wurde die Seite meistens vollständig geladen.

Betroffen waren insbesondere:

- `/identity`
- `/validity`
- `/calibration`

## Ursache

Diese Seiten werden abhängig vom aktuellen Zertifikats-, Justierungs- und Kalibrierzustand in `ethJsonBuf` aufgebaut. Anschließend wurden bis zu 16 KiB HTML vollständig über `EthBoundedWriter` gesendet. Dieser Writer besitzt absichtlich ein hartes Antwortbudget von 120 ms, damit ein langsamer Browser den Mess-, Regel- und Safety-Hauptloop nicht lange blockieren kann.

War der NativeEthernet-TCP-Sendepuffer innerhalb dieser 120 ms nicht schnell genug wieder frei, brach der Writer die Antwort entsprechend der angekündigten `Content-Length` als unvollständig ab. Der Browser stellte dann nur den bereits empfangenen Anfang dar. Beim zweiten Aufruf war der TCP-/Browserzustand häufig günstiger, wodurch der Fehler scheinbar verschwand.

## Korrektur

Die drei dynamischen Verwaltungsseiten verwenden nun den bereits vorhandenen asynchronen Seitentransfer:

- exakte `Content-Length`
- maximal 512 Byte Ausgabe pro Hauptloop
- kein Warten auf freien TCP-Puffer
- Stall-Erkennung und sauberer Socketabschluss wie bei Haupt-, Setup-, Download- und Kalibrierscheinseite
- keine zusätzliche große RAM-Kopie; der bereits vorhandene RAM2-Puffer `ethJsonBuf` bleibt bis zum Transferende unverändert

Während `ethPageActive` gesetzt ist, werden weitere Browserrequests zwar eingelesen, aber nicht beantwortet. Dadurch kann kein anderer Handler den dynamischen Seitenpuffer überschreiben.

## Cache-Verhalten

Dynamische Seiten übergeben bewusst keinen ETag:

- `Cache-Control: no-store, no-cache, must-revalidate, max-age=0`
- keine `304 Not Modified`-Antwort
- aktueller Zertifikats-/Kalibrierstatus wird bei jedem Aufruf neu erzeugt

Statische Seiten behalten ihre bisherige ETag-Unterstützung.

## Unverändert

Nicht verändert wurden:

- Zertifikats- und Kalibrierformate
- kanonische Signaturbytes und ECDSA-P-256-Prüfung
- TP3C1 und TP3Q3
- TPSIG und TPLOG
- externe PDF-Bindung
- SD-Kalibrierarchiv und Kopfbindung
- ADC, Messwerterfassung, Regelung und Safety

Die neuen Funktionen liegen in `FLASHMEM`. Es wurde kein neuer großer RAM1- oder RAM2-Puffer angelegt.
