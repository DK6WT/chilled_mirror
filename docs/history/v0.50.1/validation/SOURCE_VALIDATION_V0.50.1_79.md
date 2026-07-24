# TP-3000 Source Validation V0.50.1_79

Stand: 21.07.2026  
Basis: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_78.zip`  
Ziel: `Taupunktspiegel_StandV1_2026-07-11_V0.50.1_79.zip`

## Gegenstand

V0.50.1_79 korrigiert die RAM2-Regression der TFT-Kalibrierschein-QR-Funktion. Die in V0.50.1_78 dauerhaft reservierten TP3C1-, Kompressions-, Parser- und QR-Arbeitspuffer werden auf bereits vorhandene, nur sequenziell verwendete Projektpuffer abgebildet. Die Ethernet-/DHCP-Logik selbst bleibt unverändert.

## Statische Prüfungen

- `TPsignedCalibration.cpp` mit Clang C++17, Projekt-Teststubs, `-Wall -Wextra -Werror` kompiliert.
- `TPqrCode.cpp` und `TPzlibDeflate.cpp` mit Clang C++17 und strengen Warnungen kompiliert.
- `TPmenu_QrCertificate.ino` mit TFT-/Menü-Teststubs und strengen Warnungen kompiliert.
- Neue API `tpSignedCalibrationEncodeTp3c1Qr()` und alle X-/Headerdeklarationen abgeglichen.
- QR-Funktions-, Codewort-, Interleave- und Blockbereiche durch `static_assert` gegen die tatsächlich wiederverwendeten Puffergrößen abgesichert.
- `ethJsonBuf` besitzt externe C++-Linkage und wird ausschließlich synchron im Hauptloop als TP3C1-Umschlagspuffer ausgeliehen.
- Projekt-Libraries gegenüber V0.50.1_78 bytegleich.
- Text-/Quelldateien auf eingebettete NUL-Zeichen und geänderte Quellen auf ausgeglichene Klammern, Kommentare und Zeichenketten geprüft.

## RAM2-/BSS-Vergleich

Quellgleicher Host-Objektvergleich mit `-O0` und identischen Stubs:

- `TPsignedCalibration.cpp`: V0.50.1_78 → V0.50.1_79: **40.816 Byte weniger BSS**
- `TPmenu_QrCertificate.ino`: V0.50.1_78 → V0.50.1_79: **15.146 Byte weniger BSS**
- zusammen zurückgewonnen: **55.962 Byte**
- verbleibender QR-Zusatz gegenüber V0.50.1_77: rund **13,5 KiB**

Diese Hostwerte bestätigen die entfernten statischen Puffer, ersetzen aber nicht den endgültigen Teensy-4.1-Linkerbericht.

## Noch ausstehend

- vollständiger Teensyduino-Build,
- realer RAM1-/RAM2-Speicherbericht,
- DHCP- und Static-IP-Test auf dem Gerät,
- Ethernet Aus/Ein ohne Watchdog-Reset,
- gleichzeitiger Webzugriff und TFT-QR-Anzeige,
- Scanvergleich beider TFT-Codes gegen den Web-Kalibrierschein.
