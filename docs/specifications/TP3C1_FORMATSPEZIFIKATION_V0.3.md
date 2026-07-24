# TP3C1 kompakter Offline-Transport – V0.3

## Geltungsbereich

TP3C1 V0.3 ist der kompakte, zweigeteilte Papiertransport des TP-3000. Dieser Entwicklungsstand unterstützt bewusst ausschließlich die aktuell verwendeten signierten Dokumentformate:

- `TP3000-DEVICE-CERTIFICATE-2`
- `TP3000-SIGNING-ROLE-CERTIFICATE-2`
- `TP3000-DEVICE-CALIBRATION-3`
- `TP3000-HEAD-CALIBRATION-3`
- `TP3000-SYSTEM-CALIBRATION-2`

Eine Kompatibilität zu TP3C1 V0.2 oder zu Geräte-/Kopfjustierungen V2 ist in diesem Teststand nicht vorgesehen.

## Unveränderte Grundprinzipien

- protobuf-artiges TLV mit Wire Types 0 und 2
- VarUInt, vorzeichenbehaftete Werte als ZigZag
- SHA-256 und alle vorhandenen ECDSA-P-256-P1363-Signaturen
- einmalige ZLIB-Kompression, Level 9
- genau zwei annähernd gleich große Fragmente
- 18-Byte-Transportkopf je Teil
- CRC-32 je Fragment
- URI-sicheres Base38
- Deep Link `tp3000://verify#TP3C1:<Base38>`
- QR-Fehlerkorrektur M

TP3C1 erzeugt keine Ersatzsignatur. Der Verifier rekonstruiert die ursprünglichen signierten Dokumente und prüft deren vorhandene Signaturen.

## Änderungen gegenüber V0.2

### Envelope

| Feld | Inhalt | V0.3-Wert |
|---:|---|---:|
| 1 | Schema-Major | 1 |
| 2 | Schema-Minor | 1 |
| 3 | RequiredFeatures | 63 / `0x3F` |
| 4 | gemeinsame Firmware-SemVer gepackt | unverändert |
| 5 | Gerätezertifikat | unverändert |
| 6 | Labor-Rollenzertifikat | optional; nur wenn mindestens ein Kalibrierdokument vom Labor signiert ist |
| 7 | Gerätejustierung V3 | erweitert |
| 8 | Kopfjustierung V3 | erweitert |
| 9 | Systemkalibrierung V2 | erweitert |

`RequiredFeatures` Bit 5 kennzeichnet, dass Kalibrierumfang und TPC1-Messpunktdaten in allen drei Kalibrierblöcken berücksichtigt werden müssen.

### Approval-Unterblock von Geräte-, Kopf- und Systemkalibrierung

Die Felder 1 bis 13 bleiben in ihrer bisherigen Bedeutung erhalten.

| Feld | Wire Type | Inhalt |
|---:|---:|---|
| 14 | 0 | Kalibrierumfang |
| 15 | 2 | rohe TPC1-Binärdaten, nur bei Umfang ungleich `NONE` |
| 16 | 0 | nur Systemkalibrierung V2: 0 = TP3000, 1 = EXTERNAL_PDF |

Unterstützte Kalibrierumfänge:

| Text im signierten Dokument | Code |
|---|---:|
| `NONE` | 0 |
| `AS_FOUND` | 1 |
| `AS_LEFT` | 2 |
| `AS_FOUND_AS_LEFT_NO_ADJUSTMENT` | 4 |
| `BEFORE_AFTER_ADJUSTMENT` | 5 |

Der historische Maschinenwert `AS_FOUND_AS_LEFT` / Code 3 wird in V0.3 bewusst nicht akzeptiert.

Bei einem Umfang ungleich `NONE` muss `CalibrationResultsEncoding` exakt `TP3000-CAL-POINTS-1-BASE64URL` lauten. Der Base64URL-Text wird dekodiert und als rohe TPC1-Bytefolge in Feld 15 gespeichert. Beim Rekonstruieren wird dieselbe Bytefolge wieder kanonisch Base64URL-kodiert.

TPC1 wird vor der QR-Erzeugung geprüft:

- Kennung `TPC1`
- Mindestlänge 8 Byte
- 24 Byte je Messpunkt
- 1 bis 40 Messpunkte
- Scope-Byte entspricht Feld 14
- reservierte Headerbytes sind null

### Transportkopf

Byte 9 des 18-Byte-Headers kennzeichnet V0.3 und den Teil:

- `0x31`: Teil 1 von 2
- `0x32`: Teil 2 von 2

Alle übrigen Headerfelder bleiben unverändert: Flags, 8-Byte-Dokumentkennung, Gesamtlänge, Fragmentlänge und CRC-32.

## Größenprüfung

Der Hosttest mit je 40 Messpunkten in Gerätejustierung, Kopfjustierung und Systemkalibrierung sowie maximal langen sechs Fachtexten ergab:

- Envelope: 6762 Byte
- ZLIB: 3170 Byte
- Binärteil 1/2: je 1603 Byte einschließlich Header
- Deep Link: je 2465 Zeichen
- QR-Version 34, 153 × 153 Module, EC M
- Modulbreite bei 115 mm Außenmaß einschließlich Ruhezone: ca. 0,714 mm

Damit bleibt selbst dieser bewusst sehr harte Test innerhalb der QR-Version 40.

## Signierertopologien

Gerätejustierung, Kopfjustierung und Systemkalibrierung dürfen jeweils direkt vom Hersteller-Root oder von einem autorisierten Kalibrierlabor signiert sein. Sind mehrere Dokumente labor-signiert, müssen sie dasselbe Labor-Rollenzertifikat verwenden. Bei ausschließlich Root-signierten Dokumenten entfällt Envelope-Feld 6.
