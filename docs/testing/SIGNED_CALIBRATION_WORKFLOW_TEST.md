# V0.50.1_15 – Testablauf signierte Kalibrierung

## 1. Voraussetzungen

- TP-3000 besitzt ein gültiges Gerätezertifikat.
- Geräte-SN und Geräte-Key-ID sind nach Power-Cycle unverändert.
- SD-Karte ist beschreibbar.
- TP3000KeyTool V0.5.0 wurde lokal erfolgreich kompiliert.
- Öffentlicher Hersteller-Root passt zur Firmware.

## 2. CALIBRATION-Rollenschlüssel einmalig erzeugen

1. Im KeyTool den privaten Hersteller-Root laden.
2. Register `Kalibrierung` öffnen.
3. `CALIBRATION-Schlüssel erzeugen…` wählen.
4. Lange Passphrase vergeben.
5. Prüfen, dass vier Dateien entstanden sind: privater PEM, öffentlicher PEM, Info-JSON und root-signiertes `.tprole`.
6. Hersteller-Root anschließend schließen.

Dieser Schritt wird bei normalen Folgekalibrierungen nicht wiederholt.

## 3. Geräte-Werksjustierung signieren

1. TP-3000-Webseite `/calibration` öffnen.
2. `.tpdcalreq` exportieren.
3. Im KeyTool den bestehenden CALIBRATION-Rollenschlüssel laden. Dazu genügt der öffentliche Hersteller-Root zur Zertifikatsprüfung.
4. Anfrage laden. Erwartung: Gerätezertifikat, Bindung, Manifest und Gerätesignatur gültig.
5. Calibration-ID, Gültig-ab/Gültig-bis, Intervall und optionale Metadaten prüfen.
6. `.tpdcal` erzeugen.
7. Datei unverändert nach `/CALIBRATION` auf der SD-Karte kopieren.
8. Am Gerät `Signierte Gerätejustierung von SD importieren` wählen.

## 4. Kopfkalibrierung signieren

Analog mit `.tphcalreq` und `.tphcal`. Kopftyp und Kopf-SN müssen sichtbar zum realen Kopf passen.

## 5. Erwarteter Gerätestatus

- Beide Signaturen gültig.
- Aktuelle Werte stimmen mit den signierten Werten überein.
- Calibration-ID, Signer-Key-ID und Gültig-bis werden angezeigt.
- Gerätezertifikat + Gerätejustierung + Kopfkalibrierung werden als vollständig gemeldet.
- `Zertifiziert` bleibt in V0.50.1_15 trotzdem gesperrt, weil `.TPSIG` noch nicht erzeugt wird.

## 6. Nicht blockierender Ablauf-Test

Eine Testfreigabe mit bereits abgelaufenem `ValidUntilUtc` erzeugen und importieren. Erwartung:

- Signatur weiterhin gültig.
- Anzeige `abgelaufen` beziehungsweise später `außerhalb Kalibrierzeitraum`.
- Messung, Logging und Export laufen weiter.
- Kein Safety- oder Zertifikatsfehler.

## 7. Manipulationstests

Jeweils nur eine Kopie verändern:

- ein Zeichen in den Kalibrierwerten,
- ein Zeichen in `ValidUntilUtc`,
- Geräte-SN oder Key-ID,
- Kopftyp oder Kopf-SN,
- ein Zeichen in `CalibrationSignatureBase64`,
- fremdes `.tprole`.

Alle manipulierten Dateien müssen abgelehnt werden; die zuvor aktive gültige Kalibrierung bleibt erhalten.

## 8. Werteänderung nach Import

Nach erfolgreichem Import einen gebundenen Geräte- oder Kopfwert ändern. Erwartung: `valuesMatchCurrent` wird falsch, die signierte Kalibrierung gilt nicht mehr als aktuell passend. Nach Rückkehr zu exakt denselben Werten muss sie wieder passen.

## 9. Folgekalibrierung

Neue Anfrage mit neuer Calibration-ID signieren, aber denselben CALIBRATION-Rollenschlüssel verwenden. Das Gerät muss die neueste gültige Datei anhand `ApprovedUtc` aktivieren. Die alte Datei bleibt für historische Lognachweise archiviert.
