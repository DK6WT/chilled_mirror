/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: Taupunkt_Regelung.ino
 * Zweck: Optische Taupunktregelung, LED-Autoeichung und Peltier-Sollwertbildung.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

//
// Sichere Regelungs-Version für Taupunktspiegel
//
// Änderungen:
// - 12-Bit PWM-Bereich 0..4095 bleibt erhalten
// - Dithering bleibt erhalten
// - Stromregler nutzt Betrag von adcRawPeltierStrom
// - harter Überstrom- und Übertemperatur-Not-Aus
// - H-Brücken-Totzeit kommt jetzt aus R.h_bruecke_totzeit
// - Sensor-Mittelwertfilter wird sauber in verarbeitePhysikalischeFormeln() geleert
// - PID-D-Term begrenzt, damit optische Sprünge nicht hart durchschlagen
// - Regelintervall wird robust aus R.regler_intervall_ms gelesen
// - Optik-Sollwert wird jetzt aus R.optik_sollwert gelesen
// - kein unkontrolliertes Hochlaufen bei unplausiblen Sensorwerten
// - Taupunkt-/Frostpunkt-Offset wird auf Taupunkt und rH angewendet, nicht auf T-Spiegel

#include <Arduino.h>
#include <math.h>
#include "TP_T.h"

// ============================================================================
// EXTERNE ROHWERTE UND PINS
// ============================================================================

extern volatile int32_t adcRawPt100_1;
extern volatile int32_t adcRawPt100_2;
extern volatile int32_t adcRawRef120R;
extern volatile int32_t adcRawRef100R;
extern volatile int32_t adcRawPhotodiode;
extern volatile int32_t adcRawPeltierStrom;

extern const byte pinEn;
extern bool safetyIsFaultActive();
extern bool safetyPeltierAllowed();
extern const byte pinPh;

extern float baroDruckHPa;
extern var_t R;

extern double amp_avg;

extern void setTargetCurrent(float currentMA);
// ADC2 Photodiode: Brutto = echte ADC-Aussteuerung, Netto = Dunkelwert abgezogen.
// Wichtig: LED-Autoeichung nutzt Brutto, Reflexionsregelung nutzt Netto.
extern int32_t ads1263Adc2GetAvg();
extern int32_t ads1263Adc2GetDark();
extern int32_t ads1263Adc2GetNet();
extern void adc1SfocalAutoCalEvent();
void berechnePräziseTemperaturen();

// Echte Referenzwiderstände aus Ref100_120_Calibration.ino
extern double refCalGet100Ohm();
extern double refCalGet120Ohm();
extern double refCalApplyChannelCorrection(uint8_t channel, double ohm);
extern void extRefCalNoteRawSample(int32_t rawPtA, int32_t rawPtB, int32_t rawRef100, int32_t rawRef120);
extern bool extRefCalIsActive(void);

// Taupunkt-/Frostpunkt-Offset aus PT100_2P_Calibration.ino
extern double taupunktOffsetGetC();

// Optikqualitaet / Status-Information
extern void opticHealthUpdateFromAutoCal(bool targetReached, bool timeout,
                                         float led_mA, float target, float brutto,
                                         float dark, float netto, float trockenRef,
                                         float restError, float noisePp,
                                         uint32_t durationMs,
                                         uint16_t coarseSteps, uint16_t fineSteps);


// ============================================================================
// GLOBALE PHYSIKALISCHE AUSGABEWERTE
// ============================================================================

float ohmPt100_1 = 0.0f;
float ohmPt100_2 = 0.0f;
float ohmRef120R = 120.0f;
float ohmRef100R = 100.0f;

float tempSpiegel = 0.0f;
float tempUmgebung = 0.0f;
float relativeFeuchte = 0.0f;
float optikReflexion = 0.0f;
float präziserTaupunkt = 0.0f;

uint8_t aktuellerModus = 0;
unsigned long umschaltTimer = 0;


// ============================================================================
// PWM / STROM / SICHERHEIT
// ============================================================================

// 12 Bit PWM-Leistungsbereich bleibt logisch 0..4095.
// Wichtig fuer Teensy: analogWriteResolution(12) verwendet fuer hartes 100%-HIGH den Wert 4096.
static const float PWM_MAX = 4095.0f;
static const int PWM_MAX_INT = 4095;
static const int PWM_HARD_HIGH_INT = 4096;

// Peltier-Leistungsgrenzen fuer den Test-/Basisbetrieb.
// Versorgung aktuell als 12,0 V angenommen.
// 8,0 V / 12,0 V * 4095 = 2730; wir nehmen 2700 als leicht konservative Grenze.
static const float PELTIER_MAX_VOLTAGE_V = 8.0f;
static const float PELTIER_SUPPLY_VOLTAGE_V = 12.0f;
static const int   PELTIER_PWM_LIMIT = 2700;
static const float PELTIER_CURRENT_LIMIT_MA = 2500.0f;  // Default; aktives Kopf-Limit kommt aus peltierCurrentLimitGetMa()
static const float PELTIER_CURRENT_MEASURE_MAX_MA      = 4096.0f;  // Messbereichsende Peltierstrom
static const float PELTIER_CURRENT_SATURATION_TRIP_MA = 4080.0f;  // nahe ADC-Saettigung sicher abschalten
static const float PELTIER_OVERCURRENT_MARGIN_MIN_MA  = 80.0f;    // Mindestreserve ueber Kopf-Limit
static const float PELTIER_OVERCURRENT_MARGIN_FRAC    = 0.05f;    // 5 % Reserve ueber Kopf-Limit

// Innerer Stromregler
float peltierStromSoll = 0.0f;
float kp_strom = 0.8f;
float ki_strom = 0.2f;
float stromIntegral = 0.0f;

// Betriebsgrenzen
// Das normale Peltier-Stromlimit ist kopfspezifisch und kommt aus dem
// geschuetzten Regelparameter / der Kopfkalibrierdatei.
static float maxPeltierStromMa()
{
  return (float)peltierCurrentLimitGetMa();
}

static float hardOvercurrentMa()
{
  // Dynamische Schutzgrenze aus dem kopfspezifischen Regelparameter:
  // Der Regler wird hart auf das Kopf-Limit begrenzt, die Safety laesst
  // zusaetzlich max(5 %, 80 mA) Reserve fuer Messrauschen/kurze Ueberschwinger.
  // Da der Peltierstrom nur bis 4096 mA messbar ist, wird die Safety vor
  // Messbereichssaettigung bei 4080 mA gedeckelt.
  float lim = maxPeltierStromMa();
  if (lim < 0.0f) lim = 0.0f;

  float margin = lim * PELTIER_OVERCURRENT_MARGIN_FRAC;
  if (margin < PELTIER_OVERCURRENT_MARGIN_MIN_MA)
  {
    margin = PELTIER_OVERCURRENT_MARGIN_MIN_MA;
  }

  float trip = lim + margin;
  float maxTrip = PELTIER_CURRENT_MEASURE_MAX_MA;
  if (maxTrip > PELTIER_CURRENT_SATURATION_TRIP_MA)
  {
    maxTrip = PELTIER_CURRENT_SATURATION_TRIP_MA;
  }

  if (trip > maxTrip)
  {
    trip = maxTrip;
  }

  return trip;
}
static const float HARD_OVERTEMP_C      = 85.0f;        // harter Not-Aus Spiegel
static const float HARD_UNDERTEMP_C     = -80.0f;       // Sensor-Plausibilität
static const float HARD_MAX_SENSOR_C    = 150.0f;       // Sensor-Plausibilität

// Optische PID-Ausgangsbegrenzung
static const float MAX_D_TERM_MA     = 1200.0f;


// ============================================================================
// DITHERING
// ============================================================================

extern float peltierSollWert;
extern unsigned long lastDitherTime;
extern uint8_t ditherCycle;

// Fractional-Accumulator fuer die Peltier-PWM.
// Er ersetzt das alte 4-Phasen-Dither-Raster und verteilt den Nachkommaanteil
// ueber die laufenden Hauptloop-Aufrufe. Dadurch stimmt der PWM-Mittelwert
// langfristig deutlich genauer, ohne den Regler selbst anzufassen.
static float pwmDitherAccumulator = 0.0f;
static uint8_t pwmDitherLastModus = 255;

static void resetPwmDither()
{
  pwmDitherAccumulator = 0.0f;
  ditherCycle = 0;
  pwmDitherLastModus = 255;
}


// ============================================================================
// ZEITSTEUERUNG
// ============================================================================

unsigned long letzteRegelZeit = 0;


// ============================================================================
// FILTERUNG
// ============================================================================

double filterSummePt100_1 = 0.0;
double filterSummePt100_2 = 0.0;
double filterSummeOptik = 0.0;
uint16_t filterZaehler = 0;


// ============================================================================
// GLEITENDE MITTELWERTE
// ============================================================================

float pufferTempSpiegel[5] = {0.0f};

#define UMGEBUNG_PUFFER_GROESSE 5
float pufferTempUmgebung[UMGEBUNG_PUFFER_GROESSE] = {0.0f};

uint8_t zeigerSpiegel = 0;
uint8_t zeigerUmgebung = 0;
bool pufferVollSpiegel = false;
bool pufferVollUmgebung = false;

#define DRUCK_PUFFER_GROESSE 150
float pufferBaroDruck[DRUCK_PUFFER_GROESSE] = {0.0f};
uint16_t zeigerDruck = 0;
bool pufferVollDruck = false;


// ============================================================================
// ÄUSSERER PID
// ============================================================================

float integralFehler = 0.0f;
float letzterFehler = 0.0f;


// ============================================================================
// KALIBRIER- UND ABLAUFSTEUERUNG
// ============================================================================

float optikTrockenReferenz = 15000.0f;
float aktuelleLedStromVorgabe = 10.0f;

const float OPTIK_ZIEL_ROHWERT = 6500000.0f;

// ============================================================================
// AUTO-CAL / FREIHEIZEN: TESTPARAMETER V31
// ============================================================================
// Diese Werte sind bewusst zentral im Code abgelegt, damit wir sie beim Testen
// schnell anpassen koennen, ohne EEPROM-Strukturen zu aendern.
// Auto-Cal Intervall kommt aus dem Setup-Menue (R.optik_autocal_interval_index).
extern uint32_t optikAutoCalIntervalMs();
static const unsigned long OPTIK_FREIHEIZ_MIN_MS           = 2000UL;
static const unsigned long OPTIK_FREIHEIZ_MAX_MS           = 60000UL;
static const unsigned long OPTIK_FREIHEIZ_STABLE_MS        = 2000UL;
static const float         OPTIK_FREIHEIZ_STABLE_PP        = 100000.0f;

static const unsigned long OPTIK_AUTOCAL_TIMEOUT_MS        = 10000UL;
static const unsigned long OPTIK_AUTOCAL_STABLE_MS         = 500UL;
static const unsigned long OPTIK_AUTOCAL_STEP_PERIOD_MS    = 50UL;
static const float         OPTIK_AUTOCAL_GROB_FENSTER      = 250000.0f;
static const float         OPTIK_AUTOCAL_FEIN_FENSTER      = 50000.0f;
static const float         OPTIK_AUTOCAL_STABLE_PP         = 50000.0f;
static const float         OPTIK_AUTOCAL_STEP_GROB_MA      = 1.0f;
static const float         OPTIK_AUTOCAL_STEP_FEIN_MA      = 0.1f;
static const float         OPTIK_LED_MIN_MA                = 2.0f;
static const float         OPTIK_LED_MAX_MA                = 80.0f;

float getOptikZielRohwert()
{
  return OPTIK_ZIEL_ROHWERT;
}

uint8_t ablaufStatus = 1;
unsigned long statusTimer = 0;

static uint8_t letzterAblaufStatus = 255;

// Freiheiz-Stabilitaet
static unsigned long freiHeizStableStartMs = 0;
static float freiHeizOptikMin = 0.0f;
static float freiHeizOptikMax = 0.0f;
static bool freiHeizStableInit = false;

// LED-Auto-Cal Ablaufdiagnose
static unsigned long autoCalStartMs = 0;
static unsigned long autoCalLastStepMs = 0;
static unsigned long autoCalStableStartMs = 0;
static float autoCalStableMin = 0.0f;
static float autoCalStableMax = 0.0f;
static bool autoCalStableInit = false;
static uint16_t autoCalCoarseSteps = 0;
static uint16_t autoCalFineSteps = 0;


// ============================================================================
// HILFSFUNKTIONEN
// ============================================================================

static inline float clampf_local(float v, float lo, float hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void optikResetFreiheizStabilitaet()
{
  freiHeizStableStartMs = 0;
  freiHeizOptikMin = 0.0f;
  freiHeizOptikMax = 0.0f;
  freiHeizStableInit = false;
}

static bool optikUpdateFreiheizStabilitaet(float wert, unsigned long nowMs)
{
  if (!freiHeizStableInit)
  {
    freiHeizStableInit = true;
    freiHeizStableStartMs = nowMs;
    freiHeizOptikMin = wert;
    freiHeizOptikMax = wert;
    return false;
  }

  if (wert < freiHeizOptikMin) freiHeizOptikMin = wert;
  if (wert > freiHeizOptikMax) freiHeizOptikMax = wert;

  float pp = freiHeizOptikMax - freiHeizOptikMin;
  if (pp > OPTIK_FREIHEIZ_STABLE_PP)
  {
    freiHeizStableStartMs = nowMs;
    freiHeizOptikMin = wert;
    freiHeizOptikMax = wert;
    return false;
  }

  return ((uint32_t)(nowMs - freiHeizStableStartMs) >= OPTIK_FREIHEIZ_STABLE_MS);
}

static void optikResetAutoCalState(unsigned long nowMs)
{
  autoCalStartMs = nowMs;
  autoCalLastStepMs = 0;
  autoCalStableStartMs = 0;
  autoCalStableMin = 0.0f;
  autoCalStableMax = 0.0f;
  autoCalStableInit = false;
  autoCalCoarseSteps = 0;
  autoCalFineSteps = 0;
}

// Eine gelatchte Safety beendet einen eventuell laufenden FREIHEIZEN-/
// LED-AUTO-Ablauf sofort logisch. Die Safety selbst bleibt aktiv und die
// H-Bruecke bleibt aus; verhindert wird nur, dass die Anzeige nach dem Fehler
// minutenlang auf FREIHEIZEN oder LED-AUTO stehen bleibt.
static void optikAbortAblaufBeiSafety(unsigned long nowMs)
{
  if (ablaufStatus == 0) return;

  ablaufStatus = 0;
  statusTimer = nowMs;
  letzterAblaufStatus = 255;

  optikResetFreiheizStabilitaet();
  optikResetAutoCalState(nowMs);

  integralFehler = 0.0f;
  letzterFehler = 0.0f;
}

static void optikAutoCalStabilitaetReset(float wert, unsigned long nowMs)
{
  autoCalStableInit = true;
  autoCalStableStartMs = nowMs;
  autoCalStableMin = wert;
  autoCalStableMax = wert;
}

static bool optikAutoCalStabilitaetUpdate(float wert, unsigned long nowMs, float* ppOut)
{
  if (!autoCalStableInit)
  {
    optikAutoCalStabilitaetReset(wert, nowMs);
    if (ppOut) *ppOut = 0.0f;
    return false;
  }

  if (wert < autoCalStableMin) autoCalStableMin = wert;
  if (wert > autoCalStableMax) autoCalStableMax = wert;

  float pp = autoCalStableMax - autoCalStableMin;
  if (ppOut) *ppOut = pp;

  if (pp > OPTIK_AUTOCAL_STABLE_PP)
  {
    optikAutoCalStabilitaetReset(wert, nowMs);
    return false;
  }

  return ((uint32_t)(nowMs - autoCalStableStartMs) >= OPTIK_AUTOCAL_STABLE_MS);
}

static void optikHealthAutoCalAbschluss(bool targetReached, bool timeout, float brutto, float noisePp)
{
  float trocken = optikTrockenReferenz;
  float rest = brutto - OPTIK_ZIEL_ROHWERT;
  opticHealthUpdateFromAutoCal(targetReached, timeout,
                               aktuelleLedStromVorgabe,
                               OPTIK_ZIEL_ROHWERT,
                               brutto,
                               (float)ads1263Adc2GetDark(),
                               (float)ads1263Adc2GetNet(),
                               trocken,
                               rest,
                               noisePp,
                               (uint32_t)(millis() - autoCalStartMs),
                               autoCalCoarseSteps,
                               autoCalFineSteps);
}

static void optikAblaufEnterIfNeeded()
{
  if (ablaufStatus == letzterAblaufStatus) return;

  letzterAblaufStatus = ablaufStatus;
  unsigned long nowMs = millis();

  if (ablaufStatus == 1)
  {
    optikResetFreiheizStabilitaet();
    adc1SfocalAutoCalEvent();
  }
  else if (ablaufStatus == 2)
  {
    optikResetAutoCalState(nowMs);
    autoCalLastStepMs = nowMs;
    setTargetCurrent(aktuelleLedStromVorgabe);
  }
}

static uint16_t holeRegelIntervallMs()
{
  uint16_t intervall = R.regler_intervall_ms;

  // Sicherheitsgrenzen:
  // zu klein überlastet den Ablauf, zu groß macht PID träge.
  if (intervall < 10) intervall = 62;
  if (intervall > 1000) intervall = 1000;

  return intervall;
}

static float holeRegelDtSekunden()
{
  return (float)holeRegelIntervallMs() / 1000.0f;
}

static float holeOptikSollwertProzent()
{
  uint16_t sollwert = R.optik_sollwert;

  // EEPROM-Wert ist Prozent * 10. Einstellbereich: 80.0 .. 99.0 %.
  // Bei alten/defekten EEPROM-Daten bleibt der bisherige Default 98.5 % aktiv.
  if (sollwert < 800 || sollwert > 990)
  {
    sollwert = 985;
  }

  return (float)sollwert / 10.0f;
}

static unsigned long holeHBrueckenTotzeitMs()
{
  unsigned long t = (unsigned long)R.h_bruecke_totzeit;

  // Nicht 0 ms erlauben. Für DRV8873/H-Brücke sicherer.
  if (t < 5UL) t = 5UL;
  if (t > 1000UL) t = 1000UL;

  return t;
}

static void drv8873PwmModeOff()
{
  // DRV8873 im PWM-Mode:
  // Pin37 = IN1, Pin36 = IN2.
  // IN1=HIGH und IN2=HIGH ergibt OUT1=H / OUT2=H, also 0 V Differenz.
  analogWrite(pinEn, PWM_HARD_HIGH_INT);
  analogWrite(pinPh, PWM_HARD_HIGH_INT);
}

static void drv8873PwmModeApply(uint8_t modus, int logicalPwm)
{
  if (logicalPwm < 0) logicalPwm = 0;
  if (logicalPwm > PELTIER_PWM_LIMIT) logicalPwm = PELTIER_PWM_LIMIT;

  // Oben in der Regelung bleibt die Logik normal:
  // logicalPwm = 0    -> aus
  // logicalPwm = 4095 -> volle Leistung
  // Am DRV-PWM-Mode ist die aktive Seite LOW-aktiv, daher hier invertieren.
  const int hwPwm = PWM_MAX_INT - logicalPwm;

  if (logicalPwm <= 0 || modus == 0)
  {
    drv8873PwmModeOff();
    return;
  }

  if (modus == 1)
  {
    // Richtung wie frueher PH=LOW im PH/EN-Konzept:
    // IN1 taktet LOW-aktiv, IN2 bleibt HIGH.
    analogWrite(pinEn, hwPwm);
    analogWrite(pinPh, PWM_HARD_HIGH_INT);
  }
  else if (modus == 2)
  {
    // Gegenrichtung:
    // IN1 bleibt HIGH, IN2 taktet LOW-aktiv.
    analogWrite(pinEn, PWM_HARD_HIGH_INT);
    analogWrite(pinPh, hwPwm);
  }
  else
  {
    drv8873PwmModeOff();
  }
}

static void peltierAus()
{
  peltierSollWert = 0.0f;
  peltierStromSoll = 0.0f;
  stromIntegral = 0.0f;
  resetPwmDither();
  drv8873PwmModeOff();
}

static void peltierNotAus(const char* grund)
{
  static unsigned long letzterLog = 0;

  peltierAus();
  aktuellerModus = 0;
  integralFehler = 0.0f;
  letzterFehler = 0.0f;

  if (millis() - letzterLog >= 1000UL)
  {
    letzterLog = millis();
    Serial.print("PELTIERSCHUTZ: ");
    Serial.println(grund);
  }
}

static bool sensorWertePlausibel()
{
  if (!isfinite(tempSpiegel)) return false;
  if (!isfinite(tempUmgebung)) return false;

  if (tempSpiegel < HARD_UNDERTEMP_C || tempSpiegel > HARD_MAX_SENSOR_C) return false;
  if (tempUmgebung < HARD_UNDERTEMP_C || tempUmgebung > HARD_MAX_SENSOR_C) return false;

  return true;
}

static bool pruefePeltierSicherheit()
{
  float istStromMA = fabsf((float)adcRawPeltierStrom);

  if (istStromMA >= hardOvercurrentMa())
  {
    peltierNotAus("Ueberstrom");
    return false;
  }

  if (tempSpiegel > HARD_OVERTEMP_C)
  {
    peltierNotAus("Spiegel-Uebertemperatur");
    return false;
  }

  if (!sensorWertePlausibel())
  {
    peltierNotAus("Sensorwert unplausibel");
    return false;
  }

  return true;
}


// ============================================================================
// SÄTTIGUNGSDAMPFDRUCK NACH HARDY/WEXLER (ITS-90, HARDY 1998)
// Rückgabe in hPa.
//
// tempCelsius >= 0 °C: über flüssigem Wasser
// tempCelsius <  0 °C: über Eis/Frost
//
// Formel/Koeffizienten: Hardy 1998 / Wexler, ITS-90 formulations for vapor pressure.
// Es wurde keine fremde Library und kein fremder Quellcode übernommen.
//
// Die Hardy/Wexler-Koeffizienten liefern e in Pa; für den bestehenden Code
// wird danach auf hPa umgerechnet, damit der Enhancement-Factor und die
// Feuchteberechnung wie bisher mit hPa weiterarbeiten.
// ============================================================================

double berechneReinenDampfdruckHardyWexler(double tempCelsius)
{
  double T = tempCelsius + 273.15;

  if (T <= 0.0 || !isfinite(T))
  {
    return 0.0;
  }

  double ln_e_pa = 0.0;

  if (tempCelsius >= 0.0)
  {
    // Hardy 1998 / Wexler, Sättigungsdampfdruck über Wasser, 0..100 °C
    double T2 = T * T;
    double T3 = T2 * T;
    double T4 = T3 * T;

    ln_e_pa = -2.8365744e3 / T2
            - 6.028076559e3 / T
            + 1.954263612e1
            - 2.737830188e-2 * T
            + 1.6261698e-5 * T2
            + 7.0229056e-10 * T3
            - 1.8680009e-13 * T4
            + 2.7150305 * log(T);
  }
  else
  {
    // Hardy 1998 / Wexler, Sättigungsdampfdruck über Eis, -100..0 °C
    double T2 = T * T;
    double T3 = T2 * T;

    ln_e_pa = -5.8666426e3 / T
            + 2.232870244e1
            + 1.39387003e-2 * T
            - 3.4262402e-5 * T2
            + 2.7040955e-8 * T3
            + 6.7063522e-1 * log(T);
  }

  return exp(ln_e_pa) / 100.0;
}


// ============================================================================
// ENHANCEMENT FACTOR NACH WMO
// ============================================================================

double berechneEnhancementFactor(double tempCelsius, double druckHPa)
{
  double f = 1.00062
           + (3.14e-6 * druckHPa)
           + (5.6e-7 * tempCelsius * tempCelsius);

  return f;
}


extern void ads1263StatsNoteTemp(float tMirror, float tAmbient);
extern void outputDataNoteSample(float tMirror, float tAmbient, float dewpoint, float rh, float pressure);

// ============================================================================
// 1. FORTLAUFENDE SENSOR-FILTERUNG
// ============================================================================

void berechneSensorWerte()
{
  // int32_t-Lesen ist auf Teensy 4.x praktisch atomar genug.
  // Die Werte können aus unterschiedlichen ADC-Zyklen stammen, das ist hier
  // unkritisch, da anschließend ohnehin gefiltert wird.
  filterSummePt100_1 += (double)adcRawPt100_1;
  filterSummePt100_2 += (double)adcRawPt100_2;
  filterSummeOptik   += (double)adcRawPhotodiode;

  if (filterZaehler < 60000)
  {
    filterZaehler++;
  }
}


// ============================================================================
// 2. PHYSIKALISCHE FORMELN UND FILTER
// ============================================================================

void verarbeitePhysikalischeFormeln()
{
  // Display-Stromvariable
  double aktuellerStromAmpere = (double)labs((long)adcRawPeltierStrom) / 1000.0;
  amp_avg = (amp_avg * 0.95) + (aktuellerStromAmpere * 0.05);

  if (filterZaehler == 0) return;

  double avgPt100_1 = filterSummePt100_1 / (double)filterZaehler;
  double avgPt100_2 = filterSummePt100_2 / (double)filterZaehler;
  double avgOptik   = filterSummeOptik   / (double)filterZaehler;

  // Filter direkt hier verbrauchen und löschen
  filterSummePt100_1 = 0.0;
  filterSummePt100_2 = 0.0;
  filterSummeOptik   = 0.0;
  filterZaehler = 0;

  optikReflexion = (float)avgOptik;

  // Ratiometrische Widerstandsberechnung über echte Ref100R/Ref120R
  if (adcRawRef100R > 0 && adcRawRef120R > 0)
  {
    double ref100Ohm = refCalGet100Ohm();
    double ref120Ohm = refCalGet120Ohm();

    double deltaRawRef = (double)adcRawRef120R - (double)adcRawRef100R;
    double deltaOhmRef = ref120Ohm - ref100Ohm;

    if (deltaRawRef > 0.0 && deltaOhmRef > 0.0)
    {
      double schritteProOhm = deltaRawRef / deltaOhmRef;

      if (schritteProOhm > 0.0 && isfinite(schritteProOhm))
      {
        ohmRef100R = (float)ref100Ohm;
        ohmRef120R = (float)ref120Ohm;

        double ohm1 = ref100Ohm + ((avgPt100_1 - (double)adcRawRef100R) / schritteProOhm);
        double ohm2 = ref100Ohm + ((avgPt100_2 - (double)adcRawRef100R) / schritteProOhm);

        // Kanal-Restkorrektur aus externem 4R-Abgleich.
        // Kanal 0 = Spiegel, Kanal 1 = Umgebung.
        ohm1 = refCalApplyChannelCorrection(0, ohm1);
        ohm2 = refCalApplyChannelCorrection(1, ohm2);

        ohmPt100_1 = (float)ohm1;
        ohmPt100_2 = (float)ohm2;

        // Externer Referenzabgleich sammelt nur mit; der ADS-Messablauf bleibt unveraendert.
        // Ext-Ref-Cal zaehlt nur neue ADC-Rohwertsaetze. Deshalb hier
        // die echten letzten ADC-Rohwerte uebergeben, nicht den schnelleren
        // Formel-/Regelungs-Mittelwert.
        extRefCalNoteRawSample(adcRawPt100_1,
                               adcRawPt100_2,
                               adcRawRef100R,
                               adcRawRef120R);
      }
    }
  }

  berechnePräziseTemperaturen();

  // Spiegeltemperatur 5er Mittel
  pufferTempSpiegel[zeigerSpiegel] = tempSpiegel;
  zeigerSpiegel++;

  if (zeigerSpiegel >= 5)
  {
    zeigerSpiegel = 0;
    pufferVollSpiegel = true;
  }

  float summeSpiegel = 0.0f;
  uint8_t anzahlSpiegel = pufferVollSpiegel ? 5 : zeigerSpiegel;

  for (uint8_t i = 0; i < anzahlSpiegel; i++)
  {
    summeSpiegel += pufferTempSpiegel[i];
  }

  if (anzahlSpiegel > 0)
  {
    tempSpiegel = summeSpiegel / (float)anzahlSpiegel;
  }

  // Umgebungstemperatur 20er Mittel
  pufferTempUmgebung[zeigerUmgebung] = tempUmgebung;
  zeigerUmgebung++;

  if (zeigerUmgebung >= UMGEBUNG_PUFFER_GROESSE)
  {
    zeigerUmgebung = 0;
    pufferVollUmgebung = true;
  }

  float summeUmgebung = 0.0f;
  uint8_t anzahlUmgebung = pufferVollUmgebung ? UMGEBUNG_PUFFER_GROESSE : zeigerUmgebung;

  for (uint8_t i = 0; i < anzahlUmgebung; i++)
  {
    summeUmgebung += pufferTempUmgebung[i];
  }

  if (anzahlUmgebung > 0)
  {
    tempUmgebung = summeUmgebung / (float)anzahlUmgebung;
  }

  // V61: ADC-Info-Live-Monitor bekommt 10-s-Min/Max der berechneten Temperaturen.
  ads1263StatsNoteTemp(tempSpiegel, tempUmgebung);

  // Luftdruck glätten
  float roherDruck = (baroDruckHPa > 200.0f && baroDruckHPa < 1300.0f) ? baroDruckHPa : 1013.25f;

  pufferBaroDruck[zeigerDruck] = roherDruck;
  zeigerDruck++;

  if (zeigerDruck >= DRUCK_PUFFER_GROESSE)
  {
    zeigerDruck = 0;
    pufferVollDruck = true;
  }

  float summeDruck = 0.0f;
  uint16_t anzahlDruck = pufferVollDruck ? DRUCK_PUFFER_GROESSE : zeigerDruck;

  for (uint16_t i = 0; i < anzahlDruck; i++)
  {
    summeDruck += pufferBaroDruck[i];
  }

  if (anzahlDruck > 0)
  {
    baroDruckHPa = summeDruck / (float)anzahlDruck;
  }

  // Relative Feuchte aus Taupunkt/Frostpunkt berechnen.
  // T-Spiegel bleibt die echte Spiegeltemperatur. Der Taupunkt-Offset ist eine
  // kleine metrologische 1-Punkt-Korrektur fuer Taupunkt/Frostpunkt und wirkt
  // deshalb auch auf die daraus berechnete rH.
  double liveDruck = (double)baroDruckHPa;
  double taupunktKorrigiert = (double)tempSpiegel + taupunktOffsetGetC();

  if (!isfinite(taupunktKorrigiert))
  {
    taupunktKorrigiert = (double)tempSpiegel;
  }

  double e_pure_umgebung = berechneReinenDampfdruckHardyWexler((double)tempUmgebung);
  double f_umgebung = berechneEnhancementFactor((double)tempUmgebung, liveDruck);
  double ew_umgebung = e_pure_umgebung * f_umgebung;

  double e_pure_taupunkt = berechneReinenDampfdruckHardyWexler(taupunktKorrigiert);
  double f_taupunkt = berechneEnhancementFactor(taupunktKorrigiert, liveDruck);
  double e_aktuell = e_pure_taupunkt * f_taupunkt;

  if (ew_umgebung > 0.0)
  {
    relativeFeuchte = (float)((e_aktuell / ew_umgebung) * 100.0);
  }
  else
  {
    relativeFeuchte = 0.0f;
  }

  relativeFeuchte = clampf_local(relativeFeuchte, 0.0f, 100.0f);

  präziserTaupunkt = (float)taupunktKorrigiert;

  // Ausgabe-Filter nur fuer Daten-Ausgaben fuettern.
  // Anzeige, Regelung und Safety bleiben unveraendert.
  // Nur bei neuem fertigem Pt100 +/- Satz uebernehmen, damit der
  // ADC-Ausgabemodus wirklich die internen ca. 3 Hz abbildet und nicht
  // den schnelleren Regelungs-Takt mit doppelten Werten.
  static bool outputLastRawValid = false;
  static int32_t outputLastRawPt1 = 0;
  static int32_t outputLastRawPt2 = 0;
  if (!outputLastRawValid ||
      adcRawPt100_1 != outputLastRawPt1 ||
      adcRawPt100_2 != outputLastRawPt2)
  {
    outputLastRawValid = true;
    outputLastRawPt1 = adcRawPt100_1;
    outputLastRawPt2 = adcRawPt100_2;
    outputDataNoteSample(tempSpiegel, tempUmgebung, präziserTaupunkt, relativeFeuchte, baroDruckHPa);
  }
}


// ============================================================================
// 3. KASKADIERTER TAUPUNKT-PID-ALGORITHMUS
// ============================================================================

void ausfuehrungRegelAlgorithmus()
{
  float gewuenschterStromStellwert = 0.0f;
  uint8_t gewuenschterModus = 0;

  float dt = holeRegelDtSekunden();

  optikAblaufEnterIfNeeded();

  // Sicherheitscheck zuerst
  if (!pruefePeltierSicherheit())
  {
    return;
  }

  switch (ablaufStatus)
  {
    // ------------------------------------------------------------------------
    // NORMALER MESSBETRIEB
    // ------------------------------------------------------------------------
    case 0:
    {
      if (millis() - statusTimer >= optikAutoCalIntervalMs())
      {
        ablaufStatus = 1;
        statusTimer = millis();

        integralFehler = 0.0f;
        letzterFehler = 0.0f;
        aktuelleLedStromVorgabe = 10.0f;

        gewuenschterStromStellwert = 0.0f;
      }
      else
      {
        if (optikTrockenReferenz < 1000.0f)
        {
          gewuenschterStromStellwert = 0.0f;
          break;
        }

        float optikIstProzent = (optikReflexion / optikTrockenReferenz) * 100.0f;
        float optikSollwertProzent = holeOptikSollwertProzent();
        float optikFehler = optikIstProzent - optikSollwertProzent;

        float pTerm = (float)R.pid_kp * optikFehler;

        integralFehler += optikFehler * dt;
        integralFehler = clampf_local(integralFehler, -2000.0f, 2000.0f);

        float iTerm = (float)R.pid_ki * integralFehler;

        float dRaw = (optikFehler - letzterFehler) / dt;
        float dTerm = (float)R.pid_kd * dRaw;

        // D-Term begrenzen, damit optische Sprünge nicht direkt hart einschlagen
        dTerm = clampf_local(dTerm, -MAX_D_TERM_MA, MAX_D_TERM_MA);

        letzterFehler = optikFehler;

        gewuenschterStromStellwert = pTerm + iTerm + dTerm;
      }
    }
    break;

    // ------------------------------------------------------------------------
    // FREIHEIZEN
    // ------------------------------------------------------------------------
    case 1:
    {
      unsigned long nowMs = millis();
      unsigned long elapsed = nowMs - statusTimer;
      float freiHeizZiel = (float)R.freiheiz_ziel_temp[0];

      if (freiHeizZiel < 40.0f || freiHeizZiel > 76.0f)
      {
        freiHeizZiel = 76.0f;
      }

      float optikAdcBrutto = (float)ads1263Adc2GetAvg();
      bool optikStabil = optikUpdateFreiheizStabilitaet(optikAdcBrutto, nowMs);

      bool minZeitOk = elapsed >= OPTIK_FREIHEIZ_MIN_MS;
      bool zielErreicht = tempSpiegel >= freiHeizZiel;
      bool maxZeitErreicht = elapsed >= OPTIK_FREIHEIZ_MAX_MS;

      if ((minZeitOk && zielErreicht && optikStabil) || maxZeitErreicht)
      {
        ablaufStatus = 2;
        statusTimer = nowMs;

        peltierAus();
        setTargetCurrent(aktuelleLedStromVorgabe);
      }
      else
      {
        // Vorzeichen bestimmt später die H-Brückenrichtung.
        gewuenschterStromStellwert = -1800.0f;
      }
    }
    break;

    // ------------------------------------------------------------------------
    // LED-AUTOEICHUNG
    // ------------------------------------------------------------------------
    case 2:
    {
      gewuenschterStromStellwert = 0.0f;

      // LED-Autoeichung muss auf den ADC2-Bruttowert regeln.
      // Der Dunkelwert darf hier NICHT abgezogen werden, sonst koennte
      // bei grossem Offset/Fremdlicht der ADC2 in die Saettigung laufen,
      // obwohl der Netto-Optikwert noch im Sollfenster liegt.
      unsigned long nowMs = millis();
      float optikAdcBrutto = (float)ads1263Adc2GetAvg();
      float fehler = optikAdcBrutto - OPTIK_ZIEL_ROHWERT;
      float absFehler = fabsf(fehler);

      if (absFehler > OPTIK_AUTOCAL_FEIN_FENSTER)
      {
        autoCalStableInit = false;

        if ((uint32_t)(nowMs - autoCalLastStepMs) >= OPTIK_AUTOCAL_STEP_PERIOD_MS)
        {
          autoCalLastStepMs = nowMs;

          float step = (absFehler > OPTIK_AUTOCAL_GROB_FENSTER) ?
                       OPTIK_AUTOCAL_STEP_GROB_MA :
                       OPTIK_AUTOCAL_STEP_FEIN_MA;

          if (optikAdcBrutto < OPTIK_ZIEL_ROHWERT)
          {
            aktuelleLedStromVorgabe += step;
          }
          else
          {
            aktuelleLedStromVorgabe -= step;
          }

          if (aktuelleLedStromVorgabe > OPTIK_LED_MAX_MA) aktuelleLedStromVorgabe = OPTIK_LED_MAX_MA;
          if (aktuelleLedStromVorgabe < OPTIK_LED_MIN_MA) aktuelleLedStromVorgabe = OPTIK_LED_MIN_MA;

          if (step >= 0.999f) autoCalCoarseSteps++; else autoCalFineSteps++;

          setTargetCurrent(aktuelleLedStromVorgabe);
        }
      }
      else
      {
        float noisePp = 0.0f;
        bool stabil = optikAutoCalStabilitaetUpdate(optikAdcBrutto, nowMs, &noisePp);

        if (stabil)
        {
          // Die Trockenreferenz dagegen bleibt dunkelstromkorrigiert.
          optikTrockenReferenz = optikReflexion;
          optikHealthAutoCalAbschluss(true, false, optikAdcBrutto, noisePp);

          ablaufStatus = 0;
          statusTimer = nowMs;

          integralFehler = 0.0f;
          letzterFehler = 0.0f;
        }
      }

      if (nowMs - statusTimer >= OPTIK_AUTOCAL_TIMEOUT_MS)
      {
        // Timeout: auch hier die Trockenreferenz aus dem Netto-Optikwert bilden,
        // aber die Optikqualitaet entsprechend als Auto-Cal/LED-Limit bewerten.
        optikTrockenReferenz = (optikReflexion > 1000.0f) ? optikReflexion : 15000.0f;
        optikHealthAutoCalAbschluss(false, true, optikAdcBrutto,
                                    autoCalStableInit ? (autoCalStableMax - autoCalStableMin) : 0.0f);

        ablaufStatus = 0;
        statusTimer = nowMs;

        integralFehler = 0.0f;
        letzterFehler = 0.0f;
      }
    }
    break;

    default:
    {
      ablaufStatus = 1;
      statusTimer = millis();

      integralFehler = 0.0f;
      letzterFehler = 0.0f;
      gewuenschterStromStellwert = 0.0f;
    }
    break;
  }

  // Ausgangswunsch begrenzen
  gewuenschterStromStellwert = clampf_local(gewuenschterStromStellwert,
                                            -maxPeltierStromMa(),
                                             maxPeltierStromMa());

  // --------------------------------------------------------------------------
  // H-BRÜCKEN-RICHTUNG BESTIMMEN
  // --------------------------------------------------------------------------

  if (gewuenschterStromStellwert > 15.0f)
  {
    gewuenschterModus = 1;
    peltierStromSoll = gewuenschterStromStellwert;
  }
  else if (gewuenschterStromStellwert < -15.0f)
  {
    gewuenschterModus = 2;
    peltierStromSoll = -gewuenschterStromStellwert;
  }
  else
  {
    gewuenschterModus = 0;
    peltierStromSoll = 0.0f;
  }

  peltierStromSoll = clampf_local(peltierStromSoll, 0.0f, maxPeltierStromMa());

  // --------------------------------------------------------------------------
  // RICHTUNGSWECHSEL MIT TOTZEIT
  // --------------------------------------------------------------------------

  unsigned long totzeit = holeHBrueckenTotzeitMs();

  if (gewuenschterModus != aktuellerModus &&
      aktuellerModus != 0 &&
      gewuenschterModus != 0)
  {
    peltierAus();
    umschaltTimer = millis();
    aktuellerModus = 0;
    return;
  }

  if (aktuellerModus == 0 &&
      gewuenschterModus != 0 &&
      (millis() - umschaltTimer < totzeit))
  {
    peltierAus();
    return;
  }

  if (gewuenschterModus == 1)
  {
    aktuellerModus = 1;
  }
  else if (gewuenschterModus == 2)
  {
    aktuellerModus = 2;
  }
  else
  {
    peltierAus();
    aktuellerModus = 0;
    return;
  }

  // --------------------------------------------------------------------------
  // INNERER STROMREGLER
  // --------------------------------------------------------------------------

  float istStromMA = fabsf((float)adcRawPeltierStrom);
  float stromFehler = peltierStromSoll - istStromMA;

  stromIntegral += stromFehler * dt;

  // Der Integrator selbst hat die Einheit mA*s. Begrenzen muessen wir daher
  // nicht den Integrator direkt auf PWM-Digits, sondern den daraus entstehenden
  // I-Anteil. Sonst kann der Stromregler bei ki_strom < 1.0 den PWM-Grenzwert
  // nie erreichen. Beispiel vorher: ki=0.2 und Integral=2700 ergaben nur
  // 540 PWM-Digits I-Anteil.
  float stromIntegralMax = (float)PELTIER_PWM_LIMIT;
  if (ki_strom > 0.0001f)
  {
    stromIntegralMax = (float)PELTIER_PWM_LIMIT / ki_strom;
  }
  stromIntegral = clampf_local(stromIntegral, 0.0f, stromIntegralMax);

  float pTerm = kp_strom * stromFehler;
  float iTerm = ki_strom * stromIntegral;
  iTerm = clampf_local(iTerm, 0.0f, (float)PELTIER_PWM_LIMIT);

  float berechnetePwm = pTerm + iTerm;

  // Spannungslimit: bei 12 V Versorgung auf ca. 8 V Peltier-Mittelspannung begrenzt.
  peltierSollWert = clampf_local(berechnetePwm, 0.0f, (float)PELTIER_PWM_LIMIT);
}


// ============================================================================
// 4. HAUPTSTEUERUNG
// ============================================================================

void verarbeiteRegelung()
{
  berechneSensorWerte();

  uint16_t intervall = holeRegelIntervallMs();
  unsigned long jetzt = millis();
  bool regelIntervallFaellig = (jetzt - letzteRegelZeit >= intervall);

  // Messwertaufbereitung und Datenausgabe laufen auch bei gelatchter Safety
  // weiter. Dadurch werden Pt100-Werte, Peltierstrom und die ADC-Ausgabe-
  // Sequenz weiterhin aktualisiert, waehrend die Leistungsregelung aus bleibt.
  if (regelIntervallFaellig)
  {
    letzteRegelZeit = jetzt;
    verarbeitePhysikalischeFormeln();
  }

  // Wenn die Sicherheitsueberwachung ausgeloest hat, darf kein alter PWM-Wert
  // erneut aktiv werden. PID, Stromregelung und PWM bleiben bis Reset aus.
  if (safetyIsFaultActive())
  {
    optikAbortAblaufBeiSafety(jetzt);
    peltierAus();
    aktuellerModus = 0;
    integralFehler = 0.0f;
    letzterFehler = 0.0f;
    return;
  }

  // Der normale PID-Regler bleibt am konfigurierten Regelintervall.
  // Die LED-Auto-Cal besitzt dagegen einen eigenen 50-ms-Schritttakt und darf
  // nicht vom deutlich langsameren Reglerintervall (z. B. 250 ms) ausgebremst
  // werden. Sonst sind innerhalb des 10-s-Timeouts nur etwa 40 Grobschritte
  // moeglich: Start 10 mA + 39 Schritte = 49 mA, exakt wie im Statusscreen.
  // Deshalb wird Status 2 in jeder Hauptloop bedient; die interne
  // OPTIK_AUTOCAL_STEP_PERIOD_MS-Sperre begrenzt die LED-Schritte weiterhin
  // auf maximal einen Schritt je 50 ms.
  bool autoCalAktiv = (ablaufStatus == 2);

  if (regelIntervallFaellig || autoCalAktiv)
  {
    // Waehrend des externen Ref-/Kanalabgleichs darf der Peltierregler nicht
    // auf die angeschlossenen Kalibrierwiderstaende reagieren.
    if (extRefCalIsActive())
    {
      peltierAus();
      aktuellerModus = 0;
      integralFehler = 0.0f;
      letzterFehler = 0.0f;
      return;
    }

    ausfuehrungRegelAlgorithmus();
  }

  // --------------------------------------------------------------------------
  // 12-BIT PWM-DITHERING, loop-synchroner Fractional-Accumulator
  // --------------------------------------------------------------------------

  if (millis() - lastDitherTime >= 1UL)
  {
    lastDitherTime = millis();

    if (!safetyPeltierAllowed())
    {
      peltierAus();
      aktuellerModus = 0;
      return;
    }

    if (aktuellerModus == 0 || peltierSollWert <= 0.0f)
    {
      resetPwmDither();
      drv8873PwmModeApply(0, 0);
      return;
    }

    if (pwmDitherLastModus != aktuellerModus)
    {
      resetPwmDither();
      pwmDitherLastModus = aktuellerModus;
    }

    // Dithering bleibt aktiv, aber nur innerhalb des gemessenen
    // Peltier-Spannungslimits. PELTIER_PWM_LIMIT = 2700 bleibt bewusst
    // unveraendert, weil dieser Wert am Geraet passend gemessen wurde.
    float soll = clampf_local(peltierSollWert, 0.0f, (float)PELTIER_PWM_LIMIT);

    int baseValue = (int)soll;
    float remainder = soll - (float)baseValue;

    int finalPwm = baseValue;

    if (remainder > 0.0f)
    {
      pwmDitherAccumulator += remainder;

      if (pwmDitherAccumulator >= 1.0f)
      {
        finalPwm += 1;
        pwmDitherAccumulator -= 1.0f;
      }
    }
    else
    {
      pwmDitherAccumulator = 0.0f;
    }

    if (finalPwm > PELTIER_PWM_LIMIT) finalPwm = PELTIER_PWM_LIMIT;
    if (finalPwm < 0) finalPwm = 0;

    drv8873PwmModeApply(aktuellerModus, finalPwm);
  }
}
