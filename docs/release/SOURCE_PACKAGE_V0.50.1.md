# TP-3000 V0.50.1 – Quellpaket und Übergabe

## Paketidentität

- Repository-/Releaseversion: `V0.50.1`
- Firmware-Build-ID: `0.50.1_87`
- Build-Datum: `24.07.2026`
- öffentliche Archivbezeichnung: `TP-3000_V0.50.1_GitHub_Source.zip`

## Enthalten

- vollständiger TP-3000-Firmwarequellcode
- projektlokale Bibliotheken mit ihren Original-Lizenzdateien
- Web-, QR-, Font- und Bildressourcen einschließlich Herkunftsnachweisen
- SD-Kartenvorlage
- aktuelle Release-, Build-, Validierungs- und Lizenzaudit-Dokumente
- vollständige V0.50.1-Entwicklungshistorie
- Test-Root-Public-Key für den dokumentierten Entwicklungsworkflow
- Release-Audit-Skript und SHA-256-Dateimanifest

## Nicht enthalten

- private Schlüssel jeder Art
- vorgebaute `.hex`, `.bin`, `.elf` oder `.map`-Dateien
- Mess- und Logdaten
- verschachtelte Archive
- panelspezifische Datei `external/GSL1680/gslX680_311_5_F.h`

## Warum die GSL1680-Datei fehlt

Die Firmwaretabelle des Touchcontrollers stammt aus dem Panelherstellerpaket. Ihre Weitergaberechte sind nicht eindeutig dokumentiert. Der öffentliche Quellrelease enthält deshalb nur GPL-lizenzierten Treibercode, Wrapper und Installationsanleitung. Die Originaldatei muss der Nutzer direkt für sein konkretes Panel beschaffen.

Ein mit dieser Datei erzeugtes Firmwareabbild enthält die Herstellerdaten ebenfalls und wird daher nicht als öffentliches Releaseartefakt beigefügt.

## Reproduzierbarkeit

Nach dem Entpacken kann die Paketstruktur geprüft werden mit:

```text
python tools/release_audit.py .
```

`MANIFEST.sha256` enthält SHA-256-Prüfsummen aller verteilten Dateien außer dem Manifest selbst.

## Build- und Hardwarestatus

In dieser Bereinigungsumgebung war keine vollständige Arduino-/Teensyduino-Installation vorhanden. Der Quellbaum wurde statisch, strukturell und lizenzseitig geprüft. Ein realer Linklauf mit Memory-Usage und die Hardwareabnahme bleiben verbindlich nachzuholen.
