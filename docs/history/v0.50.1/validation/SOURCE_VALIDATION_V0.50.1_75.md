# TP-3000 Source Validation V0.50.1_75

## Prüfgegenstand

- interne Version: `0.50.1`
- Build-ID: `0.50.1_75`
- Basis: `V0.50.1_74`
- Lernraster und Systemkurve: `-50...+110 °C`, `1 K`, `161` Positionen
- Systemdatei: `/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`

## Statische Hostprüfung

`TPledAdaptation.cpp` wurde mit einem schlanken Arduino-/SD-Hoststub unter C++17 geprüft:

```text
g++ -std=c++17 -Wall -Wextra -Werror -fsyntax-only ... TPledAdaptation.cpp
HOST_SYNTAX_OK
```

Ergebnis der Hoststrukturprüfung:

```text
LED_ADAPT_BIN_COUNT = 161
LED_ADAPT_SYSTEM_CURVE_POINT_COUNT = 161
sizeof(LedAdaptModel) = 3264 Byte
```

Damit bleibt ein einzelnes A/B-Modell weiterhin kleiner als 4096 Byte. Beide Modellpuffer liegen wie bisher in RAM2/DMAMEM.

## Verhaltenstest mit simulierter SD-Karte

Ein kompilierter C++17-Test hat geprüft:

1. keine SD-Karte und gespeicherter Modus `Selbstlernend`:
   - Rückfall auf `Ein - Grundkurve`,
   - interne Herstellerkurve aktiv;
2. gültige Format-2-Datei:
   - exakt 161 Werte werden geladen,
   - Raster `-50...+110 °C` wird bestätigt,
   - Referenzpunkt 25 °C wird auf Faktor 1,0 normiert,
   - Zwischenwerte werden linear interpoliert;
3. fehlender letzter Rasterwert:
   - vollständiger Fallback auf Herstellerkurve;
4. falsche Schrittweite:
   - vollständiger Fallback auf Herstellerkurve;
5. Klassenzuordnung:
   - `20,49 °C -> 20 °C`,
   - `20,50 °C -> 21 °C`,
   - Werte außerhalb des Lernbereichs werden auf `-50` beziehungsweise `110 °C` begrenzt.

Ergebnis:

```text
MODEL_SIZE=3264
BEHAVIOR_OK
```

## Dateivorlage

Die Vorlage enthält:

- Formatversion 2,
- vollständige 161-Punkte-Tabelle,
- feste Rastermetadaten,
- Referenztemperatur 25 °C,
- expliziten Gültigkeitsbereich,
- Dateigröße 2564 Byte.

Die Faktoren entsprechen nur der internen OD-850FHT-Herstellerkurve und sind als gefahrloser Format- und Funktionstest vorgesehen.

## Paketprüfung

Das finale Quellpaket wurde nach dem Erstellen erneut geprüft:

```text
Dateien im Quellbaum: 279
ZIP-CRC:               fehlerfrei
Dateiliste:            vollständig identisch
Entpackvergleich:      279 von 279 Dateien bytegleich
```

Der Vergleich erfolgte gegen einen frisch entpackten temporären Zielbaum. Verzeichnisse und versteckte Projektdateien wurden dabei mit einbezogen.

## Noch offene Zielsystemprüfung

Nicht durch die Hostprüfung abgedeckt sind:

- vollständiger Teensyduino-Build für Teensy 4.1,
- tatsächliche FLASH-/RAM1-/RAM2-Ausgabe des Linkers,
- Migration beziehungsweise bewusster Neustart bestehender Lerndateien,
- SD-Kartenwechsel am realen Gerät,
- Klimakammerfahrt und Ableitung einer gemessenen Seriengrundkurve,
- Langzeitprüfung der 161 Lernklassen.
