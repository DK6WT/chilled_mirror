# Bootcrash-Fix und RAM-Rückgewinnung – V0.50.1_52

## 1. Eindeutige Absturzursache

Der SD-CrashReport meldete für Build `0.50.1_50` einen DACCVIOL bei Codeadresse `0x37E82` und eine gültige Fehleradresse `0x00000000`. Das exakt passende ELF ordnet `0x37E82` der Schreibschleife von `memset()` zu.

Im disassemblierten `setup()` stehen unmittelbar vor zwei solchen `memset()`-Aufrufen jeweils:

```text
movw r0, #0xE2C
bl   operator new(unsigned int)
movw r2, #0xE2C
mov   r1, #0
bl   memset
```

`0xE2C` sind 3628 Byte und entspricht exakt `sizeof(TextBox)`. Der Quellcode erzeugte nacheinander `VirtLCDMenu` und `VirtLCDMessage` mit `new TextBox()` und dereferenzierte sie ohne Fehlerprüfung. Wenn der RAM2-Heap zu diesem späten Zeitpunkt kein zusammenhängendes Objekt mehr liefern konnte, gab `operator new()` in der Teensy-Laufzeit `nullptr` zurück. Die compilererzeugte Wertinitialisierung rief danach `memset(nullptr, 0, 3628)` auf. Das erklärt DACCVIOL, MMFAR 0 und den verzögerten Neustart des Fault-Handlers vollständig.

## 2. Bootfix

Beide Menüobjekte werden statisch bereitgestellt:

```cpp
static TextBox VirtLCDMenuStorage;
static TextBox VirtLCDMessageStorage;
TextBox* VirtLCDMenu = &VirtLCDMenuStorage;
TextBox* VirtLCDMessage = &VirtLCDMessageStorage;
```

Damit entfällt im Bootpfad jede Heap-Allokation für diese TextBoxen. Auch beim erneuten Öffnen des Setups werden die Objekte nur initialisiert und geleert, nicht freigegeben oder neu angelegt.

## 3. RAM1

Das Build-50-ELF enthält 36.382 Byte gelinkte DroidSansMono-Daten, Indizes und Fontdeskriptoren in RAM1 `.data`. Diese unveränderlichen Tabellen sind jetzt mit `PROGMEM` in `.text.progmem` abgelegt. Das entspricht der bereits im Projekt verwendeten Ablage des Font-Awesome-Fonts. Der Fontgenerator erzeugt die Attribute ebenfalls, sodass eine Neugenerierung die Optimierung nicht zurücknimmt.

Die zwei statischen TextBoxen belegen zusammen 7256 Byte RAM1. Netto ergibt sich gegenüber Build 51 daher voraussichtlich:

```text
36382 Byte Fonttabellen aus RAM1
-7256 Byte statische TextBoxen in RAM1
=29126 Byte zusätzlicher freier RAM1
```

Ausgehend von 79.516 Byte freiem RAM1 in Build 51 werden etwa 108.642 Byte erwartet.

## 4. RAM2

Firmwareabbildprüfung und externe PDF-Prüfung besaßen jeweils einen eigenen 4096-Byte-DMAMEM-Puffer. Die Firmwareprüfung läuft ausschließlich beim Booten; die externe PDF-Prüfung erst später im normalen Programmkontext. Beide verwenden deshalb nun denselben gemeinsamen Puffer. Erwartete zusätzliche RAM2-Reserve: 4096 Byte.

Zusätzlich fallen die beiden 3628-Byte-Heap-Allokationen zur Laufzeit vollständig weg. Dieser Effekt erscheint nicht als zusätzlicher freier statischer RAM2 in der Buildausgabe, steht dem Heap aber während des Betriebs zur Verfügung und verhindert die konkrete Fragmentierungs-/Allokationsfehlerklasse.

## 5. Unverändert

Firmware-Hashverfahren, Root-Signatur, Zertifikats- und Kalibrierformate, TP3C1/TP3Q3, zertifizierte Logs, Messwerterfassung, Regelung und Safety wurden nicht verändert. Das geänderte Binärabbild benötigt ein neues Firmwarezertifikat.
