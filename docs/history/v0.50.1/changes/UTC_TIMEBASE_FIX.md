# TP-3000 UTC-Zeitbasis – Korrektur 2026-07-12

## Ursache

Die Geräte-RTC und die Anzeige des TP-3000 laufen bewusst in lokaler Zeit. Bis Firmware V0.50.1_6 wurde diese lokale Zeit beim Export einer Kalibrieranfrage jedoch als `CreatedUtc` mit `+00:00` gekennzeichnet. In Deutschland während der Sommerzeit lag die Anfrage dadurch zwei Stunden in der Zukunft. Das KeyTool lehnte die Signierung korrekt ab, zeigte aber nur die allgemeine 72-Stunden-Meldung.

## Korrektur

Firmware V0.50.1_7 / Build 0.50.1_20:

- Der Browser überträgt beim PC-Zeitsync zusätzlich den aktuellen UTC-Offset.
- Die lokale RTC- und Anzeigezeit bleiben unverändert.
- Der UTC-Offset wird im vorhandenen Device-/UI-A/B-EEPROM-Block gespeichert.
- Kryptografische Zeitstempel werden als lokale RTC-Zeit minus UTC-Offset erzeugt.
- Gültigkeitsprüfungen verwenden dieselbe UTC-Zeitbasis.
- Ohne gültigen UTC-Offset wird keine Kalibrieranfrage erzeugt.

KeyGen V0.7.3:

- Prüft das 72-Stunden-Zeitfenster bereits beim Öffnen der Anfrage.
- Meldet zukünftige Gerätezeit ausdrücklich als UTC-/Zeitsynchronisationsproblem.
- Meldet tatsächlich abgelaufene Anfragen getrennt.

## Nach dem Update

1. Firmware V0.50.1_7 flashen.
2. Weboberfläche öffnen und `PC-Zeit auf Gerät übertragen` einmal drücken.
3. Die alte, mit V0.50.1_6 erzeugte `.tpdcalreq`- oder `.tphcalreq`-Datei verwerfen.
4. Eine neue Kalibrieranfrage exportieren.
5. Die neue Anfrage mit KeyGen V0.7.3 öffnen und signieren.

Die alten Anfragen werden nicht automatisch umgedeutet, weil ihr signierter Zeitstempel bereits Bestandteil der kryptografischen Anfrage ist.
