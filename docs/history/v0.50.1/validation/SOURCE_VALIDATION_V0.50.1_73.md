# Source Validation – V0.50.1_73

## Paket und Version

- Ziel: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_73.zip`
- Basis: V0.50.1_72
- interne Firmwareversion: `0.50.1`
- Build-ID: `0.50.1_73`

## Geprüfte Änderung

- `TPledAdaptation.cpp`: Modell-, Lern-, Interpolations-, CRC-, SD-, Modus-, Auto-Cal- und Diagnosefunktionen sind mit `FLASHMEM` markiert.
- `ledAdaptationTask()` bleibt ohne `FLASHMEM` als kleiner ITCM-Einstieg erhalten.
- Der Einstieg prüft nur ein 250-ms-Zeitfenster und ruft danach `ledAdaptationTaskSlow()` auf.
- `ledAdaptationTaskSlow()` ist mit `FLASHMEM` und `noinline` markiert.
- Der bestehende 120-s-Temperaturfilter wird weiterhin alle 250 ms bedient; die Stromnachführung bleibt auf mindestens 1 s Abstand begrenzt.

## Host-Prüfungen

- `TPledAdaptation.cpp` mit `g++ -std=gnu++17 -Wall -Wextra -Werror -fsyntax-only` und Teensy-kompatiblen Arduino-/SD-Stubs geprüft: erfolgreich.
- Optimiertes Host-Objekt mit `-O2 -ffunction-sections -fdata-sections` erzeugt und Abschnittszuordnung mit `readelf` geprüft.
- V0.50.1_72 Host-Vergleich: `ledAdaptationTask()` lag mit 1506 Byte vollständig im normalen Textabschnitt; kein `.flashmem`-Abschnitt aus dieser Datei.
- V0.50.1_73 Host-Vergleich: `.flashmem` aus `TPledAdaptation.cpp` umfasst 7657 Byte; `ledAdaptationTask()` verbleibt mit 47 Byte im normalen Textabschnitt.
- Bestehender Historientest ausgeführt: `PASS residual=0.497558355 count=1000`.
- Modellstruktur und SD-Dateiformat bleiben unverändert; bestehende V71/V72-Lerndateien bleiben kompatibel.

## Nicht lokal prüfbar

Ein vollständiges Teensyduino-/ARM-Linkergebnis steht in dieser Umgebung nicht zur Verfügung. Die endgültige Änderung von `RAM1 code`, `padding`, Flashbelegung und `free for local variables` muss deshalb am realen Teensy-4.1-Build geprüft werden. Die Host-Abschnittsprüfung bestätigt die beabsichtigte Codeplatzierung, ersetzt aber nicht die finale Teensy-Memory-Usage.
