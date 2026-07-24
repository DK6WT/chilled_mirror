# Firmwareabbild-Integrität – Build 0.50.1_48

## Ziel

Build `0.50.1_48` prüft beim Start das vollständige, tatsächlich programmierte
Teensy-4.1-Firmwareabbild mit SHA-256. Damit werden beschädigter Flash,
unvollständige Updates und gewöhnliche Veränderungen am Abbild erkannt.

Der Stand besitzt weiterhin **keinen Secure Boot**. Eine gezielt geänderte
Firmware könnte ihre eigene Prüfung entfernen oder neu versiegeln. Die Funktion
ist daher ein Integritäts- und Betriebsnachweis, aber keine unveränderliche
Hardware Root of Trust.

## Eingebettetes Manifest

Das Firmwareabbild enthält einen 112 Byte großen Manifestblock:

```text
Magic                 TP3000-FWIMG-1!#
FormatVersion         1
ImageBase             0x60000000
ImageSize             vom Teensy-Linkerabbild
BuildDateYmd          20260716
FirmwareVersion       0.50.1
BuildId               0.50.1_48
ExpectedSha256        32 Byte
ManifestCrc32         CRC-32 des Manifestblocks
```

Der SHA-256 umfasst das vollständige Linkerabbild. Nur das Feld
`ExpectedSha256` und das Feld `ManifestCrc32` werden beim Hashen als Null
behandelt. Dadurch gibt es keinen Kreisbezug.

## Erforderlicher Buildabschluss

Der Compiler kann den Hash des fertigen eigenen Abbilds nicht vor dem Linken
kennen. Deshalb muss die exportierte HEX-Datei einmal nach dem Build versiegelt
werden:

```bat
tools\finalize_firmware_manifest.cmd "C:\Pfad\TP-3000.ino.hex"
```

Erzeugt werden:

```text
TP-3000.ino_final.hex
TP-3000.ino_final.bin
```

**Programmiert werden muss die Datei `*_final.hex`.** Ein direkt aus der
Arduino IDE hochgeladener, nicht finalisierter Build startet zwar normal,
meldet aber `Build-Hash nicht finalisiert` und sperrt sicherheitsrelevante
Kalibrier- und zertifizierte Logfunktionen.

## Startprüfung

Beim Start wird:

1. die reale Linker-Abbildgröße aus `_flashimagelen` gelesen,
2. der komplette XIP-Flashbereich ab `0x60000000` in 4096-Byte-Blöcken gehasht,
3. Manifeststruktur und Manifest-CRC geprüft,
4. Soll- und Ist-SHA-256 verglichen,
5. der Status auf TFT und Web angezeigt,
6. der Status auf SD dokumentiert.

Mögliche Zustände:

```text
Integrität bestätigt
Build-Hash nicht finalisiert
Firmware-Integrität fehlerhaft
Firmware-Manifest fehlerhaft
```

## SD-Nachweis

Die Datei wird transaktional geschrieben:

```text
/CERTIFICATION/FIRMWARE.TPS
```

Sie enthält Geräte-SN, Version, Build-ID, Buildtag, Abbildgrößen, Soll- und
Ist-SHA-256, Prüfzeitpunkt und Ergebnis. Temporär- und Backup-Dateien werden
mit vollständiger Rückleseprüfung verwendet.

## Sicherheitsrelevante Sperren

Ohne erfolgreich bestätigtes Firmwareabbild werden gesperrt:

- Erzeugung von Geräte-, Kopf- und Systemkalibrierungsanfragen,
- Import/Aktivierung neuer Geräte-, Kopf- und Systemkalibrierungen,
- zertifizierte CSV-/TPSIG-Ausgabe,
- zertifizierte TPLOG-Ausgabe.

Die normale Messung und Anzeige laufen weiter. Unter `Info / Gültigkeit` sind
Status, Version/Build, Abbildgröße, Soll-/Ist-Hash und Prüfzeitpunkt sichtbar.

## Zertifizierte Logs

Der tatsächlich gemessene Firmware-SHA-256 wird in die vorhandene
Firmware-Manifest-Hashposition von TPSIG/TPLOG übernommen. Die bestehenden
TPSIG-/TPLOG-Containerformate und ihre Feldreihenfolge werden nicht geändert.

## Noch nicht Bestandteil dieses Builds

Der Firmware-SHA-256 wird noch nicht als neues signiertes Feld in
Gerätejustierung, Kopfjustierung oder Systemkalibrierung aufgenommen. Das
würde eine koordinierte neue Dokumentformat-/KeyGen-Version erfordern und wird
nicht still in die bestehenden kanonischen Signaturbytes eingefügt.
