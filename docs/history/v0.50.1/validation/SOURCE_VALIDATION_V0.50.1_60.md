# Source Validation V0.50.1_60

Geprüft wurde der frisch aus `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_35.zip` entpackte und ausschließlich für Build `0.50.1_60` geänderte Quellstand.

## Statische Prüfungen

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_60`.
- Der TFT-2-Werte-Screen behält seine bisherige Hauptwert-TextBox: DroidSansMono 60, x=90, y=110.
- Der TFT-3-Werte-Screen verwendet eine eigene Hauptwert-TextBox: DroidSansMono 48, x=260, y=68, drei Zeilen mit 84 px Abstand.
- Die TFT-Beschriftungen verwenden DroidSansMono 16.
- Im TFT-3-Werte-Screen stehen alle Doppelpunkte bei x=240; im TFT-2-Werte-Screen bei x=300.
- Beim Layoutwechsel wird die Hauptwert-TextBox neu initialisiert, geleert und ihr Sichtcache verworfen.
- Die geänderte `TPdisplay.ino` besitzt nach Entfernen von Kommentaren und Stringliteralen vollständig ausgeglichene Klammern.

## Webprüfung

Die im Firmwarequelltext getrennt gespeicherten Bestandteile der Hauptseite wurden in der tatsächlichen Auslieferungsreihenfolge zusammengesetzt.

- Die Elemente `rhLabel`, `dewLabel` und `taMainLabel` sind vorhanden.
- Standard- und 3-Werte-Weblayout besitzen getrennte Spaltengeometrien.
- Beschriftung und Doppelpunkt sind eigene Gridspalten; die Doppelpunkte liegen dadurch je Ansicht exakt übereinander.
- Die bestehende vertikale Webposition bleibt unverändert: Standard `top:110px`, 3-Werte-Modus `top:76px`.
- `node --check` meldete für das zusammengesetzte JavaScript keinen Syntaxfehler.

## Abgrenzung

Geändert wurden nur Hauptscreen-Darstellung, Build-ID und zugehörige Dokumentation. Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle wurden nicht verändert.

## Noch offen

- Vollständiger Teensyduino-Build und reale Memory Usage.
- Optische Prüfung auf dem echten 800×480-TFT, insbesondere die Feinlage der 16-px-Beschriftungen.
- Prüfung beider TFT-Layouts mit positiven und negativen Taupunkten sowie mehrfachem Layoutwechsel.
