# Web-QR-Stream-Fix

Stand: Firmware 0.50.1 / Build 0.50.1_22

## Fehlerbild

Auf dem druckbaren Kalibrierschein blieb dauerhaft `QR-Code wird erzeugt …` stehen. Die Kalibrierdaten selbst wurden korrekt geladen und dargestellt.

## Ursache

Der Browsercode schrieb zuerst sämtliche UTF-8-Daten in die beschreibbare Seite eines `CompressionStream` und wartete auf `writer.close()`. Erst danach wurde die lesbare Seite konsumiert. Bei hinreichend großen beziehungsweise wenig komprimierbaren signierten JSON-Paketen konnte die interne Ausgabewarteschlange voll werden. Der Schreib-/Close-Pfad wartete dann auf einen Leser, während der Leser erst nach Abschluss des Schreibens gestartet werden sollte.

## Korrektur

Die Eingabedaten werden nun über eine gleichzeitig konsumierte Stream-Pipeline verarbeitet:

```javascript
const source = new Blob([new TextEncoder().encode(text)]).stream();
const compressed = source.pipeThrough(new CompressionStream('deflate'));
const bytes = new Uint8Array(await new Response(compressed).arrayBuffer());
```

Zusätzlich bleibt der Druckknopf bis zum erfolgreichen QR-Aufbau deaktiviert. QR-Fehler erscheinen direkt im Abschnitt `Offline-Prüfung`, ohne den restlichen Kalibrierschein zu entfernen.

## Unverändert

- Transportformat `TP3Q1`
- ZLIB/DEFLATE und Base45
- ECDSA-P256/SHA-256
- Offlineprüfung mit TP3000KeyTool V0.7.3
- Daten- und Signaturformate
