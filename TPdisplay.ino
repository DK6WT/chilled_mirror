/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPdisplay.ino
 * Zweck: RA8875-Anzeige, Hauptscreen sowie Diagnose- und ADC-Info-Seiten.
 *
 * Abgeleitet aus: PSWRdisplay.ino
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2015 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

//*********************************************************************************
//**
//** DISPLAY / TFT SYSTEM - STABILISIERTE VERSION
//**
//** FIXES:
//** ------------------------------------------------------------------------------
//** ✓ RA8875 Timing stabilisiert
//** ✓ transfer() Konflikte entschärft
//** ✓ nullptr Schutz eingebaut
//** ✓ clearScreen() entfernt
//** ✓ Delays nach transfer()
//** ✓ Direkte Zeichenoperationen entkoppelt
//** ✓ SETUP Button stabilisiert
//** ✓ Weniger SPI-Kollisionen
//**
//*********************************************************************************

#include <Arduino.h>
#include <RA8875.h>
#include <TimeLib.h>
#include <string.h>
#include <math.h>

#include "TP_T.h"
#include "TPlanguage.h"
#include "TPtft.h"

// Selten verwendete Diagnoseansichten liegen bewusst im QSPI-Flash.
// noinline verhindert, dass LTO ihre groesseren Funktionskoerper wieder
// in den schnellen ITCM-Aufrufer einbettet und damit RAM1 verbraucht.
#define DISPLAY_FLASHMEM_NOINLINE FLASHMEM __attribute__((noinline))

extern RA8875 tft;

// ----------------------------------------------------------------------------
// TEXTBOXEN
// ----------------------------------------------------------------------------

extern class TextBox VirtLCDUeberschrift;
extern class TextBox VirtLCDTemperaturen;
extern TextBox* VirtLCDMessage;
extern class TextBox VirtLCDGrossanzeige;
extern class TextBox VirtLCDStatuszeile;
extern class TextBox VirtLCDChartZusatzwert;
extern class TextBox RTC_PM;

// ----------------------------------------------------------------------------
// FARBEN
// ----------------------------------------------------------------------------

#ifndef WHITE
#define WHITE 0xFFFF
#endif

#ifndef BLACK
#define BLACK 0x0000
#endif

#ifndef YELLOW
#define YELLOW 0xFFE0
#endif

// ----------------------------------------------------------------------------
// MESSWERTE
// ----------------------------------------------------------------------------

extern float tempSpiegel;
extern float tempUmgebung;
extern float relativeFeuchte;
extern float präziserTaupunkt;
extern float baroDruckHPa;

extern float ohmPt100_1;
extern float ohmPt100_2;
extern float ohmRef100R;
extern float ohmRef120R;

extern float aktuelleLedStromVorgabe;
extern float peltierSollWert;

extern uint8_t ablaufStatus;
extern unsigned long statusTimer;
extern uint8_t aktuellerModus;   // 0 = Aus, 1 = Kuehlen, 2 = Heizen

extern double amp_avg;

extern uint16_t Menu_exit_timer;

extern uint8_t mode_display;
extern var_t R;

extern volatile int32_t adcRawPt100_1;
extern volatile int32_t adcRawPt100_2;
extern volatile int32_t adcRawRef100R;
extern volatile int32_t adcRawRef120R;
extern volatile int32_t adcRawPhotodiode;
extern volatile int32_t adcRawPeltierStrom;
extern volatile int32_t adcRawPeltierStromKuehlen;
extern volatile int32_t adcRawPeltierStromHeizen;
extern volatile int32_t adcRawPeltierStromMax;

extern bool ads1263_bereit;

extern bool ads1263RefIsOld();
extern bool ads1263RefIsError();
extern uint8_t ads1263InfoGetRows();
extern const char* ads1263InfoGetCmd(uint8_t row);
extern const char* ads1263InfoGetAns(uint8_t row);
extern const char* ads1263InfoGetStatus();
extern bool ads1263InfoGetOk();
extern void ads1263LinkTestTick();
extern const char* ads1263SpiGetLabel();
extern uint32_t ads1263GetSettleMs();
extern const char* ads1263GetGainLabel();
extern float ads1263StatsCycleRate();
extern float ads1263StatsValidRate();
extern float ads1263StatsMaxCycleMs();
extern float ads1263StatsIdRate();
extern float ads1263StatsTimeoutRate();
extern float ads1263StatsRngRate();
extern float ads1263StatsRawDiscardRate(uint8_t ch);
extern float ads1263StatsPairDiscardRate(uint8_t pair);
extern bool ads1263StatsAdcMinMax(uint8_t ch, int32_t* minOut, int32_t* maxOut);
extern bool ads1263StatsTempMinMax(uint8_t ch, float* minOut, float* maxOut);
extern bool ads1263StatsRatioMinMax(float* minOut, float* maxOut);
extern uint32_t ads1263StatsDrdyIsrCount();
extern uint32_t ads1263StatsDrdyGlitchCount();
extern uint32_t ads1263StatsDrdyBusyCount();
extern uint32_t ads1263StatsDrdyGapUs();
extern int32_t ads1263Adc2GetRaw();
extern int32_t ads1263Adc2GetAvg();
extern int32_t ads1263Adc2GetDark();
extern int32_t ads1263Adc2GetNet();
extern bool ads1263Adc2DarkValid();
extern bool ads1263Adc2DarkMeasuring();
extern uint32_t ads1263Adc2DarkAgeMs();
extern uint8_t ads1263Adc2GetBufCount();
extern uint16_t ads1263Adc2GetBufSize();
extern float ads1263Adc2GetRate();
extern bool ads1263Adc2GetMinMax(int32_t* minOut, int32_t* maxOut);
extern bool mcp3202_online;
extern bool mcp3202_fehler;
extern bool drv8873StabiBereit;
extern bool drv8873SpiIsOk();
extern bool drv8873ConfigIsOk();
extern uint8_t drv8873GetLastFaultRegister();
extern uint8_t drv8873GetLastDiagRegister();
extern uint8_t drv8873GetLastIc1Register();
extern uint8_t drv8873GetLastIc3Register();
extern uint8_t drv8873GetLastIc4Register();
extern uint16_t drv8873GetSpiErrorCounter();
extern uint16_t drv8873GetLastFaultRaw();
extern uint16_t drv8873GetLastDiagRaw();
extern uint16_t drv8873GetLastIc1Raw();
extern uint16_t drv8873GetLastIc3Raw();
extern uint16_t drv8873GetLastIc4Raw();
extern const char* safetyGetFaultText();
extern const char* safetyGetStopStatusText();
extern const byte pinNfault;

extern float optikReflexion;
extern float optikTrockenReferenz;
extern float peltierStromSoll;
extern float integralFehler;
extern float letzterFehler;
extern float getOptikZielRohwert();

extern void ConfigMenu();
extern void interfaceGetDisplayIp(uint8_t ip[4], bool* valid, bool* enabled);
extern bool interfaceSdLoggingEnabled(void);
extern uint8_t sdLogGetStatus(void);
extern bool sdLogWriteBlinkActive(void);
extern uint8_t sdStorageActivityState(void);
extern bool sdStorageWriteBlinkActive(void);

extern bool sensorFanIsEnabled(void);
extern bool sensorFanTachoMissing(void);
extern const char* sensorFanStatusText(void);
extern bool fanButtonMainGhostClearActive(void);
extern void fanButtonMainGhostClearDoneOne(void);
extern bool safetyIsFaultActive(void);
extern bool opticHealthMainWarningActive(void);
extern const char* opticHealthMainStatusTextDE(void);
extern const char* opticHealthMainStatusTextEN(void);
extern uint8_t opticHealthGetStatus(void);
extern void loopDebugGetMetricsExt(uint16_t* hz, uint32_t* max_us, uint32_t* peak10_us, bool* enabled);
extern float serialFlowLastValueLMin(void);
extern bool interfaceFlowDisplayEnabled(void);
extern const char* interfaceFlowDisplayUnitText(void);
extern bool interfaceTExternalDisplayEnabled(void);
extern bool interfaceAlmemoAnyChannelEnabled(void);
extern bool interfaceAlmemoChannelEnabled(uint8_t index);
extern uint8_t interfaceAlmemoDisplayIndex(void);
extern const char* interfaceAlmemoDisplayLabel(uint8_t index);
extern const char* interfaceAlmemoDisplayUnitShort(uint8_t index);
extern float serialExternalTempLastValueC(void);
extern float serialAlmemoLastValue(uint8_t index);
extern bool alarmVisualActive(void);
extern void alarmResetDisplayCache(void);
extern uint8_t mainScreenLayoutGet(void);

// ----------------------------------------------------------------------------
// TEXTBUFFER
// ----------------------------------------------------------------------------

static char strBuf[40];

// Einheitliche Anzeige fuer noch nicht vorhandene bzw. ungueltige Messwerte.
// Die Zahl der Nachkommastellen bleibt sichtbar, der ganzzahlige Teil wird
// unabhaengig von der Feldbreite immer als "--" dargestellt. Vorangestellte
// Leerzeichen erhalten die bisherige Ausrichtung der festen Textfelder.
static void DISPLAY_FLASHMEM_NOINLINE displayFormatValueOrDashes(char* out,
                                       size_t outSize,
                                       double value,
                                       int8_t width,
                                       uint8_t decimals)
{
    if (out == nullptr || outSize == 0) return;

    if (isfinite(value))
    {
        dtostrf(value, width, decimals, out);
        return;
    }

    char placeholder[20];
    size_t pos = 0;
    placeholder[pos++] = '-';
    placeholder[pos++] = '-';

    if (decimals > 0 && pos + 1 < sizeof(placeholder))
    {
        placeholder[pos++] = '.';
        for (uint8_t i = 0; i < decimals && pos + 1 < sizeof(placeholder); i++)
        {
            placeholder[pos++] = '-';
        }
    }
    placeholder[pos] = '\0';

    const size_t placeholderLen = strlen(placeholder);
    const size_t requestedWidth = (width > 0) ? (size_t)width : placeholderLen;
    const size_t padding = (requestedWidth > placeholderLen)
                         ? (requestedWidth - placeholderLen)
                         : 0;

    size_t outPos = 0;
    for (size_t i = 0; i < padding && outPos + 1 < outSize; i++)
    {
        out[outPos++] = ' ';
    }
    for (size_t i = 0; i < placeholderLen && outPos + 1 < outSize; i++)
    {
        out[outPos++] = placeholder[i];
    }
    out[outPos] = '\0';
}

void eraseDisplay(void);

// ============================================================================
// HAUPTDISPLAY-ANSICHTEN UND CHART-HISTORIE
// ============================================================================
// Ansicht 0: bisherige grosse Zahlenanzeige
// Ansicht 1: Verlauf der relativen Feuchte
// Ansicht 2: Verlauf des Taupunkts
// Ansicht 3: Verlauf der Umgebungstemperatur
//
// Die Historie laeuft unabhaengig vom angezeigten Screen permanent weiter.
// Je Zeitfenster bleiben es 1440 Punkte. Die Abtastintervalle sind so
// gewaehlt, dass 60 min, 4 h, 8 h und 24 h exakt abgedeckt werden.

enum MainDashboardView : uint8_t
{
    MAIN_DASH_VALUES    = 0,
    MAIN_DASH_RH        = 1,
    MAIN_DASH_DEW       = 2,
    MAIN_DASH_T_AMBIENT = 3
};

static uint8_t mainDashboardView = MAIN_DASH_VALUES;
static uint8_t mainDashboardLastLayout = 0xFF;

static bool mainDashboardAltValuesLayout()
{
    return mainScreenLayoutGet() == MAIN_SCREEN_LAYOUT_3VALUES;
}

static const uint16_t MAIN_CHART_HISTORY_POINTS = 1440;
static const uint8_t MAIN_CHART_RANGE_COUNT = 4;
static const uint32_t MAIN_CHART_ACCUMULATE_MS = 100UL;

static const uint32_t MAIN_CHART_SAMPLE_MS_BY_RANGE[MAIN_CHART_RANGE_COUNT] =
{
    2500UL,    // 60 min: 1440 x 2,5 s
    10000UL,   // 4 h:    1440 x 10 s
    20000UL,   // 8 h:    1440 x 20 s
    60000UL    // 24 h:   1440 x 60 s
};

static const char* const MAIN_CHART_RANGE_LABELS[MAIN_CHART_RANGE_COUNT] =
{
    "60 min", "4 h", "8 h", "24 h"
};

enum MainChartMetric : uint8_t
{
    MAIN_CHART_METRIC_DEW      = 0,
    MAIN_CHART_METRIC_RH       = 1,
    MAIN_CHART_METRIC_T_AMBIENT = 2
};

static uint8_t mainChartMetricForView()
{
    if (mainDashboardView == MAIN_DASH_RH) return MAIN_CHART_METRIC_RH;
    if (mainDashboardView == MAIN_DASH_T_AMBIENT) return MAIN_CHART_METRIC_T_AMBIENT;
    return MAIN_CHART_METRIC_DEW;
}

static bool mainChartMetricIsRh(uint8_t metric)
{
    return metric == MAIN_CHART_METRIC_RH;
}

static void mainChartAmbientName(char* out, size_t outSize)
{
    if (out == nullptr || outSize == 0) return;

    strncpy(out, T(TXT_MAIN_T_AMBIENT), outSize - 1);
    out[outSize - 1] = '\0';

    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == ':' || out[len - 1] == ' '))
    {
        out[--len] = '\0';
    }
}

static DMAMEM float mainChartHistoryDew[MAIN_CHART_RANGE_COUNT][MAIN_CHART_HISTORY_POINTS];
static DMAMEM float mainChartHistoryRh[MAIN_CHART_RANGE_COUNT][MAIN_CHART_HISTORY_POINTS];
static DMAMEM float mainChartHistoryAmbient[MAIN_CHART_RANGE_COUNT][MAIN_CHART_HISTORY_POINTS];
static uint16_t mainChartHistoryWrite[MAIN_CHART_RANGE_COUNT] = {0, 0, 0, 0};
static uint16_t mainChartHistoryCount[MAIN_CHART_RANGE_COUNT] = {0, 0, 0, 0};
static uint32_t mainChartHistoryTotal[MAIN_CHART_RANGE_COUNT] = {0, 0, 0, 0};
static uint32_t mainChartHistoryRevision[MAIN_CHART_RANGE_COUNT] = {0, 0, 0, 0};

static uint8_t mainChartActiveRange = 0;
static uint32_t mainChartWindowStartMs[MAIN_CHART_RANGE_COUNT] = {0, 0, 0, 0};
static uint32_t mainChartLastAccumulateMs = 0;
static double mainChartDewSum[MAIN_CHART_RANGE_COUNT] = {0.0, 0.0, 0.0, 0.0};
static double mainChartRhSum[MAIN_CHART_RANGE_COUNT] = {0.0, 0.0, 0.0, 0.0};
static double mainChartAmbientSum[MAIN_CHART_RANGE_COUNT] = {0.0, 0.0, 0.0, 0.0};
static uint16_t mainChartDewSamples[MAIN_CHART_RANGE_COUNT] = {0, 0, 0, 0};
static uint16_t mainChartRhSamples[MAIN_CHART_RANGE_COUNT] = {0, 0, 0, 0};
static uint16_t mainChartAmbientSamples[MAIN_CHART_RANGE_COUNT] = {0, 0, 0, 0};

// Geometrie der grossen Chartansicht.
static const int16_t MAIN_CHART_FRAME_X = 12;
static const int16_t MAIN_CHART_FRAME_Y = 50;
static const int16_t MAIN_CHART_FRAME_W = 776;
static const int16_t MAIN_CHART_FRAME_H = 286;
static const int16_t MAIN_CHART_FRAME_R = 10;

static const int16_t MAIN_CHART_PLOT_X = 108;
static const int16_t MAIN_CHART_PLOT_Y = 100;
static const int16_t MAIN_CHART_PLOT_W = 640;
static const int16_t MAIN_CHART_PLOT_H = 164;

// Fuer den Vergleich alt/neu werden nur die gerenderten Pixelkoordinaten
// gepuffert. Die grossen Messwertpuffer liegen ebenfalls bewusst in RAM2.
static DMAMEM int16_t mainChartOldAvgY[MAIN_CHART_PLOT_W];
static DMAMEM int16_t mainChartOldMinY[MAIN_CHART_PLOT_W];
static DMAMEM int16_t mainChartOldMaxY[MAIN_CHART_PLOT_W];
static DMAMEM uint8_t mainChartOldValid[MAIN_CHART_PLOT_W];
static DMAMEM int16_t mainChartNewAvgY[MAIN_CHART_PLOT_W];
static DMAMEM int16_t mainChartNewMinY[MAIN_CHART_PLOT_W];
static DMAMEM int16_t mainChartNewMaxY[MAIN_CHART_PLOT_W];
static DMAMEM uint8_t mainChartNewValid[MAIN_CHART_PLOT_W];
static DMAMEM float mainChartColumnSum[MAIN_CHART_PLOT_W];
static DMAMEM float mainChartColumnMin[MAIN_CHART_PLOT_W];
static DMAMEM float mainChartColumnMax[MAIN_CHART_PLOT_W];
static DMAMEM uint16_t mainChartColumnCount[MAIN_CHART_PLOT_W];

static bool mainChartScreenWasDrawn = false;
static bool mainChartCurveWasDrawn = false;
static bool mainChartScaleValid = false;
static float mainChartAxisMin = 0.0f;
static float mainChartAxisMax = 1.0f;
static uint32_t mainChartLastHistoryRevision = 0;
static uint32_t mainChartLastPixelEpoch = 0;
static uint8_t mainChartLastView = 0xFF;
static uint32_t mainChartLastTextUpdateMs = 0;
// Die drei dynamischen Zahlenfelder im Chart besitzen nur einen kleinen
// Sichtcache mit je neun Zeichen. Wie bei TextBox werden ausschliesslich
// geaenderte Zeichen erst schwarz geloescht und danach neu geschrieben.
// Dadurch bleibt das optische Verhalten gleich, ohne drei grosse TextBox-
// Objekte mit ihren festen 1801-Byte-Puffern anzulegen.
static const uint8_t MAIN_CHART_VALUE_CHARS = 9;
static DMAMEM char mainChartCurrentValueCache[MAIN_CHART_VALUE_CHARS + 1];
static DMAMEM char mainChartMinValueCache[MAIN_CHART_VALUE_CHARS + 1];
static DMAMEM char mainChartMaxValueCache[MAIN_CHART_VALUE_CHARS + 1];
static int16_t mainChartCurrentValueX = 0;
static int16_t mainChartMinValueX = 0;
static int16_t mainChartMaxValueX = 0;
static bool mainChartValueFieldsReady = false;

// Monospaced-Zeichenweite von DroidSansMono_16 laut Font-Deltawert.
static const int16_t MAIN_CHART_FONT16_ADV = 13;

static void resetMainChartDisplayCache()
{
    mainChartScreenWasDrawn = false;
    mainChartCurveWasDrawn = false;
    mainChartScaleValid = false;
    mainChartLastHistoryRevision = 0;
    mainChartLastPixelEpoch = 0;
    mainChartLastView = 0xFF;
    mainChartLastTextUpdateMs = 0;
    mainChartValueFieldsReady = false;
    VirtLCDChartZusatzwert.clear();
    VirtLCDChartZusatzwert.invalidate();
    memset(mainChartCurrentValueCache, 0, sizeof(mainChartCurrentValueCache));
    memset(mainChartMinValueCache, 0, sizeof(mainChartMinValueCache));
    memset(mainChartMaxValueCache, 0, sizeof(mainChartMaxValueCache));


    memset(mainChartOldValid, 0, sizeof(mainChartOldValid));
}

static uint8_t mainChartNormalizeRange(uint8_t range)
{
    return (range < MAIN_CHART_RANGE_COUNT) ? range : 0;
}

static uint32_t mainChartSampleMsForRange(uint8_t range)
{
    return MAIN_CHART_SAMPLE_MS_BY_RANGE[mainChartNormalizeRange(range)];
}

static const char* mainChartRangeLabel(uint8_t range)
{
    return MAIN_CHART_RANGE_LABELS[mainChartNormalizeRange(range)];
}

static uint16_t mainChartOldestIndex(uint8_t range)
{
    range = mainChartNormalizeRange(range);
    return (mainChartHistoryCount[range] < MAIN_CHART_HISTORY_POINTS)
         ? 0
         : mainChartHistoryWrite[range];
}

static void mainChartStorePoint(uint8_t range, float dew, float rh, float ambient)
{
    range = mainChartNormalizeRange(range);

    mainChartHistoryDew[range][mainChartHistoryWrite[range]] = dew;
    mainChartHistoryRh[range][mainChartHistoryWrite[range]] = rh;
    mainChartHistoryAmbient[range][mainChartHistoryWrite[range]] = ambient;

    mainChartHistoryWrite[range]++;
    if (mainChartHistoryWrite[range] >= MAIN_CHART_HISTORY_POINTS)
    {
        mainChartHistoryWrite[range] = 0;
    }

    if (mainChartHistoryCount[range] < MAIN_CHART_HISTORY_POINTS)
    {
        mainChartHistoryCount[range]++;
    }

    mainChartHistoryTotal[range]++;
    mainChartHistoryRevision[range]++;
}

void chartHistoryTask()
{
    uint32_t now = millis();

    if (mainChartLastAccumulateMs == 0)
    {
        mainChartLastAccumulateMs = now;
        for (uint8_t r = 0; r < MAIN_CHART_RANGE_COUNT; r++)
        {
            mainChartWindowStartMs[r] = now;
        }
        return;
    }

    if ((uint32_t)(now - mainChartLastAccumulateMs) >= MAIN_CHART_ACCUMULATE_MS)
    {
        mainChartLastAccumulateMs = now;

        if (ads1263_bereit)
        {
            const bool dewValid = !mcp3202_fehler && isfinite(präziserTaupunkt);
            const bool rhValid = !mcp3202_fehler && isfinite(relativeFeuchte);
            const bool ambientValid = isfinite(tempUmgebung);

            for (uint8_t r = 0; r < MAIN_CHART_RANGE_COUNT; r++)
            {
                if (dewValid)
                {
                    mainChartDewSum[r] += präziserTaupunkt;
                    mainChartDewSamples[r]++;
                }

                if (rhValid)
                {
                    mainChartRhSum[r] += relativeFeuchte;
                    mainChartRhSamples[r]++;
                }

                if (ambientValid)
                {
                    mainChartAmbientSum[r] += tempUmgebung;
                    mainChartAmbientSamples[r]++;
                }
            }
        }
    }

    for (uint8_t r = 0; r < MAIN_CHART_RANGE_COUNT; r++)
    {
        if (mainChartWindowStartMs[r] == 0)
        {
            mainChartWindowStartMs[r] = now;
            continue;
        }

        const uint32_t sampleMs = mainChartSampleMsForRange(r);
        if ((uint32_t)(now - mainChartWindowStartMs[r]) < sampleMs)
        {
            continue;
        }

        float dew = (mainChartDewSamples[r] > 0)
                  ? (float)(mainChartDewSum[r] / (double)mainChartDewSamples[r])
                  : NAN;
        float rh = (mainChartRhSamples[r] > 0)
                 ? (float)(mainChartRhSum[r] / (double)mainChartRhSamples[r])
                 : NAN;
        float ambient = (mainChartAmbientSamples[r] > 0)
                      ? (float)(mainChartAmbientSum[r] / (double)mainChartAmbientSamples[r])
                      : NAN;

        mainChartStorePoint(r, dew, rh, ambient);

        mainChartDewSum[r] = 0.0;
        mainChartRhSum[r] = 0.0;
        mainChartAmbientSum[r] = 0.0;
        mainChartDewSamples[r] = 0;
        mainChartRhSamples[r] = 0;
        mainChartAmbientSamples[r] = 0;

        // Bei normalem Betrieb bleibt das Raster exakt erhalten. Nach einer
        // sehr langen Blockade wird nicht mit vielen Leerwerten nachgefuellt,
        // sondern sauber ab jetzt weitergemessen.
        mainChartWindowStartMs[r] += sampleMs;
        if ((uint32_t)(now - mainChartWindowStartMs[r]) >= sampleMs)
        {
            mainChartWindowStartMs[r] = now;
        }
    }
}

static float mainChartGetChronological(uint8_t range, uint16_t pos, uint8_t metric)
{
    range = mainChartNormalizeRange(range);
    if (pos >= mainChartHistoryCount[range])
    {
        return NAN;
    }

    uint16_t idx = (uint16_t)(mainChartOldestIndex(range) + pos);
    if (idx >= MAIN_CHART_HISTORY_POINTS)
    {
        idx = (uint16_t)(idx - MAIN_CHART_HISTORY_POINTS);
    }

    if (metric == MAIN_CHART_METRIC_RH) return mainChartHistoryRh[range][idx];
    if (metric == MAIN_CHART_METRIC_T_AMBIENT) return mainChartHistoryAmbient[range][idx];
    return mainChartHistoryDew[range][idx];
}

// Lesender Zugriff fuer den Web-Chart. Die Historie bleibt weiterhin allein
// Eigentum des Displaymoduls; Ethernet erhaelt nur konsistente Momentaufnahmen.
uint16_t mainChartHistoryCapacity()
{
    return MAIN_CHART_HISTORY_POINTS;
}

uint8_t mainChartHistoryRangeCount()
{
    return MAIN_CHART_RANGE_COUNT;
}

uint8_t mainChartHistoryActiveRange()
{
    return mainChartActiveRange;
}

uint32_t mainChartHistorySampleMs(uint8_t range)
{
    return mainChartSampleMsForRange(range);
}

uint16_t mainChartHistoryCountValue(uint8_t range)
{
    range = mainChartNormalizeRange(range);
    return mainChartHistoryCount[range];
}

uint32_t mainChartHistoryFirstSequence(uint8_t range)
{
    range = mainChartNormalizeRange(range);
    return mainChartHistoryTotal[range] - (uint32_t)mainChartHistoryCount[range];
}

uint32_t mainChartHistoryNextSequence(uint8_t range)
{
    range = mainChartNormalizeRange(range);
    return mainChartHistoryTotal[range];
}

bool mainChartHistoryGetPoint(uint8_t range, uint32_t sequence, float* dew, float* rh, float* ambient)
{
    range = mainChartNormalizeRange(range);
    const uint32_t first = mainChartHistoryFirstSequence(range);
    const uint32_t next = mainChartHistoryTotal[range];

    if (sequence < first || sequence >= next)
    {
        return false;
    }

    const uint32_t pos = sequence - first;
    uint16_t idx = (uint16_t)(mainChartOldestIndex(range) + (uint16_t)pos);
    if (idx >= MAIN_CHART_HISTORY_POINTS)
    {
        idx = (uint16_t)(idx - MAIN_CHART_HISTORY_POINTS);
    }

    if (dew != nullptr) *dew = mainChartHistoryDew[range][idx];
    if (rh != nullptr) *rh = mainChartHistoryRh[range][idx];
    if (ambient != nullptr) *ambient = mainChartHistoryAmbient[range][idx];
    return true;
}

uint8_t mainDisplayGetView()
{
    return mainDashboardView;
}

bool mainDisplayFrameHit(uint16_t x, uint16_t y)
{
    if (mode_display != 0)
    {
        return false;
    }

    if (mainDashboardView == MAIN_DASH_VALUES && !mainDashboardAltValuesLayout())
    {
        return (x >= 115 && x < 685 && y >= 88 && y < 298);
    }

    return (x >= MAIN_CHART_FRAME_X &&
            x < (MAIN_CHART_FRAME_X + MAIN_CHART_FRAME_W) &&
            y >= MAIN_CHART_FRAME_Y &&
            y < (MAIN_CHART_FRAME_Y + MAIN_CHART_FRAME_H));
}

bool mainDisplayChartRangeHit(uint16_t x, uint16_t y)
{
    if (mode_display != 0 || mainDashboardView == MAIN_DASH_VALUES)
    {
        return false;
    }

    // Touchfeld ueber der Zeitangabe rechts oben im Chart.
    return (x >= 628 && x < 756 && y >= 52 && y < 92);
}

void mainDisplayCycleChartRange()
{
    mainChartActiveRange++;
    if (mainChartActiveRange >= MAIN_CHART_RANGE_COUNT)
    {
        mainChartActiveRange = 0;
    }

    eraseDisplay();
    flag.menu_lcd_upd = true;
}

void mainDisplayCycleView()
{
    mainDashboardView++;
    if (mainDashboardView > MAIN_DASH_T_AMBIENT)
    {
        mainDashboardView = MAIN_DASH_VALUES;
    }

    eraseDisplay();
    flag.menu_lcd_upd = true;
}

struct MainChartScale
{
    bool valid;
    float dataMin;
    float dataMax;
    float axisMin;
    float axisMax;
};

static struct MainChartScale mainChartCalculateScale(uint8_t range, uint8_t metric)
{
    range = mainChartNormalizeRange(range);
    MainChartScale out;
    out.valid = false;
    out.dataMin = 0.0f;
    out.dataMax = 0.0f;
    out.axisMin = 0.0f;
    const bool rhMetric = mainChartMetricIsRh(metric);
    out.axisMax = rhMetric ? 1.0f : 0.2f;

    for (uint16_t i = 0; i < mainChartHistoryCount[range]; i++)
    {
        float v = mainChartGetChronological(range, i, metric);
        if (!isfinite(v)) continue;

        if (!out.valid)
        {
            out.dataMin = v;
            out.dataMax = v;
            out.valid = true;
        }
        else
        {
            if (v < out.dataMin) out.dataMin = v;
            if (v > out.dataMax) out.dataMax = v;
        }
    }

    if (!out.valid)
    {
        float current = NAN;
        if (metric == MAIN_CHART_METRIC_RH) current = relativeFeuchte;
        else if (metric == MAIN_CHART_METRIC_T_AMBIENT) current = tempUmgebung;
        else current = präziserTaupunkt;
        if (isfinite(current))
        {
            out.dataMin = current;
            out.dataMax = current;
            out.valid = true;
        }
        else
        {
            out.dataMin = 0.0f;
            out.dataMax = 0.0f;
        }
    }

    static const float dewSpans[] = {0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f, 200.0f};
    static const float rhSpans[]  = {1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f};

    const float* spans = rhMetric ? rhSpans : dewSpans;
    const uint8_t spanCount = rhMetric
                            ? (uint8_t)(sizeof(rhSpans) / sizeof(rhSpans[0]))
                            : (uint8_t)(sizeof(dewSpans) / sizeof(dewSpans[0]));

    float rawRange = out.dataMax - out.dataMin;
    float wantedRange = rawRange * 1.20f;
    float span = spans[spanCount - 1];

    for (uint8_t i = 0; i < spanCount; i++)
    {
        if (spans[i] + 0.000001f >= wantedRange)
        {
            span = spans[i];
            break;
        }
    }

    float step = span / 4.0f;
    float center = (out.dataMin + out.dataMax) * 0.5f;
    if (step > 0.0f)
    {
        center = roundf(center / step) * step;
    }

    out.axisMin = center - (span * 0.5f);
    out.axisMax = center + (span * 0.5f);

    // Durch das Runden des Mittelpunktes darf kein echter Messwert aus dem
    // sichtbaren Bereich fallen. Verschiebung erfolgt nur in Rasterstufen.
    while (out.dataMin < out.axisMin)
    {
        out.axisMin -= step;
        out.axisMax -= step;
    }
    while (out.dataMax > out.axisMax)
    {
        out.axisMin += step;
        out.axisMax += step;
    }

    return out;
}

bool mainChartHistoryGetScale(uint8_t range,
                              uint8_t metric,
                              bool* valid,
                              float* dataMin,
                              float* dataMax,
                              float* axisMin,
                              float* axisMax)
{
    const MainChartScale scale = mainChartCalculateScale(range, metric);

    if (valid != nullptr) *valid = scale.valid;
    if (dataMin != nullptr) *dataMin = scale.dataMin;
    if (dataMax != nullptr) *dataMax = scale.dataMax;
    if (axisMin != nullptr) *axisMin = scale.axisMin;
    if (axisMax != nullptr) *axisMax = scale.axisMax;
    return scale.valid;
}

static int16_t mainChartValueToY(float value, float axisMin, float axisMax)
{
    if (!isfinite(value) || axisMax <= axisMin)
    {
        return MAIN_CHART_PLOT_Y + MAIN_CHART_PLOT_H - 1;
    }

    float norm = (value - axisMin) / (axisMax - axisMin);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    int16_t y = (int16_t)lroundf((float)(MAIN_CHART_PLOT_H - 1) * (1.0f - norm));
    return MAIN_CHART_PLOT_Y + y;
}

static void mainChartBuildColumns(uint8_t metric, float axisMin, float axisMax)
{
    for (uint16_t x = 0; x < MAIN_CHART_PLOT_W; x++)
    {
        mainChartColumnSum[x] = 0.0f;
        mainChartColumnMin[x] = 0.0f;
        mainChartColumnMax[x] = 0.0f;
        mainChartColumnCount[x] = 0;
        mainChartNewValid[x] = 0;
    }

    // Bei noch nicht vollem Zeitfenster bleiben die fehlenden alten Werte
    // links leer. Der aktuelle Wert liegt immer am rechten Rand ("Jetzt").
    const uint8_t range = mainChartActiveRange;
    uint16_t emptySlots = MAIN_CHART_HISTORY_POINTS - mainChartHistoryCount[range];

    for (uint16_t i = 0; i < mainChartHistoryCount[range]; i++)
    {
        float v = mainChartGetChronological(range, i, metric);
        if (!isfinite(v)) continue;

        uint32_t slot = (uint32_t)emptySlots + i;
        uint16_t col = (uint16_t)((slot * (MAIN_CHART_PLOT_W - 1UL)) /
                                  (MAIN_CHART_HISTORY_POINTS - 1UL));

        if (mainChartColumnCount[col] == 0)
        {
            mainChartColumnMin[col] = v;
            mainChartColumnMax[col] = v;
        }
        else
        {
            if (v < mainChartColumnMin[col]) mainChartColumnMin[col] = v;
            if (v > mainChartColumnMax[col]) mainChartColumnMax[col] = v;
        }

        mainChartColumnSum[col] += v;
        mainChartColumnCount[col]++;
    }

    for (uint16_t x = 0; x < MAIN_CHART_PLOT_W; x++)
    {
        if (mainChartColumnCount[x] == 0)
        {
            mainChartNewValid[x] = 0;
            continue;
        }

        float avg = mainChartColumnSum[x] / (float)mainChartColumnCount[x];
        mainChartNewAvgY[x] = mainChartValueToY(avg, axisMin, axisMax);
        mainChartNewMinY[x] = mainChartValueToY(mainChartColumnMin[x], axisMin, axisMax);
        mainChartNewMaxY[x] = mainChartValueToY(mainChartColumnMax[x], axisMin, axisMax);
        mainChartNewValid[x] = 1;
    }
}

static void mainChartDrawGrid()
{
    for (uint8_t i = 0; i <= 4; i++)
    {
        int16_t y = MAIN_CHART_PLOT_Y +
                    (int16_t)(((uint32_t)i * (MAIN_CHART_PLOT_H - 1UL)) / 4UL);
        tft.drawFastHLine(MAIN_CHART_PLOT_X, y, MAIN_CHART_PLOT_W, DKBLUE);
    }

    tft.drawRect(MAIN_CHART_PLOT_X,
                 MAIN_CHART_PLOT_Y,
                 MAIN_CHART_PLOT_W,
                 MAIN_CHART_PLOT_H,
                 GREY);
}

static void mainChartDrawCurveArrays(const int16_t* avgY,
                                     const int16_t* minY,
                                     const int16_t* maxY,
                                     const uint8_t* valid,
                                     uint16_t minMaxColor,
                                     uint16_t curveColor)
{
    bool previousValid = false;
    int16_t previousX = 0;
    int16_t previousY = 0;

    for (uint16_t x = 0; x < MAIN_CHART_PLOT_W; x++)
    {
        if (!valid[x])
        {
            previousValid = false;
            continue;
        }

        int16_t screenX = MAIN_CHART_PLOT_X + (int16_t)x;
        int16_t y1 = minY[x];
        int16_t y2 = maxY[x];
        if (y1 > y2)
        {
            int16_t tmp = y1;
            y1 = y2;
            y2 = tmp;
        }

        tft.drawFastVLine(screenX, y1, (y2 - y1) + 1, minMaxColor);

        if (previousValid)
        {
            tft.drawLine(previousX, previousY, screenX, avgY[x], curveColor);
        }
        else
        {
            tft.drawPixel(screenX, avgY[x], curveColor);
        }

        previousValid = true;
        previousX = screenX;
        previousY = avgY[x];
    }
}

static void mainChartEraseOldCurve()
{
    if (!mainChartCurveWasDrawn) return;

    mainChartDrawCurveArrays(mainChartOldAvgY,
                             mainChartOldMinY,
                             mainChartOldMaxY,
                             mainChartOldValid,
                             BLACK,
                             BLACK);
}

static void mainChartCommitNewCurve()
{
    memcpy(mainChartOldAvgY, mainChartNewAvgY, sizeof(mainChartOldAvgY));
    memcpy(mainChartOldMinY, mainChartNewMinY, sizeof(mainChartOldMinY));
    memcpy(mainChartOldMaxY, mainChartNewMaxY, sizeof(mainChartOldMaxY));
    memcpy(mainChartOldValid, mainChartNewValid, sizeof(mainChartOldValid));
    mainChartCurveWasDrawn = true;
}

static void mainChartDrawAxisLabels(uint8_t metric, float axisMin, float axisMax)
{
    tft.setFont(DroidSansMono_14);
    tft.setTextColor(GREY, BLACK);

    for (uint8_t i = 0; i <= 4; i++)
    {
        float value = axisMax - ((axisMax - axisMin) * ((float)i / 4.0f));
        char label[16];
        if (mainChartMetricIsRh(metric))
        {
            snprintf(label, sizeof(label), "%6.2f", (double)value);
        }
        else
        {
            // Temperaturachsen bewusst nur mit zwei Nachkommastellen anzeigen.
            snprintf(label, sizeof(label), "%6.2f", (double)value);
        }

        int16_t y = MAIN_CHART_PLOT_Y - 7 +
                    (int16_t)(((uint32_t)i * (MAIN_CHART_PLOT_H - 1UL)) / 4UL);
        tft.setCursor(28, y);
        tft.print(label);
    }

    const char* nowText = T(TXT_MAIN_CHART_NOW);
    const char* labels60[5] = {"-60", "-45", "-30", "-15", nowText};
    const char* labels4h[5] = {"-4", "-3", "-2", "-1", nowText};
    const char* labels8h[5] = {"-8", "-6", "-4", "-2", nowText};
    const char* labels24h[7] = {"-24", "-20", "-16", "-12", "-8", "-4", nowText};

    const char* const* timeLabels = labels60;
    uint8_t labelCount = 5;
    uint8_t denominator = 4;

    if (mainChartActiveRange == 1)
    {
        timeLabels = labels4h;
    }
    else if (mainChartActiveRange == 2)
    {
        timeLabels = labels8h;
    }
    else if (mainChartActiveRange == 3)
    {
        timeLabels = labels24h;
        labelCount = 7;
        denominator = 6;
    }

    for (uint8_t i = 0; i < labelCount; i++)
    {
        int16_t textW = (int16_t)strlen(timeLabels[i]) * 8;
        int16_t x = MAIN_CHART_PLOT_X +
                    (int16_t)(((uint32_t)i * (MAIN_CHART_PLOT_W - 1UL)) /
                              (uint32_t)denominator);
        int16_t labelX;

        if (i + 1U >= labelCount)
        {
            // "Jetzt" bleibt etwas links vom rechten Diagrammrand, wie im
            // bisherigen 60-min-Chart.
            labelX = x - textW - 12;
        }
        else
        {
            labelX = x - (textW / 2);
        }

        tft.setCursor(labelX, 276);
        tft.print(timeLabels[i]);
    }
}

static void mainChartPrepareValueFields(const char* headingLabel)
{
    // Alle drei Zahlenfelder besitzen inklusive Einheit exakt neun Zeichen:
    //   Feuchte:     "100.00%rH"  -> 6 + 3
    //   Taupunkt:    "-12.345°C"  -> 7 + 2
    //   T-Umgebung:  " 23.456°C"  -> 7 + 2
    // Dadurch bleiben Position und Einheit auch bei Vorzeichenwechsel stabil.
    mainChartCurrentValueX = 106 +
                             (int16_t)strlen(headingLabel) * MAIN_CHART_FONT16_ADV;
    mainChartMinValueX = 110 + 4 * MAIN_CHART_FONT16_ADV; // nach "Min "
    mainChartMaxValueX = 582 + 4 * MAIN_CHART_FONT16_ADV; // nach "Max "

    // Der echte TFT-Bereich wurde soeben geloescht. Ein leerer Sichtcache
    // erzwingt beim ersten Update das einmalige Zeichnen aller neun Zeichen.
    memset(mainChartCurrentValueCache, 0, sizeof(mainChartCurrentValueCache));
    memset(mainChartMinValueCache, 0, sizeof(mainChartMinValueCache));
    memset(mainChartMaxValueCache, 0, sizeof(mainChartMaxValueCache));
    mainChartValueFieldsReady = true;
}

static void mainChartDrawFixedTexts(uint8_t metric)
{
    const char* name;
    char ambientName[20];
    if (metric == MAIN_CHART_METRIC_RH)
    {
        name = T(TXT_MAIN_CHART_HUMIDITY);
    }
    else if (metric == MAIN_CHART_METRIC_T_AMBIENT)
    {
        mainChartAmbientName(ambientName, sizeof(ambientName));
        name = ambientName;
    }
    else
    {
        name = T(TXT_MAIN_CHART_DEW_POINT);
    }

    char headingLabel[24];
    snprintf(headingLabel, sizeof(headingLabel), "%s: ", name);

    tft.setFont(DroidSansMono_16);
    tft.setTextColor(WHITE, BLACK);

    // Feste Beschriftungen werden nur beim kompletten Chartaufbau gesetzt.
    tft.setCursor(106, 67);
    tft.print(headingLabel);

    // Zeitraum rechtsbuendig halten: rechte Kante bleibt wie bisher bei
    // "60 min". Drauftippen schaltet 60 min / 4 h / 8 h / 24 h weiter.
    const char* rangeLabel = mainChartRangeLabel(mainChartActiveRange);
    int16_t rangeX = 746 - (int16_t)strlen(rangeLabel) * MAIN_CHART_FONT16_ADV;
    tft.setCursor(rangeX, 67);
    tft.print(rangeLabel);

    tft.setCursor(110, 305);
    tft.print("Min ");

    tft.setCursor(582, 305);
    tft.print("Max ");

    mainChartPrepareValueFields(headingLabel);
}

static void mainChartDrawStatic(uint8_t metric, const struct MainChartScale& scale)
{
    // Nur das Innere des Statusrahmens loeschen. Titel, Uhr und der komplette
    // untere Bedien-/Statusbereich bleiben unangetastet.
    tft.fillRect(MAIN_CHART_FRAME_X + 3,
                 MAIN_CHART_FRAME_Y + 3,
                 MAIN_CHART_FRAME_W - 6,
                 MAIN_CHART_FRAME_H - 6,
                 BLACK);

    mainChartDrawGrid();
    mainChartDrawAxisLabels(metric, scale.axisMin, scale.axisMax);
    mainChartDrawFixedTexts(metric);


    mainChartCurveWasDrawn = false;
    memset(mainChartOldValid, 0, sizeof(mainChartOldValid));
}

static void mainChartWriteValueField(char* visibleCache,
                                     int16_t x,
                                     int16_t y,
                                     const char* text)
{
    if (visibleCache == nullptr || text == nullptr) return;

    // Das Zielfeld wird immer auf exakt neun Zeichen normalisiert. Damit
    // bleiben Dezimalpunkt, Vorzeichen und Einheit positionsstabil.
    char fixed[MAIN_CHART_VALUE_CHARS + 1];
    uint8_t len = (uint8_t)strlen(text);
    for (uint8_t i = 0; i < MAIN_CHART_VALUE_CHARS; i++)
    {
        fixed[i] = (i < len) ? text[i] : ' ';
    }
    fixed[MAIN_CHART_VALUE_CHARS] = '\0';

    tft.setFont(DroidSansMono_16);

    for (uint8_t i = 0; i < MAIN_CHART_VALUE_CHARS; i++)
    {
        if (visibleCache[i] == fixed[i]) continue;

        int16_t charX = x + (int16_t)i * MAIN_CHART_FONT16_ADV;

        // Genau wie TextBox::transfer(): erst das alte Zeichen in
        // Hintergrundfarbe ueberschreiben, danach nur das neue Zeichen setzen.
        if (visibleCache[i] != '\0')
        {
            tft.setTextColor(BLACK);
            tft.setCursor(charX, y);
            tft.print(visibleCache[i]);
        }

        tft.setTextColor(WHITE);
        tft.setCursor(charX, y);
        tft.print(fixed[i]);
        visibleCache[i] = fixed[i];
    }

    visibleCache[MAIN_CHART_VALUE_CHARS] = '\0';
    TFTWAIT();
}

static void mainChartUpdateTexts(uint8_t metric,
                                 const struct MainChartScale& scale,
                                 bool force)
{
    uint32_t now = millis();
    if (!force && (uint32_t)(now - mainChartLastTextUpdateMs) < 250UL)
    {
        return;
    }
    mainChartLastTextUpdateMs = now;

    char text[40];
    const bool rhMetric = mainChartMetricIsRh(metric);
    float current = NAN;
    if (metric == MAIN_CHART_METRIC_RH) current = relativeFeuchte;
    else if (metric == MAIN_CHART_METRIC_T_AMBIENT) current = tempUmgebung;
    else current = präziserTaupunkt;

    // Oben sowie bei Min/Max werden nur die geaenderten Zeichen uebertragen.
    // Beschriftungen wie "Feuchte:", "Taupunkt:", "Min" und "Max" stehen
    // bereits statisch im Chart und werden hier nicht erneut geschrieben.
    if (mainChartValueFieldsReady)
    {
        if (rhMetric)
        {
            if (isfinite(current))
                snprintf(text, sizeof(text), "%6.2f\xB5""rH", (double)current);
            else
                snprintf(text, sizeof(text), " --.--\xB5""rH");
        }
        else
        {
            if (isfinite(current))
                snprintf(text, sizeof(text), "%7.3f\xB0""C", (double)current);
            else
                snprintf(text, sizeof(text), " --.---\xB0""C");
        }
        mainChartWriteValueField(mainChartCurrentValueCache, mainChartCurrentValueX, 67, text);

        if (scale.valid)
        {
            if (rhMetric)
            {
                snprintf(text, sizeof(text), "%6.2f\xB5""rH", (double)scale.dataMin);
                mainChartWriteValueField(mainChartMinValueCache, mainChartMinValueX, 305, text);
                snprintf(text, sizeof(text), "%6.2f\xB5""rH", (double)scale.dataMax);
                mainChartWriteValueField(mainChartMaxValueCache, mainChartMaxValueX, 305, text);
            }
            else
            {
                snprintf(text, sizeof(text), "%7.3f\xB0""C", (double)scale.dataMin);
                mainChartWriteValueField(mainChartMinValueCache, mainChartMinValueX, 305, text);
                snprintf(text, sizeof(text), "%7.3f\xB0""C", (double)scale.dataMax);
                mainChartWriteValueField(mainChartMaxValueCache, mainChartMaxValueX, 305, text);
            }
        }
        else
        {
            if (rhMetric)
            {
                mainChartWriteValueField(mainChartMinValueCache, mainChartMinValueX, 305, " --.--\xB5""rH");
                mainChartWriteValueField(mainChartMaxValueCache, mainChartMaxValueX, 305, " --.--\xB5""rH");
            }
            else
            {
                mainChartWriteValueField(mainChartMinValueCache, mainChartMinValueX, 305, " --.---\xB0""C");
                mainChartWriteValueField(mainChartMaxValueCache, mainChartMaxValueX, 305, " --.---\xB0""C");
            }
        }
    }

    // Unter dem Chart steht bei rH und T-Umgebung der Taupunkt,
    // beim Taupunkt-Chart die Feuchte. Der Start bei x=100 schafft
    // zusaetzlichen Platz vor dem unveraenderten T-Spiegel-/T-Umgebung-
    // Block ab x=440.
    if (metric == MAIN_CHART_METRIC_RH || metric == MAIN_CHART_METRIC_T_AMBIENT)
    {
        if (isfinite(präziserTaupunkt))
            snprintf(text, sizeof(text), "%s: %.3f\xB0""C",
                     T(TXT_MAIN_CHART_T_POINT),
                     (double)präziserTaupunkt);
        else
            snprintf(text, sizeof(text), "%s: --.---\xB0""C",
                     T(TXT_MAIN_CHART_T_POINT));
    }
    else
    {
        if (isfinite(relativeFeuchte))
            snprintf(text, sizeof(text), "%s: %.2f\xB5""rH",
                     T(TXT_MAIN_CHART_HUMIDITY),
                     (double)relativeFeuchte);
        else
            snprintf(text, sizeof(text), "%s: --.--\xB5""rH",
                     T(TXT_MAIN_CHART_HUMIDITY));
    }

    // Eigene TextBox in DMAMEM: transfer() vergleicht alle 20 Positionen
    // mit dem sichtbaren Puffer und schreibt nur geaenderte Zeichen neu.
    // Die feste Position bleibt x=100 / y=355; T-Spiegel beginnt bei x=440.
    if (force)
    {
        // Nach einem kompletten TFT-Neuaufbau muss der Sichtcache einmal
        // ungueltig werden, auch wenn der Textwert identisch geblieben ist.
        VirtLCDChartZusatzwert.invalidate();
    }
    VirtLCDChartZusatzwert.clear();
    VirtLCDChartZusatzwert.setCursor(0, 0);
    VirtLCDChartZusatzwert.print(text);
    VirtLCDChartZusatzwert.transfer();
    TFTWAIT();
}

static void mainChartUpdateCurve(uint8_t metric,
                                 const struct MainChartScale& scale,
                                 bool forceFull)
{
    const uint8_t range = mainChartActiveRange;
    uint32_t pixelEpoch = (uint32_t)(((uint64_t)mainChartHistoryTotal[range] *
                                      MAIN_CHART_PLOT_W) /
                                     MAIN_CHART_HISTORY_POINTS);

    bool scaleChanged = !mainChartScaleValid ||
                        fabsf(scale.axisMin - mainChartAxisMin) > 0.00001f ||
                        fabsf(scale.axisMax - mainChartAxisMax) > 0.00001f;
    bool viewChanged = (mainChartLastView != mainDashboardView);

    if (forceFull || scaleChanged || viewChanged)
    {
        mainChartDrawStatic(metric, scale);
        mainChartAxisMin = scale.axisMin;
        mainChartAxisMax = scale.axisMax;
        mainChartScaleValid = true;
    }
    else if (pixelEpoch == mainChartLastPixelEpoch &&
             mainChartHistoryCount[range] > 2)
    {
        // Die neue Probe veraendert noch keine sichtbare Pixelspalte.
        // Revision trotzdem als verarbeitet markieren, damit nicht 10-mal/s
        // dieselbe Historie erneut ausgewertet wird. Die ersten beiden Werte
        // werden sofort gezeigt, damit der Testchart nicht leer wirkt.
        mainChartLastHistoryRevision = mainChartHistoryRevision[range];
        return;
    }

    mainChartBuildColumns(metric, scale.axisMin, scale.axisMax);

    if (!forceFull && !scaleChanged && !viewChanged)
    {
        // Nur die alte Kurve entfernen. Raster und feste Beschriftungen
        // bleiben stehen; danach wird das Raster an den Kreuzungen repariert.
        mainChartEraseOldCurve();
        mainChartDrawGrid();
    }

    mainChartDrawCurveArrays(mainChartNewAvgY,
                             mainChartNewMinY,
                             mainChartNewMaxY,
                             mainChartNewValid,
                             GREY,
                             WHITE);
    mainChartCommitNewCurve();

    mainChartLastPixelEpoch = pixelEpoch;
    mainChartLastHistoryRevision = mainChartHistoryRevision[range];
    mainChartLastView = mainDashboardView;
}

// ============================================================================
// TFT WAIT
// ============================================================================

static inline void TFTWAIT()
{
    delayMicroseconds(80);
}


// ============================================================================
// DISPLAY LÖSCHEN
// ============================================================================

static void resetEthernetMiniStatusCache();
static void resetSystemMiniStatusCache();
static void resetLoopDebugMiniStatusCache();
static void resetFlowStatusFieldCache();
static void resetMeasurementStatusCache();
static void resetFanMiniStatusCache();
static void resetMainChartDisplayCache();

void eraseDisplay(void)
{
    VirtLCDUeberschrift.clear();
    VirtLCDTemperaturen.clear();

    if (VirtLCDMessage != nullptr)
    {
        VirtLCDMessage->clear();
    }

    VirtLCDGrossanzeige.clear();
    VirtLCDStatuszeile.clear();
    RTC_PM.clear();

    VirtLCDUeberschrift.transfer(); TFTWAIT();
    VirtLCDTemperaturen.transfer(); TFTWAIT();
    VirtLCDGrossanzeige.transfer(); TFTWAIT();
    VirtLCDStatuszeile.transfer(); TFTWAIT();
    RTC_PM.transfer(); TFTWAIT();

    if (VirtLCDMessage != nullptr)
    {
        VirtLCDMessage->transfer();
        TFTWAIT();
    }

    // WICHTIG:
    // clearScreen() NICHT verwenden
    tft.fillScreen(BLACK);
    resetEthernetMiniStatusCache();
    resetSystemMiniStatusCache();
    resetLoopDebugMiniStatusCache();
    resetFlowStatusFieldCache();
    resetMeasurementStatusCache();
    resetFanMiniStatusCache();
    resetMainChartDisplayCache();
    alarmResetDisplayCache();

    TFTWAIT();
}


// ============================================================================
// LOOP DEBUG-ANZEIGE LINKS OBEN
// ============================================================================
// Optional: zeigt Loop-Hz und maximale Loop-Zeit der letzten Sekunde.
// Aktivierung ueber Setup -> Anzeige -> Loop Hz/Max Anzeige.
// Ausgabe zweizeilig in DroidSansMono_12:
//   Loop: xxx Hz
//   Peak: x.x ms   (10-s-Fenster)

static char loopDebugLine1Cache[18] = "";
static char loopDebugLine2Cache[18] = "";
static bool loopDebugWasDrawn = false;
static bool loopDebugLastEnabled = false;
static bool loopDebugLastMenuUpd = false;

static void resetLoopDebugMiniStatusCache()
{
    loopDebugLine1Cache[0] = '\0';
    loopDebugLine2Cache[0] = '\0';
    loopDebugWasDrawn = false;
    loopDebugLastEnabled = false;
    loopDebugLastMenuUpd = false;
}

static void drawLoopDebugLine(const char* newText,
                              char* cache,
                              size_t cacheSize,
                              int16_t textX,
                              int16_t textY,
                              bool forceRedraw)
{
    if (newText == nullptr) newText = "";

    bool textChanged = (strncmp(cache, newText, cacheSize) != 0);

    if (forceRedraw)
    {
        cache[0] = '\0';
        textChanged = true;
    }

    if (!textChanged)
    {
        return;
    }

    tft.setFont(DroidSansMono_12);
    TFTWAIT();

    if (cache[0] != '\0')
    {
        tft.setTextColor(BLACK, BLACK);
        TFTWAIT();
        tft.setCursor(textX, textY);
        TFTWAIT();
        tft.print(cache);
        TFTWAIT();
    }

    strncpy(cache, newText, cacheSize - 1);
    cache[cacheSize - 1] = '\0';

    tft.setTextColor(WHITE, BLACK);
    TFTWAIT();
    tft.setCursor(textX, textY);
    TFTWAIT();
    tft.print(cache);
    TFTWAIT();
}

static void zeichneLoopDebugMiniStatus()
{
    uint16_t hz = 0;
    uint32_t maxUs = 0;
    uint32_t peak10Us = 0;
    bool enabled = false;

    loopDebugGetMetricsExt(&hz, &maxUs, &peak10Us, &enabled);

    const int16_t boxX = 4;
    const int16_t boxY = 2;
    const int16_t boxW = 140;
    const int16_t boxH = 34;
    const int16_t textX = 6;
    const int16_t textY1 = 2;
    const int16_t textY2 = 21;

    bool menuUpdEdge = (flag.menu_lcd_upd && !loopDebugLastMenuUpd);
    loopDebugLastMenuUpd = flag.menu_lcd_upd;

    if (!enabled)
    {
        if (loopDebugWasDrawn || loopDebugLastEnabled || menuUpdEdge)
        {
            tft.fillRect(boxX, boxY, boxW, boxH, BLACK);
            TFTWAIT();
        }

        loopDebugLine1Cache[0] = '\0';
        loopDebugLine2Cache[0] = '\0';
        loopDebugWasDrawn = false;
        loopDebugLastEnabled = false;
        return;
    }

    loopDebugLastEnabled = true;

    char line1[18];
    char line2[18];
    uint32_t peakMs10 = (peak10Us + 50UL) / 100UL;  // 0.1 ms Aufloesung

    snprintf(line1, sizeof(line1), "Loop:%4u Hz", (unsigned)hz);
    snprintf(line2, sizeof(line2), "Peak:%3lu.%lu ms",
             (unsigned long)(peakMs10 / 10UL),
             (unsigned long)(peakMs10 % 10UL));

    bool forceRedraw = (!loopDebugWasDrawn || menuUpdEdge);

    if (forceRedraw)
    {
        tft.fillRect(boxX, boxY, boxW, boxH, BLACK);
        TFTWAIT();
    }

    drawLoopDebugLine(line1, loopDebugLine1Cache, sizeof(loopDebugLine1Cache), textX, textY1, forceRedraw);
    drawLoopDebugLine(line2, loopDebugLine2Cache, sizeof(loopDebugLine2Cache), textX, textY2, forceRedraw);

    loopDebugWasDrawn = true;
}






static void drawMiniStatusItem(const char* text,
                               char* textCache,
                               size_t textCacheSize,
                               uint16_t color,
                               uint16_t* colorCache,
                               int16_t boxX,
                               int16_t boxY,
                               int16_t boxW,
                               int16_t boxH,
                               int16_t squareX,
                               int16_t squareY,
                               int16_t squareSize,
                               int16_t textX,
                               int16_t textY,
                               bool forceRedraw);

// ============================================================================
// MESSWERT-STATUS IM HAUPTDISPLAY
// ============================================================================
// Ein gemeinsamer 3-px-Rahmen um rF% und Taupunkt. Die Messwerte bleiben weiss;
// der Rahmen und der Statuspunkt unten zeigen den aktuellen Zustand:
//   Rot    = SAFETY-STOP oder GRENZWERT-Alarm
//   Orange = Freiheizen
//   Cyan   = LED-/Optik-Autoeichung
//   Gelb   = Messwert stabilisiert sich noch
//   Gruen  = Messwert stabil
//   Grau   = Initialisierung / Startzustand
//   Rot    = echter Messwertfehler (ohne Relais/Buzzer)

static uint16_t messwertFrameColorCache = 0xFFFF;
static uint16_t messwertMiniColorCache = 0xFFFF;
static char messwertStatusTextCache[32] = "";
static bool messwertStatusWasDrawn = false;
static bool messwertStatusLastMenuUpd = false;

static const uint32_t MESSWERT_INIT_MIN_MS = 10000UL;
static const uint32_t MESSWERT_STABILISIERZEIT_MS = 30000UL;

static void resetMeasurementStatusCache()
{
    messwertFrameColorCache = 0xFFFF;
    messwertMiniColorCache = 0xFFFF;
    messwertStatusTextCache[0] = '\0';
    messwertStatusWasDrawn = false;
    messwertStatusLastMenuUpd = false;
}

static bool messwertStatusValuesFinite()
{
    return isfinite(relativeFeuchte) && isfinite(präziserTaupunkt) &&
           isfinite(tempSpiegel) && isfinite(tempUmgebung);
}

static bool messwertStatusHardError()
{
    // INIT ist der normale Startzustand, solange die ADS1263-Messkette
    // noch nicht bereit ist. FEHLER zeigen wir erst bei echtem Fehler oder
    // bei ungueltigen Messwerten nach bereits bereiter Messkette.
    if (mcp3202_fehler)
    {
        return true;
    }

    if (ads1263_bereit && !messwertStatusValuesFinite())
    {
        return true;
    }

    return false;
}

static bool messwertStatusRefError()
{
    return ads1263_bereit && ads1263RefIsError();
}

static bool messwertStatusRefOld()
{
    return ads1263_bereit && ads1263RefIsOld();
}

static bool messwertStatusValuesValid()
{
    return messwertStatusValuesFinite() && ads1263_bereit && !mcp3202_fehler;
}

static bool messwertStatusInInitMinTime()
{
    return (uint32_t)millis() < MESSWERT_INIT_MIN_MS;
}

static uint16_t messwertStatusColor()
{
    if (safetyIsFaultActive())
    {
        return RED;
    }

    if (alarmVisualActive())
    {
        return RED;
    }

    // Nach dem Einschalten INIT mindestens kurz stehen lassen,
    // damit kurze Sensor-/Bus-Anlaufphasen nicht sofort als FEHLER erscheinen.
    // Safety und bereits aktive Alarme behalten trotzdem Vorrang.
    if (messwertStatusInInitMinTime())
    {
        return GREY;
    }

    if (messwertStatusHardError())
    {
        return RED;
    }

    if (messwertStatusRefError())
    {
        return RED;
    }

    if (!messwertStatusValuesValid())
    {
        return GREY;
    }

    if (ablaufStatus == 1)
    {
        return ORANGE;
    }

    if (ablaufStatus == 2)
    {
        return CYAN;
    }

    if ((uint32_t)(millis() - statusTimer) < MESSWERT_STABILISIERZEIT_MS)
    {
        return YELLOW;
    }

    if (opticHealthMainWarningActive())
    {
        // Optik-Hinweise sind bewusst nur Info/Warnung, kein harter Fehler.
        return ORANGE;
    }

    return GREEN;
}

static const char* messwertStatusText()
{
    if (safetyIsFaultActive())
    {
        return safetyGetStopStatusText();
    }

    if (alarmVisualActive())
    {
        return T(TXT_STATUS_LIMIT);
    }

    // Nach dem Einschalten INIT mindestens kurz stehen lassen,
    // damit kurze Sensor-/Bus-Anlaufphasen nicht sofort als FEHLER erscheinen.
    // Safety und bereits aktive Alarme behalten trotzdem Vorrang.
    if (messwertStatusInInitMinTime())
    {
        return T(TXT_STATUS_INIT);
    }

    if (messwertStatusHardError())
    {
        return T(TXT_STATUS_ERROR);
    }

    if (messwertStatusRefError())
    {
        return T(TXT_STATUS_REF_ERROR);
    }

    if (!messwertStatusValuesValid())
    {
        return T(TXT_STATUS_INIT);
    }

    if (ablaufStatus == 1)
    {
        return T(TXT_STATUS_PREHEAT);
    }

    if (ablaufStatus == 2)
    {
        return T(TXT_STATUS_AUTOCAL);
    }

    if ((uint32_t)(millis() - statusTimer) < MESSWERT_STABILISIERZEIT_MS)
    {
        return T(TXT_STATUS_SETTLING);
    }

    if (messwertStatusRefOld())
    {
        return T(TXT_STATUS_REF_OLD);
    }

    if (opticHealthMainWarningActive())
    {
        return (ui_language == LANG_EN) ? opticHealthMainStatusTextEN() : opticHealthMainStatusTextDE();
    }

    return T(TXT_STATUS_STABLE);
}

static void drawMeasurementStatusFrame(uint16_t color, bool forceRedraw)
{
    // In der Standard-Zahlenansicht bleibt der bekannte Rahmen erhalten.
    // Fuer Verlaufansichten und den alternativen 3-Werte-Hauptscreen wird
    // der gleiche grosse Rahmen wie beim Chart verwendet.
    const bool wideFrame = (mainDashboardView != MAIN_DASH_VALUES) || mainDashboardAltValuesLayout();
    const int16_t x = wideFrame ? MAIN_CHART_FRAME_X : 115;
    const int16_t y = wideFrame ? MAIN_CHART_FRAME_Y : 88;
    const int16_t w = wideFrame ? MAIN_CHART_FRAME_W : 570;
    const int16_t h = wideFrame ? MAIN_CHART_FRAME_H : 210;
    const int16_t r = wideFrame ? MAIN_CHART_FRAME_R : 8;

    if (!forceRedraw && messwertFrameColorCache == color)
    {
        return;
    }

    // Alten 3-px-Rahmen sauber schwarz loeschen.
    for (uint8_t i = 0; i < 3; i++)
    {
        tft.drawRoundRect(x + i, y + i, w - (2 * i), h - (2 * i), r, BLACK);
        TFTWAIT();
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        tft.drawRoundRect(x + i, y + i, w - (2 * i), h - (2 * i), r, color);
        TFTWAIT();
    }

    messwertFrameColorCache = color;
}

static void zeichneMesswertStatus()
{
    uint16_t color = messwertStatusColor();
    const char* text = messwertStatusText();

    bool menuUpdEdge = (flag.menu_lcd_upd && !messwertStatusLastMenuUpd);
    messwertStatusLastMenuUpd = flag.menu_lcd_upd;

    bool forceRedraw = (!messwertStatusWasDrawn || menuUpdEdge);
    messwertStatusWasDrawn = true;

    drawMeasurementStatusFrame(color, forceRedraw);

    // Statuszeile unten: direkt vor Safety, gleicher Aufbau wie die anderen Mini-Statusfelder.
    drawMiniStatusItem(text,
                       messwertStatusTextCache,
                       sizeof(messwertStatusTextCache),
                       color,
                       &messwertMiniColorCache,
                       258, 459, 112, 17,
                       258, 466, 8,
                       270, 464,
                       forceRedraw);
}

// ============================================================================
// FAN-MINI-STATUS IM HAUPTDISPLAY
// ============================================================================
// Punktfarbe:
//   gruen = FAN ON und Tacho vorhanden
//   gelb  = FAN ON, aber nach 5 s Anlaufzeit kein Tacho
//   weiss = FAN OFF (bewusst ausgeschaltet)

static char fanMiniTextCache[12] = "";
static uint16_t fanMiniColorCache = 0xFFFF;
static bool fanMiniWasDrawn = false;
static bool fanMiniLastMenuUpd = false;

static void resetFanMiniStatusCache()
{
    fanMiniTextCache[0] = '\0';
    fanMiniColorCache = 0xFFFF;
    fanMiniWasDrawn = false;
    fanMiniLastMenuUpd = false;
}

static uint16_t fanMiniStatusColor()
{
    if (!sensorFanIsEnabled())
    {
        return WHITE;
    }

    return sensorFanTachoMissing() ? YELLOW : GREEN;
}

static void zeichneFanMiniStatus()
{
    uint16_t fanColor = fanMiniStatusColor();

    bool menuUpdEdge = (flag.menu_lcd_upd && !fanMiniLastMenuUpd);
    fanMiniLastMenuUpd = flag.menu_lcd_upd;

    bool forceRedraw = (!fanMiniWasDrawn || menuUpdEdge);
    fanMiniWasDrawn = true;

    // Links vor dem Messwertstatus, ausserhalb des SETUP-Buttons.
    drawMiniStatusItem(sensorFanStatusText(),
                       fanMiniTextCache,
                       sizeof(fanMiniTextCache),
                       fanColor,
                       &fanMiniColorCache,
                       170, 459, 74, 17,
                       170, 466, 8,
                       182, 464,
                       forceRedraw);
}


// ============================================================================
// SYSTEM-MINI-STATUS IM HAUPTDISPLAY
// ============================================================================
// Statuszeile links vor der IP-Anzeige:
//   Quadrat + "Safety"     gruen = OK, rot = Safety-Fehler
//   Quadrat + "SD-Logging" weiss = Logging aus, gruen = Logging aktiv, gelb = Schreibzugriff, rot = Fehler/keine Karte
// Texte laufen ueber TPlanguage.h, damit sie spaeter leicht aenderbar sind.

static char sysSafetyTextCache[18] = "";
static char sysSdTextCache[22] = "";
static uint16_t sysSafetyColorCache = 0xFFFF;
static uint16_t sysSdColorCache = 0xFFFF;
static bool sysMiniWasDrawn = false;
static bool sysMiniLastMenuUpd = false;

static const uint8_t DISPLAY_SD_ACTIVITY_NONE   = 0;
static const uint8_t DISPLAY_SD_ACTIVITY_CSV    = 1;
static const uint8_t DISPLAY_SD_ACTIVITY_SERIAL = 2;
static const uint8_t DISPLAY_SD_ACTIVITY_BOTH   = 3;
static const uint8_t DISPLAY_SD_ACTIVITY_ERROR  = 4;

static void resetSystemMiniStatusCache()
{
    sysSafetyTextCache[0] = '\0';
    sysSdTextCache[0] = '\0';
    sysSafetyColorCache = 0xFFFF;
    sysSdColorCache = 0xFFFF;
    sysMiniWasDrawn = false;
    sysMiniLastMenuUpd = false;
}

static uint16_t systemMiniSdColor()
{
    // Grundfarbe zeigt, welche Dateien wirklich offen sind:
    // weiss = nichts, gruen = CSV, blau = Seriellog,
    // lila = CSV + Seriellog, rot = angeforderte Aufzeichnung fehlgeschlagen.
    // Ein echter Flush wird fuer 250 ms gelb hervorgehoben.
    if (sdStorageWriteBlinkActive())
    {
        return YELLOW;
    }

    switch (sdStorageActivityState())
    {
        case DISPLAY_SD_ACTIVITY_CSV:    return GREEN;
        case DISPLAY_SD_ACTIVITY_SERIAL: return BLUE;
        case DISPLAY_SD_ACTIVITY_BOTH:   return MAGENTA;
        case DISPLAY_SD_ACTIVITY_ERROR:  return RED;
        case DISPLAY_SD_ACTIVITY_NONE:
        default:                         return WHITE;
    }
}

static void drawMiniStatusItem(const char* text,
                               char* textCache,
                               size_t textCacheSize,
                               uint16_t color,
                               uint16_t* colorCache,
                               int16_t boxX,
                               int16_t boxY,
                               int16_t boxW,
                               int16_t boxH,
                               int16_t squareX,
                               int16_t squareY,
                               int16_t squareSize,
                               int16_t textX,
                               int16_t textY,
                               bool forceRedraw)
{
    if (text == nullptr) text = "";

    bool textChanged = (strncmp(textCache, text, textCacheSize) != 0);
    bool colorChanged = (*colorCache != color);

    if (forceRedraw)
    {
        tft.fillRect(boxX, boxY, boxW, boxH, BLACK);
        TFTWAIT();
        textCache[0] = '\0';
        *colorCache = 0xFFFF;
        textChanged = true;
        colorChanged = true;
    }

    if (colorChanged)
    {
        tft.fillRect(squareX - 1, squareY - 1, squareSize + 2, squareSize + 2, BLACK);
        TFTWAIT();
        tft.fillRect(squareX, squareY, squareSize, squareSize, color);
        TFTWAIT();
        *colorCache = color;
    }

    if (textChanged)
    {
        tft.setFont(DroidSansMono_12);
        TFTWAIT();

        if (textCache[0] != '\0')
        {
            tft.setTextColor(BLACK, BLACK);
            TFTWAIT();
            tft.setCursor(textX, textY);
            TFTWAIT();
            tpTftPrintUtf8(textCache);
            TFTWAIT();
        }

        strncpy(textCache, text, textCacheSize - 1);
        textCache[textCacheSize - 1] = '\0';

        tft.setTextColor(WHITE, BLACK);
        TFTWAIT();
        tft.setCursor(textX, textY);
        TFTWAIT();
        tpTftPrintUtf8(textCache);
        TFTWAIT();
    }
}

static void zeichneSystemMiniStatus()
{
    uint16_t safetyColor = safetyIsFaultActive() ? RED : GREEN;
    uint16_t sdColor = systemMiniSdColor();

    bool menuUpdEdge = (flag.menu_lcd_upd && !sysMiniLastMenuUpd);
    sysMiniLastMenuUpd = flag.menu_lcd_upd;

    bool forceRedraw = (!sysMiniWasDrawn || menuUpdEdge);
    sysMiniWasDrawn = true;

    // Gleiche Unterkante wie die IP-Anzeige, nur links davon.
    // Bei Bedarf kannst du diese X-Werte optisch noch fein verschieben.
    drawMiniStatusItem(T(TXT_MAIN_STATUS_SAFETY),
                       sysSafetyTextCache,
                       sizeof(sysSafetyTextCache),
                       safetyColor,
                       &sysSafetyColorCache,
                       390, 459, 82, 17,
                       390, 466, 8,
                       402, 464,
                       forceRedraw);

    drawMiniStatusItem(T(TXT_MAIN_STATUS_SD_LOGGING),
                       sysSdTextCache,
                       sizeof(sysSdTextCache),
                       sdColor,
                       &sysSdColorCache,
                       485, 459, 120, 17,
                       485, 466, 8,
                       497, 464,
                       forceRedraw);
}

// ============================================================================
// ETHERNET-MINI-STATUS IM HAUPTDISPLAY
// ============================================================================
// Kleines Quadrat rechts unten unter dem Stromwert.
// Quadrat gruen: gueltige IP vorhanden
// Quadrat rot:   Ethernet aus / keine IP / DHCP ohne Adresse
// IP-Text bleibt immer weiss.

static char ethMiniTextCache[20] = "";
static uint16_t ethMiniColorCache = 0xFFFF;
static bool ethMiniWasDrawn = false;
static bool ethMiniLastMenuUpd = false;

static void resetEthernetMiniStatusCache()
{
    ethMiniTextCache[0] = '\0';
    ethMiniColorCache = 0xFFFF;
    ethMiniWasDrawn = false;
    ethMiniLastMenuUpd = false;
}

static void makeFixedIpText(char* out, size_t outSize, const uint8_t ip[4], bool valid)
{
    if (out == nullptr || outSize == 0) return;

    if (!valid)
    {
        snprintf(out, outSize, "---.---.---.---");
        return;
    }

    char tmp[20];
    snprintf(tmp, sizeof(tmp), "%03u.%03u.%03u.%03u",
             (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);

    // Fest auf 15 Zeichen auffuellen, damit kuerzere IPs alte Zeichen sicher loeschen.
    snprintf(out, outSize, "%-15s", tmp);
}

static void zeichneEthernetMiniStatus()
{
    uint8_t ip[4];
    bool ipValid = false;
    bool ethEnabled = false;

    interfaceGetDisplayIp(ip, &ipValid, &ethEnabled);

    char ipText[20];
    makeFixedIpText(ipText, sizeof(ipText), ip, ipValid);

    uint16_t symbolColor = (ethEnabled && ipValid) ? GREEN : RED;

    // Position rechts unten unter dem Stromwert.
    const int16_t boxX = 620;
    const int16_t boxY = 459;
    const int16_t boxW = 150;
    const int16_t boxH = 17;

    const int16_t squareX = 620;
    const int16_t squareY = 466;
    const int16_t squareSize = 8;

    const int16_t textX = 632;
    const int16_t textY = 464;

    bool menuUpdEdge = (flag.menu_lcd_upd && !ethMiniLastMenuUpd);
    ethMiniLastMenuUpd = flag.menu_lcd_upd;

    bool textChanged = (strncmp(ethMiniTextCache, ipText, sizeof(ethMiniTextCache)) != 0);
    bool colorChanged = (ethMiniColorCache != symbolColor);

    if (!ethMiniWasDrawn || menuUpdEdge)
    {
        tft.fillRect(boxX, boxY, boxW, boxH, BLACK);
        ethMiniTextCache[0] = '\0';
        ethMiniColorCache = 0xFFFF;
        ethMiniWasDrawn = true;
        textChanged = true;
        colorChanged = true;
        TFTWAIT();
    }

    if (colorChanged)
    {
        tft.fillRect(squareX - 1, squareY - 1, squareSize + 2, squareSize + 2, BLACK);
        TFTWAIT();
        tft.fillRect(squareX, squareY, squareSize, squareSize, symbolColor);
        TFTWAIT();
        ethMiniColorCache = symbolColor;
    }

    if (textChanged)
    {
        tft.setFont(DroidSansMono_12);
        TFTWAIT();

        if (ethMiniTextCache[0] != '\0')
        {
            tft.setTextColor(BLACK, BLACK);
            TFTWAIT();
            tft.setCursor(textX, textY);
            TFTWAIT();
            tft.print(ethMiniTextCache);
            TFTWAIT();
        }

        strncpy(ethMiniTextCache, ipText, sizeof(ethMiniTextCache) - 1);
        ethMiniTextCache[sizeof(ethMiniTextCache) - 1] = '\0';

        tft.setTextColor(WHITE, BLACK);
        TFTWAIT();
        tft.setCursor(textX, textY);
        TFTWAIT();
        tft.print(ethMiniTextCache);
        TFTWAIT();
    }
}


// ============================================================================
// ZUSATZANZEIGE LINKS VOR DER STATUSZEILE
// ============================================================================
// Je nach Setup entweder Durchfluss oder die externe ALMEMO-Temperatur.
// Wie bei TextBox werden ausschliesslich geaenderte Zeichen schwarz geloescht
// und neu geschrieben. Das vermeidet das bisherige Vollflaechen-fillRect()
// und damit sichtbares Flackern bei jedem T-Ext-/F-Update, ohne fuer das kleine
// Feld eine weitere 3,6-kB-TextBox anzulegen.

static const uint8_t FLOW_STATUS_CHARS = 18;
// Monospaced-Zeichenweite von DroidSansMono_20 laut Font-Deltawert.
static const int16_t FLOW_STATUS_FONT_ADV = 17;
static DMAMEM char flowStatusVisibleCache[FLOW_STATUS_CHARS + 1];
// DMAMEM/RAM2 darf beim Kaltstart nicht als bereits geloeschter Sichtcache
// vorausgesetzt werden. Dieses normale BSS-Flag startet garantiert mit false
// und erzwingt vor der ersten Ausgabe eine definierte Initialisierung.
static bool flowStatusVisibleCacheReady = false;

static void resetFlowStatusFieldCache()
{
    memset(flowStatusVisibleCache, 0, sizeof(flowStatusVisibleCache));
    flowStatusVisibleCacheReady = true;
}

static void flowStatusWriteText(const char* text)
{
    if (text == nullptr) text = "";

    // Beim Einschalten kann RAM2 noch alte Cachezeichen enthalten. Dann wuerden
    // feste Zeichen wie "T-Ext:" und "°C" faelschlich als bereits sichtbar
    // gelten, waehrend nur die geaenderten Ziffern gezeichnet werden.
    if (!flowStatusVisibleCacheReady)
    {
        memset(flowStatusVisibleCache, 0, sizeof(flowStatusVisibleCache));
        flowStatusVisibleCacheReady = true;
    }

    char fixed[FLOW_STATUS_CHARS + 1];
    const size_t len = strlen(text);
    for (uint8_t i = 0; i < FLOW_STATUS_CHARS; i++)
    {
        fixed[i] = (i < len) ? text[i] : ' ';
    }
    fixed[FLOW_STATUS_CHARS] = '\0';

    tft.setFont(DroidSansMono_20);
    TFTWAIT();

    for (uint8_t i = 0; i < FLOW_STATUS_CHARS; i++)
    {
        if (flowStatusVisibleCache[i] == fixed[i]) continue;

        const int16_t charX = 180 + (int16_t)i * FLOW_STATUS_FONT_ADV;

        // Genau wie TextBox::transfer(): altes Zeichen schwarz ueberschreiben,
        // danach nur das neue Zeichen in Gelb setzen.
        if (flowStatusVisibleCache[i] != '\0')
        {
            tft.setTextColor(BLACK);
            tft.setCursor(charX, 391);
            tft.print(flowStatusVisibleCache[i]);
        }

        tft.setTextColor(YELLOW);
        tft.setCursor(charX, 391);
        tft.print(fixed[i]);
        flowStatusVisibleCache[i] = fixed[i];
    }

    flowStatusVisibleCache[FLOW_STATUS_CHARS] = '\0';
    TFTWAIT();
}

static void zeichneFlowStatusFeld()
{
    const bool showFlow = interfaceFlowDisplayEnabled();
    char text[28] = "";

    if (interfaceAlmemoAnyChannelEnabled())
    {
        const uint8_t idx = interfaceAlmemoDisplayIndex();
        const char* label = interfaceAlmemoDisplayLabel(idx);
        const char* unit = interfaceAlmemoDisplayUnitShort(idx);
        if (label == nullptr) label = "T-Ext:";
        if (unit == nullptr) unit = "";

        const float v = serialAlmemoLastValue(idx);
        if (isfinite(v))
        {
            // Ein einziges Leerzeichen nach dem Doppelpunkt; Einheit direkt
            // am Wert, damit das Feld beim 5-s-Wechsel nicht springt.
            snprintf(text, sizeof(text), "%s %5.2f%s", label, (double)v, unit);
        }
        else
        {
            snprintf(text, sizeof(text), "%s --.--%s", label, unit);
        }
    }
    else if (showFlow)
    {
        const char* unit = interfaceFlowDisplayUnitText();
        if (unit == nullptr || unit[0] == '\0') unit = "";

        const float flowValue = serialFlowLastValueLMin();
        if (isfinite(flowValue))
        {
            snprintf(text, sizeof(text), "F: %5.2f %s", (double)flowValue, unit);
        }
        else
        {
            snprintf(text, sizeof(text), "F: --.-- %s", unit);
        }
    }

    flowStatusWriteText(text);
}

// ============================================================================
// PELTIER-RICHTUNGSANZEIGE IN DER STATUSZEILE
// ============================================================================
// Kleiner gefuellter Punkt direkt nach "I:".
// Blau  = Kuehlen
// Rot   = Heizen
// Grau  = Aus / praktisch kein Strom

// Kleiner Software-Kreis fuer den Peltier-Richtungspunkt.
// Wichtig: NICHT tft.fillCircle() benutzen. Beim RA8875 bleiben bei
// Hardware-Kreisen manchmal 1-Pixel-Reste stehen, die dann im Setup-Menue
// sichtbar werden. Die horizontalen fillRect()-Zeilen sind robuster.
static void displayFillDotSoft(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    for (int16_t dy = -r; dy <= r; dy++)
    {
        int16_t dx = 0;
        while ((int32_t)(dx + 1) * (dx + 1) + (int32_t)dy * dy <= (int32_t)r * r)
        {
            dx++;
        }
        tft.fillRect(cx - dx, cy + dy, (dx * 2) + 1, 1, color);
    }
}

static void zeichnePeltierRichtungsPunkt()
{
    uint16_t farbe = GREY;

    if (amp_avg >= 0.020)
    {
        if (aktuellerModus == 1)
        {
            farbe = BLUE;
        }
        else if (aktuellerModus == 2)
        {
            farbe = RED;
        }
    }

    // Position in der Statuszeile: direkt nach "I:" vor dem Stromwert.
    // Durchmesser ca. 12 Pixel -> Radius 6.
    const int16_t x = 642;
    const int16_t y = 437;
    const int16_t r = 6;
    const int16_t clr = r + 3;

    // Bereich zuerst rechteckig loeschen, damit beim Farbwechsel und beim
    // spaeteren Setup-Wechsel keine alten Randpixel uebrig bleiben.
    tft.drawPixel(0, 0, BLACK);
    TFTWAIT();
    tft.fillRect(x - clr, y - clr, (clr * 2) + 1, (clr * 2) + 1, BLACK);
    TFTWAIT();

    displayFillDotSoft(x, y, r, farbe);
    TFTWAIT();
}


// ============================================================================
// SETUP-BUTTON: GELB, SOLANGE DAS WEB-SETUP AKTIV IST
// ============================================================================
// Die Farbe dient nur als Belegt-Anzeige. Der Touch-Einstieg ins lokale Setup
// wird in TP-3000.ino bereits blockiert, solange die Web-Setup-Session aktiv ist.
static int8_t mainSetupButtonLastWebBusy = -1;

static uint16_t mainSetupButtonColor()
{
    return ethernetWebSetupSessionActive() ? YELLOW : WHITE;
}

static void mainSetupButtonRememberColorState()
{
    mainSetupButtonLastWebBusy = ethernetWebSetupSessionActive() ? 1 : 0;
}

// ============================================================================
// FAN-BUTTON-GEISTER IM HAUPTSCREEN LOESCHEN
// ============================================================================

static void clearFanButtonGhostOnMainScreen()
{
    // In FANtest6 absichtlich kein Hauptscreen-Nachloeschen mehr.
    // Das grobe Nachloeschen beseitigte zwar RA8875-Kreisreste, konnte
    // aber im Hauptscreen schwarze Loecher erzeugen. Die Ursache wird jetzt
    // im Button beseitigt: kein tft.fillCircle() mehr fuer die FAN-LED.
    if (fanButtonMainGhostClearActive())
    {
        fanButtonMainGhostClearDoneOne();
    }
}



// ============================================================================
// CHART-SCREEN (rH, TAUPUNKT ODER T-UMGEBUNG, 60 MIN / 4 H / 8 H / 24 H)
// ============================================================================

static void lcd_display_chart_screen()
{
    const uint8_t metric = mainChartMetricForView();
    bool force = !mainChartScreenWasDrawn ||
                 (mainChartLastView != mainDashboardView);

    // Titel und Uhr bleiben wie in der bisherigen Hauptanzeige.
    VirtLCDUeberschrift.setCursor(0, 0);
    VirtLCDUeberschrift.print(T(TXT_MAIN_TITLE));

    // Die grosse 60-px-Zahlenbox wird in der Chartansicht bewusst geleert.
    VirtLCDGrossanzeige.clear();

    VirtLCDTemperaturen.setCursor(0, 0);
    displayFormatValueOrDashes(strBuf, sizeof(strBuf), tempSpiegel, 7, 3);
    VirtLCDTemperaturen.print(T(TXT_MAIN_T_MIRROR));
    VirtLCDTemperaturen.print(strBuf);
    VirtLCDTemperaturen.print("\xB0""C");

    VirtLCDTemperaturen.setCursor(0, 1);
    displayFormatValueOrDashes(strBuf, sizeof(strBuf), tempUmgebung, 7, 3);
    VirtLCDTemperaturen.print(T(TXT_MAIN_T_AMBIENT));
    VirtLCDTemperaturen.print(strBuf);
    VirtLCDTemperaturen.print("\xB0""C");

    VirtLCDStatuszeile.setCursor(0, 0);
    VirtLCDStatuszeile.print("       P: ");

    char pBuf[16];
    char iBuf[16];
    displayFormatValueOrDashes(pBuf, sizeof(pBuf), baroDruckHPa, 7, 2);
    VirtLCDStatuszeile.print(pBuf);
    VirtLCDStatuszeile.print(" hPa | I: ");
    displayFormatValueOrDashes(iBuf, sizeof(iBuf), amp_avg, 6, 3);
    VirtLCDStatuszeile.print(iBuf);
    VirtLCDStatuszeile.print(" A");

    char zeitBuf[16];
    sprintf(zeitBuf, "%02d:%02d:%02d", hour(), minute(), second());
    RTC_PM.setCursor(0, 0);
    RTC_PM.print(zeitBuf);

    VirtLCDUeberschrift.transfer(); TFTWAIT();
    VirtLCDGrossanzeige.transfer(); TFTWAIT();
    VirtLCDTemperaturen.transfer(); TFTWAIT();
    VirtLCDStatuszeile.transfer(); TFTWAIT();
    RTC_PM.transfer(); TFTWAIT();

    zeichneFlowStatusFeld();
    zeichneLoopDebugMiniStatus();
    zeichnePeltierRichtungsPunkt();
    zeichneFanMiniStatus();
    zeichneSystemMiniStatus();
    zeichneEthernetMiniStatus();

    MainChartScale scale = mainChartCalculateScale(mainChartActiveRange, metric);

    // Skalierung wird bei jedem neuen Punkt des aktiven Zeitfensters geprueft,
    // damit ein neuer Ausreisser sofort sichtbar wird. Die Kurve selbst wird
    // erst aktualisiert, wenn sich bei 640 Pixeln eine neue sichtbare Spalte ergibt.
    bool historyChanged = (mainChartLastHistoryRevision != mainChartHistoryRevision[mainChartActiveRange]);
    if (force || historyChanged)
    {
        mainChartUpdateCurve(metric, scale, force);
    }

    mainChartUpdateTexts(metric, scale, force);

    // Rahmen zuletzt zeichnen, damit die 3 Pixel Statusfarbe immer sauber
    // ueber allen Chart-Elementen liegen.
    zeichneMesswertStatus();

    const uint16_t setupButtonColor = mainSetupButtonColor();
    tft.drawRoundRect(10, 395, 140, 75, 8, setupButtonColor);
    tft.setFont(DroidSansMono_20);
    tft.setTextColor(setupButtonColor, BLACK);
    tft.setCursor(39, 422);
    tft.print(T(TXT_MAIN_SETUP));
    mainSetupButtonRememberColorState();

    mainChartScreenWasDrawn = true;
}


// ============================================================================
// KLIMA SCREEN
// ============================================================================

void lcd_display_klima_screen()
{
    if (mainDashboardView != MAIN_DASH_VALUES)
    {
        lcd_display_chart_screen();
        return;
    }
    // Nach dem Verlassen des Setup-Menues eventuelle RA8875-Kreisreste
    // vom FAN-Button VOR dem Hauptscreen-Transfer putzen.
    // Wichtig: Danach die TextBox-Sichtcaches invalidieren, sonst wuerden
    // unveraenderte Zeichen nicht neu gezeichnet und es entstuende ein
    // schwarzes Loch im Hauptscreen.
    bool fanGhostWasCleared = fanButtonMainGhostClearActive();
    if (fanGhostWasCleared)
    {
        clearFanButtonGhostOnMainScreen();
        VirtLCDUeberschrift.invalidate();
        VirtLCDGrossanzeige.invalidate();
        VirtLCDTemperaturen.invalidate();
        VirtLCDStatuszeile.invalidate();
        RTC_PM.invalidate();
    }

    const uint8_t currentLayout = mainScreenLayoutGet();
    if (mainDashboardLastLayout != currentLayout)
    {
        mainDashboardLastLayout = currentLayout;
        tft.fillRect(MAIN_CHART_FRAME_X,
                     MAIN_CHART_FRAME_Y,
                     MAIN_CHART_FRAME_W,
                     MAIN_CHART_FRAME_H,
                     BLACK);
        TFTWAIT();
        eraseDisplay();
        resetMeasurementStatusCache();
    }
    const bool altValuesLayout = (currentLayout == MAIN_SCREEN_LAYOUT_3VALUES);

    // ------------------------------------------------------------------------
    // ÜBERSCHRIFT
    // ------------------------------------------------------------------------

    VirtLCDUeberschrift.setCursor(0, 0);
    VirtLCDUeberschrift.print(T(TXT_MAIN_TITLE));

    // ------------------------------------------------------------------------
    // GROSSANZEIGE
    // ------------------------------------------------------------------------

    VirtLCDGrossanzeige.setCursor(0, 0);

    displayFormatValueOrDashes(strBuf, sizeof(strBuf), relativeFeuchte, 6, 2);

    VirtLCDGrossanzeige.print(" ");
    VirtLCDGrossanzeige.print(strBuf);
    VirtLCDGrossanzeige.print(" \xB5""rH");

    VirtLCDGrossanzeige.setCursor(0, 1);

    displayFormatValueOrDashes(strBuf, sizeof(strBuf), präziserTaupunkt, 7, 3);

    VirtLCDGrossanzeige.print(" ");
    VirtLCDGrossanzeige.print(strBuf);
    VirtLCDGrossanzeige.print(" \xB0""C");

    if (altValuesLayout)
    {
        VirtLCDGrossanzeige.setCursor(0, 2);

        displayFormatValueOrDashes(strBuf, sizeof(strBuf), tempUmgebung, 7, 3);

        VirtLCDGrossanzeige.print(" ");
        VirtLCDGrossanzeige.print(strBuf);
        VirtLCDGrossanzeige.print(" \xB0""C");
    }

    // ------------------------------------------------------------------------
    // TEMPERATUREN
    // ------------------------------------------------------------------------

    VirtLCDTemperaturen.clear();

    if (!altValuesLayout)
    {
        VirtLCDTemperaturen.setCursor(0, 0);

        displayFormatValueOrDashes(strBuf, sizeof(strBuf), tempSpiegel, 7, 3);

        VirtLCDTemperaturen.print(T(TXT_MAIN_T_MIRROR));
        VirtLCDTemperaturen.print(strBuf);
        VirtLCDTemperaturen.print("\xB0""C");

        VirtLCDTemperaturen.setCursor(0, 1);

        displayFormatValueOrDashes(strBuf, sizeof(strBuf), tempUmgebung, 7, 3);

        VirtLCDTemperaturen.print(T(TXT_MAIN_T_AMBIENT));
        VirtLCDTemperaturen.print(strBuf);
        VirtLCDTemperaturen.print("\xB0""C");
    }
    else
    {
        // T-Umgebung steht im alternativen Hauptscreen gross in der Mitte.
        // T-Spiegel rutscht deshalb eine Zeile nach unten auf die bisherige
        // T-Umgebung-Position rechts unten.
        VirtLCDTemperaturen.setCursor(0, 1);

        displayFormatValueOrDashes(strBuf, sizeof(strBuf), tempSpiegel, 7, 3);

        VirtLCDTemperaturen.print(T(TXT_MAIN_T_MIRROR));
        VirtLCDTemperaturen.print(strBuf);
        VirtLCDTemperaturen.print("\xB0""C");
    }

    // ------------------------------------------------------------------------
    // STATUSZEILE
    // ------------------------------------------------------------------------

    VirtLCDStatuszeile.setCursor(0, 0);

    VirtLCDStatuszeile.print("       P: ");

    char pBuf[16];
    char iBuf[16];

    displayFormatValueOrDashes(pBuf, sizeof(pBuf), baroDruckHPa, 7, 2);

    VirtLCDStatuszeile.print(pBuf);

    // Drei Leerzeichen nach I: reservieren Platz fuer den farbigen Richtungs-Punkt.
    VirtLCDStatuszeile.print(" hPa | I: ");

    displayFormatValueOrDashes(iBuf, sizeof(iBuf), amp_avg, 6, 3);

    VirtLCDStatuszeile.print(iBuf);

    VirtLCDStatuszeile.print(" A");

    // ------------------------------------------------------------------------
    // UHR
    // ------------------------------------------------------------------------

    char zeitBuf[16];

    sprintf(zeitBuf, "%02d:%02d:%02d", hour(), minute(), second());

    RTC_PM.setCursor(0, 0);

    RTC_PM.print(zeitBuf);

    // ------------------------------------------------------------------------
    // TRANSFER
    // ------------------------------------------------------------------------

    VirtLCDUeberschrift.transfer(); TFTWAIT();
    VirtLCDGrossanzeige.transfer(); TFTWAIT();
    VirtLCDTemperaturen.transfer(); TFTWAIT();
    VirtLCDStatuszeile.transfer(); TFTWAIT();
    RTC_PM.transfer(); TFTWAIT();

    // Durchfluss separat zeichnen, damit die bestehende P/I-Statuszeile
    // nicht verschoben wird.
    zeichneFlowStatusFeld();

    // Optionale Loop-Debug-Anzeige nach TextBox-Transfer zeichnen,
    // damit die Ueberschrift sie nicht ueberschreibt.
    zeichneLoopDebugMiniStatus();

    // Farbiger Peltier-Punkt erst nach dem TextBox-Transfer zeichnen,
    // damit die TextBox ihn nicht wieder ueberschreibt.
    zeichnePeltierRichtungsPunkt();
    zeichneFanMiniStatus();
    zeichneMesswertStatus();
    zeichneSystemMiniStatus();
    zeichneEthernetMiniStatus();

    // WICHTIG:
    // erst DANACH direkte TFT Befehle

    delayMicroseconds(200);

    // ------------------------------------------------------------------------
    // GRAFIK
    // ------------------------------------------------------------------------

    tft.drawPixel(0, 0, BLACK);

    TFTWAIT();

    // Gradzeichen sind jetzt direkt in DroidSansMono enthalten (0xB0).
    // Die frueheren separaten Kreis-Grafiken werden nicht mehr benoetigt.

    // ------------------------------------------------------------------------
    // SETUP BUTTON
    // ------------------------------------------------------------------------

    const uint16_t setupButtonColor = mainSetupButtonColor();
    tft.drawRoundRect(10, 395, 140, 75, 8, setupButtonColor);

    TFTWAIT();

    tft.setFont(DroidSansMono_20);

    TFTWAIT();

    tft.setTextColor(setupButtonColor, BLACK);

    TFTWAIT();

    tft.setCursor(39, 422);

    TFTWAIT();

    tft.print(T(TXT_MAIN_SETUP));

    TFTWAIT();
    mainSetupButtonRememberColorState();

}


// ============================================================================
// KLEINER SETUP-BUTTON FUER HAUPT- UND DIAGNOSE-SCREENS
// ============================================================================

static void drawSmallSetupButton()
{
    // Gleiche ruhige Optik wie beim Hauptscreen: nur Umrandung + Text,
    // keine grossen Setup-Menuebuttons.
    const uint16_t setupButtonColor = mainSetupButtonColor();

    tft.drawPixel(0, 0, BLACK);
    TFTWAIT();

    tft.drawRoundRect(10, 395, 140, 75, 5, setupButtonColor);
    TFTWAIT();

    tft.setFont(DroidSansMono_20);
    TFTWAIT();

    tft.setTextColor(setupButtonColor, BLACK);
    TFTWAIT();

    tft.setCursor(39, 422);
    TFTWAIT();

    tft.print(T(TXT_MAIN_SETUP));
    TFTWAIT();

    mainSetupButtonRememberColorState();
}

static void refreshSmallSetupButtonColor()
{
    const int8_t webBusy = ethernetWebSetupSessionActive() ? 1 : 0;
    if (mainSetupButtonLastWebBusy != webBusy)
    {
        drawSmallSetupButton();
    }
}


// ============================================================================
// DIAGNOSE-SCREENS 1/2 UND 2/2
// ============================================================================
// Wichtig:
// - Der Rahmen und die festen Labels werden nur beim Betreten/Seitenwechsel
//   neu gezeichnet.
// - Die Werte werden danach nur in kleinen Feldern aktualisiert.
// - Dadurch gibt es keinen sichtbaren Vollbild-Aufbau-Balken mehr.

static const int16_t DIAG_LEFT_LABEL_X      = 20;
static const int16_t DIAG_LEFT_VALUE_X      = 160;
static const int16_t DIAG_RIGHT_LABEL_X     = 390;
static const int16_t DIAG_RIGHT_VALUE_X     = 555;
static const int16_t DIAG_ROW_1             = 64;
static const int16_t DIAG_ROW_H             = 24;   // DroidSansMono_16 braucht wegen Unterlaengen (g, p, etc.) mehr Abstand
static const uint8_t DIAG_LEFT_VALUE_W      = 12;   // Zeichen, nicht Pixel
static const uint8_t DIAG_RIGHT_VALUE_W     = 12;   // Zeichen, nicht Pixel
static const uint32_t DIAG_UPDATE_MS        = 1000UL;

static bool diag_force_full_redraw = true;

void invalidateDiagnosisDisplay()
{
    diag_force_full_redraw = true;
}

static inline bool diagIsGerman()
{
    return ui_language == LANG_DE;
}

static const char* diagModeName()
{
    // Kurz halten, damit Kombifelder wie "PWM / Richtung" nicht ueberlaufen.
    if (aktuellerModus == 1) return diagIsGerman() ? "K\xB8HL" : "COOL";
    if (aktuellerModus == 2) return diagIsGerman() ? "HEIZ"  : "HEAT";
    return diagIsGerman() ? "AUS" : "OFF";
}

static const char* diagFlowName()
{
    if (ablaufStatus == 1) return diagIsGerman() ? "FREIHEIZ" : "HEATCLN";
    if (ablaufStatus == 2) return diagIsGerman() ? "LED-AUTO" : "LEDAUTO";
    return "NORMAL";
}

static const char* diagOkErr(bool ok)
{
    return ok ? "OK" : "ERR";
}

static void diagFormatHwStatus(char* out, size_t outSize)
{
    snprintf(out, outSize, "%s/%s/%s",
             diagOkErr(ads1263_bereit),
             diagOkErr(mcp3202_online && !mcp3202_fehler),
             diagOkErr(drv8873StabiBereit && drv8873SpiIsOk() && drv8873ConfigIsOk()));
}

static void diagPrintText(int16_t x, int16_t y, const char* text, uint16_t color = WHITE)
{
    tft.setFont(DroidSansMono_16);
    tft.setTextColor(color, BLACK);
    tft.setCursor(x, y);
    tpTftPrintUtf8(text);
}

static void adcInfoPrintText(int16_t x, int16_t y, const char* text, uint16_t color = WHITE)
{
    tft.setFont(DroidSansMono_14);
    tft.setTextColor(color, BLACK);
    tft.setCursor(x, y);
    tpTftPrintUtf8(text);
}

// Diagnose-Wertcache in RAM2, da er nur fuer Display-Redraw-Vergleiche genutzt wird.
static DMAMEM char diagValueCache[24][32];

static void diagResetValueCache()
{
    for (uint8_t i = 0; i < 24; i++)
    {
        diagValueCache[i][0] = '\0';
    }
}

static void diagMakeFixedField(char* out, size_t outSize, const char* text, uint8_t chars)
{
    if (outSize == 0) return;

    uint8_t n = 0;
    while (text && text[n] && n < chars && n < (outSize - 1))
    {
        out[n] = text[n];
        n++;
    }
    while (n < chars && n < (outSize - 1))
    {
        out[n++] = ' ';
    }
    out[n] = '\0';
}

static void diagWriteValue(uint8_t slot,
                           int16_t x,
                           int16_t y,
                           int16_t fieldChars,
                           const char* text,
                           uint16_t color = WHITE)
{
    char fixed[32];

    if (slot >= 24) return;

    if (fieldChars < 1) fieldChars = 1;
    if (fieldChars > 30) fieldChars = 30;

    diagMakeFixedField(fixed, sizeof(fixed), text, (uint8_t)fieldChars);

    if (strncmp(diagValueCache[slot], fixed, sizeof(diagValueCache[slot])) == 0) return;

    // Keine fillRect()-Felder fuer Werte benutzen. Stattdessen wird immer ein
    // fest breites Textfeld gezeichnet: erst das alte Feld schwarz, danach das
    // neue Feld weiss/gelb. Feldbreiten bewusst klein halten, damit beim
    // Uebermalen keine Labels der rechten Spalte geloescht werden.
    if (diagValueCache[slot][0] != '\0')
    {
        diagPrintText(x, y, diagValueCache[slot], BLACK);
        TFTWAIT();
    }

    size_t copyLen = strlen(fixed);
    if (copyLen >= sizeof(diagValueCache[slot]))
    {
        copyLen = sizeof(diagValueCache[slot]) - 1;
    }
    memcpy(diagValueCache[slot], fixed, copyLen);
    diagValueCache[slot][copyLen] = '\0';

    diagPrintText(x, y, fixed, color);
    TFTWAIT();
}

static void diagFormatFloat(char* out,
                            size_t outSize,
                            float value,
                            uint8_t width,
                            uint8_t decimals,
                            const char* suffix)
{
    char num[20];
    displayFormatValueOrDashes(num, sizeof(num), value, (int8_t)width, decimals);
    snprintf(out, outSize, "%s%s", num, (suffix != nullptr) ? suffix : "");
}

static void diagSetFloat(uint8_t slot,
                         int16_t x,
                         int16_t y,
                         int16_t w,
                         float value,
                         uint8_t width,
                         uint8_t decimals,
                         const char* suffix)
{
    char buf[32];
    diagFormatFloat(buf, sizeof(buf), value, width, decimals, suffix);
    diagWriteValue(slot, x, y, w, buf);
}

static void diagSetInt(uint8_t slot,
                       int16_t x,
                       int16_t y,
                       int16_t w,
                       long value,
                       const char* suffix = "")
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld%s", value, suffix);
    diagWriteValue(slot, x, y, w, buf);
}

static void diagSetStr(uint8_t slot,
                       int16_t x,
                       int16_t y,
                       int16_t w,
                       const char* value,
                       uint16_t color = WHITE)
{
    diagWriteValue(slot, x, y, w, value, color);
}

static void DISPLAY_FLASHMEM_NOINLINE diagDrawFrame(uint8_t page)
{
    tft.fillScreen(BLACK);
    TFTWAIT();

    tft.drawLine(10, 30, 790, 30, WHITE);
    tft.drawLine(10, 54, 790, 54, WHITE);
    // Untere Linie weiter nach unten: mehr Platz fuer die Diagnose-Zeilen,
    // aber der kleine SETUP-Button unten links bleibt frei.
    tft.drawLine(10, 380, 790, 380, WHITE);

    diagPrintText(20, 8, (page == 1) ? "HARDWARE DIAGNOSE" : "HARDWARE DIAGNOSE 2", YELLOW);
    diagPrintText(500, 8, (page == 1) ? "DEW POINT MIRROR" : "ADC / REGELUNG", YELLOW);

    // Hauptwerte, auf beiden Diagnose-Seiten identisch sichtbar.
    diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 0, "T-Spiegel:");
    diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 0, "PT100 Sp.:");

    diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 1, "T-Umgebung:");
    diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 1, "PT100 Umg.:");

    diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 2, "Taupunkt:");
    diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 2, (page == 1) ? "Ref Low:" : "ADC Ref Low:");

    diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 3, "Feuchte:");
    diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 3, (page == 1) ? "Ref High:" : "ADC Ref High:");

    if (page == 1)
    {
        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 5, "Regelung:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 5, "Kp:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 6, "PID Out:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 6, "Ki:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 7, "Peltier I:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 7, "Kd:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 8, "PWM/Richt:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 8, "Takt:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 10, "Kopf:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 10, "Serial:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 11, "Status:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 11, "ADS/MCP/DRV:");
    }
    else
    {
        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 5, "Optik net:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 5, "ADC Pt Sp:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 6, "Optik Ref:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 6, "ADC Pt Um:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 7, "LED Soll:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 7, "MCP cool:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 8, "Strom Soll:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 8, "MCP heat:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 10, "Integral:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 10, "Peltier raw:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 11, "A2 dark:");
        diagPrintText(DIAG_RIGHT_LABEL_X, DIAG_ROW_1 + DIAG_ROW_H * 11, "ADS/MCP/DRV:");

        diagPrintText(DIAG_LEFT_LABEL_X,  DIAG_ROW_1 + DIAG_ROW_H * 12, "A2 Ziel:");
    }

    diagPrintText(660, 400, (page == 1) ? "DIAG 1/2" : "DIAG 2/2", YELLOW);
    drawSmallSetupButton();
}

static void DISPLAY_FLASHMEM_NOINLINE diagDrawCommonValues(uint8_t page)
{
    diagSetFloat(0,  DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 0, DIAG_LEFT_VALUE_W,  tempSpiegel,      9, 4, " \xB0""C");
    diagSetFloat(1,  DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 0, DIAG_RIGHT_VALUE_W, ohmPt100_1,       8, 4, " \xB1");

    diagSetFloat(2,  DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 1, DIAG_LEFT_VALUE_W,  tempUmgebung,     9, 4, " \xB0""C");
    diagSetFloat(3,  DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 1, DIAG_RIGHT_VALUE_W, ohmPt100_2,       8, 4, " \xB1");

    diagSetFloat(4,  DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 2, DIAG_LEFT_VALUE_W,  präziserTaupunkt, 9, 4, " \xB0""C");

    if (page == 1)
    {
        diagSetFloat(5, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 2, DIAG_RIGHT_VALUE_W, ohmRef100R, 9, 5, " \xB1");
    }
    else
    {
        diagSetInt(5, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 2, DIAG_RIGHT_VALUE_W, (long)adcRawRef100R);
    }

    diagSetFloat(6,  DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 3, DIAG_LEFT_VALUE_W,  relativeFeuchte,  7, 2, " %");

    if (page == 1)
    {
        diagSetFloat(7, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 3, DIAG_RIGHT_VALUE_W, ohmRef120R, 9, 5, " \xB1");
    }
    else
    {
        diagSetInt(7, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 3, DIAG_RIGHT_VALUE_W, (long)adcRawRef120R);
    }
}

static void DISPLAY_FLASHMEM_NOINLINE diagDrawPage1Values()
{
    char buf[48];

    diagDrawCommonValues(1);

    diagSetStr(8, DIAG_LEFT_VALUE_X + 40, DIAG_ROW_1 + DIAG_ROW_H * 5, DIAG_LEFT_VALUE_W, diagModeName());
    diagSetInt(9, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 5, DIAG_RIGHT_VALUE_W, (long)R.pid_kp);

    diagSetFloat(10, DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 6, DIAG_LEFT_VALUE_W,  peltierStromSoll, 7, 0, " mA");
    diagSetFloat(11, DIAG_RIGHT_VALUE_X - 50, DIAG_ROW_1 + DIAG_ROW_H * 6, DIAG_RIGHT_VALUE_W, R.pid_ki,         7, 1, "");

    diagSetFloat(12, DIAG_LEFT_VALUE_X + 10,  DIAG_ROW_1 + DIAG_ROW_H * 7, DIAG_LEFT_VALUE_W,  (float)amp_avg,   7, 3, " A");
    diagSetFloat(13, DIAG_RIGHT_VALUE_X -50 , DIAG_ROW_1 + DIAG_ROW_H * 7, DIAG_RIGHT_VALUE_W, R.pid_kd,         7, 1, "");

    snprintf(buf, sizeof(buf), "%4ld / %s", (long)peltierSollWert, diagModeName());
    diagSetStr(14, DIAG_LEFT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 8, DIAG_LEFT_VALUE_W, buf);
    diagSetInt(15, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 8, DIAG_RIGHT_VALUE_W, (long)R.regler_intervall_ms, " ms");

    diagSetStr(16, DIAG_LEFT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 10, DIAG_LEFT_VALUE_W, headTypeTextGet());
    snprintf(buf, sizeof(buf), "%05lu", (unsigned long)R.head_serial);
    diagSetStr(17, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 10, DIAG_RIGHT_VALUE_W, buf);

    diagSetStr(18, DIAG_LEFT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 11, DIAG_LEFT_VALUE_W, diagFlowName());
    diagFormatHwStatus(buf, sizeof(buf));
    diagSetStr(19, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 11, DIAG_RIGHT_VALUE_W, buf);
}

static void DISPLAY_FLASHMEM_NOINLINE diagDrawPage2Values()
{
    diagDrawCommonValues(2);

    diagSetInt(8,  DIAG_LEFT_VALUE_X + 40,  DIAG_ROW_1 + DIAG_ROW_H * 5,  DIAG_LEFT_VALUE_W,  (long)adcRawPhotodiode);
    diagSetInt(9,  DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 5,  DIAG_RIGHT_VALUE_W, (long)adcRawPt100_1);

    diagSetFloat(10, DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 6, DIAG_LEFT_VALUE_W,  optikTrockenReferenz, 8, 0, "");
    diagSetInt(11, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 6,  DIAG_RIGHT_VALUE_W, (long)adcRawPt100_2);

    diagSetFloat(12, DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 7, DIAG_LEFT_VALUE_W, aktuelleLedStromVorgabe, 7, 1, " mA");
    diagSetInt(13, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 7,  DIAG_RIGHT_VALUE_W, (long)adcRawPeltierStromKuehlen);

    diagSetFloat(14, DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 8, DIAG_LEFT_VALUE_W, peltierStromSoll, 7, 0, " mA");
    diagSetInt(15, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 8,  DIAG_RIGHT_VALUE_W, (long)adcRawPeltierStromHeizen);

    diagSetFloat(16, DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 10, DIAG_LEFT_VALUE_W, integralFehler, 8, 2, "");
    diagSetInt(17, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 10, DIAG_RIGHT_VALUE_W, (long)adcRawPeltierStrom);

    if (ads1263Adc2DarkValid())
    {
        diagSetInt(18, DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 11, DIAG_LEFT_VALUE_W, (long)ads1263Adc2GetDark());
    }
    else
    {
        diagSetStr(18, DIAG_LEFT_VALUE_X,  DIAG_ROW_1 + DIAG_ROW_H * 11, DIAG_LEFT_VALUE_W, ads1263Adc2DarkMeasuring() ? "MEAS" : "--", CYAN);
    }
    char buf[48];
    diagFormatHwStatus(buf, sizeof(buf));
    diagSetStr(19, DIAG_RIGHT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 11, DIAG_RIGHT_VALUE_W, buf);

    diagSetInt(20, DIAG_LEFT_VALUE_X, DIAG_ROW_1 + DIAG_ROW_H * 12, DIAG_LEFT_VALUE_W, (long)getOptikZielRohwert());
}

static void DISPLAY_FLASHMEM_NOINLINE lcd_display_diagnose_page(uint8_t page)
{
    static uint8_t lastPage = 255;
    static unsigned long lastValueUpdateMs = 0;

    unsigned long now = millis();

    bool fullRedraw = false;
    if (lastPage != page) fullRedraw = true;
    if (diag_force_full_redraw) fullRedraw = true;
    if (flag.menu_lcd_upd) fullRedraw = true;   // Rueckkehr aus Setup/Anzeige-Menue

    if (fullRedraw)
    {
        lastPage = page;
        lastValueUpdateMs = 0;
        diag_force_full_redraw = false;
        flag.menu_lcd_upd = false;
        diagResetValueCache();
        diagDrawFrame(page);
    }

    refreshSmallSetupButtonColor();

    if (fullRedraw || (now - lastValueUpdateMs >= DIAG_UPDATE_MS))
    {
        lastValueUpdateMs = now;

        if (page == 1)
        {
            diagDrawPage1Values();
        }
        else
        {
            diagDrawPage2Values();
        }
    }
}

void lcd_display_metrology_debug()
{
    lcd_display_diagnose_page(1);
}

void lcd_display_metrology_debug2()
{
    lcd_display_diagnose_page(2);
}



// ============================================================================
// ADC-INFO-SCREEN V61: LIVE-MONITOR, KEINE ADS-EIGENMESSUNG
// ============================================================================
static bool adc_info_force_full_redraw = true;
static uint32_t adc_info_last_draw_ms = 0;
static DMAMEM char adcInfoValueCache[22][96];

// ADC-Info ist ein sehr dynamischer Screen. Grosse Ganzzeilen-Updates koennen
// den Loop stark blockieren. Deshalb werden nur kleine, fest positionierte
// Wertefelder geloescht und bei einer Aenderung komplett neu geschrieben.
#define ADC_INFO_LINE_H 20

void invalidateAdcInfoDisplay()
{
    adc_info_force_full_redraw = true;
}

static void adcInfoResetCache()
{
    for (uint8_t i = 0; i < 18; i++)
    {
        adcInfoValueCache[i][0] = '\0';
    }
}

static void adcInfoPrintCached(uint8_t idx, int16_t x, int16_t y, int16_t w, const char* text, uint16_t color, bool force)
{
    // Sichere ADC-Info-Variante:
    // Keine Font-Interna auswerten und kein zeichenweises Partial-Update.
    // Wenn sich ein Wert geaendert hat, wird nur sein kleines Feld geloescht
    // und komplett neu geschrieben. Das ist deutlich schneller als ganze Zeilen,
    // aber robust gegen Font-/Cursor-/Cache-Probleme.
    if (idx >= (sizeof(adcInfoValueCache) / sizeof(adcInfoValueCache[0]))) return;
    if (!text) text = "";

    char fixed[96];
    snprintf(fixed, sizeof(fixed), "%s", text);

    if (!force && strncmp(adcInfoValueCache[idx], fixed, sizeof(adcInfoValueCache[idx]) - 1) == 0)
        return;

    snprintf(adcInfoValueCache[idx], sizeof(adcInfoValueCache[idx]), "%s", fixed);

    tft.fillRect(x, y, w, ADC_INFO_LINE_H, BLACK);
    adcInfoPrintText(x, y, adcInfoValueCache[idx], color);
}

static void adcInfoFormatAdcLine(char* out, size_t outSize, const char* name, uint8_t ch, bool showPair)
{
    int32_t mn = 0;
    int32_t mx = 0;
    if (ads1263StatsAdcMinMax(ch, &mn, &mx))
    {
        if (showPair)
        {
            snprintf(out, outSize, "%-4s %11ld %11ld  D%3.1f P%3.1f",
                     name,
                     (long)mn,
                     (long)mx,
                     (double)ads1263StatsRawDiscardRate(ch),
                     (double)ads1263StatsPairDiscardRate(ch == 0 ? 0 : 1));
        }
        else
        {
            snprintf(out, outSize, "%-4s %11ld %11ld  D%3.1f",
                     name,
                     (long)mn,
                     (long)mx,
                     (double)ads1263StatsRawDiscardRate(ch));
        }
    }
    else
    {
        if (showPair)
        {
            snprintf(out, outSize, "%-4s %11s %11s  D%3.1f P%3.1f",
                     name, "--", "--",
                     (double)ads1263StatsRawDiscardRate(ch),
                     (double)ads1263StatsPairDiscardRate(ch == 0 ? 0 : 1));
        }
        else
        {
            snprintf(out, outSize, "%-4s %11s %11s  D%3.1f",
                     name, "--", "--", (double)ads1263StatsRawDiscardRate(ch));
        }
    }
}

static void adcInfoFormatTempLine(char* out, size_t outSize, const char* name, uint8_t ch)
{
    float mn = 0.0f;
    float mx = 0.0f;
    if (ads1263StatsTempMinMax(ch, &mn, &mx))
    {
        snprintf(out, outSize, "%-4s %9.4f %9.4f", name, (double)mn, (double)mx);
    }
    else
    {
        snprintf(out, outSize, "%-4s %8s %8s", name, "--.----", "--.----");
    }
}

static void DISPLAY_FLASHMEM_NOINLINE lcd_display_adc_info()
{
    // V61/V28: Der ADS wird hier NICHT mehr angefasst.
    // Der normale Hauptmesszyklus laeuft unveraendert weiter; dieser Screen
    // zeigt nur Statistik aus dem laufenden Messkarussell.
    //
    // ADC-Info ist Diagnose, kein Oszilloskop. Damit der Screen die ADC/Regelung
    // nicht mit Display-Zeichenzeit stoert, wird lange gesammelt und nur selten
    // ein Snapshot dargestellt. Messung/Regelung laufen im Hintergrund weiter.
    // Ablauf: 10 s sammeln, im 11-s-Raster kurz Anzeige aktualisieren.
    const uint32_t ADC_INFO_FIRST_SNAPSHOT_DELAY_MS = 10000UL;
    const uint32_t ADC_INFO_DISPLAY_PERIOD_MS       = 11000UL;

    static uint32_t adc_info_next_value_draw_ms = 0;
    static bool adc_info_values_need_full = true;

    // Wichtig: flag.menu_lcd_upd kann auch durch allgemeine Anzeige-Tasks gesetzt
    // werden. Im ADC-Info-Screen wuerde das sonst wieder haeufige Full-Redraws
    // ausloesen. Der ADC-Info-Screen wird nur ueber adc_info_force_full_redraw
    // komplett neu aufgebaut. Das Flag wird hier nur quittiert.
    bool fullRedraw = adc_info_force_full_redraw;
    if (flag.menu_lcd_upd)
    {
        flag.menu_lcd_upd = false;
    }
    uint32_t now = millis();

    refreshSmallSetupButtonColor();

    if (fullRedraw)
    {
        adc_info_force_full_redraw = false;
        flag.menu_lcd_upd = false;

        tft.fillScreen(BLACK);
        delayMicroseconds(200);
        adcInfoResetCache();

        drawSmallSetupButton();

        // Inhalt bewusst oberhalb des Setup-Buttons halten.
        tft.drawRect(10, 6, 780, 375, DKBLUE);
        tft.drawFastHLine(10, 44, 780, DKBLUE);

        adcInfoPrintText(20, 14, "ADC LIVE", YELLOW);
        adcInfoPrintText(20, 96,  "10s ADC      MIN         MAX   D/s  P/s", CYAN);
        adcInfoPrintText(492, 96, "10s TEMP   MIN       MAX", CYAN);
        adcInfoPrintText(20, 276, "DRV8873 / H-BRÜCKE", CYAN);
        adcInfoPrintText(492, 196, "ADC2 / TIA", CYAN);

        adc_info_values_need_full = true;
        adc_info_next_value_draw_ms = now + ADC_INFO_FIRST_SNAPSHOT_DELAY_MS;
        return;
    }

    if ((int32_t)(now - adc_info_next_value_draw_ms) < 0)
    {
        return;
    }

    adc_info_last_draw_ms = now;
    adc_info_next_value_draw_ms = now + ADC_INFO_DISPLAY_PERIOD_MS;

    bool forceValues = adc_info_values_need_full;
    adc_info_values_need_full = false;

    char line[96];

    snprintf(line, sizeof(line), "%s  SPI %s  %s  SET %lums  cyc/s %.1f  ok/s %.1f  max10s %.1fms",
             ads1263_bereit ? "OK" : "ERR",
             ads1263SpiGetLabel(),
             ads1263GetGainLabel(),
             (unsigned long)ads1263GetSettleMs(),
             (double)ads1263StatsCycleRate(),
             (double)ads1263StatsValidRate(),
             (double)ads1263StatsMaxCycleMs());
    adcInfoPrintCached(0, 20, 52, 760, line, ads1263_bereit ? GREEN : RED, forceValues);

    snprintf(line, sizeof(line), "ID/s %.1f  TO/s %.1f  RNG/s %.1f  IRQ %lu  GLT %lu  BUSY %lu  GAP %luus",
             (double)ads1263StatsIdRate(),
             (double)ads1263StatsTimeoutRate(),
             (double)ads1263StatsRngRate(),
             (unsigned long)ads1263StatsDrdyIsrCount(),
             (unsigned long)ads1263StatsDrdyGlitchCount(),
             (unsigned long)ads1263StatsDrdyBusyCount(),
             (unsigned long)ads1263StatsDrdyGapUs());
    adcInfoPrintCached(1, 20, 72, 760, line, CYAN, forceValues);

    adcInfoFormatAdcLine(line, sizeof(line), "MIR", 0, true);
    adcInfoPrintCached(2, 20, 124, 471, line, WHITE, forceValues);

    adcInfoFormatAdcLine(line, sizeof(line), "UMG", 1, true);
    adcInfoPrintCached(3, 20, 148, 471, line, WHITE, forceValues);

    adcInfoFormatAdcLine(line, sizeof(line), "R100", 2, false);
    adcInfoPrintCached(4, 20, 172, 471, line, WHITE, forceValues);

    adcInfoFormatAdcLine(line, sizeof(line), "R120", 3, false);
    adcInfoPrintCached(5, 20, 196, 471, line, WHITE, forceValues);

    float rMin = 0.0f;
    float rMax = 0.0f;

    snprintf(line, sizeof(line), "REF erlaubt %.2f...%.2f", 1.19, 1.21);
    adcInfoPrintCached(9, 20, 220, 471, line, YELLOW, forceValues);

    if (ads1263StatsRatioMinMax(&rMin, &rMax))
    {
        snprintf(line, sizeof(line), "REF  ratio %8.5f %8.5f  P%3.1f",
                 (double)rMin, (double)rMax, (double)ads1263StatsPairDiscardRate(2));
    }
    else
    {
        snprintf(line, sizeof(line), "REF  ratio %8s %8s  P%3.1f", "--.-----", "--.-----", (double)ads1263StatsPairDiscardRate(2));
    }
    adcInfoPrintCached(6, 20, 244, 471, line, WHITE, forceValues);

    bool drvSpiOk = drv8873SpiIsOk();
    bool drvCfgOk = drv8873ConfigIsOk();
    bool nFaultHigh = (digitalReadFast(pinNfault) == HIGH);

    snprintf(line, sizeof(line), "SPI %-3s CFG %-3s nF %-4s",
             drvSpiOk ? "OK" : "ERR",
             drvCfgOk ? "OK" : "ERR",
             nFaultHigh ? "HIGH" : "LOW");
    adcInfoPrintCached(15, 20, 300, 471, line,
                       (drvSpiOk && drvCfgOk && nFaultHigh) ? GREEN : RED,
                       forceValues);

    snprintf(line, sizeof(line), "REG F%02X D%02X C%02X I%02X X%02X SE%u",
             (unsigned)drv8873GetLastFaultRegister(),
             (unsigned)drv8873GetLastDiagRegister(),
             (unsigned)drv8873GetLastIc1Register(),
             (unsigned)drv8873GetLastIc3Register(),
             (unsigned)drv8873GetLastIc4Register(),
             (unsigned)drv8873GetSpiErrorCounter());
    adcInfoPrintCached(16, 20, 324, 471, line,
                       drvSpiOk ? WHITE : RED,
                       forceValues);

    snprintf(line, sizeof(line), "SAFE %s",
             safetyIsFaultActive() ? safetyGetFaultText() : "OK");
    adcInfoPrintCached(18, 20, 348, 471, line,
                       safetyIsFaultActive() ? RED : GREEN,
                       forceValues);

    adcInfoFormatTempLine(line, sizeof(line), "Tmir", 0);
    adcInfoPrintCached(7, 492, 124, 278, line, WHITE, forceValues);

    adcInfoFormatTempLine(line, sizeof(line), "Tumg", 1);
    adcInfoPrintCached(8, 492, 148, 278, line, WHITE, forceValues);

  

    int32_t a2Min = 0;
    int32_t a2Max = 0;
    if (ads1263Adc2GetMinMax(&a2Min, &a2Max))
    {
        snprintf(line, sizeof(line), "raw %ld avg %ld",
                 (long)ads1263Adc2GetRaw(),
                 (long)ads1263Adc2GetAvg());
        adcInfoPrintCached(10, 492, 220, 278, line, WHITE, forceValues);

        snprintf(line, sizeof(line), "min %ld max %ld", (long)a2Min, (long)a2Max);
        adcInfoPrintCached(11, 492, 244, 278, line, WHITE, forceValues);

        snprintf(line, sizeof(line), "net %ld", (long)ads1263Adc2GetNet());
        adcInfoPrintCached(12, 492, 268, 278, line, WHITE, forceValues);

        snprintf(line, sizeof(line), "Rate %.0f/s buf %u/%u",
                 (double)ads1263Adc2GetRate(),
                 (unsigned)ads1263Adc2GetBufCount(),
                 (unsigned)ads1263Adc2GetBufSize());
        adcInfoPrintCached(13, 492, 292, 278, line, WHITE, forceValues);

        if (ads1263Adc2DarkValid())
        {
            snprintf(line, sizeof(line), "dark %ld", (long)ads1263Adc2GetDark());
        }
        else
        {
            snprintf(line, sizeof(line), "dark %s",
                     ads1263Adc2DarkMeasuring() ? "MEAS" : "--");
        }
        adcInfoPrintCached(14, 492, 316, 278, line, YELLOW, forceValues);

        snprintf(line, sizeof(line), "Ziel %ld", (long)getOptikZielRohwert());
        adcInfoPrintCached(17, 492, 340, 278, line, YELLOW, forceValues);
    }
    else
    {
        snprintf(line, sizeof(line), "wartet...");
        adcInfoPrintCached(10, 492, 220, 278, line, CYAN, forceValues);
        adcInfoPrintCached(11, 492, 244, 278, "", CYAN, forceValues);
        adcInfoPrintCached(12, 492, 268, 278, "", CYAN, forceValues);
        adcInfoPrintCached(13, 492, 292, 278, "", CYAN, forceValues);
        adcInfoPrintCached(14, 492, 316, 278, "", CYAN, forceValues);
        adcInfoPrintCached(17, 492, 340, 278, "", CYAN, forceValues);
    }
}


// ============================================================================
// DISPLAY UPDATE
// ============================================================================

void aktualisiereDisplayAnzeige()
{
    if (flag.config_mode)
    {
        ConfigMenu();
    }
    else
    {
        if (Menu_exit_timer == 0)
        {
            if (mode_display == 0)
            {
                lcd_display_klima_screen();
            }
            else if (mode_display == 1)
            {
                lcd_display_metrology_debug();
                return;
            }
            else if (mode_display == 2)
            {
                lcd_display_metrology_debug2();
                return;
            }
            else if (mode_display == 3)
            {
                lcd_display_adc_info();
                return;
            }
            else
            {
                mode_display = 0;
            }
        }
    }

    // ------------------------------------------------------------------------
    // STABILISIERT:
    // kein alternating transfer mehr
    // ------------------------------------------------------------------------

    VirtLCDUeberschrift.transfer(); TFTWAIT();

    // VirtLCDMessage gehoert ausschliesslich zu Setup-/Meldungsseiten.
    // Nach dem Verlassen des Setups besitzt sie weiterhin deren grosse
    // Geometrie. Ein Transfer im Hauptscreen loeschte deshalb als schwarzes
    // Rechteck Teile des Messwertrahmens und der Loop-Anzeige.

    VirtLCDTemperaturen.transfer(); TFTWAIT();
    VirtLCDStatuszeile.transfer(); TFTWAIT();
    VirtLCDGrossanzeige.transfer(); TFTWAIT();
}