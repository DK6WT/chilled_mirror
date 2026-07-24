# Source Validation V0.50.1_81

## Basis

- direkte Basis: V0.50.1_80
- interne Version: `0.50.1`
- Build-ID: `0.50.1_81`
- Zielhardware: Teensy 4.1

## Quelländerung

Nur `TPledAdaptation.cpp` wurde funktional geändert:

- `DMAMEM` bei `ledAdaptModel` entfernt
- `DMAMEM` bei `ledAdaptScratchModel` entfernt
- `DMAMEM` bei `ledSystemGroundCurve` entfernt
- `DMAMEM` bei `ledSystemGroundFileBuffer` entfernt

Die vier Objekte bleiben statische globale Puffer. Es entstehen keine großen lokalen Stackobjekte und keine zusätzlichen dynamischen Allokationen.

`TPsignedData.h` enthält ausschließlich die neue Build-ID `0.50.1_81`; Version und signierte Formate bleiben unverändert.

## Größenprüfung aus der V0.50.1_80-ELF

- `ledSystemGroundFileBuffer`: `0x2000` = 8.192 Byte
- `ledSystemGroundCurve`: `0x0508` = 1.288 Byte
- `ledAdaptModel`: `0x0CC0` = 3.264 Byte
- `ledAdaptScratchModel`: `0x0CC0` = 3.264 Byte
- Summe: 16.008 Byte

## Statische Prüfungen

- keine der vier Deklarationen enthält weiterhin `DMAMEM`
- keine Änderung an Strukturgrößen, Modellversion, CRC-Pfad oder Dateiformat
- keine Änderung an Ethernet-, DHCP- oder Webserverquellen
- keine Änderung im Ordner `libraries/`
- ZIP-Inhalt und CRC nach Erstellung prüfen

## Noch erforderlich

- vollständiger Teensyduino-Build
- realer RAM1-/RAM2-Linkerbericht
- Hardwaretest von DHCP, statischer IP, Port 80 und Ethernet-Aus/Ein
- Regressionstest der LED-Autoadaption und TFT-QR-Anzeige
