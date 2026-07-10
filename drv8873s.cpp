/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: drv8873s.cpp
 * Zweck: SPI-Konfiguration und Diagnose des DRV8873-H-Brueckentreibers.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#include <SPI.h>
#include "drv8873s.h"
#include "TP_T.h"

#define REG_FAULT_STATUS  0x00
#define REG_DIAG_STATUS   0x01
#define REG_IC1_CONTROL   0x02
#define REG_IC3_CONTROL   0x04
#define REG_IC4_CONTROL   0x05

// SPI Einstellungen passend fuer den DRV8873S (Mode 1, max. 4 MHz)
SPISettings drv8873SpiSettings(2000000, MSBFIRST, SPI_MODE1);
static uint16_t transferSPI16(uint8_t regAddr, bool isRead, uint8_t data);

// DRV8873S-Konfiguration laut TI-SPI-Registermodell:
 // IC1_CONTROL:
 //   TOFF   bits 7..6 = 01b -> 40 us, Default
 //   SPI_IN bit  5    = 0b  -> Ausgaenge folgen den Pins IN1/IN2
 //   SR     bits 4..2 = 010b -> 18.3 V/us
 //   MODE   bits 1..0 = 01b -> PWM-Mode
 //
 // Hinweis Hardware V32: DISABLE liegt fest LOW. PH/EN wuerde zwar per
 // Register lesbar sein, wird ohne DISABLE HIGH->LOW aber nicht sauber
 // uebernommen. Deshalb wird der DRV bewusst im PWM-Mode betrieben.
static const uint8_t  DRV8873_IC1_CFG             = 0x49;
static const uint16_t DRV8873_PWM_OFF             = 4096;
static const uint8_t  DRV8873_IC3_UNLOCK          = 0x40; // LOCK = 100b, OUT1/2 enabled
static const uint8_t  DRV8873_IC3_CLEAR_FAULTS    = 0xC0; // CLR_FLT + LOCK = 100b
// IC4_CONTROL:
//   ITRIP_LVL bits 3..2 = 10b -> 6.5 A default bleibt als Diagnosewert
//   DIS_ITRIP bits 1..0 = 11b -> interne Stromregelung fuer OUT1 und OUT2 aus
// Damit kann der DRV8873 im PWM-Mode nicht mehr intern auf ca. 200 mA herumchoppern.
// IPROPI-Ausgabe bleibt als Diagnose/Strommesssignal erhalten.
static const uint8_t  DRV8873_IC4_DISABLE_ITRIP   = 0x0B;
static const uint32_t DRV8873_BOOT_DELAY_MS       = 2000UL;
static const uint32_t DRV8873_NFAULT_DEBOUNCE_MS  = 200UL;
static const uint32_t DRV8873_REG_POLL_MS         = 1000UL;
static const uint8_t  DRV8873_SPI_BAD_LIMIT       = 3;

// Asynchrones Hochfahren / Diagnosezustand
bool drv8873StabiBereit = false;
unsigned long drvBootTimer = 0;

static bool     drv8873SpiOkState        = false;
static bool     drv8873ConfigOkState     = false;
static uint8_t  drv8873LastFaultRegister = 0xFF;
static uint8_t  drv8873LastDiagRegister  = 0xFF;
static uint8_t  drv8873LastIc1Register   = 0xFF;
static uint8_t  drv8873LastIc3Register   = 0xFF;
static uint8_t  drv8873LastIc4Register   = 0xFF;
static uint16_t drv8873LastFaultRaw      = 0xFFFF;
static uint16_t drv8873LastDiagRaw       = 0xFFFF;
static uint16_t drv8873LastIc1Raw        = 0xFFFF;
static uint16_t drv8873LastIc3Raw        = 0xFFFF;
static uint16_t drv8873LastIc4Raw        = 0xFFFF;
static uint16_t drv8873SpiErrorCounter   = 0;
static uint8_t  drv8873SpiBadStreak      = 0;
static uint32_t drv8873LastPollMs        = 0;
static uint32_t drv8873NfaultLowSinceMs  = 0;

static void drv8873PwmModeOff()
{
    extern const byte pinEn; // Pin 37 = IN1
    extern const byte pinPh; // Pin 36 = IN2

    // PWM-Mode: IN1=HIGH und IN2=HIGH ergibt OUT1=H / OUT2=H,
    // also 0 V Differenz am Peltier. Das ist unser sicherer AUS-Zustand.
    analogWrite(pinEn, DRV8873_PWM_OFF);
    analogWrite(pinPh, DRV8873_PWM_OFF);
}

static bool drv8873LooksDisconnected(uint8_t faultReg, uint8_t diagReg, uint8_t ic1Reg, uint8_t ic3Reg)
{
    // Typischer SPI-Fehler bei nicht erreichbarem Baustein / offener MISO:
    // alle gelesenen Register 0xFF. 0x00 kann dagegen ein gueltiger Zustand sein.
    return (faultReg == 0xFF && diagReg == 0xFF && ic1Reg == 0xFF && ic3Reg == 0xFF);
}

static void drv8873ReadAllRegisters()
{
    drv8873LastFaultRaw = transferSPI16(REG_FAULT_STATUS, true, 0x00);
    drv8873LastDiagRaw  = transferSPI16(REG_DIAG_STATUS,  true, 0x00);
    drv8873LastIc1Raw   = transferSPI16(REG_IC1_CONTROL,  true, 0x00);
    drv8873LastIc3Raw   = transferSPI16(REG_IC3_CONTROL,  true, 0x00);
    drv8873LastIc4Raw   = transferSPI16(REG_IC4_CONTROL,  true, 0x00);

    drv8873LastFaultRegister = (uint8_t)(drv8873LastFaultRaw & 0xFF);
    drv8873LastDiagRegister  = (uint8_t)(drv8873LastDiagRaw  & 0xFF);
    drv8873LastIc1Register   = (uint8_t)(drv8873LastIc1Raw   & 0xFF);
    drv8873LastIc3Register   = (uint8_t)(drv8873LastIc3Raw   & 0xFF);
    drv8873LastIc4Register   = (uint8_t)(drv8873LastIc4Raw   & 0xFF);

    if (drv8873LooksDisconnected(drv8873LastFaultRegister,
                                 drv8873LastDiagRegister,
                                 drv8873LastIc1Register,
                                 drv8873LastIc3Register))
    {
        drv8873SpiOkState = false;
        drv8873ConfigOkState = false;
        if (drv8873SpiBadStreak < 255) drv8873SpiBadStreak++;
        if (drv8873SpiErrorCounter < 65535) drv8873SpiErrorCounter++;
        return;
    }

    drv8873SpiOkState = true;
    drv8873SpiBadStreak = 0;

    // IC1_CONTROL enthaelt nur Konfigurationsbits. Der erwartete Wert ist
    // 0x49: TOFF 40 us, Pins IN1/IN2 aktiv, Slew 18.3 V/us, MODE PWM.
    drv8873ConfigOkState = ((drv8873LastIc1Register == DRV8873_IC1_CFG) &&
                             (drv8873LastIc4Register == DRV8873_IC4_DISABLE_ITRIP));
}

// =========================================================================
// INITIALISIERUNG DES DRV8873S
// =========================================================================
void initDRV8873S()
{
    drvBootTimer = millis();
    drv8873StabiBereit = false;
    drv8873SpiOkState = false;
    drv8873ConfigOkState = false;
    drv8873LastFaultRegister = 0xFF;
    drv8873LastDiagRegister = 0xFF;
    drv8873LastIc1Register = 0xFF;
    drv8873LastIc3Register = 0xFF;
    drv8873LastIc4Register = 0xFF;
    drv8873SpiErrorCounter = 0;
    drv8873SpiBadStreak = 0;
    drv8873LastPollMs = 0;
    drv8873NfaultLowSinceMs = 0;
}

// =========================================================================
// SLEW RATE & INTERFACE KONFIGURIEREN
// =========================================================================
void setDRV8873SSlewRate(uint8_t modeSetting)
{
    // modeSetting 0..7 entspricht SR bits 4..2.
    // PWM-Mode bleibt aktiv, SPI_IN bleibt 0, TOFF bleibt Default 40 us.
    uint8_t configByte = (uint8_t)(0x40 | ((modeSetting & 0x07) << 2) | 0x01);
    transferSPI16(REG_IC1_CONTROL, false, configByte);
}

// =========================================================================
// DIAGNOSE, ASYNCHRONER REGLER-START & SAFETY-ABSCHALTUNG
// =========================================================================
bool checkDRV8873SFaults()
{
    extern const byte pinNfault; // Pin 41 laut aktueller Hardware
    extern void safetyTriggerDrv8873Fault();
    extern void safetyTriggerDrv8873SpiFault();

    uint32_t nowMs = millis();

    // 1. Asynchroner Boot-Schutz: nach dem Start 2 Sekunden warten.
    if (!drv8873StabiBereit)
    {
        if ((uint32_t)(nowMs - drvBootTimer) >= DRV8873_BOOT_DELAY_MS)
        {
            // PWM-Mode + Slew Rate 18.3 V/us setzen.
            transferSPI16(REG_IC1_CONTROL, false, DRV8873_IC1_CFG);

            // Interne DRV-Stromregelung/Chopper aus. Harte OCP/Thermal-Schutzfunktionen
            // bleiben davon unberuehrt. Das verhindert, dass der DRV die PWM intern
            // auf kleine Ausgangsdutys zerhackt.
            transferSPI16(REG_IC4_CONTROL, false, DRV8873_IC4_DISABLE_ITRIP);

            // Einschalt-/alte Fehlerflags ueber IC3_CONTROL loeschen.
            // Wichtig: Clear-Fault liegt nicht in IC1_CONTROL.
            transferSPI16(REG_IC3_CONTROL, false, DRV8873_IC3_CLEAR_FAULTS);
            transferSPI16(REG_IC3_CONTROL, false, DRV8873_IC3_UNLOCK);

            drv8873ReadAllRegisters();
            drv8873LastPollMs = nowMs;
            drv8873StabiBereit = true;

            Serial.print("DRV8873 2MHz init: SPI=");
            Serial.print(drv8873SpiOkState ? "OK" : "ERR");
            Serial.print(" CFG=");
            Serial.print(drv8873ConfigOkState ? "OK" : "ERR");
            Serial.print(" FAULT=0x");
            Serial.print(drv8873LastFaultRegister, HEX);
            Serial.print(" DIAG=0x");
            Serial.print(drv8873LastDiagRegister, HEX);
            Serial.print(" IC1=0x");
            Serial.print(drv8873LastIc1Register, HEX);
            Serial.print(" IC3=0x");
            Serial.print(drv8873LastIc3Register, HEX);
            Serial.print(" IC4=0x");
            Serial.print(drv8873LastIc4Register, HEX);
            Serial.print(" RAW C=0x");
            Serial.print(drv8873LastIc1Raw, HEX);
            Serial.print(" I3=0x");
            Serial.print(drv8873LastIc3Raw, HEX);
            Serial.print(" I4=0x");
            Serial.println(drv8873LastIc4Raw, HEX);

            if (!drv8873SpiOkState)
            {
                drv8873PwmModeOff();
                safetyTriggerDrv8873SpiFault();
                return false;
            }
        }

        // Waehrend der Boot-Wartezeit keinen Fehlalarm ausloesen.
        return true;
    }

    // 2. Zyklisches Registerlesen im Betrieb. Nicht in jedem Loop, sondern ca. 1x/s.
    if ((uint32_t)(nowMs - drv8873LastPollMs) >= DRV8873_REG_POLL_MS)
    {
        drv8873LastPollMs = nowMs;
        drv8873ReadAllRegisters();

        if (!drv8873SpiOkState && drv8873SpiBadStreak >= DRV8873_SPI_BAD_LIMIT)
        {
            drv8873PwmModeOff();
            safetyTriggerDrv8873SpiFault();
            return false;
        }
    }

    // 3. nFAULT ist aktiv LOW. Wir entprellen bewusst langsam, damit kurze
    // Start-/EMV-Impulse keinen Safety-Latch ausloesen.
    if (digitalReadFast(pinNfault) == HIGH)
    {
        drv8873NfaultLowSinceMs = 0;
        return drv8873SpiOkState;
    }

    if (drv8873NfaultLowSinceMs == 0)
    {
        drv8873NfaultLowSinceMs = nowMs;
        return drv8873SpiOkState;
    }

    if ((uint32_t)(nowMs - drv8873NfaultLowSinceMs) < DRV8873_NFAULT_DEBOUNCE_MS)
    {
        return drv8873SpiOkState;
    }

    // nFAULT dauerhaft LOW: Register nochmal direkt lesen und Safety ausloesen.
    drv8873ReadAllRegisters();

    extern float peltierSollWert;
    peltierSollWert = 0.0f;
    drv8873PwmModeOff();

    safetyTriggerDrv8873Fault();
    return false;
}

// =========================================================================
// Diagnose-Getter
// =========================================================================
bool drv8873SpiIsOk()
{
    return drv8873SpiOkState;
}

bool drv8873ConfigIsOk()
{
    return drv8873ConfigOkState;
}

uint8_t drv8873GetLastFaultRegister()
{
    return drv8873LastFaultRegister;
}

uint8_t drv8873GetLastDiagRegister()
{
    return drv8873LastDiagRegister;
}

uint8_t drv8873GetLastIc1Register()
{
    return drv8873LastIc1Register;
}

uint16_t drv8873GetSpiErrorCounter()
{
    return drv8873SpiErrorCounter;
}

uint8_t drv8873GetLastIc3Register()
{
    return drv8873LastIc3Register;
}

uint8_t drv8873GetLastIc4Register()
{
    return drv8873LastIc4Register;
}

uint16_t drv8873GetLastFaultRaw()
{
    return drv8873LastFaultRaw;
}

uint16_t drv8873GetLastDiagRaw()
{
    return drv8873LastDiagRaw;
}

uint16_t drv8873GetLastIc1Raw()
{
    return drv8873LastIc1Raw;
}

uint16_t drv8873GetLastIc3Raw()
{
    return drv8873LastIc3Raw;
}

uint16_t drv8873GetLastIc4Raw()
{
    return drv8873LastIc4Raw;
}

// =========================================================================
// INTERNE SPI HILFSFUNKTION
// =========================================================================
static uint16_t transferSPI16(uint8_t regAddr, bool isRead, uint8_t data)
{
    extern const byte pinNscs; // Pin 20 laut aktueller Hardware

    // DRV8873S SPI 1-Device-Frame:
    // Bit15    = 0
    // Bit14    = R/W
    // Bit13..9 = A4..A0
    // Bit8     = X
    // Bit7..0  = Daten
    //
    // Rohantworten werden fuer die ADC-Info zusaetzlich gespeichert.
    // R/W-Polaritaet laut Datenblatt/letztem Test:
    // READ setzt Bit14, WRITE laesst Bit14 0.
    uint16_t controlWord = 0;
    if (isRead) controlWord |= (1U << 14);
    controlWord |= ((uint16_t)(regAddr & 0x1F) << 9);
    controlWord |= (data & 0xFF);

    SPI.beginTransaction(drv8873SpiSettings);

    // Display bleibt mit 20 MHz unveraendert. Nur der DRV-Zugriff nutzt
    // eigene SPISettings: 2 MHz wie MCP, plus konservative CS-Setup/Hold-Zeiten.
    digitalWriteFast(pinNscs, LOW);
    delayMicroseconds(2);
    uint16_t rxWord = SPI.transfer16(controlWord);
    delayMicroseconds(2);
    digitalWriteFast(pinNscs, HIGH);
    delayMicroseconds(2);

    SPI.endTransaction();

    return rxWord;
}
