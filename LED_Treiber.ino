/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: LED_Treiber.ino
 * Zweck: AD5683R-basierte LED-Stromvorgabe und LED_OFF-Hardwareabschaltung.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#include <SPI.h>

// Verknüpfung mit dem CS-Pin aus dem Haupt-Tab
extern const byte pinCsLed;   // Pin 3 (CS/LED im Schaltplan)
extern const byte pinLedOff;  // Pin 7: LED_OFF, HIGH = LED aus

// =========================================================================
// LED_OFF HARDWARE-ABSCHALTUNG
// LED_OFF = HIGH: Q1 schaltet, Gate/Stromregelung wird nach GND gezogen -> LED aus
// LED_OFF = LOW : LED-Regelung ist freigegeben
// =========================================================================
#define LED_OFF_ACTIVE    HIGH
#define LED_OFF_INACTIVE  LOW

void setLedOffHardware(bool off)
{
  digitalWriteFast(pinLedOff, off ? LED_OFF_ACTIVE : LED_OFF_INACTIVE);
}

void initLedOffControl()
{
  // Ausgangslatch zuerst auf AUS setzen, danach Pin als Ausgang aktivieren.
  // Zusammen mit externem Pull-up bleibt die LED auch beim Booten sicher aus.
  digitalWriteFast(pinLedOff, LED_OFF_ACTIVE);
  pinMode(pinLedOff, OUTPUT);
  digitalWriteFast(pinLedOff, LED_OFF_ACTIVE);
}

// =========================================================================
// INTERNE FUNKTION: ROHDATEN AN DEN AD5683R SENDEN (Mit SPI0-Busschutz)
// =========================================================================
void writeAD5683R(byte command, uint16_t data) {
  // 24-Bit Wort bauen: [XX C1 C0 D15...D12] [D11...D4] [D3...D0 XXXX]
  byte b1 = (command << 4) | (data >> 12);  // Kommando + oberste 4 Bits
  byte b2 = (data >> 4) & 0xFF;             // Mittlere 8 Bits
  byte b3 = (data << 4) & 0xF0;             // Unterste 4 Bits + 4 Nullen

  // SPI0-Bus (Display-Bus) für den AD5683R konfigurieren und sperren
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWriteFast(pinCsLed, LOW);
  
  SPI.transfer(b1);
  SPI.transfer(b2);
  SPI.transfer(b3);
  
  digitalWriteFast(pinCsLed, HIGH);
  SPI.endTransaction(); // Bus wieder für das Display freigeben
}

// =========================================================================
// 1. INITIALISIERUNG DES LED-DACs (Wird einmal aus void setup() aufgerufen)
// =========================================================================
void initLedDac() {
  pinMode(pinCsLed, OUTPUT);
  digitalWriteFast(pinCsLed, HIGH);

  // Setzt die LED beim Systemstart auf einen sicheren Startstrom von 10 mA
  setTargetCurrent(10.0); 
}

// =========================================================================
// 2. LED-KONSTANTSTROM GENAU EINSTELLEN (In mA, von 0.0 bis 100.0)
// =========================================================================
void setTargetCurrent(float targetCurrentMA) {
    const float VREF_DAC = 2.500;   // DAC Referenzspannung (Gain 1x)
    const float R_SHUNT = 15.0;     // KORREKTUR: Ihr aktueller 15 Ohm Shunt laut Schaltplan R16
    
    // Hardwaredackel bei maximal 100 mA (Zerstörschutz der Spiegel-LED)
    if (targetCurrentMA > 100.0) targetCurrentMA = 100.0;

    if (targetCurrentMA <= 0.0) {
        // --- FALL A: AUSSCHALTEN (Echte Null) ---
        setLedOffHardware(true);   // Hardware-Abschaltung aktiv: LED sicher aus
        writeAD5683R(0x3, 0x0000); // Wert auf 0 setzen
        writeAD5683R(0x4, 0x2000); // Power-Down Mode: 1k Ohm to GND (eliminiert die 9µA)
    } 
    else {
        // --- FALL B: REGELBETRIEB ---
        setLedOffHardware(false);  // LED-Regelung freigeben
        writeAD5683R(0x6, 0x0000); // Reset / Gain 1 (2.5V) und Normal Operation
        delayMicroseconds(10);     // Kurze Pause für das DAC-Rechenwerk (besser als delay(1) im Interrupt)
        writeAD5683R(0x4, 0x0000); // Normalen Operationsmodus aktivieren

        // Berechnung der Zielspannung am Shunt für Low-Side
        float vDacTarget = (targetCurrentMA / 1000.0) * R_SHUNT; 

        // Umrechnung der analogen Zielspannung in 16-Bit DAC-Schritte
        uint16_t dacValue = (uint16_t)((vDacTarget / VREF_DAC) * 65535.0);

        // Wert in das Register schreiben und DAC updaten (Cmd 0x3)
        writeAD5683R(0x3, dacValue);
    }
}

