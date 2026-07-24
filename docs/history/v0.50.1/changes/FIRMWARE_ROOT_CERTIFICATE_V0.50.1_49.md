# TP-3000 V0.50.1_49 – root-signiertes Firmwarezertifikat

## Ziel

Das Gerät prüft beim Start den SHA-256 des tatsächlich laufenden Teensy-4.1-
Firmwareabbilds. Der Sollwert stammt aus einem separaten, mit dem privaten
Hersteller-Root signierten Firmwarezertifikat. Das bisherige nachträgliche
Patchen der HEX-Datei entfällt vollständig.

## Formate

### Anfrage

`TP3000-FIRMWARE-CERTIFICATE-REQUEST-1`

Die Anfrage enthält Geräteidentität, Request-ID, 72-h-Zeitfenster,
Firmwareversion, Build-ID, Buildtag, Zielhardware, Abbildbasis/-größe,
Firmware-SHA-256 und das vollständige Gerätezertifikat. Der kanonische
Manifest-Hash wird mit dem individuellen Geräteschlüssel ECDSA P-256 signiert.

### Zertifikat

`TP3000-FIRMWARE-CERTIFICATE-1`

Das Zertifikat ist an das Firmwareabbild und nicht an ein einzelnes Gerät
gebunden. Es enthält Version, Build-ID, Buildtag, Zielhardware, Abbildbasis,
Abbildgröße, SHA-256, Ausstellungszeit, Zertifikats-ID und Root-Key-ID. Der
Hersteller-Root signiert den kanonischen Manifest-Hash. Ein identisches Abbild
kann deshalb auf mehreren TP-3000 mit demselben Zertifikat verwendet werden.

## Speicherorte

- aktive Freigabe: `/CERTIFICATION/FIRMWARE.TPFCERT`
- Statusnachweis: `/CERTIFICATION/FIRMWARE.TPS`
- exportierte Anfragen: `/CERTIFICATION/REQUESTS/*.tpfwreq`

Alle Schreibvorgänge für aktive Freigabe und Status verwenden TMP/BAK,
Schließen, vollständiges Zurücklesen und Vergleich.

## Startprüfung

1. `_flashimagelen` plausibilisieren.
2. vollständiges XIP-Abbild in 4096-Byte-Blöcken hashen.
3. aktives Firmwarezertifikat laden.
4. Root-Key-ID, Manifest-SHA-256 und Root-Signatur prüfen.
5. Version, Build-ID, Buildtag, Hardware, Größe und SHA-256 vergleichen.
6. Status auf SD dokumentieren.

Ohne passendes Zertifikat bleiben Kalibrieranfragen, neue Kalibrieraktivierungen
und zertifizierte CSV-/TPSIG-/TPLOG-Ausgaben gesperrt. Die normale Messung und
Anzeige bleiben verfügbar. Die Firmwareanfrage selbst bleibt absichtlich
freigeschaltet, damit ein neuer Build zertifiziert werden kann.

## Sicherheitsgrenze

Ohne Secure Boot kann eine gezielt manipulierte Firmware ihre eigene Prüfung
umgehen. Der aktuelle Mechanismus schützt gegen beschädigte Firmware, falsche
Builds, versehentliche Verwechslung und normale Manipulation im vorgesehenen
TP-3000-Sicherheitsprofil.

Kanonische Kalibrierbytes, TP3C1 V0.3, TP3Q3, TPSIG, TPLOG, Messwerterfassung,
Regelung und Safety wurden nicht verändert.
