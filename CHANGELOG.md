# TP-3000 Changelog

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
