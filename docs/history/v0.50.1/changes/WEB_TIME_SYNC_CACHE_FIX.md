# TP-3000 Web-Zeitsynchronisation – Cache-Fix

Stand: Firmware 0.50.1 / Build 0.50.1_21

## Fehlerbild

Nach dem Wechsel auf den UTC-Offset-fähigen Build meldete der Button **PC-Zeit auf Gerät übertragen** teilweise `Ungültige Zeitparameter`.

## Ursache

Der vorherige und der neue Build verwendeten denselben ETag der Web-Hauptseite. Ein Browser konnte daher mit `If-None-Match` eine Antwort `304 Not Modified` erhalten und die alte eingebettete JavaScript-Funktion weiterverwenden. Diese alte Funktion übertrug noch keinen Parameter `tz`.

## Korrektur

- HTML-ETags werden aus `TP_FIRMWARE_BUILD_ID_STRING` gebildet.
- Ein neuer Build erzwingt dadurch eine neue HTML-/JavaScript-Version.
- Ein fehlender `tz`-Parameter erzeugt eine eindeutige Aufforderung zum Neuladen mit Strg+F5.

## Erwarteter Request

```text
/set-time?y=2026&mo=7&d=12&h=23&mi=29&s=26&tz=120
```

Für Deutschland in der Sommerzeit bedeutet `tz=120`: lokale Gerätezeit = UTC + 120 Minuten.
