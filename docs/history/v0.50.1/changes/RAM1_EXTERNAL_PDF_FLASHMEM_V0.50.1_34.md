# RAM1-Entlastung externer PDF-Kalibrierschein – Build 0.50.1_34

## Ziel

Der in Build 0.50.1_33 ergänzte SD-basierte Ablauf für externe PDF-Kalibrierscheine ist nicht zeitkritisch. Seine Funktionen, Konstanten und großen Arbeitspuffer sollen den knappen RAM1-Bereich des Teensy 4.1 möglichst wenig belasten.

`FLASHMEM` bezeichnet dabei den internen, speicherabgebildeten Program-Flash des Teensy 4.1. Es wird kein zusätzlicher externer 64-MiB-Flash vorausgesetzt oder angesprochen.

## Verlagerte Funktionen

Alle Funktionen in `TPexternalCalibration.cpp` sind mit `FLASHMEM` gekennzeichnet. Dazu gehören:

- Initialisierung und Laden der aktiven PDF-Metadaten
- blockweiser Upload, Abschluss und Abbruch
- zweite SHA-256-Prüfung nach erneutem Öffnen
- JSON-Metadaten lesen und schreiben
- SD-Archivierung und atomarer Wechsel von `ACTIVE.json`
- Bindungsprüfung für die Systemkalibrierung

Die bereits vorhandenen Web-Handler und Systemkalibrierungsfunktionen bleiben ebenfalls in `FLASHMEM`. Keine dieser Funktionen wird aus einer ISR oder aus dem zeitkritischen Mess-/Regelpfad aufgerufen.

## Verlagerte Konstanten

Pfade, Feldnamen, JSON-/HTML-Formate sowie Status- und Fehlertexte des externen PDF-Ablaufs liegen in `PROGMEM`. Sie werden auf dem Teensy 4.1 direkt aus dem speicherabgebildeten Program-Flash gelesen.

## Verlagerte Daten und Puffer

Folgende reine Daten liegen in `DMAMEM`/RAM2:

- aktive externe PDF-Metadaten und Uploadzustand
- Hash- und JSON-Arbeitspuffer
- temporäre Metadatensätze und Zielpfade
- Web-Ausgabe- und HTML-Escape-Puffer
- Uploadfehler- und Ergebnisfelder
- aktiver Systemkalibrierungsstatus
- intern gehaltene signierte Systemkalibrierungswerte

`File` und `TpSha256` verbleiben bewusst in RAM1, da es sich um C++-Objekte mit Konstruktoren handelt. Für DMAMEM wird keine automatische Nullinitialisierung vorausgesetzt. Persistent verwendete RAM2-Zustände werden deshalb in `tpExternalCalibrationBegin()` beziehungsweise `tpSignedCalibrationBegin()` explizit mit `memset` beziehungsweise durch den bestehenden Resetpfad initialisiert.

## Vergleichende Größenabschätzung

Ein Host-Objektvergleich mit identischen Stubs und Abschnittsattributen ergab für `TPexternalCalibration.cpp`:

| Abschnitt | Build 0.50.1_33 | Build 0.50.1_34 |
|---|---:|---:|
| normaler Text | 8181 B | 2233 B |
| `.flashmem` | 0 B | 5898 B |
| normale Read-only-Daten | 2016 B | 267 B |
| `.progmem` | 0 B | 2246 B |
| `.bss` | 619 B | 128 B |
| `.dmabuffers` | 5632 B | 7272 B |

Diese x86-Hostwerte sind keine Teensy-Linkerwerte, zeigen aber die Abschnittsverschiebung eindeutig. Zusätzlich wurden 1872 B Systemkalibrierungsstatus/-werte und 2790 B Web-Arbeitspuffer nach RAM2 verlagert. Rund 1281 B externe Webtexte und Formatstrings liegen zusätzlich in PROGMEM.

Ausgehend von der gemeldeten Belegung des Build 0.50.1_33 ist daher ungefähr zu erwarten:

- RAM1 frei: von 78.332 B auf etwa 88–90 kB
- RAM2 frei: von 145.216 B auf etwa 138–140 kB

Die exakten Werte hängen vom ARM-Linker, Inlining und der Abschnittsausrichtung ab und müssen durch den Teensyduino-Build bestätigt werden.

## Unveränderte Funktionen und Formate

Nicht verändert wurden:

- PDF-Originalbytes und 6-MiB-Grenze
- SD-Verzeichnis- und Archivlogik
- Metadatenformat und SHA-256-Prüfung
- Systemkalibrierungsbindung
- Zertifikate und Signaturen
- TP3C1 und TP3Q3
- ADC, Messwerterfassung, Regelung und Safety
