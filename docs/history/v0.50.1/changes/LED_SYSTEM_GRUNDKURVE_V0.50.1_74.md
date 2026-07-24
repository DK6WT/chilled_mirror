# TP-3000 LED-Autoadaption – System-Grundkurve V0.50.1_74

## Festgelegter Dateiname

Die bevorzugte gemessene Grundkennlinie der vollständigen TP-3000-Optikkette wird auf der SD-Karte unter exakt folgendem Pfad erwartet:

`/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`

Die Datei wird beim Start, nach erneutem Einsetzen der SD-Karte und bei einem Wechsel des gewählten Kopftyps neu geprüft. Eine während des laufenden Betriebs überschriebene Datei wird sicher durch Neustart oder Kartenentnahme/-einsetzen neu geladen.

## Auswahlreihenfolge

1. Ist die Datei vorhanden, vollständig, plausibel und zum gewählten Kopftyp passend, wird sie als System-Grundkurve verwendet.
2. Fehlt die Datei oder ist sie ungültig, wird ohne Unterbrechung die fest in der Firmware hinterlegte typische OD-850FHT-Herstellerkurve verwendet.
3. `Aus` wendet keine Kurve an.
4. `Ein - Grundkurve` verwendet die Systemdatei oder den Hersteller-Fallback.
5. `Selbstlernend` verwendet dieselbe Grundkurve und zusätzlich die kopfbezogenen Lerndaten.

## Verhalten ohne SD-Karte

Ohne SD-Karte bleiben zwei Modi verfügbar:

- `Aus`
- `Ein - Grundkurve`

`Ein - Grundkurve` verwendet dann ausschließlich die fest eingebaute OD-850FHT-Herstellerkurve. `Selbstlernend` ist gesperrt, weil das kopfbezogene Lernmodell derzeit auf SD liegt.

Wird die SD-Karte während `Selbstlernend` entfernt, wechselt die Firmware automatisch auf `Ein - Grundkurve`. Eine zuvor geladene Systemdatei wird verworfen und die interne Herstellerkurve wird aktiv. Die letzte erfolgreiche Auto-Cal bleibt der absolute Stromanker; der Übergang erfolgt weiterhin über die vorhandene Stromrampe.

## Dateiformat

Die Datei ist eine Semikolon-getrennte Textdatei. Dezimalpunkt und Dezimalkomma werden akzeptiert. Leerzeilen und Zeilen, die mit `#` beginnen, werden ignoriert.

```text
TP3000_SYSTEM_GRUNDKURVE;1
CURVE_ID;TP3000_OPTIK_SERIE_01
HEAD_TYPE;ANY
REFERENCE_TEMP_C;25.0
TEMP_C;CURRENT_FACTOR
0.0;0.884956
5.0;0.905797
10.0;0.927644
15.0;0.950570
20.0;0.974659
25.0;1.000000
30.0;1.028807
35.0;1.059322
40.0;1.091703
45.0;1.126126
50.0;1.162791
```

### Felder

- `TP3000_SYSTEM_GRUNDKURVE;1`: feste Dateikennung und Formatversion.
- `CURVE_ID`: eindeutige Kennung aus Buchstaben, Ziffern, `_`, `-` oder `.`. Bei einer neuen freigegebenen Kurve muss die ID geändert werden.
- `HEAD_TYPE`: `ANY`, `*` oder ein konkreter Typ wie `STP-3001`.
- `REFERENCE_TEMP_C`: Referenztemperatur, normalerweise 25 °C. Sie muss innerhalb des Stützstellenbereichs liegen.
- `TEMP_C;CURRENT_FACTOR`: Beginn der Stützpunkttabelle.
- `CURRENT_FACTOR`: relativer benötigter LED-Strom der vollständigen Optikkette. Der absolute Strom gehört nicht in die Datei.

Die Firmware normiert alle Faktoren nochmals auf den bei `REFERENCE_TEMP_C` interpolierten Wert. Dadurch ist der Referenzfaktor intern exakt 1,0, auch wenn die Eingangsdaten geringfügig davon abweichen.

## Gültigkeits- und Plausibilitätsprüfung

Eine Systemdatei wird nur angenommen, wenn:

- Kennung und Version stimmen,
- `CURVE_ID`, Referenztemperatur und Tabellenkopf vorhanden sind,
- der angegebene Kopftyp `ANY`, `*` oder der aktuell gewählte Kopftyp ist,
- 2 bis 32 Stützpunkte vorhanden sind,
- die Temperaturen streng aufsteigend sind,
- jede Lücke höchstens 15 K beträgt,
- Temperaturen zwischen −50 und +100 °C liegen,
- Stromfaktoren zwischen 0,5 und 2,0 liegen,
- die Referenztemperatur durch den Stützstellenbereich abgedeckt ist,
- alle Werte endlich und numerisch auswertbar sind.

Bei jedem Fehler wird die Datei vollständig verworfen. Teilweise geladene Werte werden niemals verwendet.

## Interpolation und Verhalten außerhalb des Messbereichs

Innerhalb des Bereichs der Systemkurve wird linear zwischen den benachbarten Stützpunkten interpoliert.

Außerhalb des gemessenen Systembereichs wird die gemessene Randkorrektur festgehalten und nur die Steigung der internen OD-850FHT-Herstellerkurve fortgesetzt. Dadurch entsteht keine unkontrollierte lineare Extrapolation aus den letzten zwei Systemmesspunkten, gleichzeitig bleibt der bekannte Temperaturtrend der LED erhalten.

## Bindung des Lernmodells an die Grundkurve

Aus `CURVE_ID`, Kopftyp, Referenztemperatur und den normierten Stützpunkten bildet die Firmware eine 24-Bit-Kurvenkennung. Diese wird in den bereits reservierten Bytes des kopfbezogenen Lernmodells abgelegt.

- Interne Herstellerkurve: Kennung 0; vorhandene Modelle aus V0.50.1_71 bis V0.50.1_73 bleiben kompatibel.
- Externe Systemkurve: nichtnullige Kennung.
- Geänderte Systemkurve: vorhandene Lerndaten mit anderer Kennung werden nicht angewendet; für diese Kurve wird ein neues leeres Modell begonnen.

Damit können Lerndaten, die relativ zu einer alten Grundkurve entstanden sind, nicht versehentlich auf eine neue Grundkurve aufgesetzt werden.

## Vorlage im Quellpaket

Eine direkt kopierbare Formatvorlage liegt unter:

`SD_CARD_TEMPLATE/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`

Die darin enthaltenen Zahlen entsprechen nur der eingebauten OD-850FHT-Kurve und dienen als gefahrloser Format- und Funktionstest. Für die reale System-Grundkurve müssen sie durch die in der Klimakammer gemessenen relativen Stromfaktoren der vollständigen Optikkette ersetzt werden.
