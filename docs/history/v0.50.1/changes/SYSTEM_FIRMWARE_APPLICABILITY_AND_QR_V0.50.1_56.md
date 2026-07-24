# TP-3000 V0.50.1_56 – Firmwareanwendbarkeit und QR-Nachweis

## Grundregel

Die Gültigkeit des signierten Kalibrierscheins als historisches Dokument und seine Anwendbarkeit auf den aktuell laufenden Gerätezustand sind getrennte Aussagen.

- Der Kalibrierschein bleibt echt und kryptografisch prüfbar.
- Der bei der Systemkalibrierung verwendete Firmware-SHA-256 ist gerätesigniert dokumentiert.
- Weicht der aktuelle SHA-256 später ab, ist die Anwendbarkeit der Systemkalibrierung auf den aktuellen Gerätezustand nicht mehr bestätigt und wird als möglicherweise nicht mehr gültig angezeigt.
- Messbetrieb und zertifizierte Ausgaben werden nicht gesperrt.

## Zustände

| Zustand | Bedeutung | Darstellung |
|---|---|---|
| `EXACT_MATCH` | aktueller Version/Build/SHA-256 entsprechen exakt dem Kalibrierstand | grün |
| `FIRMWARE_CHANGED` | aktueller SHA-256 bzw. Firmwareidentität weicht ab | rot, möglicherweise nicht mehr gültig |
| `NOT_COMPARABLE` | historischer Kontext oder aktueller Hash fehlt | gelb, nicht beurteilbar |

Der Herstellerzertifikatsstatus ist davon unabhängig. Eine kundenspezifische Firmware ohne Herstellerzertifikat kann bei exakt dokumentiertem SHA-256 regulär kalibriert werden.

## Nachzertifizierung

Nach einer Firmwareänderung kann der Betreiber bzw. das Kalibrierlabor die metrologische Kompatibilität bewerten. Anschließend wird eine neue signierte Systemkalibrierung für den aktuellen Firmwarestand erzeugt. Dadurch wird der neue SHA-256 wieder eindeutig mit dem Kalibriernachweis verbunden.

## Anzeigen

Die Bewertung wird ausgegeben in:

- Web `Info / Gültigkeit`,
- Web `Justierung / Systemkalibrierung verwalten`,
- Übersicht aktueller Kalibrierzertifikate,
- druckbarem internen oder externen Systemkalibrierschein,
- TFT `Info / Gültigkeit`,
- TP3C1 V0.4 und TP3Q3 QR-4,
- zertifizierten Logkontexten.

## Zertifizierte Logs

`TP3000-LOG-CONTEXT-2` enthält den vollständigen gerätesignierten Firmwarekontext und eines der Felder:

- `EXACT_MATCH`
- `FIRMWARE_CHANGED_POSSIBLY_INVALID`
- `NOT_COMPARABLE`

Die Logerzeugung bleibt möglich; der Nachweis verschweigt die Abweichung jedoch nicht.

## Externes PDF

Das externe Original-PDF bleibt bytegenau unverändert. Die Firmwarebewertung wird in der TP-3000-Hülle, im QR-Nachweis und im Logkontext dokumentiert. Externe Validierungsunterlagen kundenspezifischer Firmware werden nicht im Gerät verwaltet.
