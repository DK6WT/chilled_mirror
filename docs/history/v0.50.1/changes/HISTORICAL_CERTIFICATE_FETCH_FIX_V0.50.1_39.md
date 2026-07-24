# Historische Kalibrierzertifikate – Netzwerkfix V0.50.1_39

## Fehlerbild

Das aktuelle Kalibrierzertifikat ließ sich anzeigen. Wurde derselbe oder ein historischer Datensatz über die Zeitraumssuche ausgewählt, zeigte die Zertifikatsseite dagegen `Kalibrierschein konnte nicht aufgebaut werden – Failed to fetch`.

## Ursache

`EthBoundedWriter` begrenzt kleine HTTP-Antworten auf 120 ms. Die Zeitmessung begann bisher schon beim Eingang des Requests. Historische Zertifikate müssen jedoch zuerst von SD gelesen und anschließend kryptografisch geprüft werden. Wenn diese Vorarbeit länger dauerte, war das Antwortbudget vor dem ersten Antwortbyte abgelaufen. Der Server schloss den unvollständigen Socket; deshalb gab es keinen HTTP-Fehlertext, sondern den Browserfehler `Failed to fetch`.

## Korrektur

`EthBoundedWriter` besitzt nun `restartBudget()`. Nach abgeschlossener langsamer SD-/Kryptoprüfung wird ausschließlich das Schreibbudget neu gestartet. Die eigentliche Prüfung wird nicht verkürzt und keine Signaturprüfung übersprungen.

Der Neustart erfolgt bei:

- Zeitraumssuche im Kalibrierarchiv,
- historischer Systemkalibrierung,
- historischer Geräte- und Kopfjustierung,
- historischen externen PDF-Metadaten,
- historischem externen PDF-Download.

Zusätzlich lädt die Browserseite historische Geräte- und Kopfjustierung sequenziell. Damit konkurrieren keine zwei langsamen SD-Archivabrufe miteinander.

## Unverändert

Zertifikate, kanonische Signaturbytes, Feldreihenfolgen, Skalierungen, TP3C1, TP3Q3, externe PDF-Hashes und die Kalibrierarchivstruktur sind unverändert.
