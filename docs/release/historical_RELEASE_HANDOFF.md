# TP-3000 V0.50.1_84 – Release Handoff

- Basis: V0.50.1_83
- interne Version: `0.50.1`
- Build-ID: `0.50.1_84`
- Korrektur: Systemkalibrierung wird im TFT-QR-Aufbau mit Approval V4 statt fälschlich V2 erneut eingelesen.
- Erwartetes Ergebnis: Kein Abbruch mehr mit „Kalibrierscheinquelle ist ungültig“ bei gültiger Quelle `TP3000` oder `EXTERNAL_PDF`.

# TP-3000 V0.50.1_83 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_83`
- Arbeitsbasis: V0.50.1_82

## Fehlerkorrektur

Der TFT-QR-Pfad schaltet die erneute Verifikation des Systemkalibrierpakets ausdrücklich in den Signaturmodus. Damit bleiben Format-, Manifest- und Root-/Labor-Signaturprüfung vollständig aktiv, während aktuelle Firmwareabweichung und Laufzeitbindung nicht länger als Export-Sperre wirken.

Die TFT-Infoansicht ist auf maximal 42 Zeichen je sichtbarer Zeile begrenzt. Lange Statusmeldungen und SHA-256-Werte werden auf mehrere physische Zeilen verteilt; deutsche Texte verwenden die vorhandenen Umlaute der Schriftart.

## Hardwaretest

TFT-Kalibrierschein öffnen, beide Codes wechseln und anschließend `Info / Gültigkeit` vollständig durchscrollen. Bei erneutem QR-Fehler steht die konkrete Ursache auf dem TFT und seriell hinter `[TFT-QR]`.

---

# TP-3000 V0.50.1_82 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_82`
- Arbeitsbasis: V0.50.1_81

## Änderung

Die TFT-QR-Erzeugung bewertet die drei aktiven Kalibrierpakete für den Export nun ausschließlich nach ihrer kryptografischen Signaturgültigkeit. Eine spätere Änderung der Kundenfirmware sperrt den Kalibrierschein-QR nicht mehr. Web und TFT führen die Systemkalibrierung weiterhin als signiert und gültig; der abweichende Firmwarebezug wird gelb und mit aktuellem SHA-256 dokumentiert.

## Unverändert

Kalibrierformate, kanonische Bytes, Signaturen, TP3C1 V0.4, TP3Q3 QR-4, Messung, Regelung, Safety, LED-Autoadaption, Ethernet und Libraries bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build sowie Hardwaretest von TFT-Code 1/2 und 2/2 bei absichtlich geänderter Firmware.

---

# TP-3000 V0.50.1_81 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_81`
- direkte Arbeitsbasis: `V0.50.1_80`
- passendes KeyGen: V0.7.14, Formate unverändert

## Fehlerkorrektur

Vier nicht DMA-pflichtige LED-Autoadaptionspuffer wurden mit zusammen 16.008 Byte aus RAM2 nach RAM1 verschoben. Damit steigt die erwartete freie RAM2-Reserve von rund 71,2 KiB auf rund 87,2 KiB. NativeEthernet/FNET erhält wieder ausreichend Spielraum für seinen 64-KiB-Stack sowie DHCP, TCP-Sockets und den HTTP-Listener.

## Unverändert

DHCP-Neustartlogik, statische IP, Ethernet-Aus/Ein, TFT-Kalibrierschein-QR, LED-Autoadaption, Kalibrier- und Signaturformate, Messung, Regelung, Safety, EEPROM-Layout und `libraries/` bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, realer RAM1/RAM2-Linkerbericht und Hardwaretests für DHCP, statische IP, Port 80, Ethernet-Aus/Ein, LED-Autoadaption und TFT-QR. Wegen Build 81 ist für eine optionale Herstellerfreigabe ein neues `.tpfwcert` erforderlich.

---

# TP-3000 V0.50.1_80 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_80`
- direkte Arbeitsbasis: `V0.50.1_79`
- passendes KeyGen: V0.7.14, Formate unverändert

## Fehlerkorrektur

Die verbleibenden TFT-QR-Dauerpuffer wurden aus RAM2 nach RAM1 verschoben. Damit steht der NativeEthernet-/DHCP-Laufzeitallokation wieder der RAM2-Stand von V0.50.1_77 zur Verfügung.

NativeEthernet/FNET kann einen bereits statisch konfigurierten Stack nicht zuverlässig im laufenden Betrieb auf DHCP neu initialisieren. Der DHCP-Moduswechsel wird deshalb nach dem Speichern durch einen kontrollierten Software-Neustart übernommen. Ethernet Aus/Ein im selben Modus verwendet eine vorhandene gültige Netzwerkkonfiguration weiter und vermeidet ein unnötiges zweites `Ethernet.begin()`.

## Unverändert

Kalibrier- und Signaturformate, TP3C1 V0.4, QR-Anzeige, Web-Kalibrierschein, Messung, Regelung, Safety, EEPROM-Layout und `libraries/` bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, RAM1/RAM2-Linkerbericht und Hardwaretests für DHCP, statische IP, Moduswechsel, Ethernet Aus/Ein und beide TFT-QR-Codes. Wegen Build 80 ist für eine optionale Herstellerfreigabe ein neues `.tpfwcert` erforderlich.

---

# TP-3000 V0.50.1_79 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_79`
- direkte Arbeitsbasis: `V0.50.1_78`
- passendes KeyGen: V0.7.14, Formate unverändert

## Fehlerkorrektur

V0.50.1_78 belegte durch dauerhaft duplizierte TFT-QR-Arbeitspuffer nahezu die gesamte freie RAM2-Reserve. NativeEthernet konnte dadurch insbesondere beim DHCP-Start nicht mehr zuverlässig arbeiten; ein Ethernet-Aus/Ein konnte bis zum 8-s-Watchdog-Reset blockieren.

V0.50.1_79 entfernt die permanenten Duplikate und verwendet vorhandene Web- und Kalibrierpuffer sequenziell. Gegenüber V0.50.1_78 werden im quellgleichen Hostvergleich rund 55,9 KiB statische BSS-Belegung zurückgewonnen. Die beiden fertigen TP3C1-Texte und die sichtbare QR-Matrix bleiben eigenständig gespeichert, sodass die Anzeige und der Wechsel zwischen Code 1/2 und Code 2/2 unverändert funktionieren.

## Unverändert

Ethernet-/DHCP-Logik, Static-IP, Kalibrier- und Signaturformate, TP3C1 V0.4, ZLIB, Base38, QR-Matrix, Web-Kalibrierschein, Messung, Regelung, Safety und Libraries bleiben funktional unverändert.

## Noch offen

Vollständiger Teensyduino-Build mit realem RAM2-Bericht sowie Hardwaretests für DHCP, Static-IP, Ethernet Aus/Ein, parallelen Webzugriff und beide TFT-QR-Codes. Wegen Build 79 ist für eine optionale Herstellerfreigabe ein neues `.tpfwcert` erforderlich.

---

# TP-3000 V0.50.1_78 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_78`
- direkte Arbeitsbasis: `V0.50.1_77`
- passendes KeyGen: V0.7.14, Formate unverändert

## Änderung

Der aktive Kalibrierschein kann nun als zweiteiliger TP3C1-V0.4-QR-Nachweis direkt am TFT angezeigt und mit der TP-3000-App gescannt werden. Der neue Hauptmenüpunkt erzeugt den Nachweis ausschließlich aus den aktuell aktiven, erneut verifizierten Geräte-, Kopf- und Systemkalibrierpaketen. Die gespeicherten Dateien und Signaturen werden nicht verändert.

## Bedienung

- `UP` / `DOWN`: zwischen `CODE 1/2` und `CODE 2/2` wechseln
- `ENTER`: zurück zum Hauptmenü
- `EXIT`: Setup verlassen und zum Hauptscreen wechseln

Der QR-Code wird mit vier Modulen Ruhezone quadratisch und ohne Interpolation dargestellt. Die Firmware wählt automatisch die größte passende ganzzahlige Modulgröße 4, 3 oder 2 Pixel.

## Unverändert

Messwerterfassung, ADC-Konfiguration, Regelung, Safety, Kalibrier- und Signaturformate, Web-Kalibrierschein, LED-Autoadaption und der komplette Ordner `libraries/` bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale RAM1/RAM2-Messung und Scanprüfung beider TFT-Codes mit mehreren echten Kalibrierscheinen. Wegen der neuen Build-ID ist für eine optionale Herstellerfreigabe ein neues `.tpfwcert` erforderlich.

---

# TP-3000 V0.50.1_77 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_77`
- direkte Arbeitsbasis: `V0.50.1_76`
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Die SD-Überwachung der LED-Autoadaption ist jetzt an den tatsächlichen Betriebszustand gekoppelt. Bei aktivem Messwertlogging wird ausschließlich der vorhandene Loggerstatus abgefragt. Bei gestopptem Logging prüft die LED-Autoadaption die Karte bei jeder Auto-Cal und ansonsten in einem festen Fünf-Minuten-Raster. Dadurch entstehen keine parallelen zyklischen Kartenabfragen während des Loggings, und frei einstellbare Auto-Cal-Intervalle von 10 oder 15 Minuten bleiben vollständig abgedeckt.

## Fehlerverhalten

Meldet der Logger `Keine Karte` oder `Dateifehler`, beziehungsweise scheitert eine Prüfung ohne Logging, bleibt der letzte gültige Auto-Cal-Anker erhalten. Das Kopfmodell und eine externe Systemgrundkurve werden deaktiviert; `Selbstlernend` fällt über die bestehende Stromrampe auf `Ein - Grundkurve` mit interner Herstellerkurve zurück. Ein Wiedereinsetzen aktiviert `Selbstlernend` nicht automatisch.

## Unverändert

Messwerterfassung, ADC-Konfiguration, Auto-Cal-Ablauf, 161-Klassen-Lernmodell, System-Grundkurvenformat, A/B-Persistenz, Stromgrenzen, Regelung, Safety, Kalibrier- und Zertifikatsformate sowie der komplette Ordner `libraries/` bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build und Hardwaretest mit aktivem sowie gestopptem Logging, SD-Entnahme, Loggerfehler, fünfminütiger Wiederholungsprüfung und Auto-Cal-Intervallen 10/15 min. Wegen der neuen Build-ID ist für eine optionale Herstellerfreigabe ein neues `.tpfwcert` erforderlich.

---

# TP-3000 V0.50.1_76 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_76`
- direkte Arbeitsbasis: `V0.50.1_75`
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Im eigenen TP-3000-Projektcode wurden die sinnvollen Warnungen aus dem Buildprotokoll korrigiert. Überlappende Textkopien sind jetzt sicher, SD-Pfade werden vollständig auf Puffergröße geprüft, lange TFT- und Webtexte sind explizit begrenzt und die vorhandenen CRC-geschützten Schnittstellenkonfigurationen V5 bis V8 werden beim Fehlen gültiger aktueller A/B-Slots wieder migriert.

## Unverändert

Der komplette Ordner `libraries/` ist gegenüber V0.50.1_75 bytegleich. Messwerterfassung, ADC-Konfiguration, Regelung, Safety, Kalibrierwerte, Zertifikatsformate, LED-Autoadaption und das feste 161-Punkte-Raster bleiben unverändert. Die BMP581-Fremdbibliothekswarnung wurde bewusst nicht verändert.

## Paketprüfung

ZIP-Integrität, Textquellen ohne eingebettete NUL-Bytes, Versionsangaben, Dateidifferenz zur Basis und bytegleicher Bibliotheksvergleich wurden statisch geprüft.

## Noch offen

Vollständiger Teensyduino-Build und Hardwaretest. Nach dem Build ist zu prüfen, dass nur noch bewusst akzeptierte Fremdbibliotheks- beziehungsweise Toolchainmeldungen verbleiben. Wegen der neuen Build-ID ist für eine optionale Herstellerfreigabe ein neues `.tpfwcert` erforderlich.

---

# TP-3000 V0.50.1_75 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_75`
- direkte Arbeitsbasis: V0.50.1_74
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Die externe System-Grundkurve und das selbstlernende Kopfmodell verwenden nun ein gemeinsames festes 1-K-Raster von -50 bis +110 °C mit 161 Positionen. Die CSV-Datei hat Formatversion 2 und muss alle 161 Temperaturwerte lückenlos enthalten. Ein separat angegebener Gültigkeitsbereich kennzeichnet den tatsächlich gemessenen Abschnitt; außerhalb wird die interne OD-850FHT-Herstellerkurve stetig an den Systemrand angekoppelt.

Das Lernmodell wurde von 126 auf 161 Klassen erweitert und erhält Strukturversion 2. Bestehende A/B-Modelle der Version 1 werden wegen der geänderten Geometrie nicht übernommen. Ein Modell belegt im Hostlayout 3264 Byte und bleibt damit unter der 4096-Byte-Grenze.

## Unverändert

Dateipfad, Kurvenbindung über Fingerprint, A/B-CRC-Speicherung, Vertrauensstufen, 128-Werte-Historie, Ausreißerschutz, Interpolation des Lernmodells, Stromgrenzen, Stromrampe, Verhalten ohne SD, ADC2-Dunkelmessung, Auto-Cal, Messkette, Regelung, Safety und Zertifikatsformate bleiben unverändert.

## Paketprüfung

Das Quellpaket enthält 279 Dateien. ZIP-CRC, vollständige Dateiliste und bytegleicher Vergleich nach frischem Entpacken waren erfolgreich.

## Noch offen

Vollständiger Teensyduino-Build, reale Linker-Speicherwerte, Hardwaretest mit alter und neuer Lerndatei, SD-Wechsel sowie Klimakammerfahrt zur Ermittlung der Seriengrundkurve. Wegen Build 75 ist für eine optionale Herstellerfreigabe ein neues `.tpfwcert` erforderlich.

---

# TP-3000 V0.50.1_74 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_74`
- direkte Arbeitsbasis: V0.50.1_73
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Die LED-Autoadaption unterstützt nun eine bevorzugte, gemessene System-Grundkurve auf der SD-Karte. Der festgelegte Pfad lautet `/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`. Die Datei beschreibt relative LED-Stromfaktoren der vollständigen Optikkette und wird auf Kennung, Formatversion, Kurven-ID, Kopftyp, Referenztemperatur, Punktzahl, Reihenfolge, maximale Lücke und Wertebereiche geprüft.

Ist die Datei gültig, wird sie in den Modi `Ein - Grundkurve` und `Selbstlernend` verwendet. Fehlt sie oder ist sie ungültig, bleibt die interne OD-850FHT-Herstellerkurve aktiv. Außerhalb des gemessenen Systembereichs wird die System-Randkorrektur gehalten und nur die Herstellersteigung fortgesetzt.

Ohne SD sind `Aus` und `Ein - Grundkurve` verfügbar; nur `Selbstlernend` ist gesperrt. Eine SD-Entnahme im selbstlernenden Betrieb führt automatisch auf `Ein - Grundkurve` und die interne Herstellerkurve zurück. TFT, Web und Backend verwenden dieselbe Regel.

Das Lernmodell ist über eine 24-Bit-Kurvenkennung in den vorhandenen reservierten Modellbytes an die Grundkurve gebunden. Modelle der internen Herstellerkurve aus V0.50.1_71 bis _73 bleiben kompatibel. Bei einer anderen Systemkurve werden alte Lerndaten nicht angewendet.

## Dateivorlage

- Dokumentation: `LED_SYSTEM_GRUNDKURVE_V0.50.1_74.md`
- SD-Vorlage: `SD_CARD_TEMPLATE/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`

## Unverändert

FLASHMEM-Aufteilung aus Build 73, 120-s-Temperaturfilter, 1-s-Nachführung, ±3-%-Gesamtstromgrenze, ±5-%-Lernkorrektur, 128-Werte-Historie, ADC2-Dunkelmessung, Auto-Cal, Pt100-Messkette, Peltierregelung, Safety und Zertifikatsformate bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, Hardwaretest ohne SD sowie mit gültiger/ungültiger Systemdatei, SD-Entnahme im selbstlernenden Betrieb und reale Ermittlung der System-Grundkurve in der Klimakammer. Für eine optionale Herstellerfreigabe ist wegen Build 74 ein neues `.tpfwcert` erforderlich.

---

# TP-3000 V0.50.1_73 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_73`
- direkte Arbeitsbasis: V0.50.1_72
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Die umfangreiche LED-Autoadaptionsimplementierung wurde aus dem schnellen ITCM/RAM1-Codebereich in `FLASHMEM` verlagert. Der aus jeder Hauptschleife aufgerufene Einstieg `ledAdaptationTask()` bleibt bewusst klein im ITCM und delegiert höchstens alle 250 ms an `ledAdaptationTaskSlow()` im Program-Flash. Die langsame Funktion ist `noinline`, damit die Trennung auch bei Optimierung erhalten bleibt.

Initialisierung, Temperaturfilter, Grundkurve, Interpolation, Lernmodell, SD-A/B-Zugriff, CRC, Auto-Cal-Lernen, Rücksetzen und Diagnoseabfragen liegen ebenfalls in `FLASHMEM`. Die Temperaturfilterung bleibt mit 250-ms-Takt erhalten; die wirksame LED-Stromnachführung bleibt bei einer Sekunde.

## Unverändert

LED-Autoadaptionsmodi, Lernmodell und Dateiformat, 128-Werte-Historie, ADC2-Dunkelmessung, Optikziel, LED-Auto-Cal, Pt100-Messung, Peltierregelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte und Logformate bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build mit realer Memory Usage. Erwartet wird eine deutliche Verringerung von `RAM1 code` und ein entsprechender Wiedergewinn von ITCM-Padding; die genaue Zahl kann nur der Teensy-Linker liefern. Hardwareprüfung aller LED-Autoadaptionspfade bleibt erforderlich. Wegen Build 73 wird für eine optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_72 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_72`
- direkte Arbeitsbasis: V0.50.1_71
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Die Vertrauenszählung der LED-Lernklassen und die Nachführung ihres Mittelwerts sind getrennt. Die Klasse behält ihre bestätigte Belegung bis 1000 Werte, während der Formfaktor ab 128 Werten mit konstant 1/128 pro neuem gültigen Auto-Cal nachgeführt wird. Dadurch werden ältere Daten allmählich verdrängt, ohne seltene Temperaturklassen allein aufgrund ihres Kalenderalters zu löschen.

Die LED-Temperaturvorsteuerung ist in den Regelparametern als `Aus`, `Ein - Grundkurve` und `Selbstlernend` verfügbar. Die Datenblatt-Grundkurve der OD-850FHT bildet den Start- und Fallbackverlauf. Erfolgreiche, thermisch geeignete Auto-Cals liefern bei vorhandener SD-Karte unabhängig vom Modus Lerninformationen; erst `Selbstlernend` verwendet diese zusätzlich.

Das Modell ist für jeden Kopftyp und jede Kopf-SN getrennt. Es speichert CRC-gesicherte A/B-Slots unter `/LEDADAPT`, verwendet den letzten echten Auto-Cal als absoluten Stromanker und lernt aus den Verhältnissen aufeinanderfolgender Auto-Cals nur die verbleibende Formabweichung von der Grundkurve. Interpolation, Extrapolation, Lerngewicht, Ausreißer, Gesamtstrom und Änderungsrate sind konservativ begrenzt.

Ohne SD-Karte wird der Modus auf `Aus` erzwungen; die beiden aktiven Modi sind in TFT, Web und Backend gesperrt. `LED-Lerndaten zurücksetzen` löscht ausschließlich das Modell des aktuell gewählten Kopfes. Die spätere Verlagerung des Modells in den internen QSPI-Flash ist noch nicht Bestandteil dieses Builds.

## Unverändert

Die ADC2-Dunkelmessung vor jeder Auto-Cal aus Build 70, das ADC2-Bruttoziel, der Optik-Sollwert 98,5 %, die eigentliche LED-Auto-Cal, Pt100-Messkette, ADC1-SFOCAL, Peltierregelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte und Logformate bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und Hardwareprüfung über mehrere Temperaturzyklen. Besonders zu prüfen sind SD-Kartenentnahme, Kopfwechsel, A/B-Modellwiederherstellung, Ausreißerbestätigung und die reale Korrelation zwischen externer T-Umgebung und Optiktemperatur. Wegen Build 72 wird für eine optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_70 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_70`
- direkte Arbeitsbasis: V0.50.1_69
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Vor jedem LED-Auto-Cal-Durchlauf wird der ADC2-Dunkelwert neu gemessen. Die LED wird über `LED_OFF` abgeschaltet, nach 50 ms werden 64 ADC2-Stichproben gemittelt und der Lichtpuffer wird zurückgesetzt. Erst danach beginnt die Auto-Cal mit neu gestarteter Zeitbasis und wieder freigegebener LED.

Die Optikdiagnose erhält damit bei jedem Durchlauf einen tatsächlich neu bestimmten Dunkelwert und kann die Dunkelwertdrift zwischen zwei Auto-Cals bewerten. Bei einer fehlgeschlagenen Neumessung bleibt der zuletzt gültige Dunkelwert erhalten und der Durchlauf wird mit Warnmeldung fortgesetzt.

## Unverändert

ADC2-Bruttoziel, Optik-Sollwert 98,5 %, LED-Schrittregelung, Pt100-Messkette, ADC1-SFOCAL, Peltierregelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte, Firmwarekontext, Logging und Kommunikationsprotokolle bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und Hardwareprüfung der LED-Abschaltung sowie der seriellen Dunkelwertmeldung bei mehreren aufeinanderfolgenden Auto-Cal-Durchläufen. Wegen Build 70 wird für eine optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_69 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_69`
- direkte Arbeitsbasis: V0.50.1_68
- verbindliche Projektbasis: V0.50.1_65
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Die Sprachumschaltung des Web-Kalibrierscheins liegt in einem eigenen oberen Layoutbereich. Beim Wechsel zwischen Deutsch und Englisch bleibt sie deshalb an derselben oberen Position. Auf schmalen Ansichten steht sie weiterhin oberhalb der Aktionsschaltflächen.

Die reine Fertigmeldung unter der Werkzeugleiste wird nach erfolgreichem Aufbau geleert. Lade-, QR- und Fehlermeldungen bleiben erhalten.

## Unverändert

Kalibrierscheininhalt, Drucklayout, Signaturen, Hashes, TP3C1 V0.4, TP3Q3 QR-4, Firmwarekontext, Messung, Filterung, Regelung, Safety, Logging und Kommunikationsprotokolle bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und Prüfung im Zielbrowser auf dem Gerät. Wegen Build 69 wird für eine optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_68 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_68`
- Arbeitsbasis: V0.50.1_67 / verbindliche Projektbasis V0.50.1_65
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Leere optionale Inhalte werden im Web- und Druck-Kalibrierschein einheitlich als Gedankenstrich `—` dargestellt. Dies betrifft feste Tabellenfelder, die sechs kalibriertechnischen Fachfelder sowie optionale Firmwarezertifikatsangaben. Die signierten Quelldaten bleiben unverändert; nur die HTML-Darstellung ersetzt leere beziehungsweise ausschließlich aus Leerzeichen bestehende Werte.

Der redundante Erklärungstext unter den Systemkalibrierungs-Steuerelementen der Firmware-Webseite wurde entfernt.

## Unverändert

Kalibrierformate, kanonische Bytes, Signaturen, Hashes, TP3C1 V0.4, TP3Q3 QR-4, Firmwarekontext, Messung, Filterung, Regelung, Safety, Logging und Kommunikationsprotokolle bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und Prüfung im Zielbrowser beziehungsweise auf dem realen Druckertreiber. Wegen Build 68 wird für eine optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_67 - Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_67`
- Arbeitsbasis: V0.50.1_66 / verbindliche Projektbasis V0.50.1_65
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Druckfeinschliff des deutsch/englischen Kalibrierscheins: Kalibrierschein-Nr. auf Seite 1 einzeilig, deutsche Seite 1 kompakter, TP3C1-Codes 112 mm und Prüfverfahren-Zusatz fest auf derselben QR-Seite.

## Unverändert

Kalibrierformate, QR-Nutzdaten, Signaturen, Firmwarekontext, Messung, Filterung, Regelung, Safety, Logging und Kommunikationsprotokolle bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage, Hardwaretest und Drucktest mit dem tatsächlichen Browser/Druckertreiber. Wegen Build 67 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_65 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_65`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_41.zip`
- Arbeitsbasis: V0.50.1_40 / Build 64
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Die sichtbare Auswahl des Hauptscreen-Layouts heißt im TFT- und Web-Setup jetzt `2 Werte` und `3 Werte` statt `Standard` und `3 Werte`. In englischer Sprache werden `2 values` und `3 values` angezeigt. Interne Werte und Speicherkompatibilität bleiben unverändert.

## Unverändert

Hauptscreen-Geometrie, Messung, Filterung, Regelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte, Firmwarekontext, Logging und Kommunikationsprotokolle bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build und Prüfung der Auswahltexte auf TFT und Web. Wegen Build 65 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_64 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_64`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_40.zip`
- Arbeitsbasis: V0.50.1_39 / Build 63
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Im zweizeiligen TFT-Hauptscreen wurden beide vollständigen Messwertzeilen weitere 5 px nach links verschoben. Hauptwerte beginnen bei x=242, die Doppelpunkte liegen bei x=222. Schriftgröße, vertikale Position, Rahmen, 3-Werte-Screen und Webdarstellung bleiben unverändert.

## Unverändert

Messung, Filterung, Regelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte, Firmwarekontext, Logging und Kommunikationsprotokolle bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und optische Prüfung auf dem Ziel-TFT. Wegen Build 64 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_63 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_63`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_39.zip`
- Arbeitsbasis: V0.50.1_38 / Build 62
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Letzter Feinschliff der Zahlenhauptscreens: TFT-3-Werte 3 px höher; TFT-2-Werte 5 px weiter links. Auf TFT und Web besitzt der 2-Werte-Screen nun denselben großen Rahmen wie 3-Werte- und Chartansicht. Die Schrift des 2-Werte-Screens bleibt abgesehen von der horizontalen TFT-Verschiebung unverändert.

## Unverändert

Messung, Filterung, Regelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte, Firmwarekontext, Logging und Kommunikationsprotokolle bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und optische Prüfung auf dem Ziel-TFT. Wegen Build 63 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_62 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_62`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_38.zip`
- Arbeitsbasis: V0.50.1_37 / Build 61
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Feinpositionierung der Zahlenhauptscreens: TFT-3-Werte 5 px höher und 12 px weiter rechts; TFT-2-Werte 8 px weiter links. Im Web bleibt die vertikale Geometrie erhalten, während die Hauptwertspalte in beiden Zahlenlayouts wieder exakt auf der Bildschirmmitte liegt.

## Unverändert

Messung, Filterung, Regelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte, Firmwarekontext, Logging und Kommunikationsprotokolle bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und optische Prüfung auf dem Ziel-TFT. Wegen Build 62 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_61 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_61`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_37.zip`
- Arbeitsbasis: frisch entpackte V0.50.1_36 / Build 60
- passendes KeyGen: V0.7.13, unveraendert

## Änderung

Nur das TFT-Hauptscreenlayout wurde nach dem ersten Geraetetest feinjustiert. Der 3-Werte-Block liegt 25 Pixel tiefer. Im 2-Werte-Screen ist der Messwertrahmen horizontal verbreitert; die kleinen Bezeichnungen stehen nun inline links neben den weiterhin 60 px grossen Hauptwerten. Das Weblayout bleibt exakt auf dem Stand von Build 60.

## Unverändert

Messung, Filterung, Regelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte, Firmwarekontext, Logging und Webdarstellung bleiben unveraendert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und optische Prüfung auf dem Ziel-TFT. Wegen Build 61 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_60 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_60`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_36.zip`
- Arbeitsbasis: frisch entpackte V0.50.1.35 / Build 59
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Web und TFT erhalten Beschriftungen für die Hauptmesswerte mit vertikal ausgerichteten Doppelpunkten. Der Webscreen behält seine bisherige vertikale Geometrie. Beim TFT wird nur der 3-Werte-Screen kleiner und höher angeordnet; der 2-Werte-Screen behält seine bisherigen Hauptwertgrößen und -positionen.

## Unverändert

Messung, Filterung, Regelung, Safety, Kalibrier- und Zertifikatsformate, QR-Inhalte, Firmwarekontext und Logging bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und optische Prüfung auf dem Ziel-TFT. Wegen Build 60 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_59 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_59`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_35.zip`
- Arbeitsbasis: frisch entpackte V0.50.1.34 / Build 58
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Der druckbare Kalibrierschein verwendet nun themenweise Druckblöcke. Ein einzelnes Thema wird nicht unnötig über zwei Seiten getrennt; passen zwei oder mehr vollständige Themen auf eine Seite, bleiben sie gemeinsam dort. Die beiden Offline-QR-Codes erhalten immer eine eigene Seite. Die Kalibrierschein-Nr. wird als feste Fußzeile auf jeder Druckseite wiederholt.

## Unverändert

Kalibrier- und Zertifikatsformate, TP3C1 V0.4, TP3Q3 QR-4, Firmwarekontext, kryptografische Prüfungen, Messung, ADC, Regelung und Safety bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build, reale Memory Usage und Drucktest auf dem Zielgerät. Wegen Build 59 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_58 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_58`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_34.zip`
- Arbeitsbasis: frisch entpackte V0.50.1.33 / Build 57
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Die Firmwarestatusmeldung auf `/identity` war nach 47 Zeichen abgeschnitten, weil der Laufzeitpuffer `statusText` nur 48 Byte groß war. Der Puffer wurde auf 96 Byte erweitert. Zusätzlich wurde die Fortschrittstabelle auf 22 % / 58 % / 20 % aufgeteilt und die mittlere Statusspalte umbruchfähig gemacht.

## Unverändert

Firmwarezertifikate, Hashprüfung, Kalibrierformate, TP3C1/TP3Q3, ADC, Regelung und Safety bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build und realer Browsertest. Wegen Build 58 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

---

# TP-3000 V0.50.1_57 – Release Handoff

- interne Version: `0.50.1`
- Build-ID: `0.50.1_57`
- Quellpaket: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_33.zip`
- Arbeitsbasis: frisch entpackte V0.50.1.32 / Build 56
- passendes KeyGen: V0.7.13, unverändert

## Änderung

Der Browserdownload von `.tpscalreq` konnte nach Einführung des zusätzlichen gerätesignierten Firmwarekontexts abbrechen. Das allgemeine 120-ms-Antwortbudget lief bereits während JSON-Aufbau, ECDSA-Signatur und zweier SD-Schreibvorgänge. Wurden danach die korrekten HTTP-Header einschließlich `Content-Length` gesendet, konnte das Budget mitten im JSON ablaufen; Chromium meldete daraufhin „Internetverbindung prüfen“.

Vor den HTTP-Headern wird das Schreibbudget nun neu gestartet. Die gleiche Absicherung wurde für Geräte-, Kopf- und Firmwareanfragen sowie deren Fehlerantworten ergänzt.

## Unverändert

Anfrage- und Zertifikatsformate, Firmwarekontext, SD-Archivierung, KeyGen V0.7.13, TP3C1 V0.4, TP3Q3 QR-4, ADC, Regelung und Safety bleiben unverändert.

## Noch offen

Vollständiger Teensyduino-Build und realer Hardwaretest des Browserdownloads. Wegen Build 57 wird für die optionale Herstellerfreigabe ein neues `.tpfwcert` benötigt.

## V0.50.1_86

- TFT-QR-Rechtsspalte kompakter und UTF-8-sicher
- ENTER löscht QR-Matrix, Direkttexte und alle beteiligten Caches vollständig
- keine Änderung an TP3C1, Deep Link oder kryptografischen Formaten
