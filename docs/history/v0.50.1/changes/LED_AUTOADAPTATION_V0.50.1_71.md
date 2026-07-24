# LED-Autoadaption – V0.50.1_71

Basis ist V0.50.1_70. Die interne Firmwareversion bleibt `0.50.1`, die Build-ID ist `0.50.1_71`.

## Betriebsarten

- **Aus:** keine temperaturabhängige LED-Stromvorsteuerung. Nach dem Umschalten wird weich auf den Strom der letzten erfolgreichen Auto-Cal zurückgefahren.
- **Ein – Grundkurve:** ausschließlich die fest eingebaute typische Temperaturkurve der OD-850FHT wird relativ zur Temperatur der letzten Auto-Cal angewendet.
- **Selbstlernend:** Grundkurve plus begrenzte kopfbezogene Formkorrektur aus realen Auto-Cal-Daten.

Bei vorhandener SD-Karte wird in allen drei Modi gelernt. Die Auswahl steuert nur, ob die Grundkurve und die Lerndaten auf den LED-Strom angewendet werden. Ein Wechsel des Modus löscht keine Lerndaten und das Gerät wechselt niemals selbständig auf `Selbstlernend`.

## Temperaturgröße und Grundkurve

Verwendet wird die externe T-Umgebung, weil die vorhandene T-Kopf-Messung die Peltier-Heißseite erfasst und damit stark von der momentanen Peltierleistung beeinflusst wird. T-Umgebung wird mit einer Zeitkonstante von 120 s tiefpassgefiltert.

Die typische Datenblattkurve wird durch die Stützstellen `-50/1,40`, `-25/1,27`, `0/1,13`, `25/1,00`, `50/0,86`, `75/0,72`, `100/0,60` abgebildet. Die Werte beschreiben relative optische Leistung bei konstantem Strom. Für die Vorsteuerung wird ihr Kehrwert als relativer Stromfaktor verwendet und zwischen den Punkten linear interpoliert.

Der absolute Bezug ist immer der zuletzt von der echten LED-Auto-Cal gefundene Strom. Damit bestimmen Grundkurve und Lernmodell ausschließlich die kleine Änderung zwischen zwei Auto-Cals.

## Lernprinzip

Aufeinanderfolgende erfolgreiche Auto-Cals liefern jeweils gefilterte T-Umgebung und tatsächlich benötigten LED-Strom. Aus dem Verhältnis zweier Auto-Cals wird die Abweichung vom Verhältnis der Datenblatt-Grundkurve berechnet. Dadurch wird nur der verbleibende Temperaturgang gelernt; absolute LED-Alterung, Spiegelzustand und mechanische Änderungen bleiben über den jeweils letzten Auto-Cal-Strom verankert.

Der erste erfolgreiche Auto-Cal nach einem Neustart setzt nur den RAM-Lernanker. Ein fehlgeschlagener oder abgebrochener Auto-Cal wird weder gelernt noch als neuer Vorsteuerungsanker verwendet.

Ein Lernpunkt ist nur zulässig, wenn:

- beide Temperaturen und Ströme gültig und innerhalb der Arbeitsgrenzen liegen,
- der LED-Strom nicht an 2 mA oder 80 mA liegt,
- der Trend der gefilterten T-Umgebung höchstens 0,20 K/min beträgt,
- rohe und gefilterte T-Umgebung höchstens 2 K auseinanderliegen.

## Klassen, Gewichtung und Grenzen

Das Modell verwendet 1-K-Klassen von -40 bis +85 °C.

| bestätigte Werte einer Klasse | wirksames Lerngewicht |
|---:|---:|
| 0–2 | 0 % |
| 3–5 | 25 % |
| 6–9 | 50 % |
| 10–19 | 75 % |
| ab 20 | 90 % |

Die Grundkurve bleibt damit immer zu mindestens 10 % beteiligt. Bei einer Interpolation gilt das kleinere Gewicht beider Stützstellen.

- Lineare Interpolation nur mit einer Stützstelle unterhalb und oberhalb, jeweils höchstens 3 K entfernt und mit maximal 6 K Gesamtlücke.
- Keine Extrapolation einer gelernten Steigung.
- Außerhalb beziehungsweise in einer zu großen Lücke wird nur der nächste Randwert verwendet und über 3 K linear auf null Lernanteil ausgeblendet.
- Gelernter Formfaktor maximal ±5 % gegenüber der Grundkurve.
- Gesamter wirksamer LED-Strom maximal ±3 % gegenüber dem letzten erfolgreichen Auto-Cal-Strom.
- Änderungsrate maximal 0,02 % des Auto-Cal-Stroms pro Sekunde.
- Ab fünf Werten einer Klasse benötigt ein neuer Wert mit mehr als 1 % Abweichung drei zueinander passende Bestätigungen.

## Speicherung und Kopfbindung

Die Daten werden vorerst unter `/LEDADAPT` auf SD gespeichert. Der Dateischlüssel bindet Kopftyp und fünfstellige Kopf-SN. Zwei alternierende Binärslots mit Sequenznummer, Strukturversion und CRC32 schützen gegen einen unvollständigen Schreibvorgang. Modell- und Prüfpuffer liegen in RAM2.

`LED-Lerndaten zurücksetzen` entfernt nach Bestätigung ausschließlich die beiden Slots des aktuell gewählten Kopfs und initialisiert dessen RAM-Modell neu. Andere Kopftypen und Kopf-SNs bleiben unberührt.

## Verhalten ohne SD

Ohne verfügbare SD-Karte ist ausschließlich `Aus` zulässig. Das gilt für TFT-Auswahl, Web-Auswahl und Backendprüfung. Ein gespeicherter anderer Modus wird beim Start beziehungsweise bei einem erkannten SD-Fehler auf `Aus` gesetzt. Bei aktivierter Adaption wird das Vorhandensein des Mediums zusätzlich alle 30 s geprüft; ein Entfernen der Karte wird damit spätestens bei dieser Prüfung oder beim nächsten SD-Zugriff erkannt. Der letzte echte Auto-Cal-Anker bleibt erhalten, damit der LED-Strom weich auf den unkompensierten Wert zurückkehrt.

Die Verlagerung des Lernmodells in den internen 64-MiB-QSPI-Flash ist für einen späteren Stand vorgesehen und in V0.50.1_71 noch nicht umgesetzt.
