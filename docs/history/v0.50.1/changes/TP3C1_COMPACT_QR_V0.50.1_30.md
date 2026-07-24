# TP3C1 V0.2 in Firmware-Build 0.50.1_30

## Ziel

Der gedruckte Systemkalibrierschein verwendet genau zwei kompakte TP3C1-Codes. Das bisherige vollständige TP3Q3 bleibt für elektronische `.tpqr`-Exporte erhalten.

## Encoder

Der Browsercode erstellt aus den aktiven JSON-Paketen bytegenau:

1. TP3C1-TLV-Envelope,
2. gemeinsame ZLIB-Kompression mit pako 1.0.11, Level 9,
3. zwei ungefähr gleich große Fragmente,
4. je einen 18-Byte-Transportkopf,
5. Base38-Rohtext und Deep Link.

Der Encoder akzeptiert in V0.2 bewusst die durch die Spezifikation festgelegte Kombination:

- Gerätezertifikat V2,
- Labor-Rollenzertifikat V2,
- Gerätejustierung V2, signiert vom Labor,
- Kopfjustierung V2, signiert vom Labor,
- Systemkalibrierung V1, signiert vom Hersteller-Root.

Unbekannte oder nicht zur V0.2-Rekonstruktion passende Formate werden mit einer klaren Fehlermeldung abgewiesen, statt einen nicht prüfbaren QR-Code zu erzeugen.

## QR-Ausgabe

- Deep-Link-Segment 1: `tp3000://verify#` im Byte-Modus
- Segment 2: `TP3C1:<Base38>` im QR-Alphanumerikmodus
- Fehlerkorrektur M
- Ruhezone 4 Module
- Druckbreite 98 mm
- genau zwei Teile, scanbar in beliebiger Reihenfolge

## Referenzvergleich G00001/K30001

| Datei | Ergebnis |
|---|---|
| Envelope roh | 1176 Byte, bytegleich |
| ZLIB | 1126 Byte, bytegleich |
| Teil 1 binär | 581 Byte, bytegleich |
| Teil 2 binär | 581 Byte, bytegleich |
| Deep Links | je 907 Zeichen, bytegleich |
| QR | je Version 20, 97 × 97 Module, EC M |

ADC, Regelung, Safety, Messwerterfassung und Kalibrierwerte wurden nicht verändert.
