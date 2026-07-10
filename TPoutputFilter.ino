/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPoutputFilter.ino
 * Zweck: Von Anzeige und Regelung getrennte Ausgabefilterung.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
// TPoutputFilter.ino
// Ausgabe-Filter fuer SD/RS232/Web/USB-Datenausgabe.
// Wichtig: Anzeige, Regelung und Safety bleiben unveraendert.
// =========================================================================

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "TP_T.h"

#define OUT_RAW_PER_SEC_MAX 8
#define OUT_SEC_HISTORY_MAX 10

static output_data_sample_t outLastRaw;
static bool outLastRawValid = false;
static uint32_t outRawSeq = 0;

static uint32_t outSecStartMs = 0;
static uint8_t outSecCount = 0;
static float outSecTM[OUT_RAW_PER_SEC_MAX];
static float outSecTU[OUT_RAW_PER_SEC_MAX];
static float outSecDP[OUT_RAW_PER_SEC_MAX];
static float outSecRH[OUT_RAW_PER_SEC_MAX];
static float outSecP[OUT_RAW_PER_SEC_MAX];

static output_data_sample_t outSecHist[OUT_SEC_HISTORY_MAX];
static uint8_t outSecHistHead = 0;
static uint8_t outSecHistCount = 0;
static uint32_t outSecSeq = 0;

static float outMedianFloat(const float* in, uint8_t n)
{
  if (in == nullptr || n == 0) return NAN;
  if (n > OUT_SEC_HISTORY_MAX) n = OUT_SEC_HISTORY_MAX;

  float v[OUT_SEC_HISTORY_MAX];
  uint8_t m = 0;
  for (uint8_t i = 0; i < n; i++)
  {
    if (isfinite(in[i])) v[m++] = in[i];
  }
  if (m == 0) return NAN;

  for (uint8_t i = 1; i < m; i++)
  {
    float key = v[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && v[j] > key)
    {
      v[j + 1] = v[j];
      j--;
    }
    v[j + 1] = key;
  }

  if (m & 1) return v[m / 2];
  return (v[(m / 2) - 1] + v[m / 2]) * 0.5f;
}

static void outPushSecondHistory(const output_data_sample_t& s)
{
  outSecHist[outSecHistHead] = s;
  outSecHistHead++;
  if (outSecHistHead >= OUT_SEC_HISTORY_MAX) outSecHistHead = 0;
  if (outSecHistCount < OUT_SEC_HISTORY_MAX) outSecHistCount++;
  outSecSeq++;
}

static void outFinalizeCurrentSecond(void)
{
  if (outSecCount == 0) return;

  output_data_sample_t s;
  memset(&s, 0, sizeof(s));
  s.ms = outLastRaw.ms;
  s.rawSeq = outLastRaw.rawSeq;
  s.nSamples = outSecCount;
  s.nSeconds = 1;
  s.tMirror = outMedianFloat(outSecTM, outSecCount);
  s.tAmbient = outMedianFloat(outSecTU, outSecCount);
  s.dewpoint = outMedianFloat(outSecDP, outSecCount);
  s.rh = outMedianFloat(outSecRH, outSecCount);
  s.pressure = outMedianFloat(outSecP, outSecCount);

  float tMin = outSecTM[0];
  float tMax = outSecTM[0];
  float uMin = outSecTU[0];
  float uMax = outSecTU[0];
  for (uint8_t i = 1; i < outSecCount; i++)
  {
    if (outSecTM[i] < tMin) tMin = outSecTM[i];
    if (outSecTM[i] > tMax) tMax = outSecTM[i];
    if (outSecTU[i] < uMin) uMin = outSecTU[i];
    if (outSecTU[i] > uMax) uMax = outSecTU[i];
  }
  s.tMirrorPpMk = (tMax - tMin) * 1000.0f;
  s.tAmbientPpMk = (uMax - uMin) * 1000.0f;

  outPushSecondHistory(s);
  outSecCount = 0;
  outSecStartMs = 0;
}

void outputDataNoteSample(float tMirror, float tAmbient, float dewpoint, float rh, float pressure)
{
  uint32_t nowMs = millis();

  outRawSeq++;
  outLastRaw.ms = nowMs;
  outLastRaw.rawSeq = outRawSeq;
  outLastRaw.nSamples = 1;
  outLastRaw.nSeconds = 0;
  outLastRaw.tMirror = tMirror;
  outLastRaw.tAmbient = tAmbient;
  outLastRaw.dewpoint = dewpoint;
  outLastRaw.rh = rh;
  outLastRaw.pressure = pressure;
  outLastRaw.tMirrorPpMk = 0.0f;
  outLastRaw.tAmbientPpMk = 0.0f;
  outLastRawValid = true;

  if (outSecStartMs == 0) outSecStartMs = nowMs;

  if ((uint32_t)(nowMs - outSecStartMs) >= 1000UL && outSecCount > 0)
  {
    outFinalizeCurrentSecond();
    outSecStartMs = nowMs;
  }

  if (outSecCount < OUT_RAW_PER_SEC_MAX)
  {
    outSecTM[outSecCount] = tMirror;
    outSecTU[outSecCount] = tAmbient;
    outSecDP[outSecCount] = dewpoint;
    outSecRH[outSecCount] = rh;
    outSecP[outSecCount] = pressure;
    outSecCount++;
  }
  else
  {
    // Wenn wider Erwarten mehr als 8 Werte/s kommen, den letzten Slot aktuell halten.
    outSecTM[OUT_RAW_PER_SEC_MAX - 1] = tMirror;
    outSecTU[OUT_RAW_PER_SEC_MAX - 1] = tAmbient;
    outSecDP[OUT_RAW_PER_SEC_MAX - 1] = dewpoint;
    outSecRH[OUT_RAW_PER_SEC_MAX - 1] = rh;
    outSecP[OUT_RAW_PER_SEC_MAX - 1] = pressure;
  }
}

uint32_t outputDataRawSequence(void)
{
  return outRawSeq;
}

uint32_t outputDataSecondSequence(void)
{
  return outSecSeq;
}

static uint8_t outFilterSecondsFromIndex(uint8_t filterIndex)
{
  switch (filterIndex)
  {
    case 0: return 0;
    case 1: return 1;
    case 2: return 3;
    case 3: return 5;
    case 4: return 10;
    default: return 1;
  }
}

bool outputDataGetSample(uint8_t filterIndex, output_data_sample_t* out)
{
  if (out == nullptr) return false;

  // Falls laenger als 1 s kein neuer Rohwert kam, die begonnene Sekunde abschliessen.
  if (outSecStartMs != 0 && outSecCount > 0 && (uint32_t)(millis() - outSecStartMs) >= 1000UL)
  {
    outFinalizeCurrentSecond();
  }

  uint8_t seconds = outFilterSecondsFromIndex(filterIndex);
  if (seconds == 0)
  {
    if (!outLastRawValid) return false;
    *out = outLastRaw;
    return true;
  }

  if (outSecHistCount == 0)
  {
    if (!outLastRawValid) return false;
    *out = outLastRaw;
    return true;
  }

  uint8_t n = seconds;
  if (n > outSecHistCount) n = outSecHistCount;
  if (n > OUT_SEC_HISTORY_MAX) n = OUT_SEC_HISTORY_MAX;

  float tM[OUT_SEC_HISTORY_MAX];
  float tU[OUT_SEC_HISTORY_MAX];
  float dP[OUT_SEC_HISTORY_MAX];
  float rH[OUT_SEC_HISTORY_MAX];
  float pR[OUT_SEC_HISTORY_MAX];
  float tMin = 0.0f;
  float tMax = 0.0f;
  float uMin = 0.0f;
  float uMax = 0.0f;
  uint16_t rawN = 0;

  for (uint8_t i = 0; i < n; i++)
  {
    int16_t idx = (int16_t)outSecHistHead - 1 - i;
    while (idx < 0) idx += OUT_SEC_HISTORY_MAX;
    const output_data_sample_t& s = outSecHist[idx];

    tM[i] = s.tMirror;
    tU[i] = s.tAmbient;
    dP[i] = s.dewpoint;
    rH[i] = s.rh;
    pR[i] = s.pressure;
    rawN += s.nSamples;

    float stMin = s.tMirror - (s.tMirrorPpMk * 0.0005f);
    float stMax = s.tMirror + (s.tMirrorPpMk * 0.0005f);
    float suMin = s.tAmbient - (s.tAmbientPpMk * 0.0005f);
    float suMax = s.tAmbient + (s.tAmbientPpMk * 0.0005f);

    if (i == 0)
    {
      tMin = stMin; tMax = stMax;
      uMin = suMin; uMax = suMax;
    }
    else
    {
      if (stMin < tMin) tMin = stMin;
      if (stMax > tMax) tMax = stMax;
      if (suMin < uMin) uMin = suMin;
      if (suMax > uMax) uMax = suMax;
    }
  }

  memset(out, 0, sizeof(*out));
  out->ms = outLastRaw.ms;
  out->rawSeq = outLastRaw.rawSeq;
  out->nSamples = rawN;
  out->nSeconds = n;
  out->tMirror = outMedianFloat(tM, n);
  out->tAmbient = outMedianFloat(tU, n);
  out->dewpoint = outMedianFloat(dP, n);
  out->rh = outMedianFloat(rH, n);
  out->pressure = outMedianFloat(pR, n);
  out->tMirrorPpMk = (tMax - tMin) * 1000.0f;
  out->tAmbientPpMk = (uMax - uMin) * 1000.0f;
  return true;
}
