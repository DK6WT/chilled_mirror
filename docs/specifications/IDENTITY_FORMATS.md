# TP-3000 Geräteidentitätsformate – Firmware V0.50.1 / KeyTool V0.3.0

Diese Datei beschreibt die kanonischen Bytes, die zwischen TP-3000-Firmware
und TP3000KeyTool V0.3.0 abgestimmt sind. JSON ist nur der Transportcontainer.

## Gemeinsame Regeln

- Kurve: NIST P-256 / secp256r1
- Hash: SHA-256
- Signatur: ECDSA IEEE-P1363, exakt 64 Byte `R || S`
- Public Key: DER SubjectPublicKeyInfo, bei P-256 exakt 91 Byte
- Strings: UTF-8; alle aktuell verwendeten Werte sind ASCII
- Hex-Kennungen uppercase, SHA-256-Hex lowercase
- `WriteBytes`: `uint32_le` Länge, danach Daten
- `WriteUtf8`: UTF-8-Bytes über `WriteBytes`
- Zeit: `int64_le` Unix-Sekunden

## `.tpreq` – `TP3000-CERTIFICATE-REQUEST-1`

Kanonische Reihenfolge:

```text
WriteUtf8("TP3000-CERTIFICATE-REQUEST-1")
WriteUtf8("TP-3000")
WriteUtf8(RequestedDeviceSerial)
WriteUtf8(DeviceKeyId)
WriteBytes(DevicePublicKeySpkiDer)
WriteUtf8(RequestId)
WriteUtf8(FirmwareVersion)
int64_le CreatedUtcUnixSeconds
```

```text
ManifestSha256 = SHA256(canonical)
DeviceSelfSignature = ECDSA-P256-SignHash(DevicePrivateKey, ManifestSha256)
```

## `.tpcert` – `TP3000-DEVICE-CERTIFICATE-2`

Kanonische Reihenfolge:

```text
WriteUtf8("TP3000-DEVICE-CERTIFICATE-2")
WriteUtf8("TP-3000")
WriteUtf8(DeviceSerial)
WriteUtf8(DeviceKeyId)
WriteBytes(DevicePublicKeySpkiDer)
WriteUtf8(IssuerKeyId)
WriteUtf8(CertificateSerial)
int64_le IssuedUtcUnixSeconds
WriteUtf8(RequestId)
WriteUtf8(RequestManifestSha256)
WriteUtf8(RequestFirmwareVersion)
```

```text
ManifestSha256 = SHA256(canonical)
RootSignature = ECDSA-P256-SignHash(ManufacturerRootPrivateKey, ManifestSha256)
```

## Key-ID

```text
Fingerprint = SHA256(SPKI_DER)
KeyId = erste 8 Bytes als 16 uppercase Hex-Zeichen
```

Die Key-ID ist nur eine kompakte Anzeige. Die Zertifikatsprüfung vergleicht
zusätzlich den vollständigen 91-Byte-SPKI-Public-Key.

## Firmwareinterne SPKI-Darstellung

Der unkomprimierte raw P-256-Public-Key besteht aus `X || Y`, jeweils 32 Byte
Big-Endian. Der feste SPKI-Präfix lautet:

```text
30 59 30 13 06 07 2A 86 48 CE 3D 02 01
06 08 2A 86 48 CE 3D 03 01 07 03 42 00 04
```

Danach folgen die 64 raw Public-Key-Bytes.

## Eingebauter Test-Vertrauensanker

```text
Root-Key-ID: 6CC674A19758D153
```

Dieser Root ist ein offengelegter Wegwerf-Testschlüssel. Er darf nicht als
Produktiv-Vertrauensanker verwendet werden.
