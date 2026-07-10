/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPmenu_RefExtCal.ino
 * Zweck: Externer Vier-Widerstands-Abgleich fuer die Referenzkalibrierung.
 *
 * Abgeleitet aus: PSWRmenu.ino
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2014 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
#define EXT_REFCAL_V6_SOURCE 1
// TPmenu_RefExtCal.ino
// Externer 4-Widerstands-Abgleich fuer interne Ref Low/Ref High und Kanal A/B
// =========================================================================

#include <Arduino.h>
#include <TimeLib.h>
#include <math.h>
#include "TP_T.h"
#include "TPlanguage.h"
#include "TPtft.h"

extern uint16_t menu_level;
extern char lcd_buf[];
extern TextBox* VirtLCDMenu;
extern TextBox* VirtLCDMessage;

extern void ReadButtons(bool redraw);
extern void menuTimeoutResetActivity(void);

extern void refCalGetAllScaled(uint32_t* date_yyyymmdd,
                               int32_t* ref100,
                               int32_t* ref120,
                               int32_t* chA_corr100,
                               int32_t* chA_corr120,
                               int32_t* chB_corr100,
                               int32_t* chB_corr120);
extern bool refCalSetAllScaled(uint32_t date_yyyymmdd,
                               int32_t ref100_scaled,
                               int32_t ref120_scaled,
                               int32_t chA_corr100_scaled,
                               int32_t chA_corr120_scaled,
                               int32_t chB_corr100_scaled,
                               int32_t chB_corr120_scaled);

// Die Kalibrierung sammelt fertige Rohwerte aus dem normalen Messbetrieb.
// Damit bleiben Reihenfolge, SPS, PGA, Filter, Stromumkehr und 7 Verwerfwerte
// exakt so wie im normalen Betrieb.
static const uint16_t EXTCAL_EXT_TARGET_SAMPLES = 800;
static const uint16_t EXTCAL_INT_TARGET_SAMPLES = 200;
static const uint16_t EXTCAL_EXT_MIN_VALID      = 720;
static const uint16_t EXTCAL_INT_MIN_VALID      = 180;

static DMAMEM int32_t extCalBufA[EXTCAL_EXT_TARGET_SAMPLES];
static DMAMEM int32_t extCalBufB[EXTCAL_EXT_TARGET_SAMPLES];
static DMAMEM int32_t extCalBufR100[EXTCAL_INT_TARGET_SAMPLES];
static DMAMEM int32_t extCalBufR120[EXTCAL_INT_TARGET_SAMPLES];
static DMAMEM int32_t extCalScratch[EXTCAL_EXT_TARGET_SAMPLES];

struct ExtCalBlockResult
{
  int32_t rawA;
  int32_t rawB;
  int32_t rawRef100;
  int32_t rawRef120;

  uint16_t validA;
  uint16_t validB;
  uint16_t validRef100;
  uint16_t validRef120;

  uint16_t rejA;
  uint16_t rejB;
  uint16_t rejRef100;
  uint16_t rejRef120;
};

struct ExtCalResult
{
  uint32_t date;
  int32_t ref100;
  int32_t ref120;
  int32_t chA_corr100;
  int32_t chA_corr120;
  int32_t chB_corr100;
  int32_t chB_corr120;

  double diffRef100AB;
  double diffRef120AB;
};

static ExtCalBlockResult extCalBlockA;
static ExtCalBlockResult extCalBlockB;
static ExtCalResult extCalResult;

static uint16_t extCalCountA = 0;
static uint16_t extCalCountB = 0;
static uint16_t extCalCountR100 = 0;
static uint16_t extCalCountR120 = 0;

// Zaehlsperren gegen doppelte Uebernahme derselben ADC-Rohwerte.
// Die Kalibrierfunktion wird aus der Regel-/Formel-Ebene regelmaessig aufgerufen.
// Ohne diese Sperre wuerden gleiche letzte ADC-Werte mehrfach gezaehlt und die
// 800/200-Werte waeren viel zu schnell voll.
static bool extCalLastPtValid = false;
static bool extCalLastRef100Valid = false;
static bool extCalLastRef120Valid = false;
static int32_t extCalLastPtA = 0;
static int32_t extCalLastPtB = 0;
static int32_t extCalLastRef100 = 0;
static int32_t extCalLastRef120 = 0;

static bool extCalMeasuring = false;
static uint8_t extCalMeasureStep = 0; // 1=Stecker A, 2=Stecker B
static bool extCalResultValid = false;
static char extCalErrorText[64];

// Eingabewerte Ohm * 100000:
// Stecker A: Kanal A=LowA,  Kanal B=HighA
// Stecker B: Kanal A=HighB, Kanal B=LowB
static int32_t extCalInput[4] = {
  10000000L,  // R100_A
  12000000L,  // R120_A
  12000000L,  // R120_B
  10000000L   // R100_B
};

static uint8_t extCalInputIndex = 0;
static uint8_t extCalInputDigit = 0;

static const int32_t EXTCAL_OHM_STEPS[6] = {
  100000L,
  10000L,
  1000L,
  100L,
  10L,
  1L
};

static const char* extCalInputName(uint8_t idx)
{
  switch (idx)
  {
    case 0: return "A: ChA=Low";
    case 1: return "A: ChB=High";
    case 2: return "B: ChA=High";
    default: return "B: ChB=Low";
  }
}

static void extCalFormatOhm5(int32_t scaled, char* out, size_t outSize, bool forceSign)
{
  char sign = 0;
  if (scaled < 0)
  {
    sign = '-';
    scaled = -scaled;
  }
  else if (forceSign)
  {
    sign = '+';
  }

  int32_t pre = scaled / 100000L;
  int32_t post = scaled % 100000L;

  if (sign)
  {
    snprintf(out, outSize, "%c%ld.%05ld", sign, (long)pre, (long)post);
  }
  else
  {
    snprintf(out, outSize, "%ld.%05ld", (long)pre, (long)post);
  }
}

static uint32_t extCalTodayDate()
{
  uint16_t y = (uint16_t)year();
  uint8_t m = (uint8_t)month();
  uint8_t d = (uint8_t)day();

  if (y < 2024 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  return ((uint32_t)y * 10000UL) + ((uint32_t)m * 100UL) + (uint32_t)d;
}

static int extCalCompareInt32(const void* a, const void* b)
{
  int32_t aa = *(const int32_t*)a;
  int32_t bb = *(const int32_t*)b;
  if (aa < bb) return -1;
  if (aa > bb) return 1;
  return 0;
}

static int32_t extCalMedianSorted(const int32_t* data, uint16_t n)
{
  if (n == 0) return 0;
  if (n & 1) return data[n / 2];
  return (int32_t)(((int64_t)data[(n / 2) - 1] + (int64_t)data[n / 2]) / 2);
}

static bool extCalRobustValue(int32_t* data,
                              uint16_t count,
                              uint16_t minValid,
                              int32_t* out,
                              uint16_t* validOut,
                              uint16_t* rejectOut)
{
  if (out) *out = 0;
  if (validOut) *validOut = 0;
  if (rejectOut) *rejectOut = 0;

  if (data == nullptr || count < minValid || count == 0) return false;

  qsort(data, count, sizeof(int32_t), extCalCompareInt32);
  int32_t median = extCalMedianSorted(data, count);

  for (uint16_t i = 0; i < count; i++)
  {
    int64_t d = (int64_t)data[i] - (int64_t)median;
    if (d < 0) d = -d;
    if (d > 2147483647LL) d = 2147483647LL;
    extCalScratch[i] = (int32_t)d;
  }

  qsort(extCalScratch, count, sizeof(int32_t), extCalCompareInt32);
  int32_t mad = extCalMedianSorted(extCalScratch, count);

  int64_t limit = (int64_t)mad * 6LL;
  if (limit < 20LL) limit = 20LL;

  int64_t sum = 0;
  uint16_t valid = 0;

  for (uint16_t i = 0; i < count; i++)
  {
    int64_t d = (int64_t)data[i] - (int64_t)median;
    if (d < 0) d = -d;

    if (d <= limit)
    {
      sum += data[i];
      valid++;
    }
  }

  uint16_t rejected = count - valid;

  if (validOut) *validOut = valid;
  if (rejectOut) *rejectOut = rejected;

  if (valid < minValid) return false;

  if (out) *out = (int32_t)(sum / (int64_t)valid);
  return true;
}

static void extCalResetBuffers()
{
  extCalCountA = 0;
  extCalCountB = 0;
  extCalCountR100 = 0;
  extCalCountR120 = 0;

  extCalLastPtValid = false;
  extCalLastRef100Valid = false;
  extCalLastRef120Valid = false;
  extCalLastPtA = 0;
  extCalLastPtB = 0;
  extCalLastRef100 = 0;
  extCalLastRef120 = 0;
}

static void extCalStartMeasure(uint8_t step)
{
  extCalResetBuffers();
  extCalMeasureStep = step;
  extCalMeasuring = true;
  extCalResultValid = false;
  extCalErrorText[0] = '\0';
}

static bool extCalBuffersFull()
{
  return (extCalCountA >= EXTCAL_EXT_TARGET_SAMPLES &&
          extCalCountB >= EXTCAL_EXT_TARGET_SAMPLES &&
          extCalCountR100 >= EXTCAL_INT_TARGET_SAMPLES &&
          extCalCountR120 >= EXTCAL_INT_TARGET_SAMPLES);
}

bool extRefCalIsActive(void)
{
  return extCalMeasuring;
}

void extRefCalAbort(void)
{
  extCalMeasuring = false;
  extCalMeasureStep = 0;
  extCalResetBuffers();
}

void extRefCalNoteRawSample(int32_t rawPtA,
                            int32_t rawPtB,
                            int32_t rawRef100,
                            int32_t rawRef120)
{
  if (!extCalMeasuring) return;

  // Externe Kanalwerte nur zaehlen, wenn wirklich ein neuer fertiger
  // Pt100-Rohwertsatz angekommen ist. Sonst wuerde die Regel-/Formelroutine
  // denselben letzten ADC-Wert mehrfach in den Puffer schreiben.
  if (rawPtA > 0 && rawPtB > 0)
  {
    bool newPt = (!extCalLastPtValid ||
                  rawPtA != extCalLastPtA ||
                  rawPtB != extCalLastPtB);

    if (newPt)
    {
      extCalLastPtValid = true;
      extCalLastPtA = rawPtA;
      extCalLastPtB = rawPtB;

      if (extCalCountA < EXTCAL_EXT_TARGET_SAMPLES)
      {
        extCalBufA[extCalCountA++] = rawPtA;
      }

      if (extCalCountB < EXTCAL_EXT_TARGET_SAMPLES)
      {
        extCalBufB[extCalCountB++] = rawPtB;
      }
    }
  }

  // Interne Referenzen getrennt zaehlen. Damit passt 800 extern / 200 intern
  // zum echten 4:1-Messverhaeltnis und gleiche alte Ref-Werte werden nicht
  // mehrfach gezaehlt.
  if (rawRef100 > 0 && rawRef120 > rawRef100)
  {
    bool newRef100 = (!extCalLastRef100Valid || rawRef100 != extCalLastRef100);
    bool newRef120 = (!extCalLastRef120Valid || rawRef120 != extCalLastRef120);

    if (newRef100)
    {
      extCalLastRef100Valid = true;
      extCalLastRef100 = rawRef100;

      if (extCalCountR100 < EXTCAL_INT_TARGET_SAMPLES)
      {
        extCalBufR100[extCalCountR100++] = rawRef100;
      }
    }

    if (newRef120)
    {
      extCalLastRef120Valid = true;
      extCalLastRef120 = rawRef120;

      if (extCalCountR120 < EXTCAL_INT_TARGET_SAMPLES)
      {
        extCalBufR120[extCalCountR120++] = rawRef120;
      }
    }
  }
}

static bool extCalFinishBlock(uint8_t blockIndex)
{
  ExtCalBlockResult* outBlock = (blockIndex == 1) ? &extCalBlockA : &extCalBlockB;
  bool ok = true;

  ok &= extCalRobustValue(extCalBufA, extCalCountA, EXTCAL_EXT_MIN_VALID,
                          &outBlock->rawA, &outBlock->validA, &outBlock->rejA);
  ok &= extCalRobustValue(extCalBufB, extCalCountB, EXTCAL_EXT_MIN_VALID,
                          &outBlock->rawB, &outBlock->validB, &outBlock->rejB);
  ok &= extCalRobustValue(extCalBufR100, extCalCountR100, EXTCAL_INT_MIN_VALID,
                          &outBlock->rawRef100, &outBlock->validRef100, &outBlock->rejRef100);
  ok &= extCalRobustValue(extCalBufR120, extCalCountR120, EXTCAL_INT_MIN_VALID,
                          &outBlock->rawRef120, &outBlock->validRef120, &outBlock->rejRef120);

  if (!ok)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "zu wenig gültige Werte");
    return false;
  }

  if (outBlock->rawRef120 <= outBlock->rawRef100)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "Ref High raw <= Ref Low raw");
    return false;
  }

  // Grobe Stecker-Plausibilitaet direkt nach jedem Block.
  // Damit werden vertauschte Stecker oder gleiche Widerstaende frueh erkannt,
  // ohne dass wir eine zu enge metrologische Bewertung machen.
  // Grenze: mindestens ca. 25 % vom internen Ref-Low/Ref-High-Raw-Abstand.
  int32_t rawRefDelta = outBlock->rawRef120 - outBlock->rawRef100;
  int32_t minPlugDelta = rawRefDelta / 4;
  if (minPlugDelta < 1) minPlugDelta = 1;

  if (blockIndex == 1)
  {
    // Stecker A erwartet: Kanal A = Low, Kanal B = High
    if ((outBlock->rawB - outBlock->rawA) < minPlugDelta)
    {
      snprintf(extCalErrorText, sizeof(extCalErrorText), "Stecker A falsch/gleich");
      return false;
    }
  }
  else
  {
    // Stecker B erwartet: Kanal A = High, Kanal B = Low
    if ((outBlock->rawA - outBlock->rawB) < minPlugDelta)
    {
      snprintf(extCalErrorText, sizeof(extCalErrorText), "Stecker B falsch/gleich");
      return false;
    }
  }

  return true;
}

static int32_t extCalScaleOhm(double ohm)
{
  if (!isfinite(ohm)) return 0;
  return (int32_t)lround(ohm * 100000.0);
}

static double extCalRawToOhm(double raw, double raw100, double raw120, double ohm100, double ohm120)
{
  double dRaw = raw120 - raw100;
  double dOhm = ohm120 - ohm100;
  if (dRaw <= 0.0 || dOhm <= 0.0) return NAN;
  return ohm100 + ((raw - raw100) * dOhm / dRaw);
}

static bool extCalCalculateResult()
{
  const double R100_A = (double)extCalInput[0] / 100000.0;
  const double R120_A = (double)extCalInput[1] / 100000.0;
  const double R120_B = (double)extCalInput[2] / 100000.0;
  const double R100_B = (double)extCalInput[3] / 100000.0;

  // Plausibilitaet der Steckerwerte.
  if (R100_A < 60.0 || R100_A > 110.0 || R100_B < 60.0 || R100_B > 110.0 ||
      R120_A < 115.0 || R120_A > 152.0 || R120_B < 115.0 || R120_B > 152.0 ||
      (R120_A - R100_A) < 5.0 || (R120_B - R100_B) < 5.0)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "Ext-Werte unplausibel");
    return false;
  }

  // Kanal A: Stecker A = Low,  Stecker B = High
  // Kanal B: Stecker B = Low,  Stecker A = High
  if (extCalBlockB.rawA <= extCalBlockA.rawA)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "Kanal A rawHigh <= rawLow");
    return false;
  }

  if (extCalBlockA.rawB <= extCalBlockB.rawB)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "Kanal B rawHigh <= rawLow");
    return false;
  }

  double gainA = (R120_B - R100_A) / ((double)extCalBlockB.rawA - (double)extCalBlockA.rawA);
  double offsA = R100_A - gainA * (double)extCalBlockA.rawA;

  double gainB = (R120_A - R100_B) / ((double)extCalBlockA.rawB - (double)extCalBlockB.rawB);
  double offsB = R100_B - gainB * (double)extCalBlockB.rawB;

  if (!isfinite(gainA) || !isfinite(offsA) || !isfinite(gainB) || !isfinite(offsB) ||
      gainA <= 0.0 || gainB <= 0.0)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "Gain unplausibel");
    return false;
  }

  double rawRef100 = ((double)extCalBlockA.rawRef100 + (double)extCalBlockB.rawRef100) * 0.5;
  double rawRef120 = ((double)extCalBlockA.rawRef120 + (double)extCalBlockB.rawRef120) * 0.5;

  if (rawRef120 <= rawRef100)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "int. Ref raw unplausibel");
    return false;
  }

  double ref100A = gainA * rawRef100 + offsA;
  double ref100B = gainB * rawRef100 + offsB;
  double ref120A = gainA * rawRef120 + offsA;
  double ref120B = gainB * rawRef120 + offsB;

  double ref100 = (ref100A + ref100B) * 0.5;
  double ref120 = (ref120A + ref120B) * 0.5;

  if (!isfinite(ref100) || !isfinite(ref120) ||
      ref100 < 60.0 || ref100 > 110.0 ||
      ref120 < 115.0 || ref120 > 152.0 ||
      (ref120 - ref100) < 5.0)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "int. Ref Ergebnis unplausibel");
    return false;
  }

  // Kanal-Restkorrektur mit den neuen internen Ref-Werten berechnen.
  double measA100 = extCalRawToOhm((double)extCalBlockA.rawA, rawRef100, rawRef120, ref100, ref120);
  double measA120 = extCalRawToOhm((double)extCalBlockB.rawA, rawRef100, rawRef120, ref100, ref120);
  double measB100 = extCalRawToOhm((double)extCalBlockB.rawB, rawRef100, rawRef120, ref100, ref120);
  double measB120 = extCalRawToOhm((double)extCalBlockA.rawB, rawRef100, rawRef120, ref100, ref120);

  if (!isfinite(measA100) || !isfinite(measA120) || !isfinite(measB100) || !isfinite(measB120))
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "Kanal-Korr unplausibel");
    return false;
  }

  double corrA100 = R100_A - measA100;
  double corrA120 = R120_B - measA120;
  double corrB100 = R100_B - measB100;
  double corrB120 = R120_A - measB120;

  if (fabs(corrA100) > 0.100 || fabs(corrA120) > 0.100 ||
      fabs(corrB100) > 0.100 || fabs(corrB120) > 0.100)
  {
    snprintf(extCalErrorText, sizeof(extCalErrorText), "Kanal-Korr > 0.1 Ohm");
    return false;
  }

  extCalResult.date = extCalTodayDate();
  extCalResult.ref100 = extCalScaleOhm(ref100);
  extCalResult.ref120 = extCalScaleOhm(ref120);
  extCalResult.chA_corr100 = extCalScaleOhm(corrA100);
  extCalResult.chA_corr120 = extCalScaleOhm(corrA120);
  extCalResult.chB_corr100 = extCalScaleOhm(corrB100);
  extCalResult.chB_corr120 = extCalScaleOhm(corrB120);
  extCalResult.diffRef100AB = ref100A - ref100B;
  extCalResult.diffRef120AB = ref120A - ref120B;

  extCalResultValid = true;
  return true;
}

static void extCalDrawProgress(const char* title)
{
  // Direkt gezeichnete Wertefelder aus der Eingabe sicher loeschen.
  // VirtLCDMenu->clear() loescht nur die TextBox, nicht diese direkten TFT-Felder.
  menuDirectClearAllRows();
  menuDirectResetValueCache();

  VirtLCDMenu->clear();
  if (VirtLCDMessage != nullptr) VirtLCDMessage->clear();

  // Sicherheitsloeschung: Bei den TextBox-Zeilen koennen sonst nach einem
  // Seitenwechsel einzelne alte Zeichen stehen bleiben, wenn die neue Zeile
  // kuerzer ist. Daher schreiben wir die benutzten Zeilen einmal breit mit
  // Leerzeichen und danach die eigentlichen Texte ebenfalls mit fester Breite.
  for (uint8_t row = 1; row <= 8; row++)
  {
    VirtLCDMenu->setCursor(1, row);
    VirtLCDMenu->print("                                  ");
  }

  VirtLCDMenu->setCursor(1, 1);
  snprintf(lcd_buf, 255, "%-32s", title);
  VirtLCDMenu->print(lcd_buf);

  VirtLCDMenu->setCursor(1, 3);
  snprintf(lcd_buf, 255, "Kanal A: %3u/%3u              ", extCalCountA, EXTCAL_EXT_TARGET_SAMPLES);
  VirtLCDMenu->print(lcd_buf);

  VirtLCDMenu->setCursor(1, 4);
  snprintf(lcd_buf, 255, "Kanal B: %3u/%3u              ", extCalCountB, EXTCAL_EXT_TARGET_SAMPLES);
  VirtLCDMenu->print(lcd_buf);

  VirtLCDMenu->setCursor(1, 5);
  snprintf(lcd_buf, 255, "Ref Low : %3u/%3u             ", extCalCountR100, EXTCAL_INT_TARGET_SAMPLES);
  VirtLCDMenu->print(lcd_buf);

  VirtLCDMenu->setCursor(1, 6);
  snprintf(lcd_buf, 255, "Ref High: %3u/%3u             ", extCalCountR120, EXTCAL_INT_TARGET_SAMPLES);
  VirtLCDMenu->print(lcd_buf);

  VirtLCDMenu->setCursor(1, 8);
  VirtLCDMenu->print("EXIT = abbrechen               ");

  VirtLCDMenu->transfer();
  ReadButtons(true);
}

// Keine dauerhafte Speicherung beim reinen Rechnen. Das Result-Bild speichert bewusst.
static bool extCalSaveResult()
{
  if (!extCalResultValid) return false;
  return refCalSetAllScaled(extCalResult.date,
                            extCalResult.ref100,
                            extCalResult.ref120,
                            extCalResult.chA_corr100,
                            extCalResult.chA_corr120,
                            extCalResult.chB_corr100,
                            extCalResult.chB_corr120);
}

// =========================================================================
// Menue: vorhandene Ref-/Kanalwerte manuell editieren
// =========================================================================
void FLASHMEM referenz_werte_menu(void)
{
  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  static int32_t values[6];
  static uint32_t dateValue = 0;
  static uint8_t valueIndex = 0;
  static uint8_t digitIndex = 0;
  static bool frisch = true;
  static bool fehler = false;
  static MenuPageDrawFlag page_drawn;

  static const char* NAMES_DE[6] = {
    "Ref Low int",
    "Ref High int",
    "A Korr Low",
    "A Korr High",
    "B Korr Low",
    "B Korr High"
  };

  static const char* NAMES_EN[6] = {
    "Ref Low int",
    "Ref High int",
    "A corr Low",
    "A corr High",
    "B corr Low",
    "B corr High"
  };

  if (frisch)
  {
    refCalGetAllScaled(&dateValue,
                       &values[0], &values[1],
                       &values[2], &values[3],
                       &values[4], &values[5]);
    valueIndex = 0;
    digitIndex = 0;
    fehler = false;
    frisch = false;
    page_drawn = false;
    flag.menu_lcd_upd = false;
  }

  int32_t minVal = -10000L;
  int32_t maxVal =  10000L;

  if (valueIndex == 0)
  {
    minVal = 6000000L;
    maxVal = 11000000L;
  }
  else if (valueIndex == 1)
  {
    minVal = 11500000L;
    maxVal = 15200000L;
  }

  uint8_t action = menuDigitValueHandle(values[valueIndex],
                                        digitIndex,
                                        6,
                                        EXTCAL_OHM_STEPS,
                                        minVal,
                                        maxVal,
                                        250);

  if (action != MDVA_NONE)
  {
    if (fehler) page_drawn = false;
    fehler = false;

    if (action == MDVA_NEXT_DIGIT)
    {
      page_drawn = false;
    }

    if (action == MDVA_DONE)
    {
      if (valueIndex < 5)
      {
        valueIndex++;
        page_drawn = false;
      }
      else
      {
        bool ok = refCalSetAllScaled(dateValue,
                                     values[0], values[1],
                                     values[2], values[3],
                                     values[4], values[5]);
        if (ok)
        {
          frisch = true;
          menu_level = MENU_CALIBRATION;
          flag.menu_lcd_upd = false;
          flag.short_push = false;
          menuDigitResetTouchDebounce();
          return;
        }

        fehler = true;
        page_drawn = false;
      }
    }
  }

  if (!flag.menu_lcd_upd)
  {
    char valueText[32];
    extCalFormatOhm5(values[valueIndex], valueText, sizeof(valueText), valueIndex >= 2);

    if (!page_drawn)
    {
      menuDigitBeginDraw(TXT_REF_VALUES_TITLE);

      VirtLCDMenu->setCursor(1, 3);
      if (dateValue > 0)
      {
        snprintf(lcd_buf, 255, "Datum: %02lu.%02lu.%04lu",
                 (unsigned long)(dateValue % 100UL),
                 (unsigned long)((dateValue / 100UL) % 100UL),
                 (unsigned long)(dateValue / 10000UL));
      }
      else
      {
        snprintf(lcd_buf, 255, "Datum: --.--.----");
      }
      VirtLCDMenu->print(lcd_buf);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((ui_language == LANG_EN) ? NAMES_EN[valueIndex] : NAMES_DE[valueIndex]);

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print(T(TXT_LABEL_DIGIT));
      switch (digitIndex)
      {
        case 0: VirtLCDMenu->print("[ 1.00000 ]"); break;
        case 1: VirtLCDMenu->print("[ 0.10000 ]"); break;
        case 2: VirtLCDMenu->print("[ 0.01000 ]"); break;
        case 3: VirtLCDMenu->print("[ 0.00100 ]"); break;
        case 4: VirtLCDMenu->print("[ 0.00010 ]"); break;
        default: VirtLCDMenu->print((ui_language == LANG_EN) ? "[ 0.00001 ] -> Next/Save" : "[ 0.00001 ]->Weiter/Speichern"); break;
      }

      VirtLCDMenu->setCursor(1, 8);
      if (fehler)
      {
        VirtLCDMenu->print((ui_language == LANG_EN) ? "ERROR: value invalid" : "FEHLER: Wert ungültig");
      }
      else
      {
        VirtLCDMenu->print(T(TXT_UP_DOWN_ENTER_NEXT));
      }

      menuDigitEndDraw();
      menuDirectResetValueCache();
      page_drawn = true;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    snprintf(lcd_buf, 255, "%s \xB1", valueText);
    menuDirectWriteFixedRow(0, 1, 5, lcd_buf, 15, WHITE);

    ReadButtons(true);
  }
}

// =========================================================================
// Menue: automatische externe Referenzkalibrierung
// =========================================================================
void FLASHMEM referenz_ext_auto_menu(void)
{
  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  enum
  {
    ST_INPUT = 0,
    ST_WAIT_A,
    ST_MEASURE_A,
    ST_WAIT_B,
    ST_MEASURE_B,
    ST_RESULT,
    ST_ERROR
  };

  static uint8_t st = ST_INPUT;
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;

  if (frisch)
  {
    st = ST_INPUT;
    extCalInputIndex = 0;
    extCalInputDigit = 0;
    extCalMeasuring = false;
    extCalResultValid = false;
    extCalErrorText[0] = '\0';
    frisch = false;
    page_drawn = false;
    flag.menu_lcd_upd = false;
  }

  if (st == ST_INPUT)
  {
    int32_t minVal = (extCalInputIndex == 0 || extCalInputIndex == 3) ? 6000000L : 11500000L;
    int32_t maxVal = (extCalInputIndex == 0 || extCalInputIndex == 3) ? 11000000L : 15200000L;

    uint8_t action = menuDigitValueHandle(extCalInput[extCalInputIndex],
                                          extCalInputDigit,
                                          6,
                                          EXTCAL_OHM_STEPS,
                                          minVal,
                                          maxVal,
                                          250);

    if (action == MDVA_NEXT_DIGIT) page_drawn = false;

    if (action == MDVA_DONE)
    {
      if (extCalInputIndex < 3)
      {
        extCalInputIndex++;
        page_drawn = false;
      }
      else
      {
        // Nach dem letzten Eingabewert sofort auf die Stecker-A-Seite wechseln.
        // Sonst bleibt flag.menu_lcd_upd durch das alte Eingabebild gesetzt
        // und der Hinweis wird erst nach einer weiteren Taste sichtbar.
        st = ST_WAIT_A;
        page_drawn = false;
        flag.menu_lcd_upd = false;
        menuDirectClearAllRows();
        menuDirectResetValueCache();
        return;
      }
    }

    if (!flag.menu_lcd_upd)
    {
      char valueText[32];
      extCalFormatOhm5(extCalInput[extCalInputIndex], valueText, sizeof(valueText), false);

      if (!page_drawn)
      {
        menuDigitBeginDraw(TXT_REF_EXT_AUTO_TITLE);

        VirtLCDMenu->setCursor(1, 3);
        VirtLCDMenu->print(extCalInputName(extCalInputIndex));

        VirtLCDMenu->setCursor(1, 6);
        VirtLCDMenu->print(T(TXT_LABEL_DIGIT));
        switch (extCalInputDigit)
        {
          case 0: VirtLCDMenu->print("[ 1.00000 ]"); break;
          case 1: VirtLCDMenu->print("[ 0.10000 ]"); break;
          case 2: VirtLCDMenu->print("[ 0.01000 ]"); break;
          case 3: VirtLCDMenu->print("[ 0.00100 ]"); break;
          case 4: VirtLCDMenu->print("[ 0.00010 ]"); break;
          default: VirtLCDMenu->print((ui_language == LANG_EN) ? "[ 0.00001 ] -> Next" : "[ 0.00001 ] -> Weiter"); break;
        }

        VirtLCDMenu->setCursor(1, 8);
        VirtLCDMenu->print(T(TXT_UP_DOWN_ENTER_NEXT));

        menuDigitEndDraw();
        menuDirectResetValueCache();
        page_drawn = true;
      }
      else
      {
        flag.menu_lcd_upd = true;
      }

      snprintf(lcd_buf, 255, "%s \xB1", valueText);
      menuDirectWriteFixedRow(0, 1, 4, lcd_buf, 15, WHITE);
      ReadButtons(true);
    }
    return;
  }

  if (st == ST_WAIT_A || st == ST_WAIT_B)
  {
    ReadButtons(false);
    uint8_t key = menuReadKey(250);
    if (key == MK_ENTER)
    {
      extCalStartMeasure((st == ST_WAIT_A) ? 1 : 2);
      st = (st == ST_WAIT_A) ? ST_MEASURE_A : ST_MEASURE_B;
      page_drawn = false;
      flag.menu_lcd_upd = false;
      return;
    }

    if (!flag.menu_lcd_upd)
    {
      menuDigitBeginDraw(TXT_REF_EXT_AUTO_TITLE);

      VirtLCDMenu->setCursor(1, 3);
      if (st == ST_WAIT_A)
      {
        VirtLCDMenu->print("Stecker A einstecken:");
        VirtLCDMenu->setCursor(1, 4);
        VirtLCDMenu->print("A=Low   B=High");
      }
      else
      {
        VirtLCDMenu->print("Stecker B einstecken:");
        VirtLCDMenu->setCursor(1, 4);
        VirtLCDMenu->print("A=High  B=Low");
      }

      VirtLCDMenu->setCursor(1, 7);
      VirtLCDMenu->print("ENTER = Messung starten");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print("EXIT = abbrechen");

      menuDigitEndDraw();
      page_drawn = true;
    }
    return;
  }

  if (st == ST_MEASURE_A || st == ST_MEASURE_B)
  {
    // Die Messung dauert mehrere Minuten. Ohne Bedienung wuerde sonst
    // der normale 120-s-Menue-Timeout ausloesen und den Screen verlassen,
    // obwohl die Kalibrierung noch laeuft. Solange wirklich gesammelt wird,
    // halten wir das Menue deshalb aktiv.
    menuTimeoutResetActivity();

    if (extCalBuffersFull())
    {
      extCalMeasuring = false;
      bool ok = extCalFinishBlock((st == ST_MEASURE_A) ? 1 : 2);
      if (!ok)
      {
        st = ST_ERROR;
      }
      else if (st == ST_MEASURE_A)
      {
        st = ST_WAIT_B;
      }
      else
      {
        if (extCalCalculateResult()) st = ST_RESULT;
        else st = ST_ERROR;
      }

      page_drawn = false;
      flag.menu_lcd_upd = false;
      return;
    }

    if (!flag.menu_lcd_upd)
    {
      extCalDrawProgress((st == ST_MEASURE_A) ? "Stecker A messen..." : "Stecker B messen...");
      flag.menu_lcd_upd = true;
    }
    else
    {
      // Fortschritt ca. 2x pro Sekunde neu zeichnen.
      static uint32_t lastProg = 0;
      if (millis() - lastProg > 500UL)
      {
        lastProg = millis();
        flag.menu_lcd_upd = false;
      }
    }
    return;
  }

  if (st == ST_RESULT)
  {
    ReadButtons(false);
    uint8_t key = menuReadKey(250);
    if (key == MK_ENTER)
    {
      if (extCalSaveResult())
      {
        frisch = true;
        menu_level = MENU_CALIBRATION;
        flag.menu_lcd_upd = false;
        flag.short_push = false;
        menuDigitResetTouchDebounce();
        return;
      }
      snprintf(extCalErrorText, sizeof(extCalErrorText), "Speichern fehlgeschlagen");
      st = ST_ERROR;
      page_drawn = false;
      flag.menu_lcd_upd = false;
      return;
    }

    if (!flag.menu_lcd_upd)
    {
      char s1[24], s2[24], s3[24], s4[24], s5[24], s6[24];
      extCalFormatOhm5(extCalResult.ref100, s1, sizeof(s1), false);
      extCalFormatOhm5(extCalResult.ref120, s2, sizeof(s2), false);
      extCalFormatOhm5(extCalResult.chA_corr100, s3, sizeof(s3), true);
      extCalFormatOhm5(extCalResult.chA_corr120, s4, sizeof(s4), true);
      extCalFormatOhm5(extCalResult.chB_corr100, s5, sizeof(s5), true);
      extCalFormatOhm5(extCalResult.chB_corr120, s6, sizeof(s6), true);

      menuDigitBeginDraw(TXT_REF_EXT_AUTO_TITLE);

      VirtLCDMenu->setCursor(1, 2); snprintf(lcd_buf, 255, "RefL: %s \xB1", s1); VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 3); snprintf(lcd_buf, 255, "RefH: %s \xB1", s2); VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 4); snprintf(lcd_buf, 255, "ALow: %s AHigh: %s", s3, s4); VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 5); snprintf(lcd_buf, 255, "BLow: %s BHigh: %s", s5, s6); VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 6); snprintf(lcd_buf, 255, "Diff Ref A-B: %.5f / %.5f", extCalResult.diffRef100AB, extCalResult.diffRef120AB); VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 8); VirtLCDMenu->print("ENTER = speichern, EXIT = verw.");

      menuDigitEndDraw();
      flag.menu_lcd_upd = true;
    }
    return;
  }

  if (st == ST_ERROR)
  {
    ReadButtons(false);
    uint8_t key = menuReadKey(250);
    if (key == MK_ENTER)
    {
      frisch = true;
      menu_level = MENU_CALIBRATION;
      flag.menu_lcd_upd = false;
      flag.short_push = false;
      menuDigitResetTouchDebounce();
      return;
    }

    if (!flag.menu_lcd_upd)
    {
      menuDigitBeginDraw(TXT_REF_EXT_AUTO_TITLE);
      VirtLCDMenu->setCursor(1, 3);
      VirtLCDMenu->print("FEHLER:");
      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print(extCalErrorText[0] ? extCalErrorText : "unbekannt");
      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print("ENTER = zurück");
      menuDigitEndDraw();
      flag.menu_lcd_upd = true;
    }
  }
}
