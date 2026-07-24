# V0.50.1_87 - finaler GitHub-Quellrelease

- Basis: V0.50.1_86; öffentliche Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_87`.
- Release-Metadaten und Web-Lizenzanzeige auf den finalen Stand vom 24.07.2026 vereinheitlicht.
- Repository-Hauptebene bereinigt; historische Änderungs- und Validierungsdokumente vollständig unter `docs/history/v0.50.1/` geordnet.
- README, Buildanleitung, Releaseunterlagen und Bibliotheksübersicht vollständig aktualisiert.
- Lizenzprüfung ergänzt: pako MIT/zlib, BSD-2-Clause, BSD-3-Clause, CC-BY-SA-4.0 sowie QR-Komponentenhinweise.
- Droid-Sans-Mono-Provenienzgenerator an die bereits verteilte `PROGMEM`-Ablage angepasst; vollständige Font-Regeneration ist wieder bytegleich.
- Panelspezifische GSL1680-Herstellerfirmware aus dem öffentlichen Paket entfernt; lokale Beschaffung bleibt dokumentiert und wird vom Release-Audit erzwungen.
- Keine Änderung an Messung, Regelung, Safety, Kalibrierformaten, Signaturen, TP3C1/TP3Q3, Logging oder LED-Autoadaption.
- Dieser Stand ist die eingefrorene V0.50.1-Basis vor der späteren Ansteuerung von externem QSPI-Flash und PSRAM.

---

# V0.50.1_86 - TFT-QR-Rand und ENTER-Cleanup

- Basis: V0.50.1_85; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_86`.
- Die rechte TFT-QR-Infospalte verwendet kompaktere 11-/14-Pixel-Schriften und feste Zeilen innerhalb der 125-Pixel-Spalte.
- Deutsche Umlaute werden dort über den vorhandenen UTF-8-Ausgabepfad gezeichnet.
- `ENTER` / `zurück zum Menü` ist zweizeilig und läuft nicht mehr in den rechten Displayrand.
- Beim Verlassen der QR-Seite mit ENTER werden reale TFT-Fläche, VirtLCD-Puffer, Buttonzustände und Seiten-Caches vollständig gelöscht beziehungsweise invalidiert. Dadurch bleiben QR-Matrix und Infotexte nicht mehr hinter dem Hauptmenü oder dem FAN-Button sichtbar.
- TP3C1 V0.4, Schema 1.2, Deep Link, QR-Matrix, Signaturen, Webansicht, Messung, Regelung und Safety bleiben unverändert.

# V0.50.1_85 - Einheitlicher TP3C1-Deep-Link in Web und TFT

- Basis: V0.50.1_84; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_85`.
- TFT-QR enthält nun wie Web und Ausdruck den vollständigen Deep Link `tp3000://verify#TP3C1:<Base38>`.
- QR-Segment 1 verwendet Byte-Modus für `tp3000://verify#`; Segment 2 verwendet den kompakten Alphanumerikmodus für `TP3C1:<Base38>`.
- Der Web-QR-Generator erhält beide Segmentmodi ausdrücklich und nutzt damit denselben kanonischen Aufbau wie das TFT.
- TP3C1 V0.4, Schema 1.2, genau zwei Teile, Fragmentierung, CRC-32, ZLIB, Signaturen und TP3Q3 bleiben unverändert.

---

# V0.50.1_84 - TFT-QR-Kalibrierscheinquelle

- Basis: V0.50.1_83; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_84`.
- Der TFT-QR liest das Systemkalibrierungspaket nun mit dem korrekten Approval-Schema V4.
- Dadurch bleibt `CertificateSource` erhalten; die falsche Meldung „Kalibrierscheinquelle ist ungültig“ entfällt.
- Webanzeige, Signaturen und TP3C1-Datenformat bleiben unverändert.

# V0.50.1_83 - TFT-QR-Signaturmodus und kompakte Infoanzeige

- Basis: V0.50.1_82; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_83`.
- Systemkalibrierung wird beim TFT-QR-Export ausdrücklich im kryptografischen Signaturmodus geprüft; aktuelle Bindungs- und Firmwareabweichungen sperren den historischen QR-Nachweis nicht.
- Konkrete TFT-QR-Fehlerursache wird zusätzlich seriell mit `[TFT-QR]` ausgegeben.
- `Info / Gültigkeit` auf 43 physische Zeilen erweitert; jede sichtbare Zeile ist UTF-8-sicher auf maximal 42 Zeichen begrenzt.
- SHA-256-Werte vollständig in zwei Zeilen zu je 32 Zeichen; lange Firmwarehinweise aufgeteilt.
- Deutsche TFT-Texte verwenden echte Umlaute.
- Formate, Signaturen, Messung, Regelung, Safety, Ethernet, LED-Autoadaption und Libraries unverändert.

---

# V0.50.1_82 - TFT-Kalibrierschein-QR trotz Firmwareabweichung

- Basis: V0.50.1_81; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_82`.
- TFT-QR-Verfügbarkeit wird von der kryptografischen Signaturgültigkeit getrennt; eine geänderte Kundenfirmware sperrt Code 1/2 und Code 2/2 nicht mehr.
- Systemkalibrierung bleibt bei reiner Firmwareabweichung signiert und gültig; die Abweichung wird gelb dokumentiert.
- Vorhandenes, nicht passendes Hersteller-Firmwarezertifikat wird als Zertifikat eines anderen Firmwareabbilds gekennzeichnet.
- TP3C1/TP3Q3 enthalten weiterhin Kalibrier-Firmwarehash, aktuellen SHA-256 und `FIRMWARE_CHANGED`.
- Kalibrierformate, Signaturen, Messung, Regelung, Safety, Ethernet, LED-Autoadaption und Libraries unverändert.

---

# V0.50.1_81 - RAM2-Reserve für HTTP wiederhergestellt

- Basis: V0.50.1_80; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_81`.
- Vier LED-Autoadaptionspuffer mit zusammen 16.008 Byte aus `DMAMEM`/RAM2 nach RAM1 verschoben.
- NativeEthernet/FNET erhält damit wieder eine größere zusammenhängende RAM2-Reserve für DHCP, TCP-Sockets und HTTP.
- LED-Modell, System-Grundkurve, TFT-QR, DHCP-Neustartlogik, Zertifikatsformate, Messung, Regelung und Safety unverändert.
- Ordner `libraries/` unverändert.

---

# V0.50.1_80 - DHCP-Neustart und vollständige RAM2-Rückgewinnung

- Basis: V0.50.1_79; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_80`.
- Verbleibende TFT-QR-Texte, QR-Matrix und TP3C1-Metadaten aus RAM2 nach RAM1 verschoben.
- Wechsel zwischen statischer IP und DHCP wird nach dem Speichern durch einen kontrollierten Software-Neustart übernommen.
- Ethernet Aus/Ein im selben Modus verwendet eine noch gültige DHCP-Lease beziehungsweise statische Konfiguration weiter.
- DHCP-Start erhält bei bereits erkanntem Link einen begrenzten zweiten Warteabschnitt; Gesamtbudget 5,3 s, anschließend 30 s Hintergrundnachlauf.
- Kein zyklischer `Ethernet.begin()`-Suchlauf im Messloop; Libraries unverändert.

---

# V0.50.1_78 - Kalibrierschein-QR direkt auf dem TFT

- Basis: V0.50.1_77; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_78`.
- Neuer Hauptmenüpunkt `Kalibrierschein anzeigen` beziehungsweise `Show calibration QR`.
- Native Erzeugung des zweiteiligen TP3C1-V0.4-Transports aus den bereits aktiven und erneut kryptografisch geprüften Geräte-, Kopf- und Systemkalibrierpaketen.
- QR Model 2, Fehlerkorrektur M, automatische Maskenwahl und alphanumerischer Modus.
- Anzeige mit vier Modulen Ruhezone und größtmöglicher ganzzahliger Modulgröße 4×4, 3×3 oder 2×2 TFT-Pixel.
- UP/DOWN wechseln Code 1/2 und Code 2/2; ENTER kehrt ins Menü zurück; EXIT verlässt das Setup zum Hauptscreen.
- Kompakte Anzeige `CODE 1/2` beziehungsweise `CODE 2/2` direkt oberhalb der EXIT-Taste.
- Keine Neusignierung und keine Änderung der gespeicherten Kalibrierpakete.
- Projektlokaler QR-Generator nach Project Nayuki unter MIT-Lizenz; Herkunft in `THIRD_PARTY_NOTICES.md` dokumentiert.
- Ordner `libraries/` gegenüber V0.50.1_77 unverändert.

---

# V0.50.1_77 - adaptive SD-Überwachung für LED-Autoadaption

- Basis: V0.50.1_76; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_77`.
- Bei aktivem SD-Messwertlogging wird nur `sdLogGetStatus()` ausgewertet; keine zusätzliche zyklische `SD.mediaPresent()`-Abfrage der LED-Autoadaption.
- Explizite Loggerfehler `Keine Karte` und `Dateifehler` lösen den vorhandenen kontrollierten Fallback aus.
- Bei gestopptem Logging erfolgt die Medienprüfung spätestens alle `300000 ms` beziehungsweise fünf Minuten.
- Jede Auto-Cal erzwingt bei gestopptem Logging sofort eine Prüfung und startet das Fünf-Minuten-Raster neu.
- Start, bewusste Moduswahl sowie tatsächliche Lese- und Schreibzugriffe prüfen weiterhin unmittelbar.
- Letzter gültiger Auto-Cal-Anker, Herstellerfallback, Stromgrenzen und Stromrampe bleiben unverändert.

---

# V0.50.1_76 - Warnungsbereinigung im eigenen Projektcode

- Basis: V0.50.1_75; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_76`.
- Projektbibliotheken vollständig unverändert gelassen.
- Überlappungssichere, nullterminierte Textkopien in Zertifikats-, Kalibrier- und Logpfaden.
- SD-Archiv- und Firmwarezertifikatspfade werden vor Verwendung vollständig auf Puffergröße geprüft; keine stille Pfadkürzung.
- CRC-geprüfte Schnittstellenkonfigurationen V5 bis V8 werden beim Fehlen gültiger aktueller A/B-Slots wieder migriert.
- TFT-Statuszeilen und Web-Erfolgsmeldungen explizit begrenzt und auf Formatierungsfehler geprüft.
- Reine Alt-/Validierungshelfer als bewusst beibehalten markiert; unbenutzte lokale beziehungsweise doppelte Helfer entfernt.
- Fremdbibliothekswarnungen, insbesondere aus BMP581, bleiben bewusst unverändert.

---

# V0.50.1_75 - 161 Temperaturklassen und Systemkurve im 1-K-Raster

- Basis: V0.50.1_74; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_75`.
- Lernbereich von `-40...+85 °C` auf `-50...+110 °C` erweitert.
- Genau 161 Lernklassen zu je 1 K; Rundung auf die nächste ganze Temperatur bleibt erhalten.
- Externe System-Grundkurve auf Formatversion 2 umgestellt: exakt 161 Werte von -50 bis +110 °C.
- Neue Pflichtfelder `GRID_MIN_C`, `GRID_MAX_C`, `GRID_STEP_K`, `VALID_FROM_C` und `VALID_TO_C`.
- Außerhalb des validierten Systembereichs wird die interne Herstellerkurve stetig am Rand angekoppelt; keine lineare Systemextrapolation.
- Lernmodell-Strukturversion 2; alte 126-Klassen-Modelle werden nicht übernommen.
- Vollständige 161-Punkte-Vorlage unter `SD_CARD_TEMPLATE/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`.

---

# V0.50.1_74 - System-Grundkurve von SD und Grundkurvenbetrieb ohne SD

- Basis: V0.50.1_73; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_74`.
- Bevorzugter Dateipfad der gemessenen System-Grundkurve: `/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV`.
- Eine gültige Systemdatei wird für `Ein - Grundkurve` und `Selbstlernend` verwendet; fehlt sie oder ist sie ungültig, wird die fest eingebaute OD-850FHT-Herstellerkurve verwendet.
- Ohne SD-Karte bleiben `Aus` und `Ein - Grundkurve` verfügbar. Nur `Selbstlernend` ist gesperrt.
- Bei SD-Entnahme während `Selbstlernend` erfolgt der automatische Rückfall auf `Ein - Grundkurve` mit interner Herstellerkurve.
- Die Systemkurve enthält relative Stromfaktoren, wird auf die angegebene Referenztemperatur normiert und durch feste Format- und Plausibilitätsgrenzen geprüft.
- Innerhalb des Systembereichs wird linear interpoliert. Außerhalb wird keine Systemsteigung extrapoliert; die Randkorrektur bleibt erhalten und nur der Hersteller-Temperaturgang wird fortgesetzt.
- Das Lernmodell ist über eine 24-Bit-Kurvenkennung in den vorhandenen reservierten Modellbytes an die Grundkurve gebunden. Bestehende Modelle der internen Herstellerkurve bleiben kompatibel; bei einer geänderten Systemkurve beginnt ein neues Modell.
- TFT, Web und Backend lassen ohne SD die Modi 0 und 1 zu und lehnen ausschließlich Modus 2 ab.

# V0.50.1_73 - LED-Autoadaption aus RAM1 in FLASHMEM

- Basis: V0.50.1_72; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_73`.
- Die umfangreiche LED-Autoadaptionslogik wurde aus dem ITCM/RAM1-Codebereich in `FLASHMEM` verschoben.
- Modell-, Lern-, Interpolations-, SD-, CRC-, Modus- und Diagnosefunktionen laufen aus dem Program-Flash.
- Nur `ledAdaptationTask()` bleibt als kleiner Hauptschleifen-Einstieg im ITCM; die eigentliche Verarbeitung wird höchstens alle 250 ms an eine `FLASHMEM`-/`noinline`-Funktion delegiert.
- Temperaturfilter, 1-s-Stromrampe, Lernmodell, Dateiformat, Grenzwerte, Regelung und Safety bleiben funktional unverändert.
- Die tatsächliche ITCM-/Padding-Änderung muss im vollständigen Teensyduino-Build geprüft werden.

# V0.50.1_72 - Begrenzte effektive Historie der LED-Autoadaption

- Basis: V0.50.1_71; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_72`.
- Vertrauenszaehlung und Mittelwertnachfuehrung der 1-K-Lernklassen sind nun getrennt.
- Die Vertrauenszaehlung bleibt bis 1000 bestaetigte Werte erhalten und steuert weiterhin die Freigabestufen 0/25/50/75/90 %.
- Der gelernte Formfaktor nutzt maximal 128 Werte effektive Historie. Ab dem 128. bestaetigten Wert erhaelt jeder neue Wert konstant 1/128 Einfluss.
- Dadurch halbiert sich der Einfluss des vorherigen Modells nach rund 89 neuen gueltigen Auto-Cals derselben Temperaturklasse. Selten erreichte Sommer-/Winterklassen werden nicht allein durch Kalenderalter geloescht.
- Die vorhandenen kopfbezogenen SD-Modelle bleiben formatkompatibel; alte Mittelwerte werden ab dem naechsten gueltigen Lernwert mit der neuen Nachfuehrung aktualisiert.

# V0.50.1_71 - LED-Autoadaption mit Grundkurve und kopfbezogenem Lernen

- Basis: V0.50.1_70; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_71`.
- Neue Auswahl `LED-Autoadaption`: `Aus`, `Ein - Grundkurve` und `Selbstlernend` in TFT- und Web-Setup.
- `Aus` wendet keine Temperaturvorsteuerung an; `Ein` verwendet ausschließlich die konservativ begrenzte OD-850FHT-Datenblatt-Grundkurve; `Selbstlernend` ergänzt diese um kopfbezogene Lerndaten.
- Bei vorhandener SD-Karte werden aus jedem geeigneten erfolgreichen Auto-Cal-Durchlauf Lerninformationen gesammelt, unabhängig vom gewählten Anwendungsmodus. Die Lerndaten wirken ausschließlich in `Selbstlernend`.
- Das Lernmodell verwendet die gefilterte externe Umgebungstemperatur und den tatsächlich von der LED-Auto-Cal gefundenen Strom. Aufeinanderfolgende Auto-Cals lernen nur die relative Abweichung vom Datenblatt-Temperaturgang; der letzte echte Auto-Cal-Strom bleibt immer der absolute Anker.
- Lerndaten werden getrennt nach Kopftyp und Kopf-SN als CRC-gesicherte A/B-Dateien unter `/LEDADAPT` auf SD gespeichert. Die Modellpuffer liegen in RAM2.
- Ohne verfügbare SD-Karte ist ausschließlich `Aus` möglich; ein gespeicherter anderer Modus wird beim Start beziehungsweise bei einem SD-Fehler sicher auf `Aus` gesetzt. Bei aktiver Adaption wird das Medium zusätzlich alle 30 s geprüft.
- `LED-Lerndaten zurücksetzen` löscht nach Bestätigung ausschließlich die beiden Modellslots des aktuell gewählten Kopfes. Daten anderer Köpfe bleiben erhalten.
- Lernfreigabe ab drei bestätigten Werten je 1-K-Klasse; 25/50/75/90 % Lerngewicht, höchstens 90 %. Interpolation nur mit beidseitigen Stützstellen innerhalb 3 K und einer Gesamtlücke bis 6 K. Keine Steigungsextrapolation; der nächste Randwert wird außerhalb des Lernbereichs über 3 K auf die Grundkurve ausgeblendet.
- Gelernte Formkorrektur ist auf ±5 %, die gesamte laufende Stromabweichung vom letzten Auto-Cal auf ±3 % und die Stromänderung auf 0,02 %/s begrenzt. Ausreißer über 1 % benötigen drei übereinstimmende Bestätigungen.
- ADC2-Dunkelmessung vor jeder Auto-Cal, Optikziel, eigentliche LED-Auto-Cal, Pt100-Messung, Peltierregelung, Safety, Kalibrier- und Zertifikatsformate bleiben unverändert.

# V0.50.1_70 - ADC2-Dunkelwert vor jeder Optik-Auto-Cal

- Basis: V0.50.1_69; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_70`.
- Beim Eintritt in jede LED-Auto-Cal wird die IR-LED hardwareseitig abgeschaltet, 50 ms eingeschwungen und der ADC2-Dunkelwert aus 64 Stichproben neu gemittelt.
- Der ADC2-Lichtpuffer wird nach der Dunkelmessung zurückgesetzt; erst danach wird die LED mit der aktuellen Auto-Cal-Stromvorgabe wieder eingeschaltet.
- Auto-Cal-Startzeit, Schritttakt und 10-s-Timeout beginnen erst nach der Dunkelmessung, sodass die Messdauer nicht vom Auto-Cal-Zeitbudget abgezogen wird.
- Die Optikdiagnose bewertet dadurch nun die reale Dunkelwertänderung zwischen aufeinanderfolgenden Auto-Cal-Durchläufen statt wiederholt denselben Startwert.
- Schlägt die neue Dunkelmessung fehl, bleibt der zuletzt gültige Dunkelwert erhalten und die Auto-Cal läuft mit einer seriellen Warnmeldung weiter.
- Optikziel, LED-Regelalgorithmus, Pt100-Messung, SFOCAL, Peltierregelung, Safety, Kalibrierformate, Zertifikate, QR-Daten und Logging bleiben unverändert.

# V0.50.1_69 - stabile Sprachumschaltung im Kalibrierschein

- Basis: V0.50.1_68; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_69`.
- Deutsch/English-Umschalter bleibt unabhängig von der Textlänge immer im oberen Bereich der Werkzeugleiste.
- Aktionsschaltflächen und Sprachumschaltung sind in getrennten Layoutbereichen angeordnet.
- Redundante Fertigmeldung unter der Werkzeugleiste wird nach erfolgreichem Aufbau ausgeblendet; Lade- und Fehlermeldungen bleiben erhalten.
- Kalibrier- und Signaturdaten bleiben unverändert.

# V0.50.1_68 - leere Kalibrierscheinfelder und bereinigter Webhinweis

- Basis: V0.50.1_67; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_68`.
- Leere optionale Felder im Web- und A4-Kalibrierschein werden als Gedankenstrich `—` dargestellt.
- Die sechs kalibriertechnischen Fachfelder bleiben auch bei leerem Inhalt sichtbar und zeigen `—`.
- Leere optionale Firmwarezertifikatsfelder zeigen ebenfalls `—`.
- Die Ersetzung erfolgt nur in der Darstellung; signierte Daten, kanonische Bytes und Hashes bleiben unverändert.
- Redundanten Hinweistext unter den Systemkalibrierungs-Steuerelementen der Firmware-Webseite entfernt.
- Kalibrierformate, QR-Inhalte, Signaturen, Firmwarekontext, Messung, Regelung und Safety bleiben unverändert.

# V0.50.1_67 - Kalibrierschein-Druckfeinschliff

- Basis: V0.50.1_66; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_67`.
- TP3C1-Druckgröße von 115 mm auf 112 mm reduziert.
- Prüfverfahren-Zusatz in den QR-Themenblock verschoben, damit er nicht allein auf eine zusätzliche Seite rutscht.
- Kalibrierschein-Nr. im großen Kopf von Seite 1 bleibt auch bei langem englischem Titel einzeilig.
- Deutsche Druckansicht vertikal moderat verdichtet; der vollständige Systemkalibrierungsblock passt in der geprüften Belegung wieder auf Seite 1.
- QR-Inhalt, Signaturen, Hashes, Kalibrierformate, Firmwarekontext, Messung, Regelung und Safety bleiben unverändert.

# V0.50.1_65 – eindeutige Hauptscreen-Auswahl im Setup

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_40.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_65`.
- Unter `Anzeige > Hauptscreen Layout` heißt die erste Auswahl auf TFT und Web nun eindeutig `2 Werte` statt `Standard`; die zweite Auswahl bleibt `3 Werte`.
- In englischer Sprache lauten die beiden Einträge entsprechend `2 values` und `3 values`.
- Interne Layoutwerte, gespeicherte Konfiguration, Hauptscreen-Geometrie, Messung, Filterung, Regelung, Safety, Kalibrierung, Zertifikate, QR, Logging und Kommunikationsprotokolle bleiben unverändert.

# V0.50.1_64 – TFT-2-Werte-Screen weitere 5 px links

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_39.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_64`.
- TFT-2-Werte-Screen: beide vollständigen Zeilenblöcke weitere 5 px nach links verschoben. Hauptwerte beginnen jetzt bei x=242; Doppelpunkte liegen bei x=222. Schriftgröße und vertikale Positionen bleiben unverändert.
- TFT-3-Werte-Screen, gemeinsamer TFT-Rahmen, Touchflächen und Webdarstellung bleiben unverändert.
- Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle bleiben unverändert.

# V0.50.1_63 – gemeinsamer Rahmen und Hauptscreen-Feinschliff

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_38.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_63`.
- TFT-3-Werte-Screen: alle drei Hauptwerte und Beschriftungen weitere 3 px nach oben verschoben. Hauptwerte liegen bei x=272 und y=85/169/253; Doppelpunkte bei x=252 und Beschriftungen bei y=117/201/285.
- TFT-2-Werte-Screen: beide vollständigen Zeilenblöcke weitere 5 px nach links verschoben. Hauptwerte beginnen bei x=247; Doppelpunkte liegen bei x=227. Schriftgröße und vertikale Positionen bleiben unverändert.
- TFT: 2-Werte-, 3-Werte- und Chartansicht verwenden nun denselben Rahmen und dieselbe Touchfläche bei x=12, y=50, Breite 776 und Höhe 286.
- Web: Auch der 2-Werte-Screen verwendet nun den großen Rahmen bei x=12, y=50, Breite 776 und Höhe 286. Die 2-Werte-Schrift bleibt unverändert an ihrer bisherigen Position.
- Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle bleiben unverändert.

# V0.50.1_62 – Hauptscreen-Feinlage TFT und Web

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_37.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_62`.
- TFT-3-Werte-Screen: alle drei Hauptwerte und Beschriftungen 5 px nach oben sowie 12 px nach rechts verschoben. Hauptwerte liegen jetzt bei x=272 und y=88/172/256; Doppelpunkte bei x=252 und y=120/204/288.
- TFT-2-Werte-Screen: beide vollständigen Zeilenblöcke 8 px nach links verschoben. Hauptwerte beginnen bei x=252; Doppelpunkte liegen bei x=232. Vertikale Positionen, Schriftgrößen und Rahmen bleiben unverändert.
- Web: die vertikale Lage bleibt unverändert. Die Hauptwertspalte ist im 2- und 3-Werte-Layout wieder exakt bei x=400 zentriert; Beschriftungen bleiben rechtsbündig und die Doppelpunkte vertikal ausgerichtet.
- Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle bleiben unverändert.

# V0.50.1_61 – TFT-Hauptscreen feinpositioniert

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_36.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_61`.
- Im TFT-3-Werte-Screen wurde der komplette Messwertblock um 25 Pixel nach unten verschoben. Hauptwerte liegen jetzt bei y=93/177/261, die 16-px-Beschriftungen bei y=125/209/293. Zeilenabstand, Schriftgroesse 48 und Doppelpunktposition x=240 bleiben unveraendert.
- Der TFT-2-Werte-Rahmen nutzt nun die volle Dashboardbreite x=12 bis x=787, behaelt aber seine bisherige vertikale Lage y=88 bis y=297.
- Die beiden 60-px-Hauptwerte beginnen im 2-Werte-Screen bei x=260. `Rel. Feuchte:` und `Taupunkt:` stehen in Schriftgroesse 16 inline links davor; beide Doppelpunkte liegen bei x=240.
- Die zusaetzliche fuehrende Leerstelle der beiden grossen 2-Werte-Zeilen wurde entfernt, damit Wert und Einheit innerhalb des verbreiterten Rahmens bleiben.
- Die Touchflaeche des 2-Werte-Rahmens folgt der neuen vollen Breite.
- Weblayout, Messung, Filterung, Regelung, Safety, Kalibrierwerte, Zertifikats- und QR-Formate, Logging und Kommunikationsprotokolle bleiben unveraendert.

## 2026-07-18 - V0.50.1_60 TEST - Hauptscreen-Beschriftungen und getrennte Web/TFT-Geometrie

- Web-Hauptscreen: `Rel. Feuchte`, `Taupunkt` und im 3-Werte-Modus `T-Umgebung` als 16-px-Beschriftungen ergänzt.
- Web: Die Beschriftungen sind rechtsbündig; alle Doppelpunkte stehen auf derselben Vertikalen. Die bereits passende vertikale Anordnung der Messwertzeilen bleibt unverändert.
- TFT-3-Werte-Screen: Nur dieser Modus verwendet für die drei Hauptmesswerte DroidSansMono 48 statt 60. Der Dreierblock wurde mit den Zeilenursprüngen y=68, 152 und 236 deutlich nach oben gesetzt, ohne den Zeilenabstand zusammenzudrücken.
- TFT-3-Werte-Screen: Beschriftungen in DroidSansMono 16 inline links vor den Werten; Doppelpunkte fest bei x=240.
- TFT-2-Werte-Screen: Schriftgröße und Positionen der beiden bisherigen Hauptwerte bleiben unverändert. Die kleinen Beschriftungen sitzen an der oberen Kante der jeweiligen Zeile; Doppelpunkte fest bei x=300.
- Beim Layoutwechsel wird die Hauptwert-TextBox passend neu initialisiert und der Sichtcache vollständig verworfen.
- Messung, Regelung, Safety, Kalibrierung, Zertifikate, Logging und Datenformate bleiben unverändert.

# V0.50.1_59 – Kalibrierschein themenweise paginiert

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_34.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_59`.
- Die Druckansicht behandelt Zertifikatsaussteller, Systemkalibrierung, Kalibrierergebnisse, Gerätejustierung, Kopfjustierung und kalibriertechnische Angaben als zusammenhängende Themenblöcke.
- Ein Thema wird bei ausreichender Seitengröße nicht geteilt; mehrere vollständige Themen dürfen weiterhin dieselbe Seite nutzen.
- Bei umfangreichen As-Found/As-Left-Daten kann zwischen den beiden vollständigen Ergebnistabellen umgebrochen werden, nicht mitten in einer Tabelle.
- Die Offline-Prüfung mit den beiden TP3C1-QR-Codes beginnt immer auf einer neuen Seite.
- Die Kalibrierschein-Nr. wird als feste Druckfußzeile auf jeder Seite wiederholt.
- Webansicht, Signaturen, Kalibrierformate, QR-Inhalt, Firmwarekontext, ADC, Regelung und Safety bleiben unverändert.

# V0.50.1_58 – Zertifizierungsübersicht vollständig lesbar

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_33.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_58`.
- Die Meldung `Firmwarezertifikat passt nicht zum laufenden Abbild` wurde auf `/identity` nach 47 Zeichen abgeschnitten. Ursache war der nur 48 Byte große Laufzeitpuffer `TpFirmwareApprovalStatus::statusText`.
- Der Puffer besitzt jetzt 96 Byte und übernimmt den vollständigen Status aus der Firmwareintegritätsprüfung.
- Die Fortschrittstabelle nutzt nun 22 % / 58 % / 20 %. Die mittlere Statusspalte ist breiter und darf sauber umbrechen; die rechte Zusatzspalte bleibt lesbar.
- Zertifikatsprüfung, Hashberechnung, Kalibrierung, QR-Code, Messung, Regelung und Safety bleiben unverändert.

# V0.50.1_57 – Download der Systemkalibrierungsanfrage stabilisiert

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_32.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_57`.
- Beim Export einer neuen `.tpscalreq` konnte Chromium den Download mit „Internetverbindung prüfen“ abbrechen, obwohl Dateiname und HTTP-Header bereits empfangen worden waren.
- Ursache: Das 120-ms-Schreibbudget des `EthBoundedWriter` begann bereits beim Eingang des Requests. Aufbau und ECDSA-Signatur der Systemanfrage, Schreiben des gerätesignierten Firmwarekontexts sowie die zusätzliche SD-Archivkopie konnten dieses Budget vollständig verbrauchen, bevor die eigentliche HTTP-Dateiübertragung begann. Die Antwort wurde dann nach den Headern beziehungsweise mitten im JSON beendet; der Browser erkannte die Abweichung zur angekündigten `Content-Length` als Netzwerkfehler.
- Nach Abschluss aller Erzeugungs-, Signatur- und SD-Schritte wird das Antwortbudget jetzt unmittelbar vor den HTTP-Headern neu gestartet. Das gilt vorsorglich auch für Geräte-, Kopf- und Firmwareanfragen sowie deren Fehlerantworten.
- Requestformate, Signaturen, SD-Kopien, Firmwarekontext, Kalibrierwerte, TP3C1/TP3Q3, Messung, Regelung und Safety bleiben unverändert.

# V0.50.1_56 – strenger Firmwarebezug und QR-Nachweis

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_31.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_56`.
- Kundenspezifische Firmware ist bei der Systemkalibrierung weiterhin zulässig. Der exakt verwendete Firmware-SHA-256 wird gerätesigniert dokumentiert; ein Hersteller-Firmwarezertifikat bleibt optional.
- Stimmt der aktuell laufende SHA-256 später nicht mehr mit dem Kalibrierstand überein, bleibt der Kalibrierschein historisch und kryptografisch gültig. Seine Anwendbarkeit auf den aktuellen Gerätezustand wird jedoch rot als „möglicherweise nicht mehr gültig“ bewertet.
- Die Abweichung erscheint in `Info / Gültigkeit`, Kalibrierverwaltung, Zertifikatsübersicht, druckbarem Kalibrierschein und TFT-Info. Messbetrieb und zertifizierte Ausgaben bleiben möglich.
- Zertifizierte Logkontexte verwenden `TP3000-LOG-CONTEXT-2` und enthalten den gerätesignierten Firmwarekontext sowie `EXACT_MATCH`, `FIRMWARE_CHANGED_POSSIBLY_INVALID` oder `NOT_COMPARABLE`.
- TP3C1 wurde auf V0.4 / Schema 1.2 erweitert. Der kompakte QR-Nachweis enthält den gerätesignierten Firmwarekontext, den Vergleichsstatus und den aktuellen Firmware-SHA-256 zum Exportzeitpunkt. TP3Q3 verwendet `TP3000-CALIBRATION-QR-4`.
- Das gilt gleichermaßen für TP-3000-eigene und externe PDF-Kalibrierscheine; das externe Original-PDF bleibt unverändert.
- Eine neue signierte Systemkalibrierung kann den neuen Firmwarestand nach erfolgter Bewertung nachzertifizieren. Die kanonischen `.tpdcal`, `.tphcal` und `.tpscal`-Formate sowie KeyGen V0.7.13 bleiben unverändert.

# V0.50.1_55 – Firmwarekontext der Systemkalibrierung

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_30.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_55`.
- Neue Systemkalibrierungsanfragen erzeugen einen gerätesignierten Firmwarekontext mit Version, Build, Ist-SHA-256, Abbildgröße und optionalem Hersteller-Firmwarezertifikat.
- Der Kontext ist an `SourceRequestId` und `SourceRequestManifestSha256` gebunden und gilt dadurch auch für eine Systemkalibrierung mit externem PDF-Kalibrierschein.
- Der Web-Kalibrierschein zeigt den damaligen Firmwarestand, Herstellerstatus und einen neutralen Vergleich zur aktuell laufenden Firmware. Eine Abweichung beendet die Systemkalibrierung nicht automatisch.
- Fehlende Herstellerfreigabe blockiert weder Justierung/Systemkalibrierung noch zertifizierte CSV-/TPSIG-/TPLOG-Ausgaben. Der genaue Ist-Hash bleibt Bestandteil des Nachweises; die externe Firmwarevalidierung liegt beim Betreiber.
- Externe Validierungsdokumente werden nicht im TP-3000 gespeichert.

# V0.50.1_54 – Web-Upload des Firmwarezertifikats repariert

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_29.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_54`.
- Der nicht blockierende HTTP-Requestleser behandelt `/firmware-certificate-upload` jetzt wie die übrigen JSON-Dateiuploads und liest den durch `Content-Length` angekündigten Request-Body vollständig in den begrenzten Uploadpuffer ein.
- Ursache der bisherigen Meldung „Keine .tpfwcert-Datei übertragen“: Der Pfad wurde zwar korrekt geroutet, war aber in der Body-Whitelist des Requestlesers vergessen. Dadurch wurde der Handler mit `nullptr` und Länge 0 aufgerufen, obwohl der Browser die ausgewählte Datei gesendet hatte.
- SD-Import, Zertifikatsformat, Signaturprüfung, Kalibrierverwaltung, Bootdiagnose, Messung, Regelung und Safety bleiben unverändert.

# V0.50.1_53 – Kalibrierverwaltung nach Nachweisart gegliedert

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_28.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_53`.
- Gerätejustierung, Kopfjustierung, Systemkalibrierung und Firmwarezertifikat besitzen jetzt jeweils einen vollständigen eigenen Bedienbereich: Anfrage herunterladen, signierte Datei von SD importieren und signierte Datei direkt vom PC hochladen.
- Die bisherigen getrennten Abschnitte „Anfragen exportieren“, „Signierte Freigaben direkt hochladen“ und der alternative SD-Import wurden entfernt; dadurch ist jede Datei eindeutig ihrem Statusblock zugeordnet.
- Für Systemkalibrierungen wurde der bislang fehlende SD-Import ergänzt. Aus `/CALIBRATION` wird nur eine kryptografisch gültige, zur aktuellen Geräte-/Kopfkombination passende `.tpscal` ausgewählt; bei mehreren passenden Dateien gewinnt die jüngste nach `Gültig ab`, danach nach Kalibrierdatum.
- Firmware-, Justierungs- und Kalibrierformate, Signaturen, Messwerte, ADC, Regelung, Safety, TP3C1/TP3Q3, TPSIG und TPLOG bleiben unverändert. Das geänderte Firmwareabbild benötigt ein neues Firmwarezertifikat.

# V0.50.1_52 – Nullzeiger-Bootfix und RAM-Rückgewinnung

- Arbeitsbasis: Diagnose-Stand `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_27.zip`, ursprünglich frisch aus der bestätigten V0.50.1.26-Basis; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_52`.
- Der CrashReport aus Build `0.50.1_50` wurde mit dem passenden ELF aufgelöst: Adresse `0x37E82` liegt in `memset()`, Zieladresse war `0x00000000`. Unmittelbar davor erzeugte `setup()` zwei jeweils 3628 Byte große `TextBox`-Objekte mit `new TextBox()` und prüfte das Ergebnis nicht. Bei erschöpftem oder fragmentiertem RAM2-Heap führte das compilererzeugte Nullsetzen deshalb zu `memset(nullptr, 0, 3628)` und zum MPU-DACCVIOL.
- `VirtLCDMenu` und `VirtLCDMessage` sind nun statische, beim Start bereits vorhandene Objekte. Im Bootpfad und beim erneuten Öffnen des Setups gibt es dafür kein `operator new()` und keine Heap-Fragmentierung mehr.
- Alle tatsächlich gelinkten DroidSansMono-Daten-, Index- und Deskriptortabellen werden über `PROGMEM` im speicherabgebildeten QSPI-Flash abgelegt. Das Generatorwerkzeug erzeugt dieselbe Ablage dauerhaft reproduzierbar.
- Die zwei bisher getrennten 4096-Byte-DMAMEM-Puffer für Firmware- und externe PDF-SHA-256-Prüfung wurden zu einem gemeinsamen, nur nacheinander verwendeten Arbeitspuffer zusammengeführt.
- Erwartung gegenüber Build `0.50.1_51`: etwa 29.126 Byte mehr freier RAM1 und 4096 Byte mehr freier RAM2; die endgültigen Werte sind nach dem ersten Teensyduino-Build zu protokollieren.
- SD-Crashdiagnose, Firmwarezertifikat, Kalibrierformate, TP3C1/TP3Q3, TPSIG, TPLOG, ADC, Regelung und Safety bleiben funktional unverändert. Wegen des geänderten Firmwareabbilds ist ein neues `.tpfwcert` erforderlich.

# V0.50.1_51 – CrashReport und Bootstufen auf SD

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_26.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_51`.
- Die frühe Bootdiagnose wird unabhängig vom normalen Messwertlogging in `/TP3000_BOOT.TXT` im SD-Wurzelverzeichnis protokolliert. Gespeichert werden Build, `millis()`, `SRC_SRSR`, Bootstufe und ein vorhandener vollständiger Teensy-CrashReport.
- Jede Diagnoseetappe wird geflusht und geschlossen. Die Datei wird bei mehr als 128 KiB neu begonnen.
- Nach einem gespeicherten CrashReport oder erkannten Watchdog-Reset wird die potenziell abstürzende Firmwarehash-Prüfung in genau diesem Wiederanlauf übersprungen. Das Gerät kann bis zum Hauptbildschirm starten; zertifizierte Funktionen bleiben wegen Firmwarestatus `ERROR` gesperrt.
- CrashReport-Breadcrumb 1 kennzeichnet fünf Bootstufen. Breadcrumb 2 enthält während des Firmwarehashes den zuletzt vollständig gelesenen 4096-Byte-Offset.
- Firmwarezertifikatsformat, Kalibrierformate, TP3C1/TP3Q3, TPSIG, TPLOG, ADC, Regelung und Safety bleiben unverändert. Der neue Build benötigt nach der Diagnose ein neues `.tpfwcert`.

# V0.50.1_50 – Boot-Watchdog während Firmwarehash sicher bedient

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-17_V0.50.1_25.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_50`.
- Ursache des Bootloops: Der 8-s-Hardware-Watchdog wurde bisher erst am Ende von `setup()` neu gestartet und während des kompletten Bootvorgangs nicht gefüttert. Der neue vollständige Firmwarehash plus SD-Statusschreiben überschritt die Watchdogfrist; das Gerät blieb sichtbar auf dem Splashscreen und startete nach etwa 8 s neu.
- Der Watchdog wird nun unmittelbar am Beginn von `setup()` initialisiert. Während des Bootens wird er ausschließlich an klar definierten Checkpoints bedient.
- Die Firmware-SHA-256-Prüfung bedient den Watchdog nach jedem 4096-Byte-Block. Der normale Messbetrieb bleibt unverändert: Nach Abschluss von `setup()` wird weiterhin nur am Ende einer vollständig durchlaufenen Hauptloop gefüttert.
- Die serielle Bootdiagnose meldet zusätzlich `FIRMWARE HASH DAUER: <ms>`, damit die reale Prüfzeit am Gerät messbar ist.
- Firmwarezertifikatsformat, Root-Signatur, Kalibrierformate, TP3C1/TP3Q3, TPSIG, TPLOG, Messung, Regelung und Safety bleiben unverändert.

# V0.50.1_49 – root-signiertes Firmwarezertifikat statt HEX-Patch

- Der TP-3000 berechnet beim Start weiterhin SHA-256 über das vollständige tatsächlich gelinkte Firmwareabbild.
- Neue Geräte-signierte Anfrage `TP3000-FIRMWARE-CERTIFICATE-REQUEST-1` (`.tpfwreq`) mit 72-h-Gültigkeit und eingebettetem Gerätezertifikat.
- Neues generisches, mit dem privaten Hersteller-Root signiertes `TP3000-FIRMWARE-CERTIFICATE-1` (`.tpfwcert`).
- Webexport, Direktupload und SD-Import ergänzt; aktive Datei `/CERTIFICATION/FIRMWARE.TPFCERT`.
- Soll-/Ist-Hash, Zertifikats-ID, Root-Key-ID und Ausstellungszeit erscheinen unter `Info / Gültigkeit`.
- Post-Link-Finalisierung und `*_final.hex` entfallen.
- Labor-Key besitzt keine Firmwareberechtigung.
- Kalibrierformate, Signaturen, TP3C1/TP3Q3, TPSIG, TPLOG, Messung, Regelung und Safety unverändert.

# V0.50.1_48 – vollständige Firmwareabbild-Integritätsprüfung

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_23.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_48`.
- Ein 112-Byte-Buildmanifest im Program-Flash enthält Version, Build-ID, Abbildbasis, Linker-Abbildgröße, Soll-SHA-256 und Manifest-CRC.
- Das Post-Build-Werkzeug `tools/finalize_firmware_manifest.py/.cmd` versiegelt die exportierte HEX-Datei und erzeugt `*_final.hex` sowie das passende Binärabbild.
- Beim Start wird das vollständige Teensy-4.1-XIP-Abbild ab `0x60000000` in 4096-Byte-Blöcken gehasht. Erwarteter Hash und Manifest-CRC werden beim Hashen auf Null normalisiert, um einen Kreisbezug auszuschließen.
- Web und TFT `Info / Gültigkeit` zeigen Status, Version/Build, Abbildgröße, Soll-/Ist-SHA-256 und Prüfzeitpunkt.
- `/CERTIFICATION/FIRMWARE.TPS` wird transaktional mit TMP/BAK und vollständiger Rückleseprüfung gespeichert.
- Ohne bestätigte Firmwareintegrität werden neue Geräte-/Kopf-/Systemkalibrierungsanfragen, neue Kalibrieraktivierungen sowie zertifizierte CSV-/TPSIG-/TPLOG-Ausgaben gesperrt. Normale Messung und Anzeige bleiben verfügbar.
- TPSIG/TPLOG übernehmen den real gemessenen Firmwareabbild-SHA-256 in das bereits vorhandene Firmware-Manifest-Hashfeld; die Containerformate bleiben unverändert.
- Der SHA-256 wird in diesem Build noch nicht als neues kanonisches Feld in Kalibrierpakete aufgenommen. Das vermeidet eine unkoordinierte Änderung der KeyGen-/Signaturformate.
- Ohne Secure Boot bleibt dies ein Integritäts- und Betriebsnachweis, keine hardwareverankerte Unveränderlichkeit.

# V0.50.1_47 – TP3C1 V0.3 akzeptiert Root- und Labor-Signierer

- Der QR-Encoder akzeptiert für Gerätejustierung, Kopfjustierung und Systemkalibrierung jeweils `MANUFACTURER_ROOT` oder `CALIBRATION_LAB`.
- Ein Labor-Rollenzertifikat wird nur transportiert, wenn mindestens eines der drei Dokumente von einem Labor signiert ist.
- Alle labor-signierten Dokumente müssen weiterhin dasselbe Laborzertifikat verwenden.
- `Root/Root/Root`, `Labor/Labor/Root` und gemischte Kombinationen mit einem gemeinsamen Labor werden unterstützt.
- Die irreführende starre Prüfung `Labor/Labor/Hersteller-Root` wurde entfernt.

# V0.50.1_46 – TP3C1 V0.3

- gedruckter Offline-Transport ausschließlich für Gerätejustierung V3, Kopfjustierung V3 und Systemkalibrierung V2
- Kalibrierumfang und TPC1-Messpunktdaten in allen drei Kalibrierblöcken
- Schema 1.1, RequiredFeatures 0x3F, Transportbytes 0x31/0x32
- strikte TPC1-Prüfung; V2-Justierungen bewusst nicht unterstützt
- TP3Q3, Signaturen, Kalibrierformate, Messung, Regelung und Safety unverändert

# V0.50.1_45 – transportabler Anwendbarkeitsanhang und Druckkennzeichnung

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_20.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_45`.
- Der zentrale Endstatus aus `/CALIBRATION/APPLICABILITY.TPS` wird zusätzlich hinter dem unveränderten signierten JSON-Bereich der betreffenden `.tpscal` gespeichert.
- Der Anhang beginnt mit `--TP3000-APPLICABILITY-1--`, enthält Manifest, UTC-Endzeitpunkt, Ereigniscode und den bereits verwendeten Datensatz-SHA-256 und endet mit `--TP3000-APPLICABILITY-END--`.
- Systemkalibrierungsdateien werden für Signatur, Web-JSON, TP3C1/TP3Q3, TPSIG und TPLOG weiterhin ausschließlich bis zum Appendix-Marker ausgewertet.
- Das Anhängen erfolgt transaktional über `*.apptmp` und `*.appbak` mit vollständiger Rückleseprüfung und Wiederaufnahme nach unterbrochener Rename-Phase.
- Im dargestellten und gedruckten Systemkalibrierschein wird das ursprüngliche `Gültig bis` lesbar durchgestrichen. Direkt darunter erscheinen `Anwendbar bis`, `Valide – nicht mehr anwendbar` und der kurze Änderungsgrund. Im Druck bleiben alle Angaben schwarz.

# V0.50.1_44 – Anwendbarkeitsende historischer Systemkalibrierungen auf SD

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_19.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_44`.
- Die neue zentrale Datei `/CALIBRATION/APPLICABILITY.TPS` hält append-only den ersten UTC-Zeitpunkt und einen festen Ereigniscode fest, ab dem ein signierter Systemkalibrierschein nicht mehr zur damaligen Geräte-/Kopfkonfiguration anwendbar ist. Der Schein selbst und sein Manifest bleiben unverändert.
- Erfasst werden Änderungen der aktuellen Kopf- oder Gerätejustierwerte, der Wechsel auf eine andere signierte Kopf- oder Gerätejustierung sowie das Ersetzen durch eine neue Systemkalibrierung.
- Die zentralen Speicherpfade `tpMainConfigSave()` und `tpMetrologyConfigSave()` verwerfen den Kalibrierungs-Cache unmittelbar nach erfolgreichem Speichern. Dadurch wird eine echte relevante Abweichung zeitnah erkannt; reine UI-/Interfaceänderungen erzeugen keinen Eintrag.
- Ein reiner Kopfwechsel beendet die Kalibrierung des abgenommenen Kopfes nicht dauerhaft. Auch ein zeitweilig fehlendes externes Original-PDF erzeugt keinen irreversiblen Ende-Datensatz.
- Justierungsimporte schreiben das Anwendbarkeitsende erst nach erfolgreicher Signaturprüfung, Rückleseprüfung und Archivierung; fehlgeschlagene Importtransaktionen beenden den bisherigen Systemschein nicht.
- Eine unvollständige letzte Statuszeile nach Stromausfall wird ignoriert. Jede vollständige Zeile muss syntaktisch korrekt sein und ihren SHA-256 bestätigen; eine beschädigte Statusdatei sperrt die automatische Aktivierung fail-closed.
- Beendete historische Scheine bleiben such- und anzeigbar. Die Webübersicht kennzeichnet sie kurz als `Valide – bis DD.MM.YYYY HH:MM UTC` und zeigt den Änderungsgrund.
- Für Altzustände ohne bereits gespeicherten Endzeitpunkt unterscheidet die Übersicht zusätzlich `Valide – andere Justierung` und `Valide – nicht aktiv`, statt sie pauschal wie den aktiven Schein als gültig darzustellen.
- Die Speicherung liegt in diesem ersten Schritt bewusst auf SD und kann später hinter derselben gekapselten Schnittstelle in internen Flash verlegt werden. Ein gezielter Rollback einer zuvor vollständig kopierten SD-Karte ist damit noch nicht hardwareseitig ausgeschlossen.
- Kalibrierpakete, kanonische Signaturbytes, TP3C1/TP3Q3, TPSIG, TPLOG, externe PDFs, ADC, Regelung und Safety bleiben unverändert.

# V0.50.1_43 – große dynamische Web-Seiten vollständig beim ersten Aufruf

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_18.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_43`.
- Ursache der beim ersten Aufruf abgeschnittenen Seiten: `/identity`, `/validity` und `/calibration` wurden als bis zu 16-KiB-große dynamische HTML-Antwort noch vollständig innerhalb des 120-ms-Sofortantwortbudgets geschrieben. Bei vollem TCP-Sendepuffer endete die Antwort mitten im HTML, beispielsweise nach der Überschrift `Jus`; beim zweiten Aufruf war die Übertragung wegen Browser-/TCP-Zustand häufig schneller.
- Diese drei dynamisch erzeugten Verwaltungsseiten verwenden nun denselben nicht blockierenden 512-Byte-Seitentransfer wie Haupt-, Setup-, Download- und Kalibrierscheinseiten. Pro Hauptloop wird höchstens ein Seitenblock gesendet; Messung, Regelung und Safety werden nicht durch eine wartende TCP-Schleife blockiert.
- Dynamische Seiten besitzen bewusst keinen ETag und werden mit `no-store` ausgeliefert, damit ein geänderter Zertifikats-/Kalibrierzustand niemals durch eine 304-Antwort verdeckt wird. Statische Seiten behalten ihre bisherige ETag-Unterstützung.
- JSON-, Upload- und kleine Ergebnisantworten behalten das kurze 120-ms-Budget. Zertifikate, Signaturen, TP3C1/TP3Q3, TPSIG, TPLOG, PDF-Bindung, Kalibrierarchiv, ADC, Regelung und Safety bleiben unverändert.

# V0.50.1_42 – Kalibrierungsupload: „Failed to fetch“ nach erfolgreicher Prüfung behoben

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_17.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_42`.
- Ursache: Das 120-ms-HTTP-Schreibbudget lief während Signaturprüfung, SD-Archivierung, transaktionaler Aktivierung und der neuen kopfgebundenen Systemauswahl weiter. Nach erfolgreichem Import war das Budget beim ersten Antwortbyte bereits abgelaufen; der Browser meldete `TypeError: Failed to fetch`.
- Das Antwortbudget wird nun nach dem vollständigen Geräte-, Kopf- oder Systemkalibrierungsimport neu gestartet. Dies gilt sowohl bei Erfolg als auch bei einer fachlichen Ablehnung, damit die konkrete Prüffehlernachricht den Browser erreicht.
- Auch der Abschluss eines externen PDF-Uploads sowie der Import von Geräte-/Kopfjustierungen direkt von SD starten das Schreibbudget erst nach allen Datei- und Kryptoprüfungen.
- Die importierten Daten, Signaturen, Archive, Kopfbindung, TP3C1/TP3Q3, TPSIG, TPLOG und Messfunktionen bleiben unverändert.

# V0.50.1_41 – Kopfgebundene Auswahl gültiger Kalibrierzertifikate

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_16.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_41`.
- Maßgeblich ist immer die im Setup gewählte Kombination aus Kopftyp und Kopf-Seriennummer. Aktuelle Zertifikatsanzeige und Zeitraumssuche zeigen ausschließlich Systemkalibrierungen für genau diesen Kopf.
- Die Anzeige nennt den verwendeten Kopf eindeutig, zum Beispiel `TP-3000 / STP-3002 K12345`. Bei exakt gebundenem externen Original-PDF steht neben `Anzeigen` weiterhin der Knopf `PDF`.
- Nach Kopfwechsel oder Änderung der Kopfparameter wird der Kalibrierungs-Cache verworfen. Aus dem append-only SD-Archiv wird automatisch die neueste aktuell gültige, kryptografisch gültige und zu den aktuellen Geräte-/Kopfjustierungsmanifesten passende Systemkalibrierung gewählt.
- Auswahlreihenfolge bei mehreren passenden Paketen: neuestes `ValidFrom`, danach neuestes Kalibrierdatum, danach Manifest-SHA-256 als deterministischer Gleichstandbrecher.
- Die gebundene Kopfjustierung wird aus dem Archiv geladen, erneut signaturgeprüft und muss sowohl zu Kopftyp/Kopf-SN als auch zu den aktuell eingestellten Kopfwerten passen. Gerätejustierung und Systemkalibrierung müssen ebenfalls exakt gebunden und zeitlich gültig sein.
- Kopf- und Systempaket werden gemeinsam transaktional aktiviert. Alte wirksame Dateien bleiben bis zur abschließenden Rücklese- und Bindungsprüfung als Backup erhalten.
- Ein gebundenes externes PDF wird über Dokument-ID, Metadaten, `%PDF-`, Dateigröße und SHA-256 geprüft. Ein beschädigtes neueres PDF verdrängt kein älteres vollständig gültiges Zertifikat.
- Gibt es kein passendes Zertifikat, erscheint eine eindeutige Meldung; zertifizierte CSV-/TPLOG-Modi bleiben gesperrt. Es erfolgt niemals ein Rückfall auf ein Zertifikat eines anderen Kopfes.
- Nicht zeitkritische Auswahl-, Archiv- und Prüfpfade liegen in `FLASHMEM`. Rund 3.056 Byte neue Arbeits- und Cache-Daten liegen in RAM2; neue konstante Texte liegen im Program-Flash. Der knappe RAM1-Stack wird durch wiederverwendete RAM2-Arbeitsstrukturen entlastet.
- Zertifikatsformate, kanonische Signaturbytes, Feldreihenfolgen, Skalierungen, TP3C1/TP3Q3, TPSIG, TPLOG und die vier SD-Logmodi bleiben unverändert.

# V0.50.1_40 – RAM1-Optimierung zertifizierter Logs und Kalibrierarchive

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_15.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_40`.
- Ursache der RAM1-Abnahme in Build `0.50.1_39`: Die ITCM-Codebelegung blieb in demselben 288-KiB-Block; der Verlust entstand nahezu vollständig durch zusätzliche globale/konstante Daten.
- Nicht zeitkritische Pfade, Dateiendungen, JSON-Vorlagen, Alphabete und Fehlermeldungen des zertifizierten Logmoduls wurden in den Program-Flash verschoben.
- Neu hinzugekommene Archiv-/Historientexte der signierten Kalibrierung wurden ebenfalls in den Program-Flash verschoben.
- Status- und Wertestrukturen für Geräte-, Kopf- und Systemkalibrierung (996 Byte) liegen nun in RAM2 und werden in `tpSignedCalibrationBegin()` explizit genullt.
- Host-Objektvergleich: ca. 3.889 Byte Log-Rodata plus 1.266 Byte Kalibrierarchiv-Rodata aus RAM1 entfernt; zusätzlich 996 Byte Zustandsdaten nach RAM2 verlagert. Erwarteter RAM1-Gewinn insgesamt ca. 6.151 Byte.
- Zertifikate, Signaturen, kanonische Bytes, TP3C1/TP3Q3, TPSIG, TPLOG, PDF-Zuordnung, Kalibrierarchiv und alle vier SD-Logmodi bleiben funktional und formatseitig unverändert.

# V0.50.1_39 – Historische Kalibrierzertifikate: „Failed to fetch“ behoben

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_14.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_39`.
- Ursache: Das 120-ms-Netzwerk-Antwortbudget lief bereits während SD-Lesen und kryptografischer Prüfung historischer Pakete. Beim anschließenden ersten Antwortbyte war das Budget teilweise schon überschritten; der Socket wurde beendet und der Browser meldete `Failed to fetch`.
- Das Antwort-Schreibbudget wird bei langsamen historischen SD-Operationen erst nach abgeschlossener Archivprüfung neu gestartet. Dies gilt für Zeitraumssuche, historische Systemkalibrierung, historische Geräte-/Kopfjustierung sowie historische externe PDF-Metadaten und PDF-Download.
- Historische Geräte- und Kopfjustierung werden im Browser nacheinander statt parallel geladen.
- Aktuelle Zertifikatsansicht, Signaturen, TP3C1/TP3Q3, PDF-Bindung, SD-Logmodi, Messung, Regelung und Safety bleiben unverändert.

# V0.50.1_38 – Kalibrierzertifikat-Archiv und Zeitraumssuche

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_13.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_38`.
- Vor der eigentlichen Kalibrierzertifikatsansicht liegt nun eine Auswahlseite unter `/calibration-certificate`.
- Die Auswahlseite zeigt das aktuelle Systemkalibrierzertifikat und bietet eine Zeitraumssuche mit `Zeitraum von` und `Zeitraum bis`.
- Trefferregel: `ValidFrom <= SearchEnd && ValidUntil >= SearchStart`; die Grenzen werden einschließlich behandelt.
- Jeder Treffer besitzt einen eigenen Knopf `Anzeigen`. Existiert das gebundene externe Original-PDF auf SD, erscheint direkt dahinter zusätzlich `PDF`.
- `Anzeigen` öffnet die vollständige TP-3000-Ansicht mit Gerätejustierung, Kopfjustierung, Systemkalibrierung und den unveränderten TP3C1-/TP3Q3-Prüfdaten.
- `PDF` öffnet genau das durch Dokument-ID und SHA-256 gebundene externe Original-PDF im Browser.
- Signierte Geräte-, Kopf- und Systemkalibrierungspakete werden ab diesem Stand append-only nach ihrem Manifest-SHA-256 unter `/CALIBRATION/ARCHIVE` archiviert.
- Historische Ansichten lesen und verifizieren die drei damals gebundenen Pakete erneut; Zertifikate, Signaturen, Feldreihenfolgen, Skalierungen und QR-Formate wurden nicht verändert.

# V0.50.1_37 – vier getrennte SD-Logmodi

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_12.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_37`.
- Das Menü `SD-Karte → Log-Integrität` besitzt jetzt vier eindeutig getrennte Betriebsarten: `Aus`, `SHA-256`, `CSV zertifiziert` und `TPLOG zertifiziert`.
- `Aus` behält die bisherigen Tages-CSV-Dateien ohne Integritätsnachweis.
- `SHA-256` erzeugt eine nummerierte `.CSV` plus GNU-kompatible `.CSV.sha256`.
- `CSV zertifiziert` erzeugt eine nummerierte `.CSV` plus gleichnamige `.TPSIG`; es wird keine `.TPLOG` erzeugt.
- `TPLOG zertifiziert` verwendet CSV und TPSIG nur als geprüfte Zwischenstufen und lässt nach erfolgreichem Containerabschluss ausschließlich die gleichnamige `.TPLOG` zurück.
- Beide zertifizierten Modi verwenden unverändert `TP3000-LOG-SIGNATURE-2`, dieselbe ECDSA-P-256-Gerätesignatur und dieselben gebundenen Zertifikats-, Kalibrierungs- und Konfigurationsdaten.
- Beide zertifizierten Modi sind gemeinsam gesperrt, solange Gerätezertifikat, Gerätejustierung, Kopfjustierung oder Systemkalibrierung fehlen beziehungsweise ungültig sind.
- Web-Setup und TFT-Menü zeigen dieselben vier Optionen. Ein Snapshot im TPLOG-Modus liefert nach dem Versiegeln direkt die fertige `.TPLOG`.
- Sequenznummern werden modusübergreifend nicht wiederverwendet; eine vorhandene TPLOG reserviert ihren Dateistamm auch nach einem späteren Wechsel auf CSV- oder SHA-256-Ausgabe.
- TP3C1/TP3Q3, externe PDF-Kalibrierscheine, Zertifikate, Signaturen, ADC, Regelung und Safety bleiben unverändert.

# V0.50.1_36 – zusätzlicher TPLOG-Ein-Datei-Container

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-16_V0.50.1_11.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_36`.
- Der zertifizierte Logmodus behält die zwei bisherigen Originaldateien unverändert bei: nummerierte `.CSV` plus gleichnamige `.TPSIG` im Format `TP3000-LOG-SIGNATURE-2`.
- Zusätzlich wird nach erfolgreicher TPSIG-Prüfung eine gleichnamige `.TPLOG` im neuen Format `TP3000-LOG-CONTAINER-1` erzeugt. Sie enthält bytegenau dieselbe CSV und dieselbe TPSIG in einer einzigen transportierbaren Datei.
- TPLOG besitzt einen festen binären Header und Footer mit Little-Endian-Offsets, Längen, SHA-256 beider eingebetteter Bereiche und CRC-32 der Strukturfelder. Die kryptografische Authentizität stammt weiterhin aus der eingebetteten ECDSA-signierten TPSIG.
- CSV, TPSIG und TPLOG werden erst nach vollständigem Schreiben und erneuter Struktur-/Hashprüfung als abgeschlossen behandelt. Bei Stromausfall-Recovery wird das Kennzeichen sowohl in TPSIG als auch im TPLOG-Header gesetzt.
- `/download` akzeptiert und listet `.TPLOG` unter `Nachweise`; MIME-Typ ist `application/vnd.tp3000.tplog`.
- Das Hilfsprogramm `tools/tplog_extract.py` prüft Header/Footer, CRC-32, Abschnittsgrenzen und SHA-256 und extrahiert CSV/TPSIG bytegenau. Es führt bewusst noch keine ECDSA-Zertifikatsprüfung aus.
- Der neue Containercode liegt in `FLASHMEM`; ein 1024-Byte-I/O-Puffer liegt in RAM2/`DMAMEM`. Der zusätzliche 64-MiB-Flash wird weiterhin nicht verwendet.

# V0.50.1_35 – zertifizierter SD-Logmodus und TP3000-LOG-SIGNATURE-2

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-15_V0.50.1_10.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_35`.
- Die vorbereitete Auswahl `Log-Integrität: Zertifiziert` ist aktiv, sobald Gerätezertifikat, signierte Gerätejustierung, signierte Kopfjustierung und gebundene Systemkalibrierung gültig sind.
- SHA-256- und zertifizierter Modus schreiben nummerierte, unveränderliche Abschnitte wie `YYMMDD_001.CSV`; Diagnoseabschnitte verwenden `YYMMDDD_001.CSV`.
- Der SHA-256-Modus erzeugt eine GNU-kompatible Begleitdatei `<CSV>.sha256`.
- Der zertifizierte Modus erzeugt `TP3000-LOG-SIGNATURE-2` als `.TPSIG` mit CSV-Dateiname, Dateigröße, SHA-256, Zeitbereich, Geräte-SN/Key-ID, Geräte-/Kopf-/Systemkalibrierung, Gerätezertifikat, Firmware-Entwicklungsidentität und messrelevanter Konfiguration.
- Die Gerätesignatur ist ECDSA P-256 / SHA-256 mit 64 Byte `R || S` im IEEE-P1363-Format.
- Ein CRC-32-gesichertes Journal `/LOG/TPLOG.HST` wird erst nach erfolgreichem CSV-Flush fortgeschrieben. Nach unsauberem Neustart bleiben vollständige LF-terminierte Zeilen erhalten; ein unvollständiges Dateiende wird abgeschnitten und die `.TPSIG` mit `RecoveredAfterUncleanShutdown=true` gekennzeichnet.
- Der beim Abschnittsstart eingefrorene Kontext wird vor dem Signieren erneut per SHA-256 geprüft. Fehlender oder veränderter Kontext verhindert die `.TPSIG`-Erzeugung.
- Änderungen an Kalendertag/Zeitbasis, Integritätsmodus, CSV-Format, Zertifikat, Justierungen, Systemkalibrierung, externer PDF-Bindung, Firmwareidentität oder Messkonfiguration schließen den aktiven Abschnitt.
- Ein Web-Snapshot im SHA-256- oder zertifizierten Modus schreibt den aktuellen RAM-Puffer, versiegelt den Abschnitt und lädt danach die unveränderliche CSV. `/download` besitzt den zusätzlichen Filter `Nachweise` für `.TPSIG` und `.sha256`.
- Ein Hersteller-Firmwaremanifest bleibt im Entwicklungsstand informativ und nicht blockierend; die `.TPSIG` kennzeichnet die Firmware weiterhin sichtbar als `DEVELOPMENT`.
- Zustands-, Kontext- und Journalpuffer liegen in RAM2; nicht zeitkritische Log-, Hash-, Recovery- und Webfunktionen liegen in `FLASHMEM`. Der zusätzliche 64-MiB-Flash wird nicht verwendet.

# V0.50.1_34 – Externer PDF-Ablauf in FLASHMEM/PROGMEM und RAM2

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-15_V0.50.1_9.zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_34`.
- Sämtliche nicht zeitkritischen Funktionen des neuen externen PDF-SD-Ablaufs liegen in `FLASHMEM` und belegen damit keinen RAM1-Codebereich mehr.
- Pfade, JSON-/HTML-Formate und Status-/Fehlertexte des externen PDF-Ablaufs liegen in `PROGMEM` statt im RAM1-Datenbereich.
- PDF-Metadaten, Uploadzustand, Hash-/JSON-Arbeitspuffer, Web-Ausgabepuffer und Upload-Fehler-/Ergebnisfelder liegen als reine POD-Daten in `DMAMEM`/RAM2.
- `File`- und `TpSha256`-Objekte bleiben wegen ihrer Konstruktoren bewusst in RAM1; DMAMEM-Daten werden beim Start explizit initialisiert.
- Der aktive Systemkalibrierungsstatus und die intern gehaltenen Systemkalibrierungswerte wurden ebenfalls nach RAM2 verschoben und werden vor Verwendung explizit gelöscht.
- Große temporäre Webpuffer des externen Kalibrierscheins sind gemeinsam in RAM2 abgelegt; die Webverarbeitung ist bereits durch den zentralen Ausgabepuffer seriell.
- PDF-Dateiformat, SD-Archiv, Metadatenformat, SHA-256, Systemkalibrierungsbindung, TP3C1/TP3Q3, Zertifikate, Signaturen, ADC, Regelung und Safety bleiben unverändert.

# V0.50.1_33 – Externe PDF-Kalibrierscheine auf SD und Systembindung

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_8(1).zip`; interne Firmwareversion bleibt `0.50.1`, Build-ID ist `0.50.1_33`.
- Unter `/calibration` können externe Kalibrierscheine als unveränderte PDF-Datei bis maximal 6 MiB hochgeladen werden. Der Browser überträgt die Datei in 16.000-Byte-Blöcken; die Firmware schreibt direkt in eine temporäre SD-Datei und hält das PDF nicht vollständig im RAM.
- Pflichtmetadaten sind `Kalibrierschein-Nr. / Zeichen` (1–32 Zeichen), `Gültig von`, `Gültig bis` und der Originaldateiname.
- Vor der Aktivierung werden `%PDF-`, erwartete Dateigröße und der während des Uploads berechnete SHA-256 geprüft. Nach Schließen und erneutem Öffnen wird die komplette SD-Datei ein zweites Mal gehasht.
- Original-PDF und dokumentbezogene JSON-Metadaten werden append-only unter `/CALIBRATION/EXTERNAL` archiviert. `ACTIVE.json` wird mit temporärer Datei, Backup und Rückleseprüfung transaktional gewechselt.
- Aktiver externer Kalibrierschein kann unter `/calibration` angezeigt und heruntergeladen werden; `/validity` zeigt Nummer, Gültigkeit, Originaldatei, Dokument-ID und SHA-256.
- Systemkalibrierungsanfrage und -paket besitzen ab V2 eine kryptografische Bindung an Dokument-ID, Nummer, Gültigkeitsdaten, Originaldateiname, Dateigröße und PDF-SHA-256. V1-Systemkalibrierungen bleiben prüfbar.
- Eine Systemkalibrierung kann als Quelle `TP3000` oder `EXTERNAL_PDF` signieren. Bei `EXTERNAL_PDF` müssen die sechs kalibriertechnischen Fachangaben leer sein; auf dem Kalibrierschein erscheinen stattdessen die externen PDF-Daten.
- Der Bearbeiter ist für neue V2-Systemkalibrierungen Pflicht. Alte V1-Pakete bleiben rückwärtskompatibel.
- TP3C1 kennzeichnet die Kalibrierscheinquelle in Approval-Feld 16 und transportiert bei V2 die externen PDF-Metadaten im Systemwerteblock. Die bisherigen fünf Signaturen und ihre Reihenfolgen bleiben unverändert.
- Druckbezeichnungen auf `TP3C1-Offline-Prüfcode – Teil 1/2` bzw. `Teil 2/2` korrigiert; die technische Beschriftung bleibt durch kompaktere Schreibweise in einer Zeile.
- Kein externer SPI-/QSPI-Flash wird angesprochen. Der gesamte neue PDF-Ablauf ist ausschließlich SD-basiert.

# V0.50.1_32 – System-Fachangaben im Schein und 115-mm-TP3C1-Druck

- Interne Build-ID auf `0.50.1_32` erhöht; interne Firmwareversion bleibt `0.50.1`.
- Die sechs optionalen Fachangaben werden ausschließlich aus der aktiven Systemkalibrierung gelesen und vollständig auf dem Kalibrierschein ausgegeben.
- `Verwendete Referenznormale`, `Rückführbarkeit`, `Messunsicherheit` und `Umgebungsbedingungen` stehen in einem zweispaltigen Raster.
- `Kalibrierverfahren` und `Akkreditierungsangaben` nutzen die volle Seitenbreite. Lange Texte erhalten automatische Zeilen- und Seitenumbrüche; feste Höhen oder Abschneiden werden nicht verwendet.
- Der TP3C1-Encoder und seine Binärdaten wurden nicht geändert. Die Änderung betrifft ausschließlich die Druckdarstellung.
- Beide TP3C1-QR-Codes werden im A4-Druck mit `115 mm × 115 mm` ausgegeben. Überschriften, Zwischenraum und Fußtext wurden kompakter gesetzt, sodass beide Codes weiterhin gemeinsam auf eine A4-Seite passen.
- ADC, Regelung, Safety, Messwerterfassung, Zertifikate, Signaturen und Kalibrierwerte bleiben unverändert.

# V0.50.1_31 – TP3C1 verwendet das vorhandene aktive Gerätezertifikat

- Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_8.zip` / Build `0.50.1_30`.
- Interne Build-ID auf `0.50.1_31` erhöht; interne Firmwareversion bleibt `0.50.1`.
- Keine Zertifikate, Schlüssel, Signaturen oder Kalibrierpakete geändert.
- Neuer Nur-Lese-Endpunkt `/identity-active-certificate.json` liefert das bereits verifizierte aktive Gerätezertifikat aus dem Identitätsspeicher.
- Der TP3C1-Encoder verlangt kein eingebettetes `DeviceCertificate` mehr in `.tpdcal`, `.tphcal` oder `.tpscal`.
- Das aktive Gerätezertifikat wird gegen Geräte-SN und `DeviceCertificateSerial` aller drei Kalibrierpakete geprüft.
- Das vorhandene Labor-Rollenzertifikat wird unverändert aus Geräte- oder Kopfjustierung übernommen; zwei vorhandene Kopien müssen dasselbe Manifest besitzen.
- G00001/K30001-Test mit realistischem Datenlayout ohne eingebettetes Gerätezertifikat bestanden; Envelope, ZLIB und beide Binärteile stimmen weiterhin bytegenau mit TP3C1 V0.2 überein.
- TP3Q3, `.tpcert`, `.tpdcal`, `.tphcal`, `.tpscal`, ADC, Regelung, Safety und Messwerterfassung bleiben unverändert.

# V0.50.1_30 – TP3C1 V0.2 für den gedruckten Kalibrierschein

- Exakte Arbeitsbasis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_7.zip`.
- Interne Build-ID von `0.50.1_29` auf `0.50.1_30` erhöht.
- Bytegenauer TP3C1-V0.2-Encoder gemäß Referenzimplementierung ergänzt.
- Kein JSON/CBOR im kompakten Format: TLV, VarUInt, ZigZag und feste Schemafelder.
- Gerätezertifikat, Laborzertifikat sowie die drei signierten Kalibrierpakete bleiben mit ihren fünf vorhandenen Signaturen rekonstruierbar.
- Gemeinsame ZLIB-Kompression mit pako 1.0.11 auf Level 9; Ausgabe ist zum Python-zlib-Referenzstrom binär identisch.
- Danach feste Aufteilung in zwei Fragmente mit je 18-Byte-Transportkopf, Dokumentkennung, Teilindex, Längen und CRC-32.
- URI-sicheres Base38 ohne Prozentkodierung; Deep Link `tp3000://verify#TP3C1:...`.
- Gedruckter Kalibrierschein verwendet immer zwei QR-Codes, Fehlerkorrektur M, Version 20 bei den Referenzdaten und 98 mm Druckbreite.
- QR-Teile sind in beliebiger Reihenfolge scanbar.
- TP3Q3 bleibt als vollständiger elektronischer `.tpqr`-Export erhalten; TP3Q1/2/3 und alle bisherigen Dokumentdateien bleiben unberührt.
- G00001/K30001-Testvektoren für Envelope, ZLIB, beide Binärteile, Texte und QR-Größe bytegenau bestanden.

# V0.50.1_29 – URI-gültiger Deep Link und 20 % größere QR-Codes

- Interne Build-ID auf `0.50.1_29` angehoben.
- Der bestehende `TP3Q3:`-Rohpayload bleibt vollständig unverändert. Ausschließlich seine Darstellung im URI-Fragment wird mit `encodeURIComponent()` transportkodiert.
- Der QR-Inhalt lautet damit `tp3000://verify#` plus URI-kodierter Fragmentdarstellung. Android `Uri.getFragment()` liefert wieder den ursprünglichen `TP3Q3:`-Payload für den bestehenden Prüfbaum.
- Problematische Base45-Zeichen wie `%`, Leerzeichen, `+`, `/` und `:` können den Deep Link nicht mehr syntaktisch ungültig machen.
- Präfix und kodierter Fragmentteil bleiben getrennte QR-Segmente; der lange Fragmentteil verwendet weiterhin den kompakten QR-Alphanumerikmodus.
- `.tpqr`-Downloads bleiben unveränderte Rohpayloads ohne Deep-Link-Präfix und ohne URI-Kodierung.
- QR-Darstellung von 310 px auf 372 px beziehungsweise im A4-Druck auf 98 mm vergrößert.
- Bei zwei QR-Teilen werden beide 98-mm-Codes untereinander auf einer eigenen A4-Seite angeordnet. Damit passen beide ohne Verkleinerung auf ein Blatt.
- QR-Version und Kapazität werden automatisch aus dem längeren URI-gültigen Inhalt neu bestimmt; Fehlerkorrekturstufe L und die maximale Zweiteilung bleiben unverändert.

# V0.50.1_28 – Android-Deep-Link für Offline-QR-Prüfung

- Interne Build-ID auf `0.50.1_28` angehoben.
- Die QR-Grafik des Kalibrierscheins enthält vor jedem unveränderten `TP3Q3:`-Teil den festen Deep-Link-Präfix `tp3000://verify#`.
- Signatur, Dokumenthash, DEFLATE-Komprimierung, Base45 und der bestehende TP3Q-Payload bleiben unverändert.
- Präfix und Nutzinhalt werden intern als getrennte QR-Segmente kodiert. Dadurch bleibt der umfangreiche Base45-Nutzinhalt im platzsparenden QR-Alphanumerikmodus; der dekodierte Gesamttext ist dennoch exakt `tp3000://verify#TP3Q3:...`.
- Die automatische Ein-/Zwei-Teil-Auswahl berechnet die QR-Version mit dem zusätzlichen Präfix neu. Fehlerkorrekturstufe L und die bestehende maximale Aufteilung auf zwei Codes bleiben erhalten.
- Der Download als `.tpqr` enthält weiterhin den unveränderten Roh-Payload ohne Deep-Link-Präfix und erhält damit die bisherige Rohdatenunterstützung.

# V0.50.1_27 – direkte Freigabe-Uploads und vollständiger Systemkalibrierschein

- Interne Build-ID auf `0.50.1_27` angehoben.
- Signierte Gerätejustierungen (`.tpdcal`) und Kopfjustierungen (`.tphcal`) können nun wie die Systemkalibrierung direkt über die Weboberfläche hochgeladen werden. Übertragen wird jeweils das vollständige signierte JSON-Paket; private Schlüssel bleiben auf dem PC.
- Aktivierung von Geräte- und Kopfjustierung erfolgt transaktional mit temporärer Datei und Backup-Rollback. Bestehende Systemkalibrierungen bleiben gespeichert und werden bei abweichender Justierungsbindung automatisch inaktiv.
- Der Kalibrierschein verwendet die aktive Systemkalibrierung als Hauptdokument (`SCAL-...`), zeigt deren Geräte-/Kopfbindung, Messpunkte, Messunsicherheiten und Fachangaben und führt Geräte- sowie Kopfjustierung als Grundlagen auf.
- Neuer Offline-QR-Transport `TP3Q3` bindet Gerätejustierung, Kopfjustierung und Systemkalibrierung gemeinsam über SHA-256. Automatische Aufteilung bleibt auf höchstens zwei zusammengehörige QR-Codes begrenzt.
- Druck-CSS trennt Bildschirm- und Druck-Media-Queries. A4 wird bei Standardskalierung dauerhaft zweispaltig, mittig und ohne manuelle 88-%-Korrektur ausgegeben.

# V0.50.1_26 – eindeutige Kalibrierumfänge

- Interne Build-ID auf `0.50.1_26` angehoben.
- Systemkalibrierung unterstützt nun eindeutig `Nur As Found`, `Nur As Left`, `As Found / As Left – ohne Justierung` und `Vor und nach Justierung`.
- `As Found / As Left – ohne Justierung` verwendet eine gemeinsame Messreihe; zwei Tabellen werden nur bei `Vor und nach Justierung` verwendet.
- Neue signierte Maschinenwerte `AS_FOUND_AS_LEFT_NO_ADJUSTMENT` und `BEFORE_AFTER_ADJUSTMENT`.
- Der bisherige Wert `AS_FOUND_AS_LEFT` bleibt als Altformat für getrennte Vor-/Nach-Messreihen gültig.
- Relative Feuchte wird im Kalibrierschein mit `% rH` dargestellt.
- Web-Gültigkeitsseite zeigt den neuen Kalibrierumfang eindeutig an.

# V0.50.1_25 – gemeinsame Systemkalibrierung

- Interne Build-ID auf `0.50.1_25` angehoben.
- Geräte- und Kopfwerte bleiben getrennt als `Gerätejustierung` und `Kopfjustierung`; die echte Kalibrierung wird als gemeinsamer Nachweis `Systemkalibrierung` geführt.
- Neue gerätesignierte 72-h-Anfrage `.tpscalreq`, gebunden an Geräte-SN, Kopftyp, Kopf-SN sowie Calibration-ID und SHA-256-Manifest der aktuell aktiven Geräte- und Kopfjustierung.
- Neues signiertes Paket `.tpscal` mit As-Found-/As-Left-Kalibriermesswerten und eindeutiger Geräte-/Kopfbindung.
- Web `Info / Gültigkeit` zeigt einen eigenen Bereich `Systemkalibrierung` mit Status, Kalibrierumfang, Kalibrierdatum, Gültigkeit, Intervall, Zertifikatsaussteller, Bearbeiter, Calibration-ID und den gebundenen Justierungen.
- Web `Justierung / Systemkalibrierung verwalten` exportiert die `.tpscalreq` und lädt eine vom KeyGen erzeugte `.tpscal` direkt aus dem Browser hoch.
- Der Upload wird vor der Aktivierung vollständig kryptografisch geprüft; unpassende Geräte-/Kopfkombinationen oder abweichende Justierungsmanifeste werden abgelehnt.
- Aktive Systemkalibrierung wird auf SD unter `/CALIBRATION/ACTIVE_SYSTEM.tpscal` gespeichert. Der Austausch erfolgt mit temporärer Datei und Backup-Rollback.
- Der Nachweisstatus für den späteren zertifizierten Logmodus verlangt zusätzlich die aktive, an beide Justierungen gebundene Systemkalibrierung.
- Vorerst wird genau eine aktive Systemkalibrierung verwaltet. Mehrere Zertifikate und Auswahl einer anderen aktiven Kombination bleiben ein späterer Ausbau.

# V0.50.1_24 – stabile Datumsbindung und Justierungsstatus

- Interne Build-ID auf `0.50.1_24` angehoben.
- Ein fehlendes Referenz-/Justierungsdatum wird nicht mehr durch das aktuelle Tagesdatum ersetzt. Dadurch kann eine unveränderte signierte Gerätejustierung beim Datumswechsel nicht mehr fälschlich rot werden.
- Der Aktivstatus der Gerätejustierung vergleicht nur die tatsächlich wirksamen Referenz- und Kanalkorrekturwerte; Datumsfelder bleiben signierte Dokumentmetadaten.
- Neue Gerätejustierungsanfragen werden bei fehlendem persistenten Referenz-/Justierungsdatum mit einer eindeutigen Meldung abgewiesen.

# V0.50.1_23 – As Found / As Left, Justierungs-Wording und TP3Q2

- Interne Build-ID auf `0.50.1_23` angehoben.
- Geräte- und Kopfkorrekturwerte heißen in Weboberfläche und Kalibrierschein einheitlich `Gerätejustierung` und `Kopfjustierung`.
- Der Kalibrierschein weist `Zertifikatsaussteller` und `Bearbeiter` getrennt aus.
- Neue Kalibrierpaketformate V3 übernehmen echte Kalibriermesswerte zusätzlich zur Justierung; V1 und V2 bleiben prüfbar.
- Unterstützte Kalibrierumfänge: `AS_FOUND`, `AS_LEFT` und `AS_FOUND_AS_LEFT`.
- Der Web-Kalibrierschein zeigt Messgröße, optionalen Sollwert, Referenzwert, Anzeige TP-3000, automatisch berechnete Abweichung, erweiterte Messunsicherheit `U`, Erweiterungsfaktor `k`, Vertrauensniveau und Einheit.
- Überschriften: `Kalibrierung vor Justierung (As Found)` und `Kalibrierung nach Justierung (As Left)`.
- QR-Transport auf `TP3Q2` erweitert. Ein gemeinsamer Dokumenthash bindet Geräte- und Kopfjustierung; bei Bedarf entstehen automatisch zwei zusammengehörige QR-Codes mit Teilnummer und Gesamtanzahl.
- Die Druckfreigabe erfolgt weiterhin erst nach vollständigem QR-Aufbau.

# V0.50.1_22 – QR-Erzeugung ohne CompressionStream-Blockade

- Interne Build-ID auf `0.50.1_22` angehoben.
- Ursache für dauerhaft angezeigtes `QR-Code wird erzeugt …` behoben: Der Browsercode schrieb den vollständigen Datensatz zuerst in `CompressionStream` und begann erst nach `writer.close()` mit dem Lesen. Bei größeren, schlecht komprimierbaren signierten Paketen konnte der Stream durch Backpressure dauerhaft blockieren.
- DEFLATE wird nun als gleichzeitig gelesene Pipeline `Blob.stream().pipeThrough(new CompressionStream('deflate'))` ausgeführt.
- `Drucken / PDF` bleibt bis zum vollständig aufgebauten QR-Code deaktiviert.
- Fortschritt wird mit `QR-Daten werden komprimiert …` und `QR-Code wird aufgebaut …` angezeigt.
- QR-Fehler ersetzen nicht mehr den gesamten Kalibrierschein, sondern werden direkt im Abschnitt `Offline-Prüfung` mit Ursache angezeigt.
- Falls das gemeinsame Geräte-/Kopfpaket nicht in einen QR-Code passt, bleibt die vorhandene Aufteilung in zwei getrennte Offline-Codes aktiv.

# V0.50.1_21 – Web-Zeitsynchronisation nach Firmwareupdate

- Interne Build-ID auf `0.50.1_21` angehoben.
- Ursache für `Ungültige Zeitparameter` nach dem Update behoben: Die Web-Hauptseite verwendete noch denselben ETag wie der vorherige Build, wodurch Browser die alte JavaScript-Funktion ohne UTC-Offset per HTTP 304 weiterverwenden konnten.
- Alle HTML-ETags enthalten nun automatisch die Firmware-Build-ID. Dadurch wird eingebettetes JavaScript nach jedem neuen Build sicher neu geladen.
- Fehlt der UTC-Offset trotzdem, meldet `/set-time` nun eindeutig `Veraltete Webseite: mit Strg+F5 neu laden` statt eines allgemeinen Parameterfehlers.
- RTC und Anzeige bleiben in lokaler Zeit; kryptografische Zeitstempel werden weiterhin mit dem übertragenen UTC-Offset nach UTC umgerechnet.

# V0.50.1_20 – UTC-Zeitbasis für Kalibrieranfragen

Basis: `Taupunktspiegel_StandV1_2026-07-12_V0.50.1_6.zip`

- Interne Build-ID auf `0.50.1_20` angehoben.
- Geräte-RTC und Anzeige bleiben in lokaler Zeit; der Browser überträgt beim PC-Zeitsync zusätzlich den aktuellen UTC-Offset.
- Der UTC-Offset wird im vorhandenen Device-/UI-A/B-EEPROM-Block gespeichert, ohne das Blockformat zu vergrößern.
- `CreatedUtc` von Geräte- und Kopfkalibrieranfragen wird nun korrekt aus lokaler RTC-Zeit minus UTC-Offset erzeugt.
- Gültigkeitsanzeigen für Geräte- und Kopfkalibrierung vergleichen nun ebenfalls mit der korrigierten UTC-Zeit.
- Ohne gültigen UTC-Offset wird keine neue Kalibrieranfrage erzeugt; der Anwender erhält den Hinweis, zuerst die PC-Zeit zu übertragen.
- Neue Web-Schaltfläche `Kalibrierschein` zwischen PC-Zeitübertragung und `Info`.
- Neue druckoptimierte A4-Webseite `/calibration-certificate` mit Geräte- und Kopfkalibrierung, Gültigkeiten, Signierer, Kalibrierwerten und optionalen Fachangaben.
- Browserdruck über `window.print()`; dadurch Papierdruck oder systemeigener PDF-Druck ohne PDF-Bibliothek im Teensy.
- Offline-QR-Transport `TP3Q1`: ZLIB-komprimierte JSON-Hülle, Base45 und lokal eingebetteter SVG-QR-Encoder ohne Internetzugriff.
- Bei zu großem gemeinsamen QR werden Geräte- und Kopfkalibrierung getrennt dargestellt.
- Neue JSON-Endpunkte für die unveränderten aktiven signierten Kalibrierdateien.
- Kalibrierformat V2 akzeptiert direkte Hersteller-Root-Signaturen oder einen vom Root ausgestellten `CALIBRATION_LAB`-Schlüssel mit `SIGN_CALIBRATION`.
- Labor-ID, Laborname, Anschrift, Berechtigung und Gültigkeit sind im Laborzertifikat root-signiert.
- Kalibrieranfragen sind exakt 72 Stunden gültig; Anfrage-ID und Zeitfenster werden in der Freigabe mitsigniert und beim Import geprüft.
- Optionale mitsignierte Felder: Referenznormale, Rückführbarkeit, Messunsicherheit, Umgebungsbedingungen, Kalibrierverfahren und Akkreditierungsangaben.
- V1-Labor- und Kalibrierdateien bleiben lesbar; neue Freigaben verwenden V2.
- JSON-Parser für Top-Level-Manifest und Signaturalgorithmus gegen gleichnamige Felder in eingebetteten Laborzertifikaten gehärtet.
- QRCode-for-JavaScript-Herkunft und MIT-Lizenz in `THIRD_PARTY_NOTICES.md` ergänzt.

# V0.50.1_18 – Zertifizierungsstatus, getrennte Info-Seite und TFT-Gültigkeitsanzeige

- Basis: `V0.50.1_17` / ZIP-Arbeitsstand `V0.50.1_6`.
- Web-Hauptseite: neuer rechtsbündiger Button `Info`; `Identität` heißt jetzt `Zertifizierung`.
- Neue eigenständige Web-Seite `/validity`: nur Gesamtübersicht zu Gerät, Gerätejustierung, Kopfkalibrierung und Firmware; oben nur Rücksprung zur Hauptanzeige. Firmware besitzt bewusst kein Ablaufdatum.
- Web-Seite `/calibration`: getrennte Seite `Kalibrierung verwalten` mit unmittelbarem Status, Kalibriertag und Gültigkeit der Geräte- und Kopfkalibrierung sowie Export-/Importaktionen.
- Zertifizierungsseite zeigt oben den aktuellen Gesamtfortschritt für Geräteidentität, Gerätejustierung, Kopfkalibrierung und Firmwarefreigabe.
- Web-Setup: `Info / Gültigkeit` bleibt erhalten und öffnet dieselbe `/validity`-Seite wie der Hauptseiten-Button. Beim Rücksprung aus dem Browser-Cache wird die Setup-Sitzung neu initialisiert; `EXIT` funktioniert danach wieder.
- TFT-Setup: neuer rein lesender, mit UP/DOWN scrollbarerer Bildschirm `Info / Gültigkeit` nach dem Vorbild der Lizenzseite.
- Interne Build-ID auf `0.50.1_18` angehoben.

# V0.50.1_17 – INFO aus Hauptanzeige entfernt, Web-Übersicht im Setup

- Der versehentlich dauerhaft in die untere TFT-Hauptleiste eingebaute `INFO`-Knopf wurde vollständig entfernt.
- Die Hauptanzeige, Diagnoseansicht und ADC-Info erhalten dadurch ihren bisherigen freien Statusbereich zurück.
- Im Web-Setup gibt es den Eintrag `Info / Gültigkeit`; er öffnet die Web-Seite `/calibration`.
- Die Web-Seite fasst jetzt Gerät, Geräte-SN/Zertifikat, Geräte-Kalibrierung, Kopfzertifikat/Kopf-Kalibrierung und die vorbereitete Firmwarefreigabe zusammen.
- Geräte- und Kopfkalibrierung zeigen Kalibriertag, `Gültig ab` und `Gültig bis`.
- Das Gerätezertifikat zeigt sein Ausstellungsdatum; die Firmwarezeile zeigt bis zur späteren Manifestfunktion Buildtag und vorbereiteten Status.
- Kalibrier-, Zertifikats-, Signatur-, Mess-, Regel- und Safety-Logik bleiben unverändert.
- Interne Build-ID auf `0.50.1_17` angehoben.

# V0.50.1_16 – TFT-Kalibrierworkflow, Kopfzertifikat und Gültigkeitsübersicht

- Neue TFT-Unterseite `Signierte Kalibrierung` im Kalibriermenü.
- Geräte-Kalibrieranfrage `.tpdcalreq` kann direkt auf `/CALIBRATION/REQUESTS` der SD-Karte erzeugt werden.
- Kopf-Kalibrieranfrage `.tphcalreq` kann direkt auf SD erzeugt werden; Kopf-SN `00000` wird für diesen zertifizierten Pfad ausdrücklich abgewiesen.
- Signierte Gerätefreigabe `.tpdcal` und signiertes Kopfzertifikat `.tphcal` können direkt vom TFT importiert und kryptografisch geprüft werden.
- Neuer `INFO`-Knopf in der unteren Dashboard-Leiste, auch auf Diagnose- und ADC-Seiten.
- Die INFO-Seite zeigt Geräteidentität, Geräte-Kalibrierung, Kopfzertifikat und vorbereiteten Firmware-Freigabestatus auf einen Blick.
- Für Geräte- und Kopfkalibrierung werden Kalibriertag und `Gültig bis` angezeigt; der Ablauf bleibt informativ und blockiert die Messung nicht.
- Das Gerätezertifikat zeigt Ausstellungsdatum und ausdrücklich `ohne Ablaufdatum`, da das aktuelle `.tpcert`-Format keine Laufzeit enthält.
- Firmwarefreigabe ist als eigener Statusdatensatz und INFO-Zeile vorbereitet; ein signiertes Firmwaremanifest wird in diesem Stand noch nicht importiert.
- Interne Build-ID auf `0.50.1_16` angehoben.
- Messung, ADC, Regelung, Peltier-PWM, Safety und bestehende Kalibrierwerte wurden nicht verändert.

# TP-3000 Changelog

## V0.50.1_15 TEST – RAM1-Rodata-Optimierung der signierten Kalibrierung

- Basis: `V0.50.1_14_TEST_SIGNED_CALIBRATION_WORKFLOW`.
- Keine funktionale Änderung am Kalibrier-, Zertifikats- oder Importablauf.
- Sämtliche statischen JSON-Feldnamen, JSON-Ausgabevorlagen, Status- und Fehlermeldungen aus `TPsignedCalibration.cpp` liegen gesammelt in `.progmem` im QSPI-Flash.
- Die komplette neue Kalibrier-Webseite, Ergebnisvorlage, Download-Header und Webstatus-Texte liegen in `PROGMEM`.
- Die fünf Kalibrier-Webpfade werden als gemeinsame Flash-Konstanten wiederverwendet.
- Die Readiness-Texte aus `TPsignedData.cpp` liegen ebenfalls in `.progmem`.
- Der feste Datums-/Zeitparser bleibt unverändert. Es wurde kein `sscanf()`, `strftime()` oder `mktime()` eingebaut.
- RAM2-Arbeitspuffer für JSON, Objekte und kanonische Daten bleiben bewusst unverändert.
- Erwartung: ungefähr 10 kB bis 11 kB RAM1-Rodata werden gegenüber V0.50.1_14 zurückgewonnen; der exakte Wert ist mit dem Teensyduino-Linkerbericht zu bestätigen.

# V0.50.1_11 – passender Test-Root und JSON-Unicode-Escapes

## V0.50.1_14 TEST – Signierte Geräte- und Kopfkalibrierung

- Neue Web-Seite `/calibration` für den vollständigen Kalibrierfreigabe-Ablauf.
- Export der aktuellen Geräte-Werksjustierung als selbstsignierte `.tpdcalreq`.
- Export der aktuellen Sensorkopf-Kalibrierung als selbstsignierte `.tphcalreq`.
- Beide Anfragen enthalten das aktive Gerätezertifikat und werden mit dem internen P-256-Geräteschlüssel signiert.
- Import und Prüfung von root-vertrauenskettengestützten `.tpdcal`- und `.tphcal`-Dateien aus `/CALIBRATION`.
- Der root-zertifizierte `CALIBRATION`-Rollenschlüssel wird für Folgekalibrierungen wiederverwendet; pro Kalibrierung entstehen neue signierte Kalibrierdateien, keine neuen privaten Schlüssel.
- Kalibrier-ID, Gültig-ab/Gültig-bis, Intervall, Kalibrierstelle, Bearbeiter, Zertifikatsreferenz und Kalibriergrund sind signierter Bestandteil.
- Ablauf des Kalibrierzeitraums ist nur eine metrologische Information. Messung, Logging und Export werden nicht blockiert; die spätere Logprüfung zeigt „außerhalb Kalibrierzeitraum“.
- Geräte- und Kopfkalibrierung werden getrennt geprüft und müssen exakt zu den aktuell aktiven Werten sowie zu Geräte-SN/Key-ID beziehungsweise Kopftyp/Kopf-SN passen.
- Die Firmwarefreigabe bleibt `DEVELOPMENT` und ist für diesen Entwicklungsablauf nicht blockierend.
- Sicherheitssperre: `Zertifiziert` bleibt in V0.50.1_14 noch gesperrt, weil die eigentliche CSV-Snapshot- und `.TPSIG`-Erzeugung noch nicht aktiv ist. Die Kalibrierseite zeigt separat, ob alle dafür nötigen Nachweise vorliegen.


- Basis: `V0.50.1_10_TEST_DEVICE_IDENTITY_RODATA_FLASH`.
- Eingebetteten öffentlichen P-256-Test-Root auf den tatsächlich im Offline-KeyTool
  verwendeten Root mit Key-ID `6CC674A19758D153` umgestellt.
- Öffentliche Root-Datei unter `test_identity/TP3000_TEST_ROOT_PUBLIC.pem` aktualisiert.
- Der minimale Zertifikatsparser dekodiert jetzt vierstellige JSON-Unicode-Escapes
  für ASCII-Zeichen, insbesondere `\u002B` in Base64-Signaturen.
- Dadurch kann die unveränderte, vom KeyTool erzeugte `.tpcert` direkt importiert
  werden; ein manuelles Ersetzen von `\u002B` durch `+` ist nicht mehr nötig.
- Privater Geräteschlüssel, EEPROM-Daten, Zertifikatsformat, Signaturverfahren,
  Messung, Regelung, Logging und Safety bleiben unverändert.

# V0.50.1_10 – Identity-Konstanten und Webtexte im QSPI-Flash

- Basis: `V0.50.1_9_TEST_DEVICE_IDENTITY_RAM1_FLASHMEM_RESTORED`.
- Sämtliche zur Geräteidentität neu hinzugekommenen Laufzeittexte und die beiden
  HTML-Vorlagen der Identity-Webseite liegen jetzt explizit in `PROGMEM`.
- Die SHA-256-Rundentabelle `K[64]` liegt in `PROGMEM`.
- Das aktive micro-ecc-P-256-Kurvenobjekt liegt in `.progmem`; die bereits
  ausgelagerten micro-ecc-Funktionen bleiben in `.flashmem`.
- Öffentlicher Test-Root, SPKI-Präfix und feste Identity-Formattexte liegen
  ebenfalls in `.progmem`.
- Datenformate, Schlüssel, Zertifikate, EEPROM-Aufteilung und Webablauf bleiben
  unverändert.
- Keine ADC-, Regler-, PWM-, DRDY-, Logging- oder Safety-Funktion wurde geändert.
- Die tatsächliche RAM1-Ersparnis muss mit dem Teensyduino-Linkerbericht geprüft
  werden; dieser Stand verspricht bewusst keinen festen Bytewert vor dem Build.

# V0.50.1_9 – RAM1-Optimierung aus V0.50.1_3 wiederhergestellt

- Basis: `V0.50.1_8_TEST_DEVICE_IDENTITY_NO_SSCANF_RAM_FIX`.
- Die bereits getestete FLASHMEM-Auslagerung aus `V0.50.1_3` wurde erneut übernommen.
- Alle nicht zeitkritischen Funktionen aus `TPidentity.ino` liegen wieder im QSPI-Flash.
- Die komplette `TpSha256`-Implementierung liegt wieder im QSPI-Flash.
- Der 768-Byte-Puffer `identityCanonicalBuffer` liegt wieder in RAM2 (`DMAMEM`).
- Der feste ISO-Zeitparser aus V0.50.1_8 bleibt erhalten; `sscanf()` wird nicht wieder eingebunden.
- Die projektlokale micro-ecc-Bibliothek bleibt unverändert und weiterhin mit `.flashmem` markiert.
- Keine ADC-, Regler-, PWM-, DRDY- oder Safety-Funktion wurde verschoben.

# V0.50.1_8 – RAM1-Fix: `sscanf()` aus Zertifikatsparser entfernt

- Auf Basis von `V0.50.1_7_TEST_DEVICE_IDENTITY_LOCAL_UECC_LIBRARY_FIXED`.
- Den einzigen neuen `sscanf()`-Aufruf in `identityParseIsoUtc()` durch einen
  festen Parser fuer `YYYY-MM-DDTHH:MM:SS` ersetzt.
- Dadurch wird die universelle newlib-Scan-Engine mit ihren grossen
  Locale-/Parser-Tabellen nicht mehr allein fuer das Zertifikatsdatum
  eingebunden.
- Zertifikatsformat, Signatur, EEPROM-Aufteilung, Messung, Regelung und Safety
  bleiben unveraendert.

# V0.50.1_7 – lokale micro-ecc-Arduino-Bibliothek

- Neu auf Basis von `V0.50.1_2_TEST_DEVICE_IDENTITY` aufgebaut.
- micro-ecc 1.0.0 vollständig als echte projektlokale Arduino-Bibliothek unter
  `libraries/TP3000_micro_ecc/src` integriert.
- Eindeutiger Header `TP3000_uECC.h`; eine global installierte micro-ecc-Version
  kann dadurch nicht versehentlich ausgewählt werden.
- Alle benötigten `.inc`-Dateien liegen neben `uECC.c`; kein Include aus einem
  Sketch-Unterordner und kein Vorbereitungsskript mehr erforderlich.
- Nur secp256r1/P-256 aktiviert; komprimierte Punkte und öffentliche VLI-API deaktiviert.
- micro-ecc-Funktionen für Teensy 4.x mit `.flashmem` markiert.
- Keine Änderung an EEPROM-, Schlüssel-, Zertifikats- oder `.tpreq`/`.tpcert`-Formaten.

# TP-3000 Changelog

## 2026-07-11 – V0.50.1 Test-Provisionierung für Geräteidentität

- Firmwareversion auf `0.50.1` erhöht; Firmwaredatum bleibt `11.07.2026`.
- Individueller ECDSA-P-256-Geräteschlüssel wird im Teensy erzeugt. Der private
  Schlüssel wird niemals über Web oder SD exportiert.
- Hardware-Zufall des Teensy wird über die Teensyduino-Bibliothek `Entropy` als
  Zufallsquelle für micro-ecc verwendet.
- Neue selbstsignierte Zertifikatsanfrage `TP3000-CERTIFICATE-REQUEST-1` als
  `.tpreq`, kompatibel zum `TP3000KeyTool V0.3.0`.
- Neues Root-signiertes Gerätezertifikat
  `TP3000-DEVICE-CERTIFICATE-2` als `.tpcert`.
- Web-Testseite `/identity` für Schlüsselerzeugung, `.tpreq`-Download und
  Zertifikatsimport von der SD-Karte.
- Zertifikate werden vor Aktivierung vollständig geprüft: Test-Root, P-256-
  Signatur, Manifest-SHA-256, Geräte-Key-ID und vollständiger Public Key.
- Nach erfolgreichem Import stammt die wirksame Geräte-SN ausschließlich aus
  dem Zertifikat. Eine andere SN kann über normale Einstellungen oder USB nicht
  mehr gesetzt werden; das erneute Setzen derselben zertifizierten SN bleibt für
  bestehende Backup-Pfade zulässig.
- Privater Schlüssel und kompaktes Zertifikat werden jeweils redundant in
  A/B-EEPROM-Slots mit Sequenznummer und CRC32 gespeichert. Dafür werden die
  bisher freien EEPROM-Adressen `3712..4283` verwendet.
- Eine Compile-Sperre verhindert den Identity-Build für andere Boards als Teensy
  4.1, damit die EEPROM-Adressen nicht versehentlich auf einem kleineren Ziel
  verwendet werden.
- Diese Fassung vertraut ausdrücklich nur dem vorhandenen Wegwerf-Test-Root
  `6CC674A19758D153` und ist keine Produktiv-Provisionierung.
- Secure Boot und Teensy-Sperrmodus bleiben deaktiviert. Eigene GPLv3-Builds
  können weiterhin installiert und ausgeführt werden.
- Messdateisignatur, Kalibrierungssignatur und Firmwarefreigabe sind noch nicht
  Bestandteil dieses Teststands.
- Keine Änderung an Messwerterfassung, Regelung, Safety oder SD-Messlogging.

### 2026-07-11 – Firmwaredatum auf Veröffentlichungsstand gesetzt

- Sichtbares Firmwaredatum von `20.06.2026` auf `11.07.2026` aktualisiert.
- Datum auf TFT, Weboberfläche, USB-Versionsausgabe und in der Begleitdokumentation vereinheitlicht.
- Versionsnummer bleibt unverändert bei `0.50.0`; historische Changelog-Einträge wurden nicht umdatiert.
- Keine Änderung an Messung, Kalibrierung, Regelung, Safety, Logging oder Schnittstellenverhalten.

### 2026-07-10 – Abschließender öffentlicher Paket- und Markencheck

- Versehentlich mitgepackte interne Auditdatei `T_calls.txt` entfernt.
- ALMEMO-/WinControl-Hinweis fachlich präzisiert: ALMEMO wird AHLBORN zugeordnet; AMR WinControl wird von akrobit entwickelt und für ALMEMO-Systeme über AHLBORN vertrieben.
- Der Hinweis auf TFT, Web und in den Dokumenten nennt nun ausdrücklich, dass keine allgemeine oder vollständige ALMEMO-Kompatibilität zugesagt wird und dass keine Verbindung zu AHLBORN oder akrobit besteht.
- Breite Kompatibilitätsformulierungen in Quellkommentaren durch die technisch engere Beschreibung „ALMEMO-V6-Format-Ausgabe“ ersetzt; keine Protokolländerung.
- WDT_T4-Dokumentation an den tatsächlichen Paketinhalt angepasst: Beispiele und CI-Metadaten sind nicht enthalten, benötigte Quellen und MIT-Lizenz bleiben erhalten.
- Font-Provenienzprüfer auf den gemeinsamen UTF-8-Konverter `tpUtf8ToFontByte()` umgestellt; direkte TFT-Ausgabe und `TextBox::print()` werden gemeinsam geprüft.
- Keine Änderung an Messung, Kalibrierung, Regelung, Safety, Logging oder Schnittstellenverhalten.

### 2026-07-10 – Lizenzseiten: Umlaute, GPL-Hinweis und Quellcode-Link

- Die direkt auf den RA8875 gezeichnete TFT-Seite `Lizenzen / Marken` setzt UTF-8-Zeichen nun über eine lokale Konvertierung korrekt auf die erweiterten Droid-Sans-Mono-Zeichenbytes um. Dadurch werden unter anderem `Änderung`, `Gewährleistung`, `ausschließlich`, `Geräte`, `für`, `über`, `unabhängig`, `Unterstützung`, `Prüfung`, `Ausführliche` und `zurück` fehlerfrei dargestellt.
- TFT und Web nennen ausdrücklich, dass Weitergabe und Änderung unter `GPL-3.0-only` erlaubt sind.
- TFT und Web zeigen die Quellcodeadresse `github.com/DK6WT/chilled_mirror`; die Webanzeige verlinkt zusätzlich direkt auf `LICENSE` und `THIRD_PARTY_NOTICES.md`.
- Keine Änderung an Messung, Regelung, Safety, Kalibrierung oder Kommunikationsprotokollen.


### 2026-07-10 – Bootscreen-Umlautdarstellung korrigiert

- Die deutsche Bootzeile fuer die Geraete-Seriennummer verwendet beim direkten `tft.print()` nun den internen Droid-Sans-Mono-Zeichencode `0xB2` fuer `ä` statt der zweibyteigen UTF-8-Folge.
- Dadurch werden die Zeilen `Gerät-SN` und `Head` auf dem Startbildschirm nicht mehr ueberlagert bzw. mit schwarzen Artefakten dargestellt.
- Die UTF-8-Umsetzung in `TextBox::print()` bleibt unveraendert; der Fix betrifft nur die direkte RA8875-Ausgabe im Startbildschirm.

### 2026-07-10 - UTF-8-/Umlaut-Audit der TFT- und Webanzeige

- Gemeinsame direkte RA8875-UTF-8-Ausgabe `tpTftPrintUtf8()` ergänzt.
- `TextBox::print()` und direkte TFT-Seiten verwenden dieselbe Zuordnung für `° Ω ä ö ü Ä Ö Ü ß µ ± Δ ≤ ≥`.
- Verbleibende direkte TFT-Ausgaben mit `Zurück`, `Stabilität`, `PRÜFEN` und `H-BRÜCKE` korrigiert.
- Sichtbare Restumschriften in Menüs korrigiert (`Übersicht`, `GERÄTE-SPEICHER`, `Änderungen verwerfen`).
- Deutsche Webmeldungen und der Modustext `Kühlen` auf echtes UTF-8 umgestellt.


## 2026-07-10 – Ethernet/mDNS-Kompilierfix

- Fehlende Vorwärtsdeklaration von `ethIpIsValid()` vor
  `ethBeginMdnsIfNeeded()` ergänzt. Damit kompiliert der mDNS-Pfad auch bei
  Arduino/Teensy-Builds, bei denen der automatisch erzeugte Prototyp wegen
  `static` und `FLASHMEM` nicht rechtzeitig verfügbar war.
- Signed/unsigned-Warnung bei der Web-Einstellung des Hauptscreen-Layouts
  durch einen expliziten Integervergleich beseitigt.

## 2026-07-10 – Droid Sans Mono unabhängig neu erzeugt und erweitert

- `fonts.c` wurde durch die reproduzierbar erzeugte Datei
  `fonts_droidsansmono_data.c` ersetzt.
- `tools/font_provenance/generate_droidsansmono.py` erzeugt alle 18 Größen
  direkt aus der geprüften AOSP-TTF bei 100 dpi mit monochromer
  FreeType-Rasterung und liest keine historischen ILI9341-Fonttabellen.
- Der ASCII-Bereich bleibt 0x20-0x7E; der zweite Bereich wurde von 0xB0-0xB5
  auf 0xB0-0xBE erweitert.
- Neu verfügbar sind Ä, Ö, Ü, ß, µ, ±, Δ, ≤ und ≥. Die bestehenden Bytes für
  °, Ω, ä, ö, ü und das kleine Prozentzeichen bleiben unverändert.
- Das kleine Prozentzeichen wird in jedem Generatorlauf aus dem frisch
  gerasterten normalen Prozentzeichen erzeugt, auf 75 % verkleinert und in der
  Monospace-Zelle zentriert.
- `TextBox::print()` übersetzt die neuen UTF-8-Zeichen in die lokalen Fontbytes;
  bestehende UI-Texte wurden nicht automatisch umgestellt.
- Beide verteilten Fontdateien sind damit unabhängig aus eindeutig lizenzierten
  Originalfonts erzeugt; das historische `ILI9341_fonts`-Archiv ist für keine
  verteilten Fontdaten mehr Quelle oder Build-Abhängigkeit.

## 2026-07-10 – AwesomeF080_40 unabhängig neu erzeugt

- Nur der tatsächlich verwendete Font `AwesomeF080_40` bleibt erhalten.
- Enthalten sind nur die drei im Touch-Menü verwendeten Glyphen U+F090,
  U+F0AA und U+F0AB an den bisherigen Bytepositionen 0x10, 0x2A und 0x2B.
- `tools/font_provenance/generate_fontawesome_f080_40.py` erzeugt Daten,
  Index und Deskriptor direkt aus der offiziellen Font-Awesome-4.5.0-TTF bei
  40 pt / 100 dpi mit monochromer FreeType-Rasterung.
- Der Generator liest oder kopiert keine historischen `ILI9341_fonts`-Tabellen.
- Die 17 unbenutzten AwesomeF080-Größen und alle nicht benötigten Glyphen wurden
  entfernt; `fonts.h` deklariert nur noch `AwesomeF080_40`.
- Der Bediencode und seine Icon-Bytewerte bleiben unverändert. Die neue
  Rasterung soll einmal auf dem realen TFT optisch geprüft werden.

## 2026-07-10 – Fontdaten nach Lizenz getrennt

- `fonts.c` enthält jetzt ausschließlich Droid-Sans-Mono-Daten und ist als
  `Apache-2.0` gekennzeichnet.
- Alle `AwesomeF080`-Daten-, Index- und Deskriptortabellen wurden unverändert
  nach `fonts_fontawesome_f080_data.c` verschoben; die neue Datei ist als
  `OFL-1.1` gekennzeichnet.
- `fonts.h` enthält nur noch die gemeinsamen Symboldeklarationen und ist als
  `GPL-3.0-only` gekennzeichnet.
- Das Provenienz-Prüfskript und die Paketdokumentation wurden auf die getrennten
  Dateien umgestellt.
- Keine Fontdaten, Deskriptorwerte oder sichtbare TFT-Darstellung wurden
  verändert.

## 2026-07-10 – Font provenance hardening for public source package

- Added the exact AOSP `DroidSansMono.ttf` source reference under Apache-2.0.
- Added the exact Font Awesome 4.5.0 TTF/OTF source references under OFL-1.1.
- Documented the historical `otf2bdf` (MIT) and `bdf_to_ili9341`
  (GPL-3.0-or-later) conversion path.
- Added `tools/font_provenance/verify_font_provenance.py` for source-hash,
  embedded-table and optional historical-reference verification.
- Updated font notices and package documentation so licensing no longer relies
  solely on the missing Top-Level license of the historical font collection.
- Kept all packed font arrays and descriptors unchanged to preserve the current
  TFT rendering; only comments, documentation and provenance files changed.

## 2026-07-10 - GitHub-Publikationsvorbereitung für 0.50.0

- Basis: frisch entpackter Stand `Taupunktspiegel_StandV1_2026-07-03_V0.50.0_21.zip`.
- GSL1680-Treiberrechte nach der Upstream-Korrektur einheitlich als
  `GPL-3.0-only` dokumentiert; alte Apache/GPL-Kombinationsangaben entfernt.
- Panelspezifische GSL1680-Herstellerfirmware aus dem öffentlichen Quellstand
  ausgeschlossen und Build-/Notice-Texte auf lokale Installation umgestellt.
- WDT_T4 0.1 als mitgelieferte MIT-Bibliothek dokumentiert.
- ALMEMO-/WinControl-Hinweis präzisiert: serielle ALMEMO-Anbindung und Ausgabe
  im ALMEMO-V6-Format statt allgemeiner Kompatibilitätsaussage.
- TFT- und Web-Lizenzseiten synchronisiert; UP/DOWN scrollt am TFT weiterhin
  jeweils zwei Zeilen.
- `earth.c` um einen eindeutigen GPL-3.0-only-SPDX-Hinweis ergänzt; zugleich
  klargestellt, dass die GPL-Kennzeichnung nur die TP-3000-spezifische
  Quelldarstellung und etwaige eigene schutzfähige Bestandteile erfasst und
  NASA-Ausgangsmaterial nicht neu lizenziert.
- GitHub-README fachlich aktualisiert: Projektstatus vorsichtiger formuliert,
  Entwicklungslinie präzisiert, BMP585-Hardware korrekt benannt und die
  Feuchteberechnung von der veralteten Sonntag-Angabe auf die tatsächlich
  implementierte Hardy/Wexler-Formulierung mit WMO-Enhancement-Faktor
  berichtigt.
- Keine beabsichtigte Änderung an Messung, Kalibrierung, Regelung, Safety,
  Logging oder Kommunikationsprotokollen.

## 2026-07-03 - V0.50.0_21_TEST_ALT_HAUPTSCREEN

- Basis: `Taupunktspiegel_StandV1_2026-07-03_V0.50.0_20.zip`.
- Geaendert: `TP_T.h`, `TP-3000.ino`, `TPlanguage.h`, `TPmenu.ino`, `TPmenu_Pages.ino`, `TPdisplay.ino`, `TPethernet.ino`, `CHANGELOG.md`.
- Unter `Setup -> Anzeige` ist ein neues Menue `Hauptscreen Layout` ergaenzt. Waehlen kann man `Standard` oder `3 Werte`; die Auswahl wird im bestehenden Device-/UI-EEPROM-Block dauerhaft gespeichert.
- Neuer alternativer Hauptscreen `3 Werte`: Der grosse Statusrahmen nutzt die gleiche Flaeche wie die Chart-Ansichten und zeigt Feuchte, Taupunkt und T-Umgebung gleich gross untereinander.
- Im alternativen Hauptscreen wird `T-Umgebung` rechts unten ausgeblendet; `T-Spiegel` rutscht dort eine Zeile nach unten auf die bisherige T-Umgebung-Position.
- Web-Hauptscreen und Web-Setup wurden entsprechend erweitert; `/data.json` liefert dafuer zusaetzlich `mainLayout`, `/setup.json` liefert `ui.mainLayout`.

## 2026-07-03 - V0.50.0_20_TEST_ETH_HOSTNAME_SN

- Basis: `Taupunktspiegel_StandV1_2026-07-03_V0.50.0_19.zip`.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- Ethernet-Kennung ergaenzt: Aus der Geraete-Seriennummer wird der Hostname `TP-3000-xxxxx` gebildet.
- Nach erfolgreicher IP-Konfiguration startet NativeEthernet-mDNS und stellt das Geraet lokal als `TP-3000-xxxxx.local` bereit; der Webserver wird zusaetzlich als `_http._tcp` angekuendigt.
- Web-Setup `Schnittstellen -> Ethernet -> Status/Info` zeigt den Namen nun als `Name: TP-3000-xxxxx` an.
- Die Hardware-MAC bleibt unveraendert die eindeutige Teensy/PJRC-MAC, damit keine doppelten MAC-Adressen entstehen.

## 2026-07-03 - V0.50.0_19_TEST_T_UMGEBUNG_CHART

- Basis: `Taupunktspiegel_StandV1_2026-07-03_V0.50.0_18.zip`.
- Geaendert: `TPdisplay.ino`, `TPethernet.ino`, `CHANGELOG.md`.
- Hauptscreen-Chartfolge erweitert: Zahlenanzeige -> Feuchte -> Taupunkt -> T-Umgebung -> Zahlenanzeige.
- TFT: T-Umgebung nutzt dieselbe Chartlogik wie Taupunkt mit 60 min / 4 h / 8 h / 24 h, Min/Max, aktueller Wert und dynamischer Skalierung.
- Web: `/chart.json` liefert zusaetzlich T-Umgebung (`taScale`, dritter Wert je Punkt); die Webanzeige zeichnet daraus den dritten Chart und synchronisiert die Ansicht mit dem bestehenden Touch-/Klickbereich.

## V0.50.0_32_TEST_STATIC_IP_NO_LINK_FIX
- Static-IP-Start ohne Netzwerkkabel abgesichert: `Ethernet.begin(mac, ip, dns, gateway, subnet)` wird nicht mehr direkt aufgerufen, wenn kein belastbarer PHY-Link erkannt wird.
- Vor Static-IP wird NativeEthernet einmalig mit einem sehr kurzen DHCP-Probe initialisiert, damit `linkStatus()` verwertbar wird. Ohne Kabel endet der Versuch nach kurzer Zeit statt im Startbildschirm haengen zu bleiben.
- Feste IP mit vorhandenem Link bleibt unveraendert; es gibt weiterhin keinen automatischen `Ethernet.begin()`-Suchlauf im Messloop.

## 2026-07-02 - V0.50.0_31_TEST_DHCP_PENDING_MAINTAIN_FIX

- Basis: `Taupunktspiegel_StandV1_2026-07-02_V0.50.0_28_TEST_STATIC_IP_EDIT_BUFFER_FIX.zip`, weil dieser Stand laut Test mit fester IP und Webverbindung funktioniert.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- DHCP-Pfad wieder naeher an den letzten funktionierenden Ethernet-Start gebracht: nur ein expliziter `Ethernet.begin(mac, 5000, 1000)` beim Boot bzw. nach Bedienaktion, keine mehrfache begin-Wiederholung und keine Linkstatus-Wartekette.
- Wenn NativeEthernet nach dem DHCP-begin noch keine gueltige `localIP()` liefert, bleibt der Stack bis zu 30 s als DHCP-pending aktiv. In dieser Zeit wird nur `Ethernet.maintain()` einmal pro Sekunde ausgefuehrt; es gibt weiterhin keinen automatischen `Ethernet.begin()`-Suchlauf im Messloop.
- Sobald in der Nachlaufzeit eine IP erscheint, wird der Webserver gestartet und die IP-Anzeige wird gueltig. Wenn keine IP kommt, wird Ethernet wieder deaktiviert.



## 2026-07-02 - V0.50.0_28_TEST_STATIC_IP_EDIT_BUFFER_FIX

- TFT-Menü `Schnittstellen -> Ethernet -> IP/Subnet/Gateway/DNS` robuster gemacht.
- IP-Oktette werden jetzt zuerst in einem lokalen Editierpuffer geändert und erst beim letzten ENTER in die Schnittstellenkonfiguration übernommen.
- Wenn die IP-Seite mit DHCP AUS betreten wurde, bleibt DHCP während der gesamten Eingabe hart AUS; auch beim Speichern wird `eth_dhcp=0` gesetzt.
- Damit kann DHCP beim Ändern von IP/Subnet/Gateway/DNS nicht mehr scheinbar von selbst auf EIN springen.


## V0.50.0_27_TEST_STATIC_IP_DHCP_MENU_FIX – 2026-07-02

- Fix: DHCP wird beim Bearbeiten statischer IP-Daten nicht mehr automatisch wieder eingeschaltet.
  Ursache war die zu harte Static-IP-Sanitize-Pruefung in `interfaceConfigSanitize()`: waehrend IP, Subnet, Gateway oder DNS noch unvollstaendig/unplausibel waren, wurde `eth_dhcp` auf EIN gesetzt und die Default-IP-Felder wurden zurueckgeschrieben.
- Static-IP-Plausibilitaet wird jetzt erst beim Ethernet-Start bewertet. Ungueltige Static-IP ruft kein `Ethernet.begin()` auf und blockiert den Boot nicht, veraendert aber die gespeicherte DHCP-Auswahl nicht.
- Damit bleiben IP/Subnet/Gateway/DNS im TFT-Menue editierbar, wenn DHCP AUS ist; die Subnetzmaske zeigt nicht mehr faelschlich `DHCP aktiv`, solange DHCP tatsaechlich AUS ist.
- Hinweis: Ein kurzes Relais-Ticken beim IP-Editieren wurde noch nicht als eindeutiger Codepfad bestaetigt; bitte mit dieser Version erneut beobachten, ob es zusammen mit dem DHCP-Sanitize-Fehler verschwindet.


## 2026-07-02 - V0.50.0_26_TEST_DHCP_LINK_SETTLE_FIX

- Ethernet/DHCP: Wenn `Ethernet.begin()` beim Einschalten aus dem TFT-Menue fast sofort ohne IP zurueckkehrt, wird der PHY-Link jetzt bis zu 3000 ms abgewartet und DHCP danach begrenzt erneut versucht.
- Die Warte-Seite `Ethernet wird aktiviert` darf dadurch nicht mehr nur kurz aufblitzen, wenn der Link beim spaeten Start noch nicht stabil ist.
- Bei vorhandenem Link und echtem DHCP-Timeout wird nicht mehrfach unnoetig wiederholt.
- Es gibt weiterhin keinen automatischen `Ethernet.begin()`-Suchlauf im normalen Messloop; die Wiederholungen passieren nur beim Boot oder nach Bedienaktion.

## 2026-07-02 - V0.50.0_25_TEST_DHCP_BEGIN_RETRY_FIX

- Basis: `Taupunktspiegel_StandV1_2026-07-02_V0.50.0_24_TEST_DHCP_IP_STATUS_FIX.zip`.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- DHCP-Start weiter abgesichert: `Ethernet.hardwareStatus()` wird vor dem ersten `Ethernet.begin()` nicht mehr als harte Sperre benutzt, weil NativeEthernet beim spaeten Start aus dem Menue noch `EthernetNoHardware`/Unknown liefern kann. In diesem Fall erschien die Warte-Seite nur kurz und DHCP wurde gar nicht gestartet.
- DHCP nutzt weiterhin keinen automatischen Suchlauf im Messloop, bekommt aber beim Boot oder nach Bedienaktion bis zu drei begrenzte Startversuche mit kurzem Abstand. Dadurch kann der PHY/Link nach einem spaeten `Ethernet.begin()` stabilisieren, ohne die Messloop zu belasten.
- DHCP-Response-Timeout von 1000 ms auf 3000 ms erhoeht; Gesamt-Timeout bleibt 5000 ms pro Versuch.

## 2026-07-02 - V0.50.0_24_TEST_DHCP_IP_STATUS_FIX

- Basis: `Taupunktspiegel_StandV1_2026-07-02_V0.50.0_23_TEST_DHCP_LINK_FIX.zip`.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- DHCP-/IP-Status weiter korrigiert: `ethernetHasValidIp()` prueft ab jetzt die tatsaechliche `Ethernet.localIP()` und nicht mehr zusaetzlich `Ethernet.linkStatus()`. Dadurch wird eine erfolgreich per DHCP erhaltene Adresse nicht mehr als `---.---.---.---` angezeigt, wenn NativeEthernet den PHY-Link kurzzeitig als `LinkOFF`/`Unknown` meldet.
- `ethernetServiceTask()` blockiert Web/WinControl nicht mehr hart nur wegen `linkStatus()`. Entscheidend ist eine gueltige lokale IP; ohne physikalischen Link kommen ohnehin keine Clients an.
- `Ethernet.maintain()` laeuft bei DHCP auch dann weiter, wenn `linkStatus()` nicht `LinkON` meldet, damit ein kurzzeitig unbekannter Linkstatus die DHCP-Pflege nicht dauerhaft unterbindet.
- Statische IP wird nach `Ethernet.begin(...)` wieder anhand der gesetzten lokalen IP als konfiguriert bewertet; ein unzuverlaessiger Linkstatus deaktiviert sie nicht mehr. Es gibt weiterhin keinen automatischen `Ethernet.begin()`-Suchlauf im Messloop.

## 2026-07-02 - V0.50.0_23_TEST_DHCP_LINK_FIX

- Ethernet-Linkerkennung korrigiert: `Ethernet.linkStatus()` wird nicht mehr vor dem ersten `Ethernet.begin()` als harte Startbedingung verwendet. In V0.50.0_22 konnte dadurch DHCP trotz gestecktem Kabel uebersprungen werden, weil NativeEthernet vor der Initialisierung `LinkOFF`/`Unknown` liefern kann.
- DHCP startet wieder mit dem kurzen Timeoutpfad `Ethernet.begin(mac, ETH_DHCP_TIMEOUT_MS, ETH_DHCP_RESPONSE_TIMEOUT_MS)`.
- Statische IP wartet nach `Ethernet.begin(...)` kurz auf LinkON; ohne Link wird Ethernet bis Neustart oder Setup-Aktion deaktiviert, ohne im Mess-Loop automatisch zu suchen.
- Kommentare zur Boot-Safe-Ethernet-Reihenfolge aktualisiert.

## 2026-07-02 - V0.50.0_22_TEST_EEPROM_MAP_REPACK

- Basis: `Taupunktspiegel_StandV1_2026-07-02_V0.50.0_21_TEST_EEPROM_AB_SELF_HEAL.zip`.
- Geaendert: `TP-3000.ino`, `TP_T.h`, `TPlanguage.h`, `TPalarm.ino`, `TPmenu_Interface.ino`, `PT100_2P_Calibration.ino`, `Ref100_120_Calibration.ino`, `TPdeviceStorage.ino`, `CHANGELOG.md`.
- EEPROM-Map neu geordnet und mit Reserven versehen: Metrologie/Kalibrierung 0..767, Alarm 768..1023, Interface 1024..1535, Mess-/Regelparameter 1536..2047, Device-/UI-Settings 2048..2303, Factory-Cal-Backup 2304..2687, Device-Settings-Backup 2688..3711, freie Reserve 3712..4283.
- Die vier bisherigen Kalibrierbloecke Pt100-2P, Ref100/Ref120 inkl. Kanal-Korrekturen, Pt100-R0 und Taupunkt-/Frostpunkt-Offset wurden zu einem gemeinsamen Metrologie-/Kalibrierblock mit A/B-Slots je 384 Byte zusammengefasst. Dadurch gibt es weniger Header-/CRC-Overhead und einen konsistenten metrologischen Snapshot.
- ADC-Messfilter, ADC1-SFOCAL-Modus und Peltier-Stromlimit liegen nicht mehr als einzelne Magic-Bytes im EEPROM, sondern im A/B-gesicherten Mess-/Regelparameterblock zusammen mit PID, Reglerintervall, H-Bruecken-Totzeit, Optik-Sollwert, Freiheizen und Auto-Cal-Intervall.
- Sprache, Loop-Debug-Anzeige, TFT-Backlight, Geraete-SN, Kopftyp und Kopf-SN liegen nun im A/B-gesicherten Device-/UI-Settings-Block. Es gibt keine ungesicherten Einzelwerte mehr im aktiven EEPROM-Bereich.
- Alle aktiven A/B-Bloecke behalten Self-Heal: Wenn nur ein Slot gueltig ist, wird der Gegenslot beim Laden sofort repariert; bei leerem EEPROM werden beide Slots angelegt.

## 2026-07-02 - V0.50.0_21_TEST_EEPROM_AB_SELF_HEAL

- Basis: `Taupunktspiegel_StandV1_2026-07-02_V0.50.0_20_TEST_EEPROM_FULL_AB_CAL_SAFE.zip`.
- Geaendert: `TP-3000.ino`, `TPmenu_Interface.ino`, `TPalarm.ino`, `PT100_2P_Calibration.ino`, `Ref100_120_Calibration.ino`, `TPdeviceStorage.ino`, `CHANGELOG.md`.
- A/B-Self-Heal ergaenzt: Wenn beim Laden eines aktiven EEPROM-Blocks nur ein Slot gueltig ist, wird der defekte oder leere Gegenslot sofort aus dem geladenen Datensatz neu aufgebaut. Dadurch laeuft das Geraet nicht bis zur naechsten Benutzerspeicherung mit nur einem intakten Slot weiter.
- Bei leerem EEPROM werden neue aktive Default-Bloecke direkt in beide Slots geschrieben. Das betrifft Hauptkonfiguration, Schnittstellen, Alarm, Pt100-2P, Ref100/Ref120, Pt100-R0 und Taupunkt-/Frostpunkt-Offset.
- Geraete-Speicher-/Werksjustierungs-Backups reparieren einen fehlenden Gegenslot beim Lesen; die erste Sicherung nach leerem EEPROM schreibt ebenfalls direkt beide Backup-Slots.
- Hauptkonfiguration: Sanitizing wird nun erst nach dem Anwenden des Payloads im Load-Pfad bewertet, damit reparierte Werte auch wirklich wieder gespeichert werden.

## 2026-07-02 - V0.50.0_20_TEST_EEPROM_FULL_AB_CAL_SAFE

- Basis: `Taupunktspiegel_StandV1_2026-07-02_V0.50.0_18_TEST_EEPROM_BOOT_SAFE.zip`.
- Geaendert: `TP-3000.ino`, `TPlanguage.h`, `TPmenu_Interface.ino`, `TPalarm.ino`, `PT100_2P_Calibration.ino`, `Ref100_120_Calibration.ino`, `TPdeviceStorage.ino`, `CHANGELOG.md`.
- EEPROM-Schema weiter bereinigt: Kalibrier- und Schnittstellenbloecke nutzen jetzt ebenfalls A/B-Slots mit Magic, Version, Payload-Groesse, Sequenz und CRC.
- A/B-Schutz fuer metrologische Kalibrierung: Pt100-2P, Ref100/Ref120 inklusive Kanal-Korrekturen, Pt100-R0 und Taupunkt-/Frostpunkt-Offset. Ein Stromausfall waehrend Speichern kann dadurch jeweils nur den Zielslot ungueltig machen; der vorherige Slot bleibt gueltig.
- Schnittstellenkonfiguration jetzt V11 mit A/B-Slots; alte Einzelblock-/Migrationsdaten werden nicht mehr als Startquelle genutzt. Statische IP bleibt weiterhin hart plausibilisiert.
- Alarm-/Grenzwertkonfiguration ebenfalls A/B-gesichert.
- Geraete-Speicher: `Einstellungen speichern` und `Werksjustierung speichern` nutzen jetzt A/B-Slots; besonders die Werksjustierung des Grundgeraetes ist damit gegen halbe EEPROM-Schreibvorgaenge abgesichert.
- Alte `1 + sizeof(var_t) + Offset`-EEPROM-Adressen wurden fuer aktive Bloecke entfernt; kleine unkritische Einbyte-Einstellungen liegen in einem freien Bereich hinter den A/B-Backupslots.

## 2026-07-02 - V0.50.0_18_TEST_EEPROM_BOOT_SAFE

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_17.zip`.
- Geaendert: `TP-3000.ino`, `TP_T.h`, `TPmenu_Interface.ino`, `TPethernet.ino`, `TPdeviceStorage.ino`, `TPheadCalibration.ino`, `TPmenu.ino`, `TPmenu_Pages.ino`, `TPmenu_ValueFields.ino`, `TPusbSerial.ino`, `CHANGELOG.md`.
- EEPROM-Hauptkonfiguration bereinigt: Der alte `0xAA`-Kaltstartmarker und der direkte `var_t`-Dump ab Adresse 1 werden nicht mehr als Startquelle genutzt.
- Neue Hauptkonfiguration mit Magic, Version, Payload-Groesse, Sequenz, CRC und A/B-Slots. Bei ungueltigen oder halb geschriebenen Daten werden sichere Defaults geladen und sofort neu gespeichert.
- Laufzeitstruktur `R` bleibt intern erhalten, wird aber aus einem kleinen geprueften Payload aufgebaut. Speichern von Hauptparametern nutzt jetzt `tpMainConfigSave()` statt `EEPROM_writeAnything(1, R)`.
- Boot-Reihenfolge robuster: Ethernet, SD-Logging und ALMEMO-Seriellog starten erst nach sichtbarer TFT-/Touch-Initialisierung und Bootscreen.
- Ethernet-Bootschutz: Vor `Ethernet.begin()` wird der PHY-Link geprueft. Ohne Netzwerkkabel wird Ethernet uebersprungen; im normalen Mess-Loop wird nicht automatisch gesucht. Start erfolgt wieder per Neustart oder Setup-Aktion.
- Schnittstellen-EEPROM V10 als sauberer Schnitt: alte V5..V9-Layouts werden nicht mehr migriert, sondern durch sichere Defaults ersetzt.
- Statische IP wird plausibilisiert. Ungueltige IP/Subnet/Gateway-Kombinationen werden auf DHCP mit Default-IP-Feldern zurueckgesetzt; DNS `0.0.0.0` wird auf Gateway bzw. 8.8.8.8 ersetzt.
- USB-Kommando `$memorywipe` setzt die neue Hauptkonfiguration auf Defaults zurueck und startet danach neu.

## 2026-07-01 - V0.50.0_19_TEST_TFT_TCP_PORT_FAST_STEP

- Basis: `Taupunktspiegel_StandV1_2026-07-01_V0.50.0_18_TEST_TFT_UMLAUT_CASE_FIX.zip`.
- Geaendert: `TPmenu_Interface.ino`, `CHANGELOG.md`.
- TFT Schnittstellen: TCP-Port-Einstellseiten beschleunigt. Kurzes Tippen aendert weiter um 1, laengeres Halten um 10 und danach um 100 pro Schritt.
- Gilt fuer WinControl `TCP-Port`, Ethernet `TCP-Port` und `Web-Port`; Adress-/Kanal-Einstellungen bleiben bewusst bei Schrittweite 1.

## 2026-07-01 - V0.50.0_38_TEST_HEADTYPE_STP_DEVICE_SN_SAVE

- Basis: `Taupunktspiegel_StandV1_2026-06-30_V0.50.0_37_TEST_FACTORY_SAVE_PASSWORD_ONLY.zip`.
- Geaendert: `TP-3000.ino`, `TP_T.h`, `TPdeviceStorage.ino`, `TPdisplay.ino`, `TPethernet.ino`, `TPheadCalibration.ino`, `TPmenu.ino`, `TPmenu_Pages.ino`, `CHANGELOG.md`.
- Sensorkopf: Der Kopftyp wird jetzt als kompletter Text `STP-XXXX` gefuehrt und gespeichert. `STP-` ist fest, danach genau vier Ziffern.
- Bekannte Typen `STP-3001` bis `STP-3004` schalten weiterhin die internen Kopfprofile. Andere gueltige Typen wie `STP-1234` werden gespeichert und gelistet, ohne ein Profil umzuschalten.
- Der neue Kopftyp-Text liegt in einem separaten EEPROM-Block hinter den bestehenden Zusatzbloecken; `var_t` bleibt unveraendert, damit vorhandene EEPROM-Offets stabil bleiben.
- TFT und Web `Kopfdaten speichern`: Speichern fragt nun Kopftyp `STP-XXXX`, Kopf-SN und das aktuelle 5-stellige Geraete-SN-Passwort ab. Die Kopfdatei enthaelt `head_type=STP-XXXX` und nutzt `STP-XXXX` auch im Dateinamen.
- Kopfdateien laden/listen: Die SD-Auswahl arbeitet mit `STP-XXXX`; alte Dateinamen/Felder mit `STP3001` ohne Bindestrich werden beim Lesen normalisiert.
- TFT und Web `Werksjustierung speichern`: Zuerst wird das Passwort der bisher aktuellen Geraete-SN geprueft, danach wird eine neue Geraete-SN eingegeben. Bei erfolgreichem Speichern wird diese neue Geraete-SN uebernommen und ist ab dann das neue Passwort.
- `Einstellungen laden`, `Werksjustierung laden` und `Einstellungen speichern` bleiben wie in V37 ohne Passwort.
- Web-Setup-ETag auf `SETUP-PH7` erhoeht, damit Browser die neue Setup-Seite sicher laden.
- GSL1680-Firmware und `AwesomeF080_40` bleiben in Flash/PROGMEM; Touch-Logik bleibt wie in V32..V37 in RAM1/ITCM.

## 2026-06-30 - V0.50.0_37_TEST_DEVICE_STORAGE_FACTORY_SAVE_PASSWORD_ONLY

- Basis: `Taupunktspiegel_StandV1_2026-06-30_V0.50.0_36_TEST_HEAD_SAVE_STORAGE_PASSWORD_FIX.zip`.
- Geaendert: `TPmenu_Pages.ino`, `TPethernet.ino`, `CHANGELOG.md`.
- Geraete-Speicher: Passwortschutz wieder auf die gewuenschte kritische Aktion reduziert.
  - TFT und Web verlangen das 5-stellige Geraete-SN-Passwort nur noch bei `Werksjustierung speichern`.
  - `Einstellungen laden`, `Werksjustierung laden` und `Einstellungen speichern` laufen wieder ohne Passwort.
  - Web-Endpunkt `/setup-device-storage` akzeptiert fuer diese drei Aktionen wieder Requests ohne `pw`; `factory_save` verlangt weiterhin das korrekte Passwort.
- Bestehende Schutzmechanismen bleiben erhalten: Regelparameter-Entsperrung wird beim Verlassen/bei Speicheraktionen wieder verriegelt; Kopfdaten speichern bleibt unveraendert.

## 2026-06-30 - V0.50.0_36_TEST_HEAD_SAVE_STORAGE_PASSWORD_FIX

- Basis: `Taupunktspiegel_StandV1_2026-06-30_V0.50.0_35_TEST_RTC_YEAR_RANGE_2026_2079.zip`.
- Geaendert: `TPmenu_Pages.ino`, `CHANGELOG.md`.
- TFT Sensorkopf: Nach `Kopfdaten speichern` wird die Ergebnis-/Meldungsseite wieder aktiv bedient; `ReadButtons(false)` wird dort jetzt regelmaessig aufgerufen, ENTER fuehrt zurueck ins Sensorkopf-Menue und EXIT wird vom zentralen Setup-Exit wieder erkannt.
- TFT Geraete-Speicher: Die vier Speicheraktionen `Einstellungen laden`, `Werksjustierung laden`, `Einstellungen speichern` und `Werksjustierung speichern` verlangen jetzt wie im Web das 5-stellige Geraete-SN-Passwort.
- Bei falschem Passwort wird keine Speicher-/Ladeaktion ausgefuehrt und die Ergebnisanzeige zeigt `Passwort falsch.`.
- Begriffe im TFT-Geraete-Speicher wurden weiter auf `Werksjustierung` vereinheitlicht; `Werkskalibrierung` wird im deutschen Text nicht verwendet.
- GSL1680-Firmware und `AwesomeF080_40` bleiben in Flash/PROGMEM; Touch-Logik bleibt wie V32..V35 in RAM1/ITCM.

## 2026-06-30 - V0.50.0_35_TEST_RTC_YEAR_RANGE_2026_2079

- RTC-Stell- und Plausibilitaetsbereich an RV-3129-C3 angepasst.
- Der zulaessige Bereich fuer das Stellen der Uhr ist jetzt 2026..2079 statt 2016..2040.
- Hintergrund: Der RV-3129-C3 speichert das Jahr als 20xx-BCD-Zaehler; laut Application Manual ist der gueltige Jahreszaehler 00..79.
- TFT-Uhr-Menue, RTC-Plausibilitaetspruefung und Web-Endpunkt `/set-time` verwenden denselben Bereich.
- SD-/Log-/Dateifilter-Plausibilitaeten bleiben unveraendert, damit bestehende Dateien und andere Zeitstempel weiterhin robust gelesen werden.

## 2026-06-30 - V0.50.0_34_TEST_WEB_DOWNLOAD_DATE_FILTER

- Basis: `Taupunktspiegel_StandV1_2026-06-30_V0.50.0_33_TEST_CONTROL_PASSWORD_RELOCK.zip`.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- Web-Dateidownload: zusaetzlich zu `Alle`, `Messdaten` und `Seriellog` gibt es jetzt dynamische Filterleisten fuer `Jahr` und `Monat`.
- Die Jahres- und Monatsbuttons werden aus dem vorhandenen RAM-Dateiindex erzeugt; es wird dafuer kein zweiter SD-Verzeichnisscan gestartet.
- Angezeigt werden nur Jahre/Monate, fuer die im aktuellen Dateityp-Filter tatsaechlich Dateien vorhanden sind. Bei nur einem Jahr/Monat erscheint nur dieser eine Button.
- `/files.json` akzeptiert jetzt optional `year=YYYY` und `month=1..12` und liefert zusaetzlich `years[]` und `months[]` fuer die Web-Oberflaeche.
- GSL1680-Firmware und `AwesomeF080_40` bleiben in Flash/PROGMEM; Touch-Logik bleibt wie in V32/V33 in RAM1/ITCM.

## 2026-06-30 - V0.50.0_32_TEST_TOUCH_LOGIC_RAM1

## 2026-06-30 - V0.50.0_33_TEST_CONTROL_PASSWORD_RELOCK

- TFT: Regelparameter-Passwort wird beim Verlassen des Setups per EXIT wieder verriegelt; ungespeicherte geschuetzte Regelparameter werden dabei verworfen.
- TFT: `Einstellungen laden` und `Werksjustierung laden` verriegeln eine eventuell noch offene Regelparameter-Bearbeitung vor dem Laden.
- TFT: Nach erfolgreichem Entsperren wird die untere Meldungszeile nicht mehr dauerhaft mit `Regelparameter entsperrt` belegt; der Status steht nur noch oben in der Regelparameter-Liste.
- Web: Regelparameter-Entsperrung ist nur noch fuer die aktuelle Bearbeitungsseite aktiv. Beim Zurueck/Schliessen sowie nach Geraete-Speicher-Aktionen wird die gecachte Web-Entsperrung geloescht.
- Web-Setup-ETag auf SETUP-PH6 erhoeht, damit Browser die geaenderte Setup-Seite sicher neu laden.
- GSL1680-Firmware und `AwesomeF080_40` bleiben weiter in Flash/PROGMEM; Touch-Logik bleibt wie in V32 in RAM1/ITCM.

- Basis: `Taupunktspiegel_StandV1_2026-06-30_V0.50.0_31_TEST_GSL_FW_AWESOME40_FLASH_COMPILE_FIX.zip`.
- Geaendert: `TPtouchscreen.ino`, `CHANGELOG.md`.
- Touch-/Button-Bedienlogik wieder aus `FLASHMEM` in schnellen ITCM/RAM1-Code gelegt.
- Betroffen sind insbesondere `ReadButtons()`, `TXTtestButton()`, `menuBlockInputUntilTouchRelease()`, `menuExitRequested()`, `menuExitClearRequest()`, `menuButtonsResetAfterExit()` sowie die FAN-Button-Hilfsfunktionen in `TPtouchscreen.ino`.
- Die GSL1680-Firmware bleibt unveraendert in Flash/PROGMEM; auch `AwesomeF080_40` bleibt in Flash.
- Zweck: Bediengefuehl/Touch-Reaktion testen. Falls kein Vorteil mess- oder spuerbar ist, kann diese Aenderung spaeter wieder zurueck in `FLASHMEM`.

## 2026-06-30 - V0.50.0_31_TEST_GSL_FW_AWESOME40_FLASH_COMPILE_FIX

- Basis: `Taupunktspiegel_StandV1_2026-06-30_V0.50.0_30_TEST_GSL_FW_AWESOME40_FLASH.zip`.
- Geaendert: `fonts.c`, `CHANGELOG.md`.
- Compile-Fix fuer V0.50.0_30: `fonts.c` wird im Teensy/Arduino-Build als C-Datei kompiliert; dadurch war `PROGMEM` dort nicht definiert.
- `fonts.c` definiert `PROGMEM` nun lokal als Teensy/IMXRT-Flash-Section-Attribut, falls der Build es nicht bereits bereitstellt.
- Die RAM1-Entlastung durch `AwesomeF080_40` in Flash bleibt damit erhalten.
- Keine Aenderung an Web-/Menue-/Mess-/Regelungslogik.

# Changelog

## 2026-06-30 - V0.50.0_30_TEST_GSL_FW_AWESOME40_FLASH

- **RAM1-Optimierung Test:** Der `code`-Platzhalter des originalen GSL1680-Firmware-Headers wird im TP-3000-Wrapper jetzt auf `PROGMEM` gemappt. Die Touch-Firmware `GSLX680_FW` soll damit aus RAM1 in Flash wandern; sie wird nur einmal beim Booten gelesen und per I2C in den GSL1680 geladen.
- **Font-Test:** `AwesomeF080_40` inklusive Daten- und Index-Tabelle liegt testweise in `PROGMEM`. Dieser Font wird nur fuer die grossen Touch-Icons OK/UP/DOWN benutzt und soll nicht mehr dauerhaft RAM1 belegen.
- Keine Aenderung an Web-/Menue-Logik oder Mess-/Regelungsfunktionen.

## 2026-06-30 - V0.50.0_29_TEST_WEB_STORAGE_COMPILE_FIX

- **Compile-Fix:** In `TPethernet.ino` war in der Endpunkt-Uebersicht eine Kommentarzeile fuer `/setup-device-status` und `/setup-device-storage` ohne `//` eingefuegt; dadurch entstand `expected unqualified-id before '/' token`. Die Zeile ist jetzt korrekt kommentiert.
- **TFT-Warnungen reduziert:** Die 5-stelligen Eingabemenues fuer Passwort/Kopf-SN geben einzelne Ziffern jetzt als nullterminierte 1-Zeichen-Strings an `TextBox::print(const char*)` aus, statt ein einzelnes `char` zu uebergeben.

# 2026-06-30 - V0.50.0_28_TEST_WEB_DEVICE_STORAGE_PASSWORD

- Web-Setup um `Geraete-Speicher` erweitert. Der Web-Punkt bildet das TFT-Menue fuer Speicherstatus, Einstellungen laden/speichern und Werksjustierung laden/speichern ab.
- Kritische Web-Aktionen im Geraete-Speicher verlangen jetzt das 5-stellige Geraete-SN-Passwort; `00000` ist wie bei Regelparametern erlaubt.
- Neue Web-Endpunkte: `/setup-device-status` fuer den Speicherstatus und `/setup-device-storage` fuer die passwortgeschuetzten Aktionen.
- Web-Regelparameter: Eingabebereich fuer `Peltier max` auf 0..4000 mA korrigiert; der Server liest den Wert ueber den 32-Bit-Parser, weil der alte Integerparser bei 3000 begrenzt.
- Web-Setup-ETag auf `SETUP-PH5` erhoeht.

# 2026-06-30 - V0.50.0_26_TEST_PELTIER_LIMIT_4A

- Kopfbezogenes Peltier-Stromlimit auf den geraeteintern zulaessigen Einstellbereich 0..4000 mA erweitert.
- Zentrale Grenzen in `TP_T.h` angepasst; TFT- und Web-Regelparameter sowie Kopfkalibrierdatei-Pruefung verwenden diese gemeinsamen Grenzen automatisch.
- Default bleibt 2500 mA, damit bestehende Profile/EEPROM-Werte nicht unnoetig hochgesetzt werden.
- Dynamische Ueberstrom-Schutzgrenze in der Regelung auf eine absolute Notgrenze von 4500 mA angepasst und Reihenfolge der Kappung korrigiert.


## 2026-06-30 - V0.50.0_25_TEST_WEB_HEAD_RULE_SETUP

- Web-Setup um `Sensorkopf` erweitert: Kopftyp/Kopf-SN setzen, Kopfdaten-Status anzeigen, SD-Kopfdaten laden und Kopfdaten speichern.
- Web-Kopfdaten laden scannt `/HEADCAL`, zeigt zuerst die vorhandenen Kopftypen fuer die aktuelle Geraete-SN und danach die vorhandenen Kopf-Seriennummern; geladen wird die neueste gueltige Datei mit CRC-Pruefung.
- Web-Kopfdaten speichern erzeugt eine neue Historien-Datei und verlangt wie am TFT das 5-stellige Seriennummern-Passwort.
- Web-Regelparameter an TFT-Logik angeglichen: gesperrte Uebersicht, `Werte aendern` mit Passwort, danach editierbare Werte.
- Peltier-Maxstrom ist im Web-Regelparameter-Menue sichtbar und editierbar und wird an `/setup-control` uebergeben.
- Neue Web-Endpunkte: `/setup-control-unlock`, `/setup-head-status`, `/setup-head-list.json`, `/setup-head-current`, `/setup-head-load`, `/setup-head-save`.
- Web-Setup-ETag auf `SETUP-PH4` erhoeht, damit Browser die neue Seite nicht aus dem Cache behalten.

## 2026-06-30 - V0.50.0_24_TEST_HEAD_CAL_SD_RULE_LIMIT

- Sensorkopf-Menue umgebaut: Kopftyp, 5-stellige Kopf-Seriennummer, Kopfdaten-Status, Kopfdaten laden und Kopfdaten speichern liegen jetzt zentral unter `Setup -> Sensorkopf`.
- Kopf-Seriennummer wird jetzt manuell als exakt 5-stellige Nummer eingegeben (`00000`..`99999`) statt ueber Dummy-Listen.
- Neue SD-Kopfkalibrierdateien unter `/HEADCAL` mit Historien-Dateiname `STP300x_K12345_G54321_YYYYMMDD_HHMMSS.CAL`; beim Speichern wird immer eine neue Datei erzeugt.
- Kopfdateien enthalten Geraete-SN, Kopftyp, Kopf-SN, Kalibrierdatum/-zeit, Pt100-R0, Pt100-2-Punkt-Werte, Taupunkt-Offset, PID-/Regelparameter, Optik-Sollwert, Luefterwert und Peltier-Stromlimit.
- CRC32 steht als letzte Zeile `crc32=...`; beim Laden wird die Datei nur bei passendem Geraet, passendem Kopf und gueltiger CRC uebernommen.
- Kopfdaten laden durchsucht die SD-Karte, zeigt zuerst vorhandene Kopftypen und danach die dazu vorhandenen Kopf-Seriennummern; geladen wird die neueste gueltige Datei fuer Kopftyp + Kopf-SN + Geraete-SN.
- Kopfbezogenes Peltier-Stromlimit eingefuehrt, separat im EEPROM gespeichert und in der Regelung als Strom-Sollwertdeckel verwendet; zusaetzlich dynamischer Ueberstromschutz relativ zum Kopf-Limit.
- Regelparameter-Menue mit Seriennummern-Passwort geschuetzt: im gesperrten Zustand nur Anzeige, `Werte aendern` oeffnet eine 5-stellige Passworteingabe; `00000` ist fuer Inbetriebnahme erlaubt.
- `Einstellungen laden` bewahrt kopfspezifische Kalibrier-/Regelwerte, damit normale Geraeteeinstellungen keine geladene Kopfkalibrierung ueberschreiben.


## 2026-06-29 - V0.50.0_23_TEST_DEVICE_SERIAL_5DIGIT

- Geraete-Seriennummer auf exakt 5 numerische Stellen festgelegt (`00000`..`99999`).
- Startbildschirm und Geraete-Speicher-Status verwenden die normalisierte Geraete-SN.
- Alte Textwerte oder defekte EEPROM-Inhalte im Feld `R.geraete_name` werden beim Start automatisch auf `00000` zurueckgesetzt und im EEPROM korrigiert.
- USB-Kommando `sleepmsg=` bleibt aus Kompatibilitaetsgruenden erhalten, akzeptiert jetzt aber nur noch exakt 5 Ziffern als Geraete-SN.

# TP-3000 Changelog

## 2026-07-01 - V0.50.0_18_TEST_TFT_UMLAUT_CASE_FIX

- Basis: `Taupunktspiegel_StandV1_2026-07-01_V0.50.0_17_TEST_TFT_MENU_TEXT_FLICKER_FIX.zip`.
- Schreibweise der Umlaut-Ersatzzeichen korrigiert: bei normal großgeschriebenen Worten jetzt `Aenderungen`/`Uebersicht`, komplett großgeschriebene Wörter bleiben `AENDERUNGEN`/`UEBERSICHT`.
- Sichtbare TFT-Texte korrigiert: `AEnderungen verwerfen` -> `Aenderungen verwerfen`, `Diag 1 UEbersicht` -> `Diag 1 Uebersicht`.

Geänderte Dateien:
- `TPlanguage.h`
- `TPmenu_Pages.ino`
- `CHANGELOG.md`

## 2026-07-01 - V0.50.0_17_TEST_TFT_MENU_TEXT_FLICKER_FIX

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_16.zip`.
- TFT-Menütexte in den betroffenen Setup-/Kopf-/SD-/Geräte-Speicher-Seiten auf kleine Umlaute umgestellt; Umlaut-Ersatzzeichen werden bei normal großgeschriebenen Worten klein weitergeführt (`Aenderungen`, `Uebersicht`), bei vollständig großgeschriebenen Worten komplett groß (`AENDERUNGEN`, `UEBERSICHT`).
- Kopfdaten-Speichern/Laden zeigt auf dem TFT keine langen HEADCAL-Dateinamen mehr, sondern kurze Meldungen wie `Gespeichert: STP-3001 K01234` bzw. `Geladen: STP-3001 K01234`.
- Fehler in `head_cal_status_menu()` behoben: ENTER/EXIT werden wieder eingelesen, und die SD-Kopfdatenliste wird nur beim Zeichnen der Seite abgefragt.
- Button-Flimmern in SD-Kartenstatus, Geräte-Speicher und Sensorkopf-Untermenüs reduziert: Setup-Buttons werden dort nur noch bei Bedarf gezeichnet statt bei jedem Seitenaufbau erzwungen.

Geänderte Dateien:
- `TPlanguage.h`
- `TPheadCalibration.ino`
- `TPmenu_Interface.ino`
- `TPmenu_Pages.ino`
- `TPmenu_RefExtCal.ino`
- `TPmenu_EditPages.ino`
- `CHANGELOG.md`


## 2026-06-29 - V0.50.0_22_TEST_BOOT_DEVICE_SERIAL

- Startbildschirm zeigt in der zweiten Informationszeile jetzt die Geraete-Seriennummer bzw. das Geraetefeld statt des Firmware-Datums.
- Das Firmware-Datum bleibt im Code/Info- und Versionskontext erhalten, wird aber beim Start nicht mehr an dieser Stelle angezeigt.
- Zweck: Die fuer Service-/Speicherfunktionen benoetigte Geraete-Seriennummer ist direkt beim Einschalten sichtbar.

## 2026-06-29 - V0.50.0_21_TEST_DEVICE_STORAGE_ACTIVE

- Menue `Geraete-Speicher` mit realen Backup-/Restore-Funktionen gefuellt.
- Der alte Punkt `Werkseinstellung Reset` wurde aus dem normalen Menue entfernt.
- `Einstellungen speichern/laden` sichert und laedt jetzt einen separaten EEPROM-Backupblock fuer normale Geraeteeinstellungen:
  Hauptparameterblock `R`, Sprache, ADC/Pt100-Messfilter, ADC1-SFOCAL sowie Schnittstellen- und Alarmkonfiguration.
- `Werksjustierung speichern/laden` sichert und laedt bewusst nur die Grundgeraete-Justierung: Ref Low, Ref High und die Kanal-Korrekturwerte A/B Low/High.
- Kopfdaten, Pt100-R0, Pt100-2-Punkt-Werte und Taupunkt-Offset werden nicht als Grundgeraete-Werksjustierung gespeichert; diese Werte bleiben fuer die spaetere geraete-/kopfspezifische Kopfdatei vorgesehen.
- Speicherstatus zeigt an, ob ein Einstellungsbackup und eine Werksjustierung mit gueltiger CRC vorhanden sind.

## V0.50.0_20_TEST_DEVICE_STORAGE_MENU - 2026-06-29

- Basis: `Taupunktspiegel_StandV1_2026-06-29_V0.50.0_19_TEST_REF_RATIO_DYNAMIC.zip`.
- Geaendert: `TPmenu.ino`, `TPmenu_Pages.ino`, `TPlanguage.h`, `ads1263.ino`, `CHANGELOG.md`.
- Neuer TFT-Menuepunkt `Geraete-Speicher` als Sammelmenue fuer Laden/Speichern vorbereitet.
- Unterpunkte: `Speicherstatus`, `Einstellungen laden`, `Werksjustierung laden`, `Einstellungen speichern`, `Werksjustierung speichern`, `Werkseinstellung Reset`, `Zurueck`.
- Der bisherige Werksreset ist nicht mehr direkt im Hauptmenue, sondern unter `Geraete-Speicher -> Werkseinstellung Reset` erreichbar.
- Speicher-/Werksjustierungsfunktionen sind in diesem Teststand bewusst nur als Menuegeruest/Hinweisseiten vorbereitet und aendern noch keine EEPROM- oder Kalibrierdaten.
- Statusseite zeigt aktuelles Geraetefeld, Sensorkopf-Typ und Kopf-Seriennummer sowie die geplante Trennung: Werksjustierung = Grundgeraet, Kopfdaten spaeter von SD.
- ADS1263-Ref-Ratio-Toleranz aus dem Nutzerstand uebernommen: `ADS1263_REF_RATIO_EXPECTED_TOL_FRAC` = 0.01 (+/-1 % um Ref High / Ref Low).

## V0.50.0_19_TEST_REF_RATIO_DYNAMIC - 2026-06-29

- Basis: `Taupunktspiegel_StandV1_2026-06-29_V0.50.0_18_TEST_REF_LOW_HIGH_RANGE.zip`.
- Geaendert: `ads1263.ino`, `CHANGELOG.md`.
- ADS1263-Ref-Paar-Ratio-Plausibilitaet wird jetzt dynamisch aus den gespeicherten Ref-Low-/Ref-High-Werten abgeleitet (`Ref High / Ref Low`) statt nur ueber eine breite feste Ratio-Spanne zu pruefen.
- Erlaubtes Raw-Ratio-Fenster: Sollratio +/-5 %, mit Sicherheitsclamp 1.02 ... 2.70 und breitem Fallback nur bei unplausiblen Kalibrierwerten.
- Bei Ratio-Verwurf gibt die serielle Diagnose zusaetzlich Sollratio und erlaubtes Fenster aus.

## V0.50.0_18_TEST_REF_LOW_HIGH_RANGE - 2026-06-29

- Basis: `Taupunktspiegel_StandV1_2026-06-29_V0.50.0_17_TEST_CHART_RANGE_WEB_REMEMBER.zip`.
- Geaendert: `Ref100_120_Calibration.ino`, `TPmenu_CalibrationPages.ino`, `TPmenu_RefExtCal.ino`, `TPlanguage.h`, `TPethernet.ino`, `TPdisplay.ino`, `ads1263.ino`, `CHANGELOG.md`.
- Referenzbezeichnungen in TFT/Web/Diagnose von `Ref100`/`Ref120` auf `Ref Low`/`Ref High` umgestellt, interne Funktions-/Variablennamen bleiben zur Risikominimierung unveraendert.
- Eingabebereich erweitert: Ref Low = 60.00000 ... 110.00000 Ohm, Ref High = 115.00000 ... 152.00000 Ohm, Mindestspreizung weiterhin 5.00000 Ohm.
- Kanal-Restkorrektur interpoliert nun zwischen den tatsaechlich gespeicherten Ref-Low-/Ref-High-Werten statt fest zwischen 100 und 120 Ohm.
- ADS1263-Ref-Paar-Plausibilitaet und grobe Pt100-Ohm-Plausibilitaet an variable Ref-Low-/Ref-High-Werte angepasst.
- Web-Hauptseiten-ETag von `CHR4` auf `CHR5` erhoeht, damit Browser das aktualisierte JavaScript sicher neu laden.

## V0.50.0_17_TEST_CHART_RANGE_WEB_REMEMBER - 2026-06-29

- Basis: `Taupunktspiegel_StandV1_2026-06-29_V0.50.0_16_TEST_CHART_AXIS_LABELS.zip`.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- Web-Chart merkt sich den zuletzt gewaehlt sichtbaren Zeitraum im Browser per `localStorage`.
- Beim Ruecksprung von der Dateiseite oder nach Neuladen der Hauptseite bleibt z. B. 4 h / 8 h / 24 h erhalten, statt wieder auf 60 min zurueckzufallen.
- Web-Hauptseiten-ETag von `CHR3` auf `CHR4` erhoeht, damit Browser das aktualisierte JavaScript sicher neu laden.
- TFT-Chart unveraendert.

## V0.50.0_16_TEST_CHART_AXIS_LABELS - 2026-06-29

- Basis: `Taupunktspiegel_StandV1_2026-06-29_V0.50.0_15_TEST_CHART_RANGE_WEB_FIX.zip`.
- Web- und TFT-Chart: X-Achsenbeschriftung der langen Zeitfenster vereinheitlicht.
- 4 h, 8 h und 24 h zeigen an der X-Achse nur noch Zahlen ohne `h`, passend zur bisherigen 60-min-Anzeige.
- Web-Hauptseiten-ETag von `CHR2` auf `CHR3` erhoeht, damit Browser das aktualisierte JavaScript sicher neu laden.

## V0.50.0_15_TEST_CHART_RANGE_WEB_FIX - 2026-06-29

- Basis: `Taupunktspiegel_StandV1_2026-06-29_V0.50.0_14_TEST_CHART_RANGE_SELECT.zip`.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- Web-Chart-Umschaltung robuster gemacht: laufende alte `/chart.json`-Antworten werden nach einem Zeitraumwechsel nicht mehr in den neuen Chart-Zustand uebernommen oder als Leerzustand gespeichert.
- Beim Umschalten wird nun eine Chart-Generation mitgefuehrt; veraltete Antworten werden verworfen und anschliessend sofort ein neuer Abruf fuer den aktuell gewaehlt sichtbaren Zeitraum geplant.
- ETag der Web-Hauptseite auf `CHR2` erhoeht, damit Browser nach dem Update sicher die neue JavaScript-Version laden.
- TFT-Chart und Messwerterfassung unveraendert.

## V0.50.0_14_TEST_CHART_RANGE_SELECT - 2026-06-29

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_13.zip`.
- Geaendert: `TPdisplay.ino`, `TPethernet.ino`, `TP-3000.ino`, `CHANGELOG.md`.
- Hauptchart-Zeitraum erweitert: Umschaltung 60 min / 4 h / 8 h / 24 h.
- Jeder Zeitraum besitzt weiterhin 1440 Punkte; Abtastung: 60 min = 2,5 s, 4 h = 10 s, 8 h = 20 s, 24 h = 60 s.
- TFT: Tippen auf die Zeitangabe rechts oben im Chart schaltet den Zeitraum weiter; Tippen auf den restlichen Rahmen schaltet weiterhin Zahlen / Feuchte / Taupunkt.
- Web: Gleiche Zeitraum-Umschaltung ueber die Zeitangabe im Chart; `/chart.json` liefert nur den aktuell gewaehlt angeforderten Zeitraum.
- X-Achse: 60 min unveraendert, 4 h mit Stundenmarken, 8 h mit 2-h-Marken, 24 h mit 4-h-Marken.

## V0.50.0_15_TEST_FLASHMEM_UI_CONFIG - 2026-06-28

- Basis: `Taupunktspiegel_StandV1_2026-06-28_V0.50.0_14_TEST_ADC1_SFOCAL_MENU_WARNFIX.zip`.
- Geaendert: `TPtouchscreen.ino`, `TPusbSerial.ino`, `TPopticHealth.ino`, `I2C2_Peripherie.ino`, `TPalarm.ino`, `PT100_2P_Calibration.ino`, `Ref100_120_Calibration.ino`, `CHANGELOG.md`.
- Vorsichtige RAM1/ITCM-Entlastung: UI-, USB-, Optikdiagnose-, RTC/Drucksensor- und EEPROM-/Kalibrier-Konfigfunktionen mit `FLASHMEM` markiert.
- Nicht verschoben: ADS1263-ISR/ADC1-State-Machine, Pt100-Messpfad, Regelung/PWM, Safety-Task, Watchdog-Feed, SD-Logging und Ethernet-Download-Service.
- Messpfadnahe Getter/Korrekturfunktionen wie `pt100Cal2Apply()`, `pt100R0GetOhm()`, `taupunktOffsetGetC()`, `refCalGet100Ohm()`, `refCalGet120Ohm()` und `refCalApplyChannelCorrection()` bleiben bewusst im schnellen RAM1/ITCM.
- Keine funktionale Aenderung an ADC1-SFOCAL-Menue, Web-Menue, Messung, Regelung, Safety oder SD-Logging.

## V0.50.0_14_TEST_ADC1_SFOCAL_MENU_WARNFIX - 2026-06-28

- Basis: `Taupunktspiegel_StandV1_2026-06-28_V0.50.0_13_TEST_ADC1_SFOCAL_MENU.zip`.
- Geaendert: `TPmenu_Interface.ino`, `CHANGELOG.md`.
- Compiler-Warnung in `interfaceIpMenu()` beseitigt: Die IP-Anzeige kopiert `lcd_buf` nun begrenzt in den 24-Byte-Puffer `last_value`.
- Externe SparkFun/Bosch-BMP581-Library bleibt unveraendert; die dortige FIFO-Warnung wird bewusst nicht gepatcht.
- Keine funktionale Aenderung an ADC1-SFOCAL, Web-Menue, Messung, Regelung, Safety oder SD-Logging.

## V0.50.0_13_TEST_ADC1_SFOCAL_MENU - 2026-06-28

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_12.zip`.
- ADC1/ADS1263-SFOCAL ist nicht mehr fest alle 5 Minuten verdrahtet.
- Neuer Menuepunkt unter `Regelparameter`: `ADC1 SFOCAL`.
- Optionen: `Nur beim Start`, `Synchron mit Auto-Cal`, `Alle 10 min`, `Alle 30 min`, `Alle 60 min`.
- Default fuer neue/ungueltige EEPROM-Werte: `Synchron mit Auto-Cal`.
- Im Modus `Synchron mit Auto-Cal` wird SFOCAL beim Eintritt in Freiheizen/Auto-Cal ausgefuehrt, also in einem ohnehin markierten Sonderzustand.
- In den zyklischen Modi 10/30/60 min wird SFOCAL weiterhin nur bei ruhiger Regelung ausgefuehrt.
- Web-Setup erweitert: `ADC1 SFOCAL` wird im Regelparameter-Formular angezeigt und gespeichert.
- SFOCAL-Ablauf selbst unveraendert: ADC1-INPMUX `0xFF`, SFOCAL, 300 ms Wartezeit, danach ADC1-State-Machine neu starten.

## V0.50.0_14_TEST_DOWNLOAD_START_2_BLOCKS - 2026-06-28

## V0.50.0_15_TEST_DOWNLOAD_STALE_CLEANUP

- Basis: `V0.50.0_14_TEST_DOWNLOAD_START_2_BLOCKS`.
- Datei-Download bereinigt halb offene WLAN-/Browser-Verbindungen jetzt konsequenter:
  - bei normalem Download-Ende wird der TCP-Client nach dem Dateischluss per `client.stop()` freigegeben,
  - bei Download-Abbruch/Stall wird die Datei geschlossen, der TCP-Client gestoppt und der interne Download-Status geloescht,
  - die 15-s-Grenze bleibt ein No-Progress-Timeout, keine maximale Download-Dauer.
- Download-Parameter bleiben unveraendert: 1/2/3/4 x 512 Byte, Start mit 2 x 512 Byte, 2 ms Zeitbudget.
- Watchdog-Feed weiterhin ausschliesslich am Ende einer vollstaendig durchlaufenen Hauptloop.

- Web-/SD-Dateidownload: Startstufe von 1 x 512 B auf 2 x 512 B pro Service-Aufruf angehoben.
- Adaptive Download-Stufen bleiben 1/2/3/4 x 512 B, Zeitbudget 2 ms, Ramp-Schwellen aus `_13` bleiben erhalten.
- Watchdog-Strategie unverändert: kein Feed im Downloadpfad; Feed nur am Ende einer vollständig durchlaufenen Hauptloop.

## V0.50.0_13_TEST_DOWNLOAD_RAMP_RELAXED - 2026-06-28

- Web-/SD-Dateidownload: adaptive Hochregelung entschaerft, damit WLAN-Downloads nicht dauerhaft auf 1 x 512 Byte pro Loop haengen bleiben.
- Downloadstufen bleiben 1, 2, 3 und 4 SD-Sektoren pro Serviceaufruf.
- TCP-Schreibchunk bleibt 512 Byte, Zeitbudget bleibt 2 ms.
- Ramp-Up jetzt nach 30 guten Serviceaufrufen statt 200.
- Slow-/Very-Slow-Schwellen auf 3 ms / 8 ms angehoben; Stall-Timeout wieder 15 s.
- Watchdog-Feed-Strategie bleibt unveraendert: kein Feed im Downloadpfad, sondern nur nach vollstaendig durchlaufener Hauptloop.

## V0.50.0_12_TEST_DOWNLOAD_512_1_4 - 2026-06-28

- Web-/SD-Dateidownload wieder beschleunigt: adaptive Downloadstufen jetzt 1, 2, 3 oder 4 SD-Sektoren pro Serviceaufruf.
- TCP-Schreibchunk wieder auf 512 Byte gesetzt.
- Download-Zeitbudget pro Serviceaufruf auf 2 ms gesetzt.
- Watchdog-Feed-Strategie bleibt unveraendert: kein Feed im Downloadpfad, sondern nur nach vollstaendig durchlaufener Hauptloop.

## 2026-06-28 - V0.50.0_18_TEST_WEB_MAIN_RH_UNIT_WEIGHT

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_17_TEST_WEB_MAIN_RH_UNIT_REFINE.zip`.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- Web-Hauptanzeige: Prozentzeichen der grossen `%rH`-Einheit wieder kleiner wie in `_16`, vertikale Position aus `_17` beibehalten.
- Prozentzeichen nur optisch fetter gesetzt (`font-weight:700`); Chart und Setup bleiben unveraendert bei normaler `%rH`-Darstellung.


## 2026-06-28 - V0.50.0_17_TEST_WEB_MAIN_RH_UNIT_REFINE

- Web-Hauptanzeige: Position und Groesse des Prozentzeichens in der grossen `%rH`-Einheit nachjustiert.
  - Prozentzeichen etwas groesser als in `_16`.
  - Vertikale Absenkung gegenueber `_16` halbiert.
- Web-Chart und Web-Setup bleiben unveraendert bei normaler `%rH`-Darstellung.

## 2026-06-28 - V0.50.0_16_TEST_WEB_MAIN_RH_UNIT_ALIGN

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_14_TEST_WDT_LOOP_FEED_RESTORE.zip`.
- Geaendert: `TPethernet.ino`, `CHANGELOG.md`.
- Web-Hauptanzeige: Nur das Prozentzeichen der grossen `%rH`-Einheit wird kleiner dargestellt und optisch nach unten auf die Grundlinie von `rH` gesetzt.
- Web-Chart, Web-Setup und alle anderen `%rH`-Einheiten bleiben unveraendert mit normaler Darstellung.
- Die versehentliche TFT-Hauptanzeigen-Aenderung aus `_15_TEST_MAIN_RH_UNIT_ALIGN` ist nicht enthalten.



## 2026-06-28 - V0.50.0_14_TEST_WDT_LOOP_FEED_RESTORE

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_13_TEST_SD_FLUSH_TOTAL_DIAG.zip`.
- Geaendert: `TP-3000.ino`, `CHANGELOG.md`.
- Watchdog-Feed-Drossel aus `_12` wieder entfernt: Wenn die Hauptloop sauber bis zum Ende durchlaeuft und der Loop-Guard korrekt verlassen wird, wird das WDT-Register wieder bei jedem vollstaendigen Loopdurchlauf gefuettert.
- Die zentrale Sicherheitslogik bleibt unveraendert: Download-/SD-/Ethernet-Pfade duerfen den Watchdog weiterhin niemals selbst fuettern; bei blockierter Hauptloop erfolgt nach dem Watchdog-Timeout ein Reset.
- SD-Flush-Gesamtzeitdiagnose aus `_13` bleibt unveraendert erhalten.

## 2026-06-28 - V0.50.0_13_TEST_SD_FLUSH_TOTAL_DIAG

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_12_TEST_WDT_FEED_THROTTLE.zip`.
- Geaendert: `TPsdLog.ino`, `CHANGELOG.md`.
- Ergaenzt eine automatische SD-Gesamtzeitdiagnose fuer komplette CSV-Flushes.
- Neue CSV-Spalten direkt nach `SdMax_us`:
  - `SdCsvFlushLastTotal_us`
  - `SdCsvFlushMaxTotal_us`
  - `SdCsvFlushLastSamples`
  - `SdCsvFlushMaxSamples`
  - `SdCsvFlushCount`
- Zweck: Die regelmaessigen Loop-Peaks um 300 ms muessen nicht mehr per Auge zugeordnet werden. Die Einzelstages `CSV_OPEN/WRITE/FLUSH/CLOSE` bleiben erhalten, zusaetzlich zeigt die Gesamtzeit, ob der komplette 5-min-SD-Schreibblock der Verursacher ist.
- Keine funktionale Aenderung am Mess-, Regler-, Download- oder Watchdogpfad.


## V0.50.0_12_TEST_WDT_FEED_THROTTLE

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_11_TEST_DOWNLOAD_LOOP_GUARD.zip`.
- Hardware-Watchdog bleibt weiterhin nur am Ende einer sauber durchlaufenen Hauptloop erlaubt.
- Das eigentliche Watchdog-Register-Feed wird jetzt auf maximal einmal pro ca. 500 ms begrenzt, statt bei jedem Loopdurchlauf zu schreiben. Ziel: digitale Zusatzlast/Jitter bei reiner Webansicht minimieren, ohne die 8-s-Notbremse gegen echte Blockaden aufzugeben.

## V0.50.0_11_TEST_DOWNLOAD_LOOP_GUARD

- Basis: `Taupunktspiegel_StandV1_2026-06-20_V0.50.0_10.zip`.
- Hardware-Watchdog-Gate verschärft: Der Watchdog wird ausschließlich nach einer vollständig durchlaufenen Hauptloop gefüttert. Ein Loop-Guard markiert Eintritt/Austritt; bei nicht sauberem Loop-Zustand wird nicht mehr gefüttert.
- WDT_T4 wird bevorzugt aus der im Quellpaket enthaltenen lokalen Bibliothekskopie eingebunden, damit der Watchdog nicht versehentlich deaktiviert bleibt, wenn die Bibliothek nicht separat in der Arduino-IDE installiert ist.
- Web-/SD-Download für WLAN-Stresstests defensiver eingestellt: kleineres Zeitbudget pro Hauptloop, 256-Byte-TCP-Teilstücke, langsameres Hochrampen und 3-s-No-Progress-Abbruch. Ein langsamer oder abbrechender Browser darf den Download verlieren, aber nicht Touch, Anzeige, Logging oder Regelung festhalten.

## 2026-06-27 - V0.50.0_9_1_TEST_PWM_DITHER

- Peltier-PWM-Dithering von festem 4-Phasen-Raster auf einen Fractional-Accumulator umgestellt.
- Der PWM-Mittelwert bildet Nachkommaanteile jetzt langfristig genauer ab; kleine Sollwerte werden nicht mehr automatisch auf 0,25 PWM-Tick aufgerundet.
- Das gemessene Peltier-PWM-Limit `PELTIER_PWM_LIMIT = 2700` bleibt unverändert.
- Reglerparameter, Stromregelung, Safety und DRV8873-Konfiguration bleiben unverändert.

## 2026-06-26 - V0.50.0_13_TEST_WINCONTROL_WEB_PORT_FIX

## 2026-06-26 - V0.50.0_14_TEST_WINCONTROL_WEB_FETCH_FIX

- Fix Web-Setup WinControl: Beim Speichern der WinControl-Einstellungen wird Ethernet nicht mehr synchron im laufenden HTTP-Request neu initialisiert.
- Ursache: `ethernetServiceApplyNow()` in `interfaceWebSetWinControl()` konnte die aktive Browser-Verbindung abbrechen, besonders bei Auswahl/Änderung der Ethernet-Ausgabe.
- Der WinControl-raw-TCP-Server übernimmt Port-/Aktiv-Änderungen weiterhin nicht-blockierend über `winControlOutEthernetTask()`.


- Web-Setup: Speichern der WinControl-Ausgabe korrigiert.
- Ursache: Der bestehende Web-Integerparser begrenzt normale Parameter auf 3000. Der WinControl-TCP-Port 10001 wurde dadurch als ungültig verworfen.
- Lösung: Der Parameter `p` der Route `/setup-wincontrol` wird nun mit dem vorhandenen 32-Bit-Parser gelesen und anschließend sauber auf 1..65535 geprüft.
- Keine Änderung an TFT-Menü, Protokoll, RS232-/Ethernet-Funktion oder Lizenztexten.

## 2026-06-26 - V0.50.0_12_TEST_WINCONTROL_LICENSE_NOTICE

- Lizenz-/Markenhinweise für die neue WinControl-Ausgabe ergänzt.
- `README.md`, `THIRD_PARTY_NOTICES.md`, `ATTRIBUTION_AUDIT.md` und
  `LICENSES/ALMEMO-TRADEMARK-NOTICE.txt` nennen jetzt neben der optionalen
  ALMEMO-Anbindung auch die ALMEMO-kompatible V6-WinControl-Ausgabe über
  RS232 oder Ethernet/TCP.
- TFT-Seite `Lizenzen / Marken` und Web-Seite `Licenses / Trademarks` um
  WinControl-/Interoperabilitäts-Hinweis ergänzt.

# TP-3000 Changelog



## 2026-06-26 - V0.50.0_11_TEST_WINCONTROL_EXCLUSIVE_OUTPUT

- WinControl-Ausgabe im TFT- und Web-Menü auf exklusive Ausgabearten reduziert: `Aus`, `RS232-1`, `RS232-2`, `Ethernet`.
- Kombinationsmodi `RS232-1+Eth` und `RS232-2+Eth` entfernt, weil ein paralleles Abfragen durch zwei WinControl-Instanzen nicht sinnvoll ist.
- Bestehende Alt-Konfigurationen mit gleichzeitig aktivem RS232-WinControl und Ethernet-WinControl werden beim Laden/Speichern automatisch auf die RS232-Ausgabe zurückgeführt.

## V0.50.0_10_TEST_WINCONTROL_WEB_SETUP

- Web-Setup um `Schnittstellen -> WinControl-Ausgabe` erweitert.
- Web-Bedienung fuer Ausgabeart, Baudrate, Adresse, TCP-Port, Zyklus und Status ergaenzt.
- `/setup-wincontrol` als Web-Setup-Endpunkt ergaenzt.
- `setup.json` und `setup-live.json` liefern WinControl-Status inklusive TCP-Clientstatus.


## V0.50.0_9_TEST_WINCONTROL_RS232_ETH_V6_OUT

- Neue optionale WinControl-Ausgabe als ALMEMO-kompatible V6-Messbox ergänzt.
- RS232-1, RS232-2 und raw TCP/Ethernet verwenden denselben ASCII-Kommandohandler.
- Menüpunkt `Setup -> Schnittstellen -> WinControl-Ausgabe` mit Ausgabeart, Baudrate, Adresse, TCP-Port, Zyklus und Status ergänzt.
- Unterstützt den beobachteten AMR-/WinControl-Befehlssatz für Erkennung, Programmkopf, Messwerte, zyklische Ausgabe und `f2+t0` Seriennummernantwort aus der Kopfnummer.


## V0.50.0_13_TEST_FAT_TIMESTAMP_SOURCE_FIX

- Ursache korrigiert: Nach jeder gültigen RV-3129/TimeLib-Synchronisation wird jetzt auch `Teensy3Clock` gesetzt, weil die Teensy-SD-Library ihre FAT-Zeitstempel aus `Teensy3Clock` erzeugt.
- CSV-Dateien setzen beim Flush den FAT-Modify-Zeitstempel zusätzlich explizit aus der plausiblen TimeLib/RTC-Zeit.
- Bei neu angelegten CSV-Dateien wird auch die FAT-Erstellzeit gesetzt; bei bereits fehlerhaften Create-Zeiten wird sie nachgezogen.
- ALMEMO-Seriellog-Dateien erhalten beim Anlegen, periodischen Flush und Schließen ebenfalls explizite FAT-Zeitstempel.
- Die Web-Fallback-Korrektur aus _12 bleibt als Altdatei-/Sicherheitsnetz erhalten.
## 0.50.0 - 2026-06-20

- Umstellung des gesamten Projektstands auf die Versionsnummer **0.50.0**.
- Ablösung der bisherigen internen Vxx-Arbeitsnummern durch eine einheitliche
  dreiteilige Versionsnummer (`MAJOR.MINOR.PATCH`).
- Firmwaredatum auf **20.06.2026** gesetzt.
- Setup-Menüpunkt in **Lizenzen / Marken** beziehungsweise
  **Licenses / Trademarks** umbenannt.
- Lizenz-/Markenseite scrollbar gemacht; UP und DOWN bewegen den Text jeweils
  um zwei Zeilen.
- Vollständigen Hinweis zur optionalen seriellen ALMEMO-Anbindung und zur Marke
  in die Anzeige und in die Paketdokumentation aufgenommen.
- Web-ETags auf 0.50.0 umgestellt, damit Browser keine älteren Seiten aus dem
  Cache weiterverwenden.
- Die GSL1680-Panel-Firmware bleibt in diesem internen Arbeitsstand unverändert
  enthalten. Vor jeder öffentlichen Veröffentlichung muss sie entfernt und das
  Paket erneut vollständig geprüft werden.

Die Umstellung der Versionsbezeichnung verändert Messung, Regelung, Safety,
Kalibrierung, Logging und Kommunikationsprotokolle nicht.


## 0.50.0_10 TEST - 2026-06-22

- RV3129-Bibliothek im Header und in der Source als TP-3000-Patch dokumentiert.
- THIRD_PARTY_NOTICES, BUILDING, README, SOURCE_PACKAGE, libraries/README und
  Attribution-Audit um die optionale WDT_T4/Watchdog_t4-Abhängigkeit ergänzt.
- Keine funktionale Firmwareänderung gegenüber V0.50.0_9; nur Dokumentation,
  Lizenz-/Attributionshinweise und Header-Kommentare.

## 2026-06-30 - V0.50.0_27_TEST_PELTIER_DYNAMIC_SAFETY

- Peltier-Überstrom-Safety an das kopfbezogene Regelparameter-Limit gekoppelt.
- Der Regler bleibt hart auf den eingegebenen Peltier-Maxstrom begrenzt.
- Die Safety-Grenze berechnet sich nun dynamisch aus `Limit + max(5 %, 80 mA)`.
- Die frühere absolute 4500-mA-Grenze wurde entfernt, weil der Peltierstrom nur bis 4096 mA messbar ist.
- Die dynamische Safety-Grenze wird vor Messbereichssättigung auf 4080 mA gedeckelt.
- Auslösung erfolgt jetzt inklusive Grenzwert (`>=`), damit der Sättigungsbereich sicher erfasst wird.

Geänderte Dateien:
- `Taupunkt_Regelung.ino`
- `CHANGELOG.md`

## V0.50.1_12 TEST - zertifizierte Geräte-SN vollständig schreibgeschützt
- Bei gültigem Gerätezertifikat wird beim Speichern der Werksjustierung keine neue Geräte-SN mehr abgefragt.
- TFT: Nach Passwortprüfung wird direkt unter der zertifizierten SN gespeichert; die Seite zeigt nur noch die zertifizierte SN als Status.
- Web-Setup: Das editierbare SN-Feld entfällt bei gültigem Zertifikat.
- Server-Schutz: Auch alte/gecachete Web-Seiten können keine andere SN übermitteln; der Backendpfad erzwingt die wirksame Zertifikats-SN.
- Ohne gültiges Zertifikat bleibt der bisherige Produktionsablauf mit Eingabe einer neuen Geräte-SN unverändert.

## V0.50.1_13 TEST - Log-Integritaet und signierte Datenformate vorbereitet
- Neuer SD-Menuepunkt `Log-Integritaet` mit `Aus`, `SHA-256` und `Zertifiziert`.
- Auswahl wird im redundanten Interface-EEPROM gespeichert; V11-Payloads werden ohne Groessenaenderung sicher auf V12 migriert und starten mit `Aus`.
- Web-Setup zeigt dieselbe Auswahl. Der zertifizierte Modus bleibt solange gesperrt, bis Geraetezertifikat, signierte Geraetejustierung, signierte Kopfkalibrierung und Firmwarefreigabe gemeinsam gueltig sind.
- Alte/gecachete Web-Setup-Seiten ohne den neuen Parameter bleiben kompatibel und veraendern den gespeicherten Modus nicht.
- Neue gemeinsame Firmware-Grundlage `TPsignedData.h/.cpp` mit Formatkennungen V1, kanonischem Little-Endian-Writer, Firmwareidentitaet und stabilen Bereitschaftsschnittstellen fuer `.tpdcal`, `.tphcal`, `.tpfw` und `.TPSIG`.
- Firmwarestatus ist bewusst `DEVELOPMENT`; Secure Boot, Firmware-Signaturpflicht sowie die eigentliche `.sha256`-/`.TPSIG`-Erzeugung sind in diesem Vorbereitungsschritt noch nicht aktiviert.
- Verbindliche Byteformatbeschreibung als `SIGNED_DATA_FORMATS_V1.md` in das Quellpaket aufgenommen.

