# Source Validation – V0.50.1_71

- Basis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_70.zip` frisch entpackt.
- Ziel: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_71.zip`.
- interne Version: `0.50.1`.
- Build-ID: `0.50.1_71`.

## Geprüft

- Drei klar getrennte Anwendungsmodi: `Aus`, `Ein - Grundkurve` und `Selbstlernend`.
- Bei vorhandener SD-Karte läuft die kopfbezogene Lernwerterfassung unabhängig vom Anwendungsmodus; gelernte Korrekturen wirken ausschließlich in `Selbstlernend`.
- Ohne SD-Karte werden TFT, Web und Backend auf `Aus` begrenzt. Eine bei aktiver Adaption entfernte Karte wird über die 30-s-Medienprüfung erkannt und erzwingt `Aus`.
- Die feste OD-850FHT-Grundkurve wird ausschließlich relativ zum letzten erfolgreichen Auto-Cal-Anker angewendet.
- Das Lernmodell speichert nur die verbleibende Formabweichung gegenüber der Grundkurve aus zwei aufeinanderfolgenden erfolgreichen Auto-Cals; der absolute Strom bleibt an der letzten echten Auto-Cal verankert.
- 1-K-Klassen von -40 bis +85 °C, Lernfreigabe ab drei bestätigten Werten, Gewichtung 25/50/75/90 %, maximal 90 % Lernanteil.
- Interpolation nur mit Stützstellen höchstens 3 K je Seite und maximal 6 K Gesamtlücke. Keine Steigungsextrapolation; Randkorrektur wird über 3 K auf die Grundkurve ausgeblendet.
- Gelernte Formkorrektur auf ±5 %, Gesamtstrom gegenüber der letzten Auto-Cal auf ±3 % und Stromänderung auf 0,02 %/s begrenzt.
- Ausreißer über 1 % werden erst nach drei zueinander passenden Bestätigungen übernommen.
- SD-Modell getrennt nach Kopftyp und Kopf-SN, CRC32-gesicherte A/B-Slots, Rückleseprüfung nach Schreiben und Fallback auf den älteren gültigen Slot bei beschädigtem neuesten Slot.
- `LED-Lerndaten zurücksetzen` löscht ausschließlich die A/B-Dateien des aktuell ausgewählten Kopfs; ein zweiter Kopf bleibt im Hosttest unverändert.
- T-Umgebung wird mit 120-s-Tiefpass verwendet; Lernen wird bei mehr als 0,20 K/min Temperaturtrend oder mehr als 2 K Abstand zwischen Roh- und Filterwert verworfen.
- Hostseitiger C++-Test mit simuliertem SD-Dateisystem: Lernen im Modus `Aus`, Basis-/Selbstlernmodus, Gewichtung, Extrapolationsgrenze, A/B-CRC-Fallback, Ausreißerbestätigung, Kopftrennung, kopfbezogenes Löschen, fehlende und während des Betriebs entfernte SD-Karte.
- `TPledAdaptation.cpp` separat mit `g++ -std=gnu++17 -Wall -Wextra -Werror -fsyntax-only` geprüft.
- EEPROM-/Backup-Strukturgrößen bleiben durch Nutzung bisher reservierter Bytes unverändert.
- JavaScript-Syntax der Web-Setup-Seite mit `node --check` geprüft.
- Klammer-, Kommentar-, String- und Raw-String-Balance aller geänderten C/C++-/INO-Dateien geprüft.
- Menü-IDs 125 und 126 sind eindeutig und kollidieren mit keiner bestehenden Menü-ID.
- ZIP-CRC, vollständige Dateiliste und bytegleicher Vergleich nach frischem Entpacken geprüft.

## Noch offen

Ein vollständiger Teensyduino-Build, reale Memory Usage und der Hardwaretest stehen noch aus, da in der Arbeitsumgebung keine Teensyduino-/Arduino-Buildkette installiert ist. Am Gerät sind insbesondere Auto-Cal-Lernwerte, sanfte LED-Stromnachführung, SD-Entfernung, TFT-/Web-Auswahl und der Reset mit mindestens zwei verschiedenen Kopf-SNs zu prüfen.
