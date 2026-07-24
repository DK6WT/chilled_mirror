# SD-Boot- und Crashdiagnose – V0.50.1_51

## Zweck

Der TP-3000 besitzt im normalen Einbau keinen angeschlossenen seriellen Monitor.
Build `0.50.1_51` schreibt deshalb die Bootdiagnose in die Datei
`/TP3000_BOOT.TXT` im Wurzelverzeichnis der internen SD-Karte.

## Gespeicherte Angaben

Bei jedem Start werden mindestens folgende Angaben angehängt:

- Firmwareversion und Build-ID,
- Zeitpunkt innerhalb des Bootvorgangs in `millis()`,
- roher i.MX-RT-Resetstatus `SRC_SRSR`,
- zuletzt erreichte Bootstufe.

Liegt ein Teensy-`CrashReport` aus dem vorherigen Lauf vor, wird dessen kompletter
Text ebenfalls in dieselbe Datei geschrieben. Die Datei wird nach jeder Stufe
geflusht und geschlossen. Ab 128 KiB wird sie automatisch neu begonnen.

## Bootloop-Schutz

Nach einem gespeicherten MPU-/Hard-Fault oder einem erkannten Watchdog-Reset wird der Firmware-Flashhash in genau
diesem Wiederanlauf übersprungen. Das Gerät kann dadurch bis zum Hauptbildschirm
starten und die SD-Karte kann entnommen werden. Der Firmwarestatus lautet dann:

`Hash im Diagnose-Wiederanlauf übersprungen – siehe SD`

Alle zertifizierungsabhängigen Funktionen bleiben in diesem Zustand gesperrt.
Beim nächsten sauberen Start ohne gespeicherten CrashReport wird der vollständige
Firmwarehash wieder normal ausgeführt.

## Breadcrumbs

CrashReport-Breadcrumb 1 kennzeichnet die Bootstufe:

- `0x54503001`: `setup()` gestartet,
- `0x54503002`: TFT initialisiert,
- `0x54503003`: unmittelbar vor Firmwarehash,
- `0x54503004`: Firmwarehash verlassen,
- `0x54503005`: `setup()` vollständig beendet.

Breadcrumb 2 enthält während der Hashprüfung den zuletzt vollständig gelesenen
Flash-Offset in Bytes. Damit ist bei einem Speicherzugriffsfehler erkennbar, bis
zu welchem 4096-Byte-Block die Prüfung gekommen ist.

## Unverändert

Firmwarezertifikatsformat, Root-Signatur, KeyGen V0.7.13,
Kalibrierformate, TP3C1/TP3Q3, TPSIG, TPLOG, ADC, Regelung und Safety bleiben
unverändert. Durch den neuen Build ändert sich jedoch der Firmware-SHA-256; nach
der Fehlerdiagnose ist daher ein neues Firmwarezertifikat für Build
`0.50.1_51` erforderlich.
