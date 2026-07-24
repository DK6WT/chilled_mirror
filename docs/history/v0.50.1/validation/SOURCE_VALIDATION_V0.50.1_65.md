# Source Validation V0.50.1_65

Geprüft wurde der frisch aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_40.zip` entpackte und ausschließlich für Build `0.50.1_65` geänderte Quellstand.

## Statische Prüfungen

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_65`.
- Deutsches TFT-Spracharray: `2 Werte` und `3 Werte`.
- Englisches TFT-Spracharray: `2 values` und `3 values`.
- Web-Auswahlliste `screenLayout`: Wert 0 `2 Werte|2 values`, Wert 1 `3 Werte|3 values`.
- Die internen IDs und Werte `MAIN_SCREEN_LAYOUT_STANDARD = 0` sowie `MAIN_SCREEN_LAYOUT_3VALUES = 1` sind unverändert.
- Hauptscreen-Geometrie in `TPdisplay.ino` ist gegenüber Build 64 bytegleich.
- Das eingebettete Web-Setup-JavaScript wurde aus dem C++-Raw-String extrahiert und mit `node --check` erfolgreich geprüft. Die übrigen Quelländerungen sind reine Stringersetzungen beziehungsweise die zentrale Build-ID.

## Abgrenzung

Geändert wurden nur sichtbare Setup-Texte, Build-ID und Dokumentation. Gespeicherte Konfiguration, Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle wurden nicht verändert.

## Noch offen

Vollständiger Teensyduino-Build sowie optische Prüfung auf TFT und Web.
