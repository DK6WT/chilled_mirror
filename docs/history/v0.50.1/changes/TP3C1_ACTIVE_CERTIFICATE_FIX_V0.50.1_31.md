# TP3C1 – Korrektur der Zertifikatsquelle in Build 0.50.1_31

## Fehlerbild

Der Kalibrierschein meldete `TP3C1: Gerätezertifikat V2 fehlt`, obwohl im
Gerät bereits ein gültiges Gerätezertifikat aktiv war.

## Ursache

Der erste TP3C1-Encoder suchte das vollständige `DeviceCertificate` fälschlich
innerhalb der aktiven Gerätejustierung. Das signierte Kalibrierpaket enthält
jedoch nur die Zertifikatsseriennummer; das vollständige `.tpcert` wird getrennt
im Identitätsspeicher des TP-3000 verwaltet.

## Korrektur

- Nur-Lese-Endpunkt `/identity-active-certificate.json` ergänzt.
- Der Endpunkt verwendet `deviceIdentityBuildCertificateJson()` und gibt das
  bereits verifizierte aktive Zertifikat unverändert aus.
- Die Kalibrierscheinseite lädt Zertifikat, Gerätejustierung, Kopfjustierung und
  Systemkalibrierung getrennt.
- Der TP3C1-Encoder prüft Geräte-SN und Zertifikats-SN, bevor er den
  Identitätsblock aufbaut.
- Das Labor-Rollenzertifikat bleibt Bestandteil der bestehenden
  Justierungspakete und wird nicht verändert.

## Kompatibilität

Das TP3C1-V0.2-Binärformat wurde nicht geändert. Mit den unveränderten
G00001/K30001-Referenzdaten entstehen weiterhin exakt dieselben vier
Referenz-SHA-256-Werte. Alle bestehenden Zertifikate und fünf Signaturen bleiben
unverändert.
