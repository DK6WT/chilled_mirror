# Source Validation V0.50.1_83

## Geänderte Quellen

- `TPsignedCalibration.cpp`: Systempaket wird für den TFT-QR-Export ausdrücklich signaturbezogen geprüft.
- `TPmenu_QrCertificate.ino`: exakte serielle Fehlerdiagnose und UTF-8-sicherer 42-Zeichen-Umbruch.
- `TPmenu_Pages.ino`: Infoansicht auf 43 physische Zeilen mit maximal 42 Zeichen je Zeile, aufgeteilten SHA-256-Werten und echten Umlauten.
- `TP3C1_FORMATSPEZIFIKATION_V0.4.md`: Firmwareabweichung neutral dokumentiert; der Kalibrierschein bleibt gültig.
- `TPsignedData.h`: Build-ID `0.50.1_83`.

## Sicherheitsprüfung

- Format- und Signaturprüfung der Geräte-, Kopf- und Systempakete bleibt aktiv.
- Nur aktuelle Bindungs-/Anwendbarkeitsbewertungen werden vom historischen QR-Export getrennt.
- Beschädigte Dateien und falsche Root-/Labor-Signaturen bleiben Sperrgründe.
- Firmwareabweichung bleibt im TP3C1-Nachweis dokumentiert.

## Hardwaretest

1. TFT-Menü `Kalibrierschein anzeigen` öffnen.
2. Code 1/2 und Code 2/2 mit UP/DOWN wechseln.
3. Bei einem Fehler die vollständige 42-Zeichen-Meldung und optional die serielle Zeile `[TFT-QR] ...` notieren.
4. `Info / Gültigkeit` vollständig durchscrollen; keine Zeile darf rechts umbrechen und wieder links beginnen.
5. Beide 64-stelligen SHA-256-Werte müssen jeweils vollständig in zwei 32-stelligen Zeilen erscheinen.
