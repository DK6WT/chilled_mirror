/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPalarm.ino
 * Zweck: Alarmueberwachung fuer Taupunkt und relative Feuchte.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
// TPalarm.ino
// Alarmueberwachung fuer Taupunkt / relative Feuchte mit quittierbarem Buzzer
// =========================================================================

#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include <string.h>
#include <RA8875.h>
#include "TP_T.h"
#include "TPlanguage.h"
#include "TPtft.h"

#define ALARM_MODE_OFF    0
#define ALARM_MODE_LIMIT  1   // oberer Grenzwert: Taupunkt max ODER rF max
#define ALARM_MODE_RANGE  2   // Bereich: Taupunkt/rF ausserhalb min..max

#define ALARM_CFG_MAGIC   0x54414C31UL   // 'TAL1'
#define ALARM_CFG_VERSION 1

// Muss zur gelben STABILISIERT-Phase im Hauptdisplay passen.
// Erst danach ist der Benutzer-Grenzwertalarm scharf.
#define ALARM_STABLE_ARM_DELAY_MS 30000UL

#define ALARM_SOURCE_SAFETY     0x01
#define ALARM_SOURCE_GRENZWERT  0x02

extern RA8875 tft;
extern const byte pinAlarm;
extern const byte pinRelay;
extern float präziserTaupunkt;
extern float relativeFeuchte;
extern float tempSpiegel;
extern float tempUmgebung;
extern uint8_t ablaufStatus;
extern unsigned long statusTimer;
extern bool ads1263_bereit;
extern bool mcp3202_fehler;
extern bool safetyIsFaultActive(void);

// EEPROM V11: Alarmkonfiguration mit A/B-Slots.
#define ALARM_CFG_SLOT_MAGIC   0x54414C41UL   // 'TALA'
#define ALARM_CFG_SLOT_VERSION 1U
#define ALARM_CFG_SLOT_A_ADDR  768
#define ALARM_CFG_SLOT_B_ADDR  896
#define ALARM_CFG_SLOT_SIZE    128

typedef struct {
  uint32_t magic;
  uint16_t version;

  uint8_t  mode;             // 0=Aus, 1=Grenzwert, 2=Bereich
  uint8_t  buzzer_enabled;   // 0=aus, 1=ein

  int16_t  tau_low10;        // Taupunkt min, 0.1 C
  int16_t  tau_high10;       // Taupunkt max, 0.1 C
  uint16_t rh_low10;         // rF min, 0.1 %
  uint16_t rh_high10;        // rF max, 0.1 %

  uint16_t buzzer_pwm;       // 12-bit PWM, Default 1800
  uint32_t crc;
} alarm_config_t;

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  alarm_config_t payload;
  uint32_t crc;
} alarm_config_slot_t;

static_assert(sizeof(alarm_config_slot_t) <= ALARM_CFG_SLOT_SIZE,
              "Alarm EEPROM slot too small");

static alarm_config_t alarm_cfg;
static bool alarm_cfg_loaded = false;
static bool alarm_active = false;
static bool alarm_buzzer_ack = false;
static bool alarm_buzzer_output_on = false;
static uint32_t alarm_blink_ms = 0;
static uint32_t alarm_last_task_ms = 0;
static uint8_t alarm_buzzer_sources_prev = 0;

static bool alarm_indicator_drawn = false;
static bool alarm_indicator_ack_cache = false;
static uint32_t alarm_cfg_sequence = 0UL;
static uint8_t alarm_cfg_active_slot = 0U;

static int FLASHMEM alarmConfigSlotAddr(uint8_t slot)
{
  return slot ? ALARM_CFG_SLOT_B_ADDR : ALARM_CFG_SLOT_A_ADDR;
}

static uint32_t FLASHMEM alarmConfigCrcBytes(const void* data, size_t n)
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

static bool FLASHMEM alarmConfigPayloadValid(const alarm_config_t& cfg)
{
  if (cfg.magic != ALARM_CFG_MAGIC) return false;
  if (cfg.version != ALARM_CFG_VERSION) return false;
  const uint32_t crc = alarmConfigCrcBytes(&cfg, sizeof(alarm_config_t) - sizeof(uint32_t));
  return (cfg.crc == crc);
}

static bool FLASHMEM alarmConfigReadSlot(uint8_t slot, alarm_config_slot_t& out)
{
  EEPROM.get(alarmConfigSlotAddr(slot), out);
  if (out.magic != ALARM_CFG_SLOT_MAGIC) return false;
  if (out.version != ALARM_CFG_SLOT_VERSION) return false;
  if (out.size != sizeof(alarm_config_t)) return false;
  const uint32_t slotCrc = alarmConfigCrcBytes(&out, sizeof(out) - sizeof(out.crc));
  if (slotCrc != out.crc) return false;
  return alarmConfigPayloadValid(out.payload);
}

static uint32_t FLASHMEM alarmConfigCrc(void)
{
  return alarmConfigCrcBytes(&alarm_cfg, sizeof(alarm_config_t) - sizeof(uint32_t));
}

static void FLASHMEM alarmConfigSetDefaults(void)
{
  memset(&alarm_cfg, 0, sizeof(alarm_cfg));

  alarm_cfg.magic = ALARM_CFG_MAGIC;
  alarm_cfg.version = ALARM_CFG_VERSION;
  alarm_cfg.mode = ALARM_MODE_OFF;
  alarm_cfg.buzzer_enabled = 1;

  // Default ist bewusst AUS. Die Grenzwerte sind nur Startwerte,
  // falls der Alarm spaeter aktiviert wird.
  alarm_cfg.tau_low10 = -800;    // -80.0 C
  alarm_cfg.tau_high10 = 200;    // +20.0 C
  alarm_cfg.rh_low10 = 0;        //   0.0 %
  alarm_cfg.rh_high10 = 800;     //  80.0 %

  alarm_cfg.buzzer_pwm = 1800;
  alarm_cfg.crc = alarmConfigCrc();
}

static void FLASHMEM alarmConfigSanitize(void)
{
  bool changed = false;

  if (alarm_cfg.mode > ALARM_MODE_RANGE)
  {
    alarm_cfg.mode = ALARM_MODE_OFF;
    changed = true;
  }

  if (alarm_cfg.buzzer_enabled > 1)
  {
    alarm_cfg.buzzer_enabled = 1;
    changed = true;
  }

  if (alarm_cfg.tau_low10 < -800) { alarm_cfg.tau_low10 = -800; changed = true; }
  if (alarm_cfg.tau_low10 >  800) { alarm_cfg.tau_low10 =  800; changed = true; }
  if (alarm_cfg.tau_high10 < -800) { alarm_cfg.tau_high10 = -800; changed = true; }
  if (alarm_cfg.tau_high10 >  800) { alarm_cfg.tau_high10 =  800; changed = true; }

  if (alarm_cfg.tau_low10 > alarm_cfg.tau_high10)
  {
    alarm_cfg.tau_low10 = alarm_cfg.tau_high10;
    changed = true;
  }

  if (alarm_cfg.rh_low10 > 1000) { alarm_cfg.rh_low10 = 0; changed = true; }
  if (alarm_cfg.rh_high10 > 1000) { alarm_cfg.rh_high10 = 1000; changed = true; }

  if (alarm_cfg.rh_low10 > alarm_cfg.rh_high10)
  {
    alarm_cfg.rh_low10 = alarm_cfg.rh_high10;
    changed = true;
  }

  if (alarm_cfg.buzzer_pwm < 300 || alarm_cfg.buzzer_pwm > 4095)
  {
    alarm_cfg.buzzer_pwm = 1800;
    changed = true;
  }

  if (changed)
  {
    alarm_cfg.crc = alarmConfigCrc();
  }
}

void FLASHMEM alarmConfigSave(void)
{
  alarm_cfg.magic = ALARM_CFG_MAGIC;
  alarm_cfg.version = ALARM_CFG_VERSION;
  alarmConfigSanitize();
  alarm_cfg.crc = alarmConfigCrc();

  alarm_config_slot_t slotA;
  alarm_config_slot_t slotB;
  const bool validA = alarmConfigReadSlot(0, slotA);
  const bool validB = alarmConfigReadSlot(1, slotB);

  uint32_t nextSeq = alarm_cfg_sequence;
  if (validA && slotA.sequence > nextSeq) nextSeq = slotA.sequence;
  if (validB && slotB.sequence > nextSeq) nextSeq = slotB.sequence;
  nextSeq++;
  if (nextSeq == 0UL) nextSeq = 1UL;

  uint8_t targetSlot;
  if (!validA) targetSlot = 0;
  else if (!validB) targetSlot = 1;
  else targetSlot = (slotA.sequence <= slotB.sequence) ? 0 : 1;

  alarm_config_slot_t out;
  memset(&out, 0, sizeof(out));
  out.magic = ALARM_CFG_SLOT_MAGIC;
  out.version = ALARM_CFG_SLOT_VERSION;
  out.size = sizeof(alarm_config_t);
  out.sequence = nextSeq;
  out.payload = alarm_cfg;
  out.crc = alarmConfigCrcBytes(&out, sizeof(out) - sizeof(out.crc));
  EEPROM.put(alarmConfigSlotAddr(targetSlot), out);

  alarm_cfg_sequence = nextSeq;
  alarm_cfg_active_slot = targetSlot;
}

void FLASHMEM alarmConfigLoad(void)
{
  alarm_config_slot_t slotA;
  alarm_config_slot_t slotB;
  const bool validA = alarmConfigReadSlot(0, slotA);
  const bool validB = alarmConfigReadSlot(1, slotB);

  if (!validA && !validB)
  {
    alarm_cfg_sequence = 0UL;
    alarm_cfg_active_slot = 0U;
    alarmConfigSetDefaults();
    // Erstinitialisierung: beide A/B-Slots direkt anlegen.
    alarmConfigSave();
    alarmConfigSave();
  }
  else
  {
    const alarm_config_slot_t* best = nullptr;
    if (validA && (!validB || slotA.sequence >= slotB.sequence))
    {
      best = &slotA;
      alarm_cfg_active_slot = 0U;
    }
    else
    {
      best = &slotB;
      alarm_cfg_active_slot = 1U;
    }
    alarm_cfg = best->payload;
    alarm_cfg_sequence = best->sequence;

    alarm_config_t before = alarm_cfg;
    alarmConfigSanitize();
    alarm_cfg.magic = ALARM_CFG_MAGIC;
    alarm_cfg.version = ALARM_CFG_VERSION;
    alarm_cfg.crc = alarmConfigCrc();
    if (memcmp(&before, &alarm_cfg, sizeof(alarm_cfg)) != 0 || validA != validB)
    {
      // A/B-Self-Heal: Wenn nur ein Slot gueltig ist, den defekten/leeren
      // Gegenslot sofort aus dem geladenen Alarmblock wiederherstellen.
      alarmConfigSave();
    }
  }

  alarm_cfg_loaded = true;
  alarm_active = false;
  alarm_buzzer_ack = false;
  alarm_buzzer_sources_prev = 0;
  analogWrite(pinAlarm, 0);
  digitalWrite(pinRelay, safetyIsFaultActive() ? HIGH : LOW);
}


// ============================================================================
// OEFFENTLICH: SNAPSHOT FUER GERAETE-EINSTELLUNGEN
// ============================================================================
// Wird vom Menue "Geraete-Speicher" benutzt, um die Alarm-/Grenzwert-
// einstellungen als Teil der normalen Geraeteeinstellungen zu sichern.

bool FLASHMEM alarmConfigExport(uint8_t* dst, uint16_t dstSize, uint16_t* usedSize)
{
  if (!alarm_cfg_loaded) alarmConfigLoad();

  if (usedSize) *usedSize = (uint16_t)sizeof(alarm_config_t);
  if (dst == nullptr) return false;
  if (dstSize < sizeof(alarm_config_t)) return false;

  alarmConfigSanitize();
  alarm_cfg.magic = ALARM_CFG_MAGIC;
  alarm_cfg.version = ALARM_CFG_VERSION;
  alarm_cfg.crc = alarmConfigCrc();

  memcpy(dst, &alarm_cfg, sizeof(alarm_config_t));
  return true;
}

bool FLASHMEM alarmConfigImport(const uint8_t* src, uint16_t srcSize)
{
  if (src == nullptr) return false;
  if (srcSize != sizeof(alarm_config_t)) return false;

  alarm_config_t tmp;
  memcpy(&tmp, src, sizeof(tmp));

  const uint32_t tmpCrc = alarmConfigCrcBytes(&tmp, sizeof(tmp) - sizeof(tmp.crc));
  if (tmp.magic != ALARM_CFG_MAGIC) return false;
  if (tmp.version != ALARM_CFG_VERSION) return false;
  if (tmp.crc != tmpCrc) return false;

  alarm_cfg = tmp;
  alarmConfigSave();
  alarmConfigLoad();

  return true;
}


uint8_t alarmGetMode(void)
{
  return alarm_cfg.mode;
}

void FLASHMEM alarmSetMode(uint8_t mode)
{
  if (mode > ALARM_MODE_RANGE) mode = ALARM_MODE_OFF;
  alarm_cfg.mode = mode;
}

bool alarmBuzzerEnabled(void)
{
  return alarm_cfg.buzzer_enabled != 0;
}

void FLASHMEM alarmSetBuzzerEnabled(bool enabled)
{
  alarm_cfg.buzzer_enabled = enabled ? 1 : 0;
}

int16_t alarmGetTauLow10(void)
{
  return alarm_cfg.tau_low10;
}

int16_t alarmGetTauHigh10(void)
{
  return alarm_cfg.tau_high10;
}

uint16_t alarmGetRhLow10(void)
{
  return alarm_cfg.rh_low10;
}

uint16_t alarmGetRhHigh10(void)
{
  return alarm_cfg.rh_high10;
}

void FLASHMEM alarmSetTauLow10(int16_t value10)
{
  if (value10 < -800) value10 = -800;
  if (value10 >  800) value10 =  800;
  alarm_cfg.tau_low10 = value10;
  if (alarm_cfg.tau_low10 > alarm_cfg.tau_high10)
  {
    alarm_cfg.tau_high10 = alarm_cfg.tau_low10;
  }
}

void FLASHMEM alarmSetTauHigh10(int16_t value10)
{
  if (value10 < -800) value10 = -800;
  if (value10 >  800) value10 =  800;
  alarm_cfg.tau_high10 = value10;
  if (alarm_cfg.tau_high10 < alarm_cfg.tau_low10)
  {
    alarm_cfg.tau_low10 = alarm_cfg.tau_high10;
  }
}

void FLASHMEM alarmSetRhLow10(uint16_t value10)
{
  if (value10 > 1000) value10 = 1000;
  alarm_cfg.rh_low10 = value10;
  if (alarm_cfg.rh_low10 > alarm_cfg.rh_high10)
  {
    alarm_cfg.rh_high10 = alarm_cfg.rh_low10;
  }
}

void FLASHMEM alarmSetRhHigh10(uint16_t value10)
{
  if (value10 > 1000) value10 = 1000;
  alarm_cfg.rh_high10 = value10;
  if (alarm_cfg.rh_high10 < alarm_cfg.rh_low10)
  {
    alarm_cfg.rh_low10 = alarm_cfg.rh_high10;
  }
}

static bool alarmMeasurementValuesValid(void)
{
  return isfinite(relativeFeuchte) && isfinite(präziserTaupunkt) &&
         isfinite(tempSpiegel) && isfinite(tempUmgebung) &&
         ads1263_bereit && !mcp3202_fehler;
}

bool alarmMeasurementStableForLimit(void)
{
  if (safetyIsFaultActive()) return false;
  if (!alarmMeasurementValuesValid()) return false;
  if (ablaufStatus != 0) return false;

  return ((uint32_t)(millis() - statusTimer) >= ALARM_STABLE_ARM_DELAY_MS);
}

bool alarmVisualActive(void)
{
  // Nur der Benutzer-Grenzwert-/Bereichsalarm. Safety wird separat abgefragt,
  // damit die Anzeige zwischen GRENZWERT und SAFETY-STOP unterscheiden kann.
  return alarm_active;
}

bool alarmAnyActive(void)
{
  return alarm_active || safetyIsFaultActive();
}

static uint8_t alarmCurrentSources(void)
{
  uint8_t sources = 0;
  if (safetyIsFaultActive()) sources |= ALARM_SOURCE_SAFETY;
  if (alarm_active)         sources |= ALARM_SOURCE_GRENZWERT;
  return sources;
}

static void alarmUpdateRelayOutput(void)
{
  // Das Alarmrelais ist nicht quittierbar. Es folgt dem echten Alarmzustand:
  // Benutzer-Grenzwertalarm ODER Safety-Stop.
  digitalWrite(pinRelay, alarmCurrentSources() ? HIGH : LOW);
}

bool alarmBuzzerAcknowledged(void)
{
  return alarm_buzzer_ack;
}

void alarmResetAcknowledge(void)
{
  alarm_buzzer_ack = false;
}

void alarmAcknowledge(void)
{
  if (alarmCurrentSources())
  {
    alarm_buzzer_ack = true;
    analogWrite(pinAlarm, 0);
    alarm_buzzer_output_on = false;
  }
}

extern bool mainDisplayFrameHit(uint16_t x, uint16_t y);

bool FLASHMEM alarmTouchAcknowledge(uint16_t x, uint16_t y)
{
  // Quittierung ueber den aktuell sichtbaren Messwert-/Chart-Rahmen oder
  // den Statuszeilen-Eintrag vor "Safety". Die optische Alarmanzeige
  // bleibt rot, nur der Buzzer wird stummgeschaltet.
  bool hitMainFrame  = mainDisplayFrameHit(x, y);
  bool hitStatusLine = (x >= 268 && x <= 380 && y >= 455 && y <= 479);

  // Nur eine noch nicht quittierte und tatsaechlich aktivierte
  // Buzzer-Meldung verbraucht die Beruehrung. Nach der Quittierung darf ein
  // weiterer, neuer Tipp auf den Rahmen wieder die Hauptansicht umschalten,
  // auch wenn der optische Alarmzustand weiterhin anliegt.
  if (alarmCurrentSources() &&
      alarmBuzzerEnabled() &&
      !alarm_buzzer_ack &&
      (hitMainFrame || hitStatusLine))
  {
    alarmAcknowledge();
    return true;
  }

  return false;
}


static bool alarmEvaluateLimit(float tauC, float rhPct, bool active_now)
{
  const float tauHigh = (float)alarm_cfg.tau_high10 / 10.0f;
  const float rhHigh  = (float)alarm_cfg.rh_high10 / 10.0f;

  // Kleine Rueckfall-Hysterese gegen Flattern am Grenzwert.
  const float tauHys = active_now ? 0.2f : 0.0f;
  const float rhHys  = active_now ? 0.5f : 0.0f;

  bool tauAlarm = isfinite(tauC) && (tauC > (tauHigh - tauHys));
  bool rhAlarm  = isfinite(rhPct) && (rhPct > (rhHigh - rhHys));

  return tauAlarm || rhAlarm;
}

static bool alarmEvaluateRange(float tauC, float rhPct, bool active_now)
{
  const float tauLow  = (float)alarm_cfg.tau_low10 / 10.0f;
  const float tauHigh = (float)alarm_cfg.tau_high10 / 10.0f;
  const float rhLow   = (float)alarm_cfg.rh_low10 / 10.0f;
  const float rhHigh  = (float)alarm_cfg.rh_high10 / 10.0f;

  const float tauHys = active_now ? 0.2f : 0.0f;
  const float rhHys  = active_now ? 0.5f : 0.0f;

  bool tauLowAlarm  = isfinite(tauC)  && (tauC  < (tauLow  + tauHys));
  bool tauHighAlarm = isfinite(tauC)  && (tauC  > (tauHigh - tauHys));
  bool rhLowAlarm   = isfinite(rhPct) && (rhPct < (rhLow   + rhHys));
  bool rhHighAlarm  = isfinite(rhPct) && (rhPct > (rhHigh  - rhHys));

  return tauLowAlarm || tauHighAlarm || rhLowAlarm || rhHighAlarm;
}

static bool alarmEvaluateRawGrenzwert(void)
{
  if (!alarm_cfg_loaded) return false;
  if (alarm_cfg.mode == ALARM_MODE_OFF) return false;

  if (alarm_cfg.mode == ALARM_MODE_LIMIT)
  {
    return alarmEvaluateLimit(präziserTaupunkt, relativeFeuchte, alarm_active);
  }

  if (alarm_cfg.mode == ALARM_MODE_RANGE)
  {
    return alarmEvaluateRange(präziserTaupunkt, relativeFeuchte, alarm_active);
  }

  return false;
}

static bool alarmEvaluate(void)
{
  // Modus AUS ist eine bewusste Bedienentscheidung und loescht den
  // Benutzeralarm sofort. Safety ist davon unabhaengig.
  if (!alarm_cfg_loaded) return false;
  if (alarm_cfg.mode == ALARM_MODE_OFF) return false;

  const bool stableForAlarm = alarmMeasurementStableForLimit();
  const bool rawGrenzwertAlarm = alarmEvaluateRawGrenzwert();

  if (!alarm_active)
  {
    // Neuer Benutzeralarm nur bei stabiler, gueltiger Messung.
    return stableForAlarm && rawGrenzwertAlarm;
  }

  // Ein aktiver Benutzeralarm wird NICHT durch Freiheizen, Auto-Cal,
  // ungueltige Werte oder die gelbe Stabilisierungsphase geloescht.
  // Er faellt erst ab, wenn ein stabiler/gueltiger Messwert wieder im
  // erlaubten Bereich liegt.
  if (!stableForAlarm)
  {
    return true;
  }

  return rawGrenzwertAlarm;
}

void alarmTask(void)
{
  if (!alarm_cfg_loaded)
  {
    alarm_active = false;
    alarm_buzzer_ack = false;
    alarm_buzzer_sources_prev = 0;
    analogWrite(pinAlarm, 0);
    alarm_buzzer_output_on = false;
    alarmUpdateRelayOutput();
    return;
  }

  uint32_t now = millis();
  if ((uint32_t)(now - alarm_last_task_ms) < 50UL)
  {
    return;
  }
  alarm_last_task_ms = now;

  bool new_active = alarmEvaluate();

  if (new_active != alarm_active)
  {
    alarm_active = new_active;
    if (alarm_active)
    {
      alarm_blink_ms = now;
    }
  }

  uint8_t sources = alarmCurrentSources();
  alarmUpdateRelayOutput();

  // Buzzer-Quittierung gilt nur fuer den aktuellen Alarm. Kommt eine neue
  // Quelle hinzu (z.B. Safety zusaetzlich zu GRENZWERT), muss der Ton wieder
  // anlaufen. Rahmen/Status/Relais bleiben grundsaetzlich nicht quittierbar.
  if (sources == 0)
  {
    alarm_buzzer_ack = false;
    alarm_buzzer_sources_prev = 0;
  }
  else if ((sources & (uint8_t)~alarm_buzzer_sources_prev) != 0)
  {
    alarm_buzzer_ack = false;
    alarm_blink_ms = now;
    alarm_buzzer_sources_prev = sources;
  }
  else
  {
    alarm_buzzer_sources_prev = sources;
  }

  if (sources == 0 || alarm_buzzer_ack || !alarmBuzzerEnabled())
  {
    if (alarm_buzzer_output_on)
    {
      analogWrite(pinAlarm, 0);
      alarm_buzzer_output_on = false;
    }
    return;
  }

  // Quittierbarer Intervallton: hoerbar, aber nicht dauerhaft nervend.
  const uint32_t periodMs = 1000UL;
  const uint32_t onMs = 350UL;
  uint32_t phase = (now - alarm_blink_ms) % periodMs;
  bool wantOn = (phase < onMs);

  if (wantOn != alarm_buzzer_output_on)
  {
    alarm_buzzer_output_on = wantOn;
    analogWrite(pinAlarm, wantOn ? alarm_cfg.buzzer_pwm : 0);
  }
}

void FLASHMEM alarmResetDisplayCache(void)
{
  alarm_indicator_drawn = false;
  alarm_indicator_ack_cache = false;
}

void FLASHMEM alarmDrawMainIndicator(void)
{
  // V22: Die Alarm-/Messwertanzeige wird im Hauptscreen jetzt zentral in
  // TPdisplay.ino gezeichnet: 3-px-Rahmen um den Messwertblock plus
  // Statuszeilen-Eintrag vor Safety. Diese Funktion bleibt als leere
  // Kompatibilitaets-Huelle bestehen.
}

