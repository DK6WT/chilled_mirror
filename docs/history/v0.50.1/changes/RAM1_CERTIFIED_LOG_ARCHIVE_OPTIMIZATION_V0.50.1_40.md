# RAM1-Optimierung – zertifizierte Logs und Kalibrierarchiv

## Ausgangslage

Linkerausgabe von Build `0.50.1_39`:

```text
FLASH: code:685508, data:1217108, headers:8644, free for files:6215204
RAM1: variables:151876, code:268328, padding:26584, free for local variables:77500
RAM2: variables:396736, free for malloc/new:127552
```

Zum Vergleich hatte Build `0.50.1_34` 145.604 Byte RAM1-Variablen und 83.772 Byte freien RAM1. Code plus Padding belegen in beiden Fällen exakt 294.912 Byte. Die Verschlechterung stammt daher nicht aus einem größeren ITCM-Codeblock, sondern nahezu vollständig aus zusätzlichen globalen und konstanten Daten.

## Änderungen

### Zertifizierter Logmodus

In `TPcertifiedLog.cpp` wurden nicht zeitkritische konstante Daten in die Teensy-Program-Flash-Sektion `.progmem` verlagert:

- Pfade und Dateiendungen,
- JSON- und kanonische Textvorlagen,
- Fehlermeldungen,
- Hex-/Base64-Alphabete,
- TPLOG-/TPSIG-Hilfstexte.

Der ausführbare Code bleibt in `FLASHMEM`; Journal-, Kontext- und Containerpuffer bleiben in RAM2.

### Signierte Kalibrierung und Historienarchiv

In `TPsignedCalibration.cpp` wurden die seit dem Historienarchiv hinzugekommenen konstanten Pfade und Meldungen ebenfalls in `.progmem` gelegt. Außerdem liegen folgende reinen POD-Strukturen nun in RAM2:

- drei `TpSignedCalibrationStatus`,
- `TpDeviceCalValues`,
- `TpHeadCalValues`,
- `TpSystemCalValues`.

Gesamtgröße: 996 Byte. Alle sechs Strukturen werden in `tpSignedCalibrationBegin()` explizit mit Null initialisiert.

## Gemessener Host-Objektvergleich

`TPcertifiedLog.cpp`:

```text
vorher .rodata.str1.1: 3957 Byte
nachher .rodata.str1.1:   68 Byte
Program-Flash-Zuwachs: 4494 Byte
RAM1-Rodata-Gewinn:    3889 Byte
```

`TPsignedCalibration.cpp`:

```text
vorher .rodata.str1.1: 3699 Byte
nachher .rodata.str1.1: 2433 Byte
RAM1-Rodata-Gewinn:    1266 Byte
RAM1-BSS-Gewinn:        996 Byte
RAM2-Zuwachs:          1008 Byte inkl. Ausrichtung
```

Erwarteter Gesamtgewinn in RAM1: ungefähr **6.151 Byte**. Ausgehend von 77.500 Byte frei sind beim Teensy-Linker ungefähr 83–84 kB freier RAM1 zu erwarten. Die exakte Zahl hängt von ARM-Linker und Ausrichtung ab.

## Unverändert

Nicht verändert wurden:

- ECDSA-P-256- und SHA-256-Verfahren,
- Zertifikate und Signaturen,
- kanonische Feldreihenfolgen und Skalierungen,
- TP3C1 und TP3Q3,
- TPSIG- und TPLOG-Formate,
- die vier SD-Logmodi,
- PDF-Inhalt und PDF-Hashbindung,
- Kalibrierarchivpfade und Suchlogik,
- ADC, Regelung und Safety.
