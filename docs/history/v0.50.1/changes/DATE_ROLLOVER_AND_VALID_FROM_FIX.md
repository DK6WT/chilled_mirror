# TP-3000 Datumswechsel- und Gültig-ab-Korrektur

## Gerätejustierung

Der bisherige Code ersetzte ein nicht gesetztes Referenzdatum beim Erzeugen und Prüfen einer Gerätejustierung durch das jeweils aktuelle Tagesdatum. Eine am 12.07. exportierte Anfrage enthielt dadurch den 12.07.; nach Mitternacht wurde derselbe unveränderte Gerätezustand intern mit dem 13.07. verglichen und fälschlich als abweichend markiert.

Korrektur in Build 0.50.1_24:

- kein flüchtiger Ersatz durch das aktuelle Tagesdatum,
- Aktivvergleich ausschließlich über wirksame Referenz- und Kanalkorrekturwerte,
- neue Anfrage nur mit persistent gespeichertem Referenz-/Justierungsdatum.

## Gültig ab

Das KeyGen setzte die Standardgültigkeit bislang auf 00:00:00 UTC des Kalibriertages. In Deutschland während UTC+02:00 begann die Freigabe damit erst um 02:00 Ortszeit. Ab KeyGen V0.7.5 beginnt die Standardgültigkeit mit dem tatsächlichen Freigabezeitpunkt in UTC.
