# Source Validation – TP-3000 Build 0.50.1_31

- Basisarchiv: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_8.zip`
- Zielarchiv: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_9.zip`
- Interne Version: `0.50.1`
- Build-ID: `0.50.1_31`

## Geänderte Programmdateien

- `TPethernet.ino`
- `TPsignedData.h`

Die Identitätsimplementierung, Zertifikatsparser, Zertifikatsspeicher,
Kalibrierparser, ADC-, Regel-, Safety- und Messdateien wurden nicht geändert.

## Verifikation

- eingebettete TP3C1- und pako-JavaScript-Blöcke syntaktisch mit Node.js geprüft
- realistische Referenzdaten verwendet, bei denen `.tpdcal` kein eingebettetes
  `DeviceCertificate` besitzt
- aktives `.tpcert` separat an den Encoder übergeben
- TP3C1-Referenzhashes für Envelope, ZLIB und beide Binärteile bytegenau erreicht
- Größen 1176 / 1126 / 581 / 581 Byte und Deep-Link-Länge 907 / 907 bestätigt
- Dateien außerhalb der dokumentierten Änderung gegen die Basis verglichen

Ein vollständiger Teensyduino-Build ist weiterhin auf dem vorgesehenen
Windows-/Teensyduino-System auszuführen.
