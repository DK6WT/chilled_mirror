# QR-Deep-Link und Druckgröße – Build 0.50.1_29

## Ursache der bisherigen Samsung-Darstellung als Text

Base45 darf unter anderem `%`, Leerzeichen, `+`, `/` und `:` enthalten. Unkodiert besitzen diese Zeichen innerhalb einer URI eine Sonderbedeutung. Insbesondere ein `%`, dem keine zwei hexadezimalen Zeichen folgen, macht den Deep Link syntaktisch ungültig.

## Umsetzung

Der kryptografische Rohpayload bleibt unverändert:

```text
TP3Q3:<Base45 aus den bestehenden komprimierten und signierten Daten>
```

Nur für die QR-Linkdarstellung wird der Fragmentteil mit `encodeURIComponent()` kodiert:

```text
tp3000://verify#<URI-kodierte Darstellung des unveränderten TP3Q3-Rohpayloads>
```

Android `Uri.getFragment()` dekodiert diese Transportdarstellung und liefert dem Verifier-Prüfbaum wieder den ursprünglichen `TP3Q3:`-Text. Die `.tpqr`-Datei bleibt unverändert im Rohformat.

## QR-Kodierung und Größe

- Deep-Link-Präfix: QR-Byte-Modus
- URI-kodierter Fragmentteil: QR-Alphanumerikmodus
- Fehlerkorrektur: L wie bisher
- automatische QR-Version: weiterhin aktiv
- Bildschirmbreite: maximal 372 px statt 310 px
- A4-Druckbreite: 98 mm
- zwei Teile: untereinander auf einer eigenen A4-Seite, beide jeweils 98 mm
- Quiet Zone: weiterhin vier Module über die SVG-ViewBox

Damit wird der Code exakt 20 % größer dargestellt, ohne bei einem zweiteiligen Nachweis einen der beiden Codes verkleinern zu müssen.
