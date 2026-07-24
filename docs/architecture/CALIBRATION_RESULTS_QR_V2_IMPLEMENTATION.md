# TP-3000 V0.50.1_23 – Kalibrierergebnisse und TP3Q2

## Datenmodell

Neue V3-Geräte- und Kopfjustierungsfreigaben enthalten drei zusätzliche, ECDSA-signierte Felder: `CalibrationScope`, `CalibrationResultsEncoding` und `CalibrationResultsBase64Url`. Unterstützt werden `AS_FOUND`, `AS_LEFT`, `AS_FOUND_AS_LEFT_NO_ADJUSTMENT` und `BEFORE_AFTER_ADJUSTMENT`; `AS_FOUND_AS_LEFT` bleibt als Altformat gültig. Die Firmware prüft Formatkennung, Umfang, Base64URL-Zeichen und die Signatur; die detaillierte Punktdarstellung wird in der Weboberfläche dekodiert.

## Kalibrierschein

Der Ausdruck trennt:

- Zertifikatsaussteller und Bearbeiter
- Gerätejustierung
- Kopfjustierung
- Kalibrierung vor Justierung (As Found)
- Kalibrierung nach Justierung (As Left)
- kalibriertechnische Fachangaben
- Offlineprüfung

Die Abweichung ist fest `Anzeige TP-3000 − Referenzwert`. Sollwert ist optional. U, k und Vertrauensniveau werden je Messpunkt dargestellt.

## QR-Transport

Die Webseite erzeugt `TP3Q2` als Base45(ZLIB(JSON)). `DocumentHash` wird aus den Manifest-Hashes der eingebetteten signierten Dokumente gebildet. Passt das Gesamtpaket in einen Code, werden `QrPartCount=1` und `QrPartIndex=1` verwendet. Andernfalls entstehen genau zwei Teile, einer für Geräte- und einer für Kopfjustierung. Beide tragen dieselbe Dokument-ID und denselben Dokumenthash.
