# Source Validation V0.50.1_20

Datum: 2026-07-12

Durchgeführt:

- geänderte C/C++-Dateien auf Klammer- und Stringstruktur geprüft
- Web-Zeitsync sendet lokale Zeit und `tz=-getTimezoneOffset()`
- Endpunkt `/set-time` verlangt und plausibilisiert UTC-Offset von -14:00 bis +14:00
- Device-/UI-EEPROM-Payload bleibt unverändert groß; UTC-Offset nutzt drei bereits reservierte Bytes
- UTC-Rechnung geprüft: `UTC = lokale RTC-Zeit - UTC-Offset`
- alle Kalibrier-Gültigkeitsanzeigen verwenden die gemeinsame UTC-Funktion
- Kalibrieranfrage wird ohne gültigen UTC-Offset abgelehnt
- Firmwareversions- und Paketbezeichnungen konsistent auf Build `0.50.1_20` / Paket `V0.50.1_7` aktualisiert

Nicht durchgeführt:

- Arduino-/Teensyduino-Kompilierung
- Flash-/RAM-Linkerbericht
- Hardware-, RTC-, Netzwerk-, Drucker- oder Watchdogtest

Diese Punkte müssen vor Freigabe nach `BUILDING.md` nachgeholt werden.
