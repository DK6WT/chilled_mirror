# TP-3000 V0.50.1 – Bibliotheks- und Lizenzaudit

Stand: 24.07.2026

Dieses Dokument ist eine technische Bestands- und Plausibilitätsprüfung des Releasepakets, keine Rechtsberatung.

## Ergebnis

Der öffentliche Quellbaum enthält für alle gebündelten Fremdkomponenten einen erkennbaren Lizenz- oder Herkunftsnachweis. Der im Arbeitsarchiv enthaltene einzige wesentliche Verteilungskonflikt – die panelspezifische GSL1680-Firmwaretabelle mit unklaren Weitergaberechten – wurde durch Ausschluss aus dem öffentlichen Paket beseitigt.

Außerdem wurde der zuvor angekündigte, aber fehlende pako-Lizenzhinweis ergänzt.

## Gebündelte Komponenten

| Komponente | Version/Stand | Pfad | Lizenz | Nachweis |
|---|---:|---|---|---|
| TP-3000 / LJ2000M-Ableitung | V0.50.1 | Projektquellen | GPL-3.0-only; upstream LJ2000M GPLv3-or-later | `LICENSE`, Quellheader |
| GSL1680-Treiber | Projektstand | `GSL1680.*`, Wrapper | GPL-3.0-only | Quellheader, `GSL1680-NOTICE.txt` |
| ProtoCentral ADS1262 | 2.0.0 | `libraries/...ADS1262...` | MIT; Dokumentation upstream CC-BY-SA-4.0 | zwei Original-Lizenzdateien |
| RTC RV3129 | 1.0.0 | `libraries/RTC_RV3129...` | MIT | Original-`LICENSE.md` |
| SparkFun BMP581 | 1.0.1 | `libraries/SparkFun_BMP581...` | MIT | Original-`LICENSE.md` |
| Bosch BMP5 API | 2021-Stand | BMP581 `src/bmp5_api` | BSD-3-Clause | komponentenlokales `LICENSE` |
| micro-ecc | 1.0.0 / TP-Integration 1.0.0-tp3000.1 | `libraries/TP3000_micro_ecc` | BSD-2-Clause | Original-`LICENSE.txt` |
| WDT_T4 | 0.1 | `libraries/WDT_T4` | MIT | Original-`LICENSE` |
| pako | 1.0.11 | Raw-String in `TPethernet.ino` | MIT AND zlib | `PAKO-NOTICE.txt`, `Zlib.txt` |
| QRCode for JavaScript | Projektstand | `TPethernet.ino` | MIT | Quellkopf und Notice |
| Project Nayuki QR | reduzierter Adapter | `TPqrCode.cpp/.h` | MIT-Grundlage; TP-Adapter GPL-3.0-only | Quellkopf und Notice |
| Droid Sans Mono | AOSP-Quelle | Fontquelle/Firmwaredaten | Apache-2.0 | Source-Notice, SHA-Datei, Generator |
| Font Awesome | 4.5.0 | Fontquelle/Firmwaredaten; unveränderte Release-README | OFL-1.1 für Fonts/Glyphdaten; CC BY 3.0 für README | Source-README, zwei Notices, Generator |
| Earth-Startbild | 14.06.2026 | `assets/`, `earth.c` | dokumentierte AI-Erzeugung und NASA-Referenz | `EARTH-IMAGE-NOTICE.txt` |

## Nicht gebündelte Komponenten

RA8875, NativeEthernet/FNET, SD/SdFat und weitere Arduino-/Teensy-Kernbibliotheken werden aus der Toolchain geladen. Sie sind keine Dateien dieses Quellpakets und werden nicht als TP-3000-Eigentum oder unter der Projektlizenz ausgegeben. Für reproduzierbare Builds muss die verwendete Toolchainversion dokumentiert werden.

## GSL1680-Panel-Firmware

### Befund im hochgeladenen Arbeitsarchiv

Die Datei `external/GSL1680/gslX680_311_5_F.h` war vorhanden, obwohl `.gitignore`, Komponenten-README und Lizenznotice ausdrücklich vor einer öffentlichen Weitergabe warnen.

### Maßnahme

Die Datei wurde aus dem GitHub-Quellrelease entfernt. `tools/release_audit.py` behandelt ihr Vorhandensein als harten Fehler. Die lokale Beschaffung aus dem offiziellen Panelpaket bleibt in `external/GSL1680/README.md` dokumentiert.

### Konsequenz für Binaries

Da die Tabelle beim Build in das Programmabbild eingebettet wird, enthält der Release keine vorgebauten Firmwarebinaries. Vor einer späteren Binärveröffentlichung müssen die Weitergaberechte separat geklärt werden.

## pako-/zlib-Korrektur

`THIRD_PARTY_NOTICES.md` verwies bereits auf `LICENSES/PAKO-NOTICE.txt`; diese Datei fehlte im Arbeitsarchiv. Ergänzt wurden:

- `LICENSES/PAKO-NOTICE.txt`
- `LICENSES/Zlib.txt`

Die pako-1.0.11-Paketmetadaten verwenden die kombinierte Angabe `(MIT AND Zlib)`. Die JavaScript-Hülle steht unter MIT, der portierte zlib-Kern unter der zlib License.

## Ergänzte zentrale Lizenzkopien und Hinweise

- `BSD-2-Clause.txt`
- `BSD-3-Clause.txt`
- `CC-BY-SA-4.0.txt`
- `CC-BY-3.0-NOTICE.txt`
- `Zlib.txt`

Die unveränderte Font-Awesome-v4.5.0-README gehört zur Upstream-Dokumentation und steht unter CC BY 3.0. Sie enthielt bereits Creator, Lizenzbezeichnung und Lizenz-URI; der zusätzliche zentrale Hinweis macht diese Zuordnung im Releaseaudit ausdrücklich sichtbar. Die Fontdateien und daraus erzeugten Glyphdaten bleiben davon getrennt unter OFL-1.1. Komponentenlokale Originaltexte wurden nicht ersetzt oder entfernt.

## SparkFun-BMP581-Sonderprüfung

Die generische SparkFun-`LICENSE.md` erwähnt eine Analog-Devices-SLA. Eine Suche im tatsächlich verteilten Quellordner ergab keine Analog-Devices-Dateien oder Copyright-Hinweise. Der Release dokumentiert daher die tatsächlich vorhandenen Lizenzgruppen:

- SparkFun-Wrapper unter MIT
- Bosch BMP5 API unter BSD-3-Clause

## Font-Provenienzprüfung

Die Quellfont-Hashes, Symbolbereiche und UTF-8-Zuordnungen wurden geprüft. Der Droid-Sans-Mono-Generator hatte im Arbeitsarchiv die bereits verwendeten `PROGMEM`-Attribute nicht mehr ausgegeben und scheiterte deshalb trotz identischer Glyphdaten am Bytevergleich. Der Generator wurde an die vorhandene Flash-Ablage angepasst. Anschließend regenerierten sowohl Droid Sans Mono als auch Font Awesome bytegleich.

## Geheimnisse und Produktionsdaten

Im öffentlichen Baum wurden keine privaten PEM-Schlüssel, keine `_PRIVATE.pem`-Dateien und keine Firmwarebinaries gefunden. `test_identity/TP3000_TEST_ROOT_PUBLIC.pem` ist ausdrücklich nur ein öffentlicher Testschlüssel und kein Produktions-Root-Private-Key.

## Verbleibende organisatorische Pflichten

- Test- und Produktionsschlüssel strikt trennen.
- Toolchainversion und externe Bibliotheksstände beim finalen Build archivieren.
- bei Änderungen an eingebetteten Drittkomponenten Notices und Lizenztexte erneut prüfen.
- keine GSL1680-Paneltabelle oder daraus erzeugte Binaries ohne geklärte Rechte veröffentlichen.
- Markenbezeichnungen nur beschreibend und ohne Unterstützungsbehauptung verwenden.
