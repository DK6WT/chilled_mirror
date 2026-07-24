# ADC2-Dunkelwert bei jeder Optik-Auto-Cal – V0.50.1_70

Basis ist V0.50.1_69. Die interne Firmwareversion bleibt `0.50.1`, die Build-ID ist `0.50.1_70`.

## Änderung

Der ADC2-Dunkelwert wurde bisher nur während der ADS1263-Initialisierung gemessen. Die Optikdiagnose verglich bei späteren Auto-Cal-Durchläufen deshalb wiederholt denselben Startwert und konnte keine reale Dunkelwertdrift zwischen zwei Auto-Cals erkennen.

Beim Eintritt in den Zustand `LED-AUTOEICHUNG` wird nun `ads1263Adc2MeasureDark()` ausgeführt. Die Funktion:

1. schaltet die IR-LED über `LED_OFF` sicher aus,
2. wartet 50 ms auf das Einschwingen des Dunkelsignals,
3. mittelt 64 ADC2-Stichproben im Abstand von 2,5 ms,
4. übernimmt den neuen Dunkelwert und Zeitstempel,
5. setzt den ADC2-Lichtpuffer zurück und
6. lässt die LED ausgeschaltet, bis `setTargetCurrent()` die Auto-Cal wieder freigibt.

Erst nach Abschluss dieser Messung werden Auto-Cal-Startzeit, 50-ms-Schritttakt und 10-s-Timeout neu gesetzt. Damit wird kein Schritt aufgrund des unmittelbar zuvor zurückgesetzten ADC2-Puffers ausgelöst und die Dunkelmessung verkürzt das Auto-Cal-Zeitbudget nicht.

Kann der Dunkelwert nicht erneuert werden, bleibt der zuletzt gültige Wert erhalten. Die Firmware protokolliert eine Warnung und führt die Auto-Cal weiter aus.

Optikziel und LED-Regelung auf den ADC2-Bruttowert bleiben bewusst unverändert.
