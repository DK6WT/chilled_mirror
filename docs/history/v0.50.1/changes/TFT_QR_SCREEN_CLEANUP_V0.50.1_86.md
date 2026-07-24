# TFT-QR-Bildschirmbereinigung – V0.50.1_86

## Fehlerbild

Auf der direkten TFT-QR-Seite konnten lange beziehungsweise UTF-8-unkorrekt ausgegebene Hilfstexte bis an den rechten Rand laufen. Nach ENTER blieben außerdem Teile der direkt gezeichneten QR-Matrix und der Seiteninformationen sichtbar, während das Hauptmenü und der FAN-Button bereits darüber gezeichnet wurden.

## Ursache

Die QR-Seite zeichnet Matrix und rechte Infospalte direkt auf den realen RA8875-TFT und nicht ausschließlich in die VirtLCD-Puffer. Der bisherige ENTER-Pfad wechselte nur `menu_level`, ohne diese direkten Pixel und die Button-/Seiten-Caches vollständig zu löschen.

## Korrektur

- rechte Infospalte mit festen 11-/14-Pixel-Schriften und kurzen Zeilen
- Umlaute über `tpTftPrintUtf8()`
- `zurück zum` und `Menü` auf zwei Zeilen
- zentraler `tpTftQrLeaveToMenu()`-Pfad
- vollständiges Löschen des realen TFT
- Invalidierung beider VirtLCD-Puffer, des Seiten-Caches und der Buttonzustände
- Rücksetzen der Touchkoordinaten vor dem Hauptmenü

QR-Inhalt und kryptografische Daten sind unverändert.
