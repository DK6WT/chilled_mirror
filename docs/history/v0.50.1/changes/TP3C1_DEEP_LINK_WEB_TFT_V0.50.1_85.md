# TP3C1 Deep Link in Web und TFT – V0.50.1_85

## Ziel

Die beiden TP3C1-V0.4-Teile des aktiven Kalibrierscheins liefern auf allen
Ausgabemedien denselben kanonischen Scaninhalt:

```text
tp3000://verify#TP3C1:<Base38>
```

Der TP3C1-Nutzdatensatz, Schema 1.2, die Zweiteilung, Fragmentgrenzen,
CRC-32, ZLIB-Auswahl und Signaturen bleiben unverändert.

## QR-Segmentierung

Der Deep Link wird in zwei QR-Segmente aufgeteilt:

1. Byte-Modus: `tp3000://verify#`
2. Alphanumerischer Modus: `TP3C1:<Base38>`

Dadurch bleiben die Kleinbuchstaben und das Zeichen `#` des URI-Präfixes
exakt erhalten, während der große Base38-Anteil weiterhin platzsparend im
QR-Alphanumerikmodus codiert wird.

## TFT

Der native QR-Generator unterstützt ab diesem Stand zusätzlich die kombinierte
Byte-/Alphanumeriksegmentierung. Die bisher intern erzeugten Rohteile
`TP3C1:<Base38>` werden nicht verändert; erst beim QR-Encoding wird der
Deep-Link-Präfix als separates Byte-Segment vorangestellt.

## Web und Druck

Der im Web eingebettete QR-Generator erhält die Segmentmodi ausdrücklich.
Damit nutzen Webvorschau und Ausdruck dieselbe Segmentfolge wie das TFT.

## Unverändert

- TP3C1 V0.4
- Schema 1.2
- genau zwei QR-Teile
- Fehlerkorrektur M
- automatische Maskenwahl
- Kalibrier-, Zertifikats- und Signaturdaten
- TP3Q3-Dateiexport
- Messung, Regelung, Safety, Ethernet und LED-Autoadaption
