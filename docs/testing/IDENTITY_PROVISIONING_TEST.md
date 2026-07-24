# TP-3000 V0.50.1 – Test der Geräteidentität

Diese Fassung dient ausschließlich dazu, den späteren Produktionsablauf
`Teensy → .tpreq → Offline-KeyTool → .tpcert → Teensy` auf realer Hardware zu
prüfen. Sie verwendet den bereits offengelegten Wegwerf-Test-Root.

## Voraussetzungen

- TP3000KeyTool V0.3.0
- vorhandener Wegwerf-Test-Root mit Key-ID `6CC674A19758D153`
- Arduino IDE 2.3.10 / Teensyduino 1.62
- die projektlokale Bibliothek `libraries/TP3000_micro_ecc`
- eingesetzte SD-Karte
- Zugriff auf die TP-3000-Weboberfläche

## Vor dem Flashen

Diese Firmware belegt die bisher freien emulierten EEPROM-Adressen `3712` bis
`4283`. Dort werden privater Test-Geräteschlüssel und Zertifikat redundant mit
CRC gespeichert. Ein normaler Factory Reset löscht diese Identität bewusst
nicht. Deshalb zunächst nur auf dem vorgesehenen Test-Teensy verwenden.

Der private Schlüssel wird nicht exportiert. Ohne Secure Mode kann eine bewusst
veränderte Firmware den emulierten EEPROM-/Flashbereich dennoch lesen. Dieser
Teststand ist daher noch kein Schutz gegen vollständig manipulierte Firmware.

## 1. Kompilieren und installieren

1. `TP-3000.ino` öffnen.
2. Board `Teensy 4.1`, 600 MHz, Optimize `Faster`, USB `Serial`.
3. Keine externe micro-ecc-Version installieren; der Sketch verwendet den eindeutigen Header `TP3000_uECC.h` aus `libraries/TP3000_micro_ecc`.
   Eine falsche Boardauswahl wird absichtlich bereits beim Kompilieren abgelehnt.
4. Kompilieren und über USB auf den Test-Teensy laden.

Erwartung: Firmwareanzeige `0.50.1` vom `12.07.2026`.

## 2. Vorläufige Geräte-SN setzen

Vor der Provisionierung eine fünfstellige SN ungleich `00000` einstellen, zum
Beispiel `12345`. Solange kein gültiges Zertifikat vorhanden ist, stammt die SN
aus der normalen Geräteeinstellung.

## 3. Geräteschlüssel im Teensy erzeugen

1. Weboberfläche öffnen.
2. `Identität` auswählen oder direkt `/identity` aufrufen.
3. SN und Status kontrollieren.
4. `Geräteschlüssel erzeugen` drücken und die angezeigte SN bestätigen.

Erwartung:

- P-256-Schlüssel wird intern erzeugt.
- Es wird nur die Geräte-Key-ID angezeigt.
- Eine erneute Schlüsselerzeugung wird abgelehnt.
- Der private Schlüssel wird weder heruntergeladen noch auf SD geschrieben.

## 4. `.tpreq` herunterladen

Auf der Identity-Seite `.tpreq herunterladen` drücken.

Erwartete Datei:

```text
TP3000_12345_CERT_REQUEST.tpreq
```

Zusätzlich versucht das Gerät, dieselbe Datei unter
`/IDENTITY/TP3000_12345_CERT_REQUEST.tpreq` auf SD abzulegen. Der Webdownload
funktioniert auch dann, wenn die SD-Kopie fehlschlägt.

## 5. Zertifikat mit dem Offline-KeyTool ausstellen

1. TP3000KeyTool V0.3.0 öffnen.
2. Den vorhandenen Wegwerf-Test-Root laden.
3. `.tpreq` öffnen.
4. Selbstsignatur, Public Key, Key-ID und beantragte SN prüfen.
5. SN am realen Gerät kontrollieren.
6. `.tpcert` ausstellen.

Der Root wird dabei nicht neu erzeugt.

## 6. Zertifikat auf SD kopieren

Die ausgestellte `.tpcert` in dieses Verzeichnis der SD-Karte kopieren:

```text
/IDENTITY
```

Mehrere `.tpcert`-Dateien dürfen vorhanden sein. Das Gerät scannt die Dateien
und akzeptiert nur ein Zertifikat, das zum internen Public Key und zum
eingebauten Test-Root passt.

## 7. Zertifikat importieren

1. SD-Karte einsetzen.
2. `/identity` öffnen.
3. `Zertifikat von SD importieren` drücken.

Vor der Aktivierung prüft die Firmware:

- Format `TP3000-DEVICE-CERTIFICATE-2`
- Gerätetyp `TP-3000`
- ECDSA P-256 / IEEE-P1363
- Root-Key-ID `6CC674A19758D153`
- vollständigen Geräte-Public-Key
- Geräte-Key-ID
- Zertifikats-Manifest-SHA-256
- 64-Byte-Root-Signatur

## 8. Verhalten nach erfolgreicher Provisionierung

- wirksame Geräte-SN kommt ausschließlich aus `.tpcert`
- normale SN-Eingabe ist für andere Werte gesperrt
- USB-Befehl zum Ändern der SN wird abgelehnt
- dieselbe zertifizierte SN darf intern erneut gesetzt werden, damit vorhandene
  Backup-/Speicherpfade nicht brechen
- Zertifikat und Schlüssel werden bei jedem Boot geprüft
- beschädigter A-Slot kann über den gültigen B-Slot überlebt werden

## 9. Negativtests

Mindestens prüfen:

1. `.tpcert` eines anderen Device-Keys → Ablehnung
2. `.tpcert` eines anderen Test-Roots → Ablehnung
3. ein Zeichen in `.tpcert` ändern → Ablehnung
4. Zertifikat für andere Key-ID → Ablehnung
5. nach Zertifizierung andere Geräte-SN setzen → Ablehnung
6. Neustart → zertifizierte SN und Status bleiben erhalten
7. SD-Karte entfernen → bereits importiertes Zertifikat bleibt gültig

## Noch nicht enthalten

- Signatur der Mess-CSV/TPSIG im Teensy
- signierte Geräte- und Kopfkalibrierung
- Firmware-Release-Zertifikat und offizieller Firmwarehash
- Browser-Upload der `.tpcert`
- Produktiv-Root
- Secure Boot oder Lockable-Teensy-Sperrmodus
