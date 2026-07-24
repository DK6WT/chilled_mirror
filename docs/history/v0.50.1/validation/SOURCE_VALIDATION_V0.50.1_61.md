# Source Validation V0.50.1_61

Geprüft wurde der frisch aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_36.zip` entpackte und ausschließlich für Build `0.50.1_61` geänderte Quellstand.

## Statische Prüfungen

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_61`.
- TFT-3-Werte-Hauptwerte: DroidSansMono 48, x=260, y=93/177/261.
- TFT-3-Werte-Beschriftungen: DroidSansMono 16, y=125/209/293, Doppelpunkte x=240.
- TFT-2-Werte-Hauptwerte: DroidSansMono 60, x=260, y=110/215; TextBox 10×2 Zeichen.
- TFT-2-Werte-Beschriftungen: DroidSansMono 16, y=142/247, Doppelpunkte x=240.
- TFT-2-Werte-Rahmen und Touchflaeche: x=12..787, y=88..297.
- TFT-3-Werte-/Chart-Rahmen unveraendert: x=12..787, y=50..335.
- Beim Layoutwechsel wird die Hauptwert-TextBox neu initialisiert, geleert und ihr Sichtcache verworfen.
- Die geänderte `TPdisplay.ino` besitzt nach Entfernen von Kommentaren und Stringliteralen vollständig ausgeglichene Klammern.

## Webprüfung

`TPethernet.ino` ist gegenüber Build 60 bytegleich. Die bereits freigegebene vertikale Webanordnung und ihre Doppelpunktspalten wurden nicht verändert.

## Abgrenzung

Geändert wurden nur TFT-Hauptscreen-Darstellung, Build-ID und zugehörige Dokumentation. Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle wurden nicht verändert.

## Noch offen

- Vollständiger Teensyduino-Build und reale Memory Usage.
- Optische Prüfung auf dem echten 800×480-TFT.
- Prüfung mit positiven und negativen Taupunkten sowie mehrfachem Layoutwechsel.
