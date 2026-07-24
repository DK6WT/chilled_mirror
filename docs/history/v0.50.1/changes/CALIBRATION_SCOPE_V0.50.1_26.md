# TP-3000 Build 0.50.1_26 – Kalibrierumfang

Die Systemkalibrierung unterscheidet vier sichtbare Fälle:

- `Nur As Found`: eine As-Found-Messreihe.
- `Nur As Left`: eine As-Left-Messreihe.
- `As Found / As Left – ohne Justierung`: eine gemeinsame Messreihe, die zugleich Eingangs- und Ausgangszustand beschreibt.
- `Vor und nach Justierung`: zwei getrennte Messreihen vor und nach der Justierung.

Signierte Maschinenwerte:

- `AS_FOUND`
- `AS_LEFT`
- `AS_FOUND_AS_LEFT_NO_ADJUSTMENT`
- `BEFORE_AFTER_ADJUSTMENT`

Das frühere Token `AS_FOUND_AS_LEFT` bleibt aus Kompatibilitätsgründen gültig und wird als getrennte Vor-/Nach-Justierung interpretiert.

Im kompakten TPC1-Messpunktcontainer gelten zusätzlich:

- Scope-Byte 4 / Stage-Byte 3: gemeinsame As-Found-/As-Left-Messreihe ohne Justierung.
- Scope-Byte 5 / Stage-Bytes 1 und 2: getrennte Messreihen vor und nach Justierung.
