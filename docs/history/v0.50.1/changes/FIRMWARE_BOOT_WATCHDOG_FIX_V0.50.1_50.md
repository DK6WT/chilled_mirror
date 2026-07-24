# Firmware-Boot-Watchdog-Fix – V0.50.1_50

## Fehlerbild

Nach dem Splashscreen blieb V0.50.1_49 stehen und wurde nach etwa acht Sekunden
vom Hardware-Watchdog neu gestartet.

## Ursache

Der WDT1 mit 8-s-Timeout wurde bislang erst ganz am Ende von `setup()` bewusst
initialisiert. Ein bereits laufender Watchdog konnte den Reset jedoch ueberleben.
Der neue vollstaendige Firmwarehash plus SD-/Zertifikatsinitialisierung verlaengerte
den Bootpfad so weit, dass vor dem ersten regulaeren Loop-Feed die Frist ablief.

## Korrektur

- Watchdogstart unmittelbar am Beginn von `setup()`.
- Kontrollierte Boot-Feeds nach Hardware-, EEPROM-, TFT-, Touch-, Netzwerk-, SD-
  und Zertifikatsinitialisierung.
- Feed nach jedem 4096-Byte-Block der Firmware-SHA-256-Pruefung.
- Feed innerhalb des 3,2-s-Splash-Ladebalkens alle 100 ms.
- Serielle Messung der Hashdauer.
- Nach `setup()` weiterhin Feed ausschliesslich am Ende einer kompletten Hauptloop.

## Unveraendert

Firmwarezertifikat und dessen kanonische Signaturbytes, KeyGen, Kalibrierformate,
TP3C1/TP3Q3, TPSIG, TPLOG, Messung, Regelung und Safetylogik wurden nicht geaendert.
