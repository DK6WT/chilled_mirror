# Kalibrierschein- und Laborimplementierung V0.50.1_20

## Webpfade

```text
/calibration-certificate
/calibration-active-device.json
/calibration-active-head.json
```

Die JSON-Endpunkte liefern die auf SD gespeicherten, bereits kryptografisch geprüften Originalpakete. Lange Fachtexte werden nicht dauerhaft im normalen Mess-RAM gehalten.

## Prüfketten

```text
Gerätezertifikat: Hersteller-Root → Gerät
Herstellerkalibrierung: Hersteller-Root → Kalibrierpaket
Laborkalibrierung: Hersteller-Root → Laborzertifikat → Kalibrierpaket
```

Das Laborzertifikat V2 besitzt `Role=CALIBRATION_LAB` und `Permissions=SIGN_CALIBRATION`. Andere Rollen oder Berechtigungen werden abgelehnt.

## Anfragebindung

```text
SourceRequestExpiresUtc = SourceRequestCreatedUtc + 72 Stunden
SourceRequestCreatedUtc <= ApprovedUtc <= SourceRequestExpiresUtc
```

Bei einem Laborschlüssel muss `ApprovedUtc` zusätzlich innerhalb dessen Zertifikatsgültigkeit liegen.

## Offline-QR

```text
TP3Q1:Base45(ZLIB(UTF8({Format,Device,Head})))
```

Die Hülle ist nur Transport. Nach dem Entpacken verifiziert das PC-Tool erneut das Geräte-/Kopfmanifest, die Signatur und gegebenenfalls das Laborzertifikat gegen den Hersteller-Root.
