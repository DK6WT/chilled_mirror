# TP-3000 V0.50.1_79 – Ethernet-/DHCP-RAM2-Wiederherstellung

## Fehlerbild

Nach V0.50.1_78 konnte Ethernet bei aktivierter DHCP-Konfiguration keine IP-Adresse mehr beziehen. Beim Ausschalten und erneuten Einschalten von Ethernet blieb der Startpfad hängen, bis der 8-s-Hardware-Watchdog das Gerät zurücksetzte.

## Ursache

Die neue TFT-QR-Funktion aus V0.50.1_78 reservierte ihre vollständigen TP3C1-, ZLIB- und QR-Arbeitspuffer dauerhaft in `DMAMEM`/RAM2. Zusätzlich zu den zwei QR-Texten und der sichtbaren Matrix lagen dort unter anderem:

- 8192 Byte TP3C1-Umschlag,
- 8320 Byte Kompressionspuffer,
- 6144 Byte Common-Puffer,
- 4096 Byte Approval-Puffer,
- 4096 Byte Base38-Arbeitspuffer,
- zwei QR-Matrix-/Funktionsbitfelder,
- drei QR-Codewort-/Blockpuffer,
- doppelte Kalibrierstatus-, Werte-, Rollen- und Approval-Strukturen.

Der quellgleiche Host-Abschnittsvergleich bei deaktivierter Optimierung ergibt gegenüber V0.50.1_77 rund 69,5 KiB zusätzliche statische BSS-Belegung durch die TFT-QR-Erweiterung. Die zuvor vorhandene RAM2-Reserve wurde damit nahezu vollständig aufgebraucht. NativeEthernet benötigt beim Start und insbesondere für DHCP weiterhin freien Heap in RAM2; die verbleibende Reserve war dafür nicht mehr ausreichend.

## Korrektur

V0.50.1_79 behält die TFT-Funktion, entfernt jedoch die dauerhaft duplizierten Arbeitspuffer:

- TP3C1-Umschlag verwendet während des synchronen Aufbaus den vorhandenen Web-JSON-Puffer `ethJsonBuf`.
- Kompression, Common-, Approval-, Values-, Base38- und QR-Codewortarbeit verwenden nacheinander bereits vorhandene Kalibrier-Arbeitspuffer.
- Geräte-, Kopf- und Systemwerte sowie Verifikationsstatus werden in den bestehenden aktiven Strukturen erneut geprüft, statt sie für die QR-Seite zu duplizieren.
- Dauerhaft neu bleiben nur die beiden fertigen TP3C1-Texte, die sichtbare QR-Matrix sowie kleine Anzeige-/Metadatenzustände.
- Die beiden TP3C1-Texte bleiben unabhängig von Web- und Kalibrier-Arbeitspuffern erhalten, sodass UP/DOWN ohne erneutes Lesen oder Komprimieren zwischen Code 1/2 und Code 2/2 wechseln kann.

Der gleiche Host-Abschnittsvergleich zeigt gegenüber V0.50.1_78 eine Rückgewinnung von rund 55,9 KiB statischer BSS-Belegung. Gegenüber V0.50.1_77 verbleiben für die vollständige TFT-QR-Funktion nur rund 13,5 KiB zusätzlicher statischer Zustand.

## Unverändert

TP3C1 V0.4, ZLIB/DEFLATE, Base38, CRC-32, QR-Matrix, Fehlerkorrektur M, automatische Maske, Ruhezone, Codeaufteilung, Signaturprüfung und Bedienung bleiben unverändert. Messung, Regelung, Safety, Schnittstellenkonfiguration und NativeEthernet-Logik wurden nicht funktional geändert.

## Hardwareprüfung

Nach dem Flashen sind zu prüfen:

1. Ethernet mit DHCP einschalten; innerhalb des bestehenden Zeitbudgets muss eine IP erscheinen.
2. Ethernet aus- und wieder einschalten; kein Watchdog-Reset.
3. Static-IP-Betrieb prüfen.
4. Weboberfläche aufrufen und parallel den TFT-Menüpunkt `Kalibrierschein anzeigen` öffnen.
5. Beide Codes scannen und mit der Webansicht vergleichen.
6. Teensyduino-Speicherbericht sichern; besonders `RAM2 free for malloc/new` mit V0.50.1_78 vergleichen.
