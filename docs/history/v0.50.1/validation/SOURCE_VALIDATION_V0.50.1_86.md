# Source Validation V0.50.1_86

Geprüft wurden:

- Build-ID `0.50.1_86`
- neuer zentraler ENTER-Cleanup `tpTftQrLeaveToMenu()`
- vollständiges TFT-Löschen und Cache-Invalidierung
- kompakte rechte Infospalte ohne überlange sichtbare Zeilen
- UTF-8-Ausgabe für `zurück zum Menü`
- TP3C1-Deep-Link und QR-Encoder gegenüber V0.50.1_85 unverändert
- Projekt-Libraries unverändert
- ZIP-CRC und Frischentpackvergleich

Statischer Test: `tools/test_v86_tft_qr_screen_cleanup.py`.

Ein vollständiger Teensyduino-Build ist in der Übergabeumgebung nicht verfügbar und muss auf dem Entwicklungsrechner erfolgen.
