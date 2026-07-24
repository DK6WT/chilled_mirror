# TP-3000 Source Validation V0.50.1_74

## Prüfgegenstand

- interne Version: `0.50.1`
- Build-ID: `0.50.1_74`
- Basis: `V0.50.1_73`
- neue System-Grundkurve: `/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`

## Statische Prüfung der LED-Autoadaption

`TPledAdaptation.cpp` wurde mit einem schlanken Arduino-/SD-Hoststub unter C++17 geprüft:

```text
g++ -std=c++17 -Wall -Wextra -Werror -fsyntax-only ... TPledAdaptation.cpp
HOST_SYNTAX_OK
```

Damit wurden Syntax, Typen, Warnungsfreiheit des isolierten Moduls und die neue CSV-Parserlogik auf Hostebene geprüft. Dies ersetzt keinen vollständigen Teensyduino-Linklauf.

## Verhaltenstest mit simulierter SD-Karte

Ein kompilierter C++17-Test hat folgende Fälle ausgeführt:

1. keine SD-Karte:
   - `Aus` und `Ein - Grundkurve` bleiben zulässig,
   - `Selbstlernend` wird abgelehnt,
   - interne Kennlinie `OD-850FHT-HERSTELLER` bleibt aktiv;
2. gültige Datei mit Dezimalkomma:
   - Datei wird geladen,
   - `CURVE_ID` wird übernommen,
   - `Selbstlernend` ist freigegeben;
3. SD-Entnahme während `Selbstlernend`:
   - automatischer Rückfall auf `Ein - Grundkurve`,
   - externe Systemkurve wird verworfen,
   - interne Herstellerkennlinie wird aktiv;
4. ungültige Dateikennung:
   - Datei wird verworfen,
   - interne Herstellerkennlinie bleibt Fallback.

Ergebnis:

```text
BEHAVIOR_OK
```

## Web-Setup

Der eingebettete JavaScript-Block der Setupseite wurde extrahiert und mit Node.js syntaktisch geprüft:

```text
node --check tp74_setup.js
JS_OK
```

Ohne SD enthält die Auswahlliste nur `Aus` und `Ein - Grundkurve`; das Backend sperrt ausschließlich `Selbstlernend`.

## Dateiformat und Plausibilitätsgrenzen

Die Vorlage unter

`SD_CARD_TEMPLATE/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`

wurde gegen die Parserbedingungen geprüft:

- Kennung `TP3000_SYSTEM_GRUNDKURVE;1`,
- gültige `CURVE_ID`,
- `HEAD_TYPE` passend oder `ANY`,
- Referenztemperatur innerhalb des Stützstellenbereichs,
- 2 bis 32 streng aufsteigende Stützstellen,
- maximal 15 K Abstand zwischen benachbarten Stützstellen,
- Temperaturbereich -50 bis +100 °C,
- relative Stromfaktoren 0,5 bis 2,0,
- Dateigröße kleiner als 4096 Byte.

## Paketprüfung

Das Release-ZIP wurde vollständig geprüft:

```text
unzip -t Taupunktspiegel_StandV1_2026-07-19_V0.50.1_74.zip
No errors detected in compressed data.
BYTE_COMPARE_OK files=277
```

Dabei wurden alle Dateinamen sowie sämtliche Dateiinhalte nach erneutem Entpacken bytegleich mit dem Arbeitsverzeichnis verglichen. Build-ID, Systemkurvenvorlage und Dokumentation sind im Paket enthalten.

## Noch offene Hardwareprüfung

Nicht in dieser Hostprüfung enthalten sind:

- vollständiger Teensyduino-Build für Teensy 4.1,
- tatsächliche FLASH-/RAM1-/RAM2-Ausgabe des Linkers,
- SD-Kartenwechsel am realen Gerät,
- Temperaturfahrt mit gemessener TP-3000-Systemgrundkurve,
- Langzeittest des selbstlernenden Modells.
