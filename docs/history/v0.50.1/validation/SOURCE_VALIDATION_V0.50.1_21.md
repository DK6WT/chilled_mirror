# Source Validation V0.50.1_21

## Geprüfte Änderungen

- Build-ID ist konsistent `0.50.1_21`.
- Paketname ist konsistent `Taupunktspiegel_StandV1_2026-07-12_V0.50.1_8.zip`.
- Die Web-Hauptseite sendet weiterhin `tz=-Date.getTimezoneOffset()` in Minuten.
- Der `/set-time`-Handler unterscheidet fehlenden UTC-Offset von ungültigen Datums-/Zeitwerten.
- Alle statischen HTML-ETags enthalten automatisch `TP_FIRMWARE_BUILD_ID_STRING`.
- Die vorhandene lokale RTC-Zeitdarstellung und die UTC-Umrechnung für kryptografische Zeitstempel bleiben unverändert.

## Nicht ausgeführt

Eine vollständige Teensyduino-Kompilierung und ein Lauf auf echter TP-3000-Hardware waren in dieser Umgebung nicht möglich.
