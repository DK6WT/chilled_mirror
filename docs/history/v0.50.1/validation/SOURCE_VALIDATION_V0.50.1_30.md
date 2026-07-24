# Source Validation – Firmware 0.50.1 / Build 0.50.1_30

## Grundlage

- exakt frisch entpackt: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_7.zip`
- verbindliche TP3C1-Referenz: `TP3C1_Compact_Prototype_V0.2.zip`
- die Basis trägt intern bereits Build `0.50.1_29`
- neuer Build gemäß Vorgabe: `0.50.1_30`

## Bytegenauer Referenzvergleich

Der aus `TPethernet.ino` extrahierte produktive Browserencoder wurde mit den
G00001/K30001-Referenzdaten ausgeführt. Folgende Dateien sind bytegleich zur
Python-Referenz:

| Artefakt | Größe | SHA-256 | Ergebnis |
|---|---:|---|---|
| Envelope roh | 1176 Byte | `56d5906d997004528fe1b312b1bbc9c8d675664ce0229868571d10b83827801b` | bytegleich |
| Envelope ZLIB | 1126 Byte | `32444aba20ecb36f5d550be7692f181578bb17620b1c3ba434e61afcefcc92a1` | bytegleich |
| Binärteil 1 | 581 Byte | `4435b2ce8ff0c86c67014de12f049fe6884632afc67c9d160a6d5a5fc60f2383` | bytegleich |
| Binärteil 2 | 581 Byte | `9f6d39ddd6de1f85e1700969c4fae3f1b9f63b351de9a5fc4868924c7b9bb7ff` | bytegleich |
| QR-PNG Teil 1 | 1050 × 1050 px | `3e47528aeaf322f6322a2280b6581c6c29e5ee2fb17999f89f5d3024efbf00db` | bytegleich |
| QR-PNG Teil 2 | 1050 × 1050 px | `21b5599165db93cd10f187da1b5408434467b34da5a49c5f0d102cf1e6ed40c5` | bytegleich |

Zusätzlich geprüft:

- ZLIB-Fragmente: 563 + 563 Byte
- Transportkopf: 18 Byte je Teil
- rohe TP3C1-Texte: je 891 Zeichen
- Deep Links: je 907 Zeichen
- QR-Fehlerkorrektur: M
- QR-Version: 20
- Symbolgröße: 97 × 97 Module
- Ruhezone: 4 Module

## Syntax- und Integrationsprüfungen

- die fünf eingebetteten Seitenfragmente wurden wieder zu vollständigem HTML zusammengesetzt
- QR-Bibliothek, pako, TP3C1-Encoder und gesamtes Seiten-JavaScript bestanden `node --check`
- produktiver Encoder und produktive pako-Fassung wurden direkt aus `TPethernet.ino` extrahiert; getestet wurde keine separate Ersatzimplementierung
- TP3C1 verwendet zwei QR-Segmente: Byte-Modus für `tp3000://verify#` und Alphanumerikmodus für `TP3C1:<Base38>`
- das vollständige TP3Q3 bleibt für den elektronischen `.tpqr`-Download aktiv

## Unveränderte Mess- und Safety-Kette

Folgende Kerndateien sind gegenüber der Basis bytegleich:

- `ads1263.ino`
- `Taupunkt_Regelung.ino`
- `TPsafety.ino`
- `MCP3202_Treiber.ino`
- `Ref100_120_Calibration.ino`
- `PT100_2P_Calibration.ino`
- `TPopticHealth.ino`
- `TPsdLog.ino`
- `drv8873s.cpp` / `drv8873s.h`
- `TP-3000.ino`
- `TPdisplay.ino`

ADC, Regelung, Safety, Messwerterfassung und bestehende Kalibrierwerte wurden
nicht verändert.

## Speicher- und Bibliotheksbewertung

- pako Deflate, minimiert: etwa 27,5 kB als `PROGMEM`-Seitensegment
- TP3C1-Encoder: etwa 11,3 kB als `PROGMEM`-Seitensegment
- die TLV-/ZLIB-Arbeit erfolgt im Browser des Bediengeräts, nicht im Teensy-RAM
- auf der Firmwareseite wächst hauptsächlich der Flash-/Programmtext der Webressourcen
- die bestehende sequentielle Ethernet-Seitenausgabe wurde um zwei zusätzliche Segmente erweitert; es wird keine vollständige HTML-Seite im RAM zusammengesetzt

## Buildstatus

In der Ausführungsumgebung waren weder Arduino CLI noch Teensyduino vorhanden.
Daher konnte kein vollständiger Teensy-Releasebuild erzeugt werden. Die
Quell-, JavaScript-, Referenz-, Hash- und Unverändertheitsprüfungen wurden
vollständig durchgeführt. Der abschließende Hardware-/Teensyduino-Build ist
auf dem vorgesehenen Windows-Entwicklungsrechner auszuführen.
