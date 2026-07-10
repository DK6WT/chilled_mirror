/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: drv8873s.h
 * Zweck: Oeffentliche Schnittstelle des DRV8873-Treibers.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#ifndef DRV8873S_H
#define DRV8873S_H

#include <Arduino.h>

// Globale Pins aus der Haupt-Pinliste
extern const byte pinNscs;   // Pin 20 (Chip Select)
extern const byte pinNfault; // Pin 41 (Fehlerleitung nFAULT)

// Oeffentliche Funktionen des Treibers
void initDRV8873S();
void setDRV8873SSlewRate(uint8_t modeSetting);
bool checkDRV8873SFaults();

bool drv8873SpiIsOk();
bool drv8873ConfigIsOk();
uint8_t drv8873GetLastFaultRegister();
uint8_t drv8873GetLastDiagRegister();
uint8_t drv8873GetLastIc1Register();
uint8_t drv8873GetLastIc3Register();
uint8_t drv8873GetLastIc4Register();
uint16_t drv8873GetSpiErrorCounter();
uint16_t drv8873GetLastFaultRaw();
uint16_t drv8873GetLastDiagRaw();
uint16_t drv8873GetLastIc1Raw();
uint16_t drv8873GetLastIc3Raw();
uint16_t drv8873GetLastIc4Raw();

#endif
