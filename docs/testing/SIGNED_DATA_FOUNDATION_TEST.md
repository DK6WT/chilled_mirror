# V0.50.1_13 Signed-Data-Grundlage – Vorprüfung

Stand: 2026-07-12

## Automatisch geprüft

- `TPsignedData.cpp` wurde mit Host-Stubs als C++17 mit
  `-Wall -Wextra -Werror` kompiliert.
- Der kanonische Writer wurde gegen eine feste Little-Endian-Testfolge geprüft.
- Firmwareidentität `0.50.1_13` und Evidence-Maske wurden im Hosttest geprüft.
- JavaScript der eingebetteten Web-Setup-Seite bestand `node --check`.
- Geänderte C/C++/INO-Dateien bestanden eine Zeichenketten-/Kommentar-aware
  Klammerprüfung.
- Interface-EEPROM-Layout geprüft: 72 Byte, Integritätsmodus Offset 67,
  CRC Offset 68.
- Setup-ETag wurde geändert, damit die neue Seite nicht durch eine alte
  Browser-Cache-Version verdeckt wird.
- Paketprüfung fand keinen privaten Schlüssel und keine Laufzeit-`.tpreq` oder
  `.tpcert`. Der bereits offengelegte öffentliche Test-Root bleibt enthalten.

## Noch auf realer Toolchain zu prüfen

Eine vollständige Teensyduino-Kompilierung war in der Vorbereitungsumgebung
nicht möglich. Erforderlich sind daher:

1. Build für Teensy 4.1 mit der üblichen Arduino-/Teensyduino-Konfiguration.
2. Vergleich des Speicherberichts mit V0.50.1_12.
3. Flash ohne EEPROM-Löschung und Kontrolle der bestehenden Geräteidentität.
4. Migrationstest `Log-Integritaet = Aus` beim ersten Start.
5. Auswahl `SHA-256`, Neustart und Kontrolle der Persistenz.
6. Kontrolle, dass `Zertifiziert` sichtbar, aber gesperrt ist.

## Abgrenzung

Dieser Stand erzeugt noch keine `.sha256`- oder `.TPSIG`-Datei. Er fixiert
Menü, EEPROM, Firmwareidentität, Formatkennungen und Schnittstellen für den
nächsten Implementierungsschritt.
