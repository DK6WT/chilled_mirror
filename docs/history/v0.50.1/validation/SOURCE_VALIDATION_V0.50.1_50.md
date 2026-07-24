# Source Validation – V0.50.1_50

Geprueft:

- Build-ID `0.50.1_50` in `TPsignedData.h`.
- Watchdogstart am Beginn von `setup()` und kein zweiter `begin()` am Ende.
- Boot-Service waehrend Splash, Firmwarehash und Zertifikatsfinalisierung.
- Hashschleife bedient den Watchdog pro 4096-Byte-Block.
- Normaler Loop-Feed bleibt am Ende der Hauptloop.
- Serielle Ausgabe der Hashdauer vorhanden.
- Bestehende Python-Quelltests fuer Anwendbarkeit, dynamische Webseiten und kopfgebundene Zertifikatsauswahl bestanden.
- Vollstaendiger Teensyduino- und Hardwaretest steht aus.
