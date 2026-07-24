# Kalibrierverwaltung – integriertes Bedienlayout V0.50.1_53

## Ziel

Die Bedienhandlungen sollen nicht mehr nach Übertragungsart über die Seite verteilt sein. Jede Nachweisart zeigt Status und zugehörige Aktionen gemeinsam.

## Neues Layout

Die Bereiche **Gerätejustierung**, **Kopfjustierung**, **Systemkalibrierung** und **Firmwarezertifikat** enthalten jeweils:

1. Status und Metadaten,
2. Download der passenden Anfrage,
3. Import einer signierten Datei aus dem jeweiligen SD-Verzeichnis,
4. direkten Web-Upload der vom KeyGen erzeugten signierten Datei.

Geräte- und Kopfjustierung bleiben zweispaltig. Systemkalibrierung und Firmwarezertifikat nutzen die volle Breite. Die Aktionsbereiche der beiden oberen Karten sind vertikal ausgerichtet.

## Systemkalibrierung von SD

Neu ist `/calibration-system-import`. Die Firmware durchsucht `/CALIBRATION` nach `.tpscal`, ignoriert `ACTIVE_SYSTEM.tpscal` und akzeptiert ausschließlich Pakete, deren Root-/Labor-Signatur, Gerätebindung, Kopfbindung, Justierungsbindung und gegebenenfalls PDF-Bindung vollständig zur aktuellen Konfiguration passen.

Bei mehreren gültigen Kandidaten wird zuerst der spätere Beginn des Kalibrierintervalls und danach das spätere Kalibrierdatum bevorzugt. Die Aktivierung verwendet anschließend denselben transaktionalen Importpfad wie der Web-Upload.

## Unverändert

Die Dateiformate `.tpdcalreq`, `.tphcalreq`, `.tpscalreq`, `.tpdcal`, `.tphcal`, `.tpscal`, `.tpfwreq` und `.tpfwcert` bleiben unverändert. Private Schlüssel verbleiben im KeyGen.
