# V0.50.1_84 – TFT-QR-Kalibrierscheinquelle

Basis ist V0.50.1_83. Die interne Firmwareversion bleibt `0.50.1`, die Build-ID ist `0.50.1_84`.

## Fehlerursache

Der TFT-QR-Pfad prüfte das aktuelle Systemkalibrierungspaket zunächst korrekt mit Approval-Schema V4. Beim anschließenden erneuten Einlesen für den TP3C1-Aufbau wurde jedoch versehentlich die Zahl `2` übergeben. Diese Zahl wurde als Dateiformat-Version verstanden, ist in `scParseCommonPackageV2()` aber tatsächlich die Version des eingebetteten Approval-Schemas.

Approval V2 enthält noch kein Feld `CertificateSource`. Dadurch blieb `certificateSource` beim zweiten Einlesen leer und der QR-Aufbau brach mit `Kalibrierscheinquelle ist ungültig` ab. Webansicht und Infoanzeige waren nicht betroffen, weil deren reguläre Systemprüfung bereits Approval V4 verwendete.

## Korrektur

- Gerätejustierung und Kopfjustierung werden weiterhin mit Approval V3 eingelesen.
- Das aktuelle Systemkalibrierungspaket V2 wird für den TFT-QR nun korrekt mit Approval V4 eingelesen.
- `CertificateSource=TP3000` beziehungsweise `EXTERNAL_PDF` bleibt dadurch beim QR-Aufbau erhalten.
- Signaturprüfung, Kalibrierwerte, TP3C1-Format, Firmwareabweichungsbewertung und Webanzeige bleiben unverändert.
