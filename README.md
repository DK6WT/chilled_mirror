# chilled_mirror

**High-precision chilled mirror dew point controller for Teensy 4.1, ADS1263, BMP585 and RA8875 touch display under GNU GPLv3.**

Dieses Projekt beinhaltet die komplette Software zur Steuerung eines hochpräzisen Taupunktspiegels. Die Basis des Codes wurde optimiert für maximale Stabilität, schonende Auslastung des SPI- und I2C-Busses und eine ruhige Messwertanzeige.

## Features
* **Leistungsstarke Hardware:** Optimiert für den Teensy 4.1 mit 600 MHz.
* **Präzise Sensorik:** Direkte Einlesung des ADS1263 und BMP585 Luftdrucksensors über den I2C2-Bus.
* **Geglättete Messwerte:** Integrierter Ringpuffer im Regelungskreis für ein absolut stabiles Signal ohne Flackern.
* **Hochpräzise Berechnung:** Mathematische Berechnung des Sättigungsdampfdrucks nach der Sonntag-Formel (1990) inklusive Real-Gas-Korrektur (WMO f-Faktor) in `double`-Präzision.
* **Schonende RTC-Abfrage:** Die externe Echtzeit-Uhr (RV-3129) synchronisiert sich stressfrei nur alle 15 Minuten mit der internen Teensy-Uhr. Das verhindert Störungen des Quarzes und sorgt für eine perfekt gehende Uhrzeit.
* **Grafische Oberfläche:** Menügestützte Bedienung über ein RA8875 & GSL1680 Touchscreen-Display.

## Lizenz & Herkunft (Lineage)
Dieses Programm ist freie Software unter den Bedingungen der **GNU General Public License Version 3 (GPLv3)**.

Das Projekt basiert historisch auf den großartigen Vorarbeiten der Open-Source-Community:
1. **Initial-Version (2013-2015):** Multi-Display SWR-Meter von *Loftur E. Jonasson (TF3LJ)*.
2. **Display- & Touch-Umbau (2017-2021):** Anpassung an RA8875/GSL1680 von *J.G. Holstein*.
3. **Taupunkt-Steuerung (2025/2026):** Kompletter Umbau der Messstrecke, Sensorik und Regelung von DK6WT.

Note: This project is currently a work in progress (WIP)! 

**Disclaimer:** This is a private open-source project. Code is provided "as is" without any warranty, guarantees, or official support.
