# Source Validation V0.50.1_49

- Basis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_24.zip`
- Ziel: `Taupunktspiegel_StandV1_2026-07-17_V0.50.1_25.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_49`

## Geprüft

- Post-Link-Manifestwerkzeuge und deren Test entfernt.
- Exakter Start-Hash über `_flashimagelen` bleibt erhalten.
- Geräte-signierte `.tpfwreq` mit eingebettetem Gerätezertifikat.
- Root-signierte, bildgebundene `.tpfwcert` ohne Gerätebindung.
- kanonische Feldreihenfolge in Firmware und KeyGen abgeglichen.
- P-256/P1363-Hostvektor für das Firmwarezertifikat erfolgreich.
- Webrouten für Anfrage, Direktupload und SD-Import vorhanden.
- dynamische Seiten-Printf-Platzhalter stimmen mit den Argumenten überein.
- C++-Syntax des neuen Firmwaremoduls mit Teensy-kompatiblen Stubs geprüft.
- HTML-Template separat kompiliert; Script- und Inline-Handler mit Node.js syntaktisch geprüft.
- UTC-Parser akzeptiert ausschließlich `Z` oder `+00:00`.
- vorhandene Kalibrier-, TP3C1-, Anwendbarkeits- und Seitenübertragungstests bestanden.

## Nicht ausgeführt

- vollständiger Teensyduino-Linkerbuild
- realer Startzeit-/Flash-Lesetest auf Teensy 4.1
- realer Windows-.NET-Build des KeyGen
- Hardwaretest mit Root-Zertifikatimport und Neustart
