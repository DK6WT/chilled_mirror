# TP-3000 V0.50.1_55 – Firmwarekontext der Systemkalibrierung

## Ziel

Der TP-3000 dokumentiert, welche Firmware bei der Erzeugung einer Systemkalibrierungsanfrage tatsächlich lief. Ein Herstellerzertifikat ist optional. Eigene Kundenfirmware bleibt vollständig nutzbar; deren Validierung und Freigabe erfolgen außerhalb des Geräts im Qualitätsmanagement des Betreibers.

## Gerätesignierter Kontext

Bei jeder neuen `.tpscalreq` wird zusätzlich gespeichert:

- `SourceRequestId` und `SourceRequestManifestSha256`,
- Firmwareversion und Build-ID,
- Basis, Größe und Ist-SHA-256 des laufenden Firmwareabbilds,
- Status des optionalen Hersteller-Firmwarezertifikats,
- Firmwarezertifikats-ID, Hersteller-Root-ID und Freigabezeit, soweit vorhanden,
- Erfassungszeitpunkt und Erfassungsart.

Der feste Binärdatensatz `TP3000-SYSTEM-FIRMWARE-CONTEXT-1` wird mit dem individuellen Geräteschlüssel ECDSA-P256/SHA-256 signiert und unter

`/CALIBRATION/FIRMWARE_CONTEXT/<SourceRequestId>.tpfctx`

gespeichert. Die Systemkalibrierung signiert ihrerseits das unveränderte `SourceRequestManifestSha256`. Dadurch ist der Firmwarekontext an dieselbe Anfrage gebunden, aus der der Kalibrierschein entstand, ohne das bestehende `.tpscal`-Format oder KeyGen V0.7.13 zu ändern.

## Eigener und externer Kalibrierschein

Die Firmwareregel ist für beide Quellen identisch:

- TP-3000-eigener Kalibrierschein,
- externer PDF-Kalibrierschein, der in die Systemkalibrierung eingebunden ist.

Das externe Original-PDF wird niemals verändert. Der TP-3000 zeigt seinen gerätesignierten Firmwarekontext im darüberliegenden Systemkalibrierungsnachweis an.

## Anzeige

Der Web-Kalibrierschein zeigt:

- Firmwareversion / Build,
- Firmware-SHA-256 und Abbildgröße,
- Herstellerfreigabe gültig, nicht vorhanden, nicht passend oder ungültig,
- optionale Zertifikats-ID und Hersteller-Root,
- Erfassungszeitpunkt,
- Vergleich mit der aktuell laufenden Firmware.

Eine spätere Firmwareabweichung wird nur angezeigt. Sie macht die signierte Systemkalibrierung nicht automatisch ungültig oder unanwendbar.

## Kundenfirmware

Ohne passendes Herstellerzertifikat lautet der neutrale Status sinngemäß:

`Kein passendes Herstellerzertifikat – Validierung, Freigabe und Dokumentation durch Betreiber.`

Justierung, Systemkalibrierung, Messbetrieb und zertifizierte Logausgaben bleiben möglich. Externe Validierungsberichte, SOPs oder Freigabedokumente werden bewusst nicht im Gerät gespeichert.

## Altbestand

Für alte Systemkalibrierungsanfragen ohne `.tpfctx` wird nur dann ein ausdrücklich als Aktivierungs-Fallback gekennzeichneter Kontext nachgetragen, wenn Firmwareversion und Build-ID des signierten Pakets exakt der aktuell laufenden Firmware entsprechen. Andernfalls wird kein historischer SHA-256 erfunden; die Anzeige nennt den Kontext als nicht vorhanden.
