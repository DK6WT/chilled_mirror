# Source Validation V0.50.1_82

## Basis

- direkte Basis: V0.50.1_81
- interne Version: `0.50.1`
- Build-ID: `0.50.1_82`
- Zielhardware: Teensy 4.1

## Geänderte Quellen

- `TPsignedCalibration.cpp`: TP3C1-TFT-Export trennt kryptografische Signaturgültigkeit von Laufzeitanwendbarkeit.
- `TPmenu_Pages.ino`: Firmwareabweichung gelb; Systemkalibrierung bleibt signiert und gültig.
- `TPethernet.ino`: Webstatus und Kalibrierschein verwenden dieselbe Trennung.
- `TPfirmwareIntegrity.cpp`: nicht passendes Firmwarezertifikat als anderes Firmwareabbild bezeichnet.
- `TPsignedData.h`: Build-ID `0.50.1_82`.

## Statische Prüfungen

- QR-Aufbau verwirft weiterhin jede Geräte-, Kopf- oder Systemdatei ohne gültige kryptografische Signatur.
- Kein verbleibender TFT-/Webtext behauptet bei reiner Firmwareabweichung, der Kalibrierschein sei möglicherweise ungültig.
- `FIRMWARE_CHANGED` und aktueller Firmware-SHA-256 bleiben Bestandteil des TP3C1-/TP3Q3-Nachweises.
- Keine Änderung im Ordner `libraries/`.
- Keine Änderung an Kalibrierformaten, kanonischen Bytes, Signaturen oder EEPROM.

## Hardwaretest

1. V0.50.1_82 über eine nach der Systemkalibrierung geänderte Firmware aufspielen.
2. `Kalibrierschein anzeigen` öffnen.
3. Code 1/2 und Code 2/2 müssen sichtbar und scanbar sein.
4. Web-Kalibrierschein muss öffnen und `signiert und gültig` zeigen.
5. Firmwareabweichung muss gelb dokumentiert sein.
6. Mit absichtlich beschädigter Kalibrierdatei muss der TFT-QR weiterhin gesperrt bleiben.
