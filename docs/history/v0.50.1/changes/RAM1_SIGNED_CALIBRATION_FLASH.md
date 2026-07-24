# V0.50.1_15 – RAM1-Optimierung signierte Kalibrierung

## Ausgangspunkt

Lokaler Teensyduino-Linkerbericht von V0.50.1_14:

```text
FLASH: code:606544, data:1079124, headers:8508   free for files:6432288
RAM1:  variables:148132, code:265576, padding:29336   free for local variables:81244
RAM2:  variables:328064  free for malloc/new:196224
```

Gegenüber dem zuvor optimierten Identitätsstand lag der Mehrverbrauch vor allem in statischen Texten der neuen Kalibrierfunktion.

## Änderungen

- 130 eindeutige Texte aus `TPsignedCalibration.cpp` wurden in einer konstanten `.progmem`-Tabelle zusammengeführt.
- Darunter liegen JSON-Feldnamen, Fehlertexte, Statusmeldungen sowie die vollständigen Geräte- und Kopf-Anfragevorlagen.
- 42 eindeutige Texte der Web-Kalibrierseite wurden in `PROGMEM` zusammengeführt.
- Die fünf Kalibrier-Webpfade und die Readiness-Texte aus `TPsignedData.cpp` liegen ebenfalls im QSPI-Flash.
- `DMAMEM`-Puffer und damit RAM2 bleiben unverändert.
- Der feste Zeitparser bleibt unverändert; kein `sscanf()`, `strftime()` oder `mktime()`.

## Erwartete Wirkung

Die explizit verschobenen Textnutzdaten umfassen rund 10,8 kB inklusive Nullterminatoren. Linker-Ausrichtung, String-Zusammenfassung und kleine Codeänderungen können den tatsächlich gemeldeten Wert leicht verändern. Maßgeblich ist deshalb der lokale Teensyduino-Linkerbericht.

Erwartungsbereich für `RAM1 variables`: ungefähr 137 kB bis 138 kB. Ein fester Bytewert wird vor dem echten Teensyduino-Linklauf nicht behauptet.

## Vorprüfung

- `TPsignedCalibration.cpp`: `clang++ -fsyntax-only` gegen Arduino-/SD-/TimeLib-Stubs bestanden.
- `TPsignedData.cpp`: `clang++ -fsyntax-only` bestanden.
- Extrahierter Kalibrier-Webblock aus `TPethernet.ino`: `clang++ -fsyntax-only` bestanden.
- Keine funktionale Änderung an Signaturformaten, EEPROM, Gerätedaten, Kalibrierwerten oder Importablauf.
