# TP-3000 V0.50.1_28 – Android-Deep-Link für Kalibrierungs-QR

## Ziel

Die auf dem Kalibrierschein dargestellten QR-Codes öffnen über die Samsung-Kamera die installierte Android-App **TP-3000 Verifier**. Vor den bereits erzeugten QR-Nutzinhalt wird ausschließlich folgender feste Präfix gesetzt:

```text
tp3000://verify#
```

Der dekodierte QR-Inhalt lautet damit beispielsweise:

```text
tp3000://verify#TP3Q3:<unveränderter bisheriger Payload>
```

Für mehrteilige Codes wird derselbe Präfix vor jeden einzelnen bisherigen Teil gesetzt.

## Unveränderte kryptografische Daten

Nicht verändert werden:

- TP3Q3-Nutzinhalt und Teilhüllen
- Signaturen
- Manifest- und Dokumenthashes
- DEFLATE/ZLIB-Komprimierung
- Base45
- Teilnummern und gemeinsame Dokument-ID

## QR-Kodierung

Der Deep-Link-Präfix enthält Kleinbuchstaben und muss deshalb im QR-Byte-Modus kodiert werden. Würde Präfix plus Nutzinhalt als ein einziges Segment übergeben, würde auch der komplette Base45-Text in den weniger effizienten Byte-Modus wechseln.

V0.50.1_28 verwendet deshalb zwei unmittelbar aufeinanderfolgende QR-Segmente:

1. `tp3000://verify#` im Byte-Modus
2. unveränderter `TP3Q3:...`-Payload weiterhin im QR-Alphanumerikmodus

Scanner liefern beide Segmente ohne Trennzeichen als exakt einen Text zurück. Dies hält die QR-Dichte nahezu auf dem bisherigen Stand und fügt nur den tatsächlichen Präfix- und Segment-Overhead hinzu.

## Rohdatenkompatibilität

Die Schaltfläche zum Herunterladen der `.tpqr`-Datei speichert weiterhin ausschließlich den bisherigen Roh-Payload `TP3Q3:...`. Damit bleiben bestehende Datei- und Rohdatenprüfpfade erhalten.

## App-Test

Ziel-App: **TP-3000 Verifier V0.3.3**

Erwarteter Ablauf:

1. Samsung-Kamera erkennt den QR-Code als `tp3000://`-Link.
2. Antippen öffnet TP-3000 Verifier.
3. Die App liest den Fragmentinhalt hinter `#`.
4. Der vorhandene Prüfbaum erhält unverändert `TP3Q3:...`.
