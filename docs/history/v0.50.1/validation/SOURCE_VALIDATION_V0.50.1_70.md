# Source Validation – V0.50.1_70

- Basis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_69.zip` frisch entpackt.
- Ziel: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_70.zip`.
- interne Version: `0.50.1`.
- Build-ID: `0.50.1_70`.

## Geprüft

- `ads1263Adc2MeasureDark()` wird weiterhin beim Start und zusätzlich genau einmal beim Eintritt in jede LED-Auto-Cal aufgerufen.
- Reihenfolge des Auto-Cal-Eintritts: LED-Dunkelmessung, neue Zeitbasis, Rücksetzen der Auto-Cal-Diagnose, Wiederfreigabe der LED über `setTargetCurrent()`.
- Die 50-ms-Einschwingzeit, 64 Stichproben und 2,5-ms-Abstände der vorhandenen Dunkelmessung bleiben unverändert.
- Der ADC2-Lichtpuffer wird von der bestehenden Dunkelmessfunktion nach jeder Messung zurückgesetzt.
- Bei fehlgeschlagener Neumessung wird der vorhandene Dunkelwert nicht überschrieben und eine serielle Warnung ausgegeben.
- Build-ID-Konsistenz in Firmwarekopf, README, BUILDING, Changelog, Release-Handoff und Source-Package.
- Quellvergleich gegen V0.50.1_69: Messalgorithmusänderung nur in `Taupunkt_Regelung.ino`; in `ads1263.ino` wurden ausschließlich die bisher startbezogenen Kommentare an die wiederholte Nutzung angepasst.
- Klammer-/Präprozessor-Balance der geänderten C/C++-/INO-Dateien.
- ZIP-CRC und bytegleicher Vergleich nach frischem Entpacken.

## Noch offen

Ein vollständiger Teensyduino-Build, reale Memory Usage und der Hardwaretest stehen noch aus, da in der Arbeitsumgebung keine Teensyduino-/Arduino-Buildkette installiert ist. Am Gerät sind insbesondere mehrere aufeinanderfolgende Auto-Cal-Durchläufe und die jeweilige serielle Dunkelwertmeldung zu prüfen.
