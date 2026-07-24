# V0.50.1_83 – TFT-QR-Signaturprüfung und 42-Zeichen-Infoanzeige

## Basis

- direkte Basis: V0.50.1_82
- interne Version: `0.50.1`
- Build-ID: `0.50.1_83`

## TFT-Kalibrierschein-QR

Die erneute Prüfung des aktiven Systemkalibrierpakets läuft beim TFT-QR-Export nun ausdrücklich im Signaturmodus und verwendet dafür einen getrennten Scratch-Status, sodass der aktive Laufzeitstatus nicht überschrieben wird. Dabei werden Format, kanonischer Manifest-Hash sowie Root-/Labor-Signatur vollständig geprüft. Aktuelle Geräte-/Kopfbindung, Firmwareabweichung und Laufzeitanwendbarkeit werden in diesem Prüfschritt nicht als Sperrgrund verwendet.

Eine nach der Kalibrierung geänderte Kundenfirmware darf den historischen, weiterhin gültig signierten Kalibrierschein deshalb nicht blockieren. Beschädigte Dateien oder ungültige kryptografische Signaturen bleiben gesperrt.

Bei einem Fehler wird die konkrete Ursache zusätzlich mit dem Präfix `[TFT-QR]` seriell ausgegeben. Die Fehlermeldung am TFT wird UTF-8-sicher auf höchstens 42 Zeichen je Zeile umgebrochen.

## Info / Gültigkeit

- Jede sichtbare TFT-Zeile ist auf höchstens 42 Zeichen begrenzt.
- SHA-256-Werte werden in zwei vollständigen Zeilen zu je 32 Zeichen dargestellt.
- Der lange Firmwarezertifikat-Hinweis wird auf zwei Zeilen verteilt.
- Der gültige Kalibrierscheinstatus bleibt grün; nur der getrennte Firmwarebezug wird bei Abweichung gelb.
- Deutsche Texte verwenden die in der eingebauten Schrift vorhandenen Zeichen `ä`, `ö`, `ü`, `Ä`, `Ö`, `Ü` und `ß` statt Umschreibungen.
- Die scrollbare Ansicht wurde von 36 auf 43 physische Zeilen erweitert.

## Unverändert

Kalibrierformate, kanonische Signaturbytes, TP3C1 V0.4, TP3Q3, EEPROM, Messwerterfassung, Regelung, Safety, LED-Autoadaption, Ethernet und Libraries bleiben unverändert.
