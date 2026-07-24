# Source Validation V0.50.1_63

Geprüft wurde der frisch aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_38.zip` entpackte und ausschließlich für Build `0.50.1_63` geänderte Quellstand.

## Statische Prüfungen

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_63`.
- TFT-3-Werte-Hauptwerte: DroidSansMono 48, x=272, y=85/169/253.
- TFT-3-Werte-Beschriftungen: DroidSansMono 16, y=117/201/285, Doppelpunkte x=252.
- TFT-2-Werte-Hauptwerte: DroidSansMono 60, x=247, y=110/215; TextBox 10×2 Zeichen.
- TFT-2-Werte-Beschriftungen: DroidSansMono 16, y=142/247, Doppelpunkte x=227.
- TFT-Rahmen und Touchfläche sind in 2-Werte-, 3-Werte- und Chartansicht identisch: x=12, y=50, Breite 776, Höhe 286.
- Beim Layoutwechsel wird die Hauptwert-TextBox neu initialisiert, geleert und ihr Sichtcache verworfen.
- Die geänderte `TPdisplay.ino` besitzt nach Entfernen von Kommentaren und Stringliteralen vollständig ausgeglichene Klammern.

## Webprüfung

- Der Standardrahmen in CSS und die Laufzeitgeometrie verwenden x=12, y=50, Breite 776, Höhe 286 und Radius 10.
- `applyMainLayout()` setzt denselben Rahmen unabhängig vom 2- oder 3-Werte-Layout.
- Die 2-Werte-Textgeometrie `.big` blieb unverändert.
- Die 3-Werte-Textgeometrie `.big3` blieb unverändert.
- Beschriftungen bleiben rechtsbündig und die Doppelpunkte stehen vertikal untereinander.
- Ein lokaler Chromium-DOM-Test bestätigte für beide Zahlenlayouts denselben Rahmen `12 / 50 / 776 / 286`; die 2-Werte-Schrift blieb bei `left=115`, `top=110`, `font-size=60`, die 3-Werte-Schrift bei `left=20`, `top=76`, `font-size=56`.

## Abgrenzung

Geändert wurden nur Hauptscreen-Darstellung, Build-ID und zugehörige Dokumentation. Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle wurden nicht verändert.

## Noch offen

- Vollständiger Teensyduino-Build und reale Memory Usage.
- Optische Prüfung auf dem echten 800×480-TFT.
- Prüfung mit positiven und negativen Taupunkten sowie mehrfachem Layoutwechsel.
