# TP3C1 kompakter Offline-Transport – V0.4

## Geltungsbereich

TP3C1 V0.4 erweitert V0.3 um den gerätesignierten Firmwarekontext der Systemkalibrierung und um eine klar getrennte, nur auf den Exportzeitpunkt bezogene Anwendbarkeitsinformation.

Die fünf bisherigen Signaturketten bleiben unverändert:

- Gerätezertifikat,
- optionales Labor-Rollenzertifikat,
- Gerätejustierung,
- Kopfjustierung,
- Systemkalibrierung.

Zusätzlich wird die ECDSA-P-256-Gerätesignatur des Firmwarekontexts geprüft.

## Envelope

| Feld | Inhalt | V0.4-Wert |
|---:|---|---:|
| 1 | Schema-Major | 1 |
| 2 | Schema-Minor | 2 |
| 3 | RequiredFeatures | 63 plus Bit 6 und 7 bei Firmwarekontext |
| 4–9 | bisherige V0.3-Inhalte | unverändert |
| 10 | gerätesignierter Firmwarekontext | optional |
| 11 | Firmwareanwendbarkeit beim Export | 0 nicht beurteilbar, 1 exakt, 2 verändert |
| 12 | aktueller Firmware-SHA-256 beim Export | optional, 32 Byte |

Bit 6 verlangt die Prüfung des gerätesignierten Firmwarekontexts. Bit 7 verlangt die Auswertung des aktuellen Firmwarevergleichs zum Exportzeitpunkt.

## Firmwarekontext-Unterblock, Envelope-Feld 10

| Feld | Wire Type | Inhalt |
|---:|---:|---|
| 1 | 0 | Kontextformatversion 1 |
| 2 | 0 | Erfassungsmodus: 1 Anfrage, 2 Aktivierungs-Fallback |
| 3 | 0 | Erfassungszeit UTC |
| 4 | 2 | Firmwareversion |
| 5 | 2 | Build-ID |
| 6 | 0 | ImageBase |
| 7 | 0 | ImageSize |
| 8 | 2 | Firmware-SHA-256 bei Systemkalibrierung |
| 9 | 0 | Herstellerstatus: 0 kein Zertifikat, 1 gültig, 2 unpassend, 3 ungültig |
| 10 | 2 | Firmwarezertifikat-ID, optional |
| 11 | 2 | Hersteller-Root-Key-ID, optional |
| 12 | 0 | Zertifizierungszeit UTC, optional |
| 13 | 2 | SourceRequestId, 16 Byte |
| 14 | 2 | SourceRequestManifestSha256, 32 Byte |
| 15 | 2 | Kontext-Manifest-SHA-256, 32 Byte |
| 16 | 2 | Gerätesignatur IEEE-P1363, 64 Byte |

Der Verifier rekonstruiert die kanonische Bytefolge `TP3000-SYSTEM-FIRMWARE-CONTEXT-1`, prüft den Manifest-SHA-256 und anschließend die Gerätesignatur mit dem öffentlichen Schlüssel aus dem Gerätezertifikat.

## Anwendbarkeitsbewertung

Eine Firmwareabweichung verändert keine der historischen Kalibriersignaturen. Der Verifier zeigt stattdessen getrennt:

- Kalibrierschein kryptografisch gültig,
- Firmwarestand bei Kalibrierung,
- aktueller Firmwarestand zum QR-Export,
- Firmwareabweichung dokumentiert, wenn beide Hashes abweichen; der Kalibrierschein bleibt kryptografisch und historisch gültig.

## Transportkopf

Byte 9 des 18-Byte-Headers:

- `0x41`: Teil 1 von 2
- `0x42`: Teil 2 von 2

ZLIB Level 9, CRC-32, Base38, Deep Link und Fehlerkorrektur M bleiben unverändert.

## TP3Q3

Der vollständige Export verwendet `TP3000-CALIBRATION-QR-4` und enthält:

- `FirmwareContext`,
- `FirmwareApplicabilityAtExport`,
- `CurrentFirmwareSha256AtExport`.

Die beiden Exportzeitfelder sind informativ. Die historische Firmwarebindung wird ausschließlich durch den gerätesignierten Firmwarekontext abgesichert.
