# Source Validation 0.50.1_25

- Firmwareidentität: `0.50.1`, Build-ID `0.50.1_25`, Buildtag 13.07.2026.
- Neue Formate: `TP3000-SYSTEM-CALIBRATION-REQUEST-1` und `TP3000-SYSTEM-CALIBRATION-1`.
- Canonical-Reihenfolge der Systemwerte zwischen Firmware und KeyGen abgeglichen.
- `.tpscalreq` ist gerätesigniert und auf 72 Stunden begrenzt.
- `.tpscal` akzeptiert Hersteller-Root oder gültiges Laborzertifikat mit Kalibrierberechtigung.
- Aktive Systemkalibrierung bindet Geräte-SN, Kopftyp, Kopf-SN sowie IDs und SHA-256-Manifeste beider Justierungen.
- Web-Endpunkte für Anfrage, aktives JSON und direkten Upload ergänzt.
- Der Nachweisstatus für den späteren zertifizierten Logmodus umfasst die aktive Systemkalibrierung.
- HTTP-Upload liest den Body nicht blockierend mit 512 Byte je Hauptloop, maximal 16.383 Byte, 2 s Idle- und 15 s Gesamtgrenze.
- C/C++-Quellen erfolgreich präprozessiert; `TPsignedCalibration.cpp`, `TPsignedData.cpp` und `TPethernet.ino` wurden mit Host-Stubs ohne Syntaxfehler geprüft.
- Eine native Teensyduino-Kompilierung war in der Prüfungsumgebung nicht verfügbar.
