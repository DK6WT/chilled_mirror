# Source Validation V0.50.1_85

## Geprüfte Änderung

- Build-ID `0.50.1_85`
- kanonischer Deep Link `tp3000://verify#TP3C1:<Base38>`
- Byte-Segment für `tp3000://verify#`
- Alphanumeriksegment für `TP3C1:<Base38>`
- identischer Segmentaufbau in Web/Druck und TFT
- unveränderte TP3C1-Rohteile und Zweiteilung

## Statische Prüfung

```text
python tools/test_v85_tp3c1_deep_link.py
```

## Native QR-Prüfung

`TPqrCode.cpp` wurde zusätzlich auf dem Host kompiliert. Ein erzeugter
Test-QR mit 500 Zeichen TP3C1-Nutzdaten wurde mit OpenCV wieder eingelesen.
Der dekodierte Inhalt begann exakt mit:

```text
tp3000://verify#TP3C1:
```

Die Kapazitätsgrenze für die kombinierte Segmentierung wurde geprüft:
3364 alphanumerische Zeichen passen in QR Version 40-M, 3365 werden mit
einer eindeutigen Fehlermeldung abgelehnt.

Zusätzlich wurde derselbe 500-Zeichen-Testinhalt mit dem eingebetteten
Web-QR-Generator und dem nativen TFT-QR-Generator aufgebaut. QR-Version,
Modulzahl und sämtliche Matrixbits waren identisch.

## Nicht durchgeführt

Ein vollständiger Teensyduino-Build und der Hardwaretest auf dem TP-3000
müssen in der vorgesehenen Arduino-/Teensyduino-Umgebung erfolgen.
