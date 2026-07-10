/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPsafety.ino
 * Zweck: Safety-Ueberwachung fuer Peltier, H-Bruecke und Fehler-Latches.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// ============================================================================

#include <TimeLib.h>
#include "TP_T.h"
// TPsafety.ino
// Sicherheitsueberwachung fuer Peltier-/H-Bruecken-PWM
//
// Stufe 1:
// - Die Regel-/Hauptschleife muss die Sicherheitsueberwachung regelmaessig
//   fuettern.
// - Wenn laenger als SAFETY_PELTIER_TIMEOUT_MS kein Feed kommt, werden
//   beide DRV8873-PWM-Mode-Eingaenge sofort auf HIGH/HIGH gesetzt.
// - Der Fehler bleibt gelatcht, damit die Regelung nicht automatisch wieder
//   anlaeuft.
//
// Stufe 2:
// - DRV8873 nFAULT oder DRV8873-SPI-Fehler koennen dieselbe gelatchte Safety
//   ausloesen. Damit schaltet die H-Bruecke nicht mehr still ab, sondern der
//   Fehler wird im Status sichtbar.
// ============================================================================

extern const byte pinEn;
extern const byte pinPh;
extern const byte pinRelay;


// Ethernet-Diagnosewerte werden im Timeout-Moment nur gelesen.
extern volatile uint8_t  ethDiagStage;
extern volatile uint8_t  ethDiagRequest;
extern volatile uint32_t ethDiagRequestStartMs;
extern volatile uint32_t ethDiagLastRequestUs;
extern volatile uint32_t ethDiagMaxRequestUs;
extern volatile uint8_t  ethDiagMaxRequestCode;
extern volatile uint32_t ethDiagMaxWriteUs;
extern volatile uint8_t  sdDiagStage;
extern volatile uint32_t sdDiagStageStartMs;
extern volatile uint8_t  sdDiagLastStage;
extern volatile uint32_t sdDiagLastUs;
extern volatile uint8_t  sdDiagMaxStage;
extern volatile uint32_t sdDiagMaxUs;
extern volatile uint8_t  ethDownloadIoKind;
extern volatile uint8_t  ethDownloadStallDetected;
extern const char* ethernetDiagStageText(uint8_t stage);
extern const char* ethernetDiagRequestText(uint8_t request);
extern const char* sdStorageDiagStageText(uint8_t stage);

static const uint32_t SAFETY_PELTIER_TIMEOUT_MS = 1000UL;
static const uint32_t SAFETY_TIMER_PERIOD_US    = 20000UL;  // 20 ms

#define SAFETY_REASON_NONE        0
#define SAFETY_REASON_TIMEOUT     1
#define SAFETY_REASON_DRV8873     2
#define SAFETY_REASON_DRV8873_SPI 3

static IntervalTimer safetyPeltierTimer;

volatile uint32_t safetyLastFeedMs       = 0;
volatile bool     safetyFaultActive      = false;
volatile bool     safetyEnabled          = false;
volatile bool     safetyPeltierCutDone   = false;
volatile bool     safetyBlockingHold      = false;
volatile uint8_t  safetyFaultReason      = SAFETY_REASON_NONE;
volatile uint8_t  safetyFaultOrigin      = SAFETY_ORIGIN_NONE;
volatile uint32_t safetyTriggerMs         = 0;
volatile uint32_t safetyEventCount        = 0;
volatile bool     safetyTriggerTimePending = false;

static uint32_t safetyTriggerEpoch = 0;
static volatile bool safetyTriggerEpochValid = false;


// Diagnose-Snapshot des letzten Peltier-PWM-Timeouts. Die Werte bleiben auch
// nach einer Safety-Quittierung erhalten und werden erst beim nächsten Timeout
// überschrieben bzw. beim Neustart gelöscht.
static volatile uint32_t safetyTimeoutLoopAgeMs = 0;
static volatile uint8_t  safetyTimeoutEthStage = 0;
static volatile uint8_t  safetyTimeoutEthRequest = 0;
static volatile uint32_t safetyTimeoutEthElapsedMs = 0;
static volatile uint32_t safetyTimeoutEthLastRequestUs = 0;
static volatile uint32_t safetyTimeoutEthMaxRequestUs = 0;
static volatile uint8_t  safetyTimeoutEthMaxRequestCode = 0;
static volatile uint32_t safetyTimeoutEthMaxWriteUs = 0;
static volatile uint8_t  safetyTimeoutSdStage = SD_DIAG_IDLE;
static volatile uint32_t safetyTimeoutSdElapsedMs = 0;
static volatile uint8_t  safetyTimeoutSdLastStage = SD_DIAG_IDLE;
static volatile uint32_t safetyTimeoutSdLastUs = 0;
static volatile uint8_t  safetyTimeoutSdMaxStage = SD_DIAG_IDLE;
static volatile uint32_t safetyTimeoutSdMaxUs = 0;

static uint8_t safetyClassifyTimeoutOrigin(uint8_t sdStage, uint32_t sdElapsedMs,
                                           uint8_t ethStage, uint32_t ethElapsedMs)
{
  // Im ersten Schritt werden nur bereits sicher instrumentierte Bereiche
  // klassifiziert. SD hat Vorrang vor Web, weil Web-Downloads ebenfalls einen
  // Ethernet-Request aktiv haben koennen, die eigentliche Blockade aber im
  // SD-Dateizugriff liegen kann. Nicht instrumentierte Haenger bleiben LOOP.
  if (sdStage != SD_DIAG_IDLE || sdElapsedMs > 0UL)
  {
    return SAFETY_ORIGIN_SD;
  }

  if (ethStage != 0U || ethElapsedMs > 0UL)
  {
    return SAFETY_ORIGIN_WEB;
  }

  return SAFETY_ORIGIN_LOOP;
}

static void safetyCutPeltierNow()
{
  // DRV8873 im PWM-Mode: Pin37=IN1, Pin36=IN2.
  // Sicher AUS ist HIGH/HIGH, denn dann liegen OUT1 und OUT2 beide auf H
  // und am Peltier liegt 0 V Differenz an. LOW waere hier NICHT sicher.
  analogWrite(pinEn, 4096);
  analogWrite(pinPh, 4096);
}

static void safetyPeltierTimerISR()
{
  if (!safetyEnabled || safetyBlockingHold)
  {
    return;
  }

  uint32_t now = millis();
  uint32_t age = now - safetyLastFeedMs;

  if (age > SAFETY_PELTIER_TIMEOUT_MS)
  {
    // Ein einzelner blockierender Download-I/O-Aufruf wird separat behandelt:
    // Peltier sofort sicher AUS, aber keine gelatchte Safety. Nach Rueckkehr
    // bricht der Ethernetpfad den Download ab. Nur SD-Stalls markieren die
    // Datei rot; TCP-/Browserfehler lassen die Datei unveraendert.
    if (!safetyFaultActive && ethDownloadIoKind != ETH_DOWNLOAD_IO_NONE)
    {
      if (ethDownloadStallDetected == ETH_DOWNLOAD_IO_NONE)
      {
        ethDownloadStallDetected = ethDownloadIoKind;
      }

      if (!safetyPeltierCutDone)
      {
        safetyPeltierCutDone = true;
        safetyCutPeltierNow();
      }
      return;
    }

    if (!safetyFaultActive)
    {
      safetyFaultReason = SAFETY_REASON_TIMEOUT;
      safetyTriggerMs = now;
      safetyEventCount++;
      safetyTriggerTimePending = true;
      safetyTriggerEpochValid = false;

      safetyTimeoutLoopAgeMs = age;
      safetyTimeoutEthStage = ethDiagStage;
      safetyTimeoutEthRequest = ethDiagRequest;
      uint32_t requestStartMs = ethDiagRequestStartMs;
      safetyTimeoutEthElapsedMs = requestStartMs ? (uint32_t)(now - requestStartMs) : 0UL;
      safetyTimeoutEthLastRequestUs = ethDiagLastRequestUs;
      safetyTimeoutEthMaxRequestUs = ethDiagMaxRequestUs;
      safetyTimeoutEthMaxRequestCode = ethDiagMaxRequestCode;
      safetyTimeoutEthMaxWriteUs = ethDiagMaxWriteUs;
      safetyTimeoutSdStage = sdDiagStage;
      uint32_t sdStartMs = sdDiagStageStartMs;
      safetyTimeoutSdElapsedMs = (sdDiagStage != SD_DIAG_IDLE && sdStartMs != 0UL)
                               ? (uint32_t)(now - sdStartMs) : 0UL;
      safetyFaultOrigin = safetyClassifyTimeoutOrigin(safetyTimeoutSdStage,
                                                      safetyTimeoutSdElapsedMs,
                                                      safetyTimeoutEthStage,
                                                      safetyTimeoutEthElapsedMs);
      safetyTimeoutSdLastStage = sdDiagLastStage;
      safetyTimeoutSdLastUs = sdDiagLastUs;
      safetyTimeoutSdMaxStage = sdDiagMaxStage;
      safetyTimeoutSdMaxUs = sdDiagMaxUs;
    }
    safetyFaultActive = true;

    // Nur einmal aktiv abschalten. Das vermeidet unnoetige PWM-Zugriffe im ISR.
    if (!safetyPeltierCutDone)
    {
      safetyPeltierCutDone = true;
      safetyCutPeltierNow();
    }
  }
}

void safetyInit()
{
  safetyCutPeltierNow();

  safetyLastFeedMs     = millis();
  safetyFaultActive    = false;
  safetyPeltierCutDone = false;
  safetyBlockingHold   = false;
  safetyFaultReason    = SAFETY_REASON_NONE;
  safetyFaultOrigin    = SAFETY_ORIGIN_NONE;
  safetyTriggerMs      = 0;
  safetyEventCount     = 0;
  safetyTriggerTimePending = false;
  safetyTriggerEpoch   = 0;
  safetyTriggerEpochValid = false;
  safetyTimeoutLoopAgeMs = 0;
  safetyTimeoutEthStage = 0;
  safetyTimeoutEthRequest = 0;
  safetyTimeoutEthElapsedMs = 0;
  safetyTimeoutEthLastRequestUs = 0;
  safetyTimeoutEthMaxRequestUs = 0;
  safetyTimeoutEthMaxRequestCode = 0;
  safetyTimeoutEthMaxWriteUs = 0;
  safetyTimeoutSdStage = SD_DIAG_IDLE;
  safetyTimeoutSdElapsedMs = 0;
  safetyTimeoutSdLastStage = SD_DIAG_IDLE;
  safetyTimeoutSdLastUs = 0;
  safetyTimeoutSdMaxStage = SD_DIAG_IDLE;
  safetyTimeoutSdMaxUs = 0;
  safetyEnabled        = true;

  safetyPeltierTimer.begin(safetyPeltierTimerISR, SAFETY_TIMER_PERIOD_US);
}

void safetyFeedRegelung()
{
  if (!safetyEnabled || safetyFaultActive)
  {
    return;
  }

  safetyLastFeedMs = millis();
}

void safetyBeginBlockingOperation()
{
  // Bekannte blockierende Aktionen, z.B. SD.begin() ohne Karte:
  // Peltier/H-Bruecke vorher sicher abschalten und den Safety-Timer kurz halten,
  // damit kein gelatchter Fehler entsteht, nur weil die SD-Erkennung blockiert.
  safetyCutPeltierNow();

  safetyBlockingHold = true;
  safetyLastFeedMs = millis();
}

void safetyEndBlockingOperation()
{
  // Nach der blockierenden Aktion wieder sauber starten.
  // Ein bereits aktiver Safety-Fehler wird NICHT geloescht.
  safetyLastFeedMs = millis();
  safetyPeltierCutDone = false;
  safetyBlockingHold = false;

  safetyCutPeltierNow();
}


bool safetyIsFaultActive()
{
  return safetyFaultActive;
}

bool safetyPeltierAllowed()
{
  return safetyEnabled && !safetyFaultActive;
}

uint8_t safetyGetFaultReason()
{
  return safetyFaultReason;
}

uint8_t safetyGetFaultOrigin()
{
  return safetyFaultOrigin;
}

const char* safetyGetFaultOriginText()
{
  switch (safetyFaultOrigin)
  {
    case SAFETY_ORIGIN_SD:      return "SD";
    case SAFETY_ORIGIN_WEB:     return "WEB";
    case SAFETY_ORIGIN_ADC:     return "ADC";
    case SAFETY_ORIGIN_TFT:     return "TFT";
    case SAFETY_ORIGIN_SER:     return "SER";
    case SAFETY_ORIGIN_LOOP:    return "LOOP";
    case SAFETY_ORIGIN_PELTIER: return "PELT";
    default:                    return "";
  }
}

const char* safetyGetStopStatusText()
{
  if (!safetyFaultActive)
  {
    return "OK";
  }

  switch (safetyFaultOrigin)
  {
    case SAFETY_ORIGIN_SD:      return "STOP SD";
    case SAFETY_ORIGIN_WEB:     return "STOP WEB";
    case SAFETY_ORIGIN_ADC:     return "STOP ADC";
    case SAFETY_ORIGIN_TFT:     return "STOP TFT";
    case SAFETY_ORIGIN_SER:     return "STOP SER";
    case SAFETY_ORIGIN_LOOP:    return "STOP LOOP";
    case SAFETY_ORIGIN_PELTIER: return "STOP PELT";
    default:                    return "STOP";
  }
}

uint32_t safetyGetEventCount()
{
  noInterrupts();
  uint32_t count = safetyEventCount;
  interrupts();
  return count;
}

bool safetyGetTriggerEpoch(uint32_t* epochOut)
{
  noInterrupts();
  bool valid = safetyTriggerEpochValid;
  uint32_t epoch = safetyTriggerEpoch;
  interrupts();

  if (!valid)
  {
    return false;
  }

  if (epochOut) *epochOut = epoch;
  return true;
}


uint32_t safetyGetTimeoutLoopAgeMs()
{
  return safetyTimeoutLoopAgeMs;
}

uint8_t safetyGetTimeoutEthStage()
{
  return safetyTimeoutEthStage;
}

uint8_t safetyGetTimeoutEthRequest()
{
  return safetyTimeoutEthRequest;
}

uint32_t safetyGetTimeoutEthElapsedMs()
{
  return safetyTimeoutEthElapsedMs;
}

uint32_t safetyGetTimeoutEthLastRequestUs()
{
  return safetyTimeoutEthLastRequestUs;
}

uint32_t safetyGetTimeoutEthMaxRequestUs()
{
  return safetyTimeoutEthMaxRequestUs;
}

uint8_t safetyGetTimeoutEthMaxRequestCode()
{
  return safetyTimeoutEthMaxRequestCode;
}

uint32_t safetyGetTimeoutEthMaxWriteUs()
{
  return safetyTimeoutEthMaxWriteUs;
}

uint8_t safetyGetTimeoutSdStage()
{
  return safetyTimeoutSdStage;
}

uint32_t safetyGetTimeoutSdElapsedMs()
{
  return safetyTimeoutSdElapsedMs;
}

uint8_t safetyGetTimeoutSdLastStage()
{
  return safetyTimeoutSdLastStage;
}

uint32_t safetyGetTimeoutSdLastUs()
{
  return safetyTimeoutSdLastUs;
}

uint8_t safetyGetTimeoutSdMaxStage()
{
  return safetyTimeoutSdMaxStage;
}

uint32_t safetyGetTimeoutSdMaxUs()
{
  return safetyTimeoutSdMaxUs;
}

const char* safetyGetFaultText()
{
  if (safetyFaultReason == SAFETY_REASON_TIMEOUT)
  {
    switch (safetyFaultOrigin)
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

  switch (safetyFaultReason)
  {
    case SAFETY_REASON_DRV8873:
      return "DRV8873 nFAULT";
    case SAFETY_REASON_DRV8873_SPI:
      return "DRV8873 SPI";
    default:
      return "Safety";
  }
}

void safetyTriggerDrv8873Fault()
{
  if (!safetyEnabled)
  {
    return;
  }

  uint32_t triggerMs = millis();
  noInterrupts();
  if (!safetyFaultActive)
  {
    safetyFaultReason = SAFETY_REASON_DRV8873;
    safetyFaultOrigin = SAFETY_ORIGIN_PELTIER;
    safetyTriggerMs = triggerMs;
    safetyEventCount++;
    safetyTriggerTimePending = true;
    safetyTriggerEpochValid = false;
  }
  safetyFaultActive = true;
  safetyPeltierCutDone = true;
  interrupts();
  safetyCutPeltierNow();
}

void safetyTriggerDrv8873SpiFault()
{
  if (!safetyEnabled)
  {
    return;
  }

  uint32_t triggerMs = millis();
  noInterrupts();
  if (!safetyFaultActive)
  {
    safetyFaultReason = SAFETY_REASON_DRV8873_SPI;
    safetyFaultOrigin = SAFETY_ORIGIN_PELTIER;
    safetyTriggerMs = triggerMs;
    safetyEventCount++;
    safetyTriggerTimePending = true;
    safetyTriggerEpochValid = false;
  }
  safetyFaultActive = true;
  safetyPeltierCutDone = true;
  interrupts();
  safetyCutPeltierNow();
}

static void safetyCaptureTriggerDateTime()
{
  noInterrupts();
  bool pending = safetyTriggerTimePending;
  uint32_t triggerMs = safetyTriggerMs;
  interrupts();

  if (!pending)
  {
    return;
  }

  time_t currentTime = now();
  if (year(currentTime) >= 2020 && year(currentTime) <= 2099)
  {
    uint32_t elapsedMs = (uint32_t)(millis() - triggerMs);
    uint32_t elapsedSeconds = (elapsedMs + 500UL) / 1000UL;
    time_t estimatedTriggerTime = currentTime;
    if ((uint32_t)estimatedTriggerTime >= elapsedSeconds)
    {
      estimatedTriggerTime -= elapsedSeconds;
    }

    noInterrupts();
    if (safetyTriggerTimePending && safetyTriggerMs == triggerMs)
    {
      safetyTriggerEpoch = (uint32_t)estimatedTriggerTime;
      safetyTriggerEpochValid = true;
      safetyTriggerTimePending = false;
    }
    interrupts();
  }
}

void safetyTask()
{
  static bool lastPrintedFault = false;

  safetyCaptureTriggerDateTime();

  if (!safetyFaultActive)
  {
    lastPrintedFault = false;
    return;
  }

  // Auch ausserhalb des Timers sicher auf HIGH/HIGH halten.
  safetyCutPeltierNow();

  // Alarmrelais bei Safety sofort setzen. Ruecknahme erfolgt zentral in
  // alarmTask(), damit ein paralleler Grenzwertalarm das Relais halten kann.
  digitalWrite(pinRelay, HIGH);

  if (!lastPrintedFault)
  {
    lastPrintedFault = true;
    Serial.print("SAFETY: ");
    Serial.print(safetyGetFaultText());
    Serial.print(" [");
    Serial.print(safetyGetStopStatusText());
    Serial.println("] - H-Bruecke AUS");

    if (safetyFaultReason == SAFETY_REASON_TIMEOUT)
    {
      Serial.print("  LoopAge_ms=");
      Serial.print(safetyGetTimeoutLoopAgeMs());
      Serial.print(" ETH_Stage=");
      Serial.print(ethernetDiagStageText(safetyGetTimeoutEthStage()));
      Serial.print(" ETH_Request=");
      Serial.print(ethernetDiagRequestText(safetyGetTimeoutEthRequest()));
      Serial.print(" ETH_Elapsed_ms=");
      Serial.print(safetyGetTimeoutEthElapsedMs());
      Serial.print(" ETH_Last_us=");
      Serial.print(safetyGetTimeoutEthLastRequestUs());
      Serial.print(" ETH_Max_us=");
      Serial.print(safetyGetTimeoutEthMaxRequestUs());
      Serial.print(" ETH_Max_Request=");
      Serial.print(ethernetDiagRequestText(safetyGetTimeoutEthMaxRequestCode()));
      Serial.print(" ETH_MaxWrite_us=");
      Serial.print(safetyGetTimeoutEthMaxWriteUs());
      Serial.print(" SD_Stage=");
      Serial.print(sdStorageDiagStageText(safetyGetTimeoutSdStage()));
      Serial.print(" SD_Elapsed_ms=");
      Serial.print(safetyGetTimeoutSdElapsedMs());
      Serial.print(" SD_Last=");
      Serial.print(sdStorageDiagStageText(safetyGetTimeoutSdLastStage()));
      Serial.print('/');
      Serial.print(safetyGetTimeoutSdLastUs());
      Serial.print("us SD_Max=");
      Serial.print(sdStorageDiagStageText(safetyGetTimeoutSdMaxStage()));
      Serial.print('/');
      Serial.print(safetyGetTimeoutSdMaxUs());
      Serial.println("us");
    }
  }
}

void safetyResetFault()
{
  // Fuer spaetere Quittierung im Menue vorbereitet.
  safetyCutPeltierNow();

  safetyLastFeedMs     = millis();
  safetyFaultActive    = false;
  safetyPeltierCutDone = false;
  safetyFaultReason    = SAFETY_REASON_NONE;
  safetyFaultOrigin    = SAFETY_ORIGIN_NONE;
  safetyTriggerMs      = 0;
  safetyTriggerTimePending = false;
  safetyTriggerEpoch   = 0;
  safetyTriggerEpochValid = false;
}
