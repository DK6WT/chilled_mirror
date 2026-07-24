# Source Validation – TP-3000 V0.50.1_48

## Prüfgegenstand

- Basis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_23.zip`
- Zielpaket: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_24.zip`
- interne Version: `0.50.1`
- Build-ID: `0.50.1_48`

## Geprüfte Änderungen

- neuer eingebetteter 112-Byte-Firmwaremanifestblock,
- vollständiger SHA-256 über das Teensy-XIP-Abbild,
- normalisierte Hash-/CRC-Felder ohne Kreisbezug,
- Manifest-CRC und Abbildgrößenvergleich,
- transaktionale SD-Datei `/CERTIFICATION/FIRMWARE.TPS`,
- TFT-/Webanzeige unter `Info / Gültigkeit`,
- Sperren für Kalibrieranfragen und Kalibrieraktivierungen,
- Firmwareintegrität als Pflichtnachweis für zertifizierte CSV/TPSIG/TPLOG,
- tatsächlich gemessener Firmware-SHA-256 im vorhandenen TPSIG/TPLOG-Firmwarehashfeld,
- Post-Build-Werkzeug für HEX/BIN.

## Hosttests

Bestanden:

```text
python -m py_compile tools/finalize_firmware_manifest.py
python tools/test_firmware_manifest_finalize.py
python tools/test_calibration_applicability_state.py
python tools/test_dynamic_page_transfer.py
python tools/test_head_bound_certificate_selection.py
node tools/test_tp3c1_v03.mjs
```

Der synthetische Firmwaretest erzeugt ein Teensy-ähnliches HEX-Abbild mit
BootData und Manifest, finalisiert es und prüft unabhängig:

- Abbildgröße,
- Finalisierungsmarker,
- normalisierten SHA-256,
- Manifest-CRC,
- HEX-Roundtrip.

`TPfirmwareIntegrity.cpp` wurde zusätzlich isoliert mit GCC 14.2 und
Teensy-/SD-Stubs mit `-Wall -Wextra -Werror` kompiliert.

Der `snprintf`-Vertrag der erweiterten Web-Gültigkeitsseite wurde geprüft:
47 Formatfelder und 47 Übergabewerte.

## Formatprüfung

Unverändert bestätigt:

```text
TP3000-DEVICE-CALIBRATION-3
TP3000-HEAD-CALIBRATION-3
TP3000-SYSTEM-CALIBRATION-2
TP3000-LOG-SIGNATURE-2
TP3000-LOG-CONTAINER-1
TP3C1 V0.3
TP3Q3
```

Kanonische Kalibrierbytes, Signaturfeldreihenfolgen und ECDSA-P-256-Verfahren
wurden nicht erweitert. Der Firmware-SHA-256 ist in diesem Build kein neues
signiertes Kalibrierfeld.

## RAM-/Flash-Konzept

- 4096-Byte-Hashpuffer: RAM2 (`DMAMEM`),
- zwei 1024-Byte-SD-Transaktionspuffer: RAM2,
- Statusstruktur: RAM2,
- nicht zeitkritische Prüf- und Dateifunktionen: `FLASHMEM`,
- Manifest und feste Texte: Program-Flash.

Die genaue FLASH-, RAM1- und RAM2-Belegung kann nur der vollständige
Teensyduino-Linkerbuild liefern.

## Noch offen

Nicht in dieser Umgebung durchgeführt:

- vollständiger Teensyduino-Build,
- Programmierung einer realen `*_final.hex`,
- Laufzeitmessung der Startprüfung auf Teensy 4.1,
- reales Verhalten bei verändertem Flashabbild,
- SD-Stromausfalltest,
- Prüfung aller Sperren am Gerät,
- finale FLASH-/RAM1-/RAM2-Zahlen.
