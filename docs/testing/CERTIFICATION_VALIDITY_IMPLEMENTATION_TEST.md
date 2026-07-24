# TP-3000 V0.50.1_18 – Umsetzung und Prüfplan

## Umgesetzte Bedienstruktur

### Web-Hauptseite

- Der bisherige Button `Identität` heißt `Zertifizierung`.
- Rechts in der unteren Leiste steht der neue Button `Info`.
- `Info` öffnet `/validity`.

### Web-Seite Zertifizierung

- Seitentitel: `TP-3000 Zertifizierung`.
- Oben wird der aktuelle Fortschritt für Geräteidentität, Gerätejustierung, Kopfkalibrierung und Firmwarefreigabe angezeigt.
- Die vorhandenen Schritte Geräteschlüssel, Zertifikatsanfrage und Zertifikatsimport bleiben erhalten.
- Der Einstieg zur signierten Kalibrierung heißt `Kalibrierung verwalten`.

### Web-Seite Kalibrierung verwalten

- Gerätejustierung und Kopfkalibrierung zeigen Status, Kalibriertag, `Gültig ab` und `Gültig bis` direkt vor den Aktionen.
- Export und Import bleiben getrennt für Gerät und Kopf verfügbar.
- Ergebnis- und Fehlerseiten führen zurück zu `Kalibrierung verwalten`.

### Web-Seite Info / Gültigkeit

- Oben steht ausschließlich `← Hauptanzeige`.
- Gerät: Zertifikatstatus, Geräte-SN, Ausstellungsdatum.
- Geräte-Kalibrierung: Status, Kalibriertag, Gültigkeitszeitraum und IDs.
- Kopfzertifikat / Kopf-Kalibrierung: Status, Kopftyp, Kopf-SN, Kalibriertag, Gültigkeitszeitraum und IDs.
- Firmware: Status, Version, Build und Freigabe-/Buildtag; bewusst kein Ablaufdatum.
- Der Web-Setup-Eintrag `Info / Gültigkeit` öffnet dieselbe Seite.

### Web-Setup EXIT

Beim Öffnen der Info-Seite wird die Web-Setup-Sitzung geschlossen. Kehrt der Browser über seinen Seitencache zum Setup zurück, werden Zustand und Setupdaten über `pageshow` neu initialisiert. Der Button behält den Text `EXIT` und funktioniert danach wieder.

### TFT-Setup

- Neuer Menüpunkt `Info / Gültigkeit` direkt nach `Statusinformationen`.
- Reine Anzeige ohne Export, Import oder Bearbeitung.
- 23 Informationszeilen, 14 gleichzeitig sichtbar.
- UP/DOWN scrollt jeweils zwei Zeilen; ENTER/EXIT kehrt ins Setup zurück.
- Firmware besitzt auch dort kein `Gültig bis`.

## Farbregeln

- Grün: aktuell gültig.
- Gelb: fehlt, vorbereitet, noch nicht gültig oder zeitlich nicht beurteilbar.
- Rot: vorhandener Nachweis ungültig, nicht passend oder abgelaufen.

## Durchgeführte statische Prüfungen

- 51 C-/C++-/INO-Dateien: Klammern, Kommentare, Zeichenketten und Raw-Strings lexikalisch geprüft.
- Sprachsystem: 229 Text-IDs, jeweils 229 deutsche und englische Einträge.
- TFT-Hauptmenü: deklarierte Größe 14, 14 Texte und 14 Zielseiten.
- JavaScript der Web-Hauptseite: `node --check` ohne Syntaxfehler.
- JavaScript des Web-Setups einschließlich `pageshow`-Reinitialisierung: `node --check` ohne Syntaxfehler.
- Drei neue HTML-Formatvorlagen: Struct-/Initialisierungsprüfung mit `g++ -Wall -Wextra -Werror`.
- `snprintf`-Signaturen: 27, 25 und 15 Platzhalter mit strikter `-Wformat=2`-Prüfung.
- Neu eingefügter TFT-Gültigkeitsblock: isoliert mit strikter C++17-Stubumgebung kompiliert.
- Größte simulierte neue Web-Seite blieb deutlich unter dem 8192-Byte-Ausgabepuffer.

## Noch erforderliche praktische Prüfung

In der bereitstellenden Umgebung steht kein Teensyduino-Compiler zur Verfügung. Vor Freigabe sind deshalb noch nötig:

1. Kompilieren mit Teensyduino 1.62 für Teensy 4.1.
2. Web-Hauptseite nach Firmwareupdate mit geleertem Browsercache prüfen; die ETags wurden vorsorglich geändert.
3. `Info` und `Setup → Info / Gültigkeit` gegeneinander vergleichen.
4. Info aus dem Web-Setup öffnen, Browser-Zurück verwenden und anschließend `EXIT` testen.
5. Geräte- und Kopfkalibrieranfrage exportieren, im KeyTool V0.7.1 signieren und importieren.
6. Statusänderung unmittelbar auf `Zertifizierung`, `Kalibrierung verwalten`, Web-Info und TFT-Info prüfen.
7. Ablauf-, falsche Kopf-SN- und ungültige-Datei-Fälle kontrollieren.
