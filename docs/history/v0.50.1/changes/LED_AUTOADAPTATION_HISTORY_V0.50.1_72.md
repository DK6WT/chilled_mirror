# LED-Autoadaption – begrenzte effektive Historie V0.50.1_72

Basis ist V0.50.1_71. Die interne Firmwareversion bleibt `0.50.1`, die Build-ID ist `0.50.1_72`.

## Ziel

Die bestehende kopfbezogene LED-Autoadaption soll reale Alterung und langsame Änderungen nachführen, ohne wertvolle seltene Temperaturklassen allein wegen ihres Alters zu löschen. Vertrauen in eine Klasse und Einfluss alter Messwerte werden deshalb getrennt behandelt.

## Vertrauenszählung

`count` bleibt die bestätigte Belegung einer 1-K-Temperaturklasse und steigt gesättigt bis 1000. Die vorhandenen Anwendungsgewichte bleiben unverändert:

- 0–2 Werte: 0 % Lerndaten
- 3–5 Werte: 25 %
- 6–9 Werte: 50 %
- 10–19 Werte: 75 %
- ab 20 Werten: 90 %

Damit verliert eine gut belegte Klasse ihre Freigabe nicht, nur weil die Mittelwertnachführung adaptiver wird.

## Effektive Historie

Bis einschließlich 128 bestätigten Werten wird der normale laufende Mittelwert verwendet. Danach bleibt der Divisor auf 128 begrenzt:

```text
mean = mean + (newValue - mean) / 128
```

Jeder neue gültige Auto-Cal-Wert derselben Temperaturklasse hat damit rund 0,78125 % Einfluss. Der Einfluss des vorherigen Modells halbiert sich nach rund 89 neuen gültigen Werten derselben Klasse.

Die Streuungsreserve `m2Shape` wird ab diesem Punkt ebenfalls exponentiell mit derselben Historienlänge begrenzt, damit sie nicht unbegrenzt anwächst. Sie wird derzeit noch nicht zur Stromvorsteuerung verwendet.

## Persistenz und Kompatibilität

Das Binärformat der SD-Modelle bleibt unverändert. Bereits vorhandene V0.50.1_71-Modelle werden weiter geladen. Ihr bisheriger Mittelwert ist der Startzustand; ab dem nächsten akzeptierten Lernwert gilt die neue 128-Werte-Nachführung.

Es gibt weiterhin keine automatische Löschung nach Tagen oder Monaten. Ein LED-/Photodiodentausch, mechanischer Umbau oder eine grundlegende Reinigung kann bewusst über `LED-Lerndaten zurücksetzen` nur für den aktuell ausgewählten Kopf behandelt werden.
