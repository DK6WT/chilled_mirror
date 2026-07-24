# Source Validation V0.50.1_22

## Geprüfte Änderungen

- Build-ID ist konsistent `0.50.1_22`.
- Paketname ist konsistent `Taupunktspiegel_StandV1_2026-07-12_V0.50.1_9.zip`.
- `CompressionStream` wird über `pipeThrough()` gleichzeitig geschrieben und gelesen.
- Der Druckknopf ist initial deaktiviert und wird erst nach erfolgreichem QR-Aufbau freigeschaltet.
- QR-Fehler bleiben auf den Abschnitt `Offline-Prüfung` begrenzt.
- Das Format `TP3Q1`, Base45 und die kryptografischen Kalibrierformate wurden nicht geändert.
- Statischer JavaScript-Syntaxtest wurde ausgeführt.
- Ein repräsentatives Geräte-Kalibrierpaket wurde in Node.js per DEFLATE und Base45 verarbeitet und als QR-Code aufgebaut.

## Nicht ausgeführt

Eine vollständige Teensyduino-Kompilierung und ein Lauf auf echter TP-3000-Hardware waren in dieser Umgebung nicht möglich.
