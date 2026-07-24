/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: ads1263.ino
 * Zweck: ADS1263-Messpfad fuer Pt100, Referenzen und Photodioden-ADC2.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

//
// ADS1263 / ADS1262-Auslesung für Taupunktspiegel
//
// Änderungen in dieser Version:
// - DRDY läuft über eigenen Interrupt-Zähler statt bool-Flag
// - keine verlorenen / mehrfach gezählten DRDY-Flanken durch stehenbleibendes Flag
// - PinModes werden in initADS1263() explizit gesetzt
// - State-Machine wartet wirklich auf echte neue Wandlungen
// - Hardware-Nullabgleich bleibt erhalten, läuft aber nach 5 Minuten nur,
//   wenn Temperatur und Peltier-Stellwert ca. 30 s ruhig waren
// - altes ADC-Kompatibilitaetsflag entfernt
// - V55: ADS-SPI-Takt im ADC-Info-Screen per Touch umschaltbar
// - V56: DRDY-Glitch-/Timeout-/Range-Zähler und ISR-Filter
// - V57: DRDY-Entprellung verschärft: 2000 us Mindestabstand, stabile Pegelprüfung vor RDATA, ISR/s + GLT/s Anzeige
// - V58: Hauptmessung nutzt DRDY-Interrupt nicht mehr als Tickquelle; Interrupt zählt nur Diagnose,
//        RDATA wartet aktiv/pollend auf stabil aktives DRDY-Fenster.
// - V60: Messblock-Timing fuer SINC4-Test, ID nur am Blockanfang, Dummy-ID vor RDATA.
// - V61: ADC-Info ist nur noch Live-Monitor. Hauptmesszyklus laeuft weiter.
//        10-s-Min/Max, Messzykluszeit, D/s und P/s fuer ADC-Kanaele.
// - ADuM/Direktleitung: keine TOSLINK-Preambles, keine Dummy-Frames.
//        ADS-Befehle laufen als komplette CS-Frames.
// - ADC2 fuer Photodiode: RDATA2-Kommando, 400 SPS, AVDD/AVSS-Ref, Gain 2, 64-Werte-Ringpuffer.

#include <SPI.h>
#include <ads1262.h>
#include <math.h>
#include <string.h>
#include "TP_T.h"

// Externe Pins aus deiner Hauptliste (main)
extern const byte pincs1;      // Pin 9
extern const byte pinDrdy;     // Pin 2
extern const byte pinStart;    // Pin 6
extern const byte pinRstAd;    // Pin 0
extern const byte pinCurrRev;  // Pin 22
extern uint8_t mode_display; // 3 = ADC-Info/Diagnose; Hauptmesskarussell dann pausieren

// ADS1263/ADS1262 auf SPI1
ADS1262 tAdc(pincs1, pinDrdy, pinStart, pinRstAd, &SPI1);

// Externe Variablen für Messergebnisse
extern volatile int32_t adcRawPt100_1;
extern volatile int32_t adcRawPt100_2;
extern volatile int32_t adcRawRef120R;
extern volatile int32_t adcRawRef100R;
extern volatile int32_t adcRawPhotodiode;
extern void setLedOffHardware(bool off);  // Pin 7 LED_OFF: HIGH = LED aus
extern double refCalGet100Ohm();
extern double refCalGet120Ohm();

// Für dynamische Nullung
extern float tempSpiegel;
extern float peltierSollWert;

// SPI-Einstellungen fuer ADS1263 auf SPI1
// V61: SPI-Takt bleibt im ADC-Info-Screen umschaltbar.
// Default ist der aktuelle stabile Teststand mit 1.5 MHz.
#define ADS1263_SPI_HZ_DEFAULT  2000000UL
#define ADS1263_CS_SETUP_US      5UL
#define ADS1263_CS_HOLD_US       5UL
#define ADS1263_TRANS_PAUSE_US   0UL
#define ADS1263_FRAME_GAP_US     0UL

// TOSLINK-Sync vor jedem echten ADS-Befehl:
// jeweils ein kompletter RREG-Frame: CS low -> 8 Byte -> CS high -> Gap.
#define ADS1263_TOSLINK_PREAMBLE_COUNT       0
#define ADS1263_TOSLINK_PREAMBLE_FIRST_REG   0x00
#define ADS1263_TOSLINK_PREAMBLE_REG_COUNT   6

// ADC2 Photodiode / Optik
// 400 SPS = ein neuer Wert alle 2500 us. Der Service liest ADC2 per RDATA2-Kommando
// zeitgesteuert und legt den Mittelwert in adcRawPhotodiode ab.
#define ADS1263_ADC2_ENABLE              1
#define ADS1263_ADC2_SAMPLE_INTERVAL_US  2500UL
#define ADS1263_ADC2_BUF_SIZE            32U
#define ADS1263_ADC2_DARK_SETTLE_MS      50UL
#define ADS1263_ADC2_DARK_SAMPLES        64U
// ADC2CFG: DR2=400 SPS (10b), REF2=AVDD/AVSS (100b), GAIN2=2 (001b) -> 0xA1
#define ADS1263_ADC2_CFG_VALUE           0xA1
// ADC2MUX: Photodiode/TIA P=AIN8, N=AIN9(GND) -> 0x89.
#define ADS1263_ADC2_MUX_VALUE           0x89

// ADuM/Direktverdrahtung: DRDY jetzt direkt vom ADS1263, nicht invertiert.
// ADS1263-DRDY ist aktiv LOW -> Interrupt auf FALLING.
#define ADS1263_CS_INVERTED     0
#define ADS1263_START_INVERTED  0
#define ADS1263_RESET_INVERTED  0
#define ADS1263_DRDY_INVERTED   0

static const uint32_t ads1263SpiRateTable[] = {
  50000UL,
  100000UL,
  200000UL,
  250000UL,
  300000UL,
  500000UL,
  1000000UL,
  1500000UL,
  2000000UL,
  3000000UL,
  4000000UL
};
static const uint8_t ADS1263_SPI_RATE_COUNT = sizeof(ads1263SpiRateTable) / sizeof(ads1263SpiRateTable[0]);

static uint8_t ads1263SpiRateIndex = 8;
static uint32_t ads1263SpiHz = ADS1263_SPI_HZ_DEFAULT;
SPISettings ads1263SpiSettings(ADS1263_SPI_HZ_DEFAULT, MSBFIRST, SPI_MODE1);
static uint8_t ads1263SpiModeAktiv = 1;

// ADS1263 REFMUX: externe Referenz an AIN0/AIN1 = RMUXP AIN0 + RMUXN AIN1
#define ADS1263_REFMUX_AIN0_AIN1 0x09

// Interne Temperaturmessung ADC1:
// Datenblatt: INPMUX = 0xBB, interne 2.5-V-Referenz, PGA aktiv, Gain=1, Chop aus.
// Achtung: Die ProtoCentral-Konstante ADS1262_TEMP_SENSOR_N zeigt auf 0x0C,
// fuer den ADS126x-Temperatursensor muss ADC1-INPMUX aber explizit 0xBB sein.
#define ADS1263_INPMUX_TEMP_SENSOR 0xBB
#define ADS1263_REFMUX_INTERNAL    0x00
#define ADS1263_INTERFACE_DATA4    0x00
#define ADS1263_POWER_INTREF_ON    0x01
#define ADS1263_MODE0_TEMP         0x00
#define ADS1263_MODE1_TEMP         0x00
#define ADS1263_MODE2_TEMP_400SPS  0x08

// Status
bool ads1263_bereit = false;

// State-Machine
uint8_t maintStep = 0;
uint16_t drdyCounter = 0;
uint8_t refKaskade = 0;

// DRDY-Zähler aus Interrupt
volatile uint16_t ads1263DrdyTicks = 0;

// V56 Diagnosezaehler fuer DRDY/Timing-Fehlersuche
#define ADS1263_DRDY_MIN_GAP_US       7000UL   // bei 100 SPS sind ca. 10000 us normal; <7000 us = Glitch
#define ADS1263_RAW_MIN_ABS_VALID     1000000L // Kleinkram wie 300/400/1000 verwerfen
// V58: DRDY-Interrupt ist nur noch Diagnose. Die State-Machine wird zeitgesteuert
// weitergeschaltet und vor RDATA wird DRDY stabil gepollt. So kann ein falscher
// Interrupt keine Messung mehr auslösen.
#define ADS1263_DRDY_INTERRUPT_TICKS_ENABLE 1
#define ADS1263_DRDY_SHARED_WITH_DOUT       0
// Test: separate direkte DRDY-Leitung an Pin 2, aktiv LOW -> fallende Flanke
#define ADS1263_DRDY_IRQ_FORCE_RISING       0
#define ADS1263_DRDY_POLL_TIMEOUT_US        30000UL
// V28 100-SPS-NOPOLL-Test:
// ADC1-Hauptmessung darf bei Timingproblemen nicht blockierend auf DRDY-Level warten.
// Die State-Machine gibt den Takt ueber IRQ-/Fallback-Ticks vor; RDATA wird dann direkt gelesen.
#define ADS1263_MAIN_RDATA_NOPOLL_TEST      1
// IRQ-Test: Wenn ein echter separater DRDY-Falling-Tick die State-Machine
// weitergeschaltet hat, wird vor RDATA keine zweite DRDY-Level-Wartepruefung
// mehr gemacht. Der DRDY-Puls ist kurz; spaeteres Level-Polling erzeugt sonst
// kuenstliche TD/TO-Fehler.
#define ADS1263_SKIP_LEVEL_POLL_AFTER_IRQ_TICK 1

volatile uint32_t ads1263DrdyIsrCount = 0;
volatile uint32_t ads1263DrdyGlitchCount = 0;
volatile uint32_t ads1263DrdyBusyIgnoreCount = 0;
volatile uint32_t ads1263DrdyLastGapUs = 0;
volatile uint32_t ads1263DrdyLastIsrUs = 0;
volatile bool ads1263SpiBusy = false;
static bool ads1263ReadTriggeredByDrdyIrq = false;

static uint32_t ads1263DrdyTimeoutCount = 0;
static uint32_t ads1263AdcRangeErrorCount = 0;
static int32_t ads1263LastRangeRaw = 0;

// Mess-Zwischenwerte
int32_t pt100_1_pos = 0;
int32_t pt100_2_pos = 0;
int32_t pt100_1_neg = 0;
int32_t pt100_2_neg = 0;

int32_t ref100r_pos_tmp = 0;
int32_t ref100r_neg_tmp = 0;
int32_t ref120r_pos_tmp = 0;
int32_t ref120r_neg_tmp = 0;

// V60: Referenzen nur als komplettes plausibles Ref Low/Ref High-Paar uebernehmen.
static int32_t ref100_candidate = 0;
static int32_t ref120_candidate = 0;
static bool ref100_candidate_valid = false;
static bool ref120_candidate_valid = false;

// Referenzpuffer
#define ADS1263_REF_BUF_SIZE 32U
// Maximal erlaubte Spruenge neuer Ref-Paare gegen den aktuellen Ref-Mittelwert.
// Einzelwerte Ref Low/Ref High duerfen etwas lockerer wandern (0,10 %),
// die wichtige Spanne DeltaRef = Ref High-Ref Low bleibt streng (0,02 %).
#define ADS1263_REF_ABS_JUMP_LIMIT_FRAC   0.0010   // 0,10 % fuer Ref Low/Ref High einzeln
#define ADS1263_REF_DELTA_JUMP_LIMIT_FRAC 0.0002   // 0,02 % fuer DeltaRef
#define ADS1263_REF_JUMP_MIN_SAMPLES ADS1263_REF_BUF_SIZE
#define ADS1263_REF_FILL_SEED_LIMIT_FRAC 0.002      // 0,20 % in der Einlaufphase
#define ADS1263_REF_OLD_MS             60000UL
#define ADS1263_REF_ERROR_MS          300000UL

#define ADS1263_REF_REJ_RANGE  0x01
#define ADS1263_REF_REJ_RATIO  0x02
#define ADS1263_REF_REJ_SEED   0x04
#define ADS1263_REF_REJ_J100   0x08
#define ADS1263_REF_REJ_J120   0x10
#define ADS1263_REF_REJ_JDELTA 0x20
int32_t puffer100R[ADS1263_REF_BUF_SIZE] = {0};
int32_t puffer120R[ADS1263_REF_BUF_SIZE] = {0};
uint8_t zeiger100R = 0;
uint8_t zeiger120R = 0;
static int32_t adsRefFillSeed100 = 0;
static int32_t adsRefFillSeed120 = 0;
static uint32_t adsRefLastAcceptedMs = 0;
static uint32_t adsRefAcceptedPairCount = 0;
static uint32_t adsRefRejectRangeCount = 0;
static uint32_t adsRefRejectRatioCount = 0;
static uint32_t adsRefRejectSeedCount = 0;
static uint32_t adsRefRejectJump100Count = 0;
static uint32_t adsRefRejectJump120Count = 0;
static uint32_t adsRefRejectJumpDeltaCount = 0;
static uint8_t adsRefLastRejectReason = 0;

static bool adsReadRawDirectSafe(int32_t* ziel, const char* tag);

// Timing
unsigned long letzteMittelWertZeit = 0;
unsigned long letzteNullAbgleichZeit = 0;
unsigned long letzterNullVersuchLog = 0;

// V0.50.0_5 TEST SYNC: Der zeitgesteuerte DRDY-Fallback darf nach
// MUX-/CurrRev-/START1-Neusynchronisation nicht sofort einen kompletten
// Settle-Zyklus vortaeuschen. Deshalb global, damit die Sync-Funktion
// den Fallback-Zeitpunkt mit zuruecksetzen kann.
static unsigned long ads1263LastFallbackTickMs = 0;

// V60 Test: Die State-Machine laeuft zeitgesteuert. Ein Fallback-Tick entspricht
// einer kompletten SINC4-Settlezeit von ca. 11 ms nach MUX-/CurrRev-Umschaltung.
#define ADS1263_SETTLE_TICKS_AFTER_MUX   5U

// =========================================================================
// ADAPTIVER PT100-MESSFILTER
// =========================================================================
// Normal:    ein ADC-Wert je Kanal/Polung wie bisher.
// Praezision: fuenf aufeinanderfolgende ADC-Werte je Kanal/Polung; Min/Max
//             werden verworfen, die mittleren drei gemittelt.
// Auto:      schaltet nach mehreren ruhigen Pt100-Saetzen automatisch auf
//             Praezision und bei deutlicher Dynamik wieder auf Normal.
#define ADS1263_PT_FILTER_TRIM_SAMPLES        5U
#define ADS1263_PT_FILTER_AUTO_STABLE_CYCLES  6U
#define ADS1263_PT_FILTER_AUTO_DELTA_RAW      30000L  // ca. 5...6 mK bei aktueller Skalierung

static uint8_t adsPtFilterSamplesTarget = 1U;
static int32_t adsPtFilterSamples[ADS1263_PT_FILTER_TRIM_SAMPLES] = {0};
static uint8_t adsPtFilterSampleCount = 0;
static bool adsPtAutoLastValid = false;
static int32_t adsPtAutoLastMir = 0;
static int32_t adsPtAutoLastUmg = 0;
static uint8_t adsPtAutoStableCycles = 0;

static uint8_t ads1263PtFilterSanitize(uint8_t mode)
{
  return (mode <= ADC_MEAS_FILTER_MAX) ? mode : ADC_MEAS_FILTER_DEFAULT;
}

static bool ads1263PtFilterUsePrecisionNow()
{
  const uint8_t mode = ads1263PtFilterSanitize(adcFilterModeGet());
  if (mode == ADC_MEAS_FILTER_PRECISION) return true;
  if (mode == ADC_MEAS_FILTER_NORMAL) return false;
  return adsPtAutoStableCycles >= ADS1263_PT_FILTER_AUTO_STABLE_CYCLES;
}

static void ads1263PtFilterStartBlock()
{
  adsPtFilterSamplesTarget = ads1263PtFilterUsePrecisionNow()
                           ? ADS1263_PT_FILTER_TRIM_SAMPLES
                           : 1U;
  adsPtFilterSampleCount = 0;
}

static void ads1263PtFilterResetAccumulator()
{
  adsPtFilterSampleCount = 0;
}

static int32_t ads1263PtFilterFinishValue()
{
  if (adsPtFilterSampleCount >= ADS1263_PT_FILTER_TRIM_SAMPLES)
  {
    int64_t sum = 0;
    int32_t minVal = adsPtFilterSamples[0];
    int32_t maxVal = adsPtFilterSamples[0];

    for (uint8_t i = 0; i < ADS1263_PT_FILTER_TRIM_SAMPLES; i++)
    {
      const int32_t v = adsPtFilterSamples[i];
      sum += (int64_t)v;
      if (v < minVal) minVal = v;
      if (v > maxVal) maxVal = v;
    }

    sum -= (int64_t)minVal;
    sum -= (int64_t)maxVal;
    return (int32_t)(sum / 3);
  }

  return adsPtFilterSamples[0];
}

// Rueckgabe: 0=warten/noch nicht fertig, 1=fertig, 2=Lesefehler
static uint8_t ads1263PtFilterReadStep(int32_t* out, const char* tag)
{
  const uint16_t requiredTicks = (adsPtFilterSampleCount == 0)
                               ? ADS1263_SETTLE_TICKS_AFTER_MUX
                               : 1U;
  if (drdyCounter < requiredTicks) return 0U;

  int32_t raw = 0;
  if (!adsReadRawDirectSafe(&raw, tag))
  {
    drdyCounter = 0;
    ads1263PtFilterResetAccumulator();
    return 2U;
  }

  if (adsPtFilterSampleCount < ADS1263_PT_FILTER_TRIM_SAMPLES)
  {
    adsPtFilterSamples[adsPtFilterSampleCount] = raw;
  }
  adsPtFilterSampleCount++;
  drdyCounter = 0;

  if (adsPtFilterSampleCount >= adsPtFilterSamplesTarget)
  {
    *out = ads1263PtFilterFinishValue();
    ads1263PtFilterResetAccumulator();
    return 1U;
  }

  return 0U;
}

static void ads1263PtFilterNoteAccepted(int32_t ptMir, int32_t ptUmg)
{
  if (!adsPtAutoLastValid)
  {
    adsPtAutoLastValid = true;
    adsPtAutoLastMir = ptMir;
    adsPtAutoLastUmg = ptUmg;
    adsPtAutoStableCycles = 0;
    return;
  }

  int64_t dMir = (int64_t)ptMir - (int64_t)adsPtAutoLastMir;
  int64_t dUmg = (int64_t)ptUmg - (int64_t)adsPtAutoLastUmg;
  if (dMir < 0) dMir = -dMir;
  if (dUmg < 0) dUmg = -dUmg;

  if (dMir <= ADS1263_PT_FILTER_AUTO_DELTA_RAW &&
      dUmg <= ADS1263_PT_FILTER_AUTO_DELTA_RAW)
  {
    if (adsPtAutoStableCycles < 255U) adsPtAutoStableCycles++;
  }
  else
  {
    adsPtAutoStableCycles = 0;
  }

  adsPtAutoLastMir = ptMir;
  adsPtAutoLastUmg = ptUmg;
}


// =========================================================================
// ADS1263 INFO-SCREEN PUFFER
// =========================================================================
#define ADS1263_INFO_ROWS 6

static DMAMEM char ads1263InfoCmd[ADS1263_INFO_ROWS][22];
static DMAMEM char ads1263InfoAns[ADS1263_INFO_ROWS][68];
static DMAMEM char ads1263InfoStatus[34] = "INIT";
static bool ads1263InfoOk = false;

// Hauptbetrieb-Linkschutz V60:
// Nicht mehr vor jedem Einzelwert 2x ID lesen. Das hat den Messablauf zerhackt
// und kostet zu viel Zeit. Jetzt nur am Anfang eines Messblocks:
// ID1 = Flush/Wakeup, ID2 muss exakt 0x23 sein; max. 3 Versuche.
// Vor jedem RDATA gibt es nur noch eine Dummy-ID als TOSLINK-Wakeup, Antwort egal.
#define ADS1263_VALUE_ID_RETRIES 3
#define ADS1263_EXPECTED_ID      0x23
// Ref-Raw-Ratio muss zur eingestellten Ref-Low-/Ref-High-Basis passen.
// Fallback nur fuer den Fall unplausibler EEPROM-/Kalibrierwerte.
#define ADS1263_REF_RATIO_FALLBACK_MIN       1.04
#define ADS1263_REF_RATIO_FALLBACK_MAX       2.60
#define ADS1263_REF_RATIO_EXPECTED_TOL_FRAC  0.01   // +/-1 % um RefHigh/RefLow
#define ADS1263_REF_RATIO_ABS_MIN            1.02
#define ADS1263_REF_RATIO_ABS_MAX            2.70

// V49 Testmodus:
// - Start-Selbsttest blockiert den Hauptbetrieb nicht mehr.
// - Wenn DRDY am Teensy noch nicht sicher ankommt, erzeugt der Hauptbetrieb
//   testweise alle paar Millisekunden eigene "Pseudo-DRDY"-Ticks.
//   So koennen wir pruefen, ob der Hauptschirm ueberhaupt echte RDATA-Werte bekommt.
#define ADS1263_START_FORCE_READY_TEST      1
#define ADS1263_TIMED_DRDY_FALLBACK_TEST   1
#define ADS1263_TIMED_DRDY_FALLBACK_MS     120UL
// V50 Test: Hauptbetrieb ohne DRDY-Abhaengigkeit stabilisieren.
// ADC1 wird mit START1 (0x08) gestartet, SFOCAL wird vorerst uebersprungen,
// MODE2 bleibt wie beim erfolgreichen Temp-Test auf 0x08 (Gain 1 / 400 SPS).
#define ADS1263_SKIP_SFOCAL_TEST        0
// Dynamischer SFOCAL ist nicht mehr fest auf 5 min verdrahtet, sondern wird
// ueber Setup -> Regelparameter -> ADC1 SFOCAL gesteuert.
#define ADS1263_DYNAMIC_SFOCAL_ENABLE    1
#define ADS1263_MAIN_MODE2_FAST_TEST    0x57   // Gain 32 / 100 SPS fuer ruhigen ADC1-Test

// V53 Test: Ref Low/Ref High-Kanalzuordnung getauscht.
// V52 hatte die Polaritaet korrigiert, aber Ref100 bekam den groesseren Rohwert.
// Jetzt soll Ref100 den kleineren Rohwert und Ref120 den groesseren Rohwert bekommen.
// V51 Test: Wenn der ADC-Info-Screen aktiv ist, darf der normale Haupt-ADC-Pfad
// den ADS1263 nicht parallel umkonfigurieren. Sonst ueberschreiben sich
// INPMUX/REFMUX/MODE2 gegenseitig und der Temp-Test verschwindet.
#define ADS1263_PAUSE_MAIN_WHEN_ADC_INFO 0

static uint8_t ads1263LastValueId1 = 0;
static uint8_t ads1263LastValueId2 = 0;
static uint16_t ads1263ValueIdOkCount = 0;
static uint16_t ads1263ValueIdErrCount = 0;
static bool ads1263LastRdataOk = false;

static void ads1263InfoClear(const char* status, bool ok)
{
  for (uint8_t i = 0; i < ADS1263_INFO_ROWS; i++)
  {
    ads1263InfoCmd[i][0] = '\0';
    ads1263InfoAns[i][0] = '\0';
  }

  snprintf(ads1263InfoStatus, sizeof(ads1263InfoStatus), "%s", status != nullptr ? status : "");
  ads1263InfoOk = ok;
}

static void ads1263InfoStatusSet(const char* status, bool ok)
{
  snprintf(ads1263InfoStatus, sizeof(ads1263InfoStatus), "%s", status != nullptr ? status : "");
  ads1263InfoOk = ok;
}

static void ads1263InfoSet(uint8_t row, const char* cmd, const char* ans)
{
  if (row >= ADS1263_INFO_ROWS) return;

  snprintf(ads1263InfoCmd[row], sizeof(ads1263InfoCmd[row]), "%s", cmd != nullptr ? cmd : "");
  snprintf(ads1263InfoAns[row], sizeof(ads1263InfoAns[row]), "%s", ans != nullptr ? ans : "");
}

uint8_t ads1263InfoGetRows()
{
  return ADS1263_INFO_ROWS;
}

const char* ads1263InfoGetCmd(uint8_t row)
{
  if (row >= ADS1263_INFO_ROWS) return "";
  return ads1263InfoCmd[row];
}

const char* ads1263InfoGetAns(uint8_t row)
{
  if (row >= ADS1263_INFO_ROWS) return "";
  return ads1263InfoAns[row];
}

const char* ads1263InfoGetStatus()
{
  return ads1263InfoStatus;
}

bool ads1263InfoGetOk()
{
  return ads1263InfoOk;
}


// =========================================================================
// V61 ADC-LIVE-MONITOR / 10-s-STATISTIK
// =========================================================================
// ADC-Info liest den ADS nicht mehr selbst. Diese Statistik wird nur vom
// normalen Hauptmesszyklus gefuellt und vom Display abgefragt.

enum
{
  ADS1263_STAT_MIR = 0,
  ADS1263_STAT_UMG = 1,
  ADS1263_STAT_R100 = 2,
  ADS1263_STAT_R120 = 3,
  ADS1263_STAT_CH_COUNT = 4
};

enum
{
  ADS1263_PAIR_MIR = 0,
  ADS1263_PAIR_UMG = 1,
  ADS1263_PAIR_REF = 2,
  ADS1263_PAIR_COUNT = 3
};

static uint32_t adsStatWindowStartMs = 0;
static int32_t adsStatAdcMin[ADS1263_STAT_CH_COUNT] = {0};
static int32_t adsStatAdcMax[ADS1263_STAT_CH_COUNT] = {0};
static bool adsStatAdcValid[ADS1263_STAT_CH_COUNT] = {false};
static uint32_t adsStatDiscardRaw[ADS1263_STAT_CH_COUNT] = {0};
static uint32_t adsStatDiscardPair[ADS1263_PAIR_COUNT] = {0};
static uint32_t adsStatIdDiscard = 0;
static uint32_t adsStatTimeoutDiscard = 0;
static uint32_t adsStatRngCount = 0;
static uint32_t adsStatCycles = 0;
static uint32_t adsStatValidCycles = 0;
static uint32_t adsStatMaxCycleUs = 0;
static uint32_t adsStatCycleStartUs = 0;
static bool adsStatCyclePtValid = false;
static float adsStatTempMin[2] = {0.0f, 0.0f};
static float adsStatTempMax[2] = {0.0f, 0.0f};
static bool adsStatTempValid[2] = {false, false};
static float adsStatRatioMin = 0.0f;
static float adsStatRatioMax = 0.0f;
static bool adsStatRatioValid = false;

// ADC2 / Photodiode Statistik und Ringpuffer
static int32_t ads1263Adc2Buf[ADS1263_ADC2_BUF_SIZE] = {0};
static uint8_t ads1263Adc2Index = 0;
static uint8_t ads1263Adc2Count = 0;
static int64_t ads1263Adc2Sum = 0;
static int32_t ads1263Adc2LastRaw = 0;
static int32_t ads1263Adc2Avg = 0;
static uint32_t ads1263Adc2LastSampleUs = 0;
static uint32_t ads1263Adc2WindowStartMs = 0;
static uint32_t ads1263Adc2WindowSamples = 0;
static int32_t ads1263Adc2Min = 0;
static int32_t ads1263Adc2Max = 0;
static bool ads1263Adc2MinMaxValid = false;
static int32_t ads1263Adc2DarkRaw = 0;
static bool ads1263Adc2DarkValidFlag = false;
static bool ads1263Adc2DarkMeasuringFlag = false;
static uint32_t ads1263Adc2DarkLastMs = 0;

static int32_t ads1263Adc2GetNetInternal()
{
  return ads1263Adc2DarkValidFlag ? (ads1263Adc2Avg - ads1263Adc2DarkRaw) : ads1263Adc2Avg;
}

static void ads1263Adc2ResetStats(uint32_t nowMs)
{
  ads1263Adc2WindowStartMs = nowMs;
  ads1263Adc2WindowSamples = 0;
  ads1263Adc2Min = 0;
  ads1263Adc2Max = 0;
  ads1263Adc2MinMaxValid = false;
}

static void ads1263Adc2TouchWindow()
{
  uint32_t nowMs = millis();
  if (ads1263Adc2WindowStartMs == 0 || (uint32_t)(nowMs - ads1263Adc2WindowStartMs) >= 10000UL)
  {
    ads1263Adc2ResetStats(nowMs);
  }
}

static void ads1263Adc2NoteSample(int32_t raw)
{
  ads1263Adc2TouchWindow();
  ads1263Adc2WindowSamples++;

  if (!ads1263Adc2MinMaxValid)
  {
    ads1263Adc2Min = raw;
    ads1263Adc2Max = raw;
    ads1263Adc2MinMaxValid = true;
  }
  else
  {
    if (raw < ads1263Adc2Min) ads1263Adc2Min = raw;
    if (raw > ads1263Adc2Max) ads1263Adc2Max = raw;
  }

  if (ads1263Adc2Count < ADS1263_ADC2_BUF_SIZE)
  {
    ads1263Adc2Buf[ads1263Adc2Index] = raw;
    ads1263Adc2Sum += raw;
    ads1263Adc2Count++;
  }
  else
  {
    ads1263Adc2Sum -= ads1263Adc2Buf[ads1263Adc2Index];
    ads1263Adc2Buf[ads1263Adc2Index] = raw;
    ads1263Adc2Sum += raw;
  }

  ads1263Adc2Index++;
  if (ads1263Adc2Index >= ADS1263_ADC2_BUF_SIZE) ads1263Adc2Index = 0;

  ads1263Adc2LastRaw = raw;
  if (ads1263Adc2Count > 0)
  {
    ads1263Adc2Avg = (int32_t)(ads1263Adc2Sum / (int64_t)ads1263Adc2Count);
    adcRawPhotodiode = ads1263Adc2GetNetInternal();
  }
}

static void ads1263Adc2ResetBuffer()
{
  for (uint8_t i = 0; i < ADS1263_ADC2_BUF_SIZE; i++) ads1263Adc2Buf[i] = 0;
  ads1263Adc2Index = 0;
  ads1263Adc2Count = 0;
  ads1263Adc2Sum = 0;
  ads1263Adc2LastRaw = 0;
  ads1263Adc2Avg = 0;
  ads1263Adc2LastSampleUs = micros();
  adcRawPhotodiode = 0;
  ads1263Adc2ResetStats(millis());
}

static float ads1263Adc2Seconds()
{
  uint32_t nowMs = millis();
  uint32_t elapsed = (ads1263Adc2WindowStartMs == 0) ? 0 : (uint32_t)(nowMs - ads1263Adc2WindowStartMs);
  if (elapsed < 1000UL) elapsed = 1000UL;
  if (elapsed > 10000UL) elapsed = 10000UL;
  return (float)elapsed / 1000.0f;
}

static void ads1263StatsResetWindow(uint32_t nowMs)
{
  adsStatWindowStartMs = nowMs;
  for (uint8_t i = 0; i < ADS1263_STAT_CH_COUNT; i++)
  {
    adsStatAdcMin[i] = 0;
    adsStatAdcMax[i] = 0;
    adsStatAdcValid[i] = false;
    adsStatDiscardRaw[i] = 0;
  }
  for (uint8_t i = 0; i < ADS1263_PAIR_COUNT; i++)
  {
    adsStatDiscardPair[i] = 0;
  }
  adsStatIdDiscard = 0;
  adsStatTimeoutDiscard = 0;
  adsStatRngCount = 0;
  adsStatCycles = 0;
  adsStatValidCycles = 0;
  adsStatMaxCycleUs = 0;
  for (uint8_t i = 0; i < 2; i++)
  {
    adsStatTempMin[i] = 0.0f;
    adsStatTempMax[i] = 0.0f;
    adsStatTempValid[i] = false;
  }
  adsStatRatioMin = 0.0f;
  adsStatRatioMax = 0.0f;
  adsStatRatioValid = false;
}

static void ads1263StatsTouchWindow()
{
  uint32_t nowMs = millis();
  if (adsStatWindowStartMs == 0 || (uint32_t)(nowMs - adsStatWindowStartMs) >= 10000UL)
  {
    ads1263StatsResetWindow(nowMs);
  }
}

static void ads1263StatsNoteAdc(uint8_t ch, int32_t raw)
{
  if (ch >= ADS1263_STAT_CH_COUNT) return;
  ads1263StatsTouchWindow();
  if (!adsStatAdcValid[ch])
  {
    adsStatAdcMin[ch] = raw;
    adsStatAdcMax[ch] = raw;
    adsStatAdcValid[ch] = true;
  }
  else
  {
    if (raw < adsStatAdcMin[ch]) adsStatAdcMin[ch] = raw;
    if (raw > adsStatAdcMax[ch]) adsStatAdcMax[ch] = raw;
  }
}

static void ads1263StatsNoteRawDiscard(uint8_t ch)
{
  if (ch >= ADS1263_STAT_CH_COUNT) return;
  ads1263StatsTouchWindow();
  adsStatDiscardRaw[ch]++;
}

static void ads1263StatsNotePairDiscard(uint8_t pair)
{
  if (pair >= ADS1263_PAIR_COUNT) return;
  ads1263StatsTouchWindow();
  adsStatDiscardPair[pair]++;
}

static void ads1263StatsNoteIdDiscard()
{
  ads1263StatsTouchWindow();
  adsStatIdDiscard++;
}

static void ads1263StatsNoteTimeoutDiscard()
{
  ads1263StatsTouchWindow();
  adsStatTimeoutDiscard++;
}

static void ads1263StatsNoteRatio(float ratio)
{
  if (!isfinite(ratio)) return;
  ads1263StatsTouchWindow();
  if (!adsStatRatioValid)
  {
    adsStatRatioMin = ratio;
    adsStatRatioMax = ratio;
    adsStatRatioValid = true;
  }
  else
  {
    if (ratio < adsStatRatioMin) adsStatRatioMin = ratio;
    if (ratio > adsStatRatioMax) adsStatRatioMax = ratio;
  }
}

static void ads1263StatsCycleStart()
{
  ads1263StatsTouchWindow();
  adsStatCycles++;
  adsStatCycleStartUs = micros();
  adsStatCyclePtValid = false;
}

static void ads1263StatsCycleEnd(bool valid)
{
  ads1263StatsTouchWindow();
  if (valid) adsStatValidCycles++;
  if (adsStatCycleStartUs != 0)
  {
    uint32_t dt = (uint32_t)(micros() - adsStatCycleStartUs);
    if (dt > adsStatMaxCycleUs) adsStatMaxCycleUs = dt;
  }
  adsStatCycleStartUs = 0;
}

static float ads1263StatsSeconds()
{
  uint32_t nowMs = millis();
  uint32_t elapsed = (adsStatWindowStartMs == 0) ? 0 : (uint32_t)(nowMs - adsStatWindowStartMs);
  if (elapsed < 1000UL) elapsed = 1000UL;
  if (elapsed > 10000UL) elapsed = 10000UL;
  return (float)elapsed / 1000.0f;
}

static float ads1263StatsRate(uint32_t count)
{
  return (float)count / ads1263StatsSeconds();
}

// Display-Getter
uint32_t ads1263StatsWindowMs() { return (adsStatWindowStartMs == 0) ? 0 : (uint32_t)(millis() - adsStatWindowStartMs); }
float ads1263StatsCycleRate() { return ads1263StatsRate(adsStatCycles); }
float ads1263StatsValidRate() { return ads1263StatsRate(adsStatValidCycles); }
float ads1263StatsMaxCycleMs() { return (float)adsStatMaxCycleUs / 1000.0f; }
float ads1263StatsIdRate() { return ads1263StatsRate(adsStatIdDiscard); }
float ads1263StatsTimeoutRate() { return ads1263StatsRate(adsStatTimeoutDiscard); }
float ads1263StatsRngRate() { return ads1263StatsRate(adsStatRngCount); }
float ads1263StatsRawDiscardRate(uint8_t ch) { return (ch < ADS1263_STAT_CH_COUNT) ? ads1263StatsRate(adsStatDiscardRaw[ch]) : 0.0f; }
float ads1263StatsPairDiscardRate(uint8_t pair) { return (pair < ADS1263_PAIR_COUNT) ? ads1263StatsRate(adsStatDiscardPair[pair]) : 0.0f; }
bool ads1263StatsAdcMinMax(uint8_t ch, int32_t* minOut, int32_t* maxOut)
{
  if (ch >= ADS1263_STAT_CH_COUNT || !adsStatAdcValid[ch]) return false;
  if (minOut) *minOut = adsStatAdcMin[ch];
  if (maxOut) *maxOut = adsStatAdcMax[ch];
  return true;
}
bool ads1263StatsTempMinMax(uint8_t ch, float* minOut, float* maxOut)
{
  if (ch >= 2 || !adsStatTempValid[ch]) return false;
  if (minOut) *minOut = adsStatTempMin[ch];
  if (maxOut) *maxOut = adsStatTempMax[ch];
  return true;
}
bool ads1263StatsRatioMinMax(float* minOut, float* maxOut)
{
  if (!adsStatRatioValid) return false;
  if (minOut) *minOut = adsStatRatioMin;
  if (maxOut) *maxOut = adsStatRatioMax;
  return true;
}
uint32_t ads1263StatsDrdyIsrCount() { noInterrupts(); uint32_t v = ads1263DrdyIsrCount; interrupts(); return v; }
uint32_t ads1263StatsDrdyGlitchCount() { noInterrupts(); uint32_t v = ads1263DrdyGlitchCount; interrupts(); return v; }
uint32_t ads1263StatsDrdyBusyCount() { noInterrupts(); uint32_t v = ads1263DrdyBusyIgnoreCount; interrupts(); return v; }
uint32_t ads1263StatsDrdyGapUs() { noInterrupts(); uint32_t v = ads1263DrdyLastGapUs; interrupts(); return v; }
uint32_t ads1263GetSettleMs() { return ADS1263_TIMED_DRDY_FALLBACK_MS; }
const char* ads1263GetGainLabel() { return "G32"; }

int32_t ads1263Adc2GetRaw() { return ads1263Adc2LastRaw; }
int32_t ads1263Adc2GetAvg() { return ads1263Adc2Avg; }
int32_t ads1263Adc2GetDark() { return ads1263Adc2DarkRaw; }
int32_t ads1263Adc2GetNet() { return ads1263Adc2GetNetInternal(); }
bool ads1263Adc2DarkValid() { return ads1263Adc2DarkValidFlag; }
bool ads1263Adc2DarkMeasuring() { return ads1263Adc2DarkMeasuringFlag; }
uint32_t ads1263Adc2DarkAgeMs() { return ads1263Adc2DarkValidFlag ? (uint32_t)(millis() - ads1263Adc2DarkLastMs) : 0; }
uint8_t ads1263Adc2GetBufCount() { return ads1263Adc2Count; }
uint16_t ads1263Adc2GetBufSize() { return ADS1263_ADC2_BUF_SIZE; }
float ads1263Adc2GetRate() { return (float)ads1263Adc2WindowSamples / ads1263Adc2Seconds(); }
bool ads1263Adc2GetMinMax(int32_t* minOut, int32_t* maxOut)
{
  if (!ads1263Adc2MinMaxValid) return false;
  if (minOut) *minOut = ads1263Adc2Min;
  if (maxOut) *maxOut = ads1263Adc2Max;
  return true;
}

void ads1263StatsNoteTemp(float tMirror, float tAmbient)
{
  ads1263StatsTouchWindow();
  float vals[2] = {tMirror, tAmbient};
  for (uint8_t i = 0; i < 2; i++)
  {
    if (!isfinite(vals[i])) continue;
    if (!adsStatTempValid[i])
    {
      adsStatTempMin[i] = vals[i];
      adsStatTempMax[i] = vals[i];
      adsStatTempValid[i] = true;
    }
    else
    {
      if (vals[i] < adsStatTempMin[i]) adsStatTempMin[i] = vals[i];
      if (vals[i] > adsStatTempMax[i]) adsStatTempMax[i] = vals[i];
    }
  }
}

static uint8_t adsTagToStatChannel(const char* tag)
{
  if (tag == nullptr) return ADS1263_STAT_MIR;
  if (strstr(tag, "Umgebung") != nullptr || strstr(tag, "PT100_2") != nullptr) return ADS1263_STAT_UMG;
  if (strstr(tag, "REF100") != nullptr) return ADS1263_STAT_R100;
  if (strstr(tag, "REF120") != nullptr) return ADS1263_STAT_R120;
  return ADS1263_STAT_MIR;
}


// =========================================================================
// DRDY-INTERRUPT
// =========================================================================
// WICHTIG:
// Keine SPI-Zugriffe, kein Serial, kein delay in der ISR.
void ads1263_drdy_counter_isr()
{
  uint32_t now = micros();
  ads1263DrdyIsrCount++;

#if ADS1263_DRDY_SHARED_WITH_DOUT
  // Nur bei DOUT/DRDY-Doppelnutzung: waehrend SPI entstehen Datenflanken.
  // Bei separater DRDY-Leitung NICHT sperren, sonst verlieren wir echte DRDY-Ticks.
  if (ads1263SpiBusy)
  {
    ads1263DrdyBusyIgnoreCount++;
    ads1263DrdyGlitchCount++;
    return;
  }
#else
  (void)ads1263SpiBusy;
#endif

  if (ads1263DrdyLastIsrUs != 0)
  {
    uint32_t gap = (uint32_t)(now - ads1263DrdyLastIsrUs);
    ads1263DrdyLastGapUs = gap;

    // Bei 400 SPS sind echte DRDY-Flanken etwa 2500 us auseinander.
    // V57: alles unter 2000 us werten wir sicher als Glitch und setzen kein Tick-Flag.
    if (gap < ADS1263_DRDY_MIN_GAP_US)
    {
      ads1263DrdyGlitchCount++;
      return;
    }
  }

  ads1263DrdyLastIsrUs = now;

#if ADS1263_DRDY_INTERRUPT_TICKS_ENABLE
  if (ads1263DrdyTicks < 60000)
  {
    ads1263DrdyTicks++;
  }
#endif
}


// =========================================================================
// DRDY-TICKS ATOMAR ABHOLEN
// =========================================================================
static uint16_t holeDrdyTicks()
{
  uint16_t ticks = 0;

  noInterrupts();
  ticks = ads1263DrdyTicks;
  ads1263DrdyTicks = 0;
  interrupts();

  return ticks;
}


// =========================================================================
// DRDY-TICKS LÖSCHEN
// =========================================================================
static void loescheDrdyTicks()
{
  noInterrupts();
  ads1263DrdyTicks = 0;
  interrupts();
}

static inline void adsSpiBusySet(bool busy)
{
  ads1263SpiBusy = busy;
}

static void ads1263DiagCounterReset()
{
  noInterrupts();
  ads1263DrdyIsrCount = 0;
  ads1263DrdyGlitchCount = 0;
  ads1263DrdyBusyIgnoreCount = 0;
  ads1263DrdyLastGapUs = 0;
  ads1263DrdyLastIsrUs = 0;
  ads1263DrdyTicks = 0;
  interrupts();

  ads1263DrdyTimeoutCount = 0;
  ads1263AdcRangeErrorCount = 0;
  ads1263LastRangeRaw = 0;
  ads1263StatsResetWindow(millis());
}

static void ads1263DiagCounterSnapshot(uint32_t* isr, uint32_t* glt, uint32_t* busy, uint32_t* gap)
{
  noInterrupts();
  if (isr != nullptr)  *isr  = ads1263DrdyIsrCount;
  if (glt != nullptr)  *glt  = ads1263DrdyGlitchCount;
  if (busy != nullptr) *busy = ads1263DrdyBusyIgnoreCount;
  if (gap != nullptr)  *gap  = ads1263DrdyLastGapUs;
  interrupts();
}


// ADC2-Service wird auch aus ADC1-Wartezeiten und aus der Hauptloop aufgerufen.
static void ads1263ServiceAdc2();
void ads1263ServiceAdc2Background();

// =========================================================================
// ADS1263 HARDWARE-ABSTRAKTION FUER INVERTIERTE LWL-SIGNALE
// =========================================================================
static inline void adsCsSet(bool active)
{
#if ADS1263_CS_INVERTED
  digitalWriteFast(pincs1, active ? HIGH : LOW);
#else
  digitalWriteFast(pincs1, active ? LOW : HIGH);
#endif
}

static inline void adsStartSet(bool run)
{
#if ADS1263_START_INVERTED
  digitalWriteFast(pinStart, run ? LOW : HIGH);
#else
  digitalWriteFast(pinStart, run ? HIGH : LOW);
#endif
}

static inline void adsResetSet(bool released)
{
  // released=true  -> ADS RESET/PWDN am Chip HIGH
  // released=false -> ADS RESET/PWDN am Chip LOW
#if ADS1263_RESET_INVERTED
  digitalWriteFast(pinRstAd, released ? LOW : HIGH);
#else
  digitalWriteFast(pinRstAd, released ? HIGH : LOW);
#endif
}

static inline bool adsDrdyAktiv()
{
#if ADS1263_DRDY_INVERTED
  return digitalReadFast(pinDrdy) == HIGH;
#else
  return digitalReadFast(pinDrdy) == LOW;
#endif
}

static bool adsDrdyAktivStabil()
{
  // V57: DRDY vor dem Lesen mehrfach prüfen.
  // Bei direkter ADS1263-DRDY-Leitung ist logisch AKTIV = Teensy-Pin LOW.
  // Kurze Glitches sollen keinen RDATA-Transfer ausloesen.
  if (!adsDrdyAktiv()) return false;
  delayMicroseconds(5);
  if (!adsDrdyAktiv()) return false;
  delayMicroseconds(5);
  if (!adsDrdyAktiv()) return false;
  return true;
}

static bool adsWaitDrdyAktivStabil(uint32_t timeoutUs)
{
  uint32_t t0 = micros();

  while ((uint32_t)(micros() - t0) < timeoutUs)
  {
    // ADC2 nutzt die Wartezeit auf ADC1-DRDY.
    // Der Service ist zeitgesteuert und liest nur, wenn ein 400-SPS-Slot faellig ist.
    ads1263ServiceAdc2();

    if (adsDrdyAktivStabil())
    {
      // Kleines Sicherheitsfenster nach bestaetigtem DRDY.
      delayMicroseconds(10);
      return true;
    }
    yield();
  }

  ads1263DrdyTimeoutCount++;
  ads1263StatsNoteTimeoutDiscard();
  return false;
}

static bool adsDrdyReadyBeforeRead(const char* tag)
{
#if ADS1263_MAIN_RDATA_NOPOLL_TEST
  // Testmodus fuer 100 SPS: kein blockierendes DRDY-Level-Polling vor RDATA.
  // Damit kann ein verlorener/kurzer DRDY-Puls nicht die Hauptloop und ADC2 ausbremsen.
  (void)tag;
  return true;
#endif

#if ADS1263_SKIP_LEVEL_POLL_AFTER_IRQ_TICK
  // Bei separatem DRDY-IRQ ist die FALLING-Flanke selbst der gueltige
  // ADC1-Ready-Zeitpunkt. Danach ist DRDY eventuell schon wieder high.
  // Deshalb im IRQ-getakteten Pfad kein zweites Level-Polling vor RDATA.
  if (ADS1263_DRDY_INTERRUPT_TICKS_ENABLE && !ADS1263_DRDY_SHARED_WITH_DOUT && ads1263ReadTriggeredByDrdyIrq)
  {
    return true;
  }
#endif

  // Polling-/Fallback-Pfad: hier bleibt die stabile DRDY-Level-Pruefung aktiv.
  if (!adsWaitDrdyAktivStabil(ADS1263_DRDY_POLL_TIMEOUT_US))
  {
    ads1263DrdyGlitchCount++;
    ads1263LastRdataOk = false;

    Serial.print("ADS1263: RDATA verworfen (");
    Serial.print(tag != nullptr ? tag : "RDATA");
    Serial.println(") - kein stabil aktives DRDY-Fenster.");
    return false;
  }

  return true;
}

static inline int adsDrdyInterruptMode()
{
#if ADS1263_DRDY_IRQ_FORCE_RISING
  return RISING;
#else
  #if ADS1263_DRDY_INVERTED
    return RISING;
  #else
    return FALLING;
  #endif
#endif
}

static void adsPrintPinDebug(const char* phase)
{
  Serial.print("ADS1263 DEBUG ");
  Serial.print(phase);
  Serial.print(": Teensy CS=");
  Serial.print(digitalReadFast(pincs1));
  Serial.print(" START=");
  Serial.print(digitalReadFast(pinStart));
  Serial.print(" RESET=");
  Serial.print(digitalReadFast(pinRstAd));
  Serial.print(" DRDY=");
  Serial.print(digitalReadFast(pinDrdy));
  Serial.print(" | DRDY logisch=");
  Serial.println(adsDrdyAktiv() ? "AKTIV" : "inaktiv");
}

static void adsRebuildSpiSettings()
{
  switch (ads1263SpiModeAktiv)
  {
    case 0: ads1263SpiSettings = SPISettings(ads1263SpiHz, MSBFIRST, SPI_MODE0); break;
    case 1: ads1263SpiSettings = SPISettings(ads1263SpiHz, MSBFIRST, SPI_MODE1); break;
    case 2: ads1263SpiSettings = SPISettings(ads1263SpiHz, MSBFIRST, SPI_MODE2); break;
    case 3: ads1263SpiSettings = SPISettings(ads1263SpiHz, MSBFIRST, SPI_MODE3); break;
    default:
      ads1263SpiModeAktiv = 1;
      ads1263SpiSettings = SPISettings(ads1263SpiHz, MSBFIRST, SPI_MODE1);
      break;
  }
}

static void adsSetSpiMode(uint8_t mode)
{
  ads1263SpiModeAktiv = (mode > 3) ? 1 : mode;
  adsRebuildSpiSettings();
}

static void adsSetSpiRateIndex(uint8_t index)
{
  if (index >= ADS1263_SPI_RATE_COUNT) index = 0;

  ads1263SpiRateIndex = index;
  ads1263SpiHz = ads1263SpiRateTable[ads1263SpiRateIndex];
  adsRebuildSpiSettings();

  Serial.print("ADS1263 SPI-Takt: ");
  Serial.print(ads1263SpiHz);
  Serial.println(" Hz");
}

uint32_t ads1263SpiGetHz()
{
  return ads1263SpiHz;
}

const char* ads1263SpiGetLabel()
{
  switch (ads1263SpiHz)
  {
    case 50000UL:   return "50k";
    case 100000UL:  return "100k";
    case 200000UL:  return "200k";
    case 250000UL:  return "250k";
    case 300000UL:  return "300k";
    case 500000UL:  return "500k";
    case 1000000UL: return "1M";
    case 1500000UL: return "1.5M";
    case 2000000UL: return "2M";
    case 3000000UL: return "3M";
    case 4000000UL: return "4M";
    default:        return "?";
  }
}

void ads1263SpiNextRate()
{
  uint8_t next = ads1263SpiRateIndex + 1;
  if (next >= ADS1263_SPI_RATE_COUNT) next = 0;
  adsSetSpiRateIndex(next);

  // Nach Taktwechsel den naechsten ADC-Info-Durchlauf eindeutig neu starten.
  ads1263ValueIdOkCount = 0;
  ads1263ValueIdErrCount = 0;
  ads1263DiagCounterReset();
}

void ads1263SpiPrevRate()
{
  uint8_t prev = (ads1263SpiRateIndex == 0) ? (ADS1263_SPI_RATE_COUNT - 1) : (ads1263SpiRateIndex - 1);
  adsSetSpiRateIndex(prev);

  ads1263ValueIdOkCount = 0;
  ads1263ValueIdErrCount = 0;
  ads1263DiagCounterReset();
}

static uint8_t adsReadRegister(uint8_t reg);
static void adsPrintHexByte(uint8_t value);

static bool adsIdPlausibel(uint8_t id)
{
  // ADS1263 ID-Register: Bit 7..5 = DEV_ID.
  // Am Logic-Analyzer war ID=0x23 plausibel sichtbar: 001xxxxx = ADS1263-Familie.
  return ((id & 0xE0) == 0x20);
}

static bool adsLinkCheckAtBlockStart(const char* tag)
{
  for (uint8_t versuch = 0; versuch < ADS1263_VALUE_ID_RETRIES; versuch++)
  {
    uint8_t id1 = adsReadRegister(0x00); // Flush/Wakeup, Antwort wird nur angezeigt
    uint8_t id2 = adsReadRegister(0x00); // Diese ID muss stimmen

    ads1263LastValueId1 = id1;
    ads1263LastValueId2 = id2;

    if (id2 == ADS1263_EXPECTED_ID)
    {
      ads1263ValueIdOkCount++;

      if (versuch > 0)
      {
        Serial.print("ADS1263 LINK: Block-ID nach Retry OK vor ");
        Serial.print(tag != nullptr ? tag : "BLOCK");
        Serial.print(": ID1=0x");
        adsPrintHexByte(id1);
        Serial.print(" ID2=0x");
        adsPrintHexByte(id2);
        Serial.println();
      }

      return true;
    }

    delayMicroseconds(20);
  }

  ads1263ValueIdErrCount++;
  ads1263StatsNoteIdDiscard();
  ads1263LastRdataOk = false;

  Serial.print("ADS1263 LINK FEHLER Blockstart ");
  Serial.print(tag != nullptr ? tag : "BLOCK");
  Serial.print(": ID1=0x");
  adsPrintHexByte(ads1263LastValueId1);
  Serial.print(" ID2=0x");
  adsPrintHexByte(ads1263LastValueId2);
  Serial.print(" ERR=");
  Serial.println(ads1263ValueIdErrCount);

  ads1263InfoStatusSet("MAIN ID ERR", false);
  return false;
}

static void adsDummyIdWakeBeforeRdata()
{
  // ADuM/Direktleitung: kein Dummy-ID-Wakeup mehr noetig.
}

static void adsTransferFrame(const uint8_t* tx, uint8_t* rx, uint8_t len)
{
  if (tx == nullptr || len == 0) return;

  adsSpiBusySet(true);
  SPI1.beginTransaction(ads1263SpiSettings);

  noInterrupts();
  adsCsSet(true);
  delayMicroseconds(ADS1263_CS_SETUP_US);

  for (uint8_t i = 0; i < len; i++)
  {
    uint8_t v = SPI1.transfer(tx[i]);
    if (rx != nullptr) rx[i] = v;
  }

  delayMicroseconds(ADS1263_CS_HOLD_US);
  adsCsSet(false);
  interrupts();

  SPI1.endTransaction();
  adsSpiBusySet(false);

  // Abstand zwischen kompletten ADS-Befehlen. Bei ADuM/Direktleitung normalerweise 0 us.
#if ADS1263_FRAME_GAP_US > 0
  delayMicroseconds(ADS1263_FRAME_GAP_US);
#endif
}

static void adsToslinkWakeupActiveCs()
{
  // ADuM/Direktleitung: TOSLINK-Preamble deaktiviert.
}

static uint8_t adsReadRegisterAfterWakeup(uint8_t reg)
{
  // Kompatibilitaetsname fuer den ADC-Info-Linktest.
  // adsReadRegister() enthaelt den TOSLINK-Wakeup inzwischen selbst.
  return adsReadRegister(reg);
}

static void adsCommand(uint8_t command)
{
  adsToslinkWakeupActiveCs();

  uint8_t tx[1] = { command };
  adsTransferFrame(tx, nullptr, sizeof(tx));
}

static void adsWriteRegister(uint8_t reg, uint8_t value)
{
  uint8_t command = 0x40 | (reg & 0x1F);

  adsToslinkWakeupActiveCs();

  uint8_t tx[3] = { command, 0x00, value };
  adsTransferFrame(tx, nullptr, sizeof(tx));
}

static uint8_t adsReadRegister(uint8_t reg)
{
  uint8_t command = 0x20 | (reg & 0x1F);
  uint8_t tx[3] = { command, 0x00, 0x00 };
  uint8_t rx[3] = { 0, 0, 0 };

  adsToslinkWakeupActiveCs();
  adsTransferFrame(tx, rx, sizeof(tx));

  return rx[2];
}

static void adsSetInputMux(uint8_t positive, uint8_t negative)
{
  adsWriteRegister(0x06, ((positive & 0x0F) << 4) | (negative & 0x0F));

  // V50: Nach jedem Kanalwechsel ADC1 sicher neu starten.
  // 0x08 = START1. 0x0C waere START2 und betrifft nur ADC2.
  // Das ist fuer den TOSLINK/DRDY-Test robuster als sich nur auf
  // einen automatischen Conversion-Restart durch WREG zu verlassen.
  adsCommand(0x08);
}

static void ads1263SyncAdc1AfterExternalChange()
{
  // V0.50.0_5 TEST SYNC:
  // CurrRev liegt ausserhalb des ADS1263. Der ADC-Digitalfilter erkennt diese
  // Stromumkehr nicht als echten Kanalwechsel. Deshalb wird ADC1 nach jedem
  // vorbereiteten Messabschnitt per SPI hart neu synchronisiert.
  // STOP1/START1 betrifft nur ADC1; ADC2/Photodiode bleibt davon unberuehrt.
  loescheDrdyTicks();
  drdyCounter = 0;
  ads1263ReadTriggeredByDrdyIrq = false;

  adsCommand(0x0A); // STOP1
  delayMicroseconds(20);
  adsCommand(0x08); // START1
  delayMicroseconds(20);

  loescheDrdyTicks();
  drdyCounter = 0;
  ads1263ReadTriggeredByDrdyIrq = false;
  ads1263LastFallbackTickMs = millis();
}

static void adsSetDataRate(uint8_t rate)
{
  adsWriteRegister(0x01, 0x11 | (rate & 0x0F));
}

static int32_t adsReadRawRdata4NoStatus(uint8_t* rawBytes)
{
  // RDATA fuer ADC1, wenn INTERFACE.STATUS=0 und CRC=0 gesetzt ist.
  // Dann besteht die Antwort exakt aus 4 Datenbytes ohne Status- und Checksum-Byte.
  adsDummyIdWakeBeforeRdata();

  if (!adsDrdyReadyBeforeRead("RDATA4"))
  {
    if (rawBytes != nullptr)
    {
      for (uint8_t i = 0; i < 4; i++) rawBytes[i] = 0;
    }
    return 0;
  }

  uint8_t data[4] = {0, 0, 0, 0};

  adsToslinkWakeupActiveCs();

  uint8_t tx[5] = { 0x12, 0x00, 0x00, 0x00, 0x00 };
  uint8_t rx[5] = { 0, 0, 0, 0, 0 };
  adsTransferFrame(tx, rx, sizeof(tx));
  for (uint8_t i = 0; i < 4; i++)
  {
    data[i] = rx[1 + i];
  }

  if (rawBytes != nullptr)
  {
    for (uint8_t i = 0; i < 4; i++)
    {
      rawBytes[i] = data[i];
    }
  }

  ads1263LastRdataOk = true;

  return ((int32_t)data[0] << 24) |
         ((int32_t)data[1] << 16) |
         ((int32_t)data[2] << 8)  |
          (int32_t)data[3];
}

static bool adsReadInternalTempForInfo(char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0) return false;

  uint8_t savePower     = adsReadRegister(0x01);
  uint8_t saveInterface = adsReadRegister(0x02);
  uint8_t saveMode0     = adsReadRegister(0x03);
  uint8_t saveMode1     = adsReadRegister(0x04);
  uint8_t saveMode2     = adsReadRegister(0x05);
  uint8_t saveInpmux    = adsReadRegister(0x06);
  uint8_t saveRefmux    = adsReadRegister(0x0F);

  // Temperaturtest eindeutig machen: keine Status-/Checksum-Bytes in RDATA.
  adsCommand(0x11); // SDATAC
  delay(2);
  adsWriteRegister(0x02, ADS1263_INTERFACE_DATA4);

  // Datenblatt-Empfehlung fuer interne Temperatur:
  // interne Referenz an, PGA aktiv, Gain=1, Chop aus, INPMUX=BBh.
  adsWriteRegister(0x01, ADS1263_POWER_INTREF_ON);
  adsWriteRegister(0x03, ADS1263_MODE0_TEMP);
  adsWriteRegister(0x04, ADS1263_MODE1_TEMP);
  adsWriteRegister(0x05, ADS1263_MODE2_TEMP_400SPS);
  adsWriteRegister(0x0F, ADS1263_REFMUX_INTERNAL);
  adsWriteRegister(0x06, ADS1263_INPMUX_TEMP_SENSOR);

  adsCommand(0x08); // START / restart conversions
  delay(30);        // mehrere 400-SPS-Wandlungen abwarten

  uint8_t rawBytes[4] = {0, 0, 0, 0};
  int32_t raw = adsReadRawRdata4NoStatus(rawBytes);

  // ADC1: signed 32 bit, V = Code * VREF / 2^31 / Gain.
  // Vref=2.5 V, Gain=1. Ausgabe fuer die Datenblatt-Temperaturformel in µV.
  double microVolts = (double)raw * (2500000.0 / 2147483648.0);
  double tempC = ((microVolts - 122400.0) / 420.0) + 25.0;

  snprintf(out, outSize, "RAW=%02X%02X%02X%02X uV=%.0f T=%.1fC",
           rawBytes[0], rawBytes[1], rawBytes[2], rawBytes[3],
           microVolts, tempC);

  // Urspruengliche Register moeglichst sauber wiederherstellen.
  // POWER: RESET/reservierte Bits nicht erneut setzen; INTREF/VBIAS bleiben erhalten.
  adsWriteRegister(0x06, saveInpmux);
  adsWriteRegister(0x0F, saveRefmux);
  adsWriteRegister(0x05, saveMode2);
  adsWriteRegister(0x04, saveMode1);
  adsWriteRegister(0x03, saveMode0);
  adsWriteRegister(0x02, saveInterface);
  adsWriteRegister(0x01, savePower & 0x03);
  adsCommand(0x08); // Konvertierung fuer vorherige Einstellung wieder anstossen

  return ads1263LastRdataOk && !(raw == 0 || raw == (int32_t)0xFFFFFFFF || raw == (int32_t)0x80000000);
}

static int32_t adsReadRawDirect()
{
  // Die State-Machine gibt den Takt vor; RDATA liest den letzten fertigen
  // ADC1-Wert als vier Datenbytes ohne Status- oder Checksum-Byte.
  return adsReadRawRdata4NoStatus(nullptr);
}

static bool adsReadRawDirectSafe(int32_t* ziel, const char* tag)
{
  int32_t raw = adsReadRawDirect();

  if (!ads1263LastRdataOk)
  {
    ads1263StatsNoteRawDiscard(adsTagToStatChannel(tag));
    Serial.print("ADS1263: Messwert verworfen (");
    Serial.print(tag != nullptr ? tag : "RDATA");
    Serial.println(") - RDATA/DRDY fehlgeschlagen.");
    return false;
  }

  // V56: offensichtlich unplausible Kleinstwerte nicht in die Messkette uebernehmen.
  // Genau solche Werte (300/400/1000) haben die Ref-Berechnung hart gestoert.
  if (raw > -ADS1263_RAW_MIN_ABS_VALID && raw < ADS1263_RAW_MIN_ABS_VALID)
  {
    ads1263AdcRangeErrorCount++;
    ads1263StatsTouchWindow();
    adsStatRngCount++;
    ads1263StatsNoteRawDiscard(adsTagToStatChannel(tag));
    ads1263LastRangeRaw = raw;
    ads1263LastRdataOk = false;

    Serial.print("ADS1263: Messwert verworfen (");
    Serial.print(tag != nullptr ? tag : "RDATA");
    Serial.print(") - RAW unplausibel: ");
    Serial.println(raw);
    return false;
  }

  if (ziel != nullptr)
  {
    *ziel = raw;
  }

  return true;
}

static void adsPrintHexByte(uint8_t value)
{
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

static bool adsRegisterEchoTest(uint8_t reg, uint8_t valueA, uint8_t valueB, const char* name)
{
  uint8_t original = adsReadRegister(reg);

  adsWriteRegister(reg, valueA);
  uint8_t readA = adsReadRegister(reg);

  adsWriteRegister(reg, valueB);
  uint8_t readB = adsReadRegister(reg);

  adsWriteRegister(reg, original);
  uint8_t restored = adsReadRegister(reg);

  Serial.print("ADS1263 ECHO ");
  Serial.print(name);
  Serial.print(": orig=0x");
  adsPrintHexByte(original);
  Serial.print(" A=0x");
  adsPrintHexByte(readA);
  Serial.print(" B=0x");
  adsPrintHexByte(readB);
  Serial.print(" restore=0x");
  adsPrintHexByte(restored);
  Serial.println();

  char ans[68];
  snprintf(ans, sizeof(ans), "orig=%02X 00=%02X 09=%02X R=%02X",
           original, readA, readB, restored);
  ads1263InfoSet(4, "ECHO REFMUX", ans);

  return (readA == valueA && readB == valueB && restored == original);
}

static void adsReadRegisterSetForInfo(uint8_t row, const char* cmd)
{
  uint8_t id     = adsReadRegister(0x00);
  uint8_t power  = adsReadRegister(0x01);
  uint8_t mode2  = adsReadRegister(0x05);
  uint8_t inpmux = adsReadRegister(0x06);
  uint8_t refmux = adsReadRegister(0x0F);

  char ans[68];
  snprintf(ans, sizeof(ans), "ID=%02X PW=%02X M2=%02X IN=%02X RF=%02X",
           id, power, mode2, inpmux, refmux);
  ads1263InfoSet(row, cmd, ans);

  Serial.print("ADS1263 ");
  Serial.print(cmd);
  Serial.print(": ");
  Serial.println(ans);
}



// =========================================================================
// ADC-LINK-DAUERTEST FUER OSZI / LOGIC-ANALYZER
// =========================================================================
void ads1263LinkTestTick()
{
  static uint32_t loopCount = 0;
  loopCount++;

  adsSetSpiMode(1);
  adsStartSet(true);

  // Kein 100-ms-Gate mehr: Der ADC-Info-Screen ruft diese Funktion bei jedem
  // Displaydurchlauf auf. Damit sieht der Logic-Analyzer direkt wiederholte
  // Burst-Ketten ohne kuenstliche Wartepause.
  // Ablauf je Register: CS aktiv -> 8x 0xAA Wakeup -> CS inaktiv -> CS aktiv -> RREG.

  uint8_t id1 = adsReadRegisterAfterWakeup(0x00);  // 20 00 00
  uint8_t id2 = adsReadRegisterAfterWakeup(0x00);  // 20 00 00
  uint8_t id3 = adsReadRegisterAfterWakeup(0x00);  // 20 00 00
  uint8_t id4 = adsReadRegisterAfterWakeup(0x00);  // 20 00 00

  uint8_t power     = adsReadRegisterAfterWakeup(0x01); // 21 00 00
  uint8_t interface = adsReadRegisterAfterWakeup(0x02); // 22 00 00
  uint8_t mode0     = adsReadRegisterAfterWakeup(0x03); // 23 00 00
  uint8_t mode1     = adsReadRegisterAfterWakeup(0x04); // 24 00 00
  uint8_t mode2     = adsReadRegisterAfterWakeup(0x05); // 25 00 00
  uint8_t inpmux    = adsReadRegisterAfterWakeup(0x06); // 26 00 00
  uint8_t refmux    = adsReadRegisterAfterWakeup(0x0F); // 2F 00 00

  char tempAns[68];
  bool tempOk = adsReadInternalTempForInfo(tempAns, sizeof(tempAns));

  char ans[68];
  bool idOk = adsIdPlausibel(id1) || adsIdPlausibel(id2) || adsIdPlausibel(id3) || adsIdPlausibel(id4);
  ads1263InfoStatusSet((idOk && tempOk) ? "LINK+TEMP" : (idOk ? "TEMP WAIT" : "ID WAIT"), idOk && tempOk);

  uint32_t isrCnt = 0;
  uint32_t gltCnt = 0;
  uint32_t busyCnt = 0;
  uint32_t gapUs = 0;
  ads1263DiagCounterSnapshot(&isrCnt, &gltCnt, &busyCnt, &gapUs);

  static uint32_t lastRateMs = 0;
  static uint32_t lastIsrCnt = 0;
  static uint32_t lastGltCnt = 0;
  static uint32_t isrPerSec = 0;
  static uint32_t gltPerSec = 0;

  uint32_t nowMs = millis();
  if (lastRateMs == 0)
  {
    lastRateMs = nowMs;
    lastIsrCnt = isrCnt;
    lastGltCnt = gltCnt;
  }
  else if ((uint32_t)(nowMs - lastRateMs) >= 1000UL)
  {
    uint32_t dt = nowMs - lastRateMs;
    isrPerSec = ((isrCnt - lastIsrCnt) * 1000UL) / dt;
    gltPerSec = ((gltCnt - lastGltCnt) * 1000UL) / dt;
    lastRateMs = nowMs;
    lastIsrCnt = isrCnt;
    lastGltCnt = gltCnt;
  }

  snprintf(ans, sizeof(ans), "MISO=%02X SPI=%s #%lu", id1, ads1263SpiGetLabel(), (unsigned long)loopCount);
  ads1263InfoSet(0, "ID1 20 00 00", ans);

  snprintf(ans, sizeof(ans), "ID=%02X/%02X/%02X GLT=%lu", id2, id3, id4, (unsigned long)gltCnt);
  ads1263InfoSet(1, "ID2/3/4 + GLT", ans);

  snprintf(ans, sizeof(ans), "ISR/s=%lu GLT/s=%lu GAP=%luus",
           (unsigned long)isrPerSec, (unsigned long)gltPerSec, (unsigned long)gapUs);
  ads1263InfoSet(2, "DRDY RATE/GAP", ans);

  snprintf(ans, sizeof(ans), "ISR=%lu BUSY=%lu MIN=%luus",
           (unsigned long)isrCnt, (unsigned long)busyCnt, (unsigned long)ADS1263_DRDY_MIN_GAP_US);
  ads1263InfoSet(3, "DRDY DIAG ONLY", ans);

  snprintf(ans, sizeof(ans), "TO=%lu RNG=%lu Last=%ld",
           (unsigned long)ads1263DrdyTimeoutCount,
           (unsigned long)ads1263AdcRangeErrorCount,
           (long)ads1263LastRangeRaw);
  ads1263InfoSet(4, "POLL/RANGE", ans);

  snprintf(ans, sizeof(ans), "IN=%02X RF=%02X DRDY=%u", inpmux, refmux, adsDrdyAktiv() ? 1 : 0);
  ads1263InfoSet(5, "TEMP BB RDATA", tempAns);

  Serial.print("ADS LINK #");
  Serial.print(loopCount);
  Serial.print(" ID1="); adsPrintHexByte(id1);
  Serial.print(" ID2="); adsPrintHexByte(id2);
  Serial.print(" ID3="); adsPrintHexByte(id3);
  Serial.print(" ID4="); adsPrintHexByte(id4);
  Serial.print(" PW="); adsPrintHexByte(power);
  Serial.print(" IF="); adsPrintHexByte(interface);
  Serial.print(" M0="); adsPrintHexByte(mode0);
  Serial.print(" M1="); adsPrintHexByte(mode1);
  Serial.print(" M2="); adsPrintHexByte(mode2);
  Serial.print(" IN="); adsPrintHexByte(inpmux);
  Serial.print(" RF="); adsPrintHexByte(refmux);
  Serial.print(" DRDY=");
  Serial.print(adsDrdyAktiv() ? 1 : 0);
  Serial.print(" TEMP " );
  Serial.println(tempAns);
}

static bool adsDigitalSelfTest()
{
  // ADuM/Direkt-Teststand: SPI_MODE1, CS/START/RESET nicht invertiert, DRDY direkt aktiv LOW.
  // WICHTIG fuer Fehlersuche:
  // Dieser Test laeuft immer bis zum Ende durch und fuellt alle ADC-Info-Zeilen,
  // auch wenn ID/REG READ bereits unplausibel sind.

  adsSetSpiMode(1);

  char spiAns[68];
  snprintf(spiAns, sizeof(spiAns), "MODE1 %luk CS%lu/H%lu/P%lu",
           (unsigned long)(ads1263SpiHz / 1000UL),
           (unsigned long)ADS1263_CS_SETUP_US,
           (unsigned long)ADS1263_CS_HOLD_US,
           (unsigned long)ADS1263_TRANS_PAUSE_US);
  ads1263InfoSet(1, "SPI FIX", spiAns);

  adsCommand(0x11); // SDATAC, Registerzugriffe eindeutig machen
  delay(2);

  // Erkenntnis vom Logic-Analyzer:
  // 20 00 00 -> ID kommt stabil als ca. 0x23 zurueck,
  // 22 00 00 -> INTERFACE kam plausibel als 0x05 zurueck.
  // Deshalb beim Start vier direkte ID-Lesungen und INTERFACE anzeigen.
  uint8_t id1 = adsReadRegister(0x00);
  uint8_t id2 = adsReadRegister(0x00);
  uint8_t id3 = adsReadRegister(0x00);
  uint8_t id4 = adsReadRegister(0x00);
  uint8_t interface = adsReadRegister(0x02);

  char idAns[68];
  snprintf(idAns, sizeof(idAns), "ID=%02X %02X %02X %02X IF=%02X", id1, id2, id3, id4, interface);
  ads1263InfoSet(2, "START IDx4", idAns);
  Serial.print("ADS1263 START IDx4: ");
  Serial.println(idAns);

  adsReadRegisterSetForInfo(3, "REG READ 1");

  bool idOk = adsIdPlausibel(id1) && adsIdPlausibel(id2) && adsIdPlausibel(id3) && adsIdPlausibel(id4);
  if (!idOk)
  {
    Serial.println("ADS1263 TEST HINWEIS: ID nicht plausibel. Test laeuft trotzdem weiter.");
  }

  // REFMUX Echo-Test mit richtigem AIN0/AIN1-Code 0x09.
  // Wird auch bei falscher ID ausgefuehrt, damit wir MOSI/MISO-Schreib-/Lesen sehen.
  bool echoOk = adsRegisterEchoTest(0x0F, 0x00, ADS1263_REFMUX_AIN0_AIN1, "REFMUX");
  if (!echoOk)
  {
    Serial.println("ADS1263 TEST HINWEIS: REFMUX Echo fehlgeschlagen. Test laeuft trotzdem weiter.");
  }

  // Interne Testmessung: interne Temperatur gegen interne 2.5-V-Referenz.
  // Wichtig: INPMUX=BBh und INTERFACE=00h, damit RDATA exakt 4 Datenbytes liefert.
  char ans[68];
  bool tempOk = adsReadInternalTempForInfo(ans, sizeof(ans));
  ads1263InfoSet(5, "TEMP BB RDATA", ans);

  Serial.print("ADS1263 TEST interne Temp ");
  Serial.println(ans);

  if (!tempOk)
  {
    Serial.println("ADS1263 TEST HINWEIS: Interne Testmessung liefert 0/ungueltig.");
  }

  if (idOk && echoOk && tempOk)
  {
    ads1263InfoStatusSet("TEST OK", true);
    Serial.println("ADS1263 TEST OK: DRDY, Register, Echo und interner ADC-Datenpfad plausibel.");
    return true;
  }

  if (!idOk)
  {
    ads1263InfoStatusSet("ID ERR", false);
  }
  else if (!echoOk)
  {
    ads1263InfoStatusSet("ECHO ERR", false);
  }
  else
  {
    ads1263InfoStatusSet("TEMP ERR", false);
  }

  Serial.println("ADS1263 TEST FEHLER: Mindestens ein Teiltest ist fehlgeschlagen. Simulation bleibt aktiv.");
  return false;
}


// =========================================================================
// HILFSFUNKTION: Sättigende Addition für drdyCounter
// =========================================================================
static void addiereDrdyTicks(uint16_t ticks)
{
  if (ticks == 0) return;

  uint32_t neu = (uint32_t)drdyCounter + (uint32_t)ticks;

  if (neu > 65000UL)
    drdyCounter = 65000;
  else
    drdyCounter = (uint16_t)neu;
}


// =========================================================================
// REGELUNG RUHIG?
// =========================================================================
// Die Nullung darf erst nach mindestens 5 Minuten fällig werden UND
// die Regelung muss ca. 30 Sekunden ruhig sein.
// Schwellwerte bewusst moderat:
// - tempSpiegel Änderung < 0.02 °C pro Sekunde
// - peltierSollWert Änderung < 25 PWM-Zähler pro Sekunde
static bool regelungIstRuhig()
{
  static bool initialisiert = false;
  static float letzteTemp = 0.0f;
  static float letzterPeltier = 0.0f;
  static unsigned long letzterCheck = 0;
  static unsigned long ruhigSeit = 0;

  unsigned long jetzt = millis();

  if (!initialisiert)
  {
    initialisiert = true;
    letzteTemp = tempSpiegel;
    letzterPeltier = peltierSollWert;
    letzterCheck = jetzt;
    ruhigSeit = 0;
    return false;
  }

  // Nur einmal pro Sekunde bewerten
  if (jetzt - letzterCheck < 1000)
  {
    return (ruhigSeit != 0 && (jetzt - ruhigSeit >= 30000));
  }

  float deltaTemp = fabsf(tempSpiegel - letzteTemp);
  float deltaPeltier = fabsf(peltierSollWert - letzterPeltier);

  letzteTemp = tempSpiegel;
  letzterPeltier = peltierSollWert;
  letzterCheck = jetzt;

  if (deltaTemp < 0.020f && deltaPeltier < 25.0f)
  {
    if (ruhigSeit == 0)
    {
      ruhigSeit = jetzt;
    }
  }
  else
  {
    ruhigSeit = 0;
  }

  return (ruhigSeit != 0 && (jetzt - ruhigSeit >= 30000));
}


// =========================================================================
// ADC2 / PHOTODIODE
// =========================================================================
static int32_t readADS1263_ADC2_raw()
{
  // ADC2 darf beim ADS1263 nur per RDATA2-Kommando gelesen werden.
  // INTERFACE ist ohne Status/CRC konfiguriert, daher nach dem Kommando:
  // rx[1], rx[2], rx[3] = 24-bit ADC2-Daten, rx[4] = festes 0x00-Pad.
  uint8_t tx[5] = { 0x14, 0x00, 0x00, 0x00, 0x00 };
  uint8_t rx[5] = { 0, 0, 0, 0, 0 };

  adsTransferFrame(tx, rx, sizeof(tx));

  uint32_t regData = ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | (uint32_t)rx[3];

  // 24-bit Vorzeichen erweitern
  if (regData & 0x800000UL)
  {
    regData |= 0xFF000000UL;
  }

  return (int32_t)regData;
}

bool ads1263Adc2MeasureDark()
{
#if ADS1263_ADC2_ENABLE
  if (!ads1263_bereit) return false;

  ads1263Adc2DarkMeasuringFlag = true;

  // LED sicher abschalten. Pin 7 LED_OFF ist aktiv HIGH.
  setLedOffHardware(true);
  delay(ADS1263_ADC2_DARK_SETTLE_MS);

  int64_t sum = 0;
  uint16_t samples = 0;
  uint32_t nextUs = micros();

  for (uint16_t i = 0; i < ADS1263_ADC2_DARK_SAMPLES; i++)
  {
    if (i > 0)
    {
      while ((int32_t)(micros() - nextUs) < 0)
      {
        // Kurze Wartezeit zwischen den ADC2-Stichproben. Beim Start ist
        // ADC1 noch nicht im Messkarussell; waehrend einer Auto-Cal pausiert
        // der Hauptloop hier bewusst kurz, ohne die ADC1-Konfiguration zu aendern.
      }
    }

    int32_t raw = readADS1263_ADC2_raw();
    sum += raw;
    samples++;
    nextUs = micros() + ADS1263_ADC2_SAMPLE_INTERVAL_US;
  }

  if (samples > 0)
  {
    ads1263Adc2DarkRaw = (int32_t)(sum / (int64_t)samples);
    ads1263Adc2DarkValidFlag = true;
    ads1263Adc2DarkLastMs = millis();
  }

  // Nach der Dunkelmessung den normalen Lichtpuffer sauber neu starten.
  // LED bleibt aus, bis setTargetCurrent() die Regelung freigibt.
  ads1263Adc2ResetBuffer();
  adcRawPhotodiode = 0;

  ads1263Adc2DarkMeasuringFlag = false;
  return (samples > 0);
#else
  return false;
#endif
}

int32_t readADS1263_ADC2()
{
  if (!ads1263_bereit) return 0;
  return readADS1263_ADC2_raw();
}

static void ads1263ServiceAdc2()
{
#if ADS1263_ADC2_ENABLE
  if (!ads1263_bereit) return;
  if (ads1263SpiBusy) return;
  if (ads1263Adc2DarkMeasuringFlag) return;

  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - ads1263Adc2LastSampleUs) < ADS1263_ADC2_SAMPLE_INTERVAL_US)
  {
    return;
  }

  // Phasenfehler klein halten, aber bei grosser Pause neu aufsetzen.
  if ((uint32_t)(nowUs - ads1263Adc2LastSampleUs) > (ADS1263_ADC2_SAMPLE_INTERVAL_US * 4UL))
  {
    ads1263Adc2LastSampleUs = nowUs;
  }
  else
  {
    ads1263Adc2LastSampleUs += ADS1263_ADC2_SAMPLE_INTERVAL_US;
  }

  int32_t raw = readADS1263_ADC2_raw();
  ads1263Adc2NoteSample(raw);
#endif
}

void ads1263ServiceAdc2Background()
{
  ads1263ServiceAdc2();
}

// =========================================================================
// HARDWARE-NULLABGLEICH SFOCAL
// =========================================================================
void fuehreAdsNullAbgleichAus()
{
  if (!ads1263_bereit) return;

  Serial.println("ADS1263: Starte SFOCAL Nullabgleich...");

  // Während SFOCAL keine alten DRDYs mitzählen
  detachInterrupt(digitalPinToInterrupt(pinDrdy));
  loescheDrdyTicks();

  // TI-konformer SFOCAL-Test:
  // Für SFOCAL1 ADC1-INPMUX auf 0xFF setzen.
  // Der ADS126x schliesst die PGA-Eingaenge intern kurz; nicht extern COM/COM messen.
  adsWriteRegister(0x06, 0xFF);
  adsCommand(0x08);   // ADC1 nach MUX-Aenderung sicher aktiv
  delay(2);

  // SFOCAL
  adsCommand(0x19);

  // 100 SPS + SINC4: Kalibrierung braucht deutlich laenger als 40 ms.
  // 300 ms Reserve, damit ADC1 sicher fertig ist, bevor die State-Machine neu startet.
  delay(300);

  // State-Machine sauber neu starten
  maintStep = 0;
  drdyCounter = 0;
  loescheDrdyTicks();

  attachInterrupt(digitalPinToInterrupt(pinDrdy), ads1263_drdy_counter_isr, adsDrdyInterruptMode());

  Serial.println("ADS1263: SFOCAL fertig.");
}

static void ads1263SfocalRunAndMark()
{
  fuehreAdsNullAbgleichAus();
  letzteNullAbgleichZeit = millis();
  letzterNullVersuchLog = 0;
}

void adc1SfocalAutoCalEvent()
{
  if (adc1SfocalModeGet() != ADC1_SFOCAL_SYNC_AUTOCAL)
  {
    return;
  }

  Serial.println("ADS1263: SFOCAL synchron mit Auto-Cal/Freiheizen.");
  ads1263SfocalRunAndMark();
}


// =========================================================================
// INIT ADS1263
// =========================================================================
void initADS1263()
{
  ads1263InfoClear("INIT", false);
  Serial.println("ADS1263: Initialisiere Pins und SPI1...");

  // Pins sicher setzen. CS/START/RESET nicht invertiert, DRDY direkt aktiv LOW.
  pinMode(pincs1, OUTPUT);
  adsCsSet(false);       // ADS-CS inaktiv

  pinMode(pinDrdy, INPUT);

  pinMode(pinStart, OUTPUT);
  adsStartSet(false);    // START aus

  pinMode(pinRstAd, OUTPUT);
  adsResetSet(true);     // RESET/PWDN frei

  pinMode(pinCurrRev, OUTPUT);
  digitalWriteFast(pinCurrRev, LOW);

  // SPI1-Pins für deine Platinenmischung
  SPI1.setMISO(1);
  SPI1.setMOSI(26);
  SPI1.setSCK(27);
  SPI1.begin();

  delay(20);

  adsPrintPinDebug("nach Pin-Init");

  // ADS einmal sauber resetten / powerdown-pulsen
  adsResetSet(false);
  delay(5);
  adsResetSet(true);
  delay(120);

  // Software-Reset per Kommando
  adsCommand(0x06);
  delay(120);

  // START aktivieren, damit DRDY laufen kann
  adsStartSet(true);
  delay(50);
  adsPrintPinDebug("nach Reset+START");

  // Hardware-Check: Kommt DRDY?
  unsigned long startWartezeit = millis();
  bool hardwareGefunden = false;

  while (millis() - startWartezeit < 1000)
  {
    if (adsDrdyAktiv())
    {
      hardwareGefunden = true;
      break;
    }
  }

  if (!hardwareGefunden)
  {
#if ADS1263_START_FORCE_READY_TEST
    ads1263InfoSet(0, "DRDY", "BYPASS / timed");
    ads1263InfoStatusSet("FORCE READY", true);
    Serial.println("ADS1263 V49 TEST: Kein DRDY am Teensy erkannt, Hauptbetrieb wird trotzdem freigegeben.");
#else
    ads1263InfoSet(0, "DRDY", "ERR / timeout");
    ads1263InfoStatusSet("DRDY ERR", false);
    Serial.println("WARNUNG: Kein DRDY vom ADS1263. Simulationsmodus aktiv.");
    ads1263_bereit = false;
    return;
#endif
  }
  else
  {
    ads1263InfoSet(0, "DRDY", "OK");
    ads1263InfoStatusSet("SELFTEST", false);
    Serial.println("ADS1263: DRDY erkannt.");
  }

  adsSetSpiMode(1);

#if ADS1263_START_FORCE_READY_TEST
  // V49: Selbsttest nur noch als Info ausfuehren, aber nicht mehr blockieren.
  // Wir wissen aus V47: ID=23 und interne Temperatur funktionieren. Jetzt soll der Hauptschirm laufen.
  Serial.println("ADS1263 V49 TEST: Selbsttest wird nicht mehr als Start-Blocker benutzt.");
  bool selfTestInfoOk = adsDigitalSelfTest();
  (void)selfTestInfoOk;
  ads1263InfoStatusSet(hardwareGefunden ? "ADS FORCE" : "ADS FORCE/NO DRDY", true);
  ads1263_bereit = true;
#else
  Serial.println("ADS1263: Starte festen LWL-Selbsttest mit SPI MODE1 / 50 kHz...");
  if (!adsDigitalSelfTest())
  {
    Serial.println("WARNUNG: ADS1263 Selbsttest fehlgeschlagen. Simulationsmodus bleibt aktiv.");
    ads1263_bereit = false;
    return;
  }

  ads1263InfoStatusSet("ADS OK", true);
  ads1263_bereit = true;
#endif

  // Interrupt vorbereiten
  detachInterrupt(digitalPinToInterrupt(pinDrdy));
  loescheDrdyTicks();

  // ADC konfigurieren
  adsSetDataRate(ADS1262_DR_100_SPS);

  // Unsere RDATA-Routinen lesen exakt 4 ADC1-Datenbytes.
  // Deshalb Status- und Checksum-Byte im Datenstrom abschalten.
  adsWriteRegister(0x02, ADS1263_INTERFACE_DATA4);

  // Haupt-ADC ratiometrisch ueber REF0/REF1 (AIN0/AIN1)
  adsWriteRegister(0x0F, ADS1263_REFMUX_AIN0_AIN1);

  // V50 Test: wie beim erfolgreichen internen Temperaturtest erstmal schnell und eindeutig.
  // MODE2 = 0x57: Gain 32 / 100 SPS fuer ruhigen ADC1-Pt100-Test.
  // ADC2/Optik bleibt separat auf 400 SPS.
  adsWriteRegister(0x05, ADS1263_MAIN_MODE2_FAST_TEST);

  // ADC2 fuer Photodiode vorbereiten.
  // 0x15 = ADC2CFG: 400 SPS, Ref = AVDD/AVSS, Gain = 2  -> 0xA1
  // 0x16 = ADC2MUX: P = AIN8, N = AIN9(GND)              -> 0x89
  // Falls die Photodiode anders beschaltet ist, oben nur ADS1263_ADC2_MUX_VALUE aendern.
  adsWriteRegister(0x15, ADS1263_ADC2_CFG_VALUE);
  adsWriteRegister(0x16, ADS1263_ADC2_MUX_VALUE);
  ads1263Adc2ResetBuffer();

  Serial.print("ADS1263 REG nach Konfig: ID=0x");
  adsPrintHexByte(adsReadRegister(0x00));
  Serial.print(" POWER=0x");
  adsPrintHexByte(adsReadRegister(0x01));
  Serial.print(" MODE2=0x");
  adsPrintHexByte(adsReadRegister(0x05));
  Serial.print(" REFMUX=0x");
  adsPrintHexByte(adsReadRegister(0x0F));
  Serial.print(" ADC2CFG=0x");
  adsPrintHexByte(adsReadRegister(0x15));
  Serial.print(" ADC2MUX=0x");
  adsPrintHexByte(adsReadRegister(0x16));
  Serial.println();

  delay(20);

  // V50 Test: SFOCAL vorerst ueberspringen. Solange DRDY am Teensy noch unklar ist,
  // soll die Kalibrierung den Start nicht beeinflussen. Spaeter wieder aktivieren.
#if ADS1263_SKIP_SFOCAL_TEST
  Serial.println("ADS1263 V50 TEST: SFOCAL beim Start uebersprungen.");
#else
  {
    const uint8_t sfocalMode = adc1SfocalModeGet();
    if (sfocalMode == ADC1_SFOCAL_START_ONLY ||
        sfocalMode == ADC1_SFOCAL_10MIN ||
        sfocalMode == ADC1_SFOCAL_30MIN ||
        sfocalMode == ADC1_SFOCAL_60MIN)
    {
      ads1263SfocalRunAndMark();
    }
    else
    {
      Serial.println("ADS1263: Start-SFOCAL uebersprungen, Modus Sync Auto-Cal.");
    }
  }
#endif

  // ADC1 und ADC2 sicher starten. 0x08 = START1, 0x0C = START2.
  adsCommand(0x08);
  adsCommand(0x0C);

  delay(10);

  // ADC2-Dunkelwert beim Start erstmals ermitteln. Danach wird er vor
  // jeder Optik-/LED-Auto-Cal erneut gemessen.
  // LED_OFF bleibt dabei HIGH und somit sicher aus, bis der Regler per setTargetCurrent() freigibt.
  if (ads1263Adc2MeasureDark())
  {
    Serial.print("ADS1263 ADC2 dark=");
    Serial.println((long)ads1263Adc2GetDark());
  }
  else
  {
    Serial.println("WARNUNG: ADS1263 ADC2 dark konnte nicht gemessen werden.");
  }

  adsStartSet(true);
  digitalWriteFast(pinCurrRev, LOW);

  maintStep = 0;
  drdyCounter = 0;
  refKaskade = 0;

  letzteMittelWertZeit = millis();
  letzteNullAbgleichZeit = millis();

  ads1263DiagCounterReset();
  loescheDrdyTicks();
  attachInterrupt(digitalPinToInterrupt(pinDrdy), ads1263_drdy_counter_isr, adsDrdyInterruptMode());

  Serial.println("ADS1263: Initialisierung abgeschlossen.");
}



// =========================================================================
// V60 PLAUSIBILITAET / SATZUEBERNAHME
// =========================================================================
static bool adsRawGrossGenug(int32_t raw)
{
  return (raw <= -ADS1263_RAW_MIN_ABS_VALID || raw >= ADS1263_RAW_MIN_ABS_VALID);
}

static int64_t adsAbs64(int32_t raw)
{
  int64_t v = (int64_t)raw;
  return (v < 0) ? -v : v;
}

static void adsRangeReject(const char* tag, int32_t raw)
{
  ads1263AdcRangeErrorCount++;
  ads1263StatsTouchWindow();
  adsStatRngCount++;
  uint8_t ch = adsTagToStatChannel(tag);
  ads1263StatsNoteRawDiscard(ch);
  if (tag != nullptr)
  {
    if (strstr(tag, "Spiegel") != nullptr) ads1263StatsNotePairDiscard(ADS1263_PAIR_MIR);
    else if (strstr(tag, "Umgebung") != nullptr) ads1263StatsNotePairDiscard(ADS1263_PAIR_UMG);
    else if (strstr(tag, "REF") != nullptr) ads1263StatsNotePairDiscard(ADS1263_PAIR_REF);
  }
  ads1263LastRangeRaw = raw;

  Serial.print("ADS1263: Satz/Wert verworfen (");
  Serial.print(tag != nullptr ? tag : "ADC");
  Serial.print(") raw=");
  Serial.println(raw);
}

static bool adsUebernehmePtSatzWennPlausibel()
{
  // Gain 32: pos und neg koennen je ca. +/-1.6e9 Counts haben.
  // Die Differenz pos-neg passt dann NICHT mehr in int32_t.
  // Deshalb zuerst auf int64_t erweitern, dann halbieren.
  int32_t pt1neu = (int32_t)(((int64_t)pt100_1_pos - (int64_t)pt100_1_neg) / 2);
  int32_t pt2neu = (int32_t)(((int64_t)pt100_2_pos - (int64_t)pt100_2_neg) / 2);

  if (!adsRawGrossGenug(pt1neu))
  {
    adsRangeReject("PT Spiegel Satz", pt1neu);
    return false;
  }

  if (!adsRawGrossGenug(pt2neu))
  {
    adsRangeReject("PT Umgebung Satz", pt2neu);
    return false;
  }

  // Grober Sprungschutz: Wenn bereits ein gueltiger Wert existiert, darf der neue
  // Wert nicht um Faktorbereiche springen. 30 % ist bewusst debug-grosszuegig.
  if (adsRawGrossGenug(adcRawPt100_1))
  {
    int64_t alt = adsAbs64(adcRawPt100_1);
    int64_t neu = adsAbs64(pt1neu);
    if (neu < (alt * 7) / 10 || neu > (alt * 13) / 10)
    {
      adsRangeReject("PT Spiegel Jump", pt1neu);
      return false;
    }
  }

  if (adsRawGrossGenug(adcRawPt100_2))
  {
    int64_t alt = adsAbs64(adcRawPt100_2);
    int64_t neu = adsAbs64(pt2neu);
    if (neu < (alt * 7) / 10 || neu > (alt * 13) / 10)
    {
      adsRangeReject("PT Umgebung Jump", pt2neu);
      return false;
    }
  }

  // Grobe physikalische Plausibilitaet ueber aktuelle Ref-Basis.
  if (adcRawRef100R > 0 && adcRawRef120R > adcRawRef100R)
  {
    double refLowOhm = refCalGet100Ohm();
    double refHighOhm = refCalGet120Ohm();
    double deltaOhmRef = refHighOhm - refLowOhm;
    double stepsPerOhm = 0.0;
    if (isfinite(deltaOhmRef) && deltaOhmRef > 1.0)
    {
      stepsPerOhm = ((double)adcRawRef120R - (double)adcRawRef100R) / deltaOhmRef;
    }
    if (stepsPerOhm > 0.0 && isfinite(stepsPerOhm))
    {
      double ohm1 = refLowOhm + (((double)pt1neu - (double)adcRawRef100R) / stepsPerOhm);
      double ohm2 = refLowOhm + (((double)pt2neu - (double)adcRawRef100R) / stepsPerOhm);
      if (!isfinite(ohm1) || ohm1 < 75.0 || ohm1 > 155.0)
      {
        adsRangeReject("PT Spiegel Ohm", pt1neu);
        return false;
      }
      if (!isfinite(ohm2) || ohm2 < 75.0 || ohm2 > 155.0)
      {
        adsRangeReject("PT Umgebung Ohm", pt2neu);
        return false;
      }
    }
  }

  adcRawPt100_1 = pt1neu;
  adcRawPt100_2 = pt2neu;
  ads1263PtFilterNoteAccepted(pt1neu, pt2neu);
  ads1263StatsNoteAdc(ADS1263_STAT_MIR, pt1neu);
  ads1263StatsNoteAdc(ADS1263_STAT_UMG, pt2neu);
  return true;
}

static void adsAktualisiereRefMittelwerteAusPuffer();

static uint8_t adsRefPufferGueltigePaare()
{
  uint8_t n = 0;
  for (uint8_t i = 0; i < ADS1263_REF_BUF_SIZE; i++)
  {
    if (puffer100R[i] != 0 && puffer120R[i] != 0) n++;
  }
  return n;
}

uint8_t ads1263RefFillCount()
{
  return adsRefPufferGueltigePaare();
}

uint32_t ads1263RefAcceptedCount()
{
  return adsRefAcceptedPairCount;
}

uint32_t ads1263RefRejectRangeCount()
{
  return adsRefRejectRangeCount;
}

uint32_t ads1263RefRejectRatioCount()
{
  return adsRefRejectRatioCount;
}

uint32_t ads1263RefRejectSeedCount()
{
  return adsRefRejectSeedCount;
}

uint32_t ads1263RefRejectJump100Count()
{
  return adsRefRejectJump100Count;
}

uint32_t ads1263RefRejectJump120Count()
{
  return adsRefRejectJump120Count;
}

uint32_t ads1263RefRejectJumpDeltaCount()
{
  return adsRefRejectJumpDeltaCount;
}

uint8_t ads1263RefLastRejectReason()
{
  return adsRefLastRejectReason;
}

uint32_t ads1263RefAgeMs()
{
  if (adsRefLastAcceptedMs == 0)
  {
    return (uint32_t)millis();
  }
  return (uint32_t)(millis() - adsRefLastAcceptedMs);
}

uint32_t ads1263RefAgeSeconds()
{
  return ads1263RefAgeMs() / 1000UL;
}

bool ads1263RefIsOld()
{
  return (ads1263RefAgeMs() >= ADS1263_REF_OLD_MS);
}

bool ads1263RefIsError()
{
  return (ads1263RefAgeMs() >= ADS1263_REF_ERROR_MS);
}

static double adsAbsDouble(double v)
{
  return (v < 0.0) ? -v : v;
}

static bool adsRefSprungGegenMittelwertOk(int32_t r100, int32_t r120, double* maxPctOut)
{
  if (maxPctOut) *maxPctOut = 0.0;
  adsRefLastRejectReason = 0;

  const uint8_t refPairs = adsRefPufferGueltigePaare();

  // Einlaufphase: erst den 32er Ref-Puffer komplett fuellen.
  // Dabei nicht blind uebernehmen: ab dem ersten guten Paar dient ein
  // Seed-Wert als grobe Schutzgrenze von +/-0,2 %.
  if (refPairs < ADS1263_REF_JUMP_MIN_SAMPLES)
  {
    if (adsRefFillSeed100 <= 0 || adsRefFillSeed120 <= adsRefFillSeed100)
    {
      return true;  // erstes gutes Ratio-Paar wird Seed
    }

    const double dSeed = (double)adsRefFillSeed120 - (double)adsRefFillSeed100;
    const double dCand = (double)r120 - (double)r100;
    if (dSeed <= 0.0 || dCand <= 0.0)
    {
      adsRefLastRejectReason = ADS1263_REF_REJ_SEED;
      return false;
    }

    double e100 = adsAbsDouble((double)r100 - (double)adsRefFillSeed100) / (double)adsRefFillSeed100;
    double e120 = adsAbsDouble((double)r120 - (double)adsRefFillSeed120) / (double)adsRefFillSeed120;
    double edel = adsAbsDouble(dCand - dSeed) / dSeed;

    double emax = e100;
    if (e120 > emax) emax = e120;
    if (edel > emax) emax = edel;

    if (maxPctOut) *maxPctOut = emax * 100.0;
    if (emax > ADS1263_REF_FILL_SEED_LIMIT_FRAC)
    {
      adsRefLastRejectReason = ADS1263_REF_REJ_SEED;
      return false;
    }
    return true;
  }

  // Normalbetrieb: gegen den laufenden 32er Ref-Mittelwert pruefen.
  int32_t mean100 = adcRawRef100R;
  int32_t mean120 = adcRawRef120R;
  if (mean100 <= 10000000L || mean120 <= mean100) return true;

  const double dMean = (double)mean120 - (double)mean100;
  const double dCand = (double)r120 - (double)r100;
  if (dMean <= 0.0 || dCand <= 0.0)
  {
    adsRefLastRejectReason = ADS1263_REF_REJ_JDELTA;
    return false;
  }

  double e100 = adsAbsDouble((double)r100 - (double)mean100) / (double)mean100;
  double e120 = adsAbsDouble((double)r120 - (double)mean120) / (double)mean120;
  double edel = adsAbsDouble(dCand - dMean) / dMean;

  double emax = e100;
  if (e120 > emax) emax = e120;
  if (edel > emax) emax = edel;

  if (maxPctOut) *maxPctOut = emax * 100.0;

  if (e100 > ADS1263_REF_ABS_JUMP_LIMIT_FRAC)   adsRefLastRejectReason |= ADS1263_REF_REJ_J100;
  if (e120 > ADS1263_REF_ABS_JUMP_LIMIT_FRAC)   adsRefLastRejectReason |= ADS1263_REF_REJ_J120;
  if (edel > ADS1263_REF_DELTA_JUMP_LIMIT_FRAC) adsRefLastRejectReason |= ADS1263_REF_REJ_JDELTA;

  return (adsRefLastRejectReason == 0);
}

static void adsRefZaehleVerwurf(uint8_t reason)
{
  if (reason & ADS1263_REF_REJ_RANGE)  adsRefRejectRangeCount++;
  if (reason & ADS1263_REF_REJ_RATIO)  adsRefRejectRatioCount++;
  if (reason & ADS1263_REF_REJ_SEED)   adsRefRejectSeedCount++;
  if (reason & ADS1263_REF_REJ_J100)   adsRefRejectJump100Count++;
  if (reason & ADS1263_REF_REJ_J120)   adsRefRejectJump120Count++;
  if (reason & ADS1263_REF_REJ_JDELTA) adsRefRejectJumpDeltaCount++;
}

static bool adsRefRawRatioPlausibel(double rawRatio, double* expectedOut, double* minOut, double* maxOut)
{
  double expected = 0.0;
  double minOk = ADS1263_REF_RATIO_FALLBACK_MIN;
  double maxOk = ADS1263_REF_RATIO_FALLBACK_MAX;

  if (isfinite(rawRatio))
  {
    double refLowOhm = refCalGet100Ohm();
    double refHighOhm = refCalGet120Ohm();

    if (isfinite(refLowOhm) && isfinite(refHighOhm) &&
        refLowOhm > 1.0 && refHighOhm > refLowOhm)
    {
      expected = refHighOhm / refLowOhm;
      minOk = expected * (1.0 - ADS1263_REF_RATIO_EXPECTED_TOL_FRAC);
      maxOk = expected * (1.0 + ADS1263_REF_RATIO_EXPECTED_TOL_FRAC);

      if (minOk < ADS1263_REF_RATIO_ABS_MIN) minOk = ADS1263_REF_RATIO_ABS_MIN;
      if (maxOk > ADS1263_REF_RATIO_ABS_MAX) maxOk = ADS1263_REF_RATIO_ABS_MAX;
    }
  }

  if (expectedOut) *expectedOut = expected;
  if (minOut) *minOut = minOk;
  if (maxOut) *maxOut = maxOk;

  return (isfinite(rawRatio) && rawRatio >= minOk && rawRatio <= maxOk);
}

static void adsVersucheRefPaarUebernahme()
{
  if (!ref100_candidate_valid || !ref120_candidate_valid) return;

  bool ok = false;
  bool jumpOk = true;
  double ratio = 0.0;
  double ratioExpected = 0.0;
  double ratioMinOk = 0.0;
  double ratioMaxOk = 0.0;
  double refJumpMaxPct = 0.0;
  uint8_t rejectReason = 0;
  adsRefLastRejectReason = 0;

  if (ref100_candidate > 0 && ref120_candidate > 0 && ref120_candidate > ref100_candidate)
  {
    ratio = (double)ref120_candidate / (double)ref100_candidate;
    ok = adsRefRawRatioPlausibel(ratio, &ratioExpected, &ratioMinOk, &ratioMaxOk);
    if (!ok)
    {
      rejectReason |= ADS1263_REF_REJ_RATIO;
    }
    else
    {
      jumpOk = adsRefSprungGegenMittelwertOk(ref100_candidate, ref120_candidate, &refJumpMaxPct);
      if (!jumpOk)
      {
        rejectReason |= adsRefLastRejectReason;
      }
      ok = jumpOk;
    }
  }
  else
  {
    rejectReason |= ADS1263_REF_REJ_RANGE;
  }

  if (ok)
  {
    if (adsRefFillSeed100 <= 0 || adsRefFillSeed120 <= adsRefFillSeed100)
    {
      adsRefFillSeed100 = ref100_candidate;
      adsRefFillSeed120 = ref120_candidate;
    }

    puffer100R[zeiger100R] = ref100_candidate;
    zeiger100R = (zeiger100R + 1) % ADS1263_REF_BUF_SIZE;

    puffer120R[zeiger120R] = ref120_candidate;
    zeiger120R = (zeiger120R + 1) % ADS1263_REF_BUF_SIZE;

    // Ref-Arbeitswerte nur aus dem Ringpuffer-Mittelwert bilden.
    // Dadurch wirken ADS1263_REF_BUF_SIZE Werte wirklich auf Anzeige und Berechnung.
    adsAktualisiereRefMittelwerteAusPuffer();

    adsRefLastAcceptedMs = (uint32_t)millis();
    adsRefAcceptedPairCount++;

    ads1263StatsNoteAdc(ADS1263_STAT_R100, ref100_candidate);
    ads1263StatsNoteAdc(ADS1263_STAT_R120, ref120_candidate);
    ads1263StatsNoteRatio((float)ratio);

    Serial.print("ADS1263 REF PAAR OK: RLOW=");
    Serial.print(ref100_candidate);
    Serial.print(" RHIGH=");
    Serial.print(ref120_candidate);
    Serial.print(" ratio=");
    Serial.println(ratio, 5);
  }
  else
  {
    adsRefLastRejectReason = rejectReason;
    adsRefZaehleVerwurf(rejectReason);

    ads1263AdcRangeErrorCount++;
    ads1263StatsTouchWindow();
    adsStatRngCount++;
    ads1263StatsNotePairDiscard(ADS1263_PAIR_REF);
    ads1263StatsNoteRawDiscard(ADS1263_STAT_R100);
    ads1263StatsNoteRawDiscard(ADS1263_STAT_R120);
    ads1263LastRangeRaw = ref120_candidate;

    Serial.print("ADS1263 REF PAAR VERWORFEN: RLOW=");
    Serial.print(ref100_candidate);
    Serial.print(" RHIGH=");
    Serial.print(ref120_candidate);
    Serial.print(" ratio=");
    Serial.print(ratio, 5);
    if (rejectReason & ADS1263_REF_REJ_RATIO)
    {
      Serial.print(" expect=");
      Serial.print(ratioExpected, 5);
      Serial.print(" lim=");
      Serial.print(ratioMinOk, 5);
      Serial.print("..");
      Serial.print(ratioMaxOk, 5);
    }
    Serial.print(" reason=0x");
    Serial.print(rejectReason, HEX);
    if (!jumpOk)
    {
      Serial.print(" jump=");
      Serial.print(refJumpMaxPct, 4);
      Serial.print("%");
    }
    Serial.println();
  }

  // Nach jeder Paarpruefung einen neuen Satz verlangen.
  ref100_candidate_valid = false;
  ref120_candidate_valid = false;
}

// =========================================================================
// REFERENZ-PUFFER MITTELWERT
// =========================================================================
static void adsAktualisiereRefMittelwerteAusPuffer()
{
  int64_t tempSum100 = 0;
  int64_t tempSum120 = 0;
  uint8_t gueltige100 = 0;
  uint8_t gueltige120 = 0;

  for (uint8_t i = 0; i < ADS1263_REF_BUF_SIZE; i++)
  {
    if (puffer100R[i] != 0)
    {
      tempSum100 += puffer100R[i];
      gueltige100++;
    }

    if (puffer120R[i] != 0)
    {
      tempSum120 += puffer120R[i];
      gueltige120++;
    }
  }

  if (gueltige100 > 0 && gueltige120 > 0)
  {
    int32_t mittel100 = (int32_t)(tempSum100 / gueltige100);
    int32_t mittel120 = (int32_t)(tempSum120 / gueltige120);

    // Nur sinnvolle Mittelwerte uebernehmen. Die Einzelwerte wurden bereits
    // vor dem Eintrag in den Puffer als plausibles Ref Low/Ref High-Paar geprueft.
    if (mittel100 > 0 && mittel120 > mittel100)
    {
      adcRawRef100R = mittel100;
      adcRawRef120R = mittel120;
    }
  }
}

// =========================================================================
// ADC ROHWERTE LESEN
// =========================================================================
void leseAdcRohwerte()
{
#if ADS1263_PAUSE_MAIN_WHEN_ADC_INFO
  // ADC-Info/Diagnose hat exklusiven Zugriff auf den ADS1263.
  // Der Hauptpfad wuerde sonst waehrend des Temp-Tests wieder echte Kanaele
  // setzen und die Diagnose verfälschen.
  if (mode_display == 3)
  {
    return;
  }
#endif

  // =========================================================================
  // A) SIMULATIONSMODUS
  // =========================================================================
  if (!ads1263_bereit)
  {
    static unsigned long letzterSimTakt = 0;

    if (millis() - letzterSimTakt >= 62)
    {
      letzterSimTakt = millis();

      adcRawPt100_1    = 1084000;
      adcRawPt100_2    = 1090000;
      adcRawRef100R    = 1000000;
      adcRawRef120R    = 1200000;
      adcRawPhotodiode = 50000;

    }

    return;
  }

  // ADC2 / Photodiode laeuft unabhaengig vom ADC1-Messblock.
  // Der Service ist zeitgesteuert auf 400 SPS und blockiert nur fuer einen kurzen RREG-Frame.
  ads1263ServiceAdc2();

  // =========================================================================
  // B) DYNAMISCHE NULLUNG
  // =========================================================================
#if ADS1263_DYNAMIC_SFOCAL_ENABLE
  // Zyklischer SFOCAL nur in den Menue-Modi 10/30/60 min. In den Modi
  // "Nur beim Start" und "Synchron mit Auto-Cal" gibt es hier keine eigene Luecke.
  const uint32_t sfocalIntervalMs = adc1SfocalIntervalMs();
  if (sfocalIntervalMs > 0UL && (uint32_t)(millis() - letzteNullAbgleichZeit) >= sfocalIntervalMs)
  {
    if (regelungIstRuhig())
    {
      ads1263SfocalRunAndMark();
      return;
    }
    else
    {
      // Nur selten melden, damit Serial nicht zugespammt wird
      if (millis() - letzterNullVersuchLog >= 30000UL)
      {
        letzterNullVersuchLog = millis();
        Serial.println("ADS1263: SFOCAL faellig, warte auf ruhige Regelung...");
      }
    }
  }
#endif

  // =========================================================================
  // C) REFERENZ-MITTELWERTE
  // =========================================================================
  // Sicherheits-/Refresh-Pfad: normalerweise werden die Ref-Mittelwerte bereits
  // direkt nach jedem gueltigen Ref-Paar aktualisiert.
  if (millis() - letzteMittelWertZeit >= 10000UL)
  {
    letzteMittelWertZeit = millis();
    adsAktualisiereRefMittelwerteAusPuffer();
  }

  // =========================================================================
  // D) MESS-TAKT FUER STATE-MACHINE
  // =========================================================================
#if ADS1263_DRDY_INTERRUPT_TICKS_ENABLE
  uint16_t irqTicks = holeDrdyTicks();
  uint16_t ticks = irqTicks;
#else
  (void)holeDrdyTicks();
  uint16_t irqTicks = 0;
  uint16_t ticks = 0;
#endif

  ads1263ReadTriggeredByDrdyIrq = (irqTicks > 0);

#if ADS1263_TIMED_DRDY_FALLBACK_TEST
  // Fallback bleibt als Sicherheitsnetz. Nach einer ADC1-Neusynchronisation
  // wird ads1263LastFallbackTickMs zurueckgesetzt, damit der Fallback nicht
  // sofort einen kompletten Settle-Zyklus vortaeuscht.
  if (ticks == 0 && (millis() - ads1263LastFallbackTickMs >= ADS1263_TIMED_DRDY_FALLBACK_MS))
  {
    ads1263LastFallbackTickMs = millis();
    ticks = ADS1263_SETTLE_TICKS_AFTER_MUX;
    ads1263ReadTriggeredByDrdyIrq = false;
  }
#endif

  if (ticks == 0)
  {
    ads1263ReadTriggeredByDrdyIrq = false;
    return;
  }

  addiereDrdyTicks(ticks);

  // =========================================================================
  // E) STATE-MACHINE
  // =========================================================================
  switch (maintStep)
  {
    case 0:
    {
      // V0.50.0_5 TEST: Pt100 nicht mehr blockweise ++-- messen,
      // sondern je Messwert direkt +/-. Reihenfolge im Pt100-Teil:
      //   Spiegel+ -> Spiegel- -> Umgebung+ -> Umgebung-
      // Dadurch liegen die beiden Stromrichtungen eines Kanals zeitlich dichter
      // zusammen; Thermospannungs-/Driftanteile zwischen + und - werden kleiner.
      // Ref-Kaskade und Statistik bleiben unverändert: pro Messzyklus wird
      // weiterhin genau ein Referenz-Einzelwert gelesen.
      if (!adsLinkCheckAtBlockStart("MESSBLOCK"))
      {
        drdyCounter = 0;
        break;
      }

      ads1263StatsCycleStart();
      ads1263PtFilterStartBlock();

      // Spiegel Pt100, positive Stromrichtung vorbereiten.
      digitalWriteFast(pinCurrRev, LOW);
      adsSetInputMux(ADS1262_AIN2, ADS1262_AIN3);
      adsWriteRegister(0x02, ADS1263_INTERFACE_DATA4);
      ads1263SyncAdc1AfterExternalChange();

      drdyCounter = 0;
      maintStep = 1;
    }
    break;

    case 1:
    {
      const uint8_t result = ads1263PtFilterReadStep(&pt100_1_pos, "PT100_1+");
      if (result == 2U) break;
      if (result == 1U)
      {
        // Gleicher Kanal, direkt negative Stromrichtung vorbereiten.
        digitalWriteFast(pinCurrRev, HIGH);
        adsSetInputMux(ADS1262_AIN2, ADS1262_AIN3);
        adsWriteRegister(0x02, ADS1263_INTERFACE_DATA4);
        ads1263SyncAdc1AfterExternalChange();

        drdyCounter = 0;
        maintStep = 2;
      }
    }
    break;

    case 2:
    {
      const uint8_t result = ads1263PtFilterReadStep(&pt100_1_neg, "PT100_1-");
      if (result == 2U) break;
      if (result == 1U)
      {
        // Umgebung Pt100, positive Stromrichtung vorbereiten.
        digitalWriteFast(pinCurrRev, LOW);
        adsSetInputMux(ADS1262_AIN3, ADS1262_AIN4);
        adsWriteRegister(0x02, ADS1263_INTERFACE_DATA4);
        ads1263SyncAdc1AfterExternalChange();

        drdyCounter = 0;
        maintStep = 3;
      }
    }
    break;

    case 3:
    {
      const uint8_t result = ads1263PtFilterReadStep(&pt100_2_pos, "PT100_2+");
      if (result == 2U) break;
      if (result == 1U)
      {
        // Gleicher Kanal, direkt negative Stromrichtung vorbereiten.
        digitalWriteFast(pinCurrRev, HIGH);
        adsSetInputMux(ADS1262_AIN3, ADS1262_AIN4);
        adsWriteRegister(0x02, ADS1263_INTERFACE_DATA4);
        ads1263SyncAdc1AfterExternalChange();

        drdyCounter = 0;
        maintStep = 4;
      }
    }
    break;

    case 4:
    {
      const uint8_t result = ads1263PtFilterReadStep(&pt100_2_neg, "PT100_2-");
      if (result == 2U) break;
      if (result == 1U)
      {
        // Differenzbildung gegen Thermospannungen.
        // V60/V61: erst plausibilisieren, dann beide PT-Werte gemeinsam uebernehmen.
        adsStatCyclePtValid = adsUebernehmePtSatzWennPlausibel();

        // Ref-Kaskade wie bisher: pro Messzyklus genau ein Ref-Einzelwert.
        if (refKaskade == 0)
        {
          digitalWriteFast(pinCurrRev, LOW);
          adsSetInputMux(ADS1262_AIN6, ADS1262_AIN5);   // Ref Low pos: Kanalzuordnung getauscht
        }
        else if (refKaskade == 1)
        {
          digitalWriteFast(pinCurrRev, HIGH);
          adsSetInputMux(ADS1262_AIN6, ADS1262_AIN5);   // Ref Low neg: Kanalzuordnung getauscht
        }
        else if (refKaskade == 2)
        {
          digitalWriteFast(pinCurrRev, LOW);
          adsSetInputMux(ADS1262_AIN7, ADS1262_AIN6);   // Ref High pos: Kanalzuordnung getauscht
        }
        else
        {
          digitalWriteFast(pinCurrRev, HIGH);
          adsSetInputMux(ADS1262_AIN7, ADS1262_AIN6);   // Ref High neg: Kanalzuordnung getauscht
        }

        adsWriteRegister(0x02, ADS1263_INTERFACE_DATA4);
        ads1263SyncAdc1AfterExternalChange();
        drdyCounter = 0;
        maintStep = 5;
      }
    }
    break;

    case 5:
    {
      if (drdyCounter >= ADS1263_SETTLE_TICKS_AFTER_MUX)
      {
        if (refKaskade == 0)
        {
          if (!adsReadRawDirectSafe(&ref100r_pos_tmp, "REF100+"))
          {
            drdyCounter = 0;
            break;
          }
        }
        else if (refKaskade == 1)
        {
          if (!adsReadRawDirectSafe(&ref100r_neg_tmp, "REF100-"))
          {
            drdyCounter = 0;
            break;
          }
          {
            int32_t ref100neu = (int32_t)(((int64_t)ref100r_pos_tmp - (int64_t)ref100r_neg_tmp) / 2);
            if (ref100neu > 0)
            {
              ref100_candidate = ref100neu;
              ref100_candidate_valid = true;
              adsVersucheRefPaarUebernahme();
            }
            else
            {
              adsRangeReject("REFLOW Kandidat", ref100neu);
            }
          }
        }
        else if (refKaskade == 2)
        {
          if (!adsReadRawDirectSafe(&ref120r_pos_tmp, "REF120+"))
          {
            drdyCounter = 0;
            break;
          }
        }
        else
        {
          if (!adsReadRawDirectSafe(&ref120r_neg_tmp, "REF120-"))
          {
            drdyCounter = 0;
            break;
          }
          {
            int32_t ref120neu = (int32_t)(((int64_t)ref120r_pos_tmp - (int64_t)ref120r_neg_tmp) / 2);
            if (ref120neu > 0)
            {
              ref120_candidate = ref120neu;
              ref120_candidate_valid = true;
              adsVersucheRefPaarUebernahme();
            }
            else
            {
              adsRangeReject("REFHIGH Kandidat", ref120neu);
            }
          }
        }

        refKaskade = (refKaskade + 1) % 4;
        ads1263StatsCycleEnd(adsStatCyclePtValid);
        drdyCounter = 0;
        maintStep = 0;
      }
    }
    break;

    default:
    {
      maintStep = 0;
      drdyCounter = 0;
    }
    break;
  }

}
