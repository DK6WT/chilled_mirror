# Systemkalibrierungs-Fachangaben und QR-Druck – Build 0.50.1_32

## Umfang

- Quelle der Fachfelder ist ausschließlich `system.Approval` der aktiven `.tpscal`.
- Geräte- und Kopfjustierung werden nicht nach Fachtexten durchsucht.
- Vier kompakte Felder werden zweispaltig gedruckt.
- `Kalibrierverfahren` und `Akkreditierungsangaben` werden über die volle Breite gedruckt.
- Leere Felder entfallen einzeln. Sind alle sechs leer, erscheint der bisherige Hinweistext.
- Die Werte werden mit HTML-Escaping, `white-space: pre-wrap` und dynamischer Höhe dargestellt.

## TP3C1-Druck

- Beide QR-Symbole bleiben Version 20 / 97 × 97 Module / EC M.
- Nur die physische CSS-Druckbreite steigt von 98 mm auf 115 mm.
- Die Ruhezone des erzeugten Symbols bleibt unverändert.
- Die QR-Seite beginnt weiterhin mit einem erzwungenen Seitenumbruch.
- Eine A4-Testausgabe mit den vollständigen Überschriften, Beschriftungen und dem Prüftext umfasst genau eine Seite.

## Nicht geändert

TP3C1-TLV, ZLIB, Transportkopf, CRC-32, Base38, Deep Links, TP3Q3, Zertifikate und alle fünf Signaturen wurden nicht verändert.
