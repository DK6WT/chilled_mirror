# TP-3000 V0.50.1_78 – Kalibrierschein-QR am TFT

## Ziel

Der aktuell wirksame Kalibrierschein soll ohne Browser direkt am Gerät abrufbar sein. Ein Mobiltelefon kann die beiden angezeigten TP3C1-Codes mit der TP-3000-Verifier-App scannen, kryptografisch prüfen, als Kalibrierschein darstellen und drucken beziehungsweise als PDF ausgeben.

## Menü und Bedienung

Im obersten Setup-Menü wurde der Eintrag `Kalibrierschein anzeigen` ergänzt. Die Anzeigeseite verwendet die vorhandenen vier TFT-Tasten:

- `UP` und `DOWN` wechseln zyklisch zwischen `CODE 1/2` und `CODE 2/2`.
- `ENTER` kehrt zum obersten Setup-Menü zurück.
- `EXIT` verwendet den zentralen Setup-Exitpfad und führt zum Hauptscreen.

Die Teilnummer steht in Droid Sans Mono 16 direkt oberhalb der EXIT-Taste. Die übrigen Hinweise verwenden 12 beziehungsweise 16 Pixel Schriftgröße.

## Datenquelle und Vertrauensgrenze

Die Firmware liest die aktiven Dateien `ACTIVE_DEVICE.tpdcal`, `ACTIVE_HEAD.tphcal` und `ACTIVE_SYSTEM.tpscal` erneut ein und verwendet die bestehenden Verifikationspfade. Nur wenn Geräteidentität sowie alle drei Kalibrierpakete gültig und gemeinsam gebunden sind, wird ein TP3C1-Nachweis aufgebaut.

Der QR-Export verändert keine Kalibrierdatei, keine Signatur und keinen aktiven Zustand. Er bildet ausschließlich bereits signierte Werte, Rollen- und Gerätebindung sowie den optionalen historischen Firmwarekontext ab.

## TP3C1-Transport

Die native Firmwareerzeugung entspricht TP3C1 V0.4:

- kanonischer Binärumschlag,
- SHA-256-Dokumentkennung,
- optionale ZLIB/DEFLATE-Kompression,
- zwei Fragmente mit Länge, CRC-32 und Bindung an denselben Dokumenthash,
- Base38-Transport mit Präfix `TP3C1:`.

Die QR-Matrix verwendet QR Model 2, alphanumerischen Modus, Fehlerkorrektur M und automatische Maskenwahl. Der reduzierte QR-Generator basiert auf Project Nayuki und ist im Third-Party-Hinweis dokumentiert.

## TFT-Geometrie

Der QR-Code bleibt unverzerrt quadratisch. Rund um die Matrix werden vier weiße Module Ruhezone dargestellt. Aus 4×4, 3×3 und 2×2 TFT-Pixeln pro Modul wird immer die größte vollständig passende Größe gewählt.

Beispiel für 97×97 Module:

```text
(97 + 4 + 4) × 4 Pixel = 420 × 420 Pixel
```

Die Matrix liegt zwischen den linken UP/DOWN/ENTER-Tasten und der rechten EXIT-Taste. Es wird keine Skalierung mit Bruchteilen, keine Glättung und keine rechteckige Verzerrung verwendet.

## Fehlerverhalten

Fehlen aktive Nachweise, stimmen Bindungen nicht überein oder überschreitet ein Teil die QR-Kapazität von Version 40-M, zeigt die Seite eine Fehlermeldung statt eines unvollständigen Codes. Die normale Messung, Regelung und Safety werden durch die reine Setup-Anzeige nicht verändert.
