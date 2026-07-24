# TP-3000 Hauptscreen-Layout V0.50.1_60

## Ziel

Web und TFT werden als getrennte Layouts behandelt. Die Web-Anordnung war vertikal bereits passend; am TFT musste der 3-Werte-Block nach oben und kleiner gesetzt werden.

## Web

- Beschriftung: `Rel. Feuchte`, `Taupunkt`, im 3-Werte-Modus `T-Umgebung`.
- Schriftgröße der Beschriftung: 16 px.
- Beschriftungen rechtsbündig; Doppelpunkte in einer gemeinsamen Spalte.
- Vertikale Positionen und Hauptwertgrößen bleiben wie im bisherigen Webstand.

## TFT, 3 Werte

- Hauptwertschrift: DroidSansMono 48.
- TextBox: x=260, y=68, 16 Spalten, 3 Zeilen.
- Zeilenursprünge: 68 / 152 / 236 px; Abstand 84 px.
- Beschriftung: DroidSansMono 16, Doppelpunkte bei x=240.
- Beschriftungs-y: 100 / 184 / 268 px, optisch an der Unterkante der großen Schrift ausgerichtet.

## TFT, 2 Werte

- Hauptwertschrift und Hauptwertposition unverändert: DroidSansMono 60, x=90, y=110.
- Beschriftung: DroidSansMono 16.
- Doppelpunkte bei x=300.
- Beschriftungs-y: 91 / 196 px; damit liegen sie an der oberen Kante der großen Zeilen, ohne den bisherigen Wertblock zu verschieben.

## Abgrenzung

Keine Änderung an Messwertbildung, Filterung, Regelung, Safety, Kalibrierwerten, Signaturen, Logging oder Kommunikationsprotokollen.
