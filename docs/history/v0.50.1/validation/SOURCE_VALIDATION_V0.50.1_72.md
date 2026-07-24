# Source Validation – V0.50.1_72

- Basis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_71.zip` frisch entpackt.
- Ziel: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_72.zip`.
- interne Version: `0.50.1`.
- Build-ID: `0.50.1_72`.

## Geprüft

- Vertrauenszählung und effektive Mittelwerthistorie sind getrennt.
- Die Vertrauenszählung steigt weiterhin gesättigt bis 1000 und behält die vorhandenen Lerngewichte 0/25/50/75/90 %.
- Bis 128 Werten entspricht die Aktualisierung dem normalen laufenden Mittelwert.
- Ab 128 Werten bleibt der Einfluss jedes neuen akzeptierten Lernwerts konstant bei 1/128.
- Der Einfluss des vorherigen Klassenmodells halbiert sich rechnerisch nach rund 89 neuen Werten derselben Temperaturklasse.
- `m2Shape` wird ab 128 Werten mit derselben effektiven Historienlänge begrenzt und kann nicht mehr unbegrenzt wachsen.
- Bestehendes SD-Binärformat, CRC32, A/B-Slots, Kopfbindung, Ausreißerbestätigung und Reset nur für den ausgewählten Kopf bleiben unverändert.
- Bestehende V0.50.1_71-Modelle bleiben formatkompatibel.
- Hostseitiger Rechentest für laufendes Mittel, 128er-Übergang, konstante EWMA-Gewichtung, Halbwertszeit und getrennte Vertrauenszählung bestanden.
- Der geänderte Aktualisierungsalgorithmus wurde zusätzlich als eigenständiger C++17-Test mit `-Wall -Wextra -Werror` kompiliert und ausgeführt.
- Geänderte C++-Datei auf Klammer-, Kommentar- und Stringbalance geprüft.
- ZIP-CRC, vollständige Dateiliste und bytegleicher Vergleich nach frischem Entpacken geprüft.

## Noch offen

Ein vollständiger Teensyduino-Build, reale Memory Usage und der Hardwaretest stehen noch aus. Am Gerät sollen mehrere Auto-Cals in derselben Temperaturklasse sowie ein gezielter Temperaturwechsel geprüft werden.
