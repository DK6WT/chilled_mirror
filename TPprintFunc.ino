/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPprintFunc.ino
 * Zweck: Formatierungs- und Druckhilfen fuer Anzeige und Datenausgabe.
 *
 * Abgeleitet aus: PSWRprintFunc.ino
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2014 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

//-----------------------------------------------------------------------------
//			Print SWR, returns string in lcd_buf
//-----------------------------------------------------------------------------


// =============================================================================
// 1. SINNVOLLE PRINT-FUNKTIONEN FÜR DEINEN TAUPUNKTSPIEGEL (Erhalten & Bereinigt)
// =============================================================================

// Temperatur formatieren und ausgeben (Zentel-Grad Eingabe -> Text)
void print_Temp(int16_t Temperature)
{
  int16_t temp_tenths = Temperature;

  if (temp_tenths < 0) temp_tenths *= -1;
  int16_t tempval = temp_tenths / 10;
  temp_tenths = temp_tenths % 10;

  if (Temperature < 0)
  {
    // Durch "\xB0" "C" wird das C nicht mehr in die Hex-Zahl gezogen
    sprintf(lcd_buf, "-%1u.%1u" "\xB0" "C", tempval, temp_tenths);
  }
  else 
  {
    sprintf(lcd_buf, "%2u.%1u" "\xB0" "C", tempval, temp_tenths);
  }
}

// Betriebsspannung formatieren und ausgeben
void print_Volt(int16_t Volt)
{
  int16_t Volt_tenths = Volt;

  if (Volt_tenths < 0) Volt_tenths *= -1;
  int16_t Voltval = Volt_tenths / 10;
  Volt_tenths = Volt_tenths % 10;

  if (Volt < 0)
  {
    sprintf(lcd_buf,"-%1u.%1uV",Voltval,Volt_tenths);
  }
  else sprintf(lcd_buf,"%2u.%1uV",Voltval,Volt_tenths);
}

// Peltierstrom / Lüfterstrom formatieren und ausgeben
void print_Amp(int16_t Ampere1)
{
  int16_t Ampere_tenths = Ampere1;

  if (Ampere_tenths < 0) Ampere_tenths *= -1;
  int16_t Ampereval = Ampere_tenths / 10;
  Ampere_tenths = Ampere_tenths % 10;

  if (Ampere1 < 0)
  {
    sprintf(lcd_buf," -%1u.%1uA",Ampereval,Ampere_tenths);
  }
  else sprintf(lcd_buf,"%2u.%1uA",Ampereval,Ampere_tenths);
}

// =============================================================================
// 2. INAKTIVE DUMMY-FUNKTIONEN (Verhindert Linker-Fehler in anderen Tabs)
// =============================================================================

void print_p_mw(double pwr)
{
  // Deaktiviert: Deine Metrologie-Leistungswerte werden direkt 
  // als ratiometrische Klimadaten berechnet und im Display-Tab ausgegeben.
  sprintf(lcd_buf, " -- ");
}
