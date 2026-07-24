# TP-3000 TFT-Hauptscreen-Layout V0.50.1_61

## Ziel

Die Webdarstellung bleibt unverändert. Nur die optische Feinlage der beiden TFT-Zahlenansichten wird nach dem Gerätetest angepasst.

## TFT, 3 Werte

- Hauptwertschrift: DroidSansMono 48.
- TextBox: x=260, y=93, 16 Spalten, 3 Zeilen.
- Zeilenursprünge: 93 / 177 / 261 px; Abstand weiterhin 84 px.
- Beschriftung: DroidSansMono 16, Doppelpunkte bei x=240.
- Beschriftungs-y: 125 / 209 / 293 px.
- Gegenüber Build 60 wurde der komplette Block ohne Änderung des Zeilenabstands um 25 px nach unten verschoben.

## TFT, 2 Werte

- Rahmen: x=12..787, y=88..297; nur horizontal verbreitert.
- Hauptwertschrift: weiterhin DroidSansMono 60.
- TextBox: x=260, y=110, 10 Spalten, 2 Zeilen.
- Zeilenursprünge: 110 / 215 px.
- Beschriftung: DroidSansMono 16, inline links neben den Hauptwerten.
- Doppelpunkte: x=240.
- Beschriftungs-y: 142 / 247 px.
- Die bisherige zusätzliche führende Leerstelle der großen Zeilen entfällt, damit Wert und Einheit vollständig bis x=759 innerhalb des Rahmens bleiben.
- Die Touchfläche folgt der neuen Rahmenbreite.

## Web

`TPethernet.ino` bleibt gegenüber Build 60 bytegleich. Vertikale Lage, Beschriftungen und Doppelpunktspalten werden nicht verändert.

## Abgrenzung

Keine Änderung an Messwertbildung, Filterung, Regelung, Safety, Kalibrierwerten, Signaturen, Logging oder Kommunikationsprotokollen.
