# Source Validation – V0.50.1_55

## Geprüft

- Build-ID in `TPsignedData.h` auf `0.50.1_55`.
- Neuer fester `.tpfctx`-Datensatz mit Größen-`static_assert` (393 Byte), CRC-32, kanonischem SHA-256 und Gerätesignatur.
- Bindung an `SourceRequestId` und `SourceRequestManifestSha256`.
- Herstellerzertifikat ist nicht mehr Bestandteil der blockierenden Kalibrier- oder Log-Bereitschaft; der Ist-Firmwarehash bleibt erforderlich und wird gespeichert.
- Systemstatus übernimmt Anfrage-ID, Anfrage-Manifest, Firmwareversion und Build aus dem signierten `.tpscal`.
- Externer PDF-Pfad verwendet denselben System-Firmwarekontext; Original-PDF und PDF-SHA-256 bleiben unverändert.
- Neuer JSON-Endpunkt `/calibration-firmware-context.json` und Webdarstellung geprüft.
- Zertifikats-JavaScript aus dem eingebetteten Rohstring extrahiert und mit Node.js syntaktisch geprüft.
- Render-Test für einen externen PDF-Kalibrierschein ohne Hersteller-Firmwarezertifikat bestanden; Betreiberhinweis, unverändertes Original-PDF und vollständige Firmware-SHA-256-Zeile wurden bestätigt.
- `TPsystemFirmwareContext.cpp` mit Host-Stubs sowohl normal als auch mit `__IMXRT1062__` unter `-Wall -Wextra -Werror` kompiliert.
- Funktionaler Host-Rundlauf: Kontext schreiben, vollständig zurücklesen, CRC/Manifest/Gerätesignatur prüfen und aktuellen Firmwarehash vergleichen; eine manipulierte Kontextdatei wurde abgelehnt.
- Geänderte Bereitschaftslogik aus `TPsignedData.cpp` mit Host-Stubs unter `-Wall -Wextra -Werror` kompiliert.
- Klammer-/Rohstringprüfung aller geänderten C/C++-Quelldateien bestanden.
- Keine Änderung der kanonischen `.tpdcal`, `.tphcal` oder `.tpscal`-Formate und kein KeyGen-Update erforderlich.
- TP3C1/TP3Q3 bleiben in diesem Stand unverändert; der zusätzliche Firmwarekontext ist ein separat gerätesignierter, an die Systemanfrage gebundener SD-Nachweis.

## Nicht ausgeführt

- vollständiger Teensyduino-Build und Linkerprüfung,
- reale SD-Schreib-/Rückleseprüfung des `.tpfctx`,
- Hardwaretest mit neuer eigener und externer Systemkalibrierung,
- reale zertifizierte CSV-/TPSIG-/TPLOG-Ausgabe ohne Hersteller-Firmwarezertifikat.
