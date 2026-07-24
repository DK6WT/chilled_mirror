# TP-3000 V0.50.1_57 – Download der Systemkalibrierungsanfrage

## Fehlerbild

Beim Klick auf **„Anfrage herunterladen (.tpscalreq)“** begann Chromium den Download mit dem richtigen Dateinamen, brach ihn anschließend jedoch ab und zeigte im Downloadverlauf:

```text
Internetverbindung prüfen
```

## Ursache

`EthBoundedWriter` begrenzt kleine direkte HTTP-Antworten auf 120 ms. Das Budget begann bereits beim Eingang des Requests. Eine Systemkalibrierungsanfrage führt vor der Antwort mehrere vergleichsweise lange Schritte aus:

1. Systemzustand und aktive Justierungen prüfen,
2. kanonische Anfrage erzeugen und SHA-256 bilden,
3. Anfrage mit dem Geräteschlüssel ECDSA-P256 signieren,
4. gerätesignierten Firmwarekontext erzeugen und auf SD schreiben,
5. Anfrage zusätzlich unter `/CALIBRATION/REQUESTS/` speichern.

Dadurch war das 120-ms-Budget beim Beginn der HTTP-Dateiübertragung teilweise oder vollständig aufgebraucht. Die Header mit Dateiname und `Content-Length` konnten noch beim Browser ankommen, während der JSON-Body anschließend abgebrochen wurde. Der Browser wertete die kürzere Übertragung korrekt als Netzwerkfehler.

## Korrektur

Unmittelbar nach Abschluss der Erzeugungs-, Signatur- und SD-Schritte und **vor dem ersten HTTP-Header** wird `client.restartBudget()` aufgerufen. Das 120-ms-Budget gilt damit nur noch für die eigentliche Übertragung zum Browser.

Dieselbe Absicherung wurde vorsorglich ergänzt für:

- Gerätejustierungsanfrage `.tpdcalreq`,
- Kopfjustierungsanfrage `.tphcalreq`,
- Systemkalibrierungsanfrage `.tpscalreq`,
- Firmwareanfrage `.tpfwreq`,
- zugehörige Fehlerantworten.

## Datenintegrität

Anfrageinhalt, Gerätesignatur und SD-Kopie werden vor dem HTTP-Download erzeugt und bleiben unverändert. Eine in Build 56 im Browser abgebrochene Anfrage wurde daher mit hoher Wahrscheinlichkeit trotzdem vollständig unter `/CALIBRATION/REQUESTS/` gespeichert.

## Unverändert

Kanonische Formate, Firmwarekontext, Kalibrier- und Firmwarezertifikate, KeyGen V0.7.13, TP3C1/TP3Q3, Messung, Regelung und Safety wurden nicht verändert.
