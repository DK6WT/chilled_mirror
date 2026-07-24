# Attribution Audit

Aktuelles Firmwarepaket: 0.50.1 / Build 0.50.1_54, 2026-07-18.

Neu aufgenommen wurde pako 1.0.11 für die bytegenaue browserseitige
ZLIB-Kompression des TP3C1-Envelope. Die minimierte Deflate-Komponente ist in
`TPethernet.ino` eingebettet. Lizenz und Herkunft sind unter
`LICENSES/PAKO-NOTICE.txt` und `THIRD_PARTY_NOTICES.md` dokumentiert.

Der TP3C1-TLV-Encoder, Base38, Transportkopf, CRC-32, Aufteilungslogik und die
Firmwareintegration sind projektspezifischer GPL-3.0-only-Code. Die
verbindlichen Referenzdateien liegen unter `tools/tp3c1_reference_v02/`.

micro-ecc und die übrigen bereits dokumentierten Drittbestandteile bleiben
unverändert.


V0.50.1_54 korrigiert ausschließlich die interne HTTP-Body-Erfassung für den bereits vorhandenen `.tpfwcert`-Webupload. Es wurden keine neuen Drittbibliotheken oder Binärartefakte aufgenommen.
