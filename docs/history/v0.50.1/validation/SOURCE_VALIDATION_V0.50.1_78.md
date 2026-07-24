# TP-3000 Source Validation V0.50.1_78

Stand: 21.07.2026  
Basis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_77.zip`  
Zielpaket: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_78.zip`

## Gegenstand

V0.50.1_78 ergänzt im TFT-Hauptmenü den Eintrag **Kalibrierschein anzeigen**.
Die Seite rekonstruiert aus den aktuell wirksamen und erneut geprüften Geräte-,
Kopf- und Systemkalibrierpaketen den zweiteiligen TP3C1-V0.4-Nachweis und
zeigt beide Teile als QR-Code auf dem 800 × 480-TFT an.

Bedienung:

- `UP` / `DOWN`: zwischen `CODE 1/2` und `CODE 2/2` wechseln
- `ENTER`: zurück zum Hauptmenü des Setups
- `EXIT`: über den bestehenden zentralen Exit-Pfad zum Hauptscreen

Die QR-Matrix wird mit vier Modulen Ruhezone und der größtmöglichen
quadratischen Ganzzahlskalierung 4 × 4, 3 × 3 oder 2 × 2 TFT-Pixel je Modul
dargestellt. Eine Matrix mit 97 × 97 Modulen wird einschließlich Ruhezone mit
420 × 420 Pixeln dargestellt.

## Durchgeführte statische Prüfungen

- 275 Text-/Quelldateien auf eingebettete NUL-Zeichen geprüft
- 10 geänderte bzw. neue C++-/Header-/INO-Dateien lexikalisch auf ausgeglichene
  Klammern, Zeichenketten und Kommentare geprüft
- Hauptmenügröße und beide Hauptmenütabellen geprüft: jeweils 15 Einträge
- deutsche und englische Texttabelle als C++17-Übersetzungseinheit kompiliert;
  `TXT_COUNT` stimmt mit beiden Sprachzeilen überein
- `TPsignedCalibration.cpp` vollständig mit Clang C++17 gegen schmale
  Arduino-/SD-/TimeLib-/uECC-Teststubs und `-Wall -Wextra -Werror` kompiliert
- `TPqrCode.cpp`, `TPzlibDeflate.cpp` und die neue TFT-Seite mit C++17,
  Teststubs und `-Wall -Wextra -Werror` kompiliert
- Projekt-Libraries bytegleich gegen V0.50.1_77 verglichen

## QR-Encoder

Der statische QR-Encoder wurde für QR Model 2, alphanumerischen Modus,
Fehlerkorrektur M und automatische Maskenwahl geprüft.

Die vollständigen Matrizen stimmten bytegenau mit der offiziellen
Project-Nayuki-Referenzimplementierung überein für Testtexte mit:

- 11 Zeichen
- 44 Zeichen
- 489 Zeichen
- 1906 Zeichen

Kapazitätsgrenze bei Fehlerkorrektur M und alphanumerischem Modus:

- 3391 Zeichen: akzeptiert
- 3392 Zeichen: korrekt abgelehnt

Skalierungsgrenzen der TFT-Seite wurden direkt geprüft:

- 97 und 109 Module → 4 × 4 Pixel
- 113 und 149 Module → 3 × 3 Pixel
- 153 und 177 Module → 2 × 2 Pixel

## ZLIB-/DEFLATE-Encoder

Der projektlokale statische ZLIB-Encoder erzeugt einen gültigen ZLIB-Wrapper
mit festem Huffman-DEFLATE-Block. Ein 7800-Byte-Teststrom wurde komprimiert,
mit der Python-Standardbibliothek `zlib` entpackt und bytegenau mit den
Eingabedaten verglichen.

## Paket- und Funktionsgrenzen

Der TFT-Ausgabepfad:

- verändert keine Kalibrierdatei,
- erzeugt keine neue Signatur,
- verwendet ausschließlich erneut gelesene und kryptografisch geprüfte aktive
  Quelldokumente,
- zeigt keinen vollständigen QR-Nachweis, wenn erforderliche oder gültige
  Geräte-, Kopf- oder Systemdaten fehlen,
- verwendet rohe `TP3C1:`-Nutzdaten. Diese werden vom TP-3000-Verifier sowohl
  direkt als auch innerhalb eines `tp3000://verify`-Links akzeptiert.

## Noch ausstehend

In der Erstellungsumgebung waren weder Teensyduino/Arduino-CLI noch die reale
TP-3000-Hardware verfügbar. Daher wurden noch nicht durchgeführt:

- vollständige Teensy-4.1-Kompilierung mit der produktiven Toolchain,
- reale FLASH-/RAM-Ausgabe des Gesamtprojekts,
- Scanversuch beider QR-Teile vom echten TFT mit einem Mobiltelefon,
- Prüfung der Bedienung auf dem realen resistiven Touchpanel.

Diese vier Punkte sind vor Freigabe auf der Zielhardware nachzuholen.
