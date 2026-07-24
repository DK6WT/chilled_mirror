/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPledAdaptation.cpp
 * Zweck: Temperaturabhaengige LED-Vorsteuerung mit System-Grundkurve und SD-Lernmodell.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <Arduino.h>
#include <SD.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "TP_T.h"

extern var_t R;
extern uint8_t led_autoadaptation_mode;
extern float tempUmgebung;
extern float aktuelleLedStromVorgabe;
extern uint8_t ablaufStatus;
extern void setTargetCurrent(float targetCurrentMA);
extern bool sdLogReadyForLogging(void);
extern bool sdLogEnsureReadyForAccess(void);
extern uint8_t sdLogGetStatus(void);
extern bool interfaceSdLoggingEnabled(void);
extern void tpMainConfigSave(void);
extern bool controlParamsEditUnlocked(void);
extern const char* headTypeTextGet(void);

// -----------------------------------------------------------------------------
// Konservative Grenzen aus dem abgestimmten Startkonzept.
// -----------------------------------------------------------------------------
static const float LED_ADAPT_TEMP_MIN_C               = -50.0f;
static const float LED_ADAPT_TEMP_MAX_C               = 110.0f;
static const int16_t LED_ADAPT_BIN_MIN_C              = -50;
static const int16_t LED_ADAPT_BIN_MAX_C              = 110;
static const uint16_t LED_ADAPT_BIN_COUNT             = (uint16_t)(LED_ADAPT_BIN_MAX_C - LED_ADAPT_BIN_MIN_C + 1);
static const float LED_ADAPT_FILTER_TAU_S              = 120.0f;
static const float LED_ADAPT_MAX_TEMP_TREND_K_MIN      = 0.20f;
static const float LED_ADAPT_MAX_RAW_FILTER_DELTA_K    = 2.0f;
static const float LED_ADAPT_MODEL_CORR_LIMIT          = 0.05f;   // erlernte Korrektur max. +/-5 %
static const float LED_ADAPT_TOTAL_CURRENT_LIMIT       = 0.03f;   // Strom gegen letzte Auto-Cal max. +/-3 %
static const float LED_ADAPT_SLEW_PER_SECOND           = 0.0002f; // 0,02 %/s
static const float LED_ADAPT_LED_MIN_MA                = 2.0f;
static const float LED_ADAPT_LED_MAX_MA                = 80.0f;
static const uint16_t LED_ADAPT_MIN_LEARN_COUNT        = 3U;
// Die Vertrauenszaehlung bleibt fuer Diagnose und Freigabestufen erhalten.
// Die Mittelwertnachfuehrung verwendet davon getrennt hoechstens 128 Werte
// effektive Historie. Ab dann hat jeder neue gueltige Wert 1/128 Einfluss.
static const uint16_t LED_ADAPT_CONFIDENCE_COUNT_SATURATION = 1000U;
static const uint16_t LED_ADAPT_EFFECTIVE_HISTORY        = 128U;
static const float LED_ADAPT_OUTLIER_LIMIT             = 0.01f;   // 1 % gegen Klassenmittel
static const uint8_t LED_ADAPT_OUTLIER_CONFIRMATIONS   = 3U;
static const uint8_t LED_ADAPT_INTERP_SIDE_MAX_K       = 3U;
static const uint8_t LED_ADAPT_INTERP_GAP_MAX_K        = 6U;
static const uint8_t LED_ADAPT_EXTRAP_FADE_K           = 3U;
static const uint32_t LED_ADAPT_MEDIA_CHECK_INTERVAL_MS = 300000UL; // 5 min ohne aktives Logging

// Laufzeitstatus des SD-Loggers. Die Werte entsprechen TPsdLog.ino; die
// LED-Autoadaption fragt bei aktivem Logging nur diesen bereits gepflegten
// RAM-Status ab und startet keine eigene zyklische Kartenabfrage.
static const uint8_t LED_ADAPT_SDLOG_STATUS_NO_CARD  = 1U;
static const uint8_t LED_ADAPT_SDLOG_STATUS_LOGGING  = 3U;
static const uint8_t LED_ADAPT_SDLOG_STATUS_FILE_ERR = 4U;

static const uint32_t LED_ADAPT_MODEL_MAGIC            = 0x4C41444DUL; // 'LADM'
static const uint16_t LED_ADAPT_MODEL_VERSION          = 2U;
static const char LED_ADAPT_DIRECTORY[]                = "/LEDADAPT";
// Bevorzugte, auf SD austauschbare Grundkennlinie der kompletten
// TP-3000-Optikkette. Fehlt sie oder ist sie ungueltig, bleibt die fest
// eingebaute OD-850FHT-Herstellerkurve aktiv.
static const char LED_ADAPT_SYSTEM_CURVE_PATH[]         = "/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV";
static const int16_t LED_ADAPT_SYSTEM_CURVE_GRID_MIN_C  = -50;
static const int16_t LED_ADAPT_SYSTEM_CURVE_GRID_MAX_C  = 110;
static const uint8_t LED_ADAPT_SYSTEM_CURVE_GRID_STEP_K = 1U;
static const uint16_t LED_ADAPT_SYSTEM_CURVE_POINT_COUNT =
    (uint16_t)(LED_ADAPT_SYSTEM_CURVE_GRID_MAX_C -
               LED_ADAPT_SYSTEM_CURVE_GRID_MIN_C + 1);
static const size_t LED_ADAPT_SYSTEM_CURVE_MAX_FILE_SIZE = 8192U;
static const float LED_ADAPT_SYSTEM_CURVE_FACTOR_MIN    = 0.50f;
static const float LED_ADAPT_SYSTEM_CURVE_FACTOR_MAX    = 2.00f;

struct LedAdaptBin
{
  uint16_t count;
  uint8_t pendingCount;
  uint8_t reserved0;
  float meanShape;
  float m2Shape;
  float pendingMeanShape;
  uint32_t lastSequence;
};

struct LedAdaptModel
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  uint16_t headTypeHash;
  uint16_t binCount;
  uint32_t headSerial;
  char headTypeText[HEAD_TYPE_TEXT_LEN + 1U];
  uint8_t reserved0[3];
  uint32_t totalValidAutoCals;
  uint32_t totalAcceptedAutoCals;
  LedAdaptBin bins[LED_ADAPT_BIN_COUNT];
  uint32_t crc32;
};

static_assert(LED_ADAPT_BIN_COUNT == 161U, "LED adaptation bin count changed");
static_assert(LED_ADAPT_SYSTEM_CURVE_POINT_COUNT == 161U, "System ground curve grid changed");
static_assert(sizeof(LedAdaptModel) < 4096U, "LED adaptation model unexpectedly large");

// Die beiden Modellpuffer benoetigen kein DMA. Sie liegen bewusst in RAM1,
// damit NativeEthernet/FNET in RAM2 wieder ausreichend zusammenhaengende
// Reserve fuer Stack, DHCP, TCP-Sockets und den HTTP-Listener besitzt.
static LedAdaptModel ledAdaptModel;
static LedAdaptModel ledAdaptScratchModel;
static bool ledAdaptModelLoaded = false;
static uint16_t ledAdaptLoadedHeadHash = 0U;
static uint32_t ledAdaptLoadedHeadSerial = 0UL;
static uint8_t ledAdaptActiveSlot = 0U;

static bool ledAdaptFilterValid = false;
static float ledAdaptFilteredTempC = 25.0f;
static float ledAdaptTrendRefTempC = 25.0f;
static float ledAdaptTrendKPerMin = 0.0f;
static uint32_t ledAdaptFilterLastMs = 0UL;
static uint32_t ledAdaptTrendRefMs = 0UL;

static bool ledAdaptAnchorValid = false;
static float ledAdaptAnchorTempC = 25.0f;
static float ledAdaptAnchorCurrentMa = 10.0f;
// Vorherige erfolgreiche Auto-Cal bleibt als rein kurzzeitiger Lernanker im RAM.
// Sie wird bewusst nicht ueber Neustarts hinweg persistiert, damit Alterung,
// Reinigung oder Kopfumbau waehrend einer Abschaltung nicht als Temperaturgang
// eingelernt werden.
static bool ledAdaptPreviousAutoCalValid = false;
static float ledAdaptPreviousAutoCalTempC = 25.0f;
static float ledAdaptPreviousAutoCalCurrentMa = 10.0f;
static uint32_t ledAdaptLastApplyMs = 0UL;
static float ledAdaptLastTargetMa = 10.0f;
static float ledAdaptLastGroundRatio = 1.0f;
static float ledAdaptLastLearnedRatio = 1.0f;
static bool ledAdaptSdKnownAvailable = false;
static uint32_t ledAdaptLastSdProbeMs = 0UL;
static uint32_t ledAdaptLastMediaCheckMs = 0UL;
// Der in der schnellen Hauptschleife aufgerufene Einstieg bleibt bewusst klein
// im ITCM. Die eigentliche Filter-, Modell-, SD- und Vorsteuerungslogik wird
// hoechstens alle 250 ms in FLASHMEM abgearbeitet.
static uint32_t ledAdaptTaskDispatchMs = 0UL;

// -----------------------------------------------------------------------------
// Datenblatt-Grundkurve OD-850FHT: typische relative optische Leistung bei
// konstantem Strom. Zwischen den Punkten wird linear interpoliert. Die Kurve
// ist nur Startmodell; echte Auto-Cal-Werte korrigieren sie kopfbezogen.
// -----------------------------------------------------------------------------
struct LedGroundPoint
{
  float tempC;
  float relativePower;
};

struct LedSystemGroundPoint
{
  float tempC;
  float currentFactor;
};

static const LedGroundPoint ledGroundCurve[] = {
  { -50.0f, 1.40f },
  { -25.0f, 1.27f },
  {   0.0f, 1.13f },
  {  25.0f, 1.00f },
  {  50.0f, 0.86f },
  {  75.0f, 0.72f },
  { 100.0f, 0.60f }
};

// Systemkurve und Parserpuffer benoetigen ebenfalls kein DMA. Sie liegen in
// RAM1 und geben den fuer NativeEthernet/FNET kritischen RAM2-Heap frei. Die
// Datei enthaelt relative LED-Stromfaktoren der gesamten Optikkette, normiert
// auf die Referenztemperatur.
static LedSystemGroundPoint ledSystemGroundCurve[LED_ADAPT_SYSTEM_CURVE_POINT_COUNT];
static char ledSystemGroundFileBuffer[LED_ADAPT_SYSTEM_CURVE_MAX_FILE_SIZE];
static uint16_t ledSystemGroundPointCount = 0U;
static bool ledSystemGroundCurveActive = false;
static float ledSystemGroundReferenceTempC = 25.0f;
static float ledSystemGroundValidFromC = (float)LED_ADAPT_SYSTEM_CURVE_GRID_MIN_C;
static float ledSystemGroundValidToC = (float)LED_ADAPT_SYSTEM_CURVE_GRID_MAX_C;
static char ledSystemGroundCurveId[40] = "OD-850FHT-HERSTELLER";
static char ledSystemGroundHeadType[HEAD_TYPE_TEXT_LEN + 1U] = "ANY";
// 24 Bit der kanonischen Kurvenkennung werden in den bereits vorhandenen
// reserved0-Bytes des Lernmodells gespeichert. Wert 0 kennzeichnet die
// interne Herstellerkurve. Modellversion 2 verwendet das feste 161-Punkte-
// Raster von -50 bis +110 Grad Celsius; aeltere 126-Klassen-Modelle werden
// wegen der geaenderten Geometrie bewusst nicht weiterverwendet.
static uint32_t ledAdaptGroundCurveFingerprint24 = 0UL;
static char ledAdaptLastGroundFallbackReason[48] = {0};

static float FLASHMEM ledAdaptClamp(float v, float lo, float hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static uint32_t FLASHMEM ledAdaptCrc32(const void* data, size_t length)
{
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; i++)
  {
    crc ^= p[i];
    for (uint8_t bit = 0; bit < 8U; bit++)
    {
      const uint32_t mask = (uint32_t)-(int32_t)(crc & 1UL);
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

static uint32_t FLASHMEM ledAdaptFnv1aUpdate(uint32_t hash, const void* data, size_t length)
{
  const uint8_t* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0U; i < length; i++)
  {
    hash ^= p[i];
    hash *= 16777619UL;
  }
  return hash;
}

static uint32_t FLASHMEM ledAdaptModelFingerprintGet(const LedAdaptModel& model)
{
  return (uint32_t)model.reserved0[0] |
         ((uint32_t)model.reserved0[1] << 8) |
         ((uint32_t)model.reserved0[2] << 16);
}

static void FLASHMEM ledAdaptModelFingerprintSet(LedAdaptModel& model, uint32_t fingerprint24)
{
  fingerprint24 &= 0x00FFFFFFUL;
  model.reserved0[0] = (uint8_t)(fingerprint24 & 0xFFU);
  model.reserved0[1] = (uint8_t)((fingerprint24 >> 8) & 0xFFU);
  model.reserved0[2] = (uint8_t)((fingerprint24 >> 16) & 0xFFU);
}

static char* FLASHMEM ledAdaptTrim(char* text)
{
  if (text == nullptr) return text;
  while (*text == ' ' || *text == '\t') text++;
  char* end = text + strlen(text);
  while (end > text && (end[-1] == ' ' || end[-1] == '\t')) end--;
  *end = '\0';
  return text;
}

static bool FLASHMEM ledAdaptParseFloat(const char* text, float& output)
{
  if (text == nullptr) return false;
  char local[32];
  size_t n = strlen(text);
  if (n == 0U || n >= sizeof(local)) return false;
  memcpy(local, text, n + 1U);
  for (size_t i = 0U; i < n; i++)
  {
    if (local[i] == ',') local[i] = '.';
  }
  char* end = nullptr;
  const float value = strtof(local, &end);
  if (end == local || *ledAdaptTrim(end) != '\0' || !isfinite(value)) return false;
  output = value;
  return true;
}

static bool FLASHMEM ledAdaptCurveIdValid(const char* text)
{
  if (text == nullptr) return false;
  const size_t n = strlen(text);
  if (n == 0U || n >= sizeof(ledSystemGroundCurveId)) return false;
  for (size_t i = 0U; i < n; i++)
  {
    const char c = text[i];
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    if (!ok) return false;
  }
  return true;
}

static bool FLASHMEM ledAdaptHeadTypeMatches(const char* configured)
{
  if (configured == nullptr) return false;
  if (strcmp(configured, "ANY") == 0 || strcmp(configured, "*") == 0) return true;
  return strncmp(configured, headTypeTextGet(), HEAD_TYPE_TEXT_LEN) == 0;
}

static uint16_t FLASHMEM ledAdaptHeadHash(const char* text)
{
  uint32_t h = 2166136261UL;
  if (text != nullptr)
  {
    while (*text != '\0')
    {
      h ^= (uint8_t)*text++;
      h *= 16777619UL;
    }
  }
  h ^= (h >> 16);
  return (uint16_t)h;
}

static void FLASHMEM ledAdaptBuildPath(char* output, size_t outputSize,
                                       uint16_t headHash, uint32_t headSerial,
                                       uint8_t slot)
{
  if (output == nullptr || outputSize == 0U) return;
  snprintf(output, outputSize, "%s/L%04X%05lu%c.BIN",
           LED_ADAPT_DIRECTORY,
           (unsigned)headHash,
           (unsigned long)(headSerial % 100000UL),
           slot ? 'B' : 'A');
}

static void FLASHMEM ledAdaptInitEmptyModel(uint16_t headHash, uint32_t headSerial)
{
  memset(&ledAdaptModel, 0, sizeof(ledAdaptModel));
  ledAdaptModel.magic = LED_ADAPT_MODEL_MAGIC;
  ledAdaptModel.version = LED_ADAPT_MODEL_VERSION;
  ledAdaptModel.size = sizeof(LedAdaptModel);
  ledAdaptModel.headTypeHash = headHash;
  ledAdaptModel.binCount = LED_ADAPT_BIN_COUNT;
  ledAdaptModel.headSerial = headSerial;
  strncpy(ledAdaptModel.headTypeText, headTypeTextGet(), HEAD_TYPE_TEXT_LEN);
  ledAdaptModel.headTypeText[HEAD_TYPE_TEXT_LEN] = '\0';
  ledAdaptModelFingerprintSet(ledAdaptModel, ledAdaptGroundCurveFingerprint24);
  ledAdaptModel.crc32 = ledAdaptCrc32(&ledAdaptModel,
                                      sizeof(ledAdaptModel) - sizeof(ledAdaptModel.crc32));
  ledAdaptLoadedHeadHash = headHash;
  ledAdaptLoadedHeadSerial = headSerial;
  ledAdaptActiveSlot = 0U;
  ledAdaptModelLoaded = true;
}

static bool FLASHMEM ledAdaptModelValid(const LedAdaptModel& model,
                                        uint16_t headHash, uint32_t headSerial)
{
  if (model.magic != LED_ADAPT_MODEL_MAGIC ||
      model.version != LED_ADAPT_MODEL_VERSION ||
      model.size != sizeof(LedAdaptModel) ||
      model.binCount != LED_ADAPT_BIN_COUNT ||
      model.headTypeHash != headHash ||
      model.headSerial != headSerial ||
      ledAdaptModelFingerprintGet(model) != ledAdaptGroundCurveFingerprint24)
  {
    return false;
  }

  char currentType[HEAD_TYPE_TEXT_LEN + 1U];
  strncpy(currentType, headTypeTextGet(), HEAD_TYPE_TEXT_LEN);
  currentType[HEAD_TYPE_TEXT_LEN] = '\0';
  if (strncmp(model.headTypeText, currentType, HEAD_TYPE_TEXT_LEN) != 0) return false;

  const uint32_t crc = ledAdaptCrc32(&model,
                                      sizeof(model) - sizeof(model.crc32));
  return crc == model.crc32;
}

static bool FLASHMEM ledAdaptReadModelSlot(uint8_t slot, uint16_t headHash,
                                           uint32_t headSerial, LedAdaptModel& output)
{
  char path[40];
  ledAdaptBuildPath(path, sizeof(path), headHash, headSerial, slot);
  File file = SD.open(path, FILE_READ);
  if (!file) return false;

  const size_t got = file.read(reinterpret_cast<uint8_t*>(&output), sizeof(output));
  const bool exactSize = (got == sizeof(output)) && (file.available() == 0);
  file.close();
  return exactSize && ledAdaptModelValid(output, headHash, headSerial);
}

static bool FLASHMEM ledAdaptEnsureDirectory(void)
{
  if (SD.exists(LED_ADAPT_DIRECTORY)) return true;
  return SD.mkdir(LED_ADAPT_DIRECTORY);
}

static float FLASHMEM ledAdaptManufacturerGroundPower(float tempC)
{
  const size_t count = sizeof(ledGroundCurve) / sizeof(ledGroundCurve[0]);
  if (tempC <= ledGroundCurve[0].tempC) return ledGroundCurve[0].relativePower;
  if (tempC >= ledGroundCurve[count - 1U].tempC) return ledGroundCurve[count - 1U].relativePower;

  for (size_t i = 1U; i < count; i++)
  {
    if (tempC <= ledGroundCurve[i].tempC)
    {
      const float x0 = ledGroundCurve[i - 1U].tempC;
      const float x1 = ledGroundCurve[i].tempC;
      const float y0 = ledGroundCurve[i - 1U].relativePower;
      const float y1 = ledGroundCurve[i].relativePower;
      const float f = (tempC - x0) / (x1 - x0);
      return y0 + (y1 - y0) * f;
    }
  }
  return 1.0f;
}

static float FLASHMEM ledAdaptManufacturerCurrentFactor(float tempC)
{
  const float power = ledAdaptManufacturerGroundPower(tempC);
  return (power > 0.05f) ? (1.0f / power) : 1.0f;
}

static float FLASHMEM ledAdaptSystemCurrentFactorInside(float tempC)
{
  if (!ledSystemGroundCurveActive || ledSystemGroundPointCount < 2U) return 1.0f;
  if (tempC <= ledSystemGroundCurve[0].tempC) return ledSystemGroundCurve[0].currentFactor;
  if (tempC >= ledSystemGroundCurve[ledSystemGroundPointCount - 1U].tempC)
    return ledSystemGroundCurve[ledSystemGroundPointCount - 1U].currentFactor;

  for (uint16_t i = 1U; i < ledSystemGroundPointCount; i++)
  {
    if (tempC <= ledSystemGroundCurve[i].tempC)
    {
      const LedSystemGroundPoint& lo = ledSystemGroundCurve[i - 1U];
      const LedSystemGroundPoint& hi = ledSystemGroundCurve[i];
      const float f = (tempC - lo.tempC) / (hi.tempC - lo.tempC);
      return lo.currentFactor + (hi.currentFactor - lo.currentFactor) * f;
    }
  }
  return 1.0f;
}

static void FLASHMEM ledAdaptUseManufacturerGroundCurve(const char* reason)
{
  const bool changed = ledSystemGroundCurveActive || ledAdaptGroundCurveFingerprint24 != 0UL;
  ledSystemGroundCurveActive = false;
  ledSystemGroundPointCount = 0U;
  ledSystemGroundReferenceTempC = 25.0f;
  ledSystemGroundValidFromC = (float)LED_ADAPT_SYSTEM_CURVE_GRID_MIN_C;
  ledSystemGroundValidToC = (float)LED_ADAPT_SYSTEM_CURVE_GRID_MAX_C;
  strncpy(ledSystemGroundCurveId, "OD-850FHT-HERSTELLER", sizeof(ledSystemGroundCurveId) - 1U);
  ledSystemGroundCurveId[sizeof(ledSystemGroundCurveId) - 1U] = '\0';
  strncpy(ledSystemGroundHeadType, "ANY", sizeof(ledSystemGroundHeadType) - 1U);
  ledSystemGroundHeadType[sizeof(ledSystemGroundHeadType) - 1U] = '\0';
  ledAdaptGroundCurveFingerprint24 = 0UL;
  if (changed)
  {
    ledAdaptModelLoaded = false;
    ledAdaptPreviousAutoCalValid = false;
  }
  if (reason != nullptr && reason[0] != '\0' &&
      (changed || strcmp(reason, ledAdaptLastGroundFallbackReason) != 0))
  {
    strncpy(ledAdaptLastGroundFallbackReason, reason,
            sizeof(ledAdaptLastGroundFallbackReason) - 1U);
    ledAdaptLastGroundFallbackReason[sizeof(ledAdaptLastGroundFallbackReason) - 1U] = '\0';
    Serial.print("LED-Autoadaption: Hersteller-Grundkurve aktiv (");
    Serial.print(reason);
    Serial.println(").");
  }
}

static uint32_t FLASHMEM ledAdaptSystemCurveFingerprint(void)
{
  uint32_t hash = 2166136261UL;
  hash = ledAdaptFnv1aUpdate(hash, ledSystemGroundCurveId, strlen(ledSystemGroundCurveId));
  hash = ledAdaptFnv1aUpdate(hash, ledSystemGroundHeadType, strlen(ledSystemGroundHeadType));
  hash = ledAdaptFnv1aUpdate(hash, &ledSystemGroundReferenceTempC,
                            sizeof(ledSystemGroundReferenceTempC));
  hash = ledAdaptFnv1aUpdate(hash, &ledSystemGroundValidFromC,
                            sizeof(ledSystemGroundValidFromC));
  hash = ledAdaptFnv1aUpdate(hash, &ledSystemGroundValidToC,
                            sizeof(ledSystemGroundValidToC));
  hash = ledAdaptFnv1aUpdate(hash, &ledSystemGroundPointCount,
                            sizeof(ledSystemGroundPointCount));
  hash = ledAdaptFnv1aUpdate(hash, ledSystemGroundCurve,
                            (size_t)ledSystemGroundPointCount * sizeof(ledSystemGroundCurve[0]));
  hash &= 0x00FFFFFFUL;
  return (hash == 0UL) ? 1UL : hash;
}

static bool FLASHMEM ledAdaptLoadSystemGroundCurve(void)
{
  if (!ledAdaptSdKnownAvailable || !SD.exists(LED_ADAPT_SYSTEM_CURVE_PATH))
  {
    ledAdaptUseManufacturerGroundCurve("Systemdatei fehlt");
    return false;
  }

  File file = SD.open(LED_ADAPT_SYSTEM_CURVE_PATH, FILE_READ);
  if (!file)
  {
    ledAdaptUseManufacturerGroundCurve("Systemdatei nicht lesbar");
    return false;
  }

  const size_t fileSize = (size_t)file.size();
  if (fileSize == 0U || fileSize >= sizeof(ledSystemGroundFileBuffer))
  {
    file.close();
    ledAdaptUseManufacturerGroundCurve("Systemdatei ungueltige Groesse");
    return false;
  }

  const size_t got = file.read(reinterpret_cast<uint8_t*>(ledSystemGroundFileBuffer), fileSize);
  file.close();
  if (got != fileSize)
  {
    ledAdaptUseManufacturerGroundCurve("Systemdatei unvollstaendig");
    return false;
  }
  ledSystemGroundFileBuffer[fileSize] = '\0';

  bool magicSeen = false;
  bool idSeen = false;
  bool headTypeSeen = false;
  bool gridMinSeen = false;
  bool gridMaxSeen = false;
  bool gridStepSeen = false;
  bool referenceSeen = false;
  bool validFromSeen = false;
  bool validToSeen = false;
  bool dataHeaderSeen = false;

  float gridMinC = NAN;
  float gridMaxC = NAN;
  float gridStepK = NAN;
  float referenceTempC = 25.0f;
  float validFromC = NAN;
  float validToC = NAN;
  char curveId[sizeof(ledSystemGroundCurveId)] = {0};
  char curveHeadType[sizeof(ledSystemGroundHeadType)] = "ANY";
  uint16_t pointCount = 0U;

  char* lineSave = nullptr;
  char* line = strtok_r(ledSystemGroundFileBuffer, "\r\n", &lineSave);
  while (line != nullptr)
  {
    char* text = ledAdaptTrim(line);
    if ((uint8_t)text[0] == 0xEFU && (uint8_t)text[1] == 0xBBU &&
        (uint8_t)text[2] == 0xBFU)
    {
      text += 3;
    }
    if (*text != '\0' && *text != '#')
    {
      char* separator = strchr(text, ';');
      if (separator == nullptr)
      {
        ledAdaptUseManufacturerGroundCurve("Systemdatei Formatfehler");
        return false;
      }
      *separator = '\0';
      char* left = ledAdaptTrim(text);
      char* right = ledAdaptTrim(separator + 1);

      if (!magicSeen)
      {
        if (strcmp(left, "TP3000_SYSTEM_GRUNDKURVE") != 0 || strcmp(right, "2") != 0)
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei Kennung/Version");
          return false;
        }
        magicSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "CURVE_ID") == 0)
      {
        if (idSeen || !ledAdaptCurveIdValid(right))
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei CURVE_ID");
          return false;
        }
        strncpy(curveId, right, sizeof(curveId) - 1U);
        idSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "HEAD_TYPE") == 0)
      {
        if (headTypeSeen || strlen(right) >= sizeof(curveHeadType))
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei HEAD_TYPE");
          return false;
        }
        strncpy(curveHeadType, right, sizeof(curveHeadType) - 1U);
        headTypeSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "GRID_MIN_C") == 0)
      {
        if (gridMinSeen || !ledAdaptParseFloat(right, gridMinC))
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei GRID_MIN_C");
          return false;
        }
        gridMinSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "GRID_MAX_C") == 0)
      {
        if (gridMaxSeen || !ledAdaptParseFloat(right, gridMaxC))
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei GRID_MAX_C");
          return false;
        }
        gridMaxSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "GRID_STEP_K") == 0)
      {
        if (gridStepSeen || !ledAdaptParseFloat(right, gridStepK))
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei GRID_STEP_K");
          return false;
        }
        gridStepSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "REFERENCE_TEMP_C") == 0)
      {
        if (referenceSeen || !ledAdaptParseFloat(right, referenceTempC))
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei Referenztemperatur");
          return false;
        }
        referenceSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "VALID_FROM_C") == 0)
      {
        if (validFromSeen || !ledAdaptParseFloat(right, validFromC))
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei VALID_FROM_C");
          return false;
        }
        validFromSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "VALID_TO_C") == 0)
      {
        if (validToSeen || !ledAdaptParseFloat(right, validToC))
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei VALID_TO_C");
          return false;
        }
        validToSeen = true;
      }
      else if (!dataHeaderSeen && strcmp(left, "TEMP_C") == 0 &&
               strcmp(right, "CURRENT_FACTOR") == 0)
      {
        dataHeaderSeen = true;
      }
      else if (dataHeaderSeen)
      {
        if (pointCount >= LED_ADAPT_SYSTEM_CURVE_POINT_COUNT)
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei zu viele Punkte");
          return false;
        }

        float tempC = NAN;
        float factor = NAN;
        const float expectedTempC =
            (float)LED_ADAPT_SYSTEM_CURVE_GRID_MIN_C +
            (float)pointCount * (float)LED_ADAPT_SYSTEM_CURVE_GRID_STEP_K;
        if (!ledAdaptParseFloat(left, tempC) || !ledAdaptParseFloat(right, factor) ||
            fabsf(tempC - expectedTempC) > 0.001f ||
            factor < LED_ADAPT_SYSTEM_CURVE_FACTOR_MIN ||
            factor > LED_ADAPT_SYSTEM_CURVE_FACTOR_MAX)
        {
          ledAdaptUseManufacturerGroundCurve("Systemdatei ungueltiges 1-K-Raster");
          return false;
        }

        ledSystemGroundCurve[pointCount].tempC = expectedTempC;
        ledSystemGroundCurve[pointCount].currentFactor = factor;
        pointCount++;
      }
      else
      {
        ledAdaptUseManufacturerGroundCurve("Systemdatei unbekanntes Feld");
        return false;
      }
    }
    line = strtok_r(nullptr, "\r\n", &lineSave);
  }

  const bool metadataComplete =
      magicSeen && idSeen && headTypeSeen && gridMinSeen && gridMaxSeen && gridStepSeen &&
      referenceSeen && validFromSeen && validToSeen && dataHeaderSeen;
  const bool fixedGridCorrect =
      fabsf(gridMinC - (float)LED_ADAPT_SYSTEM_CURVE_GRID_MIN_C) <= 0.001f &&
      fabsf(gridMaxC - (float)LED_ADAPT_SYSTEM_CURVE_GRID_MAX_C) <= 0.001f &&
      fabsf(gridStepK - (float)LED_ADAPT_SYSTEM_CURVE_GRID_STEP_K) <= 0.001f &&
      pointCount == LED_ADAPT_SYSTEM_CURVE_POINT_COUNT;
  const bool validRangeCorrect =
      validFromC >= (float)LED_ADAPT_SYSTEM_CURVE_GRID_MIN_C &&
      validToC <= (float)LED_ADAPT_SYSTEM_CURVE_GRID_MAX_C &&
      validFromC < validToC &&
      fabsf(validFromC - roundf(validFromC)) <= 0.001f &&
      fabsf(validToC - roundf(validToC)) <= 0.001f &&
      referenceTempC >= validFromC && referenceTempC <= validToC &&
      fabsf(referenceTempC - roundf(referenceTempC)) <= 0.001f;

  if (!metadataComplete || !fixedGridCorrect || !validRangeCorrect ||
      !ledAdaptHeadTypeMatches(curveHeadType))
  {
    ledAdaptUseManufacturerGroundCurve("Systemdatei unvollstaendig/Raster/Kopftyp");
    return false;
  }

  const bool wasActive = ledSystemGroundCurveActive;
  const uint32_t oldFingerprint = ledAdaptGroundCurveFingerprint24;
  ledSystemGroundPointCount = pointCount;
  ledSystemGroundCurveActive = true;
  ledSystemGroundValidFromC = validFromC;
  ledSystemGroundValidToC = validToC;

  const float referenceFactor = ledAdaptSystemCurrentFactorInside(referenceTempC);
  if (!isfinite(referenceFactor) || referenceFactor < LED_ADAPT_SYSTEM_CURVE_FACTOR_MIN ||
      referenceFactor > LED_ADAPT_SYSTEM_CURVE_FACTOR_MAX)
  {
    ledAdaptUseManufacturerGroundCurve("Systemdatei Referenzfaktor");
    return false;
  }

  for (uint16_t i = 0U; i < ledSystemGroundPointCount; i++)
  {
    ledSystemGroundCurve[i].currentFactor /= referenceFactor;
    if (!isfinite(ledSystemGroundCurve[i].currentFactor) ||
        ledSystemGroundCurve[i].currentFactor < LED_ADAPT_SYSTEM_CURVE_FACTOR_MIN ||
        ledSystemGroundCurve[i].currentFactor > LED_ADAPT_SYSTEM_CURVE_FACTOR_MAX)
    {
      ledAdaptUseManufacturerGroundCurve("Systemdatei Normierung");
      return false;
    }
  }

  strncpy(ledSystemGroundCurveId, curveId, sizeof(ledSystemGroundCurveId) - 1U);
  ledSystemGroundCurveId[sizeof(ledSystemGroundCurveId) - 1U] = '\0';
  strncpy(ledSystemGroundHeadType, curveHeadType, sizeof(ledSystemGroundHeadType) - 1U);
  ledSystemGroundHeadType[sizeof(ledSystemGroundHeadType) - 1U] = '\0';
  ledSystemGroundReferenceTempC = referenceTempC;

  const uint32_t newFingerprint = ledAdaptSystemCurveFingerprint();
  const bool changed = !wasActive || newFingerprint != oldFingerprint;
  ledAdaptGroundCurveFingerprint24 = newFingerprint;
  if (changed)
  {
    ledAdaptModelLoaded = false;
    ledAdaptPreviousAutoCalValid = false;
  }

  ledAdaptLastGroundFallbackReason[0] = '\0';
  Serial.print("LED-Autoadaption: System-Grundkurve geladen: ");
  Serial.print(ledSystemGroundCurveId);
  Serial.print(", Datei ");
  Serial.print(LED_ADAPT_SYSTEM_CURVE_PATH);
  Serial.print(", Raster -50...110 C / 1 K, gueltig ");
  Serial.print(ledSystemGroundValidFromC, 0);
  Serial.print(" bis ");
  Serial.print(ledSystemGroundValidToC, 0);
  Serial.println(" C.");
  return true;
}

static void FLASHMEM ledAdaptFallbackToBaseBecauseNoSd(void)
{
  ledAdaptSdKnownAvailable = false;
  ledAdaptUseManufacturerGroundCurve("keine SD-Karte");
  ledAdaptModelLoaded = false;
  ledAdaptPreviousAutoCalValid = false;

  // Ohne SD bleiben AUS und EIN-GRUNDKURVE voll funktionsfaehig. Nur der
  // selbstlernende Modus wird auf die fest eingebaute Herstellerkurve
  // zurueckgesetzt.
  if (led_autoadaptation_mode == LED_AUTOADAPT_SELF)
  {
    led_autoadaptation_mode = LED_AUTOADAPT_BASE;
    if (!controlParamsEditUnlocked()) tpMainConfigSave();
    Serial.println("LED-Autoadaption: SD fehlt, SELBSTLERNEND auf GRUNDKURVE gesetzt.");
  }
}

static bool FLASHMEM ledAdaptMediaPresent(void)
{
  ledAdaptLastMediaCheckMs = millis();
  if (!SD.mediaPresent())
  {
    ledAdaptFallbackToBaseBecauseNoSd();
    return false;
  }

  ledAdaptSdKnownAvailable = true;
  return true;
}

static bool FLASHMEM ledAdaptEnsureSd(void)
{
  // Bei aktivem Messwertlogging besitzt der Logger die Kartenueberwachung.
  // Fuer einen tatsaechlichen Lese-/Schreibzugriff reicht sein READY-Status;
  // der folgende Dateizugriff erkennt einen zwischenzeitlichen Medienfehler.
  if (sdLogReadyForLogging())
  {
    if (interfaceSdLoggingEnabled())
    {
      ledAdaptSdKnownAvailable = true;
      ledAdaptLastMediaCheckMs = millis();
      return true;
    }
    return ledAdaptMediaPresent();
  }

  const uint32_t nowMs = millis();
  if (ledAdaptLastSdProbeMs != 0UL &&
      (uint32_t)(nowMs - ledAdaptLastSdProbeMs) < 5000UL)
  {
    return false;
  }
  ledAdaptLastSdProbeMs = nowMs;

  if (sdLogEnsureReadyForAccess())
  {
    ledAdaptSdKnownAvailable = true;
    return ledAdaptMediaPresent();
  }

  ledAdaptFallbackToBaseBecauseNoSd();
  return false;
}

static bool FLASHMEM ledAdaptLoadCurrentHeadModel(void)
{
  const uint16_t headHash = ledAdaptHeadHash(headTypeTextGet());
  const uint32_t headSerial = R.head_serial;

  if (ledAdaptModelLoaded &&
      ledAdaptLoadedHeadHash == headHash &&
      ledAdaptLoadedHeadSerial == headSerial)
  {
    return true;
  }

  ledAdaptAnchorValid = false;
  if (!ledAdaptEnsureSd())
  {
    ledAdaptModelLoaded = false;
    return false;
  }

  if (!ledAdaptEnsureDirectory())
  {
    ledAdaptFallbackToBaseBecauseNoSd();
    ledAdaptModelLoaded = false;
    return false;
  }

  const bool validA = ledAdaptReadModelSlot(0U, headHash, headSerial,
                                             ledAdaptModel);
  const bool validB = ledAdaptReadModelSlot(1U, headHash, headSerial,
                                             ledAdaptScratchModel);

  if (!validA && !validB)
  {
    ledAdaptInitEmptyModel(headHash, headSerial);
    Serial.print("LED-Autoadaption: neues Lernmodell fuer ");
    Serial.print(headTypeTextGet());
    Serial.print(" K");
    Serial.println((unsigned long)headSerial);
    return true;
  }

  if (validA && (!validB || ledAdaptModel.sequence >= ledAdaptScratchModel.sequence))
  {
    ledAdaptActiveSlot = 0U;
  }
  else
  {
    ledAdaptModel = ledAdaptScratchModel;
    ledAdaptActiveSlot = 1U;
  }

  ledAdaptLoadedHeadHash = headHash;
  ledAdaptLoadedHeadSerial = headSerial;
  ledAdaptModelLoaded = true;
  Serial.print("LED-Autoadaption: Lernmodell geladen, gueltige Auto-Cals=");
  Serial.println((unsigned long)ledAdaptModel.totalAcceptedAutoCals);
  return true;
}

// Einheitliche SD-Ueberwachung fuer LED-Autoadaption:
// - aktives Logging: ausschliesslich vorhandenen Loggerstatus im RAM auswerten;
// - Logging aus: Karte bei Auto-Cal sofort, sonst spaetestens alle 5 min pruefen.
// Eine erfolgreiche Wiedererkennung laedt Systemkurve und Kopfmodell neu.
static bool FLASHMEM ledAdaptCheckStorage(bool forceWhenLoggingIdle)
{
  const uint32_t nowMs = millis();
  const bool wasAvailable = ledAdaptSdKnownAvailable;

  if (interfaceSdLoggingEnabled())
  {
    // Solange Logging angefordert ist, bestimmt der Logger den Medienstatus.
    // Der 5-min-Timer beginnt dadurch erst nach dem Stoppen des Loggings.
    ledAdaptLastMediaCheckMs = nowMs;
    ledAdaptLastSdProbeMs = nowMs;

    const uint8_t logStatus = sdLogGetStatus();
    if (logStatus == LED_ADAPT_SDLOG_STATUS_NO_CARD ||
        logStatus == LED_ADAPT_SDLOG_STATUS_FILE_ERR)
    {
      ledAdaptFallbackToBaseBecauseNoSd();
      return false;
    }

    if (logStatus == LED_ADAPT_SDLOG_STATUS_LOGGING &&
        sdLogReadyForLogging())
    {
      ledAdaptSdKnownAvailable = true;
      if (!wasAvailable)
      {
        (void)ledAdaptLoadSystemGroundCurve();
        ledAdaptModelLoaded = false;
        (void)ledAdaptLoadCurrentHeadModel();
      }
      return true;
    }

    // INIT ist waehrend Start/Retry ein temporaerer Zustand. Ein bereits
    // gueltig geladener Stand bleibt bis zu einem expliziten Loggerfehler aktiv.
    return ledAdaptSdKnownAvailable;
  }

  if (!forceWhenLoggingIdle && ledAdaptLastMediaCheckMs != 0UL &&
      (uint32_t)(nowMs - ledAdaptLastMediaCheckMs) <
          LED_ADAPT_MEDIA_CHECK_INTERVAL_MS)
  {
    return ledAdaptSdKnownAvailable;
  }

  ledAdaptLastMediaCheckMs = nowMs;
  const bool available = ledAdaptEnsureSd();
  if (available && !wasAvailable)
  {
    (void)ledAdaptLoadSystemGroundCurve();
    ledAdaptModelLoaded = false;
    (void)ledAdaptLoadCurrentHeadModel();
  }
  return available;
}

static bool FLASHMEM ledAdaptSaveModel(void)
{
  if (!ledAdaptModelLoaded || !ledAdaptEnsureSd() || !ledAdaptEnsureDirectory())
  {
    return false;
  }

  ledAdaptModel.sequence++;
  ledAdaptModel.crc32 = ledAdaptCrc32(&ledAdaptModel,
                                      sizeof(ledAdaptModel) - sizeof(ledAdaptModel.crc32));
  const uint8_t targetSlot = ledAdaptActiveSlot ? 0U : 1U;
  char path[40];
  ledAdaptBuildPath(path, sizeof(path), ledAdaptLoadedHeadHash,
                    ledAdaptLoadedHeadSerial, targetSlot);

  if (SD.exists(path) && !SD.remove(path))
  {
    Serial.println("LED-Autoadaption: alte SD-Modellslot-Datei nicht loeschbar.");
    ledAdaptFallbackToBaseBecauseNoSd();
    return false;
  }

  File file = SD.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("LED-Autoadaption: SD-Modellslot nicht schreibbar.");
    ledAdaptFallbackToBaseBecauseNoSd();
    return false;
  }

  const size_t written = file.write(reinterpret_cast<const uint8_t*>(&ledAdaptModel),
                                    sizeof(ledAdaptModel));
  file.flush();
  file.close();
  if (written != sizeof(ledAdaptModel))
  {
    Serial.println("LED-Autoadaption: SD-Modell unvollstaendig geschrieben.");
    ledAdaptFallbackToBaseBecauseNoSd();
    return false;
  }

  if (!ledAdaptReadModelSlot(targetSlot, ledAdaptLoadedHeadHash,
                             ledAdaptLoadedHeadSerial, ledAdaptScratchModel) ||
      ledAdaptScratchModel.sequence != ledAdaptModel.sequence)
  {
    Serial.println("LED-Autoadaption: SD-Modell Rueckpruefung fehlgeschlagen.");
    ledAdaptFallbackToBaseBecauseNoSd();
    return false;
  }

  ledAdaptActiveSlot = targetSlot;
  return true;
}

static float FLASHMEM ledAdaptGroundCurrentFactor(float tempC)
{
  if (!ledSystemGroundCurveActive ||
      ledSystemGroundPointCount != LED_ADAPT_SYSTEM_CURVE_POINT_COUNT)
  {
    return ledAdaptManufacturerCurrentFactor(tempC);
  }

  if (tempC < ledSystemGroundValidFromC)
  {
    const float systemAtEdge = ledAdaptSystemCurrentFactorInside(ledSystemGroundValidFromC);
    const float baseAtEdge = ledAdaptManufacturerCurrentFactor(ledSystemGroundValidFromC);
    const float baseAtTemp = ledAdaptManufacturerCurrentFactor(tempC);
    return (baseAtEdge > 0.0f) ?
        systemAtEdge * (baseAtTemp / baseAtEdge) : systemAtEdge;
  }
  if (tempC > ledSystemGroundValidToC)
  {
    const float systemAtEdge = ledAdaptSystemCurrentFactorInside(ledSystemGroundValidToC);
    const float baseAtEdge = ledAdaptManufacturerCurrentFactor(ledSystemGroundValidToC);
    const float baseAtTemp = ledAdaptManufacturerCurrentFactor(tempC);
    return (baseAtEdge > 0.0f) ?
        systemAtEdge * (baseAtTemp / baseAtEdge) : systemAtEdge;
  }
  return ledAdaptSystemCurrentFactorInside(tempC);
}

static int16_t FLASHMEM ledAdaptTempToBin(float tempC)
{
  int16_t rounded = (int16_t)lroundf(tempC);
  if (rounded < LED_ADAPT_BIN_MIN_C) rounded = LED_ADAPT_BIN_MIN_C;
  if (rounded > LED_ADAPT_BIN_MAX_C) rounded = LED_ADAPT_BIN_MAX_C;
  return rounded;
}

static uint16_t FLASHMEM ledAdaptBinIndex(int16_t tempC)
{
  return (uint16_t)(tempC - LED_ADAPT_BIN_MIN_C);
}

static float FLASHMEM ledAdaptWeightForCount(uint16_t count)
{
  if (count < 3U) return 0.0f;
  if (count <= 5U) return 0.25f;
  if (count <= 9U) return 0.50f;
  if (count <= 19U) return 0.75f;
  return 0.90f;
}

static void FLASHMEM ledAdaptBinAddDirect(LedAdaptBin& bin, float sample, uint16_t multiplicity)
{
  if (!isfinite(sample) || sample <= 0.0f || multiplicity == 0U) return;

  for (uint16_t i = 0U; i < multiplicity; i++)
  {
    if (bin.count == 0U)
    {
      bin.count = 1U;
      bin.meanShape = sample;
      bin.m2Shape = 0.0f;
    }
    else
    {
      // count bleibt die getrennte Vertrauenszaehlung fuer Freigabe und
      // Diagnose. Sie darf weiter bis 1000 steigen. Fuer die Nachfuehrung
      // des Mittelwerts ist die effektive Historie dagegen auf 128 Werte
      // begrenzt. Dadurch werden neuere reale Auto-Cals ab diesem Punkt mit
      // konstant 1/128 gewichtet und alte Werte allmaehlich verdraengt.
      const uint16_t newCount =
          (bin.count < LED_ADAPT_CONFIDENCE_COUNT_SATURATION) ?
          (uint16_t)(bin.count + 1U) : bin.count;
      const uint16_t effectiveCount =
          (newCount < LED_ADAPT_EFFECTIVE_HISTORY) ?
          newCount : LED_ADAPT_EFFECTIVE_HISTORY;

      const float delta = sample - bin.meanShape;
      bin.meanShape += delta / (float)effectiveCount;
      const float delta2 = sample - bin.meanShape;

      if (bin.count < LED_ADAPT_EFFECTIVE_HISTORY)
      {
        // Bis 128 Werten normales Welford-M2.
        bin.m2Shape += delta * delta2;
      }
      else
      {
        // Danach passend zur EWMA-Nachfuehrung auch die bisher nur fuer
        // Diagnose reservierte Streuung mit derselben Historienlaenge
        // begrenzt halten.
        const float decay =
            1.0f - (1.0f / (float)LED_ADAPT_EFFECTIVE_HISTORY);
        float m2 = bin.m2Shape;
        if (!isfinite(m2) || m2 < 0.0f) m2 = 0.0f;
        bin.m2Shape = decay * (m2 + delta * delta2);
      }

      bin.count = newCount;
    }
  }
}

static float FLASHMEM ledAdaptShapeForLearning(float tempC)
{
  if (!ledAdaptModelLoaded || !isfinite(tempC)) return 1.0f;

  const int16_t center = ledAdaptTempToBin(tempC);
  const LedAdaptBin& exact = ledAdaptModel.bins[ledAdaptBinIndex(center)];
  if (exact.count > 0U && isfinite(exact.meanShape) && exact.meanShape > 0.0f)
  {
    return ledAdaptClamp(exact.meanShape,
                         1.0f - LED_ADAPT_MODEL_CORR_LIMIT,
                         1.0f + LED_ADAPT_MODEL_CORR_LIMIT);
  }

  // Fuer die Lernfortschreibung duerfen auch noch nicht freigegebene Klassen
  // als relativer Anschluss dienen. Ihre Werte werden jedoch erst ab drei
  // bestaetigten Auto-Cals in der laufenden Vorsteuerung angewendet.
  int16_t lower = center - 1;
  int16_t upper = center + 1;
  for (uint8_t distance = 1U; distance <= LED_ADAPT_INTERP_SIDE_MAX_K; distance++)
  {
    if (lower >= LED_ADAPT_BIN_MIN_C)
    {
      const LedAdaptBin& bin = ledAdaptModel.bins[ledAdaptBinIndex(lower)];
      if (bin.count > 0U && isfinite(bin.meanShape) && bin.meanShape > 0.0f)
      {
        return ledAdaptClamp(bin.meanShape,
                             1.0f - LED_ADAPT_MODEL_CORR_LIMIT,
                             1.0f + LED_ADAPT_MODEL_CORR_LIMIT);
      }
    }
    if (upper <= LED_ADAPT_BIN_MAX_C)
    {
      const LedAdaptBin& bin = ledAdaptModel.bins[ledAdaptBinIndex(upper)];
      if (bin.count > 0U && isfinite(bin.meanShape) && bin.meanShape > 0.0f)
      {
        return ledAdaptClamp(bin.meanShape,
                             1.0f - LED_ADAPT_MODEL_CORR_LIMIT,
                             1.0f + LED_ADAPT_MODEL_CORR_LIMIT);
      }
    }
    lower--;
    upper++;
  }
  return 1.0f;
}

static bool FLASHMEM ledAdaptLearnTransition(float previousTempC, float previousCurrentMa,
                                             float tempC, float currentMa)
{
  if (!isfinite(previousTempC) || !isfinite(previousCurrentMa) ||
      !isfinite(tempC) || !isfinite(currentMa) ||
      previousTempC < LED_ADAPT_TEMP_MIN_C || previousTempC > LED_ADAPT_TEMP_MAX_C ||
      tempC < LED_ADAPT_TEMP_MIN_C || tempC > LED_ADAPT_TEMP_MAX_C ||
      previousCurrentMa <= LED_ADAPT_LED_MIN_MA || previousCurrentMa >= LED_ADAPT_LED_MAX_MA ||
      currentMa <= LED_ADAPT_LED_MIN_MA || currentMa >= LED_ADAPT_LED_MAX_MA)
  {
    return false;
  }

  if (fabsf(ledAdaptTrendKPerMin) > LED_ADAPT_MAX_TEMP_TREND_K_MIN ||
      (isfinite(tempUmgebung) && fabsf(tempUmgebung - tempC) > LED_ADAPT_MAX_RAW_FILTER_DELTA_K))
  {
    Serial.print("LED-Autoadaption: Lernpunkt wegen Temperaturdynamik verworfen, Trend K/min=");
    Serial.println(ledAdaptTrendKPerMin, 4);
    return false;
  }

  if (!ledAdaptLoadCurrentHeadModel()) return false;

  // Nur der Temperaturgang wird gelernt. Der absolute Strombedarf (LED-Alterung,
  // Spiegelzustand, Mechanik) bleibt ueber den letzten echten Auto-Cal-Strom
  // verankert. Aus zwei aufeinanderfolgenden Auto-Cals wird daher die Abweichung
  // vom Datenblatt-Verhaeltnis bestimmt. Langsame absolute Drift kuerzt sich ueber
  // das kurze Auto-Cal-Intervall weitgehend heraus.
  const float previousGround = ledAdaptGroundCurrentFactor(previousTempC);
  const float currentGround = ledAdaptGroundCurrentFactor(tempC);
  if (!isfinite(previousGround) || !isfinite(currentGround) ||
      previousGround <= 0.0f || currentGround <= 0.0f)
  {
    return false;
  }

  const float previousShape = ledAdaptShapeForLearning(previousTempC);
  const float measuredCurrentRatio = currentMa / previousCurrentMa;
  const float groundCurrentRatio = currentGround / previousGround;
  float sampleShape = previousShape * (measuredCurrentRatio / groundCurrentRatio);
  if (!isfinite(sampleShape) || sampleShape <= 0.0f) return false;
  sampleShape = ledAdaptClamp(sampleShape,
                              1.0f - LED_ADAPT_MODEL_CORR_LIMIT,
                              1.0f + LED_ADAPT_MODEL_CORR_LIMIT);

  const int16_t binTemp = ledAdaptTempToBin(tempC);
  LedAdaptBin& bin = ledAdaptModel.bins[ledAdaptBinIndex(binTemp)];
  ledAdaptModel.totalValidAutoCals++;
  uint16_t acceptedMultiplicity = 1U;

  if (bin.count >= 5U && bin.meanShape > 0.0f)
  {
    const float relativeDeviation = fabsf(sampleShape / bin.meanShape - 1.0f);
    if (relativeDeviation > LED_ADAPT_OUTLIER_LIMIT)
    {
      if (bin.pendingCount == 0U ||
          !isfinite(bin.pendingMeanShape) || bin.pendingMeanShape <= 0.0f ||
          fabsf(sampleShape / bin.pendingMeanShape - 1.0f) > (LED_ADAPT_OUTLIER_LIMIT * 0.5f))
      {
        bin.pendingMeanShape = sampleShape;
        bin.pendingCount = 1U;
      }
      else
      {
        bin.pendingMeanShape += (sampleShape - bin.pendingMeanShape) / (float)(bin.pendingCount + 1U);
        if (bin.pendingCount < 255U) bin.pendingCount++;
      }

      if (bin.pendingCount >= LED_ADAPT_OUTLIER_CONFIRMATIONS)
      {
        acceptedMultiplicity = LED_ADAPT_OUTLIER_CONFIRMATIONS;
        ledAdaptBinAddDirect(bin, bin.pendingMeanShape, acceptedMultiplicity);
        bin.pendingCount = 0U;
        bin.pendingMeanShape = 0.0f;
      }
      else
      {
        acceptedMultiplicity = 0U;
      }
    }
    else
    {
      bin.pendingCount = 0U;
      bin.pendingMeanShape = 0.0f;
      ledAdaptBinAddDirect(bin, sampleShape, 1U);
    }
  }
  else
  {
    bin.pendingCount = 0U;
    bin.pendingMeanShape = 0.0f;
    ledAdaptBinAddDirect(bin, sampleShape, 1U);
  }

  if (acceptedMultiplicity > 0U)
  {
    ledAdaptModel.totalAcceptedAutoCals += acceptedMultiplicity;
    bin.lastSequence = ledAdaptModel.totalValidAutoCals;
  }

  if (!ledAdaptSaveModel()) return false;

  Serial.print("LED-Autoadaption: Lernpunkt T=");
  Serial.print(tempC, 3);
  Serial.print(" C, I=");
  Serial.print(currentMa, 3);
  Serial.print(" mA, Formfaktor=");
  Serial.print(sampleShape, 6);
  Serial.print(", Klasse=");
  Serial.print((int)binTemp);
  Serial.print(" C, n=");
  Serial.print((unsigned)bin.count);
  Serial.print(acceptedMultiplicity > 0U ? ", uebernommen" : ", wartet auf Bestaetigung");
  Serial.println();
  return true;
}

struct LedAdaptEstimate
{
  bool valid;
  float shape;
  float weight;
};

static LedAdaptEstimate FLASHMEM ledAdaptEstimateShape(float tempC)
{
  LedAdaptEstimate result = { false, 1.0f, 0.0f };
  if (!ledAdaptModelLoaded || !isfinite(tempC)) return result;

  const int16_t center = ledAdaptTempToBin(tempC);
  int16_t lower = center;
  int16_t upper = center;
  bool lowerFound = false;
  bool upperFound = false;

  for (int16_t t = center; t >= LED_ADAPT_BIN_MIN_C; t--)
  {
    const LedAdaptBin& bin = ledAdaptModel.bins[ledAdaptBinIndex(t)];
    if (bin.count >= LED_ADAPT_MIN_LEARN_COUNT && isfinite(bin.meanShape) && bin.meanShape > 0.0f)
    {
      lower = t;
      lowerFound = true;
      break;
    }
  }
  for (int16_t t = center; t <= LED_ADAPT_BIN_MAX_C; t++)
  {
    const LedAdaptBin& bin = ledAdaptModel.bins[ledAdaptBinIndex(t)];
    if (bin.count >= LED_ADAPT_MIN_LEARN_COUNT && isfinite(bin.meanShape) && bin.meanShape > 0.0f)
    {
      upper = t;
      upperFound = true;
      break;
    }
  }

  if (lowerFound && upperFound)
  {
    const LedAdaptBin& lo = ledAdaptModel.bins[ledAdaptBinIndex(lower)];
    const LedAdaptBin& hi = ledAdaptModel.bins[ledAdaptBinIndex(upper)];
    const float lowerDistance = tempC - (float)lower;
    const float upperDistance = (float)upper - tempC;
    const int16_t gap = upper - lower;

    if (lower == upper)
    {
      result.valid = true;
      result.shape = lo.meanShape;
      result.weight = ledAdaptWeightForCount(lo.count);
      return result;
    }

    if (lowerDistance <= (float)LED_ADAPT_INTERP_SIDE_MAX_K &&
        upperDistance <= (float)LED_ADAPT_INTERP_SIDE_MAX_K &&
        gap <= (int16_t)LED_ADAPT_INTERP_GAP_MAX_K)
    {
      const float f = (tempC - (float)lower) / (float)gap;
      result.valid = true;
      result.shape = lo.meanShape + (hi.meanShape - lo.meanShape) * f;
      result.weight = fminf(ledAdaptWeightForCount(lo.count),
                            ledAdaptWeightForCount(hi.count));
      return result;
    }
  }

  // Keine Steigungsextrapolation: nur den naechsten Randwert verwenden und
  // seinen Lernanteil innerhalb von 3 K linear auf null ausblenden.
  int16_t nearest = 0;
  bool nearestFound = false;
  float nearestDistance = 1.0e9f;
  if (lowerFound)
  {
    nearest = lower;
    nearestDistance = fabsf(tempC - (float)lower);
    nearestFound = true;
  }
  if (upperFound)
  {
    const float d = fabsf((float)upper - tempC);
    if (!nearestFound || d < nearestDistance)
    {
      nearest = upper;
      nearestDistance = d;
      nearestFound = true;
    }
  }

  if (nearestFound && nearestDistance < (float)LED_ADAPT_EXTRAP_FADE_K)
  {
    const LedAdaptBin& bin = ledAdaptModel.bins[ledAdaptBinIndex(nearest)];
    result.valid = true;
    result.shape = bin.meanShape;
    result.weight = ledAdaptWeightForCount(bin.count) *
                    (1.0f - nearestDistance / (float)LED_ADAPT_EXTRAP_FADE_K);
  }
  return result;
}

static void FLASHMEM ledAdaptUpdateTemperatureFilter(void)
{
  const uint32_t nowMs = millis();
  if (!isfinite(tempUmgebung)) return;

  if (!ledAdaptFilterValid)
  {
    ledAdaptFilteredTempC = tempUmgebung;
    ledAdaptTrendRefTempC = tempUmgebung;
    ledAdaptTrendKPerMin = 0.0f;
    ledAdaptFilterLastMs = nowMs;
    ledAdaptTrendRefMs = nowMs;
    ledAdaptFilterValid = true;
    return;
  }

  const uint32_t elapsedMs = nowMs - ledAdaptFilterLastMs;
  if (elapsedMs < 250U) return;
  ledAdaptFilterLastMs = nowMs;
  const float dt = (float)elapsedMs / 1000.0f;
  const float alpha = dt / (LED_ADAPT_FILTER_TAU_S + dt);
  ledAdaptFilteredTempC += alpha * (tempUmgebung - ledAdaptFilteredTempC);

  const uint32_t trendElapsedMs = nowMs - ledAdaptTrendRefMs;
  if (trendElapsedMs >= 60000UL)
  {
    ledAdaptTrendKPerMin = (ledAdaptFilteredTempC - ledAdaptTrendRefTempC) *
                           (60000.0f / (float)trendElapsedMs);
    ledAdaptTrendRefTempC = ledAdaptFilteredTempC;
    ledAdaptTrendRefMs = nowMs;
  }
}

void FLASHMEM ledAdaptationBegin(void)
{
  ledAdaptUpdateTemperatureFilter();
  ledAdaptLastApplyMs = millis();
  ledAdaptTaskDispatchMs = ledAdaptLastApplyMs;
  ledAdaptLastTargetMa = aktuelleLedStromVorgabe;
  ledAdaptUseManufacturerGroundCurve(nullptr);

  if (!ledAdaptEnsureSd())
  {
    ledAdaptFallbackToBaseBecauseNoSd();
    return;
  }
  (void)ledAdaptLoadSystemGroundCurve();
  (void)ledAdaptLoadCurrentHeadModel();
}

bool FLASHMEM ledAdaptationSdAvailable(void)
{
  // Bewusste Bedienaktion: ohne Logging sofort pruefen; mit aktivem Logging
  // dessen bereits gepflegten Status verwenden.
  return ledAdaptCheckStorage(true);
}

uint8_t FLASHMEM ledAdaptationModeGet(void)
{
  if (led_autoadaptation_mode > LED_AUTOADAPT_MAX)
  {
    led_autoadaptation_mode = LED_AUTOADAPT_DEFAULT;
  }
  if (!ledAdaptSdKnownAvailable && led_autoadaptation_mode == LED_AUTOADAPT_SELF)
  {
    return LED_AUTOADAPT_BASE;
  }
  return led_autoadaptation_mode;
}

bool FLASHMEM ledAdaptationSetMode(uint8_t mode)
{
  if (mode > LED_AUTOADAPT_MAX) return false;
  if (mode == LED_AUTOADAPT_SELF && !ledAdaptationSdAvailable())
  {
    led_autoadaptation_mode = LED_AUTOADAPT_BASE;
    return false;
  }

  led_autoadaptation_mode = mode;
  ledAdaptLastApplyMs = millis();
  return true;
}

void FLASHMEM ledAdaptationOnAutoCalBegin(void)
{
  ledAdaptAnchorValid = false;
  ledAdaptLastApplyMs = millis();
  ledAdaptLastGroundRatio = 1.0f;
  ledAdaptLastLearnedRatio = 1.0f;

  // Jede Auto-Cal ist ein natuerlicher Synchronisationspunkt. Bei gestopptem
  // Logging wird die SD hier unabhaengig vom 5-min-Raster sofort geprueft.
  // Bei aktivem Logging wird nur dessen RAM-Status ausgewertet.
  (void)ledAdaptCheckStorage(true);
}

void FLASHMEM ledAdaptationOnAutoCalSuccess(float ambientTempC, float ledCurrentMa)
{
  ledAdaptUpdateTemperatureFilter();
  const float learnTemp = ledAdaptFilterValid ? ledAdaptFilteredTempC : ambientTempC;
  if (!isfinite(learnTemp) || !isfinite(ledCurrentMa))
  {
    ledAdaptAnchorValid = false;
    return;
  }

  // Der letzte echte Auto-Cal-Wert bleibt immer absoluter Anker, unabhaengig
  // vom gewaehlten Anwendungsmodus. Lernen laeuft bei vorhandener SD immer.
  ledAdaptAnchorTempC = learnTemp;
  ledAdaptAnchorCurrentMa = ledAdaptClamp(ledCurrentMa,
                                          LED_ADAPT_LED_MIN_MA,
                                          LED_ADAPT_LED_MAX_MA);
  ledAdaptAnchorValid = true;
  ledAdaptLastTargetMa = ledAdaptAnchorCurrentMa;
  ledAdaptLastApplyMs = millis();

  if (ledAdaptSdKnownAvailable)
  {
    if (ledAdaptPreviousAutoCalValid)
    {
      (void)ledAdaptLearnTransition(ledAdaptPreviousAutoCalTempC,
                                    ledAdaptPreviousAutoCalCurrentMa,
                                    learnTemp,
                                    ledAdaptAnchorCurrentMa);
    }
    else
    {
      // Der erste erfolgreiche Lauf mit verfuegbarer SD setzt nur den
      // Lernanker. Erst ein zweiter echter Auto-Cal liefert ein
      // driftarmes Temperaturverhaeltnis.
      Serial.println("LED-Autoadaption: erster Auto-Cal setzt Lernanker.");
    }

    ledAdaptPreviousAutoCalTempC = learnTemp;
    ledAdaptPreviousAutoCalCurrentMa = ledAdaptAnchorCurrentMa;
    ledAdaptPreviousAutoCalValid = true;
  }
  else
  {
    // Ohne SD wird die Hersteller-Grundkurve weiter angewendet, aber es wird
    // kein fluechtiger Lernuebergang ueber einen spaeteren Kartenwechsel
    // hinweg aufgebaut.
    ledAdaptPreviousAutoCalValid = false;
  }
}

void FLASHMEM ledAdaptationOnAutoCalFailure(void)
{
  // Ein Timeout- oder Grenzwertlauf darf weder das Modell lernen noch als
  // neuer Temperaturanker fuer die Vorsteuerung dienen.
  ledAdaptAnchorValid = false;
  ledAdaptLastApplyMs = millis();
}

static void FLASHMEM __attribute__((noinline)) ledAdaptationTaskSlow(uint32_t nowMs)
{
  ledAdaptUpdateTemperatureFilter();

  const uint16_t currentHeadHash = ledAdaptHeadHash(headTypeTextGet());
  if (currentHeadHash != ledAdaptLoadedHeadHash ||
      R.head_serial != ledAdaptLoadedHeadSerial)
  {
    ledAdaptAnchorValid = false;
    ledAdaptPreviousAutoCalValid = false;
    if (ledAdaptSdKnownAvailable)
    {
      (void)ledAdaptLoadSystemGroundCurve();
      ledAdaptModelLoaded = false;
      (void)ledAdaptLoadCurrentHeadModel();
    }
  }

  // Aktives Logging liefert den Medienstatus ohne zusaetzlichen SD-Zugriff.
  // Ohne Logging erfolgt die Kartenpruefung spaetestens alle 5 min; jede
  // Auto-Cal kann diesen Zeitpunkt vorziehen und startet das Raster neu.
  (void)ledAdaptCheckStorage(false);

  uint32_t elapsedMs = nowMs - ledAdaptLastApplyMs;
  if (elapsedMs < 1000U) return;
  ledAdaptLastApplyMs = nowMs;

  if (ablaufStatus != 0U || !ledAdaptAnchorValid || !ledAdaptFilterValid) return;

  uint8_t mode = ledAdaptationModeGet();
  float targetMa = ledAdaptAnchorCurrentMa;
  ledAdaptLastGroundRatio = 1.0f;
  ledAdaptLastLearnedRatio = 1.0f;

  if (mode != LED_AUTOADAPT_OFF)
  {
    const float anchorGround = ledAdaptGroundCurrentFactor(ledAdaptAnchorTempC);
    const float currentGround = ledAdaptGroundCurrentFactor(ledAdaptFilteredTempC);
    if (anchorGround > 0.0f && isfinite(anchorGround) && isfinite(currentGround))
    {
      ledAdaptLastGroundRatio = currentGround / anchorGround;
      targetMa *= ledAdaptLastGroundRatio;
    }

    if (mode == LED_AUTOADAPT_SELF && ledAdaptSdKnownAvailable &&
        ledAdaptLoadCurrentHeadModel())
    {
      const LedAdaptEstimate currentEstimate = ledAdaptEstimateShape(ledAdaptFilteredTempC);
      const LedAdaptEstimate anchorEstimate = ledAdaptEstimateShape(ledAdaptAnchorTempC);
      if (currentEstimate.valid && anchorEstimate.valid &&
          currentEstimate.shape > 0.0f && anchorEstimate.shape > 0.0f)
      {
        const float weight = fminf(currentEstimate.weight, anchorEstimate.weight);
        float learnedRatio = currentEstimate.shape / anchorEstimate.shape;
        learnedRatio = ledAdaptClamp(learnedRatio,
                                     1.0f - LED_ADAPT_MODEL_CORR_LIMIT,
                                     1.0f + LED_ADAPT_MODEL_CORR_LIMIT);
        ledAdaptLastLearnedRatio = 1.0f + weight * (learnedRatio - 1.0f);
        targetMa *= ledAdaptLastLearnedRatio;
      }
    }
  }

  const float totalMin = ledAdaptAnchorCurrentMa * (1.0f - LED_ADAPT_TOTAL_CURRENT_LIMIT);
  const float totalMax = ledAdaptAnchorCurrentMa * (1.0f + LED_ADAPT_TOTAL_CURRENT_LIMIT);
  targetMa = ledAdaptClamp(targetMa, totalMin, totalMax);
  targetMa = ledAdaptClamp(targetMa, LED_ADAPT_LED_MIN_MA, LED_ADAPT_LED_MAX_MA);
  ledAdaptLastTargetMa = targetMa;

  const float elapsedS = (float)elapsedMs / 1000.0f;
  float maxStep = ledAdaptAnchorCurrentMa * LED_ADAPT_SLEW_PER_SECOND * elapsedS;
  if (maxStep < 0.0025f) maxStep = 0.0025f;

  float nextMa = aktuelleLedStromVorgabe;
  if (targetMa > nextMa + maxStep) nextMa += maxStep;
  else if (targetMa < nextMa - maxStep) nextMa -= maxStep;
  else nextMa = targetMa;

  nextMa = ledAdaptClamp(nextMa, LED_ADAPT_LED_MIN_MA, LED_ADAPT_LED_MAX_MA);
  if (fabsf(nextMa - aktuelleLedStromVorgabe) >= 0.0020f)
  {
    aktuelleLedStromVorgabe = nextMa;
    setTargetCurrent(aktuelleLedStromVorgabe);
  }
}

void ledAdaptationTask(void)
{
  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - ledAdaptTaskDispatchMs) < 250UL) return;
  ledAdaptTaskDispatchMs = nowMs;
  ledAdaptationTaskSlow(nowMs);
}

bool FLASHMEM ledAdaptationResetCurrentHead(char* message, size_t messageSize)
{
  if (message != nullptr && messageSize > 0U) message[0] = '\0';
  if (!ledAdaptEnsureSd())
  {
    if (message != nullptr && messageSize > 0U)
      snprintf(message, messageSize, "Keine SD-Karte / No SD card");
    return false;
  }

  const uint16_t headHash = ledAdaptHeadHash(headTypeTextGet());
  const uint32_t headSerial = R.head_serial;
  bool ok = true;
  for (uint8_t slot = 0U; slot < 2U; slot++)
  {
    char path[40];
    ledAdaptBuildPath(path, sizeof(path), headHash, headSerial, slot);
    if (SD.exists(path) && !SD.remove(path)) ok = false;
  }

  if (!ok)
  {
    if (message != nullptr && messageSize > 0U)
      snprintf(message, messageSize, "Lerndaten nicht loeschbar / Cannot delete learning data");
    return false;
  }

  ledAdaptInitEmptyModel(headHash, headSerial);
  ledAdaptPreviousAutoCalValid = false;
  if (message != nullptr && messageSize > 0U)
  {
    snprintf(message, messageSize, "Lerndaten %s K%05lu geloescht",
             headTypeTextGet(), (unsigned long)(headSerial % 100000UL));
  }
  Serial.print("LED-Autoadaption: Lerndaten geloescht fuer ");
  Serial.print(headTypeTextGet());
  Serial.print(" K");
  Serial.println((unsigned long)headSerial);
  return true;
}

uint32_t FLASHMEM ledAdaptationAcceptedCount(void)
{
  if (!ledAdaptModelLoaded) return 0UL;
  return ledAdaptModel.totalAcceptedAutoCals;
}

uint16_t FLASHMEM ledAdaptationValidBinCount(void)
{
  if (!ledAdaptModelLoaded) return 0U;
  uint16_t count = 0U;
  for (uint16_t i = 0U; i < LED_ADAPT_BIN_COUNT; i++)
  {
    if (ledAdaptModel.bins[i].count >= LED_ADAPT_MIN_LEARN_COUNT) count++;
  }
  return count;
}

float FLASHMEM ledAdaptationFilteredTemperatureC(void)
{
  return ledAdaptFilterValid ? ledAdaptFilteredTempC : NAN;
}

float FLASHMEM ledAdaptationTargetCurrentMa(void)
{
  return ledAdaptLastTargetMa;
}

float FLASHMEM ledAdaptationGroundRatio(void)
{
  return ledAdaptLastGroundRatio;
}

float FLASHMEM ledAdaptationLearnedRatio(void)
{
  return ledAdaptLastLearnedRatio;
}


bool FLASHMEM ledAdaptationSystemGroundCurveActive(void)
{
  return ledSystemGroundCurveActive;
}

const char* FLASHMEM ledAdaptationGroundCurveId(void)
{
  return ledSystemGroundCurveId;
}

const char* FLASHMEM ledAdaptationSystemGroundCurvePath(void)
{
  return LED_ADAPT_SYSTEM_CURVE_PATH;
}
