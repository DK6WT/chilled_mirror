# Source Validation V0.50.1_56

## Prüfumfang

Geprüft wurde der aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_31.zip` abgeleitete Quellstand mit interner Version `0.50.1` und Build-ID `0.50.1_56`.

## Bestandene Prüfungen

- Host-Komponententest des gerätesignierten System-Firmwarekontexts mit normaler Host-Konfiguration und mit `__IMXRT1062__`:
  - exakt gleicher Firmwarestand wird als `EXACT_MATCH` erkannt,
  - veränderter SHA-256 wird als `FIRMWARE_CHANGED` erkannt,
  - Manipulation des Kontextdatensatzes wird verworfen.
- JavaScript-Syntaxprüfung der Zertifikatsübersicht, des druckbaren Kalibrierscheins und des TP3C1-/TP3Q3-Encoders mit Node.js.
- Render-Test für einen externen PDF-Kalibrierschein mit später veränderter Firmware:
  - strenger Hinweis „möglicherweise nicht mehr gültig“,
  - externes Original-PDF bleibt unverändert,
  - Firmware-SHA-256 und Betreiberhinweis werden ausgegeben.
- TP3C1-V0.4-Strukturtest:
  - Schema `1.2`,
  - RequiredFeatures `255`,
  - gerätesignierter Firmwarekontext,
  - Anwendbarkeitsstatus `FIRMWARE_CHANGED`,
  - aktueller SHA-256 mit 32 Byte,
  - zwei Transportteile.
- QR-Grenztest mit je 40 Messpunkten für Geräte-, Kopf- und Systemdaten, langen Fachfeldern, externem PDF und Firmwarekontext:
  - komprimierter Envelope 3955 Byte,
  - QR-Version 39 bei beiden Teilen,
  - 173 × 173 Module,
  - Modulgröße bei 115 mm etwa 0,635 mm.
- C-/C++-Lexikprüfung der geänderten Quellmodule auf geschlossene Strings, Kommentare, Klammern und Blöcke.
- Prüfung der `snprintf`-Formatargumente:
  - Web `Info / Gültigkeit`: 53 Platzhalter / 53 Argumente,
  - Web Kalibrierverwaltung: 50 Platzhalter / 50 Argumente.
- Quellprüfung bestätigt:
  - Kundenfirmware und fehlendes Hersteller-Firmwarezertifikat blockieren den Messbetrieb nicht,
  - zertifizierte Ausgaben benötigen einen berechneten Firmware-SHA-256, aber kein Herstellerzertifikat,
  - Firmwareabweichung wird in Web, TFT, QR und Lognachweis gekennzeichnet,
  - die kanonischen `.tpdcal`, `.tphcal` und `.tpscal`-Formate bleiben unverändert.

## Speicherhinweis

Der Statusdatensatz des Firmwarekontexts wurde um Manifest und Gerätesignatur erweitert. Für Web, TFT und Logkontext kommen zusätzliche DMAMEM-Statuspuffer hinzu. Die reale RAM1-/RAM2-Nutzung muss aus dem vollständigen Teensyduino-Build übernommen werden; die Änderungen betreffen vor allem RAM2, nicht die zuvor zurückgewonnene RAM1-Fontreserve.

## Noch nicht ausgeführt

- vollständiger Teensyduino-Build,
- reale Memory-Usage-Auswertung,
- Flashen und Hardwarelauf,
- Drucktest der beiden 115-mm-QR-Codes,
- Ende-zu-Ende-Prüfung mit dem externen TP-3000-Verifier. Der Verifier muss TP3C1 V0.4 und `TP3000-CALIBRATION-QR-4` unterstützen, bevor der neue QR-Nachweis produktiv verwendet wird.

## Paketprüfung

Das Übergabepaket wurde mit `unzip -t` geprüft und nach frischem Entpacken bytegleich mit dem Arbeitsverzeichnis verglichen. Der endgültige Paket-SHA-256 wird außerhalb des Archivs bei der Übergabe angegeben.
