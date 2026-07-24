# V0.50.1_81 – RAM2-Reserve für NativeEthernet und HTTP wiederhergestellt

## Ausgangslage

V0.50.1_75 bis V0.50.1_80 besitzen gegenüber der funktionierenden Referenz V0.50.1_72 rund 10.880 Byte weniger freien RAM2-Speicher. Die Ethernet- und Webserverfunktionen selbst blieben dabei weitgehend unverändert. Das Gerät konnte noch eine DHCP- oder statische IP-Adresse anzeigen, der HTTP-Listener auf Port 80 war jedoch nicht zuverlässig erreichbar.

Die ELF-Analyse zeigte, dass NativeEthernet/FNET beim Start einen zusammenhängenden 65.536-Byte-Block aus RAM2 benötigt. V0.50.1_80 ließ statisch nur 71.232 Byte frei; nach FNET-Initialisierung verblieb damit nur eine sehr kleine Reserve für DHCP, TCP-Sockets und HTTP.

## Änderung

Vier reine LED-Autoadaptions-Datenpuffer wurden aus `DMAMEM`/RAM2 in den normalen globalen RAM1-Bereich verschoben:

- `ledSystemGroundFileBuffer`: 8.192 Byte
- `ledSystemGroundCurve`: 1.288 Byte
- `ledAdaptModel`: 3.264 Byte
- `ledAdaptScratchModel`: 3.264 Byte

Gesamtverschiebung: **16.008 Byte von RAM2 nach RAM1**.

Keiner dieser Puffer wird von DMA-Hardware verwendet. Inhalt, Dateiformat, CRC, A/B-Modellablage, 161-Klassen-Geometrie und Lernalgorithmus bleiben unverändert.

## Erwartete Speicherwirkung gegenüber V0.50.1_80

Auf Basis des vollständigen V0.50.1_80-Linkerberichts:

- RAM1-Variablen: etwa 144.004 → 160.012 Byte
- RAM1 frei: etwa 85.372 → 69.364 Byte
- RAM2-Variablen: etwa 453.056 → 437.048 Byte
- RAM2 frei für `malloc/new`: etwa 71.232 → 87.240 Byte

Der reale Wert ist mit einem vollständigen Teensyduino-Build zu bestätigen.

## Unverändert

- DHCP-Neustartlogik aus V0.50.1_80
- statische IP und Ethernet-Aus/Ein
- TFT-Kalibrierschein-QR und Bedienung
- LED-Autoadaption und System-Grundkurve
- Messwerterfassung, Regelung und Safety
- signierte Daten- und Zertifikatsformate
- EEPROM-Layout
- kompletter Ordner `libraries/`

## Hardwaretest

1. DHCP EIN, Neustart, IP-Adresse abwarten und Weboberfläche auf Port 80 öffnen.
2. Mehrere Web-Seiten und Downloads nacheinander aufrufen.
3. DHCP AUS, kontrollierten Neustart abwarten und statische IP öffnen.
4. Wieder auf DHCP EIN wechseln und Weboberfläche erneut prüfen.
5. Ethernet im unveränderten Modus AUS/EIN schalten.
6. TFT-QR-Anzeige öffnen, zwischen Code 1/2 und 2/2 wechseln und anschließend Webzugriff erneut prüfen.
7. LED-Modi `Aus`, `Ein - Grundkurve` und `Selbstlernend` einschließlich SD-Systemkurve testen.
