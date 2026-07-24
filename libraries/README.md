# Mitgelieferte lokale Build-Bibliotheken

Dieser Ordner enthält die Bibliotheksstände, die für den TP-3000-V0.50.1-Release mitgeführt und gegenüber gleichnamigen globalen Installationen bevorzugt werden sollen.

| Ordner | Version | Ursprung | Lizenz des gebündelten Codes |
|---|---:|---|---|
| `ProtoCentral_ADS1262_32-bit_precision_ADC_Library` | 2.0.0 | ProtoCentral ADS1262 | MIT |
| `RTC_RV3129_Arduino_Library` | 1.0.0 | OUIDEAS / SparkFun RV-3129 Fork | MIT |
| `SparkFun_BMP581_Arduino_Library` | 1.0.1 | SparkFun BMP581 | MIT; Bosch-Unterordner BSD-3-Clause |
| `TP3000_micro_ecc` | 1.0.0-tp3000.1 | micro-ecc / Kenneth MacKay | BSD-2-Clause |
| `WDT_T4` | 0.1 | Antonio Brewer | MIT |

Jede Bibliothek enthält ihre eigene Lizenzdatei. Zusätzliche zentrale Kopien und Hinweise liegen unter `../LICENSES/`.

## ProtoCentral ADS1262

Die gebündelten Softwarequellen stehen unter MIT. Die upstream mitgeführte `LICENSE.md` nennt für separate Hardwareunterlagen CERN-OHL-P v2 und für Dokumentation CC-BY-SA-4.0. Im TP-3000-Paket sind keine ProtoCentral-Hardwaredesigns enthalten; README-/Dokumentationshinweise bleiben mit Original-Lizenzangabe erhalten.

## RTC RV3129

Der gebündelte Code steht unter MIT. Die Original-Lizenzdatei enthält zusätzlich den allgemeinen SparkFun-Hinweis für Hardwareunterlagen; solche Hardwaredateien sind hier nicht enthalten.

## SparkFun BMP581 und Bosch BMP5 API

Die SparkFun-Wrapperdateien stehen unter MIT. Der Unterordner `src/bmp5_api/` stammt von Bosch Sensortec und steht separat unter BSD-3-Clause. Dessen Original-Lizenzdatei bleibt direkt im Unterordner erhalten.

Die generische SparkFun-`LICENSE.md` erwähnt außerdem eine Analog-Devices-SLA für andere Bibliotheksinhalte. Im tatsächlich gebündelten BMP581-Quellbaum wurden keine Analog-Devices-Dateien gefunden; maßgeblich sind die SparkFun-MIT- und Bosch-BSD-3-Clause-Hinweise in den betroffenen Dateien.

## TP3000 micro-ecc

Projektlokale P-256-/ECDSA-Abhängigkeit unter BSD-2-Clause. Die TP-3000-Integration beschränkt Konfiguration und Codeplatzierung auf den benötigten Funktionsumfang. Die Original-Lizenz liegt in `TP3000_micro_ecc/LICENSE.txt`.

## WDT_T4

Die MIT-lizenzierte lokale Kopie enthält die für den Build benötigten Header/Template-Dateien und die upstream mitgelieferten Beispiele. Der TP-3000-Code kann ersatzweise eine global installierte Version finden; für reproduzierbare Releases ist die lokale Fassung vorgesehen.

## Externe Toolchain-Bibliotheken

RA8875 wird nicht doppelt gebündelt. Vorgesehen ist die Version aus der installierten Teensyduino-Umgebung. Ebenso kommen Arduino Core, EEPROM, Entropy, Metro, NativeEthernet/FNET, SD/SdFat, SPI, TimeLib und Wire aus Arduino/Teensyduino und behalten ihre eigenen Lizenzbedingungen.
