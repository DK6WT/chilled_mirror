/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TP-3000.ino
 * Zweck: Hauptprogramm, Initialisierung, Pinbelegung und zentraler Scheduler.
 *
 * Abgeleitet aus: LJ2000M_2.06c_GSL1680.ino
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2015 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

/*

    Teensy T4.0 Pinout with RA8875 SPI TFT display

                               pin numbers     I/O       pin numbers
                                       \     /       \      /
                                        -------------------
                                       1| GND        Vin   |28  +5VDC
                           RX1  2| 0         Agnd   |27
                              \   TX1  3| 1         3.3V   |26
                                       4| 2         23/A9  |25
                                       5| 3         22/A8  |24
                                       6| 4         21/A7  |23
                                       7| 5         20/A6  |22
                                       8| 6         19/A5  |21  SCL0    to external AD7991 bus speed must be set in WireIMXRT.cpp a 2MHz (not in standard code) for 50us SAMPLE_TIMER
                                       9| 7         18/A4  |20  SDA0    to external AD7991
                            TC_INT    10| 8         17/A3  |19  SDA1    to touchscreen RA8875 ***rename in GSL1680.cpp Wire to Wire1**
                            Counter   11| 9         16/A2  |18  SCL1    to touchscreen RA8875
                            tft_CS    12| 10        15/A1  |17  frei / alter Coupler-Pin
                            tft_MOSI  13| 11        14/A0  |16  Alarm-Ausgang / Summer
                            tft_MISO  14| 12        13/LED |15  tft_SCLK
                                        --------------------

   RA8875 SPI TFT connections,back side left upcorner
  ---------------------------------------------------|
            -    o                                   |
            o    o                                   |
            o    o       NOT USED                    |
            o    o                                   |
            o    o                                   |
            o    o                                   |
                                                     |
                                                     |
    GND   1 -    o 2   GND                           |
    VCC     o    o     VCC                           |
    CS      o    o     MISO                          |
    MOSI    o    o     SCLK                          |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
            o    o                                   |
    TC INT  o    o I2C SDA                           |
    I2C SCL o    o                                   |
            o    o                                   |
         39 o    o 40                                |
  ---------------------------------------------------|
 ****************************************************/
/* This must be added in WireIMXRT.cpp to get the I2C clock on 2 MHz. J.G. Holstein @ 09-04-2021

  } else if (frequency < 1000000) {
    // 400 kHz
    port->MCCR0 = LPI2C_MCCR0_CLKHI(26) | LPI2C_MCCR0_CLKLO(28) |
      LPI2C_MCCR0_DATAVD(12) | LPI2C_MCCR0_SETHOLD(18);
    port->MCFGR1 = LPI2C_MCFGR1_PRESCALE(0);
    port->MCFGR2 = LPI2C_MCFGR2_FILTSDA(2) | LPI2C_MCFGR2_FILTSCL(2) |
      LPI2C_MCFGR2_BUSIDLE(3600); // idle timeout 150 us
    port->MCFGR3 = LPI2C_MCFGR3_PINLOW(CLOCK_STRETCH_TIMEOUT * 24 / 256 + 1);
  } else {
    // 2 MHz
                port->MCCR0 = LPI2C_MCCR0_CLKHI(3) | LPI2C_MCCR0_CLKLO(4) |
                LPI2C_MCCR0_DATAVD(2) | LPI2C_MCCR0_SETHOLD(3);
                port->MCFGR1 = LPI2C_MCFGR1_PRESCALE(0);
                port->MCFGR2 = LPI2C_MCFGR2_FILTSDA(1) | LPI2C_MCFGR2_FILTSCL(1) |
                LPI2C_MCFGR2_BUSIDLE(1200); // idle timeout 50 us
                port->MCFGR3 = LPI2C_MCFGR3_PINLOW(CLOCK_STRETCH_TIMEOUT * 24 / 256 + 1);
  }
*/
/*
//#define VENDOR_ID               0x16C0
//#define PRODUCT_ID              0x0480
#define RAWHID_USAGE_PAGE       0xFFAB  // recommended: 0xFF00 to 0xFFFF
#define RAWHID_USAGE            0x0200  // recommended: 0x0100 to 0xFFFF

#define RAWHID_TX_SIZE          64      // transmit packet size
//#define RAWHID_TX_INTERVAL      2       // max # of ms between transmit packets
#define RAWHID_RX_SIZE          64      // receive packet size
//#define RAWHID_RX_INTERVAL      8       // max # of ms between receive packets
char  buffer[64];
*/
//******************************************************************************
// Teil 1: System-Pins, Libraries und explizite, speichersichere Struktur
//******************************************************************************
#include "TPsignedData.h"
#include "TPsignedCalibration.h"
#include "TPexternalCalibration.h"
#include "TPcertifiedLog.h"
#include "TPfirmwareIntegrity.h"
#include "TPsystemFirmwareContext.h"
#define VERSION TP_FIRMWARE_VERSION_STRING
#define DATE    TP_FIRMWARE_BUILD_DATE_TEXT
#define SENSOR  "STP-3000"

#include <SPI.h>
#include <SD.h>
#include <NativeEthernet.h>
#include <RA8875.h>
#include "fonts.h"
#include "earth.c"
#include <Wire.h>
#include <Metro.h>
#include <EEPROM.h>
#include "TP_T.h"
#include "TPlanguage.h"
#include "TPtft.h" 
#include "GSL1680.h"
#include "EEPROMAnything.h"
#include <TimeLib.h>
#include <Entropy.h>
#include <TP3000_uECC.h>
#include "TPsha256.h"
#if defined(__has_include)
  // Bevorzugt die im Quellpaket mitgelieferte WDT_T4-Kopie verwenden.
  // Dadurch ist der Hardware-Watchdog auch aktiv, wenn die Bibliothek nicht
  // separat in der Arduino-IDE installiert wurde.
  #if __has_include("libraries/WDT_T4/Watchdog_t4.h")
    #include "libraries/WDT_T4/Watchdog_t4.h"
    #define TP3000_HW_WATCHDOG 1
  #elif __has_include(<Watchdog_t4.h>)
    #include <Watchdog_t4.h>
    #define TP3000_HW_WATCHDOG 1
  #else
    #define TP3000_HW_WATCHDOG 0
    #warning "Watchdog_t4.h nicht gefunden - Hardware-Watchdog ist deaktiviert"
  #endif
#else
  #include <Watchdog_t4.h>
  #define TP3000_HW_WATCHDOG 1
#endif
#include "drv8873s.h"


#if TP3000_HW_WATCHDOG
static WDT_T4<WDT1> tp3000_hw_watchdog;
static bool tp3000_hw_watchdog_started = false;

static void tp3000WatchdogBegin()
{
  WDT_timings_t config;
  // Sekunden: Trigger 4 s, Reset 8 s. Waehrend setup() wird nur an klaren
  // Boot-Checkpoints gefuettert; nach setup() ausschliesslich am Ende einer
  // komplett durchlaufenen Hauptloop. Blockiert SD/Ethernet/Download laenger,
  // startet der Teensy automatisch neu.
  config.trigger = 4.0;
  config.timeout = 8.0;
  config.window  = 0.0;
  config.callback = nullptr;
  tp3000_hw_watchdog.begin(config);
  tp3000_hw_watchdog_started = true;
}

static inline void tp3000WatchdogFeed()
{
  if (!tp3000_hw_watchdog_started) return;
  tp3000_hw_watchdog.feed();
}

// Darf waehrend des Bootvorgangs und aus blockweise arbeitenden Startpruefungen
// aufgerufen werden. Im normalen Messbetrieb wird der Watchdog weiterhin nur
// am Ende einer vollstaendig durchlaufenen Hauptloop gefuettert.
void tpFirmwareIntegrityBootService(void)
{
  if (!tp3000_hw_watchdog_started) return;
  tp3000_hw_watchdog.feed();
}
#else
static inline void tp3000WatchdogBegin() {}
static inline void tp3000WatchdogFeed() {}
void tpFirmwareIntegrityBootService(void) {}
#endif

// Hauptloop-Watchdog-Gate:
// Der Hardware-Watchdog darf nur nach einer vollstaendig durchlaufenen
// Hauptloop gefuettert werden. Wird die Loop nicht sauber verlassen, wird
// nicht mehr gefuettert und der Watchdog greift als letzte Notbremse.
static volatile uint8_t tp3000_loop_guard_depth = 0;
static volatile uint32_t tp3000_loop_guard_faults = 0;
static uint32_t tp3000_loop_guard_start_ms = 0;
static uint32_t tp3000_loop_guard_last_ms = 0;
static uint32_t tp3000_loop_guard_max_ms = 0;

static bool tp3000LoopGuardEnter()
{
  if (tp3000_loop_guard_depth != 0U)
  {
    tp3000_loop_guard_faults++;
    return false;
  }

  tp3000_loop_guard_depth = 1U;
  tp3000_loop_guard_start_ms = millis();
  return true;
}

static bool tp3000LoopGuardLeave()
{
  if (tp3000_loop_guard_depth != 1U)
  {
    tp3000_loop_guard_faults++;
    tp3000_loop_guard_depth = 0U;
    return false;
  }

  uint32_t dt = (uint32_t)(millis() - tp3000_loop_guard_start_ms);
  tp3000_loop_guard_last_ms = dt;
  if (dt > tp3000_loop_guard_max_ms) tp3000_loop_guard_max_ms = dt;
  tp3000_loop_guard_depth = 0U;
  return true;
}

uint32_t tp3000LoopGuardFaultCount() { return tp3000_loop_guard_faults; }
uint32_t tp3000LoopGuardLastMs() { return tp3000_loop_guard_last_ms; }
uint32_t tp3000LoopGuardMaxMs() { return tp3000_loop_guard_max_ms; }

// Schnittstellen-Menue: Funktionen liegen in TPmenu_Interface.ino.
// Arduino erzeugt nicht in allen Faellen rechtzeitig Prototypen fuer
// Aufrufe aus der Hauptdatei heraus, daher hier explizit deklarieren.
void interfaceConfigLoad(void);
void interfaceApplySerialBaud(void);
void sdLogBegin(void);
void sdLogTask(void);
bool sdLogReadyForLogging(void);
bool sdLogEnsureReadyForAccess(void);
void ledAdaptationBegin(void);
void ledAdaptationTask(void);
bool interfaceSdLoggingEnabled(void);
void interfaceSdLoggingForceOff(void);
void taupunktOffsetEnsureLoaded(void);
void alarmConfigLoad(void);
void alarmTask(void);
bool alarmTouchAcknowledge(uint16_t x, uint16_t y);
void menuBlockInputUntilTouchRelease(void);

// System-Konfigurationen
#define PUSHBUTTON_ENABLED   1 
#define TOUCHSCREEN_ENABLED  1 
#define TC_INT               8
#define WAKE                 255
#define I2C                  Wire


// Display SPI-Pins (RA8875)
#define RA8875_RESET        14 
#define RA8875_CS           10

// ADS1263 Metrologie Hardware-Pins
extern const byte pincs1 = 9; 
extern const byte pinRstAd = 0; 
extern const byte pinStart = 6; 
extern const byte pinDrdy = 2; 
extern const byte pinCsLed = 3; 
extern const byte pinLedOff = 7;   // LED_OFF: HIGH = LED aus, LOW = LED-Regelung aktiv

// Erweiterte System-I/O Pins (Eingänge)
extern const byte pinWaitDisp = 23;
extern const byte pinIntDisp = 40;
extern const byte pinIntExp = 32;
extern const byte pinIntClk = 4;
extern const byte pinNfault = 41;
extern const byte pinIntTouch = 8;
extern const byte pinFanTacho = 21;   // FAN_SPEED / Tacho-Signal Sensorkopf

// Erweiterte System-I/O Pins (Ausgänge)
extern const byte pinNscs = 20;
extern const byte pinCsDisable = 39;
extern const byte pinPwmFan = 5; 
extern const byte pinCurrRev = 22;
extern const byte pinAlarm = 30; 
extern const byte pinRelay = 31;
extern const byte pinPh = 36; 
extern const byte pinEn = 37;

// =============================================================================
// SPEICHERSICHERE STRUKTUR-INITIALISIERUNG
// =============================================================================
var_t R = {
  .fühler_cal = {
    { .db10m = 500, .Fwd = 100.0, .Rev = 100.0 }, // reserved/deprecated: alter R0-Platzhalter
    { .db10m = 0,   .Fwd = 100.0, .Rev = 100.0 }  // reserved/deprecated: alter R0-Platzhalter
  },
  .skalierung_frei = 100,     // reserved
  .fan_percent = 75,          // STP-3001 Werksprofil
  .optik_sollwert = 985,      // 98.5 %
  .luefter_start_temp = 35,   // reserved
  .usb_report_cont = 0,
  .usb_report_type = 1,
  .pid_kp = 120,             // STP-3001 Werksprofil
  .pid_ki_alt = 0,           // reserved
  .freiheiz_ziel_temp = {76, 10, 20}, // [0] aktiv, [1]/[2] reserved
  .geraete_name = DEVICE_SERIAL_DEFAULT,
  .pid_ki = 1.5,             // STP-3001 Werksprofil
  .pid_kd = 5.0,             // STP-3001 Werksprofil
  .bildschirm_modus = 0,     // reserved
  .modus_default = 0,        // reserved
  .h_bruecke_totzeit = 15,  // STP-3001 Werksprofil
  .regler_intervall_ms = 250, // STP-3001 Werksprofil
  .akku_abschalt_spannung = 600, // reserved
  .display_konfig = { .tft_on = false, .tft_backlight = 7 },
  .head_type = HEAD_TYPE_DEFAULT,
  .head_serial = HEAD_SERIAL_DEFAULT,
  .optik_autocal_interval_index = OPTIK_AUTOCAL_INTERVAL_DEFAULT_INDEX
};

// Separat vom historischen var_t-Layout gespeichert. Die A/B-Regelparameter-
// Slots verwenden dafuer ein bisher reserviertes Byte.
uint8_t led_autoadaptation_mode = LED_AUTOADAPT_DEFAULT;
uint8_t ui_language = LANG_DE;

// =========================================================================
// LOOP DEBUG-ANZEIGE (optional im Hauptdisplay)
// =========================================================================
// Die Messung laeuft immer mit sehr kleinem Overhead. Angezeigt wird sie nur,
// wenn der Schalter unter Setup -> Anzeige aktiviert ist.

bool loop_debug_display_enabled = false;
uint8_t main_screen_layout = MAIN_SCREEN_LAYOUT_DEFAULT;

// UTC-Offset der lokalen Geräte-RTC in Minuten (lokal = UTC + Offset).
// Beispiel Deutschland Sommerzeit: +120. Der Offset wird beim Web-Zeitsync
// vom Browser geliefert und im vorhandenen Device/UI-EEPROM-Block abgelegt.
static int16_t tp_utc_offset_minutes = 0;
static bool tp_utc_offset_valid = false;
uint16_t loop_debug_hz = 0;
uint32_t loop_debug_max_us = 0;
uint32_t loop_debug_peak10_us = 0;     // hoechster Loop-Maxwert im laufenden 10-s-Fenster

static uint32_t loop_debug_count = 0;
static uint32_t loop_debug_last_start_us = 0;
static uint32_t loop_debug_max_us_work = 0;
static uint32_t loop_debug_peak10_work_us = 0;
static uint8_t  loop_debug_peak10_seconds = 0;
static uint32_t loop_debug_last_report_ms = 0;

static const uint8_t LOOP_DEBUG_EEPROM_MAGIC = 0xA6;

static int loopDebugEepromMagicAddr()
{
  return 2310;
}

[[maybe_unused]] static int loopDebugEepromValueAddr()
{
  return loopDebugEepromMagicAddr() + 1;
}

void loopDebugDisplayLoad()
{
  tpDeviceUiConfigLoad();
}


void loopDebugDisplaySave()
{
  tpDeviceUiConfigSave();
}


bool loopDebugDisplayGetEnabled()
{
  return loop_debug_display_enabled;
}

void loopDebugDisplaySetEnabled(bool enabled)
{
  loop_debug_display_enabled = enabled;
  loopDebugDisplaySave();
}

// =========================================================================
// ALTERNATIVER HAUPTSCREEN (Device-/UI-EEPROM-Block)
// =========================================================================
// Der Modus bleibt bewusst ausserhalb von var_t, damit bestehende
// Regelparameter-Bloecke und Kopfjustierungen unveraendert bleiben.

static uint8_t mainScreenLayoutSanitize(uint8_t layout)
{
  return (layout <= MAIN_SCREEN_LAYOUT_MAX) ? layout : MAIN_SCREEN_LAYOUT_DEFAULT;
}

uint8_t mainScreenLayoutGet()
{
  return mainScreenLayoutSanitize(main_screen_layout);
}

void mainScreenLayoutSet(uint8_t layout)
{
  main_screen_layout = mainScreenLayoutSanitize(layout);
  tpDeviceUiConfigSave();
}

void mainScreenLayoutLoad()
{
  main_screen_layout = mainScreenLayoutSanitize(main_screen_layout);
}

// =========================================================================
// UTC-ZEITBASIS FUER SIGNIERTE DATEN
// =========================================================================
// Die Geräteanzeige und RTC bleiben in lokaler Zeit. Nur kryptografische
// Zeitstempel werden mit dem gespeicherten UTC-Offset eindeutig umgerechnet.

static bool tpUtcOffsetMinutesPlausible(int16_t minutes)
{
  return minutes >= -14 * 60 && minutes <= 14 * 60;
}

bool tpUtcOffsetValid(void)
{
  return tp_utc_offset_valid && tpUtcOffsetMinutesPlausible(tp_utc_offset_minutes);
}

int16_t tpUtcOffsetMinutesGet(void)
{
  return tpUtcOffsetValid() ? tp_utc_offset_minutes : 0;
}

bool tpUtcOffsetSetMinutes(int16_t minutes)
{
  if (!tpUtcOffsetMinutesPlausible(minutes)) return false;
  tp_utc_offset_minutes = minutes;
  tp_utc_offset_valid = true;
  tpDeviceUiConfigSave();
  return true;
}

int64_t tpCurrentUtcUnixTime(void)
{
  const time_t localTime = now();
  if (localTime < (time_t)1577836800 || !tpUtcOffsetValid()) return 0;
  return (int64_t)localTime - (int64_t)tp_utc_offset_minutes * 60LL;
}

// =========================================================================
// ADC/PT100-MESSFILTER (separater EEPROM-Block)
// =========================================================================
// Der Modus bleibt bewusst ausserhalb von var_t, damit die bestehenden
// Kalibrier-/Interface-EEPROM-Bloecke nicht durch eine Strukturvergroesserung
// verschoben werden.
static const uint8_t ADC_FILTER_EEPROM_MAGIC = 0xC7;
static uint8_t adc_filter_mode = ADC_MEAS_FILTER_DEFAULT;

static int adcFilterEepromMagicAddr()
{
  return 2320;
}

[[maybe_unused]] static int adcFilterEepromValueAddr()
{
  return adcFilterEepromMagicAddr() + 1;
}

static uint8_t adcFilterSanitize(uint8_t mode)
{
  return (mode <= ADC_MEAS_FILTER_MAX) ? mode : ADC_MEAS_FILTER_DEFAULT;
}

uint8_t adcFilterModeGet()
{
  return adcFilterSanitize(adc_filter_mode);
}

void adcFilterModeSet(uint8_t mode)
{
  adc_filter_mode = adcFilterSanitize(mode);
  tpMainConfigSave();
}


void adcFilterModeLoad()
{
  adc_filter_mode = adcFilterSanitize(adc_filter_mode);
}


// =========================================================================
// ADC1 / ADS1263-SFOCAL-MODUS (separater EEPROM-Block)
// =========================================================================
// Der Modus bleibt bewusst ausserhalb von var_t, damit bestehende EEPROM-
// Bloecke stabil bleiben. Default: synchron mit Optik-/LED-Auto-Cal.
static const uint8_t ADC1_SFOCAL_EEPROM_MAGIC = 0x5F;
static uint8_t adc1_sfocal_mode = ADC1_SFOCAL_DEFAULT;

static int adc1SfocalEepromMagicAddr()
{
  return 2330;
}

[[maybe_unused]] static int adc1SfocalEepromValueAddr()
{
  return adc1SfocalEepromMagicAddr() + 1;
}

static uint8_t adc1SfocalSanitize(uint8_t mode)
{
  return (mode <= ADC1_SFOCAL_MAX) ? mode : ADC1_SFOCAL_DEFAULT;
}

uint8_t adc1SfocalModeGet()
{
  return adc1SfocalSanitize(adc1_sfocal_mode);
}

void adc1SfocalModeSet(uint8_t mode)
{
  adc1_sfocal_mode = adc1SfocalSanitize(mode);
  tpMainConfigSave();
}


void adc1SfocalModeLoad()
{
  adc1_sfocal_mode = adc1SfocalSanitize(adc1_sfocal_mode);
}


// =========================================================================
// Kopfbezogenes Peltier-Stromlimit (separater EEPROM-Block)
// =========================================================================
static const uint8_t PELTIER_LIMIT_EEPROM_MAGIC = 0xA5;
static uint16_t peltier_current_limit_ma = PELTIER_CURRENT_LIMIT_DEFAULT_MA;

static int peltierCurrentLimitEepromMagicAddr()
{
  return 2340;
}

[[maybe_unused]] static int peltierCurrentLimitEepromValueAddr()
{
  return peltierCurrentLimitEepromMagicAddr() + 1;
}

uint16_t peltierCurrentLimitSanitizeMa(uint16_t ma)
{
  if (ma < PELTIER_CURRENT_LIMIT_MIN_MA) return PELTIER_CURRENT_LIMIT_DEFAULT_MA;
  if (ma > PELTIER_CURRENT_LIMIT_MAX_MA) return PELTIER_CURRENT_LIMIT_DEFAULT_MA;
  return ma;
}

uint16_t peltierCurrentLimitGetMa(void)
{
  return peltierCurrentLimitSanitizeMa(peltier_current_limit_ma);
}

void peltierCurrentLimitSetMa(uint16_t ma)
{
  peltier_current_limit_ma = peltierCurrentLimitSanitizeMa(ma);
  tpMainConfigSave();
}


void peltierCurrentLimitLoad(void)
{
  peltier_current_limit_ma = peltierCurrentLimitSanitizeMa(peltier_current_limit_ma);
}


uint32_t adc1SfocalIntervalMs()
{
  switch (adc1SfocalModeGet())
  {
    case ADC1_SFOCAL_10MIN: return 10UL * 60UL * 1000UL;
    case ADC1_SFOCAL_30MIN: return 30UL * 60UL * 1000UL;
    case ADC1_SFOCAL_60MIN: return 60UL * 60UL * 1000UL;
    default:                return 0UL;
  }
}

void loopDebugGetMetrics(uint16_t* hz, uint32_t* max_us, bool* enabled)
{
  if (hz != nullptr) *hz = loop_debug_hz;
  if (max_us != nullptr) *max_us = loop_debug_max_us;
  if (enabled != nullptr) *enabled = loop_debug_display_enabled;
}

void loopDebugGetMetricsExt(uint16_t* hz, uint32_t* max_us, uint32_t* peak10_us, bool* enabled)
{
  if (hz != nullptr) *hz = loop_debug_hz;
  if (max_us != nullptr) *max_us = loop_debug_max_us;
  if (peak10_us != nullptr) *peak10_us = loop_debug_peak10_us;
  if (enabled != nullptr) *enabled = loop_debug_display_enabled;
}

static void loopDebugTick()
{
  uint32_t nowUs = micros();

  if (loop_debug_last_start_us != 0)
  {
    uint32_t dt = nowUs - loop_debug_last_start_us;
    if (dt > loop_debug_max_us_work)
    {
      loop_debug_max_us_work = dt;
    }
  }

  loop_debug_last_start_us = nowUs;
  loop_debug_count++;

  uint32_t nowMs = millis();
  if (loop_debug_last_report_ms == 0)
  {
    loop_debug_last_report_ms = nowMs;
  }

  if ((uint32_t)(nowMs - loop_debug_last_report_ms) >= 1000UL)
  {
    loop_debug_hz = (loop_debug_count > 65535UL) ? 65535U : (uint16_t)loop_debug_count;
    loop_debug_max_us = loop_debug_max_us_work;

    // 10-s-Peak: Nicht seit Reset festhalten, sondern alle 10 Sekunden neu beginnen.
    // So stoeren Boot-/Setup-Spitzen nur kurz und bleiben nicht ewig stehen.
    if (loop_debug_max_us_work > loop_debug_peak10_work_us)
    {
      loop_debug_peak10_work_us = loop_debug_max_us_work;
    }

    loop_debug_peak10_seconds++;
    if (loop_debug_peak10_seconds >= 10)
    {
      loop_debug_peak10_us = loop_debug_peak10_work_us;
      loop_debug_peak10_work_us = 0;
      loop_debug_peak10_seconds = 0;
    }
    else
    {
      // Waerend des laufenden 10-s-Fensters schon den aktuellen Peak anzeigen.
      loop_debug_peak10_us = loop_debug_peak10_work_us;
    }

    loop_debug_count = 0;
    loop_debug_max_us_work = 0;
    loop_debug_last_report_ms = nowMs;
  }
}


// Separater EEPROM-Block fuer Pt100-R0-Kalibrierung
extern void pt100R0EnsureLoaded();
flags flag;
bool frisch_geoeffnet = true;
int8_t current_selection = 0; 
uint32_t menu_global_debounce = 0; 

uint16_t menu_level = 0;          
bool touch_scroll_aktiv = false;  
//******************************************************************************
// Teil 2: Globale Variablen, UI-Objekte und korrigierte Prototypen
//******************************************************************************

// =============================================================================
// ASYNCHRONES MESSKARUSSELL (ADS1263 Interrupt-Ebene & Rohwerte)
// =============================================================================
volatile int32_t adcRawPt100_1 = 0; 
volatile int32_t adcRawPt100_2 = 0; 
volatile int32_t adcRawRef120R = 0; 
volatile int32_t adcRawRef100R = 0; 
volatile int32_t adcRawPhotodiode = 0; 
volatile int32_t adcRawPeltierStrom = 0; 

// =============================================================================
// REGELUNG, PELTIER & THERMISCHE HILFSWERTE
// =============================================================================
float peltierSollWert = 0.0; 
unsigned long lastDitherTime = 0;
uint8_t ditherCycle = 0;

// Klimavariablen aus dem Regelungs-/Peripherietab
extern float tempSpiegel;
extern float tempUmgebung;
extern float relativeFeuchte;
extern float präziserTaupunkt;

extern float baroDruckHPa;
extern uint8_t rtcSekunde, rtcMinute, rtcStunde;
extern uint8_t rtcTag, rtcMonat, rtcJahr;

extern void pt100Cal2Disable(uint8_t sensor);

double amp_avg = 0.0;

// =============================================================================
// SYSTEM-STATUS, FLAGS & TIMING (Metro/Chrono)
// =============================================================================
#ifndef ENC_RESDIVIDE
#define ENC_RESDIVIDE 4
#endif

Metro pollMetro = Metro(100); 
Metro slowMetro = Metro(100); 

uint8_t mode_display = 0;
int FirstBut = 0;
int LastBut = 3;
uint16_t Menu_exit_timer = 0;
bool refresh = false;

//Sensor Heads
const char* tp3000HeadName(uint8_t head)
{
  switch (head)
  {
    case HEAD_TYPE_STP3001: return "STP-3001";
    case HEAD_TYPE_STP3002: return "STP-3002";
    case HEAD_TYPE_STP3003: return "STP-3003";
    case HEAD_TYPE_STP3004: return "STP-3004";
    default:                return "STP-3001";
  }
}

// Geraete-Seriennummer: exakt 5 Ziffern. Alte Textwerte oder EEPROM-Muell
// werden beim Start sicher auf 00000 zurueckgesetzt.
bool FLASHMEM deviceSerialNormalize(char* text, size_t textSize)
{
  if (text == nullptr || textSize < (DEVICE_SERIAL_DIGITS + 1U)) return false;

  bool valid = true;
  for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
  {
    const char c = text[i];
    if (c < '0' || c > '9')
    {
      valid = false;
      break;
    }
  }

  if (text[DEVICE_SERIAL_DIGITS] != '\0') valid = false;

  if (valid) return false;

  strncpy(text, DEVICE_SERIAL_DEFAULT, textSize - 1U);
  text[textSize - 1U] = '\0';
  return true;
}

bool FLASHMEM deviceSerialSet(const char* text)
{
  if (text == nullptr) return false;

  char candidate[DEVICE_SERIAL_DIGITS + 1U];
  for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
  {
    const char c = text[i];
    if (c < '0' || c > '9') return false;
    candidate[i] = c;
  }

  if (text[DEVICE_SERIAL_DIGITS] != '\0' &&
      text[DEVICE_SERIAL_DIGITS] != '\r' &&
      text[DEVICE_SERIAL_DIGITS] != '\n' &&
      text[DEVICE_SERIAL_DIGITS] != ' ' &&
      text[DEVICE_SERIAL_DIGITS] != '\t')
  {
    return false;
  }

  candidate[DEVICE_SERIAL_DIGITS] = '\0';

  // Nach erfolgreicher Provisionierung ist ausschliesslich die im
  // Root-signierten Geraetezertifikat enthaltene SN verbindlich. Ein
  // erneutes Setzen exakt derselben SN gilt als erfolgreich, damit
  // bestehende Speicher-/Backup-Abläufe nicht unnoetig fehlschlagen.
  if (deviceIdentityCertificateValid())
  {
    const char* certified = deviceIdentityCertifiedSerial();
    return (certified != nullptr && strcmp(candidate, certified) == 0);
  }

  strncpy(R.geraete_name, candidate, sizeof(R.geraete_name) - 1U);
  R.geraete_name[sizeof(R.geraete_name) - 1U] = '\0';
  return true;
}

const char* FLASHMEM deviceSerialGet(void)
{
  if (deviceIdentityCertificateValid())
  {
    const char* certified = deviceIdentityCertifiedSerial();
    if (certified != nullptr && certified[0] != '\0') return certified;
  }

  deviceSerialNormalize(R.geraete_name, sizeof(R.geraete_name));
  return R.geraete_name;
}

static char tp3000_head_type_text[HEAD_TYPE_TEXT_LEN + 1U] = HEAD_TYPE_DEFAULT_TEXT;

static void FLASHMEM headTypeTextSave()
{
  // Ab EEPROM V11 liegt der Kopftyp-Text nur noch im robusten Main-A/B-Block.
  tpMainConfigSave();
}

[[maybe_unused]] static void FLASHMEM headTypeTextLoad()
{
  // Der alte Einzelblock wird nicht mehr gelesen; tpMainConfigLoad() befuellt den Text.
  headTypeTextNormalize(tp3000_head_type_text, sizeof(tp3000_head_type_text));
  const int8_t profile = headTypeProfileFromText(tp3000_head_type_text);
  if (profile >= 0) R.head_type = (uint8_t)profile;
}

static bool FLASHMEM headTypeFourDigits(const char* s)
{
  if (s == nullptr) return false;
  for (uint8_t i = 0; i < 4; i++)
  {
    if (s[i] < '0' || s[i] > '9') return false;
  }
  return true;
}

bool FLASHMEM headTypeTextFromNumber(uint16_t number, char* out, size_t outSize)
{
  if (out == nullptr || outSize < (HEAD_TYPE_TEXT_LEN + 1U)) return false;
  if (number > HEAD_TYPE_NUMBER_MAX) return false;
  snprintf(out, outSize, "STP-%04u", (unsigned)number);
  return true;
}

bool FLASHMEM headTypeTextValid(const char* text)
{
  if (text == nullptr) return false;
  if (strncmp(text, "STP-", 4) != 0) return false;
  if (!headTypeFourDigits(text + 4)) return false;
  const char tail = text[HEAD_TYPE_TEXT_LEN];
  return (tail == '\0' || tail == '\r' || tail == '\n' || tail == ' ' || tail == '\t');
}

uint16_t FLASHMEM headTypeTextNumber(const char* text)
{
  if (!headTypeTextValid(text)) return 3001U;
  return (uint16_t)((text[4] - '0') * 1000U +
                    (text[5] - '0') * 100U +
                    (text[6] - '0') * 10U +
                    (text[7] - '0'));
}

int8_t FLASHMEM headTypeProfileFromText(const char* text)
{
  if (!headTypeTextValid(text)) return -1;
  if (strncmp(text, "STP-3001", HEAD_TYPE_TEXT_LEN) == 0) return HEAD_TYPE_STP3001;
  if (strncmp(text, "STP-3002", HEAD_TYPE_TEXT_LEN) == 0) return HEAD_TYPE_STP3002;
  if (strncmp(text, "STP-3003", HEAD_TYPE_TEXT_LEN) == 0) return HEAD_TYPE_STP3003;
  if (strncmp(text, "STP-3004", HEAD_TYPE_TEXT_LEN) == 0) return HEAD_TYPE_STP3004;
  return -1;
}

bool FLASHMEM headTypeTextNormalize(char* text, size_t textSize)
{
  if (text == nullptr || textSize < (HEAD_TYPE_TEXT_LEN + 1U)) return false;

  char candidate[HEAD_TYPE_TEXT_LEN + 1U];
  bool valid = false;

  if (headTypeTextValid(text))
  {
    memcpy(candidate, text, HEAD_TYPE_TEXT_LEN);
    candidate[HEAD_TYPE_TEXT_LEN] = '\0';
    valid = true;
  }
  else if (strncmp(text, "STP", 3) == 0 && headTypeFourDigits(text + 3) &&
           (text[7] == '\0' || text[7] == '\r' || text[7] == '\n' || text[7] == ' ' || text[7] == '\t'))
  {
    memcpy(candidate, "STP-", 4);
    memcpy(candidate + 4, text + 3, 4);
    candidate[HEAD_TYPE_TEXT_LEN] = '\0';
    valid = true;
  }

  if (!valid)
  {
    strncpy(text, HEAD_TYPE_DEFAULT_TEXT, textSize - 1U);
    text[textSize - 1U] = '\0';
    return true;
  }

  if (strncmp(text, candidate, HEAD_TYPE_TEXT_LEN + 1U) != 0)
  {
    strncpy(text, candidate, textSize - 1U);
    text[textSize - 1U] = '\0';
    return true;
  }

  return false;
}

bool FLASHMEM headTypeTextSet(const char* text)
{
  if (text == nullptr) return false;

  const bool oldStyle = (strncmp(text, "STP", 3) == 0 &&
                         text[3] >= '0' && text[3] <= '9' &&
                         text[4] >= '0' && text[4] <= '9' &&
                         text[5] >= '0' && text[5] <= '9' &&
                         text[6] >= '0' && text[6] <= '9' &&
                         (text[7] == '\0' || text[7] == '\r' || text[7] == '\n' || text[7] == ' ' || text[7] == '\t'));
  if (!headTypeTextValid(text) && !oldStyle) return false;

  char candidate[HEAD_TYPE_TEXT_LEN + 1U];
  strncpy(candidate, text, sizeof(candidate) - 1U);
  candidate[sizeof(candidate) - 1U] = '\0';
  headTypeTextNormalize(candidate, sizeof(candidate));
  if (!headTypeTextValid(candidate)) return false;

  strncpy(tp3000_head_type_text, candidate, sizeof(tp3000_head_type_text) - 1U);
  tp3000_head_type_text[sizeof(tp3000_head_type_text) - 1U] = '\0';

  const int8_t profile = headTypeProfileFromText(candidate);
  if (profile >= 0) R.head_type = (uint8_t)profile;
  headTypeTextSave();
  return true;
}

const char* FLASHMEM headTypeTextGet(void)
{
  if (headTypeTextNormalize(tp3000_head_type_text, sizeof(tp3000_head_type_text)))
  {
    const int8_t profile = headTypeProfileFromText(tp3000_head_type_text);
    if (profile >= 0) R.head_type = (uint8_t)profile;
    headTypeTextSave();
  }
  return tp3000_head_type_text;
}



// =============================================================================
// EEPROM V12: bereinigte A/B-Konfigurationen
// =============================================================================
// Layout:
//   1536..2047 Mess-/Regelparameter A/B, je 256 Byte
//   2048..2303 Device-/UI-Settings A/B, je 128 Byte
// Der alte var_t-Dump ab Adresse 1, einzelne Magic-Bytes fuer Sprache/Debug/
// ADC/SFOCAL/Peltierlimit und der alte 0xAA-Kaltstartmarker werden nicht mehr
// als Startquelle benutzt.

static uint32_t FLASHMEM tpConfigCrcBytes(const void* data, size_t n)
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

static bool FLASHMEM tpFloatValid(float v, float minV, float maxV)
{
  return isfinite(v) && v >= minV && v <= maxV;
}

// -----------------------------------------------------------------------------
// Defaults und Plausibilisierung der RAM-Laufzeitstruktur
// -----------------------------------------------------------------------------
static void FLASHMEM tpConfigSetAllDefaults(void)
{
  R.fühler_cal[0].db10m = 500;
  R.fühler_cal[0].Fwd = 100.0;
  R.fühler_cal[0].Rev = 100.0;
  R.fühler_cal[1].db10m = 0;
  R.fühler_cal[1].Fwd = 100.0;
  R.fühler_cal[1].Rev = 100.0;

  R.skalierung_frei = 100;
  R.fan_percent = 75;
  R.optik_sollwert = 985;
  R.luefter_start_temp = 35;
  R.usb_report_cont = 0;
  R.usb_report_type = REPORT_DATA;
  R.pid_kp = 120;
  R.pid_ki_alt = 0;
  R.freiheiz_ziel_temp[0] = 76;
  R.freiheiz_ziel_temp[1] = 10;
  R.freiheiz_ziel_temp[2] = 20;
  strncpy(R.geraete_name, DEVICE_SERIAL_DEFAULT, sizeof(R.geraete_name) - 1U);
  R.geraete_name[sizeof(R.geraete_name) - 1U] = '\0';
  R.pid_ki = 1.5f;
  R.pid_kd = 5.0f;
  R.bildschirm_modus = 0;
  R.modus_default = 0;
  R.h_bruecke_totzeit = 15;
  R.regler_intervall_ms = 250;
  R.akku_abschalt_spannung = 600;
  R.display_konfig.tft_on = false;
  R.display_konfig.tft_backlight = 7;
  R.head_type = HEAD_TYPE_DEFAULT;
  R.head_serial = HEAD_SERIAL_DEFAULT;
  R.optik_autocal_interval_index = OPTIK_AUTOCAL_INTERVAL_DEFAULT_INDEX;
  led_autoadaptation_mode = LED_AUTOADAPT_DEFAULT;

  strncpy(tp3000_head_type_text, HEAD_TYPE_DEFAULT_TEXT, sizeof(tp3000_head_type_text) - 1U);
  tp3000_head_type_text[sizeof(tp3000_head_type_text) - 1U] = '\0';

  ui_language = LANG_DE;
  loop_debug_display_enabled = false;
  main_screen_layout = MAIN_SCREEN_LAYOUT_DEFAULT;
  tp_utc_offset_minutes = 0;
  tp_utc_offset_valid = false;
  adc_filter_mode = ADC_MEAS_FILTER_DEFAULT;
  adc1_sfocal_mode = ADC1_SFOCAL_DEFAULT;
  peltier_current_limit_ma = PELTIER_CURRENT_LIMIT_DEFAULT_MA;
}

static bool FLASHMEM tpControlConfigSanitize(void)
{
  bool changed = false;

  if (R.fan_percent < 50 || R.fan_percent > 100)
  {
    R.fan_percent = 75;
    changed = true;
  }
  if (R.optik_sollwert < 800 || R.optik_sollwert > 990)
  {
    R.optik_sollwert = 985;
    changed = true;
  }
  R.usb_report_cont = R.usb_report_cont ? 1 : 0;
  if (R.usb_report_type != REPORT_DATA &&
      R.usb_report_type != REPORT_INST &&
      R.usb_report_type != REPORT_AD_DEBUG)
  {
    R.usb_report_type = REPORT_DATA;
    changed = true;
  }
  if (R.pid_kp < 1 || R.pid_kp > 2000)
  {
    R.pid_kp = 120;
    changed = true;
  }
  if (!tpFloatValid(R.pid_ki, 0.0f, 200.0f))
  {
    R.pid_ki = 1.5f;
    changed = true;
  }
  if (!tpFloatValid(R.pid_kd, 0.0f, 200.0f))
  {
    R.pid_kd = 5.0f;
    changed = true;
  }
  if (R.freiheiz_ziel_temp[0] < 40 || R.freiheiz_ziel_temp[0] > 76)
  {
    R.freiheiz_ziel_temp[0] = 76;
    changed = true;
  }
  R.freiheiz_ziel_temp[1] = 10;
  R.freiheiz_ziel_temp[2] = 20;

  if (R.h_bruecke_totzeit < 1 || R.h_bruecke_totzeit > 100)
  {
    R.h_bruecke_totzeit = 15;
    changed = true;
  }
  if (R.regler_intervall_ms < 50 || R.regler_intervall_ms > 5000)
  {
    R.regler_intervall_ms = 250;
    changed = true;
  }
  if (R.optik_autocal_interval_index < OPTIK_AUTOCAL_INTERVAL_MIN_INDEX ||
      R.optik_autocal_interval_index > OPTIK_AUTOCAL_INTERVAL_MAX_INDEX)
  {
    R.optik_autocal_interval_index = OPTIK_AUTOCAL_INTERVAL_DEFAULT_INDEX;
    changed = true;
  }
  if (led_autoadaptation_mode > LED_AUTOADAPT_MAX)
  {
    led_autoadaptation_mode = LED_AUTOADAPT_DEFAULT;
    changed = true;
  }

  adc_filter_mode = adcFilterSanitize(adc_filter_mode);
  adc1_sfocal_mode = adc1SfocalSanitize(adc1_sfocal_mode);
  peltier_current_limit_ma = peltierCurrentLimitSanitizeMa(peltier_current_limit_ma);

  return changed;
}

static bool FLASHMEM tpDeviceUiConfigSanitize(void)
{
  bool changed = false;

  if (R.display_konfig.tft_backlight > 10)
  {
    R.display_konfig.tft_backlight = 7;
    changed = true;
  }
  R.display_konfig.tft_on = false;

  if (R.head_type >= HEAD_TYPE_COUNT)
  {
    R.head_type = HEAD_TYPE_DEFAULT;
    changed = true;
  }
  if (R.head_serial < HEAD_SERIAL_MIN || R.head_serial > HEAD_SERIAL_MAX)
  {
    R.head_serial = HEAD_SERIAL_DEFAULT;
    changed = true;
  }
  if (deviceSerialNormalize(R.geraete_name, sizeof(R.geraete_name)))
  {
    changed = true;
  }

  if (headTypeTextNormalize(tp3000_head_type_text, sizeof(tp3000_head_type_text)))
  {
    if (R.head_type < HEAD_TYPE_COUNT)
    {
      strncpy(tp3000_head_type_text, tp3000HeadName(R.head_type), sizeof(tp3000_head_type_text) - 1U);
      tp3000_head_type_text[sizeof(tp3000_head_type_text) - 1U] = '\0';
    }
    changed = true;
  }
  const int8_t profile = headTypeProfileFromText(tp3000_head_type_text);
  if (profile >= 0 && R.head_type != (uint8_t)profile)
  {
    R.head_type = (uint8_t)profile;
    changed = true;
  }

  if (ui_language >= LANG_COUNT)
  {
    ui_language = LANG_DE;
    changed = true;
  }
  loop_debug_display_enabled = loop_debug_display_enabled ? true : false;

  uint8_t safeMainLayout = mainScreenLayoutSanitize(main_screen_layout);
  if (main_screen_layout != safeMainLayout)
  {
    main_screen_layout = safeMainLayout;
    changed = true;
  }

  if (tp_utc_offset_valid && !tpUtcOffsetMinutesPlausible(tp_utc_offset_minutes))
  {
    tp_utc_offset_minutes = 0;
    tp_utc_offset_valid = false;
    changed = true;
  }

  return changed;
}

// -----------------------------------------------------------------------------
// Mess-/Regelparameter A/B
// -----------------------------------------------------------------------------
#define TP_CONTROL_CFG_MAGIC       0x5443524CUL   // 'TCRL'
#define TP_CONTROL_CFG_VERSION     1U
#define TP_CONTROL_CFG_SLOT_A_ADDR 1536
#define TP_CONTROL_CFG_SLOT_B_ADDR 1792
#define TP_CONTROL_CFG_SLOT_SIZE   256

typedef struct
{
  uint8_t  fan_percent;
  uint16_t optik_sollwert;
  uint8_t  usb_report_cont;
  uint8_t  usb_report_type;
  uint16_t pid_kp;
  float    pid_ki;
  float    pid_kd;
  uint8_t  freiheiz_ziel_temp;
  uint8_t  h_bruecke_totzeit;
  uint16_t regler_intervall_ms;
  uint8_t  optik_autocal_interval_index;
  uint8_t  adc_filter_mode;
  uint8_t  adc1_sfocal_mode;
  uint16_t peltier_current_limit_ma;
  // Kodierung 1..3. Der alte reservierte Nullwert bedeutet Migration auf Default.
  uint8_t  led_autoadaptation_mode_encoded;
  uint8_t  reserved[26];
} tp_control_config_payload_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  tp_control_config_payload_t payload;
  uint32_t crc;
} tp_control_config_slot_t;

static_assert(sizeof(tp_control_config_slot_t) <= TP_CONTROL_CFG_SLOT_SIZE,
              "TP control EEPROM slot too small");

static uint32_t tp_control_cfg_sequence = 0UL;
static uint8_t  tp_control_cfg_active_slot = 0U;
static bool     tp_control_cfg_loaded = false;

static int FLASHMEM tpControlConfigSlotAddr(uint8_t slot)
{
  return slot ? TP_CONTROL_CFG_SLOT_B_ADDR : TP_CONTROL_CFG_SLOT_A_ADDR;
}

static void FLASHMEM tpControlConfigPayloadFromRuntime(tp_control_config_payload_t& payload)
{
  memset(&payload, 0, sizeof(payload));
  payload.fan_percent = R.fan_percent;
  payload.optik_sollwert = R.optik_sollwert;
  payload.usb_report_cont = R.usb_report_cont ? 1 : 0;
  payload.usb_report_type = R.usb_report_type;
  payload.pid_kp = R.pid_kp;
  payload.pid_ki = R.pid_ki;
  payload.pid_kd = R.pid_kd;
  payload.freiheiz_ziel_temp = R.freiheiz_ziel_temp[0];
  payload.h_bruecke_totzeit = R.h_bruecke_totzeit;
  payload.regler_intervall_ms = R.regler_intervall_ms;
  payload.optik_autocal_interval_index = R.optik_autocal_interval_index;
  payload.adc_filter_mode = adcFilterModeGet();
  payload.adc1_sfocal_mode = adc1SfocalModeGet();
  payload.peltier_current_limit_ma = peltierCurrentLimitGetMa();
  payload.led_autoadaptation_mode_encoded =
      (uint8_t)(led_autoadaptation_mode + 1U);
}

static void FLASHMEM tpControlConfigApplyPayload(const tp_control_config_payload_t& payload)
{
  R.fan_percent = payload.fan_percent;
  R.optik_sollwert = payload.optik_sollwert;
  R.usb_report_cont = payload.usb_report_cont ? 1 : 0;
  R.usb_report_type = payload.usb_report_type;
  R.pid_kp = payload.pid_kp;
  R.pid_ki = payload.pid_ki;
  R.pid_kd = payload.pid_kd;
  R.freiheiz_ziel_temp[0] = payload.freiheiz_ziel_temp;
  R.h_bruecke_totzeit = payload.h_bruecke_totzeit;
  R.regler_intervall_ms = payload.regler_intervall_ms;
  R.optik_autocal_interval_index = payload.optik_autocal_interval_index;
  adc_filter_mode = adcFilterSanitize(payload.adc_filter_mode);
  adc1_sfocal_mode = adc1SfocalSanitize(payload.adc1_sfocal_mode);
  peltier_current_limit_ma = peltierCurrentLimitSanitizeMa(payload.peltier_current_limit_ma);
  if (payload.led_autoadaptation_mode_encoded >= 1U &&
      payload.led_autoadaptation_mode_encoded <= (LED_AUTOADAPT_MAX + 1U))
  {
    led_autoadaptation_mode =
        (uint8_t)(payload.led_autoadaptation_mode_encoded - 1U);
  }
  else
  {
    // Alte V1-Slots hatten an dieser Stelle ein reserviertes Nullbyte.
    led_autoadaptation_mode = LED_AUTOADAPT_DEFAULT;
  }
}

static bool FLASHMEM tpControlConfigReadSlot(uint8_t slot, tp_control_config_slot_t& out)
{
  EEPROM.get(tpControlConfigSlotAddr(slot), out);
  if (out.magic != TP_CONTROL_CFG_MAGIC) return false;
  if (out.version != TP_CONTROL_CFG_VERSION) return false;
  if (out.size != sizeof(tp_control_config_payload_t)) return false;
  const uint32_t crc = tpConfigCrcBytes(&out, sizeof(out) - sizeof(out.crc));
  return (out.crc == crc);
}

static void FLASHMEM tpControlConfigSaveInternal(void)
{
  tpControlConfigSanitize();

  tp_control_config_slot_t slotA;
  tp_control_config_slot_t slotB;
  const bool validA = tpControlConfigReadSlot(0, slotA);
  const bool validB = tpControlConfigReadSlot(1, slotB);

  uint32_t nextSeq = tp_control_cfg_sequence;
  if (validA && slotA.sequence > nextSeq) nextSeq = slotA.sequence;
  if (validB && slotB.sequence > nextSeq) nextSeq = slotB.sequence;
  nextSeq++;
  if (nextSeq == 0UL) nextSeq = 1UL;

  uint8_t targetSlot;
  if (!validA) targetSlot = 0;
  else if (!validB) targetSlot = 1;
  else targetSlot = (slotA.sequence <= slotB.sequence) ? 0 : 1;

  tp_control_config_slot_t slotData;
  memset(&slotData, 0, sizeof(slotData));
  slotData.magic = TP_CONTROL_CFG_MAGIC;
  slotData.version = TP_CONTROL_CFG_VERSION;
  slotData.size = sizeof(tp_control_config_payload_t);
  slotData.sequence = nextSeq;
  tpControlConfigPayloadFromRuntime(slotData.payload);
  slotData.crc = tpConfigCrcBytes(&slotData, sizeof(slotData) - sizeof(slotData.crc));

  EEPROM.put(tpControlConfigSlotAddr(targetSlot), slotData);
  tp_control_cfg_sequence = nextSeq;
  tp_control_cfg_active_slot = targetSlot;
  tp_control_cfg_loaded = true;
}

static bool FLASHMEM tpControlConfigLoadInternal(void)
{
  tp_control_config_slot_t slotA;
  tp_control_config_slot_t slotB;
  const bool validA = tpControlConfigReadSlot(0, slotA);
  const bool validB = tpControlConfigReadSlot(1, slotB);

  if (!validA && !validB)
  {
    tp_control_cfg_sequence = 0UL;
    tp_control_cfg_active_slot = 0U;
    tpControlConfigSanitize();
    tpControlConfigSaveInternal();
    tpControlConfigSaveInternal();
    return false;
  }

  const tp_control_config_slot_t* best = nullptr;
  uint8_t bestSlot = 0U;
  if (validA && (!validB || slotA.sequence >= slotB.sequence))
  {
    best = &slotA;
    bestSlot = 0U;
  }
  else
  {
    best = &slotB;
    bestSlot = 1U;
  }

  tpControlConfigApplyPayload(best->payload);
  tp_control_cfg_sequence = best->sequence;
  tp_control_cfg_active_slot = bestSlot;
  tp_control_cfg_loaded = true;

  const bool needsSelfHeal = (validA != validB);
  const bool needsSanitizeSave = tpControlConfigSanitize();
  if (needsSanitizeSave || needsSelfHeal)
  {
    tpControlConfigSaveInternal();
  }
  return true;
}

// -----------------------------------------------------------------------------
// Device-/UI-Settings A/B
// -----------------------------------------------------------------------------
#define TP_DEVICE_UI_CFG_MAGIC       0x54445549UL   // 'TDUI'
#define TP_DEVICE_UI_CFG_VERSION     1U
#define TP_DEVICE_UI_CFG_SLOT_A_ADDR 2048
#define TP_DEVICE_UI_CFG_SLOT_B_ADDR 2176
#define TP_DEVICE_UI_CFG_SLOT_SIZE   128

typedef struct
{
  uint8_t  language;
  uint8_t  loop_debug_display;
  uint8_t  tft_backlight;
  uint8_t  head_type;
  uint32_t head_serial;
  char     device_sn[DEVICE_SERIAL_DIGITS + 1U];
  char     head_type_text[HEAD_TYPE_TEXT_LEN + 1U];
  uint8_t  main_screen_layout;
  uint8_t  reserved[34];
} tp_device_ui_config_payload_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  tp_device_ui_config_payload_t payload;
  uint32_t crc;
} tp_device_ui_config_slot_t;

static_assert(sizeof(tp_device_ui_config_slot_t) <= TP_DEVICE_UI_CFG_SLOT_SIZE,
              "TP device/UI EEPROM slot too small");

static uint32_t tp_device_ui_cfg_sequence = 0UL;
static uint8_t  tp_device_ui_cfg_active_slot = 0U;
static bool     tp_device_ui_cfg_loaded = false;

static int FLASHMEM tpDeviceUiConfigSlotAddr(uint8_t slot)
{
  return slot ? TP_DEVICE_UI_CFG_SLOT_B_ADDR : TP_DEVICE_UI_CFG_SLOT_A_ADDR;
}

static void FLASHMEM tpDeviceUiConfigPayloadFromRuntime(tp_device_ui_config_payload_t& payload)
{
  memset(&payload, 0, sizeof(payload));
  payload.language = (ui_language < LANG_COUNT) ? ui_language : LANG_DE;
  payload.loop_debug_display = loop_debug_display_enabled ? 1 : 0;
  payload.tft_backlight = R.display_konfig.tft_backlight;
  payload.head_type = R.head_type;
  payload.head_serial = R.head_serial;
  strncpy(payload.device_sn, deviceSerialGet(), sizeof(payload.device_sn) - 1U);
  headTypeTextNormalize(tp3000_head_type_text, sizeof(tp3000_head_type_text));
  const size_t headTypeLength = strnlen(tp3000_head_type_text,
                                        sizeof(payload.head_type_text) - 1U);
  memcpy(payload.head_type_text, tp3000_head_type_text, headTypeLength);
  payload.head_type_text[headTypeLength] = '\0';
  payload.main_screen_layout = mainScreenLayoutGet();
  payload.reserved[0] = tpUtcOffsetValid() ? 1U : 0U;
  payload.reserved[1] = (uint8_t)((uint16_t)tp_utc_offset_minutes & 0xFFU);
  payload.reserved[2] = (uint8_t)(((uint16_t)tp_utc_offset_minutes >> 8) & 0xFFU);
}

static void FLASHMEM tpDeviceUiConfigApplyPayload(const tp_device_ui_config_payload_t& payload)
{
  ui_language = (payload.language < LANG_COUNT) ? payload.language : LANG_DE;
  loop_debug_display_enabled = (payload.loop_debug_display != 0);
  R.display_konfig.tft_backlight = payload.tft_backlight;
  R.head_type = payload.head_type;
  R.head_serial = payload.head_serial;

  strncpy(R.geraete_name, payload.device_sn, sizeof(R.geraete_name) - 1U);
  R.geraete_name[sizeof(R.geraete_name) - 1U] = '\0';

  strncpy(tp3000_head_type_text, payload.head_type_text, sizeof(tp3000_head_type_text) - 1U);
  tp3000_head_type_text[sizeof(tp3000_head_type_text) - 1U] = '\0';

  main_screen_layout = mainScreenLayoutSanitize(payload.main_screen_layout);

  tp_utc_offset_valid = (payload.reserved[0] == 1U);
  tp_utc_offset_minutes = (int16_t)((uint16_t)payload.reserved[1] |
                                    ((uint16_t)payload.reserved[2] << 8));
  if (!tpUtcOffsetMinutesPlausible(tp_utc_offset_minutes))
  {
    tp_utc_offset_minutes = 0;
    tp_utc_offset_valid = false;
  }
}

static bool FLASHMEM tpDeviceUiConfigReadSlot(uint8_t slot, tp_device_ui_config_slot_t& out)
{
  EEPROM.get(tpDeviceUiConfigSlotAddr(slot), out);
  if (out.magic != TP_DEVICE_UI_CFG_MAGIC) return false;
  if (out.version != TP_DEVICE_UI_CFG_VERSION) return false;
  if (out.size != sizeof(tp_device_ui_config_payload_t)) return false;
  const uint32_t crc = tpConfigCrcBytes(&out, sizeof(out) - sizeof(out.crc));
  return (out.crc == crc);
}

void FLASHMEM tpDeviceUiConfigSave(void)
{
  tpDeviceUiConfigSanitize();

  tp_device_ui_config_slot_t slotA;
  tp_device_ui_config_slot_t slotB;
  const bool validA = tpDeviceUiConfigReadSlot(0, slotA);
  const bool validB = tpDeviceUiConfigReadSlot(1, slotB);

  uint32_t nextSeq = tp_device_ui_cfg_sequence;
  if (validA && slotA.sequence > nextSeq) nextSeq = slotA.sequence;
  if (validB && slotB.sequence > nextSeq) nextSeq = slotB.sequence;
  nextSeq++;
  if (nextSeq == 0UL) nextSeq = 1UL;

  uint8_t targetSlot;
  if (!validA) targetSlot = 0;
  else if (!validB) targetSlot = 1;
  else targetSlot = (slotA.sequence <= slotB.sequence) ? 0 : 1;

  tp_device_ui_config_slot_t slotData;
  memset(&slotData, 0, sizeof(slotData));
  slotData.magic = TP_DEVICE_UI_CFG_MAGIC;
  slotData.version = TP_DEVICE_UI_CFG_VERSION;
  slotData.size = sizeof(tp_device_ui_config_payload_t);
  slotData.sequence = nextSeq;
  tpDeviceUiConfigPayloadFromRuntime(slotData.payload);
  slotData.crc = tpConfigCrcBytes(&slotData, sizeof(slotData) - sizeof(slotData.crc));

  EEPROM.put(tpDeviceUiConfigSlotAddr(targetSlot), slotData);
  tp_device_ui_cfg_sequence = nextSeq;
  tp_device_ui_cfg_active_slot = targetSlot;
  tp_device_ui_cfg_loaded = true;
}

void FLASHMEM tpDeviceUiConfigLoad(void)
{
  if (tp_device_ui_cfg_loaded)
  {
    tpDeviceUiConfigSanitize();
    return;
  }

  tp_device_ui_config_slot_t slotA;
  tp_device_ui_config_slot_t slotB;
  const bool validA = tpDeviceUiConfigReadSlot(0, slotA);
  const bool validB = tpDeviceUiConfigReadSlot(1, slotB);

  if (!validA && !validB)
  {
    tp_device_ui_cfg_sequence = 0UL;
    tp_device_ui_cfg_active_slot = 0U;
    tpDeviceUiConfigSanitize();
    tpDeviceUiConfigSave();
    tpDeviceUiConfigSave();
    return;
  }

  const tp_device_ui_config_slot_t* best = nullptr;
  uint8_t bestSlot = 0U;
  if (validA && (!validB || slotA.sequence >= slotB.sequence))
  {
    best = &slotA;
    bestSlot = 0U;
  }
  else
  {
    best = &slotB;
    bestSlot = 1U;
  }

  tpDeviceUiConfigApplyPayload(best->payload);
  tp_device_ui_cfg_sequence = best->sequence;
  tp_device_ui_cfg_active_slot = bestSlot;
  tp_device_ui_cfg_loaded = true;

  const bool needsSelfHeal = (validA != validB);
  const bool needsSanitizeSave = tpDeviceUiConfigSanitize();
  if (needsSanitizeSave || needsSelfHeal)
  {
    tpDeviceUiConfigSave();
  }
}

// -----------------------------------------------------------------------------
// Oeffentliche Kompatibilitaets-Wrapper
// -----------------------------------------------------------------------------
void FLASHMEM tpMainConfigSave(void)
{
  tpControlConfigSaveInternal();
  tpDeviceUiConfigSave();
  // Jede Änderung des Hauptblocks kann kopfbezogene Justier-/Regelwerte
  // betreffen. Die Kalibrierungsverwaltung vergleicht den tatsächlichen
  // Zustand und schreibt nur bei einer echten relevanten Abweichung ein Ende.
  tpSignedCalibrationInvalidateCache();
}

bool FLASHMEM tpMainConfigLoad(void)
{
  tpConfigSetAllDefaults();
  const bool controlOk = tpControlConfigLoadInternal();
  tpDeviceUiConfigLoad();
  return controlOk;
}

void FLASHMEM tpMainConfigFactoryReset(void)
{
  tpConfigSetAllDefaults();
  tp_control_cfg_sequence = 0UL;
  tp_control_cfg_active_slot = 0U;
  tp_device_ui_cfg_sequence = 0UL;
  tp_device_ui_cfg_active_slot = 0U;
  tpMainConfigSave();
}

// =============================================================================
// DISPLAY-HARDWARE & UI-TEXTBOXEN
// =============================================================================
RA8875 tft = RA8875(RA8875_CS, RA8875_RESET, 11, 13, 12); 
GSL1680 TS = GSL1680();
uint16_t TouchY = 0, TouchX = 0;
bool TouchZ = false;

// Flanken-Latch fuer den SETUP-Bereich unten links. Der Zustand liegt bewusst
// auf Dateiebene, damit auch Seitenwechsel aus Setup > Anzeige ihn setzen
// koennen. So wird ein noch aufliegender ENTER-Finger nach dem Wechsel zu
// Hauptscreen, Diagnose 1, Diagnose 2 oder ADC-Info nicht sofort wieder als
// SETUP erkannt. Freigabe erst nach echtem Loslassen.
static bool setup_touch_latch = false;

void setupTouchLatchUntilRelease(void)
{
  setup_touch_latch = true;
}

char lcd_buf[256]; 
char incoming_command_string[50];

// Die grossen Dashboard-TextBoxen enthalten jeweils zwei 1800-Byte-
// Zeichenpuffer und bleiben in RAM2 (DMAMEM).
DMAMEM TextBox VirtLCDUeberschrift;
DMAMEM TextBox VirtLCDTemperaturen;
DMAMEM TextBox VirtLCDGrossanzeige;
DMAMEM TextBox VirtLCDStatuszeile;
// Zusatzwert unter dem Chart (Feuchte bzw. Taupunkt) mit zeichenweisem
// Update wie die bestehenden Dashboard-TextBoxen. Bewusst in RAM2.
DMAMEM TextBox VirtLCDChartZusatzwert;
DMAMEM TextBox RTC_PM;

// Menü- und Meldungsbox werden statisch bereitgestellt. Damit gibt es beim
// Booten kein operator new() mehr, das bei knappem/fragmentiertem RAM2-Heap
// nullptr liefern und anschließend im compilererzeugten memset() abstürzen
// kann. Die Objekte liegen bewusst in RAM1; die DroidSansMono-Fonttabellen
// wurden dafür vollständig in den QSPI-Flash verschoben.
static TextBox VirtLCDMenuStorage;
static TextBox VirtLCDMessageStorage;
TextBox* VirtLCDMenu = &VirtLCDMenuStorage;
TextBox* VirtLCDMessage = &VirtLCDMessageStorage;

// =============================================================================
// PROTOTYPEN-DEKLARATIONEN (Echte Funktions-Verknüpfungen)
// =============================================================================
void leseAdcRohwerte();
void ads1263ServiceAdc2Background();
void verarbeiteRegelung();
void initI2C2Peripherie();
void leseEchtzeitUhr();
void leseDruckSensor();
void aktualisiereDisplayAnzeige();
void eraseDisplay();
void manage_Touchscreen();
void ConfigMenu();
void chartHistoryTask();
bool mainDisplayFrameHit(uint16_t x, uint16_t y);
bool mainDisplayChartRangeHit(uint16_t x, uint16_t y);
void mainDisplayCycleChartRange();
void mainDisplayCycleView();
void usb_read_serial();
void usb_cont_report();
void initDRV8873S();
void initADS1263();
void lesePeltierStrom();
void safetyInit();
void safetyFeedRegelung();
void safetyTask();
void safetyBeginBlockingOperation();
void safetyEndBlockingOperation();
void menuInvalidatePageDrawCache(void);
void menuTimeoutResetActivity();
void menuTimeoutNotifyTouch(bool touched);
void menuTimeoutTask();

// ============================================================================
// FAN PWM + FAN TACHO
// ============================================================================

#define FAN_PWM_MAX 4095
#define FAN_TACHO_STARTUP_GRACE_MS 5000UL
#define FAN_TACHO_TIMEOUT_MS       2500UL

static bool sensorFanEnabled = true;        // bewusst NICHT im EEPROM: nach Neustart immer FAN ON
static uint32_t fanOnSinceMs = 0;
static uint32_t fanTachoLastPulseMs = 0;
static uint32_t fanTachoLastSeenCount = 0;
static volatile uint32_t fanTachoPulseCount = 0;

void fanTachoISR(void)
{
  fanTachoPulseCount++;
}

void fanTachoService(void)
{
  uint32_t c;

  noInterrupts();
  c = fanTachoPulseCount;
  interrupts();

  if (c != fanTachoLastSeenCount)
  {
    fanTachoLastSeenCount = c;
    fanTachoLastPulseMs = millis();
  }
}

void fanTachoBegin(void)
{
  pinMode(pinFanTacho, INPUT_PULLUP);

  fanOnSinceMs = millis();
  fanTachoLastPulseMs = fanOnSinceMs;
  fanTachoLastSeenCount = fanTachoPulseCount;

  attachInterrupt(digitalPinToInterrupt(pinFanTacho), fanTachoISR, FALLING);
}

uint16_t fanPercentToPwm(uint8_t percent)
{
  percent = constrain(percent, 50, 100);
  return map(percent, 0, 100, 0, FAN_PWM_MAX);
}

bool sensorFanIsEnabled(void)
{
  return sensorFanEnabled;
}

bool sensorFanTachoMissing(void)
{
  if (!sensorFanEnabled) return false;

  fanTachoService();

  uint32_t now = millis();

  // 5 s Anlaufzeit: waehrenddessen noch keine gelbe Warnung.
  if ((uint32_t)(now - fanOnSinceMs) < FAN_TACHO_STARTUP_GRACE_MS)
  {
    return false;
  }

  return ((uint32_t)(now - fanTachoLastPulseMs) > FAN_TACHO_TIMEOUT_MS);
}

const char* sensorFanStatusText(void)
{
  return sensorFanEnabled ? "FAN ON" : "FAN OFF";
}

void fanApplyNormalSpeed(void)
{
  R.fan_percent = constrain(R.fan_percent, 50, 100);

  if (!sensorFanEnabled)
  {
    analogWrite(pinPwmFan, 0);
    return;
  }

  analogWrite(pinPwmFan,
              fanPercentToPwm(R.fan_percent));
}

void sensorFanSetEnabled(bool enabled)
{
  if (sensorFanEnabled != enabled)
  {
    sensorFanEnabled = enabled;
    fanOnSinceMs = millis();
    fanTachoLastPulseMs = fanOnSinceMs;

    noInterrupts();
    fanTachoLastSeenCount = fanTachoPulseCount;
    interrupts();
  }

  fanApplyNormalSpeed();
}

void sensorFanToggleEnabled(void)
{
  sensorFanSetEnabled(!sensorFanEnabled);
}

// FIX: Rückgabetyp exakt an drv8873s.h angepasst (bool statt void!)
bool checkDRV8873SFaults();

// Externe Deklarationen für echte Funktionen aus anderen Tabs freigeben
extern void TXTtestButton(short x, short y, short w, short h, short xoff, short font,
                          const char* text, short onoff, uint16_t txtcol, uint16_t butcol);

// =============================================================================
// STABILE TFT-/MENÜ-HILFSFUNKTIONEN
// =============================================================================
static inline void tftClearBlackSafe()
{
  // clearScreen() ist auf manchen RA8875-Versionen empfindlicher.
  // fillScreen(BLACK) ist für den Test robuster.
  tft.fillScreen(BLACK);
  delayMicroseconds(200);
}


static inline void tftTransferWait()
{
  delayMicroseconds(120);
}


static void initMenuTextBoxesIfNeeded()
{
  // Statische Objekte: kein delete/new und damit keine Heap-Fragmentierung
  // oder Nullzeigergefahr beim erneuten Eintritt ins Setup.
  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  // Beim Eintritt ins Menü nur neu initialisieren/clearen.
  VirtLCDMenu->init(45, 12, 155, 40, DroidSansMono_20, WHITE, BLACK);
  VirtLCDMenu->clear();
  VirtLCDMenu->invalidate();

  VirtLCDMessage->init(45, 3, 140, 300, DroidSansMono_20, WHITE, BLACK);
  VirtLCDMessage->clear();
  VirtLCDMessage->invalidate();
}


static void transferConfigTextBoxesSafe()
{
  // Nur die fürs Setup relevanten Boxen übertragen.
  // Dashboard-Transfers im Setup vermeiden, damit der RA8875 nicht doppelt beschäftigt wird.
  VirtLCDUeberschrift.transfer();
  tftTransferWait();

  if (VirtLCDMenu != nullptr) {
    VirtLCDMenu->transfer();
    tftTransferWait();
  }

  if (VirtLCDMessage != nullptr) {
    VirtLCDMessage->transfer();
    tftTransferWait();
  }

  RTC_PM.transfer();
  tftTransferWait();
}


static void enterConfigModeFromTouch()
{
  // Der SETUP-Button liegt an derselben Stelle wie ENTER im Menue.
  // Deshalb darf die noch anliegende Beruehrung nach dem Bildschirmwechsel
  // nicht als ENTER im neuen Menue weiterverwendet werden. Erst nach dem
  // echten Loslassen werden Menue-Tasten wieder freigegeben.
  menuBlockInputUntilTouchRelease();

  eraseDisplay();
  tftClearBlackSafe();

  refresh = true;

  flag.config_mode = true;
  flag.mode_change = true;

  menuInvalidatePageDrawCache();

  // Zwingt ConfigMenu zum vollständigen Neuaufbau
  flag.menu_lcd_upd = false;

  // Menü immer sauber bei Ebene 0 starten
  menu_level = 0;
  current_selection = 0;
  touch_scroll_aktiv = false;

  // Menü-Buttons: 0..3
  FirstBut = 0;
  LastBut  = 3;

  // Kaltstart-Trigger für dein Menü-Tab
  frisch_geoeffnet = true;

  menuTimeoutResetActivity();

  initMenuTextBoxesIfNeeded();
  transferConfigTextBoxesSafe();
}

// ============================================================================
// FAN PWM SERVICE
// ============================================================================

void serviceFanPwm(void)
{
  static uint8_t old_percent = 255;

  fanTachoService();

  R.fan_percent = constrain(R.fan_percent, 50, 100);

  if (old_percent != R.fan_percent)
  {
    old_percent = R.fan_percent;
    fanApplyNormalSpeed();
  }
}

bool ra8875WaitReady(uint16_t timeout_ms)
{
  uint32_t start = millis();

  while (digitalRead(pinWaitDisp) == LOW)
  {
    if ((millis() - start) > timeout_ms)
    {
      return false;   // nicht endlos haengen bleiben
    }
  }

  return true;
}

// Boot-/Resetdiagnose. Der rohe i.MX-RT Resetstatus und ein vorhandener
// Teensy-CrashReport bleiben bis zum nächsten Neustart abrufbar.
static uint32_t systemBootResetStatusRaw = 0;
static bool systemBootCrashReportAvailable = false;
static bool systemBootWatchdogReset = false;
static bool systemBootRecoveryRequired = false;

uint32_t systemGetBootResetStatusRaw(void)
{
  return systemBootResetStatusRaw;
}

bool systemGetBootCrashReportAvailable(void)
{
  return systemBootCrashReportAvailable;
}

// Fruehe Boot-/Crashdiagnose auf der internen SD-Karte. Der Pfad liegt im
// Kartenwurzelverzeichnis, damit er ohne Weboberflaeche sofort auffindbar ist.
// Jede Ausgabe wird geschlossen und damit vor einer nachfolgenden Stoerung
// sicher auf die Karte geschrieben. CrashReport.printTo() loescht den Bericht
// nach erfolgreicher Ausgabe; deshalb hat die SD-Ausgabe Vorrang vor Serial.
static const char TP_BOOT_DIAG_PATH[] = "/TP3000_BOOT.TXT";
static bool tpBootDiagSdAttempted = false;
static bool tpBootDiagSdReady = false;
static bool tpBootDiagCrashStored = false;

static bool tpBootDiagEnsureSd(void)
{
  if (tpBootDiagSdAttempted) return tpBootDiagSdReady;
  tpBootDiagSdAttempted = true;
  tpFirmwareIntegrityBootService();
  tpBootDiagSdReady = sdLogEnsureReadyForAccess();
  tpFirmwareIntegrityBootService();
  return tpBootDiagSdReady;
}

static bool tpBootDiagAppend(const char* stage, bool includeCrashReport)
{
  if (stage == nullptr || !tpBootDiagEnsureSd()) return false;

  // Ein unbegrenzt wachsendes Diagnosefile vermeiden. Nach 128 KiB beginnt
  // automatisch eine neue Datei; fuer die Fehlersuche bleiben viele Starts.
  if (SD.exists(TP_BOOT_DIAG_PATH))
  {
    File previous = SD.open(TP_BOOT_DIAG_PATH, FILE_READ);
    const bool rotate = previous && previous.size() > 131072UL;
    if (previous) previous.close();
    if (rotate) SD.remove(TP_BOOT_DIAG_PATH);
  }

  tpFirmwareIntegrityBootService();
  File logFile = SD.open(TP_BOOT_DIAG_PATH, FILE_WRITE);
  if (!logFile) return false;

  logFile.println("============================================================");
  logFile.print("Build: ");
  logFile.print(TP_FIRMWARE_VERSION_STRING);
  logFile.print(" / ");
  logFile.println(TP_FIRMWARE_BUILD_ID_STRING);
  const uint32_t linkedImageSize = tpFirmwareIntegrityLinkedImageSize();
  logFile.println("Firmware ImageBase: 0x60000000");
  logFile.print("Firmware ImageSize: ");
  logFile.println((unsigned long)linkedImageSize);
  logFile.print("Firmware ImageEnd: 0x");
  logFile.println(0x60000000UL + linkedImageSize, HEX);
  logFile.print("Millis: ");
  logFile.println((unsigned long)millis());
  logFile.print("Reset SRC_SRSR: 0x");
  logFile.println(systemBootResetStatusRaw, HEX);
  logFile.print("CrashReport vorhanden: ");
  logFile.println(systemBootCrashReportAvailable ? "JA" : "NEIN");
  logFile.print("Watchdog-Reset: ");
  logFile.println(systemBootWatchdogReset ? "JA" : "NEIN");
  logFile.print("Diagnose-Wiederanlauf: ");
  logFile.println(systemBootRecoveryRequired ? "JA" : "NEIN");
  logFile.print("Stufe: ");
  logFile.println(stage);

  if (includeCrashReport && systemBootCrashReportAvailable)
  {
    logFile.println("Gespeicherter Teensy CrashReport:");
    logFile.print(CrashReport);
    tpBootDiagCrashStored = true;
  }

  logFile.flush();
  logFile.close();
  tpFirmwareIntegrityBootService();
  return true;
}

//=========================================================
void serialProtocolBegin(void);
void serialProtocolTask(void);
void serialProtocolTraceBegin(void);
void serialProtocolTraceStorageTask(void);
void ethernetServiceBegin(void);
void ethernetServiceTask(void);
void winControlOutBegin(void);
void winControlOutEthernetTask(void);
bool ethernetWebSetupSessionActive(void);

// =============================================================================
// 8. SETUP (Komplett bereinigt & für das zentrierte Groß-Layout optimiert)
// =============================================================================
void setup() {
#if defined(__IMXRT1062__)
  systemBootResetStatusRaw = SRC_SRSR;
  systemBootWatchdogReset =
      (systemBootResetStatusRaw & (SRC_SRSR_WDOG_RST_B | SRC_SRSR_WDOG3_RST_B)) != 0U;
#else
  systemBootResetStatusRaw = 0;
  systemBootWatchdogReset = false;
#endif
  systemBootCrashReportAvailable = (bool)CrashReport;
  systemBootRecoveryRequired =
      systemBootCrashReportAvailable || systemBootWatchdogReset;
  if (!systemBootCrashReportAvailable)
    CrashReport.breadcrumb(1, 0x54503001UL); // TP0, setup gestartet

  // Ein durch einen vorherigen Lauf bereits aktiver Hardware-Watchdog kann
  // einen Reset ueberleben. Deshalb sofort neu initialisieren und waehrend des
  // gesamten Bootvorgangs an definierten Checkpoints fuettern. Erst nach setup()
  // gilt wieder strikt: Feed nur am Ende einer vollstaendigen Hauptloop.
  tp3000WatchdogBegin();
  tpFirmwareIntegrityBootService();
  
  // Hardware-Eingangspins vorbereiten (Mit deinen realen Konstanten)
  pinMode(pinWaitDisp, INPUT_PULLUP);   
  pinMode(pinIntDisp, INPUT);
  pinMode(pinIntExp, INPUT);    
  pinMode(pinIntClk, INPUT);
  pinMode(pinNfault, INPUT_PULLUP);
  
  // Schaltet deinen echten Touch-Interrupt Pin 8 (pinIntTouch) als Eingang frei
  pinMode(pinIntTouch, INPUT_PULLUP); 
  
  // Hardware-Ausgangspins initialisieren
  pinMode(pinNscs, OUTPUT);       digitalWrite(pinNscs, HIGH);
  pinMode(pinCsDisable, OUTPUT);  digitalWrite(pinCsDisable, HIGH);
  pinMode(pinCsLed, OUTPUT);      digitalWrite(pinCsLed, HIGH);

  // LED_OFF ist aktiv HIGH: Q1 zieht die LED-Stromregelung/Gate nach GND.
  // Wichtig: vor allen DAC-/Regler-Aktionen sicher AUS setzen.
  initLedOffControl();
  
  // FIX: Wir setzen die Ausgänge beim Systemstart auf HIGH oder INPUT,
  // um interne Multiplexer-Fehlzündungen auf den Touch-Pins (16/17) zu verhindern!
  pinMode(pinCurrRev, OUTPUT);    digitalWrite(pinCurrRev, LOW);
  pinMode(pinRelay, OUTPUT);      digitalWrite(pinRelay, LOW);
  
  // ADS1263/LWL-Pins frueh in einen sicheren Grundzustand bringen.
  // Achtung: CS/START/RESET/DRDY sind in der LWL-Hardware invertiert.
  // Teensy LOW  an pincs1   = ADS-CS inaktiv
  // Teensy HIGH an pinStart = ADS-START aus
  // Teensy HIGH an pinRstAd = ADS-RESET/PWDN aktiv
  pinMode(pincs1, OUTPUT);        digitalWrite(pincs1, LOW);
  pinMode(pinStart, OUTPUT);      digitalWrite(pinStart, HIGH);
  pinMode(pinRstAd, OUTPUT);      digitalWrite(pinRstAd, HIGH);
  pinMode(pinDrdy, INPUT);
  fanTachoBegin();
  
  // H-Bruecken Pins initialisieren. DRV8873 wird im PWM-Mode genutzt:
  // Pin37=IN1, Pin36=IN2, sicher AUS = HIGH/HIGH.
  pinMode(pinPh, OUTPUT);         digitalWrite(pinPh, HIGH);
  pinMode(pinEn, OUTPUT);         digitalWrite(pinEn, HIGH);
  
  // Serielle Hardware-Schnittstellen für Peripherie starten
  Serial7.begin(9600);           
  Serial8.begin(9600);
  
  // PWM-Frequenzen und Auflösungen festlegen
  analogWriteResolution(12); 
  analogWriteFrequency(pinAlarm, 3600); 
  analogWriteFrequency(pinPwmFan, 30000); 
  analogWriteFrequency(pinEn, 50000); 
  analogWriteFrequency(pinPh, 50000); 
  
  // Ausgaenge in sicheren Startzustand versetzen
  analogWrite(pinAlarm, 0); 
  analogWrite(pinPwmFan, 4000); 
  analogWrite(pinEn, 4096); 
  analogWrite(pinPh, 4096); 
  delay(250);
  tpFirmwareIntegrityBootService();

  // EEPROM V10: saubere Hauptkonfiguration laden.
  // Der alte 0xAA-Marker und der alte var_t-Dump werden nicht mehr benutzt.
  // Bei ungueltigen/halben Daten werden sichere Defaults erzeugt.
  tpMainConfigLoad();

  // Kalibrier-/Zusatzbloecke getrennt laden. Diese Bloecke haben eigene
  // Magic/Version/CRC-Pruefungen und fallen bei Schrott auf Defaults zurueck.
  pt100R0EnsureLoaded();
  taupunktOffsetEnsureLoaded();
  uiLanguageLoad();
  loopDebugDisplayLoad();
  adcFilterModeLoad();
  adc1SfocalModeLoad();
  peltierCurrentLimitLoad();
  alarmConfigLoad();

  // Schnittstellen-Konfiguration nur laden und serielle Baudraten setzen.
  // Ethernet/SD/Seriellog werden erst NACH der Displayinitialisierung gestartet,
  // damit statische IP, fehlendes LAN-Kabel oder SD-Probleme nie wieder einen
  // schwarzen Boot vor dem TFT erzeugen.
  interfaceConfigLoad();
  interfaceApplySerialBaud();
  tpFirmwareIntegrityBootService();

  // =========================================================================
  // SCHRITT 1: BUS ANMELDEN & TOUCH UNGESTÖRT INITIALISIEREN (Wire1)
  // =========================================================================
  // MASTER-FIX: Wir zwingen den Teensy 4.1 Core, den Wire1-Bus (Pins 16 & 17)
  // offiziell anzumelden und intern auf 400kHz einzustellen!
  //Wire1.begin();
  //Wire1.setClock(400000); 
  //delay(50); // Dem Bus Zeit zum elektrischen Stabilisieren geben

  tft.begin(RA8875_800x480, 16, 20000000, 8000000);
  if (!systemBootCrashReportAvailable)
    CrashReport.breadcrumb(1, 0x54503002UL); // TP0, TFT initialisiert
  tpFirmwareIntegrityBootService();
  // Startet den GSL1680. Da Wire1 nun im Hauptcode bereits wach ist,
  // funktioniert der Firmware-Upload über die Pins 16/17 jetzt absolut fehlerfrei!
  TS.begin(-1, TC_INT);
  tpFirmwareIntegrityBootService();
  tft.setRotation(0);
  
  // Zeichnet das Erdbild auf den Schirm
  tft.writeRect(0, 0, 800, 480, (uint16_t*)earth);

  // Individuellen Geraeteschluessel und ein eventuell vorhandenes
  // Root-signiertes Geraetezertifikat laden und kryptografisch pruefen.
  // Erst ab hier kann die zertifizierte SN die vorlaeufige EEPROM-SN ersetzen.
  deviceIdentityBegin();
  tpFirmwareIntegrityBootService();
  
  tft.setFont(DroidSansMono_28);
  tft.setTextColor(BLACK);
  tft.setCursor(240 - 5, 328);
  tft.print(T(TXT_BOOT_PRODUCT_TITLE));
  tft.setCursor(240 + 5, 328);
  tft.print(T(TXT_BOOT_PRODUCT_TITLE));
  tft.setCursor(240, 328 - 5);
  tft.print(T(TXT_BOOT_PRODUCT_TITLE));
  tft.setCursor(240, 328 + 5);
  tft.print(T(TXT_BOOT_PRODUCT_TITLE));
  tft.setCursor(240 - 5, 328 - 5);
  tft.print(T(TXT_BOOT_PRODUCT_TITLE));
  tft.setCursor(240 + 5, 328 + 5);
  tft.print(T(TXT_BOOT_PRODUCT_TITLE));
  tft.setTextColor(WHITE);
  tft.setCursor(240, 328);  
  tft.print(T(TXT_BOOT_PRODUCT_TITLE));
  
  tft.setTextColor(BLACK);
  tft.setCursor(325 - 5, 378);
  tft.print("TP-3000");
  tft.setCursor(325 + 5, 378);
  tft.print("TP-3000");
  tft.setCursor(325, 378 - 5);
  tft.print("TP-3000");
  tft.setCursor(325, 378 + 5);
  tft.print("TP-3000");
  tft.setCursor(325 - 5, 378 - 5);
  tft.print("TP-3000");
  tft.setCursor(325 + 5, 378 + 5);
  tft.print("TP-3000");
  tft.setCursor(325, 378);
  tft.setTextColor(WHITE);
  tft.print("TP-3000");
  
  tft.setTextColor(WHITE);
  tft.setFont(DroidSansMono_18);

  tft.setCursor(1, 10);
  tft.print(T(TXT_BOOT_VERSION_LABEL));
  tft.print(VERSION);

tft.setCursor(1, 40);
snprintf(lcd_buf, sizeof(lcd_buf), T(TXT_BOOT_DEVICE_SN_FMT), deviceSerialGet());
tft.print(lcd_buf);

tft.setCursor(1, 70);
tft.print(T(TXT_BOOT_HEAD_LABEL));
tft.print(headTypeTextGet());

tft.setCursor(1, 100);
snprintf(lcd_buf, sizeof(lcd_buf), T(TXT_BOOT_HEAD_SERIAL_FMT), R.head_serial);
tft.print(lcd_buf);
tft.setFont(DroidSansMono_14);
tft.setCursor(1, 439);  tft.print(T(TXT_BOOT_FIRMWARE_AUTHOR));
tft.setCursor(1, 457);  tft.print(T(TXT_BOOT_BASED_ON));
  // Der langsame Ladebalken: Garantiert dem Touch-Chip die nötige Ruhezeit zum Booten
  for (int progress_bar = 0; progress_bar <= 800; progress_bar++) {
    delay(4);
    tft.fillRoundRect(0, 475, progress_bar, 5, 0, GREEN);
    if ((progress_bar % 25) == 0) tpFirmwareIntegrityBootService();
  }
  
  // PC-Debug-Schnittstelle und vor allem die abziehbare SD-Bootdiagnose.
  // Ein gespeicherter CrashReport wird zuerst auf SD geschrieben, weil seine
  // Ausgabe den Teensy-Core-Bericht anschliessend loescht.
  Serial.begin(115200);
  delay(10);
  const bool bootDiagWritten =
      tpBootDiagAppend("BOOT VOR FIRMWAREHASH", systemBootCrashReportAvailable);
#if defined(__IMXRT1062__)
  // Watchdog-Resetursache nach dem sicheren SD-Eintrag quittieren, damit der
  // Diagnose-Wiederanlauf wirklich nur einmal erfolgt. CrashReport.printTo()
  // quittiert den Resetstatus selbst; bei einem noch nicht gesicherten Bericht
  // bleibt er dagegen bewusst unangetastet.
  if (!systemBootCrashReportAvailable && systemBootWatchdogReset)
    SRC_SRSR = systemBootResetStatusRaw;
#endif
  Serial.print("BOOT: SRC_SRSR=0x");
  Serial.println(systemBootResetStatusRaw, HEX);
  if (systemBootCrashReportAvailable)
  {
    if (tpBootDiagCrashStored)
      Serial.println("BOOT: CrashReport auf /TP3000_BOOT.TXT gespeichert");
    else
    {
      // Nicht seriell ausgeben: CrashReport.printTo() wuerde den einzigen
      // gespeicherten Bericht loeschen. Er bleibt fuer einen spaeteren
      // SD-Schreibversuch erhalten.
      Serial.println("BOOT: SD-Diagnose fehlgeschlagen, CrashReport bleibt gespeichert");
    }
  }
  else if (!bootDiagWritten)
  {
    Serial.println("BOOT: SD-Diagnosedatei konnte nicht geschrieben werden");
  }

  // Das tatsaechlich gelinkte Firmwareabbild hashen, bevor Netzwerk,
  // Kalibrierimporte oder zertifizierte Logs gestartet werden. Nach einem
  // gespeicherten MPU-Crash oder Watchdog-Reset wird dieser Start ohne Hash
  // fortgesetzt. Dadurch kommt das Geraet aus der Bootschleife und der Bericht
  // kann von der SD-Karte geholt werden; zertifizierte Funktionen bleiben zu.
  const bool bootBreadcrumbsWritable =
      !systemBootCrashReportAvailable || tpBootDiagCrashStored;
  if (bootBreadcrumbsWritable)
  {
    CrashReport.breadcrumb(1, 0x54503003UL); // TP0, vor Firmwarehash
    CrashReport.breadcrumb(2, 0U);          // letzter kompletter Hash-Offset
  }
  (void)tpBootDiagAppend("FIRMWAREHASH START", false);
  const uint32_t firmwareHashStartedMs = millis();
  if (systemBootRecoveryRequired)
    tpFirmwareIntegritySkipAfterBootCrash();
  else
    tpFirmwareIntegrityBegin();
  const uint32_t firmwareHashDurationMs = millis() - firmwareHashStartedMs;
  if (bootBreadcrumbsWritable)
    CrashReport.breadcrumb(1, 0x54503004UL); // TP0, Firmwarehash verlassen

  Serial.print("FIRMWARE INTEGRITY: ");
  Serial.println(tpFirmwareIntegrityStatusText());
  Serial.print("FIRMWARE HASH DAUER: ");
  Serial.print(firmwareHashDurationMs);
  Serial.println(" ms");
  (void)tpBootDiagAppend(tpFirmwareIntegrityStatusText(), false);
  tpFirmwareIntegrityBootService();

  // Schnittstellen erst nach sichtbarem TFT-Bootscreen starten.
  // ethernetServiceBegin() startet Ethernet erst nach sichtbarem TFT-Bootscreen.
  // Im normalen Mess-Loop wird Ethernet.begin() nicht automatisch erneut gesucht.
  serialProtocolBegin();
  ethernetServiceBegin();
  winControlOutBegin();
  tpFirmwareIntegrityBootService();

  // Externe PDF-Kalibrierscheine und signierte Justierungen werden vor dem
  // Logger geladen. Der zertifizierte Logger kann dadurch beim Start einen
  // unterbrochenen Abschnitt mit exakt der gespeicherten Evidenz versiegeln.
  tpExternalCalibrationBegin();
  tpFirmwareIntegrityBootService();
  tpSignedCalibrationBegin();
  tpFirmwareIntegrityBootService();

  // LED-Grundkurve und kopfbezogenes Lernmodell liegen vorerst auf SD.
  // Ohne SD wird die Autoadaption sicher auf AUS gesetzt.
  ledAdaptationBegin();
  tpFirmwareIntegrityBootService();

  if (interfaceSdLoggingEnabled())
  {
    sdLogBegin();
    tpFirmwareIntegrityBootService();
  }

  serialProtocolTraceBegin();

  // =========================================================================
  // SCHRITT 2: DIE TEXTBOX-GEOMETRIEN IM ARBEITSSPEICHER DEKLARIEREN (Dashboard)
  // =========================================================================
  VirtLCDUeberschrift.init(50, 1, 8, 15, DroidSansMono_20, WHITE, BLACK);
  VirtLCDTemperaturen.init(25, 4, 440, 355, DroidSansMono_20, YELLOW, BLACK);
  VirtLCDGrossanzeige.init(10, 2, 252, 110, DroidSansMono_60, WHITE, BLACK);
  VirtLCDStatuszeile.init(45, 1, 186, 427, DroidSansMono_20, WHITE, BLACK);
  VirtLCDChartZusatzwert.init(20, 1, 100, 355, DroidSansMono_20, YELLOW, BLACK);
  RTC_PM.init(15, 1, 660, 15, DroidSansMono_20, GREEN, BLACK);

  // Statisch bereitgestellte Menü-/Nachrichtenboxen initialisieren.
  // Es findet an dieser Stelle keinerlei Heap-Allokation mehr statt.
  VirtLCDMenu->init(45, 12, 155, 40, DroidSansMono_20, WHITE, BLACK);
  VirtLCDMenu->clear();

  VirtLCDMessage->init(40, 10, 200, 150, DroidSansMono_20, YELLOW, BLACK);

  // =========================================================================
  // SCHRITT 3: TOUCH-BUS (WIRE1) HOCHTAKTEN & WIRE2 WECKEN
  // =========================================================================
  // Jetzt, wo die Firmware sicher im GSL1680 sitzt, schalten wir den Bus auf High-Speed
  //Wire1.setClock(2000000);
  delay(10);

  // Klima-Bus (Wire2) für RV-3129 RTC und den BMP585 Drucksensor wecken
  initI2C2Peripherie();
  delay(5);
  tpFirmwareIntegrityBootService();

  // Jetzt stehen RTC und SD bereit. Das Hersteller-Root-Firmwarezertifikat wird
  // geladen, gegen den gemessenen Hash geprüft und der Status transaktional
  // unter /CERTIFICATION/FIRMWARE.TPS dokumentiert.
  tpFirmwareIntegrityFinalizeStartup(tpCurrentUtcUnixTime());
  tpFirmwareIntegrityBootService();
  

  // Touch ist ab hier initialisiert und wird direkt im loop() gelesen.

  // =========================================================================
  // SCHRITT 4: SPLASH SAUBER LOESCHEN & DIREKT IN DEN HAUPTSCREEN WECHSELN
  // =========================================================================
  // Wichtig:
  // Nach dem Splash keine TextBox-Transfers mehr auf das Erdbild schreiben.
  // Sonst werden alte Textreste kurz schwarz ins Splash-Bild gezeichnet,
  // bevor der Bildschirm geloescht wird.
  tftClearBlackSafe();

  VirtLCDUeberschrift.clear();  VirtLCDTemperaturen.clear();
  VirtLCDStatuszeile.clear();   VirtLCDGrossanzeige.clear();
  RTC_PM.clear();

  VirtLCDUeberschrift.invalidate();  VirtLCDTemperaturen.invalidate();
  VirtLCDGrossanzeige.invalidate();  VirtLCDStatuszeile.invalidate();
  RTC_PM.invalidate();
  
  // Diagnose-Seiten nicht aus dem EEPROM starten.
  // Nach Power-Cycle/Reset immer mit dem Hauptscreen beginnen.
  mode_display = 0;
  flag.mode_change = true;
  flag.mode_display = true;
  
  R.display_konfig.tft_backlight = constrain(R.display_konfig.tft_backlight, 0, 10);
  tft.brightness((uint8_t)map(R.display_konfig.tft_backlight, 0, 10, 5, 230));

  // Metrologie-Messkarussell des ADS1263 auf SPI1 unblockiert scharfschalten
  initDRV8873S();
  initADS1263();

  // Sicherheitsueberwachung erst ganz am Ende des Bootvorgangs starten.
  // Waehrend Boot/SD/Display-Init stehen die DRV-Eingaenge bereits auf HIGH/HIGH;
  // der Hardware-Watchdog wird bis hier nur an kontrollierten Boot-Checkpoints gefuettert.
  safetyInit();

  // Der Watchdog wurde bereits ganz am Anfang des Bootvorgangs aktiviert und
  // dort nur an kontrollierten Boot-Checkpoints gefuettert. Ab jetzt erfolgt
  // der Feed ausschliesslich am Ende einer vollstaendig durchlaufenen Hauptloop.
  if (bootBreadcrumbsWritable)
    CrashReport.breadcrumb(1, 0x54503005UL); // TP0, setup vollstaendig
  (void)tpBootDiagAppend("SETUP VOLLSTAENDIG", false);
  tp3000WatchdogFeed();
}

void loop() {
  if (!tp3000LoopGuardEnter())
  {
    // Nicht fuettern: Eine verschachtelte oder nicht sauber beendete Loop soll
    // vom Watchdog erkannt werden.
    return;
  }

  loopDebugTick();
    // =========================================================================
  // SCHRITT 1: HARDWARE-ABFRAGE & LIVE-I2C-SCANNER (GSL1680 Test)
  // =========================================================================
  // 1. Holt die rohe Anzahl der erkannten Finger direkt von der Hardware
  int fingerZaehler = TS.dataread(); 
  
  if (fingerZaehler > 0) {
      TouchX = TS.readFingerX(0); 
      TouchY = TS.readFingerY(0);
      TouchZ = true; 
  } else {
      TouchX = 0;
      TouchY = 0;
      TouchZ = false;
  }

  // Hauptscreen-Touch direkt im schnellen Loop auswerten.
  // Der Display-/Menue-Takt laeuft nur alle 100 ms; kurze Tippberuehrungen
  // konnten deshalb bisher komplett zwischen zwei pollMetro-Abfragen liegen.
  // Die Flankensperre sorgt weiterhin fuer genau eine Aktion pro Beruehrung.
  static bool main_screen_touch_latch = false;

  if (!TouchZ)
  {
    main_screen_touch_latch = false;
  }
  else if (!main_screen_touch_latch)
  {
    main_screen_touch_latch = true;

    if (!flag.config_mode)
    {
      // Eine noch nicht quittierte Alarmmeldung hat Vorrang vor dem
      // Umschalten der Hauptansicht.
      bool alarm_touch_consumed = alarmTouchAcknowledge(TouchX, TouchY);

      if (!alarm_touch_consumed &&
               mode_display == 0 &&
               mainDisplayChartRangeHit(TouchX, TouchY))
      {
        mainDisplayCycleChartRange();
      }
      else if (!alarm_touch_consumed &&
               mode_display == 0 &&
               mainDisplayFrameHit(TouchX, TouchY))
      {
        mainDisplayCycleView();
      }
    }
  }

  // Ruft deine Touch-Verarbeitung fuer das Menue auf.
  manage_Touchscreen();
  ads1263ServiceAdc2Background();

  // RS232-Protokolle non-blocking bedienen (PC-Ausgabe, Befehle, Durchfluss).
  serialProtocolTask();
  ads1263ServiceAdc2Background();

  // Ethernet/Webserver und WinControl-raw-TCP non-blocking bedienen.
  ethernetServiceTask();
  winControlOutEthernetTask();
  ads1263ServiceAdc2Background();

  if (flag.config_mode) {
    menuTimeoutNotifyTouch(TouchZ);
  }

  // =========================================================================
  // SCHRITT 2: ASYNCHRONES MESSKARUSSELL (PT100, Photodiode & Regelung)
  // =========================================================================
  leseAdcRohwerte();
  ads1263ServiceAdc2Background();
  lesePeltierStrom();
  ads1263ServiceAdc2Background();
  checkDRV8873SFaults(); 
  verarbeiteRegelung();
  // Temperaturvorsteuerung nur im normalen Messbetrieb; Freiheizen,
  // Dunkelmessung und LED-Auto-Cal bleiben davon strikt getrennt.
  ledAdaptationTask();
  safetyFeedRegelung();
  safetyTask();
  alarmTask();

  // 1-h-Chart-Historie laeuft immer weiter, auch in Setup/Diagnose.
  chartHistoryTask();
 
  // ========================================================================= 
// SCHRITT 3: BEREINIGTE I2C2-PERIPHERIE (Drucksensor & Uhr) 
// ========================================================================= 
if (slowMetro.check()) { 
  usb_cont_report(); 
  
  // Holt die stabilen Luftdruck-Messwerte deines neuen BMP585 
  leseDruckSensor(); 
  
  //Setzt Lüfter drehzahl
  serviceFanPwm();

  // SD-Logging serviced sich selbst ueber das eingestellte Intervall.
  sdLogTask();

  // Der optionale ALMEMO-Rohlog schreibt erst danach auf die SD-Karte.
  // Dadurch kann er den normalen Messwert-CSV-Log nicht verdraengen.
  serialProtocolTraceStorageTask();

  // NEU: Abgleich der Teensy-Uhr mit der externen RTC nur alle 15 Minuten
  // 15 Minuten = 15 * 60 * 1000 Millisekunden = 900.000 ms
  static unsigned long rtcAbgleichTimer = 0;
  static bool ersterStart = true;

  if (ersterStart || (millis() - rtcAbgleichTimer >= 900000)) { 
    rtcAbgleichTimer = millis(); 
    ersterStart = false; 
    
    leseEchtzeitUhr(); // Holt die Zeit stressfrei von der externen RTC
  } 
}


  // =========================================================================
  // SCHRITT 4: DISPLAY REFRESH-KARUSSELL & NAVIGATION (10 ms Takt)
  // =========================================================================
  if (pollMetro.check()) {
    usb_read_serial();
 
    // SETUP-Button unten links abfragen.
    // Flanken-Latch: Der Einstieg wird pro Fingerberuehrung nur einmal ausgefuehrt.
    // Der Latch kann ausserdem beim Verlassen von Setup > Anzeige gesetzt werden,
    // weil ENTER und SETUP geometrisch dieselbe Touchflaeche benutzen.
    if (!TouchZ) {
      setup_touch_latch = false;
    }

    if (!flag.config_mode) {
      if (!ethernetWebSetupSessionActive()) {
        const bool setup_touch_now =
          TouchZ &&
          TouchX > 10 && TouchX < 150 &&
          TouchY > 380 && TouchY < 460;

        if (setup_touch_now && !setup_touch_latch) {
          setup_touch_latch = true;
          enterConfigModeFromTouch();
        }
      }
    }
  
    if (Menu_exit_timer == 1) eraseDisplay();
    if (Menu_exit_timer > 0) Menu_exit_timer--;
    
    if (flag.mode_change && !flag.mode_display) {
      flag.mode_change = false;
      if (flag.config_mode) eraseDisplay();
    }
    
    if (Menu_exit_timer == 0) {
      if (flag.config_mode) {
        menuTimeoutTask();
      }
      ads1263ServiceAdc2Background();

      if (flag.config_mode) {
        // Wenn wir im Config-Modus sind, zwingen wir das Update-Flag im Wechsel taktweise auf false, 
        // bis der erste Text steht! Das verhindert jede leere Box!
        if (frisch_geoeffnet) {
          flag.menu_lcd_upd = false;
        }
        ConfigMenu();
      } else {
        aktualisiereDisplayAnzeige();
      }
      ads1263ServiceAdc2Background();
    }

    // WICHTIG:
    // Im normalen Dashboard macht aktualisiereDisplayAnzeige() die Transfers selbst.
    // Deshalb hier KEINE zweite Dashboard-Transfer-Runde mehr.
    // Nur im Setup-Modus werden die Menü-Textboxen minimal übertragen.
    if (flag.config_mode && Menu_exit_timer == 0) {
      transferConfigTextBoxesSafe();
      ads1263ServiceAdc2Background();
    }
  }

  // Hardware-Watchdog nur hier fuettern: erst nachdem die komplette Hauptloop
  // wirklich sauber verlassen wurde. Download-/SD-/Ethernet-Pfade duerfen den
  // Watchdog niemals selbst fuettern. Jeder vollstaendige Loopdurchlauf triggert
  // wieder direkt das Watchdog-Register; es gibt keine 500-ms-Drossel mehr.
  if (tp3000LoopGuardLeave())
  {
    tp3000WatchdogFeed();
  }
}

