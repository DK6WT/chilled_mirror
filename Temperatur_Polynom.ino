/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: Temperatur_Polynom.ino
 * Zweck: Pt100-Temperaturberechnung nach Callendar-Van-Dusen.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

//
// Pt100 Callendar-Van-Dusen Temperaturberechnung
// mit R0-Kalibrierung aus dem separaten Pt100-R0-EEPROM-Block
// und optionaler 2-Punkt-Korrektur aus PT100_2P_Calibration.ino

#include <Arduino.h>
#include <math.h>
#include "TP_T.h"

// Externe Ohm-Werte aus dem Regelungs-Tab
extern float ohmPt100_1;
extern float ohmPt100_2;

// Externe Ziel-Temperaturen
extern float tempSpiegel;
extern float tempUmgebung;

// Kalibrierdaten aus EEPROM/Main
extern var_t R;

// Pt100-R0-Kalibrierung aus separatem EEPROM-Block
extern double pt100R0GetOhm(uint8_t sensor);

// 2-Punkt-Korrektur
extern double pt100Cal2Apply(uint8_t sensor, double tCvd);


// ============================================================================
// CALLENDAR-VAN-DUSEN KONSTANTEN
// Pt100 nach IEC / DIN EN 60751
// ============================================================================

static const double CVD_A = 3.9083e-3;
static const double CVD_B = -5.775e-7;
static const double CVD_C = -4.183e-12;

static const double PT100_T_MIN = -200.0;
static const double PT100_T_MAX = 850.0;


// ============================================================================
// SENSOR-R0 AUS KALIBRIERDATEN HOLEN
// ============================================================================

static double holeKalibriertesR0(uint8_t sensorIndex)
{
  if (sensorIndex > 1)
  {
    sensorIndex = 0;
  }

  // R0 liegt seit V33 getrennt von var_t im EEPROM.
  // R.fühler_cal[] ist nur noch ein reservierter/deprecated Platzhalter
  // und darf für die Temperaturrechnung nicht mehr verwendet werden.
  double r0 = pt100R0GetOhm(sensorIndex);

  if (!isfinite(r0)) return 100.0;
  if (r0 < 80.0)     return 100.0;
  if (r0 > 120.0)    return 100.0;

  return r0;
}


// ============================================================================
// CVD VORWÄRTS: TEMPERATUR -> OHM
// ============================================================================

static double cvdCelsiusZuOhm(double tempCelsius, double r0)
{
  double T = tempCelsius;
  double T2 = T * T;

  if (T >= 0.0)
  {
    return r0 * (1.0 + CVD_A * T + CVD_B * T2);
  }
  else
  {
    double T3 = T2 * T;
    return r0 * (1.0 + CVD_A * T + CVD_B * T2 + CVD_C * (T - 100.0) * T3);
  }
}


// ============================================================================
// CVD ABLEITUNG dR/dT
// ============================================================================

static double cvdAbleitung(double tempCelsius, double r0)
{
  double T = tempCelsius;
  double T2 = T * T;
  double T3 = T2 * T;

  if (T >= 0.0)
  {
    return r0 * (CVD_A + 2.0 * CVD_B * T);
  }
  else
  {
    return r0 * (CVD_A + 2.0 * CVD_B * T + CVD_C * (4.0 * T3 - 300.0 * T2));
  }
}


// ============================================================================
// HOCHPRÄZISE WANDLUNG OHM -> CELSIUS
// ============================================================================

double cvdOhmZuCelsiusDouble(double rIst, double r0)
{
  if (!isfinite(rIst)) return NAN;
  if (!isfinite(r0))   return NAN;

  if (rIst <= 0.0) return NAN;
  if (r0   <= 0.0) return NAN;

  double rMin = cvdCelsiusZuOhm(PT100_T_MIN, r0);
  double rMax = cvdCelsiusZuOhm(PT100_T_MAX, r0);

  if (rIst < rMin || rIst > rMax)
  {
    return NAN;
  }

  // >= 0 °C: direkte quadratische Lösung
  if (rIst >= r0)
  {
    double ratio = rIst / r0;
    double c = 1.0 - ratio;

    double diskriminante = (CVD_A * CVD_A) - (4.0 * CVD_B * c);

    if (diskriminante < 0.0)
    {
      return NAN;
    }

    double wurzel = sqrt(diskriminante);

    double T = (-CVD_A + wurzel) / (2.0 * CVD_B);

    if (!isfinite(T)) return NAN;
    if (T < 0.0) T = 0.0;
    if (T > PT100_T_MAX) return NAN;

    return T;
  }

  // < 0 °C: Newton mit Bracket-Schutz
  double lo = PT100_T_MIN;
  double hi = 0.0;

  double T = (rIst - r0) / (r0 * CVD_A);

  if (T < lo) T = lo;
  if (T > hi) T = hi;

  for (uint8_t i = 0; i < 20; i++)
  {
    double rSim = cvdCelsiusZuOhm(T, r0);
    double f = rSim - rIst;

    if (fabs(f) < 1e-7)
    {
      break;
    }

    if (f > 0.0)
    {
      hi = T;
    }
    else
    {
      lo = T;
    }

    double d = cvdAbleitung(T, r0);
    double neuerT;

    if (d != 0.0 && isfinite(d))
    {
      neuerT = T - (f / d);
    }
    else
    {
      neuerT = (lo + hi) * 0.5;
    }

    if (!isfinite(neuerT) || neuerT <= lo || neuerT >= hi)
    {
      neuerT = (lo + hi) * 0.5;
    }

    T = neuerT;
  }

  if (T < PT100_T_MIN || T > 0.0)
  {
    return NAN;
  }

  return T;
}


// ============================================================================
// KOMPATIBLE FLOAT-FUNKTION
// ============================================================================

float cvdOhmZuCelsius(double rIst)
{
  double T = cvdOhmZuCelsiusDouble(rIst, 100.0);

  if (!isfinite(T))
  {
    return NAN;
  }

  return (float)T;
}


// ============================================================================
// HAUPT-AUFRUF FÜR DIE TEMPERATURBERECHNUNG
// ============================================================================

void berechnePräziseTemperaturen()
{
  double r0Spiegel  = holeKalibriertesR0(0);
  double r0Umgebung = holeKalibriertesR0(1);

  double tSpiegelCvd  = cvdOhmZuCelsiusDouble((double)ohmPt100_1, r0Spiegel);
  double tUmgebungCvd = cvdOhmZuCelsiusDouble((double)ohmPt100_2, r0Umgebung);

  if (isfinite(tSpiegelCvd))
  {
    double t = pt100Cal2Apply(0, tSpiegelCvd);
    tempSpiegel = isfinite(t) ? (float)t : NAN;
  }
  else
  {
    tempSpiegel = NAN;
  }

  if (isfinite(tUmgebungCvd))
  {
    double t = pt100Cal2Apply(1, tUmgebungCvd);
    tempUmgebung = isfinite(t) ? (float)t : NAN;
  }
  else
  {
    tempUmgebung = NAN;
  }
}