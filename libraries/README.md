# Mitgelieferte lokale Build-Bibliotheken

Dieser Ordner enthält Bibliotheksstände, die direkt aus dem TP-3000-Projektordner
geladen werden.

| Ordner | Version | Ursprung | Lizenz |
|---|---:|---|---|
| `ProtoCentral_ADS1262_32-bit_precision_ADC_Library` | 2.0.0 | https://github.com/Protocentral/ProtoCentral_ads1262 | MIT |
| `RTC_RV3129_Arduino_Library` | 1.0.0 | https://github.com/OUIDEAS/SparkFun_RV-3129_Arduino_Library | MIT |
| `SparkFun_BMP581_Arduino_Library` | 1.0.1 | https://github.com/sparkfun/SparkFun_BMP581_Arduino_Library | MIT |
| `WDT_T4` | 0.1 | https://github.com/tonton81/WDT_T4 | MIT |

Die SparkFun-Bibliothek enthält unter `src/bmp5_api/` zusätzlich die Bosch BMP5
Sensor API. Diese Unterkomponente ist Copyright Bosch Sensortec GmbH und steht
unter BSD-3-Clause. Der Lizenztext liegt in
`SparkFun_BMP581_Arduino_Library/src/bmp5_api/LICENSE`.

Die WDT_T4-Kopie enthält ihre ursprüngliche MIT-Datei unter
`WDT_T4/LICENSE`. TP-3000 bindet bevorzugt
`libraries/WDT_T4/Watchdog_t4.h` ein und kann ersatzweise eine global
installierte Version verwenden.

Bei den Sensorbibliotheken und bei WDT_T4 wurden nicht benötigte Beispiele,
Hardware-/CAD-Daten sowie CI-/GitHub-Metadaten entfernt. Die benötigten Quellen,
Metadaten, README- und Lizenzdateien bleiben erhalten.

## RA8875-Displaybibliothek

RA8875 wird **nicht** aus diesem Ordner geladen. Verwendet wird RA8875 0.7.11
aus der installierten Teensyduino-Umgebung. Für das aktuelle Boardpaket 1.62.0
lautet der erwartete Pfad:

```text
Arduino15/packages/teensy/hardware/avr/1.62.0/libraries/RA8875
```

Autor: Max MC Costa / sumotoy. Lizenz: GPL-3.0-or-later. Die Bibliothek wird im
TP-3000-Paket nicht doppelt verteilt. Siehe
`../LICENSES/RA8875-NOTICE.txt` und `../THIRD_PARTY_NOTICES.md`.

Weitere Kernbibliotheken wie NativeEthernet, FNET, Time, Metro, SD, SdFat, SPI,
Wire und EEPROM kommen ebenfalls aus der installierten Teensyduino-/Arduino-
Umgebung und behalten ihre eigenen Urheber- und Lizenzbedingungen.
