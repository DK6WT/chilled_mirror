# TP-3000 – transportabler Anwendbarkeitsanhang in `.tpscal`

## Stand

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_20.zip`
- neuer Paketstand: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_21.zip`
- interne Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_45`

## Ziel

Der erste UTC-Zeitpunkt, ab dem eine weiterhin kryptografisch valide Systemkalibrierung nicht mehr zur Geräte-/Kopfkonfiguration passt, bleibt zentral in

```text
/CALIBRATION/APPLICABILITY.TPS
```

gespeichert. Zusätzlich wird derselbe Datensatz direkt an die betreffende aktive beziehungsweise archivierte `.tpscal` angehängt. Dadurch bleibt die Information beim Kopieren oder späteren Export der Datei erhalten.

## Dateiaufbau

Der signierte JSON-Bereich bleibt unverändert am Dateianfang. Danach folgt optional:

```text
--TP3000-APPLICABILITY-1--
ManifestSha256=<64 Hex-Zeichen>
ApplicableUntilUtc=<Unixzeit UTC>
ReasonCode=<fester Ereigniscode>
RecordSha256=<64 Hex-Zeichen>
--TP3000-APPLICABILITY-END--
```

`RecordSha256` verwendet dieselbe kanonische SHA-256-Bildung wie der zentrale Datensatz in `APPLICABILITY.TPS`.

## Prüfung

- Signatur-, Manifest-, TP3C1-, TP3Q3-, TPSIG- und TPLOG-Prüfungen verwenden ausschließlich das JSON vor `--TP3000-APPLICABILITY-1--`.
- Web-JSON-Endpunkte liefern ebenfalls nur den signierten JSON-Bereich, damit die Antwort gültiges JSON bleibt.
- Der Anhang wird separat auf Manifest, UTC-Zeit, Ereigniscode und SHA-256 geprüft.
- Die zentrale Datei bleibt auf dem Gerät die maßgebliche Quelle. Fehlende Anhänge werden beim Anzeigen beziehungsweise beim nächsten Zugriff nachgeführt.

## Stromausfallsicherheit

Der Anhang wird nicht direkt in die Originaldatei geschrieben. Der Ablauf lautet:

1. Originaldatei nach `*.apptmp` kopieren.
2. Anhang an die Temp-Datei schreiben und flushen.
3. Temp-Datei vollständig zurücklesen und prüfen.
4. Original nach `*.appbak` umbenennen.
5. Temp-Datei atomar auf den Originalnamen umbenennen.
6. Ergebnis erneut prüfen und erst dann das Backup löschen.

Zurückgebliebene Temp-/Backup-Dateien aus einer unterbrochenen Rename-Phase werden beim nächsten Versuch wieder aufgenommen oder auf den unveränderten Altstand zurückgesetzt.

## Darstellung im Kalibrierschein

Bei einem nicht mehr anwendbaren Zertifikat steht der Zusatz direkt im Abschnitt `Systemkalibrierung`:

```text
Gültig bis       13.07.2027 23:59:59 UTC   (durchgestrichen)
Anwendbar bis    16.07.2026 10:35:00 UTC
Status           Valide – nicht mehr anwendbar
Grund            Kopfjustierung geändert
```

Auf dem Bildschirm sind die zusätzlichen Angaben orange. Im Druck/PDF bleiben sie schwarz; das ursprüngliche Gültigkeitsende bleibt lesbar und wird nur mit einer dünnen horizontalen Linie durchgestrichen.

## Unverändert

Nicht geändert wurden:

- der signierte JSON-Inhalt,
- kanonische Signaturbytes und Manifestbildung,
- ECDSA P-256 / SHA-256,
- TP3C1 / TP3Q3,
- TPSIG / TPLOG,
- Geräte-, Kopf- und Systemkalibrierungsformate innerhalb des signierten Bereichs.

Der Anhang ist eine praktische transportable Statuskopie, aber ohne späteren internen Flash-/Secure-Anker nicht gegen gezieltes Zurückspielen einer älteren vollständigen Dateikopie geschützt.
