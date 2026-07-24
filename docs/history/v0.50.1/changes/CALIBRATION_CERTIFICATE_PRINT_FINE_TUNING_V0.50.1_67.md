# Kalibrierschein-Druckfeinschliff - V0.50.1_67

## Ziel

Die in V0.50.1_66 eingeführte deutsch/englische Druckausgabe wurde anhand realer Chromium-Druckvorschauen weiter optimiert. Drei sichtbare Randfälle werden behoben:

1. Der Prüfverfahren-Zusatz unter den beiden TP3C1-QR-Codes darf nicht allein auf eine zusätzliche Seite rutschen.
2. Die Kalibrierschein-Nr. im großen Kopf der ersten Seite muss auch in Englisch vollständig in einer Zeile stehen.
3. Der deutsche Warntext darf den vollständigen Themenblock `Systemkalibrierung` nicht unnötig von Seite 1 verdrängen.

## QR-Seite

- Beide TP3C1-QR-Codes werden im A4-Druck mit `112 x 112 mm` statt `115 x 115 mm` ausgegeben.
- Überschriften, Abstand zwischen den Codes und technische Beschriftungen wurden geringfügig verdichtet.
- Der Prüfverfahren-Zusatz ist jetzt Bestandteil desselben QR-Themenblocks und wird gemeinsam mit den beiden QR-Codes auf der QR-Seite gehalten.
- Die QR-Seite bleibt eine eigene Seite und trägt weiterhin die kompakte Kalibrierschein-Nr. oben rechts sowie `Seite x von y` beziehungsweise `Page x of y` unten rechts.
- QR-Nutzdaten, Fehlerkorrektur, Modulzahl, Signaturen und Hashes bleiben unverändert. Nur die physische Druckgröße ändert sich.

## Dokumentkopf auf Seite 1

Der Kopf verwendet ein zweispaltiges Grid:

- links der Dokumenttitel und Untertitel,
- rechts `TP-3000` und die Kalibrierschein-Nr.

Die rechte Spalte bestimmt ihre Breite aus der vollständigen Nummer und darf nicht umbrechen. Bei langen englischen Titeln wird stattdessen ausschließlich der linke Titelbereich schmaler. Die Kalibrierschein-Nr. bleibt dadurch auch bei `SCAL-G00001-K30001-20260718-B03D60AA` einzeilig.

## Deutsche Seite 1

Für den Druck wurden ausschließlich die vertikalen Abstände und Schriftgrößen der Kopf-, Warn- und Tabellenbereiche moderat verdichtet. Zusätzlich wurde der deutsche Firmwarewarntext sprachlich gestrafft, ohne seine Aussage zu verändern.

Der vollständige Themenblock `Systemkalibrierung` passt dadurch in der geprüften Belegung wieder auf Seite 1. Die Regel `Themen möglichst nicht teilen` bleibt bestehen.

## Unverändert

Unverändert bleiben insbesondere:

- deutsch/englische Sprachumschaltung und Browser-Speicherung,
- Kalibrier- und Zertifikatsformate,
- TP3C1 V0.4 und TP3Q3 QR-4,
- ECDSA-P256-/SHA-256-Signaturen,
- Firmwarekontext und Anwendbarkeitslogik,
- Messung, Filterung, Regelung und Safety.
