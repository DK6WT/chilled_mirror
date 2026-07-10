/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPopticHealth.ino
 * Zweck: Optikqualitaet, Verschmutzungsbewertung und Auto-Cal-Diagnose.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// ============================================================================
// TPopticHealth.ino
// Optikqualitaet / Auto-Cal-Diagnose fuer TP-3000
//
// Alle Grenzwerte sind bewusst als compile-time Konstanten hier gesammelt.
// Damit koennen wir die Bewertung spaeter im Programmcode schnell abstimmen,
// ohne EEPROM-/Menue-Strukturen anzufassen.
// ============================================================================

#include <Arduino.h>
#include <math.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Einstellbare Bewertungsparameter
// ---------------------------------------------------------------------------

// LED-Reserve: bis GOOD volle Punktzahl, ab BAD 0 %.
static const float    OPTIC_Q_LED_GOOD_MA             = 50.0f;
static const float    OPTIC_Q_LED_WARN_MA             = 70.0f;
static const float    OPTIC_Q_LED_BAD_MA              = 80.0f;

// Zielwert-Restfehler Auto-Cal in ADC2-Counts.
static const float    OPTIC_Q_TARGET_GOOD_COUNTS      = 10000.0f;
static const float    OPTIC_Q_TARGET_BAD_COUNTS       = 100000.0f;

// Stabilitaet/Rauschen im 500-ms-Fenster, Peak-to-Peak in ADC2-Counts.
static const float    OPTIC_Q_STABILITY_GOOD_PP       = 10000.0f;
static const float    OPTIC_Q_STABILITY_BAD_PP        = 100000.0f;

// Dunkelwert-Drift gegen letzte gueltige Auto-Cal, ADC2-Counts.
static const float    OPTIC_Q_DARK_GOOD_DELTA         = 5000.0f;
static const float    OPTIC_Q_DARK_BAD_DELTA          = 50000.0f;

// Auto-Cal-Dauer.
static const uint32_t OPTIC_Q_TIME_GOOD_MS            = 2000UL;
static const uint32_t OPTIC_Q_TIME_BAD_MS             = 10000UL;

// Gewichtung Gesamtqualitaet.
static const float    OPTIC_Q_WEIGHT_LED              = 0.35f;
static const float    OPTIC_Q_WEIGHT_TARGET           = 0.25f;
static const float    OPTIC_Q_WEIGHT_STABILITY        = 0.20f;
static const float    OPTIC_Q_WEIGHT_DARK             = 0.10f;
static const float    OPTIC_Q_WEIGHT_TIME             = 0.10f;

// Hauptscreen erst ab kritischer Optik melden. PRUEFEN wird dort bewusst nicht angezeigt.
static const uint8_t  OPTIC_MAIN_WARN_TOTAL_LIMIT     = 60U;
static const uint8_t  OPTIC_MAIN_WARN_MIN_LIMIT       = 50U;

// Fehler nur bei echter Nichterfuellbarkeit.
static const float    OPTIC_HARD_LED_LIMIT_MA         = 79.9f;

// ---------------------------------------------------------------------------
// Statuscodes / Gruende
// ---------------------------------------------------------------------------

enum OpticQReason : uint8_t
{
  OPTIC_REASON_NONE = 0,
  OPTIC_REASON_LED,
  OPTIC_REASON_TARGET,
  OPTIC_REASON_STABILITY,
  OPTIC_REASON_DARK,
  OPTIC_REASON_TIME
};

enum OpticQStatus : uint8_t
{
  OPTIC_STATUS_OK = 0,
  OPTIC_STATUS_CHECK,
  OPTIC_STATUS_CLEAN,
  OPTIC_STATUS_LED_HIGH,
  OPTIC_STATUS_LED_LIMIT,
  OPTIC_STATUS_UNSTABLE,
  OPTIC_STATUS_DARK,
  OPTIC_STATUS_AUTOCAL
};

struct OpticHealthState
{
  bool valid;
  bool hardError;
  bool timeout;
  bool targetReached;

  uint8_t qTotal;
  uint8_t qLed;
  uint8_t qTarget;
  uint8_t qStability;
  uint8_t qDark;
  uint8_t qTime;
  uint8_t qMin;
  uint8_t qMinReason;
  uint8_t status;

  float led_mA;
  float target;
  float brutto;
  float dark;
  float netto;
  float trockenRef;
  float restError;
  float noisePp;
  uint32_t durationMs;
  uint16_t coarseSteps;
  uint16_t fineSteps;
  uint32_t lastMs;

  float previousDark;
  bool previousDarkValid;
};

static OpticHealthState opticHealth = {
  false, false, false, false,
  0, 0, 0, 0, 0, 0, 0, OPTIC_REASON_NONE, OPTIC_STATUS_AUTOCAL,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, false
};

static uint8_t FLASHMEM opticPercentFromRange(float value, float good, float bad)
{
  if (value <= good) return 100;
  if (value >= bad) return 0;

  float q = 100.0f * (bad - value) / (bad - good);
  if (q < 0.0f) q = 0.0f;
  if (q > 100.0f) q = 100.0f;
  return (uint8_t)(q + 0.5f);
}

static uint8_t FLASHMEM opticPercentFromTime(uint32_t value, uint32_t good, uint32_t bad)
{
  if (value <= good) return 100;
  if (value >= bad) return 0;

  float q = 100.0f * (float)(bad - value) / (float)(bad - good);
  if (q < 0.0f) q = 0.0f;
  if (q > 100.0f) q = 100.0f;
  return (uint8_t)(q + 0.5f);
}

static void FLASHMEM opticHealthDetermineMin()
{
  opticHealth.qMin = opticHealth.qLed;
  opticHealth.qMinReason = OPTIC_REASON_LED;

  if (opticHealth.qTarget < opticHealth.qMin)
  {
    opticHealth.qMin = opticHealth.qTarget;
    opticHealth.qMinReason = OPTIC_REASON_TARGET;
  }
  if (opticHealth.qStability < opticHealth.qMin)
  {
    opticHealth.qMin = opticHealth.qStability;
    opticHealth.qMinReason = OPTIC_REASON_STABILITY;
  }
  if (opticHealth.qDark < opticHealth.qMin)
  {
    opticHealth.qMin = opticHealth.qDark;
    opticHealth.qMinReason = OPTIC_REASON_DARK;
  }
  if (opticHealth.qTime < opticHealth.qMin)
  {
    opticHealth.qMin = opticHealth.qTime;
    opticHealth.qMinReason = OPTIC_REASON_TIME;
  }
}

static void FLASHMEM opticHealthDetermineStatus()
{
  opticHealth.status = OPTIC_STATUS_OK;
  opticHealth.hardError = false;

  if (opticHealth.timeout || !opticHealth.targetReached)
  {
    if (opticHealth.led_mA >= OPTIC_HARD_LED_LIMIT_MA)
    {
      opticHealth.status = OPTIC_STATUS_LED_LIMIT;
      opticHealth.hardError = true;
    }
    else
    {
      opticHealth.status = OPTIC_STATUS_AUTOCAL;
      opticHealth.hardError = true;
    }
    return;
  }

  if (opticHealth.qTotal < 40 || opticHealth.qMin < 20)
  {
    switch (opticHealth.qMinReason)
    {
      case OPTIC_REASON_LED:       opticHealth.status = OPTIC_STATUS_LED_HIGH; break;
      case OPTIC_REASON_TARGET:    opticHealth.status = OPTIC_STATUS_AUTOCAL;  break;
      case OPTIC_REASON_STABILITY: opticHealth.status = OPTIC_STATUS_CLEAN;    break;
      case OPTIC_REASON_DARK:      opticHealth.status = OPTIC_STATUS_DARK;     break;
      case OPTIC_REASON_TIME:      opticHealth.status = OPTIC_STATUS_AUTOCAL;  break;
      default:                     opticHealth.status = OPTIC_STATUS_CHECK;    break;
    }
    return;
  }

  if (opticHealth.qTotal < 60 || opticHealth.qMin < 50)
  {
    switch (opticHealth.qMinReason)
    {
      case OPTIC_REASON_LED:       opticHealth.status = OPTIC_STATUS_LED_HIGH; break;
      case OPTIC_REASON_TARGET:    opticHealth.status = OPTIC_STATUS_AUTOCAL;  break;
      case OPTIC_REASON_STABILITY: opticHealth.status = OPTIC_STATUS_CLEAN;    break;
      case OPTIC_REASON_DARK:      opticHealth.status = OPTIC_STATUS_DARK;     break;
      case OPTIC_REASON_TIME:      opticHealth.status = OPTIC_STATUS_AUTOCAL;  break;
      default:                     opticHealth.status = OPTIC_STATUS_CHECK;    break;
    }
    return;
  }

  if (opticHealth.qTotal < 80)
  {
    opticHealth.status = OPTIC_STATUS_CHECK;
    return;
  }

  opticHealth.status = OPTIC_STATUS_OK;
}

void FLASHMEM opticHealthReset()
{
  bool darkValid = opticHealth.previousDarkValid;
  float dark = opticHealth.previousDark;
  memset(&opticHealth, 0, sizeof(opticHealth));
  opticHealth.status = OPTIC_STATUS_AUTOCAL;
  opticHealth.qMinReason = OPTIC_REASON_NONE;
  opticHealth.previousDarkValid = darkValid;
  opticHealth.previousDark = dark;
}

void FLASHMEM opticHealthUpdateFromAutoCal(bool targetReached,
                                  bool timeout,
                                  float led_mA,
                                  float target,
                                  float brutto,
                                  float dark,
                                  float netto,
                                  float trockenRef,
                                  float restError,
                                  float noisePp,
                                  uint32_t durationMs,
                                  uint16_t coarseSteps,
                                  uint16_t fineSteps)
{
  opticHealth.valid = true;
  opticHealth.timeout = timeout;
  opticHealth.targetReached = targetReached;

  opticHealth.led_mA = led_mA;
  opticHealth.target = target;
  opticHealth.brutto = brutto;
  opticHealth.dark = dark;
  opticHealth.netto = netto;
  opticHealth.trockenRef = trockenRef;
  opticHealth.restError = fabsf(restError);
  opticHealth.noisePp = noisePp;
  opticHealth.durationMs = durationMs;
  opticHealth.coarseSteps = coarseSteps;
  opticHealth.fineSteps = fineSteps;
  opticHealth.lastMs = millis();

  opticHealth.qLed       = opticPercentFromRange(led_mA, OPTIC_Q_LED_GOOD_MA, OPTIC_Q_LED_BAD_MA);
  opticHealth.qTarget    = opticPercentFromRange(opticHealth.restError, OPTIC_Q_TARGET_GOOD_COUNTS, OPTIC_Q_TARGET_BAD_COUNTS);
  opticHealth.qStability = opticPercentFromRange(noisePp, OPTIC_Q_STABILITY_GOOD_PP, OPTIC_Q_STABILITY_BAD_PP);

  if (opticHealth.previousDarkValid)
  {
    opticHealth.qDark = opticPercentFromRange(fabsf(dark - opticHealth.previousDark),
                                              OPTIC_Q_DARK_GOOD_DELTA,
                                              OPTIC_Q_DARK_BAD_DELTA);
  }
  else
  {
    opticHealth.qDark = 100;
  }

  opticHealth.qTime = opticPercentFromTime(durationMs, OPTIC_Q_TIME_GOOD_MS, OPTIC_Q_TIME_BAD_MS);

  float total = (OPTIC_Q_WEIGHT_LED       * (float)opticHealth.qLed) +
                (OPTIC_Q_WEIGHT_TARGET    * (float)opticHealth.qTarget) +
                (OPTIC_Q_WEIGHT_STABILITY * (float)opticHealth.qStability) +
                (OPTIC_Q_WEIGHT_DARK      * (float)opticHealth.qDark) +
                (OPTIC_Q_WEIGHT_TIME      * (float)opticHealth.qTime);

  if (total < 0.0f) total = 0.0f;
  if (total > 100.0f) total = 100.0f;
  opticHealth.qTotal = (uint8_t)(total + 0.5f);

  opticHealthDetermineMin();
  opticHealthDetermineStatus();

  if (targetReached && !timeout)
  {
    opticHealth.previousDark = dark;
    opticHealth.previousDarkValid = true;
  }
}

bool opticHealthIsValid()              { return opticHealth.valid; }
uint8_t opticHealthGetTotal()          { return opticHealth.qTotal; }
uint8_t opticHealthGetLed()            { return opticHealth.qLed; }
uint8_t opticHealthGetTarget()         { return opticHealth.qTarget; }
uint8_t opticHealthGetStability()      { return opticHealth.qStability; }
uint8_t opticHealthGetDark()           { return opticHealth.qDark; }
uint8_t opticHealthGetTime()           { return opticHealth.qTime; }
uint8_t opticHealthGetMin()            { return opticHealth.qMin; }
uint8_t opticHealthGetMinReason()      { return opticHealth.qMinReason; }
uint8_t opticHealthGetStatus()         { return opticHealth.status; }
bool opticHealthIsHardError()          { return opticHealth.hardError; }
float opticHealthGetLedMA()            { return opticHealth.led_mA; }
float opticHealthGetTargetRaw()        { return opticHealth.target; }
float opticHealthGetBrutto()           { return opticHealth.brutto; }
float opticHealthGetDarkRaw()          { return opticHealth.dark; }
float opticHealthGetNetto()            { return opticHealth.netto; }
float opticHealthGetTrockenRef()       { return opticHealth.trockenRef; }
float opticHealthGetRestError()        { return opticHealth.restError; }
float opticHealthGetNoisePp()          { return opticHealth.noisePp; }
uint32_t opticHealthGetDurationMs()    { return opticHealth.durationMs; }
uint16_t opticHealthGetCoarseSteps()   { return opticHealth.coarseSteps; }
uint16_t opticHealthGetFineSteps()     { return opticHealth.fineSteps; }
uint32_t opticHealthGetLastMs()        { return opticHealth.lastMs; }

const char* FLASHMEM opticHealthStatusTextDE()
{
  switch (opticHealth.status)
  {
    case OPTIC_STATUS_OK:        return "OPTIK OK";
    case OPTIC_STATUS_CHECK:     return "PRÜFEN";
    case OPTIC_STATUS_CLEAN:     return "REINIGEN";
    case OPTIC_STATUS_LED_HIGH:  return "LED HOCH";
    case OPTIC_STATUS_LED_LIMIT: return "LED LIM";
    case OPTIC_STATUS_UNSTABLE:  return "INSTABIL";
    case OPTIC_STATUS_DARK:      return "DUNKEL";
    case OPTIC_STATUS_AUTOCAL:   return "AUTOCAL";
    default:                     return "PRÜFEN";
  }
}

const char* FLASHMEM opticHealthStatusTextEN()
{
  switch (opticHealth.status)
  {
    case OPTIC_STATUS_OK:        return "OPTIC OK";
    case OPTIC_STATUS_CHECK:     return "CHECK";
    case OPTIC_STATUS_CLEAN:     return "CLEAN";
    case OPTIC_STATUS_LED_HIGH:  return "LED HIGH";
    case OPTIC_STATUS_LED_LIMIT: return "LED LIM";
    case OPTIC_STATUS_UNSTABLE:  return "UNSTABLE";
    case OPTIC_STATUS_DARK:      return "DARK";
    case OPTIC_STATUS_AUTOCAL:   return "AUTOCAL";
    default:                     return "CHECK";
  }
}

bool FLASHMEM opticHealthMainWarningActive()
{
  if (!opticHealth.valid) return false;
  if (opticHealth.hardError) return true;
  if (opticHealth.status == OPTIC_STATUS_OK) return false;
  if (opticHealth.status == OPTIC_STATUS_CHECK) return false;
  return (opticHealth.qTotal < OPTIC_MAIN_WARN_TOTAL_LIMIT) ||
         (opticHealth.qMin   < OPTIC_MAIN_WARN_MIN_LIMIT);
}

const char* FLASHMEM opticHealthMainStatusTextDE()
{
  if (!opticHealthMainWarningActive()) return "";
  return opticHealthStatusTextDE();
}

const char* FLASHMEM opticHealthMainStatusTextEN()
{
  if (!opticHealthMainWarningActive()) return "";
  return opticHealthStatusTextEN();
}
