/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: PT100_2P_Calibration.ino
 * Zweck: R0- und Zwei-Punkt-Kalibrierung der beiden Pt100-Kanaele.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

//
// PT100 2-Punkt- und R0-Kalibrierung
//
// Version 2:
// - Arduino-INO-Preprocessor-sicher
// - keine Funktion verwendet pt100_2p_store_t als Parameter
// - dadurch kein Fehler mehr:
//   'pt100_2p_store_t' does not name a type
//
// Speichert die 2-Punkt-Kalibrierung und die R0-Werte getrennt von var_t
// im EEPROM. Dadurch bleiben R0-Werte bei Werkseinstellungen erhalten und
// TP_T.h muss fuer das EEPROM-Layout NICHT umgebaut werden.
//
// Werte im Menü:
//   Soll1, Ist1, Soll2, Ist2
//   Bereich -30.000 C bis +90.000 C
//   intern als m°C gespeichert.
//
// Anwendung:
//   T_final = T_cvd * gain + offset
//
// Sicherheitsregeln:
//   |Soll2 - Soll1| >= 30.000 C
//   |Ist2  - Ist1 | >= 25.000 C

#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include "TP_T.h"

extern var_t R;

// Zentraler Metrologie-/Kalibrierblock (A/B) liegt ab V0.50.0_22 im EEPROM.
// Die einzelnen Kalibrierfunktionen arbeiten weiterhin mit ihren bestehenden
// RAM-Strukturen; geladen/gespeichert wird aber als gemeinsamer Snapshot.
void tpMetrologyConfigEnsureLoaded(void);
void tpMetrologyConfigSave(void);

static uint32_t FLASHMEM tpCalCrcBytes(const void* data, size_t n)
{
  const uint8_t* p = (const uint8_t*)data;
  uint32_t crc = 2166136261UL;
  for (size_t i = 0; i < n; i++)
  {
    crc ^= p[i];
    crc *= 16777619UL;
  }
  return crc;
}


// ============================================================================
// SPEICHERSTRUKTUR
// ============================================================================

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  int32_t soll1_mC[2];
  int32_t ist1_mC[2];
  int32_t soll2_mC[2];
  int32_t ist2_mC[2];

  uint8_t aktiv[2];
  uint8_t reserved[6];

  uint32_t crc;
} pt100_2p_store_t;


static pt100_2p_store_t pt100_2p;
static bool pt100_2p_loaded = false;

static const uint32_t PT100_2P_MAGIC = 0x32505431UL;
static const uint16_t PT100_2P_VERSION = 1;

// ============================================================================
// CRC ÜBER GLOBALE STRUKTUR
// ============================================================================
// Wichtig:
// Kein Struct-Typ als Funktionsparameter.
// Das verhindert den Arduino-INO-Preprocessor-Fehler.

static uint32_t FLASHMEM pt100Cal2CrcBerechnen()
{
  return tpCalCrcBytes(&pt100_2p, sizeof(pt100_2p_store_t) - sizeof(uint32_t));
}


// ============================================================================
// PLAUSIBILITÄT
// ============================================================================

static bool FLASHMEM pt100Cal2ScaledPlausibel(int32_t v)
{
  return (v >= -30000L && v <= 90000L);
}


// ============================================================================
// DEFAULTWERTE
// ============================================================================

static void FLASHMEM pt100Cal2Default()
{
  memset(&pt100_2p, 0, sizeof(pt100_2p));

  pt100_2p.magic = PT100_2P_MAGIC;
  pt100_2p.version = PT100_2P_VERSION;
  pt100_2p.size = sizeof(pt100_2p_store_t);

  for (uint8_t i = 0; i < 2; i++)
  {
    pt100_2p.soll1_mC[i] = 20000L;
    pt100_2p.ist1_mC[i]  = 20000L;
    pt100_2p.soll2_mC[i] = 50000L;
    pt100_2p.ist2_mC[i]  = 50000L;
    pt100_2p.aktiv[i] = 0;
  }

  pt100_2p.crc = pt100Cal2CrcBerechnen();
}


// ============================================================================
// VALIDIERUNG
// ============================================================================

static bool FLASHMEM pt100Cal2ValidateStore(const pt100_2p_store_t& st)
{
  if (st.magic != PT100_2P_MAGIC) return false;
  if (st.version != PT100_2P_VERSION) return false;
  if (st.size != sizeof(pt100_2p_store_t)) return false;

  uint32_t crc = tpCalCrcBytes(&st, sizeof(pt100_2p_store_t) - sizeof(uint32_t));
  if (crc != st.crc) return false;

  for (uint8_t i = 0; i < 2; i++)
  {
    if (!pt100Cal2ScaledPlausibel(st.soll1_mC[i])) return false;
    if (!pt100Cal2ScaledPlausibel(st.ist1_mC[i]))  return false;
    if (!pt100Cal2ScaledPlausibel(st.soll2_mC[i])) return false;
    if (!pt100Cal2ScaledPlausibel(st.ist2_mC[i]))  return false;
    if (st.aktiv[i] > 1) return false;
  }

  return true;
}

static bool FLASHMEM pt100Cal2Validate()
{
  return pt100Cal2ValidateStore(pt100_2p);
}




// ============================================================================
// SPEICHERN
// ============================================================================

static void FLASHMEM pt100Cal2Save()
{
  pt100_2p.magic = PT100_2P_MAGIC;
  pt100_2p.version = PT100_2P_VERSION;
  pt100_2p.size = sizeof(pt100_2p_store_t);
  pt100_2p.crc = pt100Cal2CrcBerechnen();
  pt100_2p_loaded = true;
  tpMetrologyConfigSave();
}



// ============================================================================
// LADEN
// ============================================================================

static void FLASHMEM pt100Cal2EnsureLoaded()
{
  if (pt100_2p_loaded) return;
  tpMetrologyConfigEnsureLoaded();
}



// ============================================================================
// ÖFFENTLICH: WERTE LESEN
// ============================================================================

void pt100Cal2GetScaled(uint8_t sensor,
                        int32_t* soll1,
                        int32_t* ist1,
                        int32_t* soll2,
                        int32_t* ist2,
                        bool* aktiv)
{
  pt100Cal2EnsureLoaded();

  if (sensor > 1) sensor = 0;

  if (soll1) *soll1 = pt100_2p.soll1_mC[sensor];
  if (ist1)  *ist1  = pt100_2p.ist1_mC[sensor];
  if (soll2) *soll2 = pt100_2p.soll2_mC[sensor];
  if (ist2)  *ist2  = pt100_2p.ist2_mC[sensor];

  if (aktiv) *aktiv = (pt100_2p.aktiv[sensor] != 0);
}


// ============================================================================
// ÖFFENTLICH: WERTE SPEICHERN
// ============================================================================

bool pt100Cal2SetScaled(uint8_t sensor,
                        int32_t soll1,
                        int32_t ist1,
                        int32_t soll2,
                        int32_t ist2)
{
  pt100Cal2EnsureLoaded();

  if (sensor > 1) return false;

  if (!pt100Cal2ScaledPlausibel(soll1)) return false;
  if (!pt100Cal2ScaledPlausibel(ist1))  return false;
  if (!pt100Cal2ScaledPlausibel(soll2)) return false;
  if (!pt100Cal2ScaledPlausibel(ist2))  return false;

  int32_t deltaSoll = labs(soll2 - soll1);
  int32_t deltaIst  = labs(ist2  - ist1);

  // Mindestabstand:
  // Soll mindestens 30.000 C, Ist mindestens 25.000 C.
  if (deltaSoll < 30000L) return false;
  if (deltaIst  < 25000L) return false;

  pt100_2p.soll1_mC[sensor] = soll1;
  pt100_2p.ist1_mC[sensor]  = ist1;
  pt100_2p.soll2_mC[sensor] = soll2;
  pt100_2p.ist2_mC[sensor]  = ist2;
  pt100_2p.aktiv[sensor] = 1;

  pt100Cal2Save();

  return true;
}


// ============================================================================
// ÖFFENTLICH: KALIBRIERUNG DEAKTIVIEREN
// ============================================================================

void FLASHMEM pt100Cal2Disable(uint8_t sensor)
{
  pt100Cal2EnsureLoaded();

  if (sensor > 1) return;

  pt100_2p.aktiv[sensor] = 0;
  pt100Cal2Save();
}


// ============================================================================
// ÖFFENTLICH: 2-PUNKT-KORREKTUR ANWENDEN
// ============================================================================

double pt100Cal2Apply(uint8_t sensor, double tCvd)
{
  pt100Cal2EnsureLoaded();

  if (sensor > 1) sensor = 0;

  if (pt100_2p.aktiv[sensor] == 0)
  {
    return tCvd;
  }

  double soll1 = (double)pt100_2p.soll1_mC[sensor] / 1000.0;
  double ist1  = (double)pt100_2p.ist1_mC[sensor]  / 1000.0;
  double soll2 = (double)pt100_2p.soll2_mC[sensor] / 1000.0;
  double ist2  = (double)pt100_2p.ist2_mC[sensor]  / 1000.0;

  double deltaIst = ist2 - ist1;

  if (fabs(deltaIst) < 0.001)
  {
    return tCvd;
  }

  double gain = (soll2 - soll1) / deltaIst;
  double offset = soll1 - (gain * ist1);

  double tKorr = (tCvd * gain) + offset;

  if (!isfinite(tKorr))
  {
    return tCvd;
  }

  return tKorr;
}


// ============================================================================
// PT100-R0-KALIBRIERUNG
// ============================================================================
//
// Speichert die R0-Werte getrennt von var_t im EEPROM.
// Dadurch bleiben sie bei "Werkseinstellungen" erhalten, weil der
// Werksreset weiterhin nur den Hauptparameterblock var_t neu schreibt.
//
// EEPROM-Lage:
//   Marker bei 0
//   var_t ab Adresse 1
//   PT100 2P:       1 + sizeof(var_t) + 16
//   Ref100/120:     1 + sizeof(var_t) + 512
//   Pt100 R0:       1 + sizeof(var_t) + 600
//   Sprache/UI:     1 + sizeof(var_t) + 800
//
// Migration:
//   Wenn der neue R0-Block noch nicht existiert, werden die alten Werte aus
//   R.fuehler_cal[x].Fwd uebernommen. Danach ist R.fuehler_cal[] nur noch
//   reservierter EEPROM-Platzhalter.

// ============================================================================
// SPEICHERSTRUKTUR
// ============================================================================

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  int32_t r0_scaled[2];   // Ohm * 10000: [0] Spiegel, [1] Umgebung

  uint8_t reserved[16];

  uint32_t crc;
} pt100_r0_store_t;


static pt100_r0_store_t pt100_r0;
static bool pt100_r0_loaded = false;

static const uint32_t PT100_R0_MAGIC = 0x52304331UL; // "R0C1"
static const uint16_t PT100_R0_VERSION = 1;

static const int32_t PT100_R0_DEFAULT_SCALED = 1000000L; // 100.0000 Ohm
static const int32_t PT100_R0_MIN_SCALED     = 800000L;  //  80.0000 Ohm
static const int32_t PT100_R0_MAX_SCALED     = 1200000L; // 120.0000 Ohm




// ============================================================================
// CRC UEBER GLOBALE STRUKTUR
// ============================================================================

static uint32_t FLASHMEM pt100R0CrcBerechnen()
{
  return tpCalCrcBytes(&pt100_r0, sizeof(pt100_r0_store_t) - sizeof(uint32_t));
}


// ============================================================================
// PLAUSIBILITAET / KONVERTIERUNG
// ============================================================================

static bool FLASHMEM pt100R0ScaledPlausibel(int32_t v)
{
  return (v >= PT100_R0_MIN_SCALED && v <= PT100_R0_MAX_SCALED);
}

static int32_t pt100R0OhmToScaled(double r0_ohm)
{
  if (!isfinite(r0_ohm)) return PT100_R0_DEFAULT_SCALED;
  if (r0_ohm < 80.0)    return PT100_R0_DEFAULT_SCALED;
  if (r0_ohm > 120.0)   return PT100_R0_DEFAULT_SCALED;

  int32_t scaled = (int32_t)round(r0_ohm * 10000.0);

  if (!pt100R0ScaledPlausibel(scaled))
  {
    return PT100_R0_DEFAULT_SCALED;
  }

  return scaled;
}


// ============================================================================
// DEFAULT / MIGRATION AUS ALTER var_t-ABLAGE
// ============================================================================

static void FLASHMEM pt100R0DefaultFromLegacy()
{
  memset(&pt100_r0, 0, sizeof(pt100_r0));

  pt100_r0.magic = PT100_R0_MAGIC;
  pt100_r0.version = PT100_R0_VERSION;
  pt100_r0.size = sizeof(pt100_r0_store_t);

  // Sauberer Schnitt ab EEPROM V11: keine Migration aus der alten var_t-Ablage.
  pt100_r0.r0_scaled[0] = PT100_R0_DEFAULT_SCALED;
  pt100_r0.r0_scaled[1] = PT100_R0_DEFAULT_SCALED;

  pt100_r0.crc = pt100R0CrcBerechnen();
}


// ============================================================================
// VALIDIERUNG
// ============================================================================

static bool FLASHMEM pt100R0ValidateStore(const pt100_r0_store_t& st)
{
  if (st.magic != PT100_R0_MAGIC) return false;
  if (st.version != PT100_R0_VERSION) return false;
  if (st.size != sizeof(pt100_r0_store_t)) return false;

  uint32_t crc = tpCalCrcBytes(&st, sizeof(pt100_r0_store_t) - sizeof(uint32_t));
  if (crc != st.crc) return false;

  if (!pt100R0ScaledPlausibel(st.r0_scaled[0])) return false;
  if (!pt100R0ScaledPlausibel(st.r0_scaled[1])) return false;

  return true;
}

static bool FLASHMEM pt100R0Validate()
{
  return pt100R0ValidateStore(pt100_r0);
}




// ============================================================================
// SPEICHERN / LADEN
// ============================================================================

static void FLASHMEM pt100R0Save()
{
  pt100_r0.magic = PT100_R0_MAGIC;
  pt100_r0.version = PT100_R0_VERSION;
  pt100_r0.size = sizeof(pt100_r0_store_t);
  pt100_r0.crc = pt100R0CrcBerechnen();
  pt100_r0_loaded = true;
  tpMetrologyConfigSave();
}


void FLASHMEM pt100R0EnsureLoaded()
{
  if (pt100_r0_loaded) return;
  tpMetrologyConfigEnsureLoaded();
}



// ============================================================================
// OEFFENTLICH: LESEN
// ============================================================================

int32_t pt100R0GetScaled(uint8_t sensor)
{
  pt100R0EnsureLoaded();

  if (sensor > 1) sensor = 0;

  return pt100_r0.r0_scaled[sensor];
}

double pt100R0GetOhm(uint8_t sensor)
{
  return (double)pt100R0GetScaled(sensor) / 10000.0;
}

void FLASHMEM pt100R0GetBothScaled(int32_t* spiegel, int32_t* umgebung)
{
  pt100R0EnsureLoaded();

  if (spiegel)  *spiegel  = pt100_r0.r0_scaled[0];
  if (umgebung) *umgebung = pt100_r0.r0_scaled[1];
}


// ============================================================================
// OEFFENTLICH: SCHREIBEN
// ============================================================================

bool FLASHMEM pt100R0SetScaled(uint8_t sensor, int32_t r0_scaled)
{
  pt100R0EnsureLoaded();

  if (sensor > 1) return false;
  if (!pt100R0ScaledPlausibel(r0_scaled)) return false;

  pt100_r0.r0_scaled[sensor] = r0_scaled;
  pt100R0Save();

  return true;
}

bool FLASHMEM pt100R0SetOhm(uint8_t sensor, double r0_ohm)
{
  int32_t scaled = pt100R0OhmToScaled(r0_ohm);
  return pt100R0SetScaled(sensor, scaled);
}


// ============================================================================
// TAUPUNKT-/FROSTPUNKT-OFFSET
// ============================================================================
//
// 1-Punkt-Offset fuer die Taupunkt-/Frostpunktanzeige.
// Der Offset wird NICHT auf T-Spiegel angewendet. T-Spiegel bleibt die echte
// gemessene Spiegeltemperatur. Nur der daraus abgeleitete Taupunkt und die
// daraus berechnete rH werden korrigiert.
//
// Wertebereich: -2.000 C bis +2.000 C
// Aufloesung:   0.001 C
// Speicherung separat hinter var_t im EEPROM, damit Werkseinstellungen die
// metrologische Kalibrierung nicht loeschen.

// ============================================================================
// SPEICHERSTRUKTUR
// ============================================================================

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  int32_t offset_mC;      // mC: -2000 .. +2000

  uint8_t reserved[16];

  uint32_t crc;
} taupunkt_offset_store_t;


static taupunkt_offset_store_t taupunkt_offset;
static bool taupunkt_offset_loaded = false;

static const uint32_t TAUPUNKT_OFFSET_MAGIC = 0x54504F31UL; // "TPO1"
static const uint16_t TAUPUNKT_OFFSET_VERSION = 1;

static const int32_t TAUPUNKT_OFFSET_MIN_MC = -2000L;
static const int32_t TAUPUNKT_OFFSET_MAX_MC =  2000L;
static const int32_t TAUPUNKT_OFFSET_DEFAULT_MC = 0L;




// ============================================================================
// CRC UEBER GLOBALE STRUKTUR
// ============================================================================

static uint32_t FLASHMEM taupunktOffsetCrcBerechnen()
{
  return tpCalCrcBytes(&taupunkt_offset, sizeof(taupunkt_offset_store_t) - sizeof(uint32_t));
}


// ============================================================================
// PLAUSIBILITAET
// ============================================================================

static bool FLASHMEM taupunktOffsetScaledPlausibel(int32_t v)
{
  return (v >= TAUPUNKT_OFFSET_MIN_MC && v <= TAUPUNKT_OFFSET_MAX_MC);
}


// ============================================================================
// DEFAULT / VALIDIERUNG
// ============================================================================

static void FLASHMEM taupunktOffsetDefault()
{
  memset(&taupunkt_offset, 0, sizeof(taupunkt_offset));

  taupunkt_offset.magic = TAUPUNKT_OFFSET_MAGIC;
  taupunkt_offset.version = TAUPUNKT_OFFSET_VERSION;
  taupunkt_offset.size = sizeof(taupunkt_offset_store_t);
  taupunkt_offset.offset_mC = TAUPUNKT_OFFSET_DEFAULT_MC;

  taupunkt_offset.crc = taupunktOffsetCrcBerechnen();
}

static bool FLASHMEM taupunktOffsetValidateStore(const taupunkt_offset_store_t& st)
{
  if (st.magic != TAUPUNKT_OFFSET_MAGIC) return false;
  if (st.version != TAUPUNKT_OFFSET_VERSION) return false;
  if (st.size != sizeof(taupunkt_offset_store_t)) return false;

  uint32_t crc = tpCalCrcBytes(&st, sizeof(taupunkt_offset_store_t) - sizeof(uint32_t));
  if (crc != st.crc) return false;

  if (!taupunktOffsetScaledPlausibel(st.offset_mC)) return false;

  return true;
}

static bool FLASHMEM taupunktOffsetValidate()
{
  return taupunktOffsetValidateStore(taupunkt_offset);
}




// ============================================================================
// SPEICHERN / LADEN
// ============================================================================

static void FLASHMEM taupunktOffsetSave()
{
  taupunkt_offset.magic = TAUPUNKT_OFFSET_MAGIC;
  taupunkt_offset.version = TAUPUNKT_OFFSET_VERSION;
  taupunkt_offset.size = sizeof(taupunkt_offset_store_t);
  taupunkt_offset.crc = taupunktOffsetCrcBerechnen();
  taupunkt_offset_loaded = true;
  tpMetrologyConfigSave();
}


void FLASHMEM taupunktOffsetEnsureLoaded()
{
  if (taupunkt_offset_loaded) return;
  tpMetrologyConfigEnsureLoaded();
}



// ============================================================================
// OEFFENTLICH: LESEN / SCHREIBEN
// ============================================================================

int32_t taupunktOffsetGetScaled()
{
  taupunktOffsetEnsureLoaded();
  return taupunkt_offset.offset_mC;
}

double taupunktOffsetGetC()
{
  return (double)taupunktOffsetGetScaled() / 1000.0;
}

bool FLASHMEM taupunktOffsetSetScaled(int32_t offset_mC)
{
  taupunktOffsetEnsureLoaded();

  if (!taupunktOffsetScaledPlausibel(offset_mC)) return false;

  taupunkt_offset.offset_mC = offset_mC;
  taupunktOffsetSave();

  return true;
}

bool FLASHMEM taupunktOffsetSetC(double offsetC)
{
  if (!isfinite(offsetC)) return false;

  int32_t scaled = (int32_t)round(offsetC * 1000.0);
  return taupunktOffsetSetScaled(scaled);
}
