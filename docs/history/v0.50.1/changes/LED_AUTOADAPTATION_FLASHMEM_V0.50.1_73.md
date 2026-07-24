# LED-Autoadaption – FLASHMEM-Auslagerung V0.50.1_73

Basis ist V0.50.1_72. Die interne Firmwareversion bleibt `0.50.1`, die Build-ID ist `0.50.1_73`.

## Ziel

Die mit V0.50.1_71/72 hinzugekommene LED-Autoadaptionslogik soll die knappe ITCM-/RAM1-Code-Reserve des Teensy 4.1 nicht dauerhaft belegen. Die Modell-, Interpolations-, Lern-, SD- und Diagnosefunktionen sind nicht Teil der schnellen 16-Hz-Regelstrecke und werden deshalb aus dem Program-Flash ausgeführt.

## Umsetzung

- Sämtliche Hilfsfunktionen in `TPledAdaptation.cpp` sind mit `FLASHMEM` markiert.
- Auch Initialisierung, Auto-Cal-Lernpfad, Modusverwaltung, SD-Modellzugriff, Rücksetzen und Diagnoseabfragen liegen in `FLASHMEM`.
- `ledAdaptationTask()` bleibt als sehr kleiner Einstieg im ITCM, weil er aus jeder Hauptschleife aufgerufen wird.
- Der ITCM-Einstieg prüft nur `millis()` und ein 250-ms-Dispatchfenster.
- Die eigentliche langsame Verarbeitung erfolgt in der explizit `noinline` markierten Funktion `ledAdaptationTaskSlow()` in `FLASHMEM`.
- Der 120-s-Temperaturtiefpass wird damit weiterhin viermal pro Sekunde bedient. Die LED-Stromrampe bleibt unverändert auf einen wirksamen Schritt pro Sekunde begrenzt.

## Unverändert

Lernmodell, SD-A/B-Dateiformat, CRC32, Kopfbindung, Grundkurve, Lerngewichte, effektive Historie von 128 Werten, Grenzwerte, Auto-Cal-Ablauf, ADC2-Dunkelmessung, Peltierregelung und Safety bleiben funktional unverändert.

## Erwartung und Prüfung

Die reale Einsparung ist erst im vollständigen Teensyduino-Linklauf sichtbar. Erwartet wird, dass der überwiegende Teil der durch V0.50.1_71/72 hinzugekommenen ITCM-Codegröße wieder als `padding` erscheint. Entscheidend ist die reale `Memory Usage on Teensy 4.1`; ein Host-Syntaxtest kann die Abschnittszuordnung, aber nicht die endgültige Teensy-Linkgröße bestätigen.
