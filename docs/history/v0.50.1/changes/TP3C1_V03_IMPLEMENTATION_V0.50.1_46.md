# TP3C1 V0.3 – Implementierung in Firmware-Build 0.50.1_46

## Ursache

Der bisherige TP3C1-V0.2-Encoder akzeptierte ausschließlich Geräte- und Kopfjustierungen V2. Der aktuelle KeyGen erzeugt Geräte- und Kopfjustierungen V3 mit signiertem `CalibrationScope`, `CalibrationResultsEncoding` und `CalibrationResultsBase64Url`. Deshalb brach die QR-Erzeugung mit `Kopfjustierung V2 erforderlich` ab.

## Umsetzung

Der Browserencoder des Kalibrierscheins wurde auf TP3C1 V0.3 umgestellt und akzeptiert bewusst nur:

- Gerätejustierung V3
- Kopfjustierung V3
- Systemkalibrierung V2

Die TPC1-Kalibriermesspunkte werden nun nicht nur für die Systemkalibrierung, sondern auch für Geräte- und Kopfjustierung in den jeweiligen Approval-Unterblock aufgenommen. Dadurch können deren V3-Signaturen später bytegenau rekonstruiert werden.

Weitere Änderungen:

- Schema 1.1
- RequiredFeatures `0x3F`
- Transportkennungen `0x31` und `0x32`
- strikte Prüfung von TPC1-Kennung, Umfang, Punktzahl, Länge und reservierten Bytes
- sichtbare Beschriftung `TP3C1 V0.3`
- alter V0.2-Fehlertext entfernt

Unverändert bleiben:

- Gerätezertifikat und Laborzertifikat
- alle fünf vorhandenen Signaturen
- kanonische Bytes der signierten Dokumente
- TP3Q3-Download
- Base38, ZLIB Level 9, CRC-32, zwei Teile, Fehlerkorrektur M und 115-mm-Druckgröße
- ADC, Messwerterfassung, Regelung, Optik und Safety

## Migrationshinweis für das erste Testgerät

Da V0.3 keine V2-Dokumente akzeptiert, müssen Gerätejustierung und Kopfjustierung mit dem aktuellen KeyGen als V3 ausgestellt werden. Eine neue Gerätejustierung verändert deren Manifest. Deshalb muss anschließend auch die Systemkalibrierung neu ausgestellt werden, damit sie an die beiden aktuellen V3-Justierungsmanifeste gebunden ist.
