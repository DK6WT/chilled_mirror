# TP-3000 V0.50.1_58 – Zertifizierungsübersicht vollständig lesbar

## Fehlerbild

Auf `/identity` wurde der Status

`Firmwarezertifikat passt nicht zum laufenden Abbild`

sichtbar nach `... laufenden Ab` abgeschnitten, obwohl in der Tabellenzeile noch Platz vorhanden war.

## Ursache

Nicht die Browserbreite war die eigentliche Begrenzung. `TpFirmwareApprovalStatus::statusText` besaß nur 48 Byte. Für einen nullterminierten C-String standen damit höchstens 47 Zeichen zur Verfügung. Der vollständige deutsche Status benötigt 51 Zeichen plus Abschlussnull.

## Änderung

- `statusText` wurde von 48 auf 96 Byte erweitert.
- Die Fortschrittstabelle verwendet ein festes Spaltenlayout mit 22 % / 58 % / 20 %.
- Die mittlere Statusspalte darf normal umbrechen und verwendet bei Bedarf `overflow-wrap:anywhere`.
- Die rechte Zusatzspalte bleibt sichtbar, wird aber zugunsten der langen Statusmeldungen etwas weiter nach rechts verschoben.

## Unverändert

Firmwareprüfung, Zertifikatsformat, Signaturen, Kalibrierung, QR-Code, Messung, Regelung und Safety bleiben unverändert.
