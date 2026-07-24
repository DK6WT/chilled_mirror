# Source Validation V0.50.1_62

Geprüft wurde der aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_37.zip` übernommene und ausschließlich für Build `0.50.1_62` geänderte Quellstand.

## Statische Prüfungen

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_62`.
- TFT-3-Werte-Hauptwerte: DroidSansMono 48, x=272, y=88/172/256.
- TFT-3-Werte-Beschriftungen: DroidSansMono 16, y=120/204/288, Doppelpunkte x=252.
- TFT-2-Werte-Hauptwerte: DroidSansMono 60, x=252, y=110/215; TextBox 10×2 Zeichen.
- TFT-2-Werte-Beschriftungen: DroidSansMono 16, y=142/247, Doppelpunkte x=232.
- TFT-2-Werte-Rahmen und Touchfläche bleiben x=12..787, y=88..297.
- Beim Layoutwechsel wird die Hauptwert-TextBox neu initialisiert, geleert und ihr Sichtcache verworfen.
- Die geänderte `TPdisplay.ino` besitzt nach Entfernen von Kommentaren und Stringliteralen vollständig ausgeglichene Klammern.

## Webprüfung

- Die vertikale Geometrie blieb unverändert.
- Im 2-Werte-Layout liegt die Mitte der Hauptwertspalte rechnerisch bei x=400: 115 + 132 + 13 + 280/2.
- Im 3-Werte-Layout liegt die Mitte der Hauptwertspalte rechnerisch bei x=400: 20 + 192 + 13 + 350/2.
- Ein lokaler Chromium-Layouttest bestätigte für alle drei Hauptwerte eine Spaltenmitte von 399,99 px.
- Beschriftungen bleiben rechtsbündig und die Doppelpunkte stehen vertikal untereinander.

## Abgrenzung

Geändert wurden nur Hauptscreen-Darstellung, Build-ID und zugehörige Dokumentation. Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle wurden nicht verändert.

## Noch offen

- Vollständiger Teensyduino-Build und reale Memory Usage.
- Optische Prüfung auf dem echten 800×480-TFT.
- Prüfung mit positiven und negativen Taupunkten sowie mehrfachem Layoutwechsel.
