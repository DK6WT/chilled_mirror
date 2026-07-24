# Source Validation – V0.50.1_52

## Paket

- Ausgabe: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_28.zip`
- bestätigte Basis: V0.50.1.26
- direkter Vorgänger: V0.50.1.27 / Build `0.50.1_51`
- neuer Build: `0.50.1_52`

## Statisch geprüft

- kein `new TextBox()` mehr im Quellbaum
- `VirtLCDMenu` und `VirtLCDMessage` zeigen auf zwei statische Objekte
- alle generierten DroidSansMono-Daten-, Index- und Deskriptortabellen besitzen `PROGMEM`
- der Fontgenerator erzeugt dieselbe PROGMEM-Ablage
- genau ein gemeinsamer 4096-Byte-DMAMEM-SHA-256-Arbeitspuffer vorhanden
- Firmware- und externe PDF-Prüfung verwenden den gemeinsamen Puffer
- Build-ID im aktiven Quellcode ist `0.50.1_52`
- Dokumentation und Paketbezeichnungen stimmen überein
- ZIP wurde frisch entpackt und erneut gegen diese Bedingungen geprüft

## ELF-Nachweis des Vorgängerfehlers

Das hochgeladene Build-50-ELF enthält Build-ID `0.50.1_50`. `addr2line` ordnet `0x37E82` `memset()` zu. Die zwei unmittelbar vorausgehenden `operator new(0xE2C)`-Sequenzen im `setup()` stimmen mit den beiden dynamischen Menü-TextBoxen überein.

## Noch offen

Ein vollständiger Teensyduino-Build und der Hardwaretest sind in der bereitstellenden Umgebung nicht möglich. Die tatsächliche Memory-Usage und der fehlerfreie Start müssen am Teensy 4.1 bestätigt werden.
