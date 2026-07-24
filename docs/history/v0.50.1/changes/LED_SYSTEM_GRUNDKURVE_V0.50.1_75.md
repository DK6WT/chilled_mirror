# TP-3000 LED-Autoadaption - festes 1-K-Raster V0.50.1_75

## Festlegung

Die externe System-Grundkurve und das kopfbezogene Lernmodell verwenden dasselbe feste Temperaturraster:

- Rasterbeginn: `-50 °C`
- Rasterende: `+110 °C`
- Schrittweite: `1 K`
- Anzahl Werte beziehungsweise Lernklassen: `161`
- Referenztemperatur: üblicherweise `25 °C`

Damit ist jeder Temperaturklasse genau eine Tabellenposition zugeordnet:

```text
Index 0   = -50 °C
Index 50  =   0 °C
Index 75  =  25 °C
Index 160 = 110 °C
```

Die 1-K-Klassen des Lernmodells fassen alle erfolgreichen Auto-Cals zusammen, deren gefilterte Umgebungstemperatur auf dieselbe ganze Temperatur gerundet wird. Beispiel: `20,5 °C`, `20,7 °C` und `20,9 °C` gehören zur Klasse `21 °C`. Der Grundkurvenanteil wird weiterhin bei der exakten Temperatur berechnet; gemittelt wird nur die verbleibende Formabweichung.

## Dateiname

Die Datei wird auf der SD-Karte unter exakt folgendem Pfad erwartet:

`/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`

## Dateiformat Version 2

```text
TP3000_SYSTEM_GRUNDKURVE;2
CURVE_ID;TP3000_OPTIK_SERIE_01
HEAD_TYPE;ANY
GRID_MIN_C;-50
GRID_MAX_C;110
GRID_STEP_K;1
REFERENCE_TEMP_C;25
VALID_FROM_C;-20
VALID_TO_C;60
TEMP_C;CURRENT_FACTOR
-50;0.714286
-49;0.716949
...
110;1.666667
```

Die Tabelle muss genau 161 Datenzeilen enthalten. Die Temperaturen müssen lückenlos und in genau dieser Reihenfolge von `-50` bis `110` angegeben sein. Andere Raster, fehlende Werte, doppelte Werte oder zusätzliche Temperaturpunkte führen zum vollständigen Verwerfen der Datei.

## Gültigkeitsbereich

`VALID_FROM_C` und `VALID_TO_C` kennzeichnen den tatsächlich messtechnisch bestimmten beziehungsweise freigegebenen Bereich der Systemkurve. Beide Grenzen müssen auf dem 1-K-Raster liegen und die Referenztemperatur einschließen.

Innerhalb dieses Bereichs verwendet die Firmware die externe Systemkurve. Zwischen zwei ganzzahligen Rasterpunkten wird linear interpoliert. Außerhalb des freigegebenen Bereichs wird keine Systemsteigung extrapoliert. Stattdessen wird die Systemkorrektur am Rand fest angekoppelt und nur der Verlauf der internen OD-850FHT-Herstellerkurve fortgeführt. Dadurch bleibt der Übergang stetig.

## Auswahlreihenfolge

1. Gültige Systemdatei auf SD: externe System-Grundkurve.
2. Datei fehlt oder ist ungültig: interne OD-850FHT-Herstellerkurve.
3. Ohne SD bleiben `Aus` und `Ein - Grundkurve` verfügbar.
4. `Selbstlernend` ist ohne SD gesperrt.
5. Wird die SD während `Selbstlernend` entfernt, fällt das Gerät auf `Ein - Grundkurve` mit interner Herstellerkurve zurück.

## Lernmodell

Das Lernmodell wurde auf 161 Klassen erweitert. Die Strukturversion steigt von 1 auf 2. Bestehende A/B-Lerndateien mit dem alten Bereich `-40...+85 °C` werden wegen der geänderten Modellgeometrie nicht übernommen; für den betroffenen Kopf wird ein neues leeres Modell begonnen.

Die Lernklassen sind positionsgleich zur Systemkurve. Dadurch können später aus mehreren repräsentativen Geräten ermittelte Klassenwerte ohne erneute Temperaturzuordnung in eine neue Seriengrundkurve überführt werden. Dabei ist zu beachten, dass das Lernmodell eine relative Formkorrektur zur jeweils verwendeten Grundkurve enthält. Für eine neue absolute Systemgrundkurve ist daher der jeweilige Grundkurvenfaktor mit dem gemittelten Lernfaktor zu kombinieren.

## Plausibilitätsprüfung

Die Datei wird nur angenommen, wenn:

- Kennung und Formatversion 2 stimmen,
- `CURVE_ID`, `HEAD_TYPE`, Rasterdaten, Referenz- und Gültigkeitsbereich vorhanden sind,
- Raster exakt `-50...+110 °C` mit `1 K` Schrittweite ist,
- genau 161 Faktoren vorhanden sind,
- jeder Faktor zwischen `0,5` und `2,0` liegt,
- Referenz- und Gültigkeitsgrenzen ganzzahlig und innerhalb des Rasters liegen,
- die Referenztemperatur vom Gültigkeitsbereich eingeschlossen wird,
- der Kopftyp passt,
- die Datei vollständig lesbar und kleiner als 8192 Byte ist.

Die Faktoren werden nach dem Laden nochmals auf den Faktor an der Referenztemperatur normiert. Eine geänderte Kurven-ID oder ein geänderter Kurveninhalt erzeugt eine neue Kurvenkennung; Lerndaten einer anderen Grundkurve werden nicht angewendet.

## Vorlage

Eine vollständige 161-Punkte-Vorlage liegt unter:

`SD_CARD_TEMPLATE/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`

Die Beispielwerte bilden ausschließlich die interne OD-850FHT-Herstellerkurve nach. Für die reale Seriengrundkurve sind die Faktoren im gemessenen Gültigkeitsbereich zu ersetzen, `CURVE_ID` zu ändern und `VALID_FROM_C` beziehungsweise `VALID_TO_C` auf den tatsächlich validierten Bereich zu setzen.
