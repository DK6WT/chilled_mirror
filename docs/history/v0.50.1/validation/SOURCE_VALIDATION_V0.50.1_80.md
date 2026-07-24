# Source Validation V0.50.1_80

## Geprüfte Änderungen

- Build-ID auf `0.50.1_80` angehoben.
- Neue TFT-QR-Dauerpuffer enthalten kein `DMAMEM` mehr und werden damit nicht mehr aus RAM2 belegt.
- Die neuen TP3C1-Metadatenpuffer in `TPsignedCalibration.cpp` enthalten ebenfalls kein `DMAMEM` mehr.
- DHCP-Wechsel ruft nach persistentem Speichern den kontrollierten `SOFT_RESET()`-Pfad auf.
- Ethernet Aus/Ein im gleichen Modus besitzt Wiederverwendungspfade für vorhandene DHCP- beziehungsweise Static-IP-Konfigurationen.
- DHCP-Haupt- und Zweitzeitbudget ergeben zusammen 5300 ms und bleiben unter dem 8-s-Hardware-Watchdog.
- Kein automatischer zyklischer `Ethernet.begin()`-Suchlauf im normalen Messloop ergänzt.
- Ordner `libraries/` gegenüber V0.50.1_79 bytegleich.

## Statische Prüfungen

- Klammer-, String- und Kommentarlauf über alle Projektdateien.
- Prüfung aller neuen Funktionsnamen und Aufrufstellen.
- Suche nach verbliebenen `DMAMEM`-Deklarationen in den TFT-QR-Neudateien.
- Versionskonsistenz in Quellcode und Übergabedokumentation.
- ZIP-CRC sowie bytegleicher Frischentpackvergleich.

## Noch offen

Ein vollständiger Teensyduino-Linkerbericht und reale Hardwaretests können in der Erstellumgebung nicht ausgeführt werden. Insbesondere DHCP, Static-IP, Moduswechsel, Ethernet Aus/Ein und der TFT-QR-Scan müssen am Gerät geprüft werden.
