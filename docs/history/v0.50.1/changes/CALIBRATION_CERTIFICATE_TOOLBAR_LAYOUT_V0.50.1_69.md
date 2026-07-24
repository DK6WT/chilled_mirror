# Kalibrierschein-Werkzeugleiste – V0.50.1_69

Basis ist V0.50.1_68. Interne Version bleibt `0.50.1`, Build-ID ist `0.50.1_69`.

## Änderung

Die Sprachumschaltung `Deutsch | English` liegt in einer eigenen oberen Grid-Spalte der Werkzeugleiste. Dadurch bleibt sie beim Wechsel auf die längeren deutschen Schaltflächentexte oben rechts und springt nicht mehr in eine zweite Zeile. Die Aktionsschaltflächen dürfen innerhalb ihres eigenen Bereichs umbrechen, ohne die Sprachumschaltung zu verschieben.

Unter 900 px Breite bleibt die Sprachumschaltung ebenfalls oben; die Aktionsschaltflächen folgen darunter. Die Werkzeugleiste darf auf großen Bildschirmen bis 1400 px breit werden.

Die reine Erfolgsmeldung `Systemkalibrierschein bereit` beziehungsweise `System calibration certificate ready` wird nach dem vollständigen Aufbau nicht mehr angezeigt, weil der fertige Schein bereits sichtbar ist. Lade-, QR- und Fehlermeldungen bleiben erhalten.

Kalibrierscheininhalt, Drucklayout, Signaturen, QR-Daten und Messfunktionen bleiben unverändert.
