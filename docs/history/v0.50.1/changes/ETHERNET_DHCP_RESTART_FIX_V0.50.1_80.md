# TP-3000 V0.50.1_80 – DHCP-Neustart und vollständige RAM2-Rückgewinnung

## Fehlerbild

Auch nach V0.50.1_79 wurde nach dem Wechsel von statischer Adresse auf DHCP keine Adresse angezeigt. Zuvor konnte Ethernet Aus/Ein beim erneuten Einschalten bis zum 8-s-Hardware-Watchdog hängen.

## Bestätigte zweite Ursache

NativeEthernet basiert auf FNET und besitzt keine belastbare `end()`- beziehungsweise vollständige Neuinitialisierungsfunktion. Wird der bereits initialisierte Stack von statischer IP auf DHCP umgestellt, bleibt die statische lokale Adresse im Stack aktiv. Ein erneutes `Ethernet.begin(mac, ...)` startet dann nicht zuverlässig einen neuen DHCP-Client. Das Gerät wartet bis zum Timeout; wiederholte Startversuche können den Bedienpfad festhalten.

Zusätzlich verblieben in V0.50.1_79 noch rund 12 KiB TFT-QR-Dauerpuffer in `DMAMEM`. Sie waren deutlich kleiner als in V0.50.1_78, reduzierten aber weiterhin genau den RAM2-Bereich, aus dem NativeEthernet seine Laufzeitpuffer bezieht.

## Korrektur

- Alle dauerhaft benötigten TFT-QR-Texte, die QR-Matrix und kleinen TP3C1-Metadaten liegen nun im ausreichend freien RAM1 statt in RAM2.
- Die QR-Funktion benötigt damit vor dem Ethernetstart keinen zusätzlichen dauerhaften RAM2-Block mehr.
- Ein Wechsel `DHCP AUS` ↔ `DHCP EIN` wird nach dem Speichern durch einen kontrollierten Software-Neustart wirksam. Dadurch beginnt FNET immer aus einem definierten Zustand; der frühere Watchdog-Hänger wird nicht mehr benutzt.
- Reines Ethernet Aus/Ein im unveränderten Modus verwendet eine noch gültige DHCP-Lease beziehungsweise dieselbe statische Adresse weiter und ruft `Ethernet.begin()` nicht unnötig erneut auf.
- Beim ersten DHCP-Start gibt es innerhalb eines Gesamtbudgets von 5,3 s einen Hauptversuch und – nur bei bereits erkanntem Link – einen kurzen zweiten Warteabschnitt. Anschließend darf FNET noch bis zu 30 s im Hintergrund eine Adresse übernehmen.

## Unverändert

TP3C1 V0.4, ZLIB/DEFLATE, Base38, QR-Codeinhalt, Code 1/2 und Code 2/2, Webserver, Messung, Regelung, Safety, EEPROM-Format und Projektbibliotheken bleiben unverändert.

## Hardwareprüfung

1. Nach Flashen mit DHCP EIN und gestecktem Kabel starten; IP bis zu 30 s beobachten.
2. DHCP AUS wählen: Das Gerät muss kontrolliert neu starten.
3. DHCP wieder EIN wählen: erneut kontrollierter Neustart, danach DHCP-Adresse.
4. Ethernet AUS und wieder EIN im selben DHCP-Modus: kein Watchdog-Reset.
5. Statische IP prüfen und danach wieder auf DHCP wechseln.
6. Beide TFT-QR-Codes anzeigen und scannen.
7. Teensyduino-Speicherbericht sichern; `RAM2 free for malloc/new` muss wieder mindestens das Niveau von V0.50.1_77 erreichen.
