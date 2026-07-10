# GSL1680-Panel-Firmware

Die panelspezifische Firmware des kapazitiven GSL1680-Touchcontrollers ist eine
vom GPL-lizenzierten TP-3000-Treibercode getrennte Herstellerkomponente.

## Verwendetes Display

- Hersteller/Lieferant: EastRising / BuyDisplay
- Modul: `ER-TFTM050A2-3-3661`
- Auflösung: 800 × 480 Pixel
- Displaycontroller: RA8875
- Touchcontroller: GSL1680X

Das Modul-Datenblatt nennt den GSL1680X als Touchcontroller und verweist darauf,
dass zugehörige Interface-Dokumente, Democode und IC-Datenblätter über die
Herstellerseite bereitgestellt werden.

## Datei im öffentlichen Quellpaket nicht enthalten

Die Weitergaberechte der Firmwaretabelle sind nicht abschließend dokumentiert.
Deshalb enthält dieses öffentliche TP-3000-Quellpaket die Herstellerdatei
**nicht**. TP-3000 erteilt daran keine Lizenz und behauptet nicht, dass sie unter
GPL steht.

Für den lokalen Build muss die zum konkreten Display gehörende Originaldatei
direkt aus dem EastRising-/BuyDisplay-Herstellerpaket bezogen und unverändert an
folgenden Pfad kopiert werden:

```text
external/GSL1680/gslX680_311_5_F.h
```

Der Pfad ist in `.gitignore` eingetragen. Die Datei darf nicht in öffentliche
Commits, Forks, Release-ZIPs oder andere weitergegebene Projektarchive gelangen.
Ohne die lokale Datei bricht der Build mit einer eindeutigen Fehlermeldung ab.

## Getestete Referenzdatei

Die auf der vorhandenen TP-3000-Hardware erfolgreich getestete Originaldatei
trug den Namen:

```text
gslX680_311_5_F.h
```

Kennzeichnungen im Dateikopf:

```text
IC: GSL1680f
DTAE: Aug-04-2017
VER: 1.1
```

SHA-256 der getesteten Originaldatei:

```text
ea756ce8d96631337fa09abe22c4e95aaad86cb706b561a52dfd143e751edd96
```

Die Prüfsumme dient ausschließlich zur Identifikation der getesteten Fassung.
Eine andere offizielle Herstellerdatei für ein anderes Panel kann abweichen und
muss separat geprüft werden.

## Originaldatei unverändert verwenden

Nicht vorgesehen sind insbesondere:

- das 8051-Schlüsselwort `code` aus der Datei zu entfernen,
- den Arraynamen oder Datentyp zu ändern,
- auskommentierte Firmwarebereiche zu aktivieren,
- insbesondere die Seiten `0xE0` bis `0xE6` zu entkommentieren,
- die Datei umzubenennen oder neu zu formatieren.

Der TP-3000-Wrapper `GSL1680Firmware.h` definiert das 8051-spezifische
Schlüsselwort `code` ausschließlich während des Includes als `PROGMEM`. Die
Herstellerdatei selbst bleibt dadurch bytegenau unverändert, während die
Firmwaretabelle im Teensy-Flash abgelegt wird.

Die Referenzfassung wurde am 14.06.2026 auf der TP-3000-Hardware kompiliert,
geladen und praktisch getestet. Touchstart, Bedienung, Ecken, Randbereiche und
kurze Tippberührungen verhielten sich normal.

## Kompilierte Firmwareabbilder

Beim Kompilieren wird die lokal bereitgestellte Firmwaretabelle in das
Teensy-Programmabbild aufgenommen. Ein so erzeugtes HEX-, BIN- oder sonstiges
Firmwareabbild enthält daher ebenfalls die Herstellerdaten. Solche Abbilder
werden vom TP-3000-Projekt nicht öffentlich mit diesem Quellpaket verteilt.

Der GSL1680 verliert seinen flüchtigen Inhalt bei einem Versorgungsausfall;
deshalb lädt der TP-3000-Treiber die Tabelle bei jedem Gerätestart erneut über
I²C in den Touchcontroller.
