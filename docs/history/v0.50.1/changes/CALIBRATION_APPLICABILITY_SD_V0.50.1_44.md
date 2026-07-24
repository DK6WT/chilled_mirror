# TP-3000 – Anwendbarkeitsende von Systemkalibrierungen auf SD

## Stand

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_19.zip`
- neuer Paketstand: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_20.zip`
- interne Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_44`

## Ziel

Der signierte Systemkalibrierschein bleibt unverändert und kryptografisch valide. Sobald eine kalibrierrelevante Änderung seine Bindung an den tatsächlichen Gerätezustand beendet, wird der erste bekannte Zeitpunkt getrennt in einer zentralen SD-Datei festgehalten.

Datei:

```text
/CALIBRATION/APPLICABILITY.TPS
```

Die Speicherzugriffe sind in der signierten Kalibrierungsverwaltung gekapselt. Ein späterer Wechsel auf den internen Teensy-/SPI-Flash kann deshalb ohne Änderung der signierten Kalibrierformate erfolgen.

## Dateiformat

Header:

```text
TP3000-CAL-APPLICABILITY-1
```

Ein Datensatz pro dauerhaft beendeter Systemkalibrierung:

```text
<SystemManifestSha256>|<ApplicableUntilUtc>|<ReasonCode>|<RecordSha256>
```

Der Datensatz-Hash wird über folgende kanonische Bytefolge gebildet:

```text
TP3000-CAL-APPLICABILITY-1\n
<SystemManifestSha256>\n
<ApplicableUntilUtc>\n
<ReasonCode>\n
```

Für jeden Systemschein gilt der zeitlich erste korrekte Ende-Datensatz. Spätere doppelte Einträge können das frühere Ende nicht nach hinten verschieben.

## Erfasste Änderungen

Die zentralen Speicherpfade für Hauptkonfiguration und Metrologieblock melden jede erfolgreiche Speicherung an die Kalibrierungsverwaltung. Dort wird immer erst der tatsächliche Zustand verglichen; nicht kalibrierrelevante Änderungen erzeugen keinen Datensatz.

- `HEAD_VALUES_CHANGED` – aktuelle Kopfjustierwerte weichen ab
- `DEVICE_VALUES_CHANGED` – aktuelle Gerätejustierwerte weichen ab
- `HEAD_ADJUSTMENT_CHANGED` – andere signierte Kopfjustierung aktiviert
- `DEVICE_ADJUSTMENT_CHANGED` – andere signierte Gerätejustierung aktiviert
- `SYSTEM_CALIBRATION_REPLACED` – durch neue Systemkalibrierung ersetzt

Ein reiner Wechsel auf einen anderen Kopf beendet die Kalibrierung des vorherigen Kopfes nicht dauerhaft. Wird derselbe unveränderte Kopf erneut angeschlossen, darf sein weiterhin anwendbarer Schein wieder ausgewählt werden.

Ein zeitweilig fehlendes oder nicht lesbares externes Original-PDF erzeugt ebenfalls kein dauerhaftes Ende. Es blockiert nur die aktuelle vollständige Verifikation.

## Transaktionsverhalten

Bei einem Justierungsimport wird ein Ende-Datensatz erst geschrieben, nachdem die neue Justierung:

1. kryptografisch geprüft,
2. als aktive Datei zurückgelesen,
3. mit den aktuellen Werten verglichen und
4. im Historienarchiv gesichert wurde.

Schlägt einer dieser Schritte vorher fehl, bleibt der bisherige Systemschein unverändert anwendbar.

Beim Schreiben wird die Datei geschlossen und anschließend vollständig zurückgelesen. Eine durch Stromausfall unvollständig gebliebene letzte Zeile wird ignoriert. Jede vollständig abgeschlossene Zeile muss syntaktisch korrekt sein und ihren SHA-256 bestätigen; andernfalls wird die Statusdatei als fehlerhaft behandelt und kein betroffener Schein automatisch aktiviert.

## Webanzeige

Aktueller Schein:

```text
Aktiv – gültig
```

Historischer, weiterhin valider, aber nicht mehr anwendbarer Schein mit gespeichertem Endzeitpunkt:

```text
Valide – bis 16.07.2026 10:35 UTC
```

Ist bei einem bereits vor diesem Build entstandenen Altzustand noch kein Endzeitpunkt gespeichert, wird er trotzdem nicht wie ein aktiver Schein dargestellt:

```text
Valide – andere Justierung
Valide – nicht aktiv
```

Zusätzlich wird bei gespeichertem Endzeitpunkt der kurze Grund angezeigt, beispielsweise:

```text
Kopfjustierwerte geändert
```

## Sicherheitsgrenze dieses ersten Schritts

Die SD-Datei schützt gegen unbeabsichtigtes Wiederaktivieren, Dateibeschädigung und normale Bedienfehler. Da sie noch auf derselben austauschbaren SD-Karte liegt, ist sie kein vollständiger Schutz gegen einen gezielten Rollback mit zuvor kopierter SD-Karte. Der vorgesehene nächste Härtungsschritt ist die unveränderte Übernahme dieses Zustands in internen Flash beziehungsweise einen geschützteren Speicher.

Änderungen, die vor Installation dieses Builds stattgefunden haben, können nicht rückwirkend mit einem exakten Zeitpunkt rekonstruiert werden.
