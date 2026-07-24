# Kalibrierschein-Drucklayout V0.50.1_59

## Ziel

Der A4-Kalibrierschein soll fachlich zusammengehörige Inhalte nicht unnötig über Seiten trennen. Gleichzeitig wird kein künstlicher Seitenumbruch zwischen zwei vollständigen Themen erzeugt, wenn beide auf dieselbe Seite passen.

## Themenblöcke

Folgende Abschnitte werden in der Druckansicht als zusammenhängende Themen behandelt:

- aktueller Gültigkeits-/Firmwarestatus,
- Zertifikatsaussteller und Dokument,
- Systemkalibrierung einschließlich Firmwarekontext,
- Kalibrierergebnisse,
- Gerätejustierung,
- Kopfjustierung,
- kalibriertechnische Angaben beziehungsweise externer PDF-Nachweis.

Die Umsetzung verwendet `break-inside: avoid-page` und den kompatiblen Fallback `page-break-inside: avoid`. Ist ein Thema größer als eine vollständige A4-Seite, darf der Browser es weiterhin sinnvoll teilen. Bei Kalibrierergebnissen werden einzelne As-Found-/As-Left-Tabellen als eigene unteilbare Unterblöcke behandelt, sodass ein notwendiger Umbruch bevorzugt zwischen den Tabellen erfolgt.

## QR-Seite

Die Offline-Prüfung verwendet unabhängig von der vorherigen Restfläche immer `break-before: page`. Beide TP3C1-QR-Codes bleiben gemeinsam auf dieser eigenen Seite. QR-Inhalt, Größe und kryptografische Struktur wurden nicht verändert.

## Wiederholte Kalibrierschein-Nr.

Eine nur im Druck sichtbare feste Fußzeile wiederholt die Kalibrierschein-Nr. unten rechts auf jeder Seite. Der Seiteninhalt erhält zusätzlichen unteren Innenabstand, damit die Fußzeile keine Tabellen oder Hinweise überlagert.

## Unverändert

Webdarstellung außerhalb des Druckmodus, Kalibrier- und Zertifikatsdaten, Signaturen, TP3C1/TP3Q3, externer PDF-Bezug, Firmwarekontext, Messung, Regelung und Safety bleiben unverändert.
