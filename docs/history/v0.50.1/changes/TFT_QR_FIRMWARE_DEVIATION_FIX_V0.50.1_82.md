# V0.50.1_82 – TFT-QR bei geänderter Firmware und eindeutige Statusanzeige

## Basis

- direkte Basis: V0.50.1_81
- interne Version: `0.50.1`
- Build-ID: `0.50.1_82`

## Fehlerursache

Die TFT-QR-Erzeugung verwendete bei der erneuten Prüfung der aktiven Geräte-, Kopf- und Systempakete den vollständigen Laufzeit-Rückgabewert der Verifikationsfunktionen. Dieser Rückgabewert enthält neben der kryptografischen Signatur auch die aktuelle Anwendbarkeit beziehungsweise Bindung. Dadurch konnte ein weiterhin korrekt signierter Kalibrierschein am TFT als nicht verfügbar behandelt werden, obwohl die Webansicht das signierte Dokument weiterhin öffnen konnte.

## Korrektur

- Die TFT-QR-Erzeugung prüft die drei aktiven Pakete weiterhin vollständig kryptografisch.
- Für die QR-Verfügbarkeit ist jetzt ausschließlich `signatureValid` maßgeblich.
- Eine geänderte Kundenfirmware oder ein abweichender aktueller Firmware-SHA-256 sperrt Code 1/2 und Code 2/2 nicht mehr.
- Der TP3C1-Nachweis enthält weiterhin den bei der Kalibrierung dokumentierten Firmwarekontext, den aktuellen SHA-256 und den Status `FIRMWARE_CHANGED`.
- Beschädigte Dateien, falsche Root-/Labor-Signaturen und unvollständige Pakete bleiben weiterhin gesperrt.

## Anzeige

- Systemkalibrierung bleibt bei reiner Firmwareabweichung `signiert und gültig`.
- Der Firmwarebezug wird gelb als Abweichung vom dokumentierten Kalibrierstand angezeigt.
- Ein nicht passendes vorhandenes Hersteller-Firmwarezertifikat wird als Hinweis formuliert: Es gehört zu einem anderen Firmwareabbild.
- Kryptografische Fehler bleiben rot.

## Unverändert

Kalibrier- und Zertifikatsformate, kanonische Bytes, Signaturen, TP3C1 V0.4, TP3Q3 QR-4, Firmwarehashes, Messung, Regelung, Safety, LED-Autoadaption, Ethernet und Libraries bleiben unverändert.
