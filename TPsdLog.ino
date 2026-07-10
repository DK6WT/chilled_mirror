/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPsdLog.ino
 * Zweck: Gepuffertes SD-Logging auf dem internen Teensy-4.1-SD-Slot.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
// TPsdLog.ino
// SD-Logging auf dem internen Teensy-4.1-SD-Slot
// =========================================================================
// Bibliothek: Arduino/Teensy SD.h
// Zugriff:    SD.begin(BUILTIN_SDCARD)
// Format:     CSV-DE mit Semikolon, Tagesdateien unter /LOG/YYMMDD.CSV
//
// Diagnose-Logging V28:
// - sammelt 1 Hz Diagnose-Snapshots im RAM
// - schreibt gesammelt auf SD, maximal einmal pro Minute
// - laeuft unabhaengig vom Bildschirm, also auch im Hauptscreen
// =========================================================================

#include <Arduino.h>
#include <SD.h>
#include <TimeLib.h>
#include <math.h>
#include <string.h>
#include "TP_T.h"

// Einstellungen aus TPmenu_Interface.ino
extern bool interfaceSdLoggingEnabled(void);
extern uint32_t interfaceSdLogIntervalMsValue(void);
extern uint32_t interfaceSdOutputIntervalMsValue(void);
extern uint8_t interfaceSdOutputFilterIndex(void);
extern bool interfaceSdDiagnosticDataEnabled(void);
extern bool outputDataGetSample(uint8_t filterIndex, output_data_sample_t* out);
extern uint32_t outputDataRawSequence(void);
extern bool interfaceSdLogHeaderEnabled(void);
extern bool interfaceFlowDisplayEnabled(void);
extern const char* interfaceFlowDisplayUnitText(void);
extern bool interfaceAlmemoTraceEnabled(void);
extern bool serialProtocolTraceIsActive(void);
extern bool serialProtocolTraceHasError(void);
extern bool serialProtocolTraceWriteBlinkActive(void);

// Messwerte aus der Taupunkt-Regelung / Anzeige
extern float tempSpiegel;
extern float tempUmgebung;
extern float relativeFeuchte;
extern float präziserTaupunkt;
extern float baroDruckHPa;
extern double amp_avg;
extern float optikReflexion;
extern float optikTrockenReferenz;
extern float aktuelleLedStromVorgabe;
extern float peltierStromSoll;
extern float peltierSollWert;
extern float integralFehler;
extern uint8_t aktuellerModus;
extern uint8_t ablaufStatus;
extern var_t R;
extern float serialFlowLastValueLMin(void);
extern float serialExternalTempLastValueC(void);
extern uint32_t serialExternalTempAgeMs(void);
extern uint32_t serialExternalTempRxErrors(void);
extern int8_t serialExternalTempPort(void);
extern uint8_t interfaceAlmemoAddress(void);
extern uint8_t interfaceAlmemoChannel(void);
extern float serialAlmemoLastValue(uint8_t index);
extern bool serialAlmemoIsValid(uint8_t index);
extern uint32_t serialAlmemoAgeMs(uint8_t index);
extern uint32_t serialAlmemoRxErrors(uint8_t index);
extern bool interfaceAlmemoChannelEnabled(uint8_t index);
extern uint8_t interfaceAlmemoAddressFor(uint8_t index);
extern uint8_t interfaceAlmemoChannelFor(uint8_t index);
extern const char* interfaceAlmemoDisplayLabel(uint8_t index);
extern const char* interfaceAlmemoCsvUnitText(uint8_t index);
extern const char* sensorFanStatusText(void);

// ADC-Rohwerte / Diagnose
extern volatile int32_t adcRawPt100_1;
extern volatile int32_t adcRawPt100_2;
extern volatile int32_t adcRawRef100R;
extern volatile int32_t adcRawRef120R;
extern volatile int32_t adcRawPhotodiode;
extern volatile int32_t adcRawPeltierStrom;

extern float ads1263StatsCycleRate(void);
extern float ads1263StatsValidRate(void);
extern float ads1263StatsMaxCycleMs(void);
extern float ads1263StatsTimeoutRate(void);
extern float ads1263StatsRngRate(void);
extern float ads1263StatsRawDiscardRate(uint8_t ch);
extern int32_t ads1263Adc2GetRaw(void);
extern int32_t ads1263Adc2GetAvg(void);
extern int32_t ads1263Adc2GetDark(void);
extern int32_t ads1263Adc2GetNet(void);
extern float ads1263Adc2GetRate(void);
extern bool ads1263Adc2GetMinMax(int32_t* minOut, int32_t* maxOut);
extern uint8_t ads1263RefFillCount();
extern uint32_t ads1263RefAcceptedCount();
extern uint32_t ads1263RefRejectRangeCount();
extern uint32_t ads1263RefRejectRatioCount();
extern uint32_t ads1263RefRejectSeedCount();
extern uint32_t ads1263RefRejectJump100Count();
extern uint32_t ads1263RefRejectJump120Count();
extern uint32_t ads1263RefRejectJumpDeltaCount();
extern uint8_t ads1263RefLastRejectReason();
extern uint32_t ads1263RefAgeSeconds();
extern bool ads1263RefIsOld();
extern bool ads1263RefIsError();
extern void loopDebugGetMetricsExt(uint16_t* hz, uint32_t* max_us, uint32_t* peak10_us, bool* enabled);
extern bool safetyIsFaultActive(void);
extern uint8_t safetyGetFaultReason(void);
extern uint8_t safetyGetFaultOrigin(void);
extern const char* safetyGetFaultText(void);
extern uint32_t safetyGetEventCount(void);
extern bool safetyGetTriggerEpoch(uint32_t* epochOut);
extern uint32_t safetyGetTimeoutLoopAgeMs(void);
extern uint8_t safetyGetTimeoutEthStage(void);
extern uint8_t safetyGetTimeoutEthRequest(void);
extern uint32_t safetyGetTimeoutEthElapsedMs(void);
extern uint32_t safetyGetTimeoutEthLastRequestUs(void);
extern uint32_t safetyGetTimeoutEthMaxRequestUs(void);
extern uint8_t safetyGetTimeoutEthMaxRequestCode(void);
extern uint32_t safetyGetTimeoutEthMaxWriteUs(void);
extern uint8_t safetyGetTimeoutSdStage(void);
extern uint32_t safetyGetTimeoutSdElapsedMs(void);
extern uint8_t safetyGetTimeoutSdLastStage(void);
extern uint32_t safetyGetTimeoutSdLastUs(void);
extern uint8_t safetyGetTimeoutSdMaxStage(void);
extern uint32_t safetyGetTimeoutSdMaxUs(void);
extern const char* ethernetDiagStageText(uint8_t stage);
extern const char* ethernetDiagRequestText(uint8_t request);

#define SDLOG_STATUS_INIT       0
#define SDLOG_STATUS_NO_CARD    1
#define SDLOG_STATUS_READY      2
#define SDLOG_STATUS_LOGGING    3
#define SDLOG_STATUS_FILE_ERR   4

#define SDSTORAGE_ACTIVITY_NONE    0
#define SDSTORAGE_ACTIVITY_CSV     1
#define SDSTORAGE_ACTIVITY_SERIAL  2
#define SDSTORAGE_ACTIVITY_BOTH    3
#define SDSTORAGE_ACTIVITY_ERROR   4

#define SDLOG_SAMPLE_INTERVAL_MS       1000UL
#define SDLOG_MAX_FLUSH_INTERVAL_MS  300000UL
#define SDLOG_BUFFER_SAMPLES            250U
#define SDLOG_ALMEMO_CHANNEL_COUNT        2U

static bool sdLogReady = false;
static uint8_t sdLogStatus = SDLOG_STATUS_INIT;
static uint32_t sdLogLastWriteMs = 0;
static uint32_t sdLogLastRetryMs = 0;
static bool sdLogRuntimeBlocked = false;

// Globale SD-Zeitdiagnose. Die Werte werden vom Safety-Timer nur gelesen.
// Der aktuelle Stage bleibt waehrend eines potenziell blockierenden Aufrufs
// gesetzt, sodass ein Timeout mitten in SD.open()/write()/flush() eindeutig
// zugeordnet werden kann.
volatile uint8_t  sdDiagStage = SD_DIAG_IDLE;
volatile uint32_t sdDiagStageStartMs = 0;
volatile uint32_t sdDiagStageStartUs = 0;
volatile uint8_t  sdDiagLastStage = SD_DIAG_IDLE;
volatile uint32_t sdDiagLastUs = 0;
volatile uint8_t  sdDiagMaxStage = SD_DIAG_IDLE;
volatile uint32_t sdDiagMaxUs = 0;

// Zusaetzliche Gesamtzeit fuer einen kompletten CSV-Flush.
// Die bisherigen Einzelstages zeigen open/write/flush/close separat;
// bei kurzen Loop-Peaks ist aber die Summe aus FAT, Header, Write, Flush,
// Timestamp und Close entscheidend. Diese Werte machen die 5-min-Peaks
// ohne Sichtauswertung direkt im CSV sichtbar.
volatile uint32_t sdDiagCsvFlushLastTotalUs = 0;
volatile uint32_t sdDiagCsvFlushMaxTotalUs = 0;
volatile uint16_t sdDiagCsvFlushLastSamples = 0;
volatile uint16_t sdDiagCsvFlushMaxSamples = 0;
volatile uint32_t sdDiagCsvFlushCount = 0;

void sdStorageDiagBegin(uint8_t stage)
{
  sdDiagStageStartMs = millis();
  sdDiagStageStartUs = micros();
  sdDiagStage = stage;
}

void sdStorageDiagEnd(uint8_t stage)
{
  uint32_t elapsedUs = (uint32_t)(micros() - sdDiagStageStartUs);
  sdDiagLastStage = stage;
  sdDiagLastUs = elapsedUs;
  // SD.begin()/mkdir werden bewusst separat behandelt und koennen beim
  // Start laenger dauern. Der Laufzeit-Maximalwert soll dagegen nur die
  // normalen CSV-/Seriellog-Zugriffe erfassen, die die Hauptloop belasten.
  if (stage >= SD_DIAG_CSV_OPEN && elapsedUs > sdDiagMaxUs)
  {
    sdDiagMaxUs = elapsedUs;
    sdDiagMaxStage = stage;
  }
  sdDiagStage = SD_DIAG_IDLE;
  sdDiagStageStartMs = 0;
  sdDiagStageStartUs = 0;
}

const char* sdStorageDiagStageText(uint8_t stage)
{
  switch (stage)
  {
    case SD_DIAG_BEGIN:          return "SD_BEGIN";
    case SD_DIAG_MKDIR:          return "SD_MKDIR";
    case SD_DIAG_CSV_OPEN:       return "CSV_OPEN";
    case SD_DIAG_CSV_HEADER:     return "CSV_HEADER";
    case SD_DIAG_CSV_WRITE:      return "CSV_WRITE";
    case SD_DIAG_CSV_FLUSH:      return "CSV_FLUSH";
    case SD_DIAG_CSV_CLOSE:      return "CSV_CLOSE";
    case SD_DIAG_SERIAL_PREPARE: return "SER_PREP";
    case SD_DIAG_SERIAL_OPEN:    return "SER_OPEN";
    case SD_DIAG_SERIAL_HEADER:  return "SER_HEADER";
    case SD_DIAG_SERIAL_WRITE:   return "SER_WRITE";
    case SD_DIAG_SERIAL_FLUSH:   return "SER_FLUSH";
    case SD_DIAG_SERIAL_CLOSE:   return "SER_CLOSE";
    case SD_DIAG_DOWNLOAD_OPEN:  return "DL_OPEN";
    case SD_DIAG_DOWNLOAD_SEEK:  return "DL_SEEK";
    case SD_DIAG_DOWNLOAD_READ:  return "DL_READ";
    case SD_DIAG_DOWNLOAD_CLOSE: return "DL_CLOSE";
    default:                     return "IDLE";
  }
}

// Kurzer gelber Aktivitaets-Blinker im Hauptdisplay nach einem SD-Schreibzugriff.
// Der eigentliche Status bleibt: aus=weiss, aktiv=gruen, Fehler=rot.
#define SDLOG_WRITE_BLINK_MS 250UL
static uint32_t sdLogLastSampleMs = 0;
static uint32_t sdLogLastRawSeq = 0;
static char sdLogCurrentFile[32] = "";
static char sdLogLastError[32] = "";

// 64-Bit-Laufzeit fuer Langzeit-CSV. millis() selbst laeuft nach rund
// 49,7 Tagen ueber; der obere 32-Bit-Anteil wird deshalb bei jedem Aufruf
// von sdLogTask() fortgeschrieben, auch wenn das Logging gerade ausgeschaltet ist.
static uint32_t sdLogUptimeLast32 = 0;
static uint32_t sdLogUptimeHigh32 = 0;

static uint32_t sdLogUpdateUptime64(void)
{
  uint32_t now32 = millis();
  if (now32 < sdLogUptimeLast32)
  {
    sdLogUptimeHigh32++;
  }
  sdLogUptimeLast32 = now32;
  return now32;
}

struct SdLogDiagSample
{
  uint32_t ms;
  uint32_t uptimeHigh32;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;

  uint8_t mode;
  uint8_t status;
  uint8_t safetyActive;
  uint8_t safetyReason;
  uint8_t safetyOrigin;
  uint32_t safetyEventCount;
  uint32_t safetyTriggerEpoch;
  uint32_t safetyTimeoutLoopAgeMs;
  uint8_t safetyTimeoutEthStage;
  uint8_t safetyTimeoutEthRequest;
  uint32_t safetyTimeoutEthElapsedMs;
  uint32_t safetyTimeoutEthLastRequestUs;
  uint32_t safetyTimeoutEthMaxRequestUs;
  uint8_t safetyTimeoutEthMaxRequestCode;
  uint32_t safetyTimeoutEthMaxWriteUs;
  uint8_t safetyTimeoutSdStage;
  uint32_t safetyTimeoutSdElapsedMs;
  uint8_t safetyTimeoutSdLastStage;
  uint32_t safetyTimeoutSdLastUs;
  uint8_t safetyTimeoutSdMaxStage;
  uint32_t safetyTimeoutSdMaxUs;
  uint8_t sdLastStage;
  uint32_t sdLastUs;
  uint8_t sdMaxStage;
  uint32_t sdMaxUs;
  uint32_t sdCsvFlushLastTotalUs;
  uint32_t sdCsvFlushMaxTotalUs;
  uint16_t sdCsvFlushLastSamples;
  uint16_t sdCsvFlushMaxSamples;
  uint32_t sdCsvFlushCount;

  float tMirror;
  float tMirrorMin;
  float tMirrorMax;
  float tMirrorPpMk;
  float tAmbient;
  float tAmbientMin;
  float tAmbientMax;
  float tAmbientPpMk;
  float dewpoint;
  float rh;
  float pressure;

  float adc1Rate;
  float adc1ValidRate;
  float adc1MaxCycleMs;
  float adc1TdRate;
  float adc1RngRate;
  float adc1DMirrorRate;
  float adc1DAmbientRate;
  float adc1DRef100Rate;
  float adc1DRef120Rate;

  int32_t rawPtMirror;
  int32_t rawPtAmbient;
  int32_t rawRef100;
  int32_t rawRef120;
  float refRatio;
  uint8_t refFill;
  uint32_t refAge_s;
  uint8_t refStatus;
  uint32_t refAccepted;
  uint32_t refRejectRange;
  uint32_t refRejectRatio;
  uint32_t refRejectSeed;
  uint32_t refRejectJ100;
  uint32_t refRejectJ120;
  uint32_t refRejectJDelta;
  uint8_t refLastRejectReason;

  float adc2Rate;
  int32_t adc2Raw;
  int32_t adc2Avg;
  int32_t adc2Dark;
  int32_t adc2Net;
  int32_t adc2PpDigits;

  float opticPct;
  float opticSollPct;
  float ledMa;
  float peltierIstA;
  float peltierSollMa;
  float peltierPwm;
  float integral;
  int32_t peltierRaw;

  uint16_t loopHz;
  float loopMaxMs;
  float loopPeak10Ms;

  float flow;
  bool flowValid;

  float tExt;
  bool tExtValid;
  uint32_t tExtAgeMs;
  uint32_t tExtRxErrors;
  int8_t tExtPort;
  uint8_t tExtAddress;
  uint8_t tExtChannel;

  float almemoValue[SDLOG_ALMEMO_CHANNEL_COUNT];
  bool almemoValid[SDLOG_ALMEMO_CHANNEL_COUNT];
  uint32_t almemoAgeMs[SDLOG_ALMEMO_CHANNEL_COUNT];
  uint32_t almemoRxErrors[SDLOG_ALMEMO_CHANNEL_COUNT];
  uint8_t almemoEnabled[SDLOG_ALMEMO_CHANNEL_COUNT];
  uint8_t almemoAddress[SDLOG_ALMEMO_CHANNEL_COUNT];
  uint8_t almemoChannel[SDLOG_ALMEMO_CHANNEL_COUNT];
};


// Arduino-Preprocessor: explizite Prototypen nach der Struct-Definition,
// damit SdLogDiagSample bei automatisch erzeugten Funktionsprototypen bekannt ist.
static void FLASHMEM sdLogWriteDateTime(Print& f, const SdLogDiagSample& s);
static void FLASHMEM sdLogFillDateTime(SdLogDiagSample& s);
static void FLASHMEM sdLogCaptureSample(SdLogDiagSample& s);
static void FLASHMEM sdLogWriteOneSample(Print& f, const SdLogDiagSample& s);
static void FLASHMEM sdLogWriteAlmemoFields(Print& f, const SdLogDiagSample& s);
static void FLASHMEM sdLogWriteSafetyTriggerDate(Print& f, const SdLogDiagSample& s);
static void FLASHMEM sdLogWriteSafetyTriggerTime(Print& f, const SdLogDiagSample& s);

static DMAMEM SdLogDiagSample sdLogBuffer[SDLOG_BUFFER_SAMPLES];
static uint16_t sdLogBufferCount = 0;

static bool sdLogAccValid = false;
static float sdLogTmirrMin = 0.0f;
static float sdLogTmirrMax = 0.0f;
static float sdLogTumgMin = 0.0f;
static float sdLogTumgMax = 0.0f;

static bool sdLogTimeValid(void)
{
  int y = year();
  return (y >= 2020 && y <= 2099);
}

static void sdLogSetError(const char* txt)
{
  if (txt == nullptr) txt = "";
  strncpy(sdLogLastError, txt, sizeof(sdLogLastError) - 1);
  sdLogLastError[sizeof(sdLogLastError) - 1] = '\0';
}

static bool FLASHMEM sdLogBuildFatDateTime(DateTimeFields& tm)
{
  int y = year();
  if (y < 2020 || y > 2099) return false;

  uint8_t mo = (uint8_t)month();
  uint8_t d  = (uint8_t)day();
  uint8_t h  = (uint8_t)hour();
  uint8_t mi = (uint8_t)minute();
  uint8_t se = (uint8_t)second();

  if (mo < 1U || mo > 12U) return false;
  if (d < 1U || d > 31U) return false;
  if (h > 23U || mi > 59U || se > 59U) return false;

  tm.year = y - 1900;     // DateTimeFields: Jahre seit 1900
  tm.mon  = mo - 1U;      // DateTimeFields: Monate 0..11
  tm.mday = d;
  tm.hour = h;
  tm.min  = mi;
  tm.sec  = se;
  return true;
}

static bool FLASHMEM sdLogFatDateTimeLooksInvalid(const DateTimeFields& tm)
{
  int y = (int)tm.year + 1900;
  if (y < 2020 || y > 2099) return true;
  if (tm.mon > 11U) return true;
  if (tm.mday < 1U || tm.mday > 31U) return true;
  if (tm.hour > 23U || tm.min > 59U || tm.sec > 59U) return true;
  return false;
}

static void FLASHMEM sdLogApplyFatTimestamp(File& f, bool fileWasNew)
{
  DateTimeFields tm = {};
  if (!sdLogBuildFatDateTime(tm)) return;

  DateTimeFields createTm = {};
  bool setCreate = fileWasNew || !f.getCreateTime(createTm) || sdLogFatDateTimeLooksInvalid(createTm);
  if (setCreate)
  {
    f.setCreateTime(tm);
  }

  // Der eigentliche Fix: Der FAT-Modify-Zeitstempel wird nach jedem
  // erfolgreichen Flush explizit aus der gleichen TimeLib/RTC-Zeit gesetzt,
  // aus der auch Dateiname und CSV-Zeitstempel entstehen.
  f.setModifyTime(tm);
}

static void FLASHMEM sdLogBuildFilename(char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0) return;

  if (sdLogTimeValid())
  {
    if (interfaceSdDiagnosticDataEnabled())
    {
      snprintf(out, outSize, "/LOG/%02u%02u%02uD.CSV",
               (unsigned)(year() % 100),
               (unsigned)month(),
               (unsigned)day());
    }
    else
    {
      snprintf(out, outSize, "/LOG/%02u%02u%02u.CSV",
               (unsigned)(year() % 100),
               (unsigned)month(),
               (unsigned)day());
    }
  }
  else
  {
    snprintf(out, outSize, "/LOG/LOG000.CSV");
  }
}

static const char* FLASHMEM sdLogModeText(uint8_t mode)
{
  if (mode == 1) return "KUEHLEN";
  if (mode == 2) return "HEIZEN";
  return "AUS";
}

static const char* FLASHMEM sdLogProcessText(uint8_t status)
{
  if (status == 1) return "FREIHEIZEN";
  if (status == 2) return "LED_AUTO";
  return "REGELT";
}

static void FLASHMEM sdLogWriteDateTime(Print& f, const SdLogDiagSample& s)
{
  if (s.year >= 2020 && s.year <= 2099)
  {
    char b[32];
    snprintf(b, sizeof(b), "%04u-%02u-%02u;%02u:%02u:%02u",
             (unsigned)s.year,
             (unsigned)s.month,
             (unsigned)s.day,
             (unsigned)s.hour,
             (unsigned)s.minute,
             (unsigned)s.second);
    f.print(b);
  }
  else
  {
    f.print("0000-00-00;00:00:00");
  }
}

static void FLASHMEM sdLogPrintFloat(Print& f, float v, uint8_t digits)
{
  if (isfinite(v)) f.print(v, digits);
}

static void FLASHMEM sdLogPrintUptime64(Print& f, uint32_t high32, uint32_t low32)
{
  uint64_t value = ((uint64_t)high32 << 32) | (uint64_t)low32;
  char text[24];
  snprintf(text, sizeof(text), "%llu", (unsigned long long)value);
  f.print(text);
}

static const char* FLASHMEM sdLogSafetyReasonText(uint8_t reason, uint8_t origin)
{
  if (reason == 1)
  {
    switch (origin)
    {
      case SAFETY_ORIGIN_SD:   return "Peltier PWM Timeout SD";
      case SAFETY_ORIGIN_WEB:  return "Peltier PWM Timeout WEB";
      case SAFETY_ORIGIN_ADC:  return "Peltier PWM Timeout ADC";
      case SAFETY_ORIGIN_TFT:  return "Peltier PWM Timeout TFT";
      case SAFETY_ORIGIN_SER:  return "Peltier PWM Timeout SER";
      case SAFETY_ORIGIN_LOOP: return "Peltier PWM Timeout LOOP";
      default:                 return "Peltier PWM Timeout";
    }
  }

  switch (reason)
  {
    case 2: return "DRV8873 nFAULT";
    case 3: return "DRV8873 SPI";
    default: return "OK";
  }
}

static void FLASHMEM sdLogWriteSafetyTriggerDate(Print& f, const SdLogDiagSample& s)
{
  if (s.safetyTriggerEpoch == 0)
  {
    return;
  }

  time_t triggerTime = (time_t)s.safetyTriggerEpoch;
  uint16_t triggerYear = (uint16_t)year(triggerTime);
  if (triggerYear < 2020 || triggerYear > 2099) return;

  const uint8_t triggerDay = (uint8_t)day(triggerTime);
  const uint8_t triggerMonth = (uint8_t)month(triggerTime);

  char text[11];
  text[0] = (char)('0' + (triggerDay / 10));
  text[1] = (char)('0' + (triggerDay % 10));
  text[2] = '.';
  text[3] = (char)('0' + (triggerMonth / 10));
  text[4] = (char)('0' + (triggerMonth % 10));
  text[5] = '.';
  text[6] = (char)('0' + ((triggerYear / 1000) % 10));
  text[7] = (char)('0' + ((triggerYear / 100) % 10));
  text[8] = (char)('0' + ((triggerYear / 10) % 10));
  text[9] = (char)('0' + (triggerYear % 10));
  text[10] = '\0';
  f.print(text);
}

static void FLASHMEM sdLogWriteSafetyTriggerTime(Print& f, const SdLogDiagSample& s)
{
  if (s.safetyTriggerEpoch == 0)
  {
    return;
  }

  time_t triggerTime = (time_t)s.safetyTriggerEpoch;
  uint16_t triggerYear = (uint16_t)year(triggerTime);
  if (triggerYear < 2020 || triggerYear > 2099) return;

  char text[9];
  snprintf(text, sizeof(text), "%02u:%02u:%02u",
           (unsigned)hour(triggerTime),
           (unsigned)minute(triggerTime),
           (unsigned)second(triggerTime));
  f.print(text);
}

static bool FLASHMEM sdLogEnsureReady(void)
{
  if (sdLogReady) return true;

  sdStorageDiagBegin(SD_DIAG_BEGIN);
  bool beginOk = SD.begin(BUILTIN_SDCARD);
  sdStorageDiagEnd(SD_DIAG_BEGIN);
  if (!beginOk)
  {
    sdLogReady = false;
    sdLogStatus = SDLOG_STATUS_NO_CARD;
    sdLogSetError("Keine SD-Karte");
    return false;
  }

  if (!SD.exists("/LOG"))
  {
    sdStorageDiagBegin(SD_DIAG_MKDIR);
    bool mkdirOk = SD.mkdir("/LOG");
    sdStorageDiagEnd(SD_DIAG_MKDIR);
    if (!mkdirOk)
    {
      sdLogReady = false;
      sdLogStatus = SDLOG_STATUS_FILE_ERR;
      sdLogSetError("/LOG Fehler");
      return false;
    }
  }

  sdLogReady = true;
  sdLogStatus = SDLOG_STATUS_READY;
  sdLogSetError("");
  sdLogBuildFilename(sdLogCurrentFile, sizeof(sdLogCurrentFile));
  return true;
}

static void sdLogResetRuntimeBuffers(void)
{
  sdLogBufferCount = 0;
  sdLogLastSampleMs = 0;
  sdLogLastRawSeq = 0;
  sdLogLastWriteMs = 0;
  sdLogAccValid = false;
}

bool sdLogWriteBlinkActive(void)
{
  if (!interfaceSdLoggingEnabled()) return false;
  if (!sdLogReady) return false;
  if (sdLogLastWriteMs == 0) return false;
  return ((uint32_t)(millis() - sdLogLastWriteMs) < SDLOG_WRITE_BLINK_MS);
}

void FLASHMEM sdLogCheckOnce(void)
{
  sdLogRuntimeBlocked = false;
  // Eine bereits initialisierte Karte nicht erneut mit SD.begin() oeffnen.
  // Das wuerde vorhandene File-Handles des ALMEMO-Seriellogs oder eines
  // parallelen Webzugriffs gefaehrden. Nur wenn die Karte noch nicht bereit
  // ist, wird beim Oeffnen der Statusseite genau ein Initialisierungsversuch
  // ausgefuehrt.
  if (sdLogReady) return;

  sdLogStatus = SDLOG_STATUS_INIT;
  sdLogCurrentFile[0] = '\0';
  sdLogSetError("");
  if (!sdLogEnsureReady())
  {
    sdLogRuntimeBlocked = true;
  }
}

void FLASHMEM sdLogBegin(void)
{
  const bool cardAlreadyReady = sdLogReady;

  sdLogStatus = SDLOG_STATUS_INIT;
  sdLogLastRetryMs = 0;
  sdLogRuntimeBlocked = false;
  sdLogCurrentFile[0] = '\0';
  sdLogSetError("");
  sdLogResetRuntimeBuffers();

  // Wurde die Karte bereits fuer Webdownload oder ALMEMO-Seriellog
  // initialisiert, SD.begin() nicht erneut aufrufen. Ein erneutes Begin bei
  // bereits geoeffneter SERIALnnn.TXT koennte deren File-Handle ungueltig
  // machen und damit eines der beiden Logs unterbrechen.
  if (cardAlreadyReady)
  {
    sdLogReady = true;
    sdLogStatus = SDLOG_STATUS_READY;
    sdLogBuildFilename(sdLogCurrentFile, sizeof(sdLogCurrentFile));
    return;
  }

  sdLogReady = false;
  if (!sdLogEnsureReady())
  {
    sdLogRuntimeBlocked = true;
  }
}

void FLASHMEM sdLogResetSchedule(void)
{
  sdLogResetRuntimeBuffers();
}

bool sdLogReadyForLogging(void)
{
  return sdLogReady;
}

bool FLASHMEM sdLogEnsureReadyForAccess(void)
{
  // Karten-/Verzeichniszugriff fuer Webdownload und Seriellog, ohne die
  // Messwertpuffer oder den Zeitplan des normalen CSV-Loggings anzutasten.
  return sdLogEnsureReady();
}

static void sdLogAccumulateLiveValues(void)
{
  if (!isfinite(tempSpiegel) || !isfinite(tempUmgebung)) return;

  if (!sdLogAccValid)
  {
    sdLogTmirrMin = tempSpiegel;
    sdLogTmirrMax = tempSpiegel;
    sdLogTumgMin = tempUmgebung;
    sdLogTumgMax = tempUmgebung;
    sdLogAccValid = true;
  }
  else
  {
    if (tempSpiegel < sdLogTmirrMin) sdLogTmirrMin = tempSpiegel;
    if (tempSpiegel > sdLogTmirrMax) sdLogTmirrMax = tempSpiegel;
    if (tempUmgebung < sdLogTumgMin) sdLogTumgMin = tempUmgebung;
    if (tempUmgebung > sdLogTumgMax) sdLogTumgMax = tempUmgebung;
  }
}

static void FLASHMEM sdLogFillDateTime(SdLogDiagSample& s)
{
  if (sdLogTimeValid())
  {
    s.year = (uint16_t)year();
    s.month = (uint8_t)month();
    s.day = (uint8_t)day();
    s.hour = (uint8_t)hour();
    s.minute = (uint8_t)minute();
    s.second = (uint8_t)second();
  }
  else
  {
    s.year = 0;
    s.month = 0;
    s.day = 0;
    s.hour = 0;
    s.minute = 0;
    s.second = 0;
  }
}

static void FLASHMEM sdLogCaptureSample(SdLogDiagSample& s)
{
  memset(&s, 0, sizeof(s));

  s.ms = sdLogUpdateUptime64();
  s.uptimeHigh32 = sdLogUptimeHigh32;
  sdLogFillDateTime(s);
  s.mode = aktuellerModus;
  s.status = ablaufStatus;
  s.safetyActive = safetyIsFaultActive() ? 1U : 0U;
  s.safetyReason = s.safetyActive ? safetyGetFaultReason() : 0U;
  s.safetyOrigin = s.safetyActive ? safetyGetFaultOrigin() : SAFETY_ORIGIN_NONE;
  s.safetyEventCount = safetyGetEventCount();
  if (s.safetyActive)
  {
    safetyGetTriggerEpoch(&s.safetyTriggerEpoch);
  }
  s.safetyTimeoutLoopAgeMs = safetyGetTimeoutLoopAgeMs();
  s.safetyTimeoutEthStage = safetyGetTimeoutEthStage();
  s.safetyTimeoutEthRequest = safetyGetTimeoutEthRequest();
  s.safetyTimeoutEthElapsedMs = safetyGetTimeoutEthElapsedMs();
  s.safetyTimeoutEthLastRequestUs = safetyGetTimeoutEthLastRequestUs();
  s.safetyTimeoutEthMaxRequestUs = safetyGetTimeoutEthMaxRequestUs();
  s.safetyTimeoutEthMaxRequestCode = safetyGetTimeoutEthMaxRequestCode();
  s.safetyTimeoutEthMaxWriteUs = safetyGetTimeoutEthMaxWriteUs();
  s.safetyTimeoutSdStage = safetyGetTimeoutSdStage();
  s.safetyTimeoutSdElapsedMs = safetyGetTimeoutSdElapsedMs();
  s.safetyTimeoutSdLastStage = safetyGetTimeoutSdLastStage();
  s.safetyTimeoutSdLastUs = safetyGetTimeoutSdLastUs();
  s.safetyTimeoutSdMaxStage = safetyGetTimeoutSdMaxStage();
  s.safetyTimeoutSdMaxUs = safetyGetTimeoutSdMaxUs();
  s.sdLastStage = sdDiagLastStage;
  s.sdLastUs = sdDiagLastUs;
  s.sdMaxStage = sdDiagMaxStage;
  s.sdMaxUs = sdDiagMaxUs;
  s.sdCsvFlushLastTotalUs = sdDiagCsvFlushLastTotalUs;
  s.sdCsvFlushMaxTotalUs = sdDiagCsvFlushMaxTotalUs;
  s.sdCsvFlushLastSamples = sdDiagCsvFlushLastSamples;
  s.sdCsvFlushMaxSamples = sdDiagCsvFlushMaxSamples;
  s.sdCsvFlushCount = sdDiagCsvFlushCount;

  output_data_sample_t outSample;
  if (outputDataGetSample(interfaceSdOutputFilterIndex(), &outSample))
  {
    s.tMirror = outSample.tMirror;
    s.tAmbient = outSample.tAmbient;
    s.dewpoint = outSample.dewpoint;
    s.rh = outSample.rh;
    s.pressure = outSample.pressure;
  }
  else
  {
    s.tMirror = tempSpiegel;
    s.tAmbient = tempUmgebung;
    s.dewpoint = präziserTaupunkt;
    s.rh = relativeFeuchte;
    s.pressure = baroDruckHPa;
  }

  if (sdLogAccValid)
  {
    s.tMirrorMin = sdLogTmirrMin;
    s.tMirrorMax = sdLogTmirrMax;
    s.tMirrorPpMk = (sdLogTmirrMax - sdLogTmirrMin) * 1000.0f;
    s.tAmbientMin = sdLogTumgMin;
    s.tAmbientMax = sdLogTumgMax;
    s.tAmbientPpMk = (sdLogTumgMax - sdLogTumgMin) * 1000.0f;
  }
  else
  {
    s.tMirrorMin = tempSpiegel;
    s.tMirrorMax = tempSpiegel;
    s.tMirrorPpMk = 0.0f;
    s.tAmbientMin = tempUmgebung;
    s.tAmbientMax = tempUmgebung;
    s.tAmbientPpMk = 0.0f;
  }
  sdLogAccValid = false;


  s.adc1Rate = ads1263StatsCycleRate();
  s.adc1ValidRate = ads1263StatsValidRate();
  s.adc1MaxCycleMs = ads1263StatsMaxCycleMs();
  s.adc1TdRate = ads1263StatsTimeoutRate();
  s.adc1RngRate = ads1263StatsRngRate();
  s.adc1DMirrorRate = ads1263StatsRawDiscardRate(0);
  s.adc1DAmbientRate = ads1263StatsRawDiscardRate(1);
  s.adc1DRef100Rate = ads1263StatsRawDiscardRate(2);
  s.adc1DRef120Rate = ads1263StatsRawDiscardRate(3);

  s.rawPtMirror = adcRawPt100_1;
  s.rawPtAmbient = adcRawPt100_2;
  s.rawRef100 = adcRawRef100R;
  s.rawRef120 = adcRawRef120R;
  if (s.rawRef100 != 0)
  {
    s.refRatio = (float)((double)s.rawRef120 / (double)s.rawRef100);
  }
  else
  {
    s.refRatio = NAN;
  }
  s.refFill = ads1263RefFillCount();
  s.refAge_s = ads1263RefAgeSeconds();
  s.refStatus = ads1263RefIsError() ? 2 : (ads1263RefIsOld() ? 1 : 0);
  s.refAccepted = ads1263RefAcceptedCount();
  s.refRejectRange = ads1263RefRejectRangeCount();
  s.refRejectRatio = ads1263RefRejectRatioCount();
  s.refRejectSeed = ads1263RefRejectSeedCount();
  s.refRejectJ100 = ads1263RefRejectJump100Count();
  s.refRejectJ120 = ads1263RefRejectJump120Count();
  s.refRejectJDelta = ads1263RefRejectJumpDeltaCount();
  s.refLastRejectReason = ads1263RefLastRejectReason();

  s.adc2Rate = ads1263Adc2GetRate();
  s.adc2Raw = ads1263Adc2GetRaw();
  s.adc2Avg = ads1263Adc2GetAvg();
  s.adc2Dark = ads1263Adc2GetDark();
  s.adc2Net = ads1263Adc2GetNet();
  int32_t a2Min = 0;
  int32_t a2Max = 0;
  if (ads1263Adc2GetMinMax(&a2Min, &a2Max))
  {
    s.adc2PpDigits = a2Max - a2Min;
  }
  else
  {
    s.adc2PpDigits = 0;
  }

  if (isfinite(optikTrockenReferenz) && fabsf(optikTrockenReferenz) > 1.0f)
  {
    s.opticPct = (optikReflexion / optikTrockenReferenz) * 100.0f;
  }
  else
  {
    s.opticPct = NAN;
  }
  s.opticSollPct = (float)R.optik_sollwert / 10.0f;
  s.ledMa = aktuelleLedStromVorgabe;
  s.peltierIstA = (float)amp_avg;
  s.peltierSollMa = peltierStromSoll;
  s.peltierPwm = peltierSollWert;
  s.integral = integralFehler;
  s.peltierRaw = adcRawPeltierStrom;

  uint32_t loopMaxUs = 0;
  uint32_t loopPeak10Us = 0;
  bool loopEnabled = false;
  loopDebugGetMetricsExt(&s.loopHz, &loopMaxUs, &loopPeak10Us, &loopEnabled);
  (void)loopEnabled;
  s.loopMaxMs = (float)loopMaxUs / 1000.0f;
  s.loopPeak10Ms = (float)loopPeak10Us / 1000.0f;

  float flowValue = serialFlowLastValueLMin();
  s.flowValid = interfaceFlowDisplayEnabled() && isfinite(flowValue);
  s.flow = s.flowValid ? flowValue : NAN;

  float tExtValue = serialExternalTempLastValueC();
  s.tExtValid = isfinite(tExtValue);
  s.tExt = s.tExtValid ? tExtValue : NAN;
  s.tExtAgeMs = serialExternalTempAgeMs();
  s.tExtRxErrors = serialExternalTempRxErrors();
  s.tExtPort = serialExternalTempPort();
  s.tExtAddress = interfaceAlmemoAddress();
  s.tExtChannel = interfaceAlmemoChannel();

  for (uint8_t i = 0; i < SDLOG_ALMEMO_CHANNEL_COUNT; ++i)
  {
    float v = serialAlmemoLastValue(i);
    bool valid = serialAlmemoIsValid(i) && isfinite(v);
    s.almemoValue[i] = valid ? v : NAN;
    s.almemoValid[i] = valid;
    s.almemoAgeMs[i] = serialAlmemoAgeMs(i);
    s.almemoRxErrors[i] = serialAlmemoRxErrors(i);
    s.almemoEnabled[i] = interfaceAlmemoChannelEnabled(i) ? 1U : 0U;
    s.almemoAddress[i] = interfaceAlmemoAddressFor(i);
    s.almemoChannel[i] = interfaceAlmemoChannelFor(i);
  }
}

static void FLASHMEM sdLogWriteHeader(Print& f)
{
  if (!interfaceSdDiagnosticDataEnabled())
  {
    f.println("Datum;Uhrzeit;Millis;Uptime_ms;Modus;Status;SafetyActive;SafetyReason;SafetyTriggerDate;SafetyTriggerTime;SafetyEventCount;SafetyLoopAge_ms;SafetyEthStage;SafetyEthRequest;SafetyEthElapsed_ms;SafetyEthLast_us;SafetyEthMax_us;SafetyEthMaxRequest;SafetyEthMaxWrite_us;SafetySdStage;SafetySdElapsed_ms;SafetySdLastStage;SafetySdLast_us;SafetySdMaxStage;SafetySdMax_us;SdLastStage;SdLast_us;SdMaxStage;SdMax_us;SdCsvFlushLastTotal_us;SdCsvFlushMaxTotal_us;SdCsvFlushLastSamples;SdCsvFlushMaxSamples;SdCsvFlushCount;T_Mirr_C;T_Mirr_min_C;T_Mirr_max_C;T_Mirr_pp_mK;T_Umg_C;T_Umg_min_C;T_Umg_max_C;T_Umg_pp_mK;Taupunkt_C;rF_pct;Druck_hPa;Peltier_Ist_A;Optik_pct;LED_mA;RefAge_s;RefStatus;FAN;Flow;Flow_Unit;T_Ext_C;T_ExtValid;T_ExtAge_ms;T_ExtRxErrors;T_ExtPort;T_ExtAddress;T_ExtChannel;Almemo1_Value;Almemo1_Unit;Almemo1_Valid;Almemo1_Age_ms;Almemo1_RxErrors;Almemo1_Address;Almemo1_Channel;Almemo1_Label;Almemo2_Value;Almemo2_Unit;Almemo2_Valid;Almemo2_Age_ms;Almemo2_RxErrors;Almemo2_Address;Almemo2_Channel;Almemo2_Label");
    return;
  }

  f.println("Datum;Uhrzeit;Millis;Uptime_ms;Modus;Status;SafetyActive;SafetyReason;SafetyTriggerDate;SafetyTriggerTime;SafetyEventCount;SafetyLoopAge_ms;SafetyEthStage;SafetyEthRequest;SafetyEthElapsed_ms;SafetyEthLast_us;SafetyEthMax_us;SafetyEthMaxRequest;SafetyEthMaxWrite_us;SafetySdStage;SafetySdElapsed_ms;SafetySdLastStage;SafetySdLast_us;SafetySdMaxStage;SafetySdMax_us;SdLastStage;SdLast_us;SdMaxStage;SdMax_us;SdCsvFlushLastTotal_us;SdCsvFlushMaxTotal_us;SdCsvFlushLastSamples;SdCsvFlushMaxSamples;SdCsvFlushCount;T_Mirr_C;T_Mirr_min_C;T_Mirr_max_C;T_Mirr_pp_mK;T_Umg_C;T_Umg_min_C;T_Umg_max_C;T_Umg_pp_mK;Taupunkt_C;rF_pct;Druck_hPa;ADC1_rate_Hz;ADC1_valid_Hz;ADC1_cycleMax_ms;ADC1_TD_s;ADC1_RNG_s;ADC1_D_Mirr_s;ADC1_D_Umg_s;ADC1_D_Ref100_s;ADC1_D_Ref120_s;ADC1_PtMirr_raw;ADC1_PtUmg_raw;ADC1_Ref100_raw;ADC1_Ref120_raw;ADC1_RefRatio;RefFill;RefAge_s;RefStatus;RefAccepted;RefRejectRange;RefRejectRatio;RefRejectSeed;RefRejectJ100;RefRejectJ120;RefRejectJDelta;RefLastRejectReason;ADC2_rate_Hz;ADC2_raw;ADC2_avg;ADC2_dark;ADC2_net;ADC2_pp_Dig;Optik_pct;OptikSoll_pct;LED_mA;Peltier_Ist_A;PeltierSoll_mA;Peltier_PWM;Integral;Peltier_raw;Loop_Hz;Loop_Max_ms;Loop_Peak10_ms;Flow;Flow_Unit;T_Ext_C;T_ExtValid;T_ExtAge_ms;T_ExtRxErrors;T_ExtPort;T_ExtAddress;T_ExtChannel;Almemo1_Value;Almemo1_Unit;Almemo1_Valid;Almemo1_Age_ms;Almemo1_RxErrors;Almemo1_Address;Almemo1_Channel;Almemo1_Label;Almemo2_Value;Almemo2_Unit;Almemo2_Valid;Almemo2_Age_ms;Almemo2_RxErrors;Almemo2_Address;Almemo2_Channel;Almemo2_Label");
}

static void FLASHMEM sdLogWriteAlmemoFields(Print& f, const SdLogDiagSample& s)
{
  for (uint8_t i = 0; i < SDLOG_ALMEMO_CHANNEL_COUNT; ++i)
  {
    f.print(';');
    if (s.almemoValid[i]) sdLogPrintFloat(f, s.almemoValue[i], 3);
    f.print(';');
    if (s.almemoEnabled[i]) f.print(interfaceAlmemoCsvUnitText(i));
    f.print(';'); f.print(s.almemoValid[i] ? 1 : 0);
    f.print(';');
    if (s.almemoAgeMs[i] != 0xFFFFFFFFUL) f.print(s.almemoAgeMs[i]);
    f.print(';'); f.print(s.almemoRxErrors[i]);
    f.print(';'); f.print(s.almemoAddress[i]);
    f.print(';'); f.print(s.almemoChannel[i]);
    f.print(';');
    if (s.almemoEnabled[i]) f.print(interfaceAlmemoDisplayLabel(i));
  }
}

static void FLASHMEM sdLogWriteOneSample(Print& f, const SdLogDiagSample& s)
{
  sdLogWriteDateTime(f, s);
  f.print(';'); f.print(s.ms);
  f.print(';'); sdLogPrintUptime64(f, s.uptimeHigh32, s.ms);
  f.print(';'); f.print(sdLogModeText(s.mode));
  f.print(';'); f.print(sdLogProcessText(s.status));
  f.print(';'); f.print(s.safetyActive);
  f.print(';'); f.print(sdLogSafetyReasonText(s.safetyReason, s.safetyOrigin));
  f.print(';'); sdLogWriteSafetyTriggerDate(f, s);
  f.print(';'); sdLogWriteSafetyTriggerTime(f, s);
  f.print(';'); f.print(s.safetyEventCount);
  f.print(';'); f.print(s.safetyTimeoutLoopAgeMs);
  f.print(';'); f.print(ethernetDiagStageText(s.safetyTimeoutEthStage));
  f.print(';'); f.print(ethernetDiagRequestText(s.safetyTimeoutEthRequest));
  f.print(';'); f.print(s.safetyTimeoutEthElapsedMs);
  f.print(';'); f.print(s.safetyTimeoutEthLastRequestUs);
  f.print(';'); f.print(s.safetyTimeoutEthMaxRequestUs);
  f.print(';'); f.print(ethernetDiagRequestText(s.safetyTimeoutEthMaxRequestCode));
  f.print(';'); f.print(s.safetyTimeoutEthMaxWriteUs);
  f.print(';'); f.print(sdStorageDiagStageText(s.safetyTimeoutSdStage));
  f.print(';'); f.print(s.safetyTimeoutSdElapsedMs);
  f.print(';'); f.print(sdStorageDiagStageText(s.safetyTimeoutSdLastStage));
  f.print(';'); f.print(s.safetyTimeoutSdLastUs);
  f.print(';'); f.print(sdStorageDiagStageText(s.safetyTimeoutSdMaxStage));
  f.print(';'); f.print(s.safetyTimeoutSdMaxUs);
  f.print(';'); f.print(sdStorageDiagStageText(s.sdLastStage));
  f.print(';'); f.print(s.sdLastUs);
  f.print(';'); f.print(sdStorageDiagStageText(s.sdMaxStage));
  f.print(';'); f.print(s.sdMaxUs);
  f.print(';'); f.print(s.sdCsvFlushLastTotalUs);
  f.print(';'); f.print(s.sdCsvFlushMaxTotalUs);
  f.print(';'); f.print(s.sdCsvFlushLastSamples);
  f.print(';'); f.print(s.sdCsvFlushMaxSamples);
  f.print(';'); f.print(s.sdCsvFlushCount);

  f.print(';'); sdLogPrintFloat(f, s.tMirror, 4);
  f.print(';'); sdLogPrintFloat(f, s.tMirrorMin, 4);
  f.print(';'); sdLogPrintFloat(f, s.tMirrorMax, 4);
  f.print(';'); sdLogPrintFloat(f, s.tMirrorPpMk, 2);
  f.print(';'); sdLogPrintFloat(f, s.tAmbient, 4);
  f.print(';'); sdLogPrintFloat(f, s.tAmbientMin, 4);
  f.print(';'); sdLogPrintFloat(f, s.tAmbientMax, 4);
  f.print(';'); sdLogPrintFloat(f, s.tAmbientPpMk, 2);
  f.print(';'); sdLogPrintFloat(f, s.dewpoint, 4);
  f.print(';'); sdLogPrintFloat(f, s.rh, 3);
  f.print(';'); sdLogPrintFloat(f, s.pressure, 2);

  if (!interfaceSdDiagnosticDataEnabled())
  {
    f.print(';'); sdLogPrintFloat(f, s.peltierIstA, 4);
    f.print(';'); sdLogPrintFloat(f, s.opticPct, 4);
    f.print(';'); sdLogPrintFloat(f, s.ledMa, 2);
    f.print(';'); f.print(s.refAge_s);
    f.print(';'); f.print(s.refStatus);
    f.print(';'); f.print(sensorFanStatusText());
    f.print(';');
    if (s.flowValid) sdLogPrintFloat(f, s.flow, 3);
    f.print(';');
    if (s.flowValid) f.print(interfaceFlowDisplayUnitText());
    f.print(';'); if (s.tExtValid) sdLogPrintFloat(f, s.tExt, 2);
    f.print(';'); f.print(s.tExtValid ? 1 : 0);
    f.print(';'); if (s.tExtAgeMs != 0xFFFFFFFFUL) f.print(s.tExtAgeMs);
    f.print(';'); f.print(s.tExtRxErrors);
    f.print(';'); if (s.tExtPort >= 0) f.print((unsigned)(s.tExtPort + 1));
    f.print(';'); f.print(s.tExtAddress);
    f.print(';'); f.print(s.tExtChannel);
    sdLogWriteAlmemoFields(f, s);
    f.println();
    return;
  }

  f.print(';'); sdLogPrintFloat(f, s.adc1Rate, 2);
  f.print(';'); sdLogPrintFloat(f, s.adc1ValidRate, 2);
  f.print(';'); sdLogPrintFloat(f, s.adc1MaxCycleMs, 1);
  f.print(';'); sdLogPrintFloat(f, s.adc1TdRate, 2);
  f.print(';'); sdLogPrintFloat(f, s.adc1RngRate, 2);
  f.print(';'); sdLogPrintFloat(f, s.adc1DMirrorRate, 2);
  f.print(';'); sdLogPrintFloat(f, s.adc1DAmbientRate, 2);
  f.print(';'); sdLogPrintFloat(f, s.adc1DRef100Rate, 2);
  f.print(';'); sdLogPrintFloat(f, s.adc1DRef120Rate, 2);

  f.print(';'); f.print(s.rawPtMirror);
  f.print(';'); f.print(s.rawPtAmbient);
  f.print(';'); f.print(s.rawRef100);
  f.print(';'); f.print(s.rawRef120);
  f.print(';'); sdLogPrintFloat(f, s.refRatio, 7);
  f.print(';'); f.print(s.refFill);
  f.print(';'); f.print(s.refAge_s);
  f.print(';'); f.print(s.refStatus);
  f.print(';'); f.print(s.refAccepted);
  f.print(';'); f.print(s.refRejectRange);
  f.print(';'); f.print(s.refRejectRatio);
  f.print(';'); f.print(s.refRejectSeed);
  f.print(';'); f.print(s.refRejectJ100);
  f.print(';'); f.print(s.refRejectJ120);
  f.print(';'); f.print(s.refRejectJDelta);
  f.print(';'); f.print(s.refLastRejectReason);

  f.print(';'); sdLogPrintFloat(f, s.adc2Rate, 1);
  f.print(';'); f.print(s.adc2Raw);
  f.print(';'); f.print(s.adc2Avg);
  f.print(';'); f.print(s.adc2Dark);
  f.print(';'); f.print(s.adc2Net);
  f.print(';'); f.print(s.adc2PpDigits);

  f.print(';'); sdLogPrintFloat(f, s.opticPct, 4);
  f.print(';'); sdLogPrintFloat(f, s.opticSollPct, 2);
  f.print(';'); sdLogPrintFloat(f, s.ledMa, 2);
  f.print(';'); sdLogPrintFloat(f, s.peltierIstA, 4);
  f.print(';'); sdLogPrintFloat(f, s.peltierSollMa, 1);
  f.print(';'); sdLogPrintFloat(f, s.peltierPwm, 0);
  f.print(';'); sdLogPrintFloat(f, s.integral, 3);
  f.print(';'); f.print(s.peltierRaw);

  f.print(';'); f.print(s.loopHz);
  f.print(';'); sdLogPrintFloat(f, s.loopMaxMs, 1);
  f.print(';'); sdLogPrintFloat(f, s.loopPeak10Ms, 1);

  f.print(';');
  if (s.flowValid) sdLogPrintFloat(f, s.flow, 3);
  f.print(';');
  if (s.flowValid) f.print(interfaceFlowDisplayUnitText());
  f.print(';'); if (s.tExtValid) sdLogPrintFloat(f, s.tExt, 2);
  f.print(';'); f.print(s.tExtValid ? 1 : 0);
  f.print(';'); if (s.tExtAgeMs != 0xFFFFFFFFUL) f.print(s.tExtAgeMs);
  f.print(';'); f.print(s.tExtRxErrors);
  f.print(';'); if (s.tExtPort >= 0) f.print((unsigned)(s.tExtPort + 1));
  f.print(';'); f.print(s.tExtAddress);
  f.print(';'); f.print(s.tExtChannel);
  sdLogWriteAlmemoFields(f, s);
  f.println();
}

static void sdStorageDiagRecordCsvFlushTotal(uint32_t startUs, uint16_t sampleCount)
{
  uint32_t elapsedUs = (uint32_t)(micros() - startUs);
  sdDiagCsvFlushLastTotalUs = elapsedUs;
  sdDiagCsvFlushLastSamples = sampleCount;
  sdDiagCsvFlushCount++;
  if (elapsedUs > sdDiagCsvFlushMaxTotalUs)
  {
    sdDiagCsvFlushMaxTotalUs = elapsedUs;
    sdDiagCsvFlushMaxSamples = sampleCount;
  }
}

static bool FLASHMEM sdLogFlushBuffer(void)
{
  if (sdLogBufferCount == 0) return true;
  uint32_t csvTotalStartUs = micros();
  uint16_t csvTotalSamples = sdLogBufferCount;
  if (!sdLogEnsureReady())
  {
    sdStorageDiagRecordCsvFlushTotal(csvTotalStartUs, csvTotalSamples);
    return false;
  }

  char filename[32];
  sdLogBuildFilename(filename, sizeof(filename));
  strncpy(sdLogCurrentFile, filename, sizeof(sdLogCurrentFile) - 1);
  sdLogCurrentFile[sizeof(sdLogCurrentFile) - 1] = '\0';

  sdStorageDiagBegin(SD_DIAG_CSV_OPEN);
  auto f = SD.open(sdLogCurrentFile, FILE_WRITE);
  sdStorageDiagEnd(SD_DIAG_CSV_OPEN);
  if (!f)
  {
    sdStorageDiagRecordCsvFlushTotal(csvTotalStartUs, csvTotalSamples);
    sdLogStatus = SDLOG_STATUS_FILE_ERR;
    sdLogSetError("Datei Fehler");
    return false;
  }

  const bool fileWasNew = (f.size() == 0);
  bool needHeader = (fileWasNew && interfaceSdLogHeaderEnabled());
  if (needHeader)
  {
    sdStorageDiagBegin(SD_DIAG_CSV_HEADER);
    sdLogWriteHeader(f);
    sdStorageDiagEnd(SD_DIAG_CSV_HEADER);
  }

  sdStorageDiagBegin(SD_DIAG_CSV_WRITE);
  for (uint16_t i = 0; i < sdLogBufferCount; i++)
  {
    sdLogWriteOneSample(f, sdLogBuffer[i]);
  }
  sdStorageDiagEnd(SD_DIAG_CSV_WRITE);

  sdStorageDiagBegin(SD_DIAG_CSV_FLUSH);
  sdLogApplyFatTimestamp(f, fileWasNew);
  f.flush();
  sdStorageDiagEnd(SD_DIAG_CSV_FLUSH);

  sdStorageDiagBegin(SD_DIAG_CSV_CLOSE);
  f.close();
  sdStorageDiagEnd(SD_DIAG_CSV_CLOSE);

  sdStorageDiagRecordCsvFlushTotal(csvTotalStartUs, csvTotalSamples);

  sdLogBufferCount = 0;
  sdLogStatus = interfaceSdLoggingEnabled() ? SDLOG_STATUS_LOGGING : SDLOG_STATUS_READY;
  sdLogSetError("");
  sdLogLastWriteMs = millis();
  return true;
}

static bool FLASHMEM sdLogStoreSample(void)
{
  if (sdLogBufferCount >= SDLOG_BUFFER_SAMPLES)
  {
    if (!sdLogFlushBuffer()) return false;
  }

  if (sdLogBufferCount < SDLOG_BUFFER_SAMPLES)
  {
    sdLogCaptureSample(sdLogBuffer[sdLogBufferCount]);
    sdLogBufferCount++;
  }
  return true;
}

void sdLogTask(void)
{
  sdLogUpdateUptime64();

  if (!interfaceSdLoggingEnabled())
  {
    sdLogRuntimeBlocked = false;
    sdLogResetRuntimeBuffers();
    if (sdLogReady && sdLogStatus != SDLOG_STATUS_FILE_ERR)
    {
      sdLogStatus = SDLOG_STATUS_READY;
    }
    return;
  }

  uint32_t nowMs = millis();
  sdLogAccumulateLiveValues();

  if (sdLogRuntimeBlocked)
  {
    // Kein periodisches Nachsuchen im laufenden Betrieb: Ohne Karte oder nach
    // Dateifehlern koennte SD.begin/open/flush wiederholt Sekunden blockieren.
    // Der EEPROM-Schalter bleibt aber auf EIN; ein erneutes Aktivieren im Menue
    // ruft sdLogBegin() auf und versucht es dann bewusst einmal neu.
    return;
  }

  if (!sdLogReady)
  {
    sdLogLastRetryMs = nowMs;
    if (!sdLogEnsureReady())
    {
      sdLogRuntimeBlocked = true;
      sdLogResetRuntimeBuffers();
      return;
    }
  }

  uint32_t sampleIntervalMs = interfaceSdOutputIntervalMsValue();
  bool sampleDue = false;

  if (sampleIntervalMs == 0UL)
  {
    uint32_t seq = outputDataRawSequence();
    if (seq != 0 && seq != sdLogLastRawSeq)
    {
      sdLogLastRawSeq = seq;
      sampleDue = true;
    }
  }
  else if (sdLogLastSampleMs == 0 || (uint32_t)(nowMs - sdLogLastSampleMs) >= sampleIntervalMs)
  {
    sampleDue = true;
  }

  if (sampleDue)
  {
    sdLogLastSampleMs = nowMs;
    if (!sdLogStoreSample())
    {
      sdLogReady = false;
      sdLogRuntimeBlocked = true;
      sdLogLastRetryMs = nowMs;
      sdLogResetRuntimeBuffers();
      return;
    }
  }

  uint32_t flushIntervalMs = interfaceSdLogIntervalMsValue();
  if (flushIntervalMs < 10000UL) flushIntervalMs = 10000UL;
  if (flushIntervalMs > SDLOG_MAX_FLUSH_INTERVAL_MS) flushIntervalMs = SDLOG_MAX_FLUSH_INTERVAL_MS;

  uint32_t nominalSampleMs = (sampleIntervalMs == 0UL) ? 333UL : sampleIntervalMs;
  uint16_t flushSamples = (uint16_t)(flushIntervalMs / nominalSampleMs);
  if (flushSamples < 1) flushSamples = 1;
  if (flushSamples > SDLOG_BUFFER_SAMPLES) flushSamples = SDLOG_BUFFER_SAMPLES;

  if (sdLogBufferCount >= SDLOG_BUFFER_SAMPLES ||
      (sdLogLastWriteMs == 0 && sdLogBufferCount >= flushSamples) ||
      (sdLogLastWriteMs != 0 && (uint32_t)(nowMs - sdLogLastWriteMs) >= flushIntervalMs && sdLogBufferCount > 0))
  {
    if (!sdLogFlushBuffer())
    {
      // Datei-/Kartenfehler: Laufzeit-Logging stoppen, damit das Geraet nicht
      // durch wiederholte SD-Zugriffe kurz einfriert. Der EEPROM-Schalter bleibt
      // absichtlich unveraendert auf EIN.
      sdLogReady = false;
      sdLogRuntimeBlocked = true;
      sdLogLastRetryMs = nowMs;
      sdLogResetRuntimeBuffers();
      return;
    }
  }
}

uint8_t sdLogGetStatus(void)
{
  if (interfaceSdLoggingEnabled() && sdLogReady && sdLogStatus == SDLOG_STATUS_READY)
  {
    return SDLOG_STATUS_LOGGING;
  }
  return sdLogStatus;
}


uint8_t sdStorageActivityState(void)
{
  bool csvRequested = interfaceSdLoggingEnabled();
  bool serialRequested = interfaceAlmemoTraceEnabled();
  uint8_t csvStatus = sdLogGetStatus();

  bool csvActive = csvRequested &&
                   (csvStatus == SDLOG_STATUS_READY || csvStatus == SDLOG_STATUS_LOGGING);
  bool serialActive = serialRequested && serialProtocolTraceIsActive();

  bool error = (csvRequested &&
                (csvStatus == SDLOG_STATUS_NO_CARD || csvStatus == SDLOG_STATUS_FILE_ERR)) ||
               (serialRequested && serialProtocolTraceHasError());

  if (error) return SDSTORAGE_ACTIVITY_ERROR;
  if (csvActive && serialActive) return SDSTORAGE_ACTIVITY_BOTH;
  if (csvActive) return SDSTORAGE_ACTIVITY_CSV;
  if (serialActive) return SDSTORAGE_ACTIVITY_SERIAL;
  return SDSTORAGE_ACTIVITY_NONE;
}

bool sdStorageWriteBlinkActive(void)
{
  return sdLogWriteBlinkActive() || serialProtocolTraceWriteBlinkActive();
}

const char* FLASHMEM sdLogGetStatusTextDE(void)
{
  switch (sdLogGetStatus())
  {
    case SDLOG_STATUS_NO_CARD:  return "Keine Karte";
    case SDLOG_STATUS_READY:    return "Bereit";
    case SDLOG_STATUS_LOGGING:  return "Logging aktiv";
    case SDLOG_STATUS_FILE_ERR: return "Dateifehler";
    case SDLOG_STATUS_INIT:
    default:                    return "Initialisiere";
  }
}

const char* FLASHMEM sdLogGetStatusTextEN(void)
{
  switch (sdLogGetStatus())
  {
    case SDLOG_STATUS_NO_CARD:  return "No card";
    case SDLOG_STATUS_READY:    return "Ready";
    case SDLOG_STATUS_LOGGING:  return "Logging active";
    case SDLOG_STATUS_FILE_ERR: return "File error";
    case SDLOG_STATUS_INIT:
    default:                    return "Init";
  }
}

const char* FLASHMEM sdLogGetCurrentFile(void)
{
  if (sdLogCurrentFile[0] == '\0')
  {
    sdLogBuildFilename(sdLogCurrentFile, sizeof(sdLogCurrentFile));
  }
  return sdLogCurrentFile;
}

const char* FLASHMEM sdLogGetLastError(void)
{
  return sdLogLastError;
}
