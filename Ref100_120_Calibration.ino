/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: Ref100_120_Calibration.ino
 * Zweck: Kalibrierung der Referenzwiderstaende und ADC-Kanaele.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

//
// Referenzwiderstands- und Kanal-Kalibrierung fuer ADC-Ratiometrie
//
// Speichert echte Werte fuer:
//   Ref Low  = z.B. 100.00370 Ohm
//   Ref High = z.B. 119.99840 Ohm
//
// Zusaetzlich ab V3:
//   Kanal A/B Korrektur bei Ref Low und Ref High
//   Korrektur = Soll - gemessen, also spaeter: R_korrigiert = R_gemessen + Korrektur
//
// Aufloesung:
//   0.00001 Ohm
//
// Speicherung separat hinter var_t im EEPROM.
// TP_T.h muss dafuer NICHT geaendert werden.

#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include "TP_T.h"

extern var_t R;


// ============================================================================
// SPEICHERSTRUKTUR
// ============================================================================

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  int32_t ref100_scaled;   // Ohm * 100000
  int32_t ref120_scaled;   // Ohm * 100000

  uint8_t reserved[16];

  uint32_t crc;
} ref_cal_store_v2_t;


typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  int32_t ref100_scaled;       // Ohm * 100000
  int32_t ref120_scaled;       // Ohm * 100000

  uint32_t cal_date_yyyymmdd;  // 0 = unbekannt

  int32_t chA_corr100_scaled;  // Ohm * 100000, Korrektur = Soll - gemessen
  int32_t chA_corr120_scaled;
  int32_t chB_corr100_scaled;
  int32_t chB_corr120_scaled;

  uint8_t reserved[16];

  uint32_t crc;
} ref_cal_store_t;


static ref_cal_store_t ref_cal;
static bool ref_cal_loaded = false;

static const uint32_t REF_CAL_MAGIC = 0x52454631UL; // "REF1"
static const uint16_t REF_CAL_VERSION = 3;

#define REF_CAL_SLOT_MAGIC   0x52464142UL   // 'RFAB'
#define REF_CAL_SLOT_VERSION 1U
#define REF_CAL_SLOT_A_ADDR  192
#define REF_CAL_SLOT_B_ADDR  288
#define REF_CAL_SLOT_SIZE    96

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  ref_cal_store_t payload;
  uint32_t crc;
} ref_cal_slot_t;

static_assert(sizeof(ref_cal_slot_t) <= REF_CAL_SLOT_SIZE,
              "Ref EEPROM slot too small");

static uint32_t ref_cal_sequence = 0UL;
static uint8_t  ref_cal_active_slot = 0U;

static int FLASHMEM refCalSlotAddr(uint8_t slot)
{
  return slot ? REF_CAL_SLOT_B_ADDR : REF_CAL_SLOT_A_ADDR;
}


// ============================================================================
// CRC
// ============================================================================

static uint32_t FLASHMEM refCalCrcBerechnenBuf(const uint8_t* p, size_t n)
{
  uint32_t crc = 2166136261UL;

  for (size_t i = 0; i < n; i++)
  {
    crc ^= p[i];
    crc *= 16777619UL;
  }

  return crc;
}

static uint32_t FLASHMEM refCalCrcBerechnen()
{
  return refCalCrcBerechnenBuf((const uint8_t*)&ref_cal,
                               sizeof(ref_cal_store_t) - sizeof(uint32_t));
}

static uint32_t FLASHMEM refCalCrcBerechnenV2(const void* old_cal_ptr)
{
  const ref_cal_store_v2_t* old_cal = (const ref_cal_store_v2_t*)old_cal_ptr;
  return refCalCrcBerechnenBuf((const uint8_t*)old_cal,
                               sizeof(ref_cal_store_v2_t) - sizeof(uint32_t));
}


// ============================================================================
// PLAUSIBILITAET
// ============================================================================

static const int32_t REF_CAL_LOW_MIN_SCALED   =  6000000L;  //  60.00000 Ohm
static const int32_t REF_CAL_LOW_MAX_SCALED   = 11000000L;  // 110.00000 Ohm
static const int32_t REF_CAL_HIGH_MIN_SCALED  = 11500000L;  // 115.00000 Ohm
static const int32_t REF_CAL_HIGH_MAX_SCALED  = 15200000L;  // 152.00000 Ohm
static const int32_t REF_CAL_MIN_SPAN_SCALED  =   500000L;  //   5.00000 Ohm

static bool FLASHMEM refCalScaled100Plausibel(int32_t v)
{
  // Ref Low: 60.00000 bis 110.00000 Ohm
  return (v >= REF_CAL_LOW_MIN_SCALED && v <= REF_CAL_LOW_MAX_SCALED);
}

static bool FLASHMEM refCalScaled120Plausibel(int32_t v)
{
  // Ref High: 115.00000 bis 152.00000 Ohm
  return (v >= REF_CAL_HIGH_MIN_SCALED && v <= REF_CAL_HIGH_MAX_SCALED);
}

static bool FLASHMEM refCalCorrPlausibel(int32_t v)
{
  // +/-0.10000 Ohm ist fuer die Kanal-Restkorrektur schon sehr gross,
  // verhindert aber grobe Fehleingaben.
  return (v >= -10000L && v <= 10000L);
}

static bool FLASHMEM refCalDatePlausibel(uint32_t yyyymmdd)
{
  if (yyyymmdd == 0) return true;

  uint16_t y = (uint16_t)(yyyymmdd / 10000UL);
  uint8_t m  = (uint8_t)((yyyymmdd / 100UL) % 100UL);
  uint8_t d  = (uint8_t)(yyyymmdd % 100UL);

  return (y >= 2024 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31);
}


// ============================================================================
// DEFAULTWERTE
// ============================================================================

static void FLASHMEM refCalDefault()
{
  memset(&ref_cal, 0, sizeof(ref_cal));

  ref_cal.magic = REF_CAL_MAGIC;
  ref_cal.version = REF_CAL_VERSION;
  ref_cal.size = sizeof(ref_cal_store_t);

  ref_cal.ref100_scaled = 10000000L; // 100.00000 Ohm
  ref_cal.ref120_scaled = 12000000L; // 120.00000 Ohm

  ref_cal.cal_date_yyyymmdd = 0;

  ref_cal.chA_corr100_scaled = 0;
  ref_cal.chA_corr120_scaled = 0;
  ref_cal.chB_corr100_scaled = 0;
  ref_cal.chB_corr120_scaled = 0;

  ref_cal.crc = refCalCrcBerechnen();
}


// ============================================================================
// VALIDIERUNG
// ============================================================================

static bool FLASHMEM refCalValidateStore(const ref_cal_store_t& st)
{
  if (st.magic != REF_CAL_MAGIC) return false;
  if (st.version != REF_CAL_VERSION) return false;
  if (st.size != sizeof(ref_cal_store_t)) return false;

  uint32_t crc = refCalCrcBerechnenBuf((const uint8_t*)&st,
                                       sizeof(ref_cal_store_t) - sizeof(uint32_t));
  if (crc != st.crc) return false;

  if (!refCalScaled100Plausibel(st.ref100_scaled)) return false;
  if (!refCalScaled120Plausibel(st.ref120_scaled)) return false;
  if ((st.ref120_scaled - st.ref100_scaled) < REF_CAL_MIN_SPAN_SCALED) return false;
  if (!refCalDatePlausibel(st.cal_date_yyyymmdd)) return false;

  if (!refCalCorrPlausibel(st.chA_corr100_scaled)) return false;
  if (!refCalCorrPlausibel(st.chA_corr120_scaled)) return false;
  if (!refCalCorrPlausibel(st.chB_corr100_scaled)) return false;
  if (!refCalCorrPlausibel(st.chB_corr120_scaled)) return false;

  return true;
}

static bool FLASHMEM refCalValidate()
{
  return refCalValidateStore(ref_cal);
}

static bool FLASHMEM refCalReadSlot(uint8_t slot, ref_cal_slot_t& out)
{
  EEPROM.get(refCalSlotAddr(slot), out);
  if (out.magic != REF_CAL_SLOT_MAGIC) return false;
  if (out.version != REF_CAL_SLOT_VERSION) return false;
  if (out.size != sizeof(ref_cal_store_t)) return false;
  const uint32_t slotCrc = refCalCrcBerechnenBuf((const uint8_t*)&out,
                                                 sizeof(out) - sizeof(out.crc));
  if (slotCrc != out.crc) return false;
  return refCalValidateStore(out.payload);
}

static bool FLASHMEM refCalValidateV2(const void* old_cal_ptr)
{
  const ref_cal_store_v2_t& old_cal = *(const ref_cal_store_v2_t*)old_cal_ptr;
  if (old_cal.magic != REF_CAL_MAGIC) return false;
  if (old_cal.version != 2) return false;
  if (old_cal.size != sizeof(ref_cal_store_v2_t)) return false;

  uint32_t crc = refCalCrcBerechnenV2(&old_cal);

  if (crc != old_cal.crc) return false;

  if (!refCalScaled100Plausibel(old_cal.ref100_scaled)) return false;
  if (!refCalScaled120Plausibel(old_cal.ref120_scaled)) return false;

  if ((old_cal.ref120_scaled - old_cal.ref100_scaled) < REF_CAL_MIN_SPAN_SCALED) return false;

  return true;
}

// Alte EEPROM-Version 1 nutzte Ohm * 10000.
static bool FLASHMEM refCalValidateV1(const void* old_cal_ptr)
{
  const ref_cal_store_v2_t& old_cal = *(const ref_cal_store_v2_t*)old_cal_ptr;
  if (old_cal.magic != REF_CAL_MAGIC) return false;
  if (old_cal.version != 1) return false;
  if (old_cal.size != sizeof(ref_cal_store_v2_t)) return false;

  uint32_t crc = refCalCrcBerechnenV2(&old_cal);

  if (crc != old_cal.crc) return false;

  if (old_cal.ref100_scaled < 900000L || old_cal.ref100_scaled > 1100000L) return false;
  if (old_cal.ref120_scaled < 1100000L || old_cal.ref120_scaled > 1300000L) return false;

  if ((old_cal.ref120_scaled - old_cal.ref100_scaled) < 50000L) return false;

  return true;
}


// ============================================================================
// SPEICHERN / LADEN
// ============================================================================

static void FLASHMEM refCalSave()
{
  ref_cal.magic = REF_CAL_MAGIC;
  ref_cal.version = REF_CAL_VERSION;
  ref_cal.size = sizeof(ref_cal_store_t);
  ref_cal.crc = refCalCrcBerechnen();
  ref_cal_loaded = true;
  tpMetrologyConfigSave();
}


static void FLASHMEM refCalEnsureLoaded()
{
  if (ref_cal_loaded) return;
  tpMetrologyConfigEnsureLoaded();
}



// ============================================================================
// OEFFENTLICH: LESEN
// ============================================================================

double refCalGet100Ohm()
{
  refCalEnsureLoaded();
  return (double)ref_cal.ref100_scaled / 100000.0;
}

double refCalGet120Ohm()
{
  refCalEnsureLoaded();
  return (double)ref_cal.ref120_scaled / 100000.0;
}

void FLASHMEM refCalGetScaled(int32_t* ref100, int32_t* ref120)
{
  refCalEnsureLoaded();

  if (ref100) *ref100 = ref_cal.ref100_scaled;
  if (ref120) *ref120 = ref_cal.ref120_scaled;
}

void refCalGetAllScaled(uint32_t* date_yyyymmdd,
                        int32_t* ref100,
                        int32_t* ref120,
                        int32_t* chA_corr100,
                        int32_t* chA_corr120,
                        int32_t* chB_corr100,
                        int32_t* chB_corr120)
{
  refCalEnsureLoaded();

  if (date_yyyymmdd) *date_yyyymmdd = ref_cal.cal_date_yyyymmdd;
  if (ref100) *ref100 = ref_cal.ref100_scaled;
  if (ref120) *ref120 = ref_cal.ref120_scaled;
  if (chA_corr100) *chA_corr100 = ref_cal.chA_corr100_scaled;
  if (chA_corr120) *chA_corr120 = ref_cal.chA_corr120_scaled;
  if (chB_corr100) *chB_corr100 = ref_cal.chB_corr100_scaled;
  if (chB_corr120) *chB_corr120 = ref_cal.chB_corr120_scaled;
}

int32_t refCalGetChannelCorrectionScaled(uint8_t channel, double ohm)
{
  refCalEnsureLoaded();

  if (!isfinite(ohm)) return 0;

  int32_t c100 = (channel == 0) ? ref_cal.chA_corr100_scaled : ref_cal.chB_corr100_scaled;
  int32_t c120 = (channel == 0) ? ref_cal.chA_corr120_scaled : ref_cal.chB_corr120_scaled;

  double refLow = (double)ref_cal.ref100_scaled / 100000.0;
  double refHigh = (double)ref_cal.ref120_scaled / 100000.0;
  double refSpan = refHigh - refLow;
  if (!isfinite(refLow) || !isfinite(refHigh) || refSpan < 1.0) return 0;

  double f = (ohm - refLow) / refSpan;
  double corr = (double)c100 + f * ((double)c120 - (double)c100);

  if (!isfinite(corr)) return 0;
  if (corr >  1000000.0) corr =  1000000.0;
  if (corr < -1000000.0) corr = -1000000.0;

  return (int32_t)lround(corr);
}

double refCalApplyChannelCorrection(uint8_t channel, double ohm)
{
  int32_t corr = refCalGetChannelCorrectionScaled(channel, ohm);
  return ohm + ((double)corr / 100000.0);
}


// ============================================================================
// OEFFENTLICH: SCHREIBEN
// ============================================================================

bool FLASHMEM refCalSet100Scaled(int32_t ref100_scaled)
{
  refCalEnsureLoaded();

  if (!refCalScaled100Plausibel(ref100_scaled)) return false;

  // Ref High muss plausibel ueber Ref Low bleiben.
  if ((ref_cal.ref120_scaled - ref100_scaled) < REF_CAL_MIN_SPAN_SCALED) return false;

  ref_cal.ref100_scaled = ref100_scaled;
  refCalSave();

  return true;
}

bool FLASHMEM refCalSet120Scaled(int32_t ref120_scaled)
{
  refCalEnsureLoaded();

  if (!refCalScaled120Plausibel(ref120_scaled)) return false;

  // Ref High muss plausibel ueber Ref Low bleiben.
  if ((ref120_scaled - ref_cal.ref100_scaled) < REF_CAL_MIN_SPAN_SCALED) return false;

  ref_cal.ref120_scaled = ref120_scaled;
  refCalSave();

  return true;
}

bool FLASHMEM refCalSetScaled(int32_t ref100_scaled, int32_t ref120_scaled)
{
  refCalEnsureLoaded();

  if (!refCalScaled100Plausibel(ref100_scaled)) return false;
  if (!refCalScaled120Plausibel(ref120_scaled)) return false;

  if ((ref120_scaled - ref100_scaled) < REF_CAL_MIN_SPAN_SCALED) return false;

  ref_cal.ref100_scaled = ref100_scaled;
  ref_cal.ref120_scaled = ref120_scaled;

  refCalSave();

  return true;
}

bool refCalSetAllScaled(uint32_t date_yyyymmdd,
                        int32_t ref100_scaled,
                        int32_t ref120_scaled,
                        int32_t chA_corr100_scaled,
                        int32_t chA_corr120_scaled,
                        int32_t chB_corr100_scaled,
                        int32_t chB_corr120_scaled)
{
  refCalEnsureLoaded();

  if (!refCalScaled100Plausibel(ref100_scaled)) return false;
  if (!refCalScaled120Plausibel(ref120_scaled)) return false;
  if ((ref120_scaled - ref100_scaled) < REF_CAL_MIN_SPAN_SCALED) return false;

  if (!refCalDatePlausibel(date_yyyymmdd)) return false;

  if (!refCalCorrPlausibel(chA_corr100_scaled)) return false;
  if (!refCalCorrPlausibel(chA_corr120_scaled)) return false;
  if (!refCalCorrPlausibel(chB_corr100_scaled)) return false;
  if (!refCalCorrPlausibel(chB_corr120_scaled)) return false;

  ref_cal.ref100_scaled = ref100_scaled;
  ref_cal.ref120_scaled = ref120_scaled;
  ref_cal.cal_date_yyyymmdd = date_yyyymmdd;
  ref_cal.chA_corr100_scaled = chA_corr100_scaled;
  ref_cal.chA_corr120_scaled = chA_corr120_scaled;
  ref_cal.chB_corr100_scaled = chB_corr100_scaled;
  ref_cal.chB_corr120_scaled = chB_corr120_scaled;

  refCalSave();

  return true;
}


// ============================================================================
// ZENTRALER METROLOGIE-/KALIBRIERBLOCK EEPROM V12
// ============================================================================
// Gemeinsamer A/B-Snapshot fuer:
// - Pt100-2P-Kalibrierung
// - Ref100/Ref120 + Kanal-Korrekturen
// - Pt100-R0
// - Taupunkt-/Frostpunkt-Offset
// Dadurch weniger Overhead und ein konsistenter metrologischer Datensatz.

#define TP_METRO_CFG_SLOT_MAGIC   0x544D4554UL   // 'TMET'
#define TP_METRO_CFG_SLOT_VERSION 1U
#define TP_METRO_CFG_SLOT_A_ADDR  0
#define TP_METRO_CFG_SLOT_B_ADDR  384
#define TP_METRO_CFG_SLOT_SIZE    384

typedef struct
{
  pt100_2p_store_t        p2;
  ref_cal_store_t         ref;
  pt100_r0_store_t        r0;
  taupunkt_offset_store_t tau;
  uint8_t                 reserved[128];
} tp_metrology_payload_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  tp_metrology_payload_t payload;
  uint32_t crc;
} tp_metrology_slot_t;

static_assert(sizeof(tp_metrology_slot_t) <= TP_METRO_CFG_SLOT_SIZE,
              "Metrology EEPROM slot too small");

static uint32_t tp_metrology_sequence = 0UL;
static uint8_t  tp_metrology_active_slot = 0U;
static bool     tp_metrology_loaded = false;

static int FLASHMEM tpMetrologySlotAddr(uint8_t slot)
{
  return slot ? TP_METRO_CFG_SLOT_B_ADDR : TP_METRO_CFG_SLOT_A_ADDR;
}

static void FLASHMEM tpMetrologyDefaultsToRuntime(void)
{
  pt100Cal2Default();
  refCalDefault();
  pt100R0DefaultFromLegacy();
  taupunktOffsetDefault();

  pt100_2p_loaded = true;
  ref_cal_loaded = true;
  pt100_r0_loaded = true;
  taupunkt_offset_loaded = true;
}

static void FLASHMEM tpMetrologyPayloadFromRuntime(tp_metrology_payload_t& payload)
{
  memset(&payload, 0, sizeof(payload));

  pt100_2p.magic = PT100_2P_MAGIC;
  pt100_2p.version = PT100_2P_VERSION;
  pt100_2p.size = sizeof(pt100_2p_store_t);
  pt100_2p.crc = pt100Cal2CrcBerechnen();

  ref_cal.magic = REF_CAL_MAGIC;
  ref_cal.version = REF_CAL_VERSION;
  ref_cal.size = sizeof(ref_cal_store_t);
  ref_cal.crc = refCalCrcBerechnen();

  pt100_r0.magic = PT100_R0_MAGIC;
  pt100_r0.version = PT100_R0_VERSION;
  pt100_r0.size = sizeof(pt100_r0_store_t);
  pt100_r0.crc = pt100R0CrcBerechnen();

  taupunkt_offset.magic = TAUPUNKT_OFFSET_MAGIC;
  taupunkt_offset.version = TAUPUNKT_OFFSET_VERSION;
  taupunkt_offset.size = sizeof(taupunkt_offset_store_t);
  taupunkt_offset.crc = taupunktOffsetCrcBerechnen();

  payload.p2 = pt100_2p;
  payload.ref = ref_cal;
  payload.r0 = pt100_r0;
  payload.tau = taupunkt_offset;
}

static bool FLASHMEM tpMetrologyPayloadValid(const tp_metrology_payload_t& payload)
{
  if (!pt100Cal2ValidateStore(payload.p2)) return false;
  if (!refCalValidateStore(payload.ref)) return false;
  if (!pt100R0ValidateStore(payload.r0)) return false;
  if (!taupunktOffsetValidateStore(payload.tau)) return false;
  return true;
}

static void FLASHMEM tpMetrologyApplyPayload(const tp_metrology_payload_t& payload)
{
  pt100_2p = payload.p2;
  ref_cal = payload.ref;
  pt100_r0 = payload.r0;
  taupunkt_offset = payload.tau;

  pt100_2p_loaded = true;
  ref_cal_loaded = true;
  pt100_r0_loaded = true;
  taupunkt_offset_loaded = true;
}

static bool FLASHMEM tpMetrologyReadSlot(uint8_t slot, tp_metrology_slot_t& out)
{
  EEPROM.get(tpMetrologySlotAddr(slot), out);
  if (out.magic != TP_METRO_CFG_SLOT_MAGIC) return false;
  if (out.version != TP_METRO_CFG_SLOT_VERSION) return false;
  if (out.size != sizeof(tp_metrology_payload_t)) return false;
  const uint32_t crc = refCalCrcBerechnenBuf((const uint8_t*)&out,
                                             sizeof(out) - sizeof(out.crc));
  if (out.crc != crc) return false;
  return tpMetrologyPayloadValid(out.payload);
}

void FLASHMEM tpMetrologyConfigSave(void)
{
  tp_metrology_payload_t payload;
  tpMetrologyPayloadFromRuntime(payload);

  tp_metrology_slot_t slotA;
  tp_metrology_slot_t slotB;
  const bool validA = tpMetrologyReadSlot(0, slotA);
  const bool validB = tpMetrologyReadSlot(1, slotB);

  uint32_t nextSeq = tp_metrology_sequence;
  if (validA && slotA.sequence > nextSeq) nextSeq = slotA.sequence;
  if (validB && slotB.sequence > nextSeq) nextSeq = slotB.sequence;
  nextSeq++;
  if (nextSeq == 0UL) nextSeq = 1UL;

  uint8_t targetSlot;
  if (!validA) targetSlot = 0;
  else if (!validB) targetSlot = 1;
  else targetSlot = (slotA.sequence <= slotB.sequence) ? 0 : 1;

  tp_metrology_slot_t out;
  memset(&out, 0, sizeof(out));
  out.magic = TP_METRO_CFG_SLOT_MAGIC;
  out.version = TP_METRO_CFG_SLOT_VERSION;
  out.size = sizeof(tp_metrology_payload_t);
  out.sequence = nextSeq;
  out.payload = payload;
  out.crc = refCalCrcBerechnenBuf((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));

  EEPROM.put(tpMetrologySlotAddr(targetSlot), out);
  tp_metrology_sequence = nextSeq;
  tp_metrology_active_slot = targetSlot;
  tp_metrology_loaded = true;
}

void FLASHMEM tpMetrologyConfigEnsureLoaded(void)
{
  if (tp_metrology_loaded)
  {
    pt100_2p_loaded = true;
    ref_cal_loaded = true;
    pt100_r0_loaded = true;
    taupunkt_offset_loaded = true;
    return;
  }

  tp_metrology_slot_t slotA;
  tp_metrology_slot_t slotB;
  const bool validA = tpMetrologyReadSlot(0, slotA);
  const bool validB = tpMetrologyReadSlot(1, slotB);

  if (!validA && !validB)
  {
    tp_metrology_sequence = 0UL;
    tp_metrology_active_slot = 0U;
    tpMetrologyDefaultsToRuntime();
    // Erstinitialisierung: beide A/B-Slots direkt anlegen.
    tpMetrologyConfigSave();
    tpMetrologyConfigSave();
  }
  else if (validA && (!validB || slotA.sequence >= slotB.sequence))
  {
    tpMetrologyApplyPayload(slotA.payload);
    tp_metrology_sequence = slotA.sequence;
    tp_metrology_active_slot = 0U;
    tp_metrology_loaded = true;
    if (!validB)
    {
      tpMetrologyConfigSave();
    }
  }
  else
  {
    tpMetrologyApplyPayload(slotB.payload);
    tp_metrology_sequence = slotB.sequence;
    tp_metrology_active_slot = 1U;
    tp_metrology_loaded = true;
    if (!validA)
    {
      tpMetrologyConfigSave();
    }
  }
}
