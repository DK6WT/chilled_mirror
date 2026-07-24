# TP-3000 V0.50.1_77 – SD-Überwachung der LED-Autoadaption

## Ziel

Die LED-Autoadaption soll eine entfernte oder fehlerhafte SD-Karte sicher erkennen, ohne parallel zum aktiven Messwertlogger unnötige Medienabfragen auszuführen. Da das Auto-Cal-Intervall frei geändert werden kann, reicht eine ausschließliche Prüfung bei Auto-Cal nicht aus.

## Laufzeitlogik

### Messwertlogging aktiv

Ist das SD-Messwertlogging eingeschaltet, fragt die LED-Autoadaption ausschließlich den bereits vom Logger gepflegten Laufzeitstatus über `sdLogGetStatus()` ab. Sie startet in diesem Zustand keine eigene zyklische `SD.mediaPresent()`-Prüfung.

Die Statuswerte `Keine Karte` und `Dateifehler` lösen sofort den vorhandenen Fallback aus. Während `Initialisiere` oder eines vorübergehenden Retry-Zustands bleibt ein bereits gültig geladener Stand erhalten, bis der Logger einen eindeutigen Fehler meldet.

### Messwertlogging gestoppt

Ohne aktives Messwertlogging wird die SD-Karte:

- beim Start,
- bei bewusster Wahl von `Selbstlernend`,
- bei jedem tatsächlichen Lese- oder Schreibzugriff,
- zu Beginn jeder Auto-Cal und
- ansonsten spätestens alle fünf Minuten

geprüft.

Die Konstante lautet:

```cpp
LED_ADAPT_MEDIA_CHECK_INTERVAL_MS = 300000UL;
```

Eine Auto-Cal zieht die Prüfung vor und startet das Fünf-Minuten-Raster neu. Damit entstehen bei Auto-Cal-Intervallen von 10 oder 15 Minuten klare, gleichmäßige Prüfzeitpunkte.

## Fallback

Bei erkannter Kartenentnahme oder einem Dateifehler:

1. bleibt der letzte gültige Auto-Cal-Anker erhalten,
2. wird das kopfbezogene Lernmodell nicht mehr angewendet,
3. wird eine externe System-Grundkurve verworfen,
4. fällt `Selbstlernend` auf `Ein - Grundkurve` zurück,
5. verwendet das Gerät die interne OD-850FHT-Herstellerkurve,
6. wird der resultierende LED-Strom weiterhin über die vorhandene Stromrampe angefahren.

Beim Wiedereinsetzen der Karte werden gültige Dateien wieder geladen, der Bedienmodus wechselt jedoch nicht automatisch zurück auf `Selbstlernend`.

## Unverändert

Auto-Cal-Messablauf, Dunkelwertbestimmung, absoluter Stromanker, 161-Klassen-Modell, A/B-CRC-Persistenz, Systemkurvenformat, Kurvenbindung, Lerngewichtung, Stromgrenzen und Regelung bleiben unverändert.
