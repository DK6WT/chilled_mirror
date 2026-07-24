# Source Validation – TP-3000 V0.50.1 / Build 0.50.1_40

## Basis und Ziel

- Basis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_15.zip`
- Ziel: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_16.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_40`

## Analyse der gemeldeten Speicherwerte

Build `0.50.1_39`:

```text
RAM1: variables:151876, code:268328, padding:26584, free:77500
RAM2: variables:396736, free:127552
```

Build `0.50.1_34`:

```text
RAM1: variables:145604, code:266520, padding:28392, free:83772
RAM2: variables:385280, free:139008
```

Code plus Padding belegen bei beiden Ständen exakt 294.912 Byte. Der Rückgang des freien RAM1 stammt daher nicht aus einer vergrößerten ITCM-Blockreservierung, sondern aus zusätzlichen globalen und konstanten Daten.

## Geprüfte Optimierungen

- `TPcertifiedLog.cpp`: nicht zeitkritische Pfade, Suffixe, JSON-Vorlagen, Fehlermeldungen sowie Hex-/Base64-Alphabete in `.progmem` verschoben.
- `TPsignedCalibration.cpp`: neue Archiv-/Historientexte in `.progmem` verschoben.
- Geräte-/Kopfstatus und Geräte-/Kopfwertestrukturen in `DMAMEM` verlagert; gemeinsam 996 Byte.
- Alle nach RAM2 verlagerten Zustandsstrukturen werden in `tpSignedCalibrationBegin()` explizit initialisiert.
- Build-ID in `TPsignedData.h` und Web-Info auf `0.50.1_40` aktualisiert.

## Host-Objektvergleich

### TPcertifiedLog.cpp

```text
vorher: .rodata.str1.1 = 3957 Byte, .progmem = 157 Byte
nachher: .rodata.str1.1 = 68 Byte, .progmem = 4651 Byte
RAM1-Rodata-Gewinn: 3889 Byte
```

Ausführbarer `.flashmem`-Code blieb mit 21.374 Byte gleich.

### TPsignedCalibration.cpp

```text
vorher: .rodata.str1.1 = 3699 Byte, BSS der verlagerten Strukturen = 996 Byte
nachher: .rodata.str1.1 = 2433 Byte, BSS der verlagerten Strukturen = 0 Byte
RAM1-Rodata-Gewinn: 1266 Byte
RAM1-BSS-Gewinn:     996 Byte
RAM2-Zuwachs:       1008 Byte einschließlich Ausrichtung
```

Erwarteter RAM1-Gewinn insgesamt: etwa **6.151 Byte**. Die endgültige ARM-Linkerzahl kann nur der Teensyduino-Build liefern.

## Hostseitige Prüfungen

- `TPcertifiedLog.cpp` mit Teensy-/Arduino-Stubs und Clang C++17 syntaxgeprüft: bestanden.
- `TPsignedCalibration.cpp` mit Teensy-/Arduino-Stubs und Clang C++17 syntaxgeprüft: bestanden.
- Host-Objekte mit `-ffunction-sections -fdata-sections` erstellt und Sektionen verglichen: bestanden.
- `tools/tplog_extract.py` mit `py_compile` geprüft: bestanden.
- `TPsignedData.cpp`, `TPsignedCalibration.h`, `TPcertifiedLog.h`, `TPexternalCalibration.cpp/.h`, `TPmenu_CalibrationPages.ino` und `TPsdLog.ino` gegen Build `0.50.1_39` bytegenau unverändert.
- `TPethernet.ino` unterscheidet sich außerhalb der Dokumentation nur in der angezeigten Build-ID.
- `TPsignedData.h` unterscheidet sich nur in der Build-ID.

## Format- und Funktionsschutz

Nicht verändert wurden:

- Zertifikate und Signaturen,
- kanonische Bytes, Feldreihenfolgen und Skalierungen,
- TP3C1/TP3Q3,
- TPSIG und TPLOG,
- externe PDF-Bindung,
- Kalibrierarchivstruktur und Zeitraumssuche,
- die vier SD-Logmodi,
- ADC, Regelung und Safety.

## Noch ausstehend

- vollständiger Teensyduino-Build,
- neue FLASH-/RAM1-/RAM2-Linkerwerte,
- Hardware-Regressionsprüfung der zertifizierten Logmodi und historischen Kalibrierzertifikate.
