# Source Validation V0.50.1_64

Geprüft wurde der frisch aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_39.zip` entpackte und ausschließlich für Build `0.50.1_64` geänderte Quellstand.

## Statische Prüfungen

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_64`.
- TFT-2-Werte-Hauptwerte: DroidSansMono 60, x=242, y=110/215; TextBox 10×2 Zeichen.
- TFT-2-Werte-Beschriftungen: DroidSansMono 16, y=142/247, Doppelpunkte x=222.
- Gegenüber Build 63 wurden Hauptwerte und Beschriftungen gemeinsam exakt 5 px nach links verschoben.
- TFT-3-Werte-Geometrie ist unverändert: Hauptwerte x=272, y=85/169/253; Doppelpunkte x=252; Beschriftungen y=117/201/285.
- Gemeinsamer TFT-Rahmen und Touchfläche bleiben unverändert bei x=12, y=50, Breite 776, Höhe 286.
- Webcode ist gegenüber Build 63 unverändert.
- Die geänderte `TPdisplay.ino` besitzt nach Entfernen von Kommentaren und Stringliteralen vollständig ausgeglichene Klammern.

## Abgrenzung

Geändert wurden nur die horizontale Position des zweizeiligen TFT-Hauptscreens, die Build-ID und die zugehörige Dokumentation. Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle wurden nicht verändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und optische Prüfung auf dem Ziel-TFT.
