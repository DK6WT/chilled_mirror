# TP-3000 V0.50.1 – finaler GitHub-Quellrelease

## Kennzeichnung

- öffentliche Version: `0.50.1`
- interne Build-ID: `0.50.1_87`
- Datum: `24.07.2026`
- Ziel: Teensy 4.1
- Status: Development

## Zweck dieses Releases

V0.50.1 friert den bisherigen Funktionsstand ein, bevor externe 64-MB-QSPI-Flash- und 8-MB-PSRAM-Bausteine in einem späteren Entwicklungszweig initialisiert und genutzt werden. Der Release bildet damit eine klar abgegrenzte Rückfall- und Vergleichsbasis.

## Inhaltlicher Stand

Der Release umfasst unter anderem:

- vollständige Pt100-/Referenzmesskette, Optikregelung, Peltiersteuerung und Safety
- TFT- und Webbedienung in Deutsch und Englisch
- SD-, USB-, RS232-, Ethernet-, ALMEMO- und WinControl-Pfade
- Geräteidentität, signierte Geräte-/Kopf-/Systemkalibrierung und Firmwarekontext
- Kalibrierscheinarchiv, externe PDF-Bindung und Gültigkeits-/Wirksamkeitsanzeige
- zertifizierte CSV-/TPSIG-/TPLOG-Protokollierung
- TP3C1 V0.4 / Schema 1.2 und zweiteiliger QR-Deep-Link auf Web, Ausdruck und TFT
- LED-Autoadaption mit Herstellerkurve, optionaler Systemkurve und kopfbezogenem Lernmodell
- RAM1/RAM2-Bereinigung für NativeEthernet/FNET sowie kontrollierter DHCP-/Static-IP-Wechsel

Die Einzeländerungen der internen Builds sind vollständig in `CHANGELOG.md` und `docs/history/v0.50.1/` dokumentiert.

## Release-Bereinigung gegenüber dem hochgeladenen Arbeitsarchiv

- Paket- und interne Buildnummer auf `_87` vereinheitlicht
- veraltete Web-Lizenzanzeige von Build `_47` auf `_87` korrigiert
- README, Buildanleitung, Releaseunterlagen und Lizenzprüfung auf den finalen Stand gebracht
- 135 historische Entwicklungs-/Validierungsdateien aus der Hauptebene nach `docs/history/` verschoben
- Bibliotheksübersicht vervollständigt
- fehlender pako-/zlib-Lizenznachweis ergänzt
- zentrale BSD-2-/BSD-3-Lizenzkopien und QR-Komponentenhinweise ergänzt
- nicht eindeutig weiterverteilbare GSL1680-Panel-Firmware aus dem öffentlichen Paket entfernt
- automatisches Release-Audit und SHA-256-Manifest ergänzt
- Droid-Sans-Mono-Generator an die vorhandene `PROGMEM`-Ablage angepasst; beide eingebetteten Fontdateien regenerieren wieder bytegleich

Die Mess-, Regel-, Kalibrier-, Signatur- und Logalgorithmen wurden im Rahmen dieser Paketbereinigung nicht verändert.

## Bewusste Ausschlüsse

Nicht enthalten sind:

- panelspezifische GSL1680-Herstellerfirmware
- daraus erzeugte HEX-/BIN-Abbilder
- private Hersteller-, Labor- oder Geräteschlüssel
- produktive Gerätezertifikate und reale Kundendaten
- externe Flash-/PSRAM-Ansteuerung

## Freigabestatus

Die statische Paket- und Lizenzprüfung ist dokumentiert. Ein vollständiger Teensyduino-Linklauf sowie die reale Hardware-/End-to-End-Abnahme waren in der Bereinigungsumgebung nicht möglich und bleiben vor einer praktischen Freigabe erforderlich.
