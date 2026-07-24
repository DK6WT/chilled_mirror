# Web-Firmwarezertifikat-Upload – Body-Reader-Fix V0.50.1_54

## Fehlerbild

Im Browser ließ sich eine gültige `.tpfwcert` auswählen und der Aktivierungsknopf wurde freigeschaltet. Nach dem Klick antwortete das Gerät dennoch mit:

`Keine .tpfwcert-Datei übertragen`

## Ursache

Die Weboberfläche sendet den vollständigen Dateiinhalt als `POST /firmware-certificate-upload` mit `Content-Type: application/json` und korrektem `Content-Length`. Der nicht blockierende Requestleser erkannte diesen Pfad bereits für Routing und Diagnose, nahm ihn aber nicht in die Liste der POST-Pfade auf, deren Body vollständig gelesen werden muss.

Dadurch wurde der Request nach den Headern als vollständig markiert. Der Uploadhandler erhielt `requestBody == nullptr` und `requestBodyLength == 0`, obwohl der Browser die Datei korrekt gesendet hatte.

## Korrektur

`ETH_PATH_FIRMWARE_CERT_UPLOAD` wurde in dieselbe begrenzte Body-Leselogik aufgenommen wie Geräte-, Kopf- und Systemkalibrierungsuploads. Es gelten unverändert:

- Auswertung von `Content-Length`,
- Größenbegrenzung durch `ETH_UPLOAD_MAX_BYTES`,
- exklusiver Besitzer des gemeinsamen Uploadpuffers,
- blockweises Lesen pro Hauptloop,
- absolute und Inaktivitäts-Zeitgrenzen,
- abschließende NUL-Terminierung nur innerhalb des reservierten Puffers.

Die kryptografische Prüfung und Aktivierung in `tpFirmwareIntegrityImportCertificateJson()` wurde nicht verändert.
