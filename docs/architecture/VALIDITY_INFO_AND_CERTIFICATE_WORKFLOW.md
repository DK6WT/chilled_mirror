# TP-3000 V0.50.1_18 – Zertifizierung, Kalibrierung und Gültigkeitsübersicht

## Begriffe

- `.tpcert`: root-signiertes Gerätezertifikat. Das aktuelle Format besitzt ein Ausstellungsdatum, aber kein Ablaufdatum.
- `.tpdcalreq`: vom TP-3000 selbstsignierte Anfrage für die Geräte-Werksjustierung.
- `.tpdcal`: extern signierte Geräte-Kalibrierfreigabe.
- `.tphcalreq`: vom TP-3000 selbstsignierte Anfrage für die aktuelle Sensorkopf-Kalibrierung.
- `.tphcal`: extern signiertes Kopfzertifikat und Kopfkalibrierung. Es bindet Kopftyp, Kopf-SN, Kalibrierwerte, Gerät und Gültigkeitszeitraum kryptografisch zusammen.

## Produktions-/Kalibrierablauf

1. Gültige Geräte-SN ungleich `00000` setzen.
2. Im Web `Zertifizierung` öffnen, den Geräteschlüssel erzeugen, `.tpreq` exportieren und im KeyTool ein `.tpcert` ausstellen.
3. `.tpcert` im TP-3000 importieren. Die Zertifizierungsseite zeigt den neuen Status sofort an.
4. Kopf-SN im KeyTool registrieren. Für neue Köpfe wird ab `30001` vorgeschlagen.
5. Kopftyp und Kopf-SN am TFT einstellen und die reale Kalibrierung durchführen.
6. Im Web `Zertifizierung → Kalibrierung verwalten` oder am TFT `Kalibrierung → Signierte Kalibrierung` öffnen.
7. Geräte- und/oder Kopf-Anfrage erzeugen.
8. Die Dateien aus `/CALIBRATION/REQUESTS` im KeyTool öffnen, prüfen und signieren.
9. `.tpdcal` und `.tphcal` nach `/CALIBRATION` kopieren.
10. Signierte Gerätejustierung beziehungsweise Kopfkalibrierung importieren. Die Verwaltungsseite zeigt Status, Kalibriertag und Gültigkeit unmittelbar an.
11. Den Gesamtstatus über den Web-Button `Info`, `Setup → Info / Gültigkeit` oder die TFT-Setup-Seite `Info / Gültigkeit` kontrollieren.

## Anzeigen und Navigation

- **Web-Hauptseite:** rechter Button `Info` öffnet `/validity`.
- **Web-Setup:** `Info / Gültigkeit` öffnet dieselbe Seite. Nach Browser-Zurück wird die Setup-Sitzung neu aufgebaut, damit `EXIT` wieder sicher funktioniert.
- **Zertifizierung:** zeigt Arbeitsfortschritt und die Geräteidentitätsaktionen.
- **Kalibrierung verwalten:** zeigt Geräte- und Kopfkalibrierstatus direkt neben Export und Import.
- **TFT-Setup:** reine Anzeige, UP/DOWN scrollt jeweils zwei Zeilen; keine Export-, Import- oder Bearbeitungsfunktion.

## Gültigkeitsübersicht

- **Gerät:** Zertifikatstatus, Geräte-SN und Ausstellungsdatum; kein Ablaufdatum.
- **Geräte-Kalibrierung:** Signatur-/Wertebindung, Kalibriertag, gültig ab und gültig bis.
- **Kopfzertifikat / Kopf-Kalibrierung:** Bindung an aktuellen Kopftyp und aktuelle Kopf-SN, Kalibriertag, gültig ab und gültig bis.
- **Firmware:** Versions-/Buildanzeige, Freigabestatus und Freigabe-/Buildtag; kein `Gültig bis`.

Farben:

- Grün: aktuell gültig.
- Gelb: fehlt, vorbereitet, noch nicht gültig oder zeitlich nicht beurteilbar.
- Rot: ungültig, nicht zu den aktuellen Werten passend oder abgelaufen.

Eine abgelaufene Kalibrierung bleibt kryptografisch prüfbar. Der Ablauf blockiert die normale Messung nicht.

## Sicherheitsregel für `00000`

Die manuelle Kopf-SN-Eingabe bleibt am TFT erhalten. `00000` kennzeichnet weiterhin einen nicht provisionierten beziehungsweise lokalen Servicezustand. Eine Kopf-Kalibrieranfrage und damit ein extern zertifiziertes Kopfzertifikat dürfen für `00000` nicht erzeugt werden.
