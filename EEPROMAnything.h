/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: EEPROMAnything.h
 * Zweck: Generische EEPROM-Lese-/Schreibhilfe.
 *
 * Abgeleitet aus: _EEPROMAnything.h
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2014 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#ifndef EEPROMANYTHING_H
#define EEPROMANYTHING_H

#include <Arduino.h>
#include <EEPROM.h>

// Schreibt Daten nur, wenn sie sich geändert haben (schont das EEPROM)
template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
    EEPROM.put(ee, value);
    return sizeof(value);
}

// Liest die Daten sicher aus
template <class T> int EEPROM_readAnything(int ee, T& value)
{
    EEPROM.get(ee, value);
    return sizeof(value);
}

#endif // EEPROMANYTHING_H
