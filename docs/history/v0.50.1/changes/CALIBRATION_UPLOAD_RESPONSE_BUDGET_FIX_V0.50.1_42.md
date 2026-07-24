# TP-3000 V0.50.1_42 – Kalibrierungsupload: Antwortbudget

## Fehlerbild

Beim Hochladen einer `.tpdcal`, `.tphcal` oder `.tpscal` konnte der Browser nach längerer Prüfung melden:

```text
Upload fehlgeschlagen: TypeError: Failed to fetch
```

Der Upload konnte dabei bereits vollständig geprüft, archiviert und aktiviert worden sein. Nur die kleine HTTP-Antwort wurde nicht mehr übertragen.

## Ursache

`EthBoundedWriter` begrenzt das Schreiben kleiner HTTP-Antworten auf 120 ms. Dieses Budget begann bisher schon mit Eingang des Requests. Seit der kopfgebundenen Zertifikatsauswahl kann ein Kopfimport zusätzlich das Kalibrierarchiv durchsuchen, historische Kopf-/Systempakete prüfen und gegebenenfalls gemeinsam aktivieren. Diese Arbeit dauert auf SD länger als 120 ms.

## Korrektur

Unmittelbar nach Abschluss der jeweiligen langen Operation wird `client.restartBudget()` aufgerufen:

- Gerätejustierung hochladen
- Kopfjustierung hochladen
- Systemkalibrierung hochladen
- Geräte-/Kopfjustierung direkt von SD importieren
- externen PDF-Upload abschließen

Erst danach wird die Erfolgs- oder Fehlermeldung geschrieben. Fachliche Fehler bleiben unverändert und werden nun zuverlässig als Text an den Browser übertragen.

## Sicherheitswirkung

Die Korrektur verändert keine Prüfentscheidung und keine gespeicherten Daten. Sie verhindert ausschließlich, dass eine bereits ermittelte Antwort wegen eines abgelaufenen Netzwerk-Schreibbudgets verloren geht.
