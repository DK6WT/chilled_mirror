/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: MCP3202_Treiber.ino
 * Zweck: Peltierstrommessung mit MCP3202 und Plausibilitaetspruefung.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

//
// SICHERE MCP3202-Peltierstrommessung für gemeinsamen SPI0 mit RA8875
// mit Online-/Plausibilitätsprüfung.
//
// Wichtig:
// Der MCP3202 ist ein SPI-ADC und hat KEINE Hardware-Adresse und KEIN ID-Register.
// Man kann ihn daher nicht wie einen I2C-Sensor per Adresse oder WHOAMI abfragen.
// Stattdessen wird hier plausibilisiert:
//
// - Modus 0 / AUS:
//     nur langsam, max. 1x pro Sekunde, beide Kanäle testweise lesen.
//     Erwartung: beide Kanäle nahe 0 mA.
//     Wenn beide plausibel sind -> mcp3202_online = true.
//     Wenn nicht -> mcp3202_online = false.
//
// - Modus 1 / Kühlen und Modus 2 / Heizen:
//     Im DRV8873-PWM-Mode können beide IPROPI-Kanäle Stromanteile sehen.
//     Deshalb werden CH0 und CH1 gelesen und zum Peltierstrom addiert.
//     Sättigung auf einem Kanal bleibt ungültig.
//
// Sicherheitsverhalten:
// - Im Modus AUS wird bei ungültigem MCP 0 mA gemeldet.
// - Im aktiven Modus wird bei ungültigem MCP 4095 mA gemeldet,
//   damit der Überstromschutz der Regelung sicher auslöst.
//
// MCP3202 Kommandos im 3-Byte-Schema:
//      CH0 single-ended = 0xA0
//      CH1 single-ended = 0xE0

#include <SPI.h>

// Externe Variablen aus dem Haupt-Tab
extern volatile int32_t adcRawPeltierStrom;
extern const byte pinCsDisable;      // Pin 39, CS für MCP3202

// 0 = Aus, 1 = Kühlen, 2 = Heizen
extern uint8_t aktuellerModus;
extern bool safetyIsFaultActive(void);

// RA8875 TFT-CS aus deiner Main.
// RA8875_CS = 10.
static const uint8_t TFT_CS_PIN = 10;

// Diagnosewerte.
// Bei Bedarf in anderen Tabs mit extern sichtbar machen.
volatile int32_t adcRawPeltierStromKuehlen = 0;
volatile int32_t adcRawPeltierStromHeizen  = 0;
volatile int32_t adcRawPeltierStromMax     = 0;

// Online-/Fehlerstatus
bool mcp3202_online = false;
bool mcp3202_fehler = false;
uint8_t mcp3202FehlerZaehler = 0;
uint8_t mcp3202OkZaehler = 0;

// Boot-Schutz
bool mcp3202Bereit = false;
unsigned long mcpBootTimer = 0;

// Selbsttest nur langsam im AUS-Modus
unsigned long letzterMcpSelbsttest = 0;

// SPI-Einstellung.
// 2 MHz ist konservativ.
static const SPISettings mcp3202SpiSettings(2000000, MSBFIRST, SPI_MODE0);

// Plausibilitätsgrenzen.
// Vor der Kalibrierung gilt näherungsweise: 1 Digit = 1 mA.
static const uint16_t MCP_ZERO_MAX_MA      = 200;   // Leerlauf / Aus-Zustand
static const uint16_t MCP_SATURATION_RAW   = 4088;  // nahe ADC-Vollaussteuerung
static const uint16_t MCP_FAULT_VALUE      = 4095;  // Fehlerwert für Not-Aus


// =========================================================================
// PELTIER-STROMKALIBRIERUNG
// =========================================================================
// Tabelle ist absichtlich hier gut editierbar gehalten.
//
// raw_mA  = ungekalibrierter Wert, den das Gerät bisher angezeigt hat
//           also Summe aus IPROPI/MCP3202 in mA-Digits.
// real_mA = echter Strom, gemessen mit Multimeter / elektronischer Last.
//
// Regeln:
// - Werte müssen nach raw_mA aufsteigend sortiert sein.
// - Zwischen zwei Punkten wird linear interpoliert.
// - Unter PELTIER_CURRENT_DEADZONE_MA wird 0 mA ausgegeben.
// - Über dem letzten Punkt wird mit dem Faktor des letzten Punkts weitergerechnet.
//
// Messreihe 2026-06-10, alle Punkte bei festem PWM-Limit 2700 Digit gemessen.

struct PeltierCurrentCalPoint
{
  float raw_mA;
  float real_mA;
};

static const float PELTIER_CURRENT_DEADZONE_MA = 80.0f;

static const PeltierCurrentCalPoint peltierCurrentCalTable[] =
{
  // raw_mA, real_mA
  {   0.0f,    0.0f },
  { 129.0f,  100.0f },
  { 159.0f,  148.0f },
  { 197.0f,  202.0f },
  { 240.0f,  251.0f },
  { 512.0f,  530.0f },
  { 747.0f,  770.0f },
  { 977.0f, 1007.0f },
  { 990.0f, 1020.0f },
  {1466.0f, 1504.0f },
  {1950.0f, 2000.0f },
  {2055.0f, 2108.0f }
};

static const uint8_t PELTIER_CURRENT_CAL_COUNT =
  sizeof(peltierCurrentCalTable) / sizeof(peltierCurrentCalTable[0]);

// Diagnose: ungekalibrierte Summe vor Tabellenkorrektur.
// Wird aktuell nicht im Menü angezeigt, ist aber beim Debuggen sichtbar.
volatile int32_t adcRawPeltierStromUnkalibriert = 0;

static float kalibrierePeltierStrom(float raw_mA)
{
  if (raw_mA <= 0.0f)
  {
    return 0.0f;
  }

  if (raw_mA < PELTIER_CURRENT_DEADZONE_MA)
  {
    return 0.0f;
  }

  // Unterhalb des ersten echten Messpunkts von 0 bis Punkt 1 interpolieren.
  // Dadurch wird der unstabile 50-mA-Bereich nicht hochgezogen.
  for (uint8_t i = 1; i < PELTIER_CURRENT_CAL_COUNT; i++)
  {
    float x0 = peltierCurrentCalTable[i - 1].raw_mA;
    float x1 = peltierCurrentCalTable[i].raw_mA;

    if (raw_mA <= x1)
    {
      float y0 = peltierCurrentCalTable[i - 1].real_mA;
      float y1 = peltierCurrentCalTable[i].real_mA;

      if (x1 <= x0)
      {
        return y1;
      }

      float t = (raw_mA - x0) / (x1 - x0);
      return y0 + t * (y1 - y0);
    }
  }

  // Oberhalb des letzten Punktes mit dessen Faktor weiterrechnen.
  float lastRaw  = peltierCurrentCalTable[PELTIER_CURRENT_CAL_COUNT - 1].raw_mA;
  float lastReal = peltierCurrentCalTable[PELTIER_CURRENT_CAL_COUNT - 1].real_mA;

  if (lastRaw <= 1.0f)
  {
    return raw_mA;
  }

  return raw_mA * (lastReal / lastRaw);
}

static int32_t kalibrierePeltierStromInt(int32_t raw_mA)
{
  float korrigiert = kalibrierePeltierStrom((float)raw_mA);

  if (korrigiert < 0.0f)
  {
    korrigiert = 0.0f;
  }

  if (korrigiert > (float)MCP_FAULT_VALUE)
  {
    korrigiert = (float)MCP_FAULT_VALUE;
  }

  return (int32_t)(korrigiert + 0.5f);
}


// =========================================================================
// STATUS HELFER
// =========================================================================
static void mcpMarkiereOk()
{
  if (mcp3202OkZaehler < 10) mcp3202OkZaehler++;
  if (mcp3202FehlerZaehler > 0) mcp3202FehlerZaehler--;

  // Nach zwei guten Messungen online setzen
  if (mcp3202OkZaehler >= 2)
  {
    mcp3202_online = true;
    mcp3202_fehler = false;
  }
}

static void mcpMarkiereFehler()
{
  if (mcp3202FehlerZaehler < 10) mcp3202FehlerZaehler++;
  mcp3202OkZaehler = 0;

  // Nach zwei schlechten Messungen offline setzen
  if (mcp3202FehlerZaehler >= 2)
  {
    mcp3202_online = false;
    mcp3202_fehler = true;
  }
}


// =========================================================================
// MCP3202 KANAL LESEN
// =========================================================================
static uint16_t leseMCP3202KanalSicher(uint8_t kanal)
{
  uint8_t commandByte = (kanal == 0) ? 0xA0 : 0xE0;

  // Wichtig bei gemeinsamem SPI mit RA8875:
  // TFT sicher abwählen, bevor MCP-CS LOW wird.
  digitalWriteFast(TFT_CS_PIN, HIGH);
  delayMicroseconds(2);

  digitalWriteFast(pinCsDisable, LOW);
  delayMicroseconds(2);

  SPI.transfer(0x01);
  uint8_t highByte = SPI.transfer(commandByte);
  uint8_t lowByte  = SPI.transfer(0x00);

  delayMicroseconds(2);
  digitalWriteFast(pinCsDisable, HIGH);

  uint16_t raw = ((uint16_t)(highByte & 0x0F) << 8) | lowByte;

  if (raw > 4095) raw = 4095;

  return raw;
}


// =========================================================================
// BEIDE MCP-KANÄLE LESEN
// =========================================================================
static void leseMCP3202BeideKanaele(uint16_t* rawKuehlen, uint16_t* rawHeizen)
{
  SPI.beginTransaction(mcp3202SpiSettings);

  *rawKuehlen = leseMCP3202KanalSicher(0);  // CH0
  *rawHeizen  = leseMCP3202KanalSicher(1);  // CH1

  SPI.endTransaction();

  adcRawPeltierStromKuehlen = (int32_t)(*rawKuehlen);
  adcRawPeltierStromHeizen  = (int32_t)(*rawHeizen);

  if (*rawKuehlen >= *rawHeizen)
    adcRawPeltierStromMax = (int32_t)(*rawKuehlen);
  else
    adcRawPeltierStromMax = (int32_t)(*rawHeizen);
}


// =========================================================================
// PLAUSIBILITÄT
// =========================================================================
static bool mcpPlausibelAus(uint16_t rawKuehlen, uint16_t rawHeizen)
{
  // Im Aus-Zustand müssen beide Stromkanäle nahe Null sein.
  if (rawKuehlen >= MCP_SATURATION_RAW) return false;
  if (rawHeizen  >= MCP_SATURATION_RAW) return false;

  if (rawKuehlen > MCP_ZERO_MAX_MA) return false;
  if (rawHeizen  > MCP_ZERO_MAX_MA) return false;

  return true;
}

static bool mcpPlausibelAktiv(uint8_t modus, uint16_t rawKuehlen, uint16_t rawHeizen)
{
  // Im DRV8873-PWM-Mode sind CH0 und CH1 keine exklusiven Richtungswerte mehr.
  // Durch HIGH/HIGH-Freilauf und LC-Filter können beide IPROPI-Kanäle Stromanteile sehen.
  // Deshalb wird aktiv nur noch auf Sättigung geprüft; die Summe wird anschließend
  // als Peltierstrom verwendet.
  if (modus != 1 && modus != 2) return false;

  if (rawKuehlen >= MCP_SATURATION_RAW) return false;
  if (rawHeizen  >= MCP_SATURATION_RAW) return false;

  return true;
}


// =========================================================================
// EINMALIGE PIN-ABSICHERUNG
// =========================================================================
static void initialisiereMcpPinsEinmal()
{
  static bool einmalInitialisiert = false;

  if (!einmalInitialisiert)
  {
    einmalInitialisiert = true;

    pinMode(pinCsDisable, OUTPUT);
    digitalWriteFast(pinCsDisable, HIGH);

    pinMode(TFT_CS_PIN, OUTPUT);
    digitalWriteFast(TFT_CS_PIN, HIGH);

    // Wichtig:
    // Kein SPI.begin() hier.
    // Der SPI-Bus wurde bereits vom RA8875/TFT initialisiert.
  }
}


// =========================================================================
// STROM-ADC AUSLESEN
// =========================================================================
void lesePeltierStrom()
{
  initialisiereMcpPinsEinmal();

  // -------------------------------------------------------------------------
  // Boot-Schutz: erst nach 2,5 s grundsätzlich freigeben
  // -------------------------------------------------------------------------
  if (!mcp3202Bereit)
  {
    if (mcpBootTimer == 0)
    {
      mcpBootTimer = millis();
    }

    if (millis() - mcpBootTimer < 2500UL)
    {
      adcRawPeltierStrom = 0;
      adcRawPeltierStromUnkalibriert = 0;
      adcRawPeltierStromKuehlen = 0;
      adcRawPeltierStromHeizen  = 0;
      adcRawPeltierStromMax     = 0;
      mcp3202_online = false;
      mcp3202_fehler = false;
      return;
    }

    mcp3202Bereit = true;
  }

  // -------------------------------------------------------------------------
  // SAFETY AKTIV:
  // Auch nach einer gelatchten Abschaltung beide IPROPI-Kanaele weiter
  // auslesen. So zeigen TFT, Web und Log den tatsaechlich gemessenen Strom
  // und nicht den letzten Wert vor der Safety-Ausloesung.
  // -------------------------------------------------------------------------
  if (safetyIsFaultActive())
  {
    uint16_t rawKuehlen = 0;
    uint16_t rawHeizen  = 0;

    leseMCP3202BeideKanaele(&rawKuehlen, &rawHeizen);

    if (rawKuehlen >= MCP_SATURATION_RAW || rawHeizen >= MCP_SATURATION_RAW)
    {
      mcpMarkiereFehler();
      adcRawPeltierStrom = MCP_FAULT_VALUE;
      adcRawPeltierStromUnkalibriert = MCP_FAULT_VALUE;
      return;
    }

    mcpMarkiereOk();

    int32_t rawSumme = (int32_t)rawKuehlen + (int32_t)rawHeizen;
    adcRawPeltierStromUnkalibriert = rawSumme;
    adcRawPeltierStrom = kalibrierePeltierStromInt(rawSumme);
    return;
  }

  uint8_t modus = aktuellerModus;

  // -------------------------------------------------------------------------
  // MODUS AUS:
  // Nur langsam einen Selbsttest machen, sonst SPI-Bus in Ruhe lassen.
  // -------------------------------------------------------------------------
  if (modus == 0)
  {
    adcRawPeltierStrom = 0;
    adcRawPeltierStromUnkalibriert = 0;

    if (millis() - letzterMcpSelbsttest >= 1000UL)
    {
      letzterMcpSelbsttest = millis();

      uint16_t rawKuehlen = 0;
      uint16_t rawHeizen  = 0;

      leseMCP3202BeideKanaele(&rawKuehlen, &rawHeizen);

      if (mcpPlausibelAus(rawKuehlen, rawHeizen))
      {
        mcpMarkiereOk();
      }
      else
      {
        mcpMarkiereFehler();
      }
    }

    return;
  }

  // -------------------------------------------------------------------------
  // AKTIVER MODUS:
  // Beide Kanäle lesen, damit der inaktive Kanal als Plausibilitätscheck dient.
  // -------------------------------------------------------------------------
  uint16_t rawKuehlen = 0;
  uint16_t rawHeizen  = 0;

  leseMCP3202BeideKanaele(&rawKuehlen, &rawHeizen);

  bool plausibel = mcpPlausibelAktiv(modus, rawKuehlen, rawHeizen);

  if (!plausibel)
  {
    mcpMarkiereFehler();

    // Sicherheitsverhalten:
    // Im aktiven Betrieb absichtlich Fehlerwert melden.
    // Damit löst der Überstromschutz in der Regelung aus.
    adcRawPeltierStrom = MCP_FAULT_VALUE;
    adcRawPeltierStromUnkalibriert = MCP_FAULT_VALUE;
    return;
  }

  mcpMarkiereOk();

  if (modus == 1 || modus == 2)
  {
    // DRV8873-PWM-Mode:
    // IPROPI1/IPROPI2 spiegeln die High-Side-Stromanteile.
    // Bei unserem LOW-aktiven PWM gegen HIGH und LC-Filter kann der Strom auf
    // beide Kanäle verteilt erscheinen. Darum für den Regler beide addieren.
    int32_t rawSumme = (int32_t)rawKuehlen + (int32_t)rawHeizen;

    adcRawPeltierStromUnkalibriert = rawSumme;
    adcRawPeltierStrom = kalibrierePeltierStromInt(rawSumme);
  }
  else
  {
    adcRawPeltierStrom = 0;
    adcRawPeltierStromUnkalibriert = 0;
  }
}
