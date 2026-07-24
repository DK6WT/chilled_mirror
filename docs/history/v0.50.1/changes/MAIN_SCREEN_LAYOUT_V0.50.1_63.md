# TP-3000 Hauptscreen-Feinlage V0.50.1_63

## Ziel

Nach dem Gerätetest werden die letzten Pixelkorrekturen umgesetzt und die Rahmengeometrie der Zahlenansichten vereinheitlicht. Messwertbildung und alle metrologischen Funktionen bleiben unverändert.

## TFT, 3 Werte

- Hauptwertschrift: DroidSansMono 48.
- TextBox: x=272, y=85, 16 Spalten, 3 Zeilen.
- Zeilenursprünge: 85 / 169 / 253 px; Abstand weiterhin 84 px.
- Gegenüber Build 62 liegt der komplette Dreierblock weitere 3 px höher.
- Beschriftung: DroidSansMono 16, Doppelpunkte bei x=252.
- Beschriftungs-y: 117 / 201 / 285 px.

## TFT, 2 Werte

- Hauptwertschrift: DroidSansMono 60, unverändert.
- TextBox: x=247, y=110, 10 Spalten, 2 Zeilen.
- Zeilenursprünge: 110 / 215 px, unverändert.
- Gegenüber Build 62 liegen beide vollständigen Zeilenblöcke weitere 5 px links.
- Beschriftung: DroidSansMono 16, Doppelpunkte bei x=227.
- Beschriftungs-y: 142 / 247 px, unverändert.

## Gemeinsamer TFT-Rahmen

- 2-Werte-, 3-Werte- und Chartansicht: x=12, y=50, Breite 776, Höhe 286, Radius 10.
- Alle drei Ansichten verwenden dieselbe Touchfläche.
- Die Schriftpositionen des 2-Werte-Screens werden durch die Rahmenänderung nicht vertikal verändert.

## Web

- 2-Werte-, 3-Werte- und Chartansicht verwenden denselben Rahmen: x=12, y=50, Breite 776, Höhe 286, Radius 10.
- Die 2-Werte-Schrift behält ihre bisherige Position und Größe.
- Die Hauptwertspalte bleibt im 2- und 3-Werte-Layout auf x=400 zentriert.
- Beschriftungen bleiben rechtsbündig; die Doppelpunkte bleiben je Ansicht exakt untereinander.

## Abgrenzung

Keine Änderung an Messung, Filterung, Regelung, Safety, Kalibrierwerten, Signaturen, Logging, QR-Inhalten oder Kommunikationsprotokollen.
