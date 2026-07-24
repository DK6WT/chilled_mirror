# TP-3000 signierte Datenformate V1

Stand: 2026-07-12  
Status: verbindliche Formatgrundlage für Firmware, Windows-KeyTool und Web-Prüfer

## 1. Ziel

Das Format bildet eine durchgängige Prüfkette:

1. Hersteller-Root
2. Gerätezertifikat
3. signierte Geräte-Werksjustierung
4. signierte Sensorkopf-Kalibrierung
5. signiertes Firmware-Manifest
6. signierte Messkonfiguration
7. Gerätesignatur über einen unveränderlichen CSV-Snapshot

Der normale zertifizierte Download besteht nur aus:

```text
YYMMDDD.CSV
YYMMDDD.TPSIG
```

Die `.TPSIG` ist selbständig prüfbar und enthält die benötigten Zertifikate und
signierten Manifeste. Eine dritte Begleitdatei ist nicht erforderlich.

## 2. Kryptografische Grundregeln

- Kurve: NIST P-256 / secp256r1
- Hash: SHA-256
- ECDSA-Signatur: IEEE P1363, exakt 64 Byte (`R || S`)
- Public Key: DER SubjectPublicKeyInfo (SPKI)
- Key-ID: erste 8 Byte von `SHA256(SPKI_DER)`, 16 Hex-Zeichen uppercase
- SHA-256-Hex: 64 Hex-Zeichen lowercase
- JSON ist nur Transportcontainer
- Signiert wird ausschließlich der definierte kanonische Binärdatensatz
- Strings: UTF-8, Unicode NFC, keine Steuerzeichen
- Zeit: UTC als `int64_le` Unix-Sekunden
- Datum ohne Uhrzeit: `uint32_le` im Format `YYYYMMDD`
- Alle Ganzzahlen haben feste Breite; keine Fließkommazahlen werden signiert

### 2.1 Kanonische Schreibfunktionen

```text
WriteU8(v)       = 1 Byte
WriteU16(v)      = uint16 little-endian
WriteU32(v)      = uint32 little-endian
WriteU64(v)      = uint64 little-endian
WriteI32(v)      = int32 little-endian
WriteI64(v)      = int64 little-endian
WriteBytes(b)    = uint32_le Länge + b
WriteUtf8(text)  = WriteBytes(UTF8(NFC(text)))
WriteHash32(h)   = exakt 32 rohe Byte, ohne Längenfeld
```

Base64- und JSON-Escaping sind niemals Bestandteil der signierten Bytes.

## 3. Vertrauenskette und Schlüsselrollen

Der Hersteller-Root bleibt möglichst offline. Für operative Signaturen werden
root-zertifizierte Rollenschlüssel verwendet:

```text
CALIBRATION   Geräte- und Kopfkalibrierungen
FIRMWARE      Firmwarefreigaben
```

Die bestehende Gerätezertifizierung V2 darf während der Migration weiterhin
direkt vom Hersteller-Root signiert werden. Eine spätere Rolle
`DEVICE_CERTIFICATE` ist im Rollenformat reserviert, aber für V1 nicht nötig.

## 4. Rollenschlüssel-Zertifikat `.tprole`

Format:

```text
TP3000-SIGNING-ROLE-CERTIFICATE-1
```

Zulässige Rollen:

```text
CALIBRATION
FIRMWARE
DEVICE_CERTIFICATE
```

### JSON-Felder

```text
Format
Product
Role
KeyName
KeyId
PublicKeySpkiBase64
IssuerKeyId
CertificateSerial
IssuedUtc
NotBeforeUtc
NotAfterUtc
ManifestSha256
SignatureAlgorithm
SignatureEncoding
RootSignatureBase64
```

### Kanonischer Datensatz

```text
WriteUtf8(Format)
WriteUtf8(Product)                    // "TP-3000"
WriteUtf8(Role)
WriteUtf8(KeyName)
WriteUtf8(KeyId)
WriteBytes(PublicKeySpkiDer)
WriteUtf8(IssuerKeyId)
WriteUtf8(CertificateSerial)
WriteI64(IssuedUtc)
WriteI64(NotBeforeUtc)
WriteI64(NotAfterUtc)
```

`ManifestSha256 = SHA256(KanonischerDatensatz)`; der Hersteller-Root signiert
diesen Hash.

## 5. Geräte-Werksjustierung

### 5.1 Anfrage `.tpdcalreq`

Format:

```text
TP3000-DEVICE-CALIBRATION-REQUEST-1
```

Die Anfrage wird vom internen Geräteschlüssel signiert. Das KeyTool prüft zuerst
das eingebettete Gerätezertifikat und anschließend die Gerätesignatur.

### Nutzdaten und Einheiten

```text
DeviceSerial                         5 ASCII-Ziffern
DeviceKeyId                          16 Hex uppercase
DeviceCertificateSerial             32 Hex uppercase
RequestId                            32 Hex uppercase
CreatedUtc                           int64 Unix-Sekunden
CalibrationDateYmd                  uint32 YYYYMMDD
ReferenceDateYmd                    uint32 YYYYMMDD
RefLowScaled                        int32, Ohm × 100000
RefHighScaled                       int32, Ohm × 100000
ChannelALowCorrectionScaled         int32, Ohm × 100000
ChannelAHighCorrectionScaled        int32, Ohm × 100000
ChannelBLowCorrectionScaled         int32, Ohm × 100000
ChannelBHighCorrectionScaled        int32, Ohm × 100000
FirmwareVersion                     z. B. "0.50.1"
FirmwareBuildId                     z. B. "0.50.1_15"
```

### Kanonischer Datensatz

```text
WriteUtf8(Format)
WriteUtf8("TP-3000")
WriteUtf8(DeviceSerial)
WriteUtf8(DeviceKeyId)
WriteUtf8(DeviceCertificateSerial)
WriteUtf8(RequestId)
WriteI64(CreatedUtc)
WriteU32(CalibrationDateYmd)
WriteU32(ReferenceDateYmd)
WriteI32(RefLowScaled)
WriteI32(RefHighScaled)
WriteI32(ChannelALowCorrectionScaled)
WriteI32(ChannelAHighCorrectionScaled)
WriteI32(ChannelBLowCorrectionScaled)
WriteI32(ChannelBHighCorrectionScaled)
WriteUtf8(FirmwareVersion)
WriteUtf8(FirmwareBuildId)
```

### 5.2 Freigabe `.tpdcal`

Format:

```text
TP3000-DEVICE-CALIBRATION-1
```

Die Freigabe enthält alle obigen Nutzdaten sowie:

```text
SourceRequestManifestSha256
Approval.CalibrationId
Approval.CalibrationDateYmd
Approval.ValidFromUtc
Approval.ValidUntilUtc
Approval.CalibrationIntervalMonths
Approval.CalibrationLaboratory
Approval.CalibrationOperator
Approval.CertificateReference
Approval.ReasonForCalibration
ApprovedUtc
SignerRoleCertificate
SignerKeyId
ManifestSha256
CalibrationSignatureBase64
```

Der Kalibrierzeitraum ist signierter Inhalt. Sein Ablauf ändert die kryptografische Gültigkeit nicht und darf Messung, Logging oder Export nicht blockieren. Die spätere Log-Verifikation bewertet den Messzeitraum unabhängig als `INNERHALB`, `TEILWEISE AUSSERHALB`, `AUSSERHALB` oder `NICHT BEURTEILBAR`.

Der signierte Datensatz enthält die Nutzdaten, den rohen 32-Byte-Hash der
Anfrage, `ApprovedUtc`, `SignerKeyId` und den rohen 32-Byte-Hash des
Rollenschlüssel-Zertifikats.

## 6. Sensorkopf-Kalibrierung

### 6.1 Anfrage `.tphcalreq`

Format:

```text
TP3000-HEAD-CALIBRATION-REQUEST-1
```

### Nutzdaten und Einheiten

```text
DeviceSerial                         5 ASCII-Ziffern
DeviceKeyId                          16 Hex uppercase
DeviceCertificateSerial             32 Hex uppercase
RequestId                            32 Hex uppercase
CreatedUtc                           int64 Unix-Sekunden
HeadType                             exakt "STP-XXXX"
HeadSerial                           uint32, Anzeige fünfstellig
CalibrationDateYmd                  uint32 YYYYMMDD
CalibrationTimeHms                  uint32 HHMMSS
R0MirrorScaled                      int32, Ohm × 10000
R0AmbientScaled                     int32, Ohm × 10000
Mirror2PointActive                  uint8 0/1
MirrorSet1_mC                       int32
MirrorActual1_mC                    int32
MirrorSet2_mC                       int32
MirrorActual2_mC                    int32
Ambient2PointActive                 uint8 0/1
AmbientSet1_mC                      int32
AmbientActual1_mC                   int32
AmbientSet2_mC                      int32
AmbientActual2_mC                   int32
DewFrostOffset_mC                   int32
PidKp                               uint16
PidKi_x1000                         int32
PidKd_x1000                         int32
ControlInterval_ms                  uint16
HBridgeDeadtime_ms                  uint8
FanPercent                          uint8
OpticalTarget_x10                   uint16
PeltierCurrentLimit_mA              uint16
FirmwareVersion
FirmwareBuildId
```

Die Reihenfolge des kanonischen Datensatzes entspricht exakt der obigen
Reihenfolge, jeweils mit der festgelegten Schreibfunktion.

### 6.2 Freigabe `.tphcal`

Format:

```text
TP3000-HEAD-CALIBRATION-1
```

Die Struktur entspricht der Geräte-Werksjustierung, wird aber mit den
Sensorkopf-Nutzdaten gebildet und vom Rollenschlüssel `CALIBRATION` signiert.

Nach jeder Geräte- oder Kopfkalibrierung wird eine neue Freigabedatei erzeugt. Der private `CALIBRATION`-Rollenschlüssel bleibt dabei gleich und wird nur bei geplanter Rotation, Ablauf oder Kompromittierung ersetzt.

## 7. Firmwarefreigabe `.tpfw`

Format:

```text
TP3000-FIRMWARE-MANIFEST-1
```

### JSON-Felder

```text
Format
Product
FirmwareVersion
FirmwareBuildId
BuildDateYmd
TargetHardware
ImageFileName
ImageFileSize
ImageSha256
SourceRevision
ReleaseStatus
CreatedUtc
SignerRoleCertificate
SignerKeyId
ManifestSha256
FirmwareSignatureBase64
```

`ReleaseStatus` ist für V1 exakt einer der Werte:

```text
TEST
RELEASE
REVOKED
```

### Kanonischer Datensatz

```text
WriteUtf8(Format)
WriteUtf8("TP-3000")
WriteUtf8(FirmwareVersion)
WriteUtf8(FirmwareBuildId)
WriteU32(BuildDateYmd)
WriteUtf8(TargetHardware)            // "Teensy 4.1"
WriteUtf8(ImageFileName)
WriteU64(ImageFileSize)
WriteHash32(ImageSha256)
WriteUtf8(SourceRevision)
WriteUtf8(ReleaseStatus)
WriteI64(CreatedUtc)
WriteUtf8(SignerKeyId)
WriteHash32(SignerRoleCertificate.ManifestSha256)
```

Das Manifest signiert den exakten Hash der ausgelieferten `.hex`- oder
`.bin`-Datei. Ohne Secure Boot beweist dies die Herstellerfreigabe der Datei,
aber noch nicht automatisch, dass exakt dieses Abbild unverändert im laufenden
Gerät ausgeführt wird. Der Web-Prüfer muss diesen Unterschied sichtbar anzeigen.

## 8. Messkonfiguration

Format:

```text
TP3000-MEASUREMENT-CONFIG-1
```

V1 enthält mindestens:

```text
SdOutputInterval_ms                  uint32
SdOutputFilter_s                     uint16
SdWriteInterval_ms                   uint32
DiagnosticDataEnabled                uint8
CsvHeaderEnabled                     uint8
AdcMeasurementFilterMode             uint8
Adc1SfocalMode                       uint8
PidKp                                uint16
PidKi_x1000                          int32
PidKd_x1000                          int32
ControlInterval_ms                   uint16
HBridgeDeadtime_ms                   uint8
FanPercent                           uint8
OpticalTarget_x10                    uint16
PeltierCurrentLimit_mA               uint16
AlmemoChannel1Enabled                uint8
AlmemoChannel1Address                uint8
AlmemoChannel1Channel                uint8
AlmemoChannel1Role                   uint8
AlmemoChannel2Enabled                uint8
AlmemoChannel2Address                uint8
AlmemoChannel2Channel                uint8
AlmemoChannel2Role                   uint8
```

Der Hash dieses kanonischen Datensatzes wird in `.TPSIG` gebunden. Die lesbaren
Werte werden zusätzlich in der JSON-Datei eingebettet.

## 9. Zertifizierte Logsignatur `.TPSIG`

### 9.1 Historisches V1-Format

Die vorbereitete, aber nie als vollständiger Logger aktivierte Kennung bleibt
für Dokumentations- und spätere Importzwecke reserviert:

```text
TP3000-LOG-SIGNATURE-1
```

V1 enthielt noch keine Systemkalibrierung und kein Recovery-Kennzeichen. Die
Kennung wird nicht nachträglich verändert.

### 9.2 Aktives V2-Format ab Build 0.50.1_35

```text
TP3000-LOG-SIGNATURE-2
```

Jeder unveränderliche Logabschnitt besitzt einen nummerierten Dateinamen:

```text
YYMMDD_001.CSV
YYMMDDD_001.CSV          // Diagnoseformat
YYMMDD_001.TPSIG
```

Die `.TPSIG` ist selbständig prüfbar. Ihr eingebetteter Kontext enthält:

```text
DeviceCertificate
DeviceCalibration
HeadCalibration
SystemCalibration
Firmware                 // DEVELOPMENT-Identität oder spätere Freigabe
MeasurementConfiguration
```

Das zur Laufzeit verwendete Kontext-JSON wird beim Start des Abschnitts auf SD
eingefroren, per SHA-256 gebunden und nach erfolgreicher Erstellung der
`.TPSIG` wieder entfernt. Für die Prüfung werden nur CSV und `.TPSIG` benötigt.

### JSON-Felder des V2-Envelopes

```text
Format
Product
LogFileName
LogFileSize
LogFileSha256
SnapshotStartUtc
SnapshotEndUtc
CreatedUtc
DeviceSerial
DeviceKeyId
DeviceCertificateManifestSha256
DeviceCalibrationManifestSha256
HeadCalibrationManifestSha256
SystemCalibrationManifestSha256
FirmwareManifestSha256
MeasurementConfigurationSha256
ContextSha256
RecoveredAfterUncleanShutdown
Context
ManifestSha256
SignatureAlgorithm
SignatureEncoding
DeviceSignatureBase64
```

`SnapshotEndUtc` ist der Zeitpunkt des letzten bestätigten SD-Flushs.
`CreatedUtc` ist der Zeitpunkt, an dem der Nachweis versiegelt wurde. Bei einer
Wiederherstellung nach Stromausfall können zusätzliche, vollständig mit
Zeilenumbruch abgeschlossene CSV-Zeilen zwischen diesen Zeitpunkten liegen; der
CSV-Inhalt selbst bleibt die maßgebliche Zeitquelle.

### Kanonischer V2-Datensatz

```text
WriteUtf8(Format)                         // TP3000-LOG-SIGNATURE-2
WriteUtf8("TP-3000")
WriteUtf8(LogFileName)
WriteU64(LogFileSize)
WriteHash32(LogFileSha256)
WriteI64(SnapshotStartUtc)
WriteI64(SnapshotEndUtc)
WriteI64(CreatedUtc)
WriteUtf8(DeviceSerial)
WriteUtf8(DeviceKeyId)
WriteHash32(DeviceCertificateManifestSha256)
WriteHash32(DeviceCalibrationManifestSha256)
WriteHash32(HeadCalibrationManifestSha256)
WriteHash32(SystemCalibrationManifestSha256)
WriteHash32(FirmwareManifestSha256)
WriteHash32(MeasurementConfigurationSha256)
WriteHash32(ContextSha256)
WriteU8(RecoveredAfterUncleanShutdown ? 1 : 0)
```

Der interne Geräteschlüssel signiert den SHA-256 dieses Datensatzes mit
ECDSA P-256. `DeviceSignatureBase64` enthält exakt 64 Byte `R || S` im
IEEE-P1363-Format.

Solange kein Hersteller-Firmwaremanifest importiert ist, bleibt
`FirmwareManifestSha256` null und der eingebettete Firmwarestatus sichtbar
`DEVELOPMENT`. Das wird nicht als Herstellerfreigabe ausgegeben.

## 10. Einfacher SHA-256-Modus

Im Modus `SHA-256` entstehen nummerierte, unveränderliche Abschnitte:

```text
YYMMDD_001.CSV
YYMMDD_001.CSV.sha256
```

Die Begleitdatei enthält genau eine GNU-kompatible Zeile:

```text
<64 lowercase hex> *YYMMDD_001.CSV
```

Ein aktiver Web-Snapshot schreibt zuerst den RAM-Puffer auf SD, schließt und
versiegelt den Abschnitt und lädt anschließend die unveränderliche CSV-Datei.

## 11. Journal und Wiederherstellung

Während eines offenen SHA-256- oder zertifizierten Abschnitts liegt ein
CRC-32-gesichertes Journal unter `/LOG/TPLOG.HST`. Es enthält mindestens:

- CSV-Pfad und nummerierte Abschnittskennung,
- zuletzt bestätigte Dateigröße,
- Start- und letztes bestätigtes UTC-Datum,
- Hashes der eingefrorenen Nachweise,
- Hash der Messkonfiguration und des Kontextes.

Das Journal wird erst nach erfolgreichem CSV-Flush und Schließen der Datei
fortgeschrieben. Nach einem unsauberen Neustart werden Bytes hinter der letzten
bestätigten Position erneut gelesen. Vollständige, mit `LF` abgeschlossene
Zeilen bleiben erhalten; ein unvollständiges Dateiende wird abgeschnitten.
Danach wird der Abschnitt mit
`RecoveredAfterUncleanShutdown = true` versiegelt.

Vor jeder zertifizierten Signatur wird der eingefrorene Kontext erneut gehasht.
Fehlt er oder weicht sein SHA-256 ab, wird keine `.TPSIG` erzeugt.

## 12. Zustandswechsel, Menü und Download

Ein aktiver Abschnitt wird beendet und ein neuer begonnen, wenn sich mindestens
einer dieser Zustände ändert:

- Integritätsmodus,
- Kalendertag oder Zeitbasis wird gültig/ungültig,
- normales/diagnostisches CSV-Format,
- Gerätezertifikat,
- Gerätejustierung,
- Kopf oder Kopfjustierung,
- Systemkalibrierung einschließlich externer PDF-Bindung,
- Firmwareidentität/Firmwaremanifest,
- messrelevante Konfiguration.

Noch nicht auf SD geschriebene Pufferwerte werden bei einem solchen
Provenienzwechsel bewusst verworfen, damit niemals zwei verschiedene Zustände
in derselben signierten Datei vermischt werden.

```text
Setup → SD-Karte → Log-Integrität
Aus
SHA-256
Zertifiziert
```

`Zertifiziert` ist auswählbar, wenn Gerätezertifikat, signierte
Gerätejustierung, signierte Kopfjustierung und die daran gebundene signierte
Systemkalibrierung gültig sind. Das Firmwaremanifest ist im Entwicklungsstand
noch informativ und nicht blockierend.

Unter `/download` stehen die Filter `Messdaten`, `Seriellog` und `Nachweise`
zur Verfügung. `.TPSIG` und `.sha256` können separat heruntergeladen werden.
Ein Snapshot einer aktiven Integritätsdatei versiegelt den aktuellen Abschnitt
zuerst vollständig.

## 13. Übergang von TEST zu Produktion

Die Texte `TEST-ROOT` und `Wegwerf-Test-Root` werden erst entfernt, nachdem:

1. ein neuer Produktions-Root auf dem Produktionsrechner erzeugt wurde,
2. Rollen-Zertifikate für `CALIBRATION` und `FIRMWARE` ausgestellt wurden,
3. der Produktions-Root in der Firmware eingebettet wurde,
4. Gerät `00001` ein neues Gerätezertifikat der Produktionskette erhalten hat,
5. alle Format-Testvektoren zwischen Tool, Firmware und Web-Prüfer übereinstimmen.

Ein Entwicklungsschlüssel darf nicht lediglich umbenannt und als Produktion
weiterverwendet werden.

## Systemkalibrierung – Build 0.50.1_26

### Anfrage

`TP3000-SYSTEM-CALIBRATION-REQUEST-1`

Kanonische Reihenfolge:

1. gemeinsamer Anfragepräfix
2. `CalibrationDateYmd`
3. `HeadType`
4. `HeadSerial`
5. `DeviceAdjustmentCalibrationId`
6. `DeviceAdjustmentManifestSha256` als 32 Byte
7. `HeadAdjustmentCalibrationId`
8. `HeadAdjustmentManifestSha256` als 32 Byte
9. `FirmwareVersion`
10. `FirmwareBuildId`

### Signiertes Paket

`TP3000-SYSTEM-CALIBRATION-1`

Das Paket nutzt dieselben Systemwerte und den V3-Freigabesuffix. Die Systemkalibrierung ist nur aktiv, wenn Geräte-SN, Kopftyp, Kopf-SN sowie Calibration-ID und Manifest beider aktuell aktiver Justierungen übereinstimmen.

Kalibrierumfang:

- `AS_FOUND`: nur As Found, Stage 1.
- `AS_LEFT`: nur As Left, Stage 2.
- `AS_FOUND_AS_LEFT_NO_ADJUSTMENT`: gemeinsame Messreihe ohne Justierung, Scope-Byte 4 und Stage 3.
- `BEFORE_AFTER_ADJUSTMENT`: getrennte Messreihen vor/nach Justierung, Scope-Byte 5 und Stage 1/2.
- `AS_FOUND_AS_LEFT`: lesbares Altformat für getrennte Vor-/Nach-Messreihen, Scope-Byte 3.


## Offline-Kalibrierscheintransport TP3Q3 – Build 0.50.1_27

Der druckbare Systemkalibrierschein verwendet `TP3Q3:` + Base45(ZLIB(JSON)). Das Envelope-Format lautet `TP3000-CALIBRATION-QR-3` und enthält die vollständigen signierten Pakete `Device`, `Head` und `System`.

Der gemeinsame Dokumenthash ist SHA-256 über:

```text
TP3000-CAL-DOC-2
D=<ManifestSha256 Gerätejustierung>
H=<ManifestSha256 Kopfjustierung>
S=<ManifestSha256 Systemkalibrierung>
```

`QrPartCount` und `QrPartIndex` erlauben genau einen oder zwei zusammengehörige QR-Codes.

## 10 Zusätzlicher Ein-Datei-Container TP3000-LOG-CONTAINER-1

Ab Build `0.50.1_36` bleibt die Ausgabe aus `.CSV` und `.TPSIG` vollständig
bestehen. Zusätzlich wird eine gleichnamige `.TPLOG` erzeugt. Sie enthält exakt
dieselben CSV- und TPSIG-Bytes in einem festen binären Container.

Die Containerstruktur ist nicht Ersatz für die Gerätesignatur. Ihre CRC-32- und
SHA-256-Felder sichern Framing und Transportintegrität; die Authentizität wird
weiterhin durch die eingebettete `TP3000-LOG-SIGNATURE-2` nachgewiesen.

Das vollständige Layout, die Little-Endian-Felder und der Recovery-Ablauf sind
in `TPLOG_CONTAINER_V0.50.1_36.md` festgelegt.


## Build 0.50.1_37 – getrennte zertifizierte Ausgabemodi

`TP3000-LOG-SIGNATURE-2` und `TP3000-LOG-CONTAINER-1` bleiben byte- und
strukturkompatibel. Neu ist nur die Auswahl der Ausgabeverpackung: Modus 2 lässt CSV und
TPSIG separat bestehen; Modus 3 entfernt beide nach erfolgreicher TPLOG-Erzeugung.
