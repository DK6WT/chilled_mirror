/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPdeviceStorage.ino
 * Zweck: Backup-/Restore-Funktionen fuer Geraeteeinstellungen und
 *        Werksjustierung des Grundgeraetes.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <TimeLib.h>
#include <string.h>
#include "TP_T.h"
#include "TPlanguage.h"
#include "EEPROMAnything.h"

extern var_t R;
extern bool deviceSerialNormalize(char* text, size_t textSize);
extern const char* deviceSerialGet(void);
extern bool deviceSerialSet(const char* text);
extern bool controlParamsEditUnlocked(void);
extern void controlParamsDiscardAndLock(void);

// Aus anderen Modulen
extern uint8_t ui_language;
extern void fanApplyNormalSpeed(void);
extern void interfaceApplySerialBaud(void);
extern bool interfaceConfigExport(uint8_t* dst, uint16_t dstSize, uint16_t* usedSize);
extern bool interfaceConfigImport(const uint8_t* src, uint16_t srcSize);
extern bool alarmConfigExport(uint8_t* dst, uint16_t dstSize, uint16_t* usedSize);
extern bool alarmConfigImport(const uint8_t* src, uint16_t srcSize);
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

// Reservierte Snapshot-Groessen. Bewusst etwas groesser als die aktuellen
// Strukturen, damit kleine Erweiterungen nicht sofort den Backupblock brechen.
#define DEVICE_SETTINGS_INTERFACE_BYTES 192U
#define DEVICE_SETTINGS_ALARM_BYTES      64U

static const uint32_t DEVICE_SETTINGS_MAGIC = 0x44535431UL; // "DST1"
static const uint16_t DEVICE_SETTINGS_VERSION = 1;
static const uint32_t DEVICE_FACTORY_MAGIC = 0x44574631UL;  // "DWF1"
static const uint16_t DEVICE_FACTORY_VERSION = 1;

// EEPROM V11: Backup-/Factory-Bloecke mit A/B-Slots.
#define DEVICE_FACTORY_SLOT_MAGIC    0x44464142UL  // 'DFAB'
#define DEVICE_FACTORY_SLOT_VERSION  1U
#define DEVICE_FACTORY_SLOT_A_ADDR   2304
#define DEVICE_FACTORY_SLOT_B_ADDR   2496
#define DEVICE_FACTORY_SLOT_SIZE     192

#define DEVICE_SETTINGS_SLOT_MAGIC   0x44534142UL  // 'DSAB'
#define DEVICE_SETTINGS_SLOT_VERSION 1U
#define DEVICE_SETTINGS_SLOT_A_ADDR  2688
#define DEVICE_SETTINGS_SLOT_B_ADDR  3200
#define DEVICE_SETTINGS_SLOT_SIZE    512

static uint32_t FLASHMEM deviceStorageCrc(const uint8_t* p, size_t n)
{
  uint32_t crc = 2166136261UL;
  for (size_t i = 0; i < n; i++)
  {
    crc ^= p[i];
    crc *= 16777619UL;
  }
  return crc;
}

static uint32_t FLASHMEM deviceStorageCurrentDate()
{
  const int y = year();
  const int m = month();
  const int d = day();

  if (y < 2024 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31)
  {
    return 0;
  }

  return (uint32_t)y * 10000UL + (uint32_t)m * 100UL + (uint32_t)d;
}

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t save_date_yyyymmdd;

  var_t settings;

  uint8_t ui_language_value;
  uint8_t adc_filter_mode;
  uint8_t adc1_sfocal_mode;

  uint16_t interface_size;
  uint16_t alarm_size;
  uint8_t  interface_data[DEVICE_SETTINGS_INTERFACE_BYTES];
  uint8_t  alarm_data[DEVICE_SETTINGS_ALARM_BYTES];

  uint8_t reserved[24];
  uint32_t crc;
} device_settings_backup_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t save_date_yyyymmdd;

  // Reine Grundgeraete-Werksjustierung:
  // Ref Low/High und kanalbezogene Restkorrekturen.
  uint32_t ref_date_yyyymmdd;
  int32_t ref100_scaled;
  int32_t ref120_scaled;
  int32_t chA_corr100_scaled;
  int32_t chA_corr120_scaled;
  int32_t chB_corr100_scaled;
  int32_t chB_corr120_scaled;

  char device_name[21];
  uint8_t reserved[23];
  uint32_t crc;
} device_factory_cal_backup_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  device_factory_cal_backup_t payload;
  uint32_t crc;
} device_factory_slot_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  device_settings_backup_t payload;
  uint32_t crc;
} device_settings_slot_t;

static_assert(sizeof(device_factory_slot_t) <= DEVICE_FACTORY_SLOT_SIZE,
              "Factory EEPROM slot too small");
static_assert(sizeof(device_settings_slot_t) <= DEVICE_SETTINGS_SLOT_SIZE,
              "Settings EEPROM slot too small");

static uint32_t device_factory_sequence = 0UL;
static uint32_t device_settings_sequence = 0UL;

static int FLASHMEM deviceFactorySlotAddr(uint8_t slot)
{
  return slot ? DEVICE_FACTORY_SLOT_B_ADDR : DEVICE_FACTORY_SLOT_A_ADDR;
}

static int FLASHMEM deviceSettingsSlotAddr(uint8_t slot)
{
  return slot ? DEVICE_SETTINGS_SLOT_B_ADDR : DEVICE_SETTINGS_SLOT_A_ADDR;
}

static bool FLASHMEM deviceSettingsBackupValidate(const void* storePtr)
{
  const device_settings_backup_t& st = *(const device_settings_backup_t*)storePtr;
  if (st.magic != DEVICE_SETTINGS_MAGIC) return false;
  if (st.version != DEVICE_SETTINGS_VERSION) return false;
  if (st.size != sizeof(device_settings_backup_t)) return false;
  if (st.ui_language_value >= LANG_COUNT) return false;
  if (st.adc_filter_mode > ADC_MEAS_FILTER_MAX) return false;
  if (st.adc1_sfocal_mode > ADC1_SFOCAL_MAX) return false;
  if (st.interface_size == 0 || st.interface_size > DEVICE_SETTINGS_INTERFACE_BYTES) return false;
  if (st.alarm_size == 0 || st.alarm_size > DEVICE_SETTINGS_ALARM_BYTES) return false;

  const uint32_t crc = deviceStorageCrc((const uint8_t*)&st,
                                        sizeof(device_settings_backup_t) - sizeof(uint32_t));
  return (crc == st.crc);
}

static bool FLASHMEM deviceFactoryCalValidate(const void* storePtr)
{
  const device_factory_cal_backup_t& st = *(const device_factory_cal_backup_t*)storePtr;
  if (st.magic != DEVICE_FACTORY_MAGIC) return false;
  if (st.version != DEVICE_FACTORY_VERSION) return false;
  if (st.size != sizeof(device_factory_cal_backup_t)) return false;

  // Plausibilitaeten entsprechen dem Ref-Low/High-Bereich aus der aktiven
  // Referenzkalibrierung. Die eigentliche Pruefung beim Laden macht zusaetzlich
  // refCalSetAllScaled().
  if (st.ref100_scaled < 6000000L || st.ref100_scaled > 11000000L) return false;
  if (st.ref120_scaled < 11500000L || st.ref120_scaled > 15200000L) return false;
  if ((st.ref120_scaled - st.ref100_scaled) < 500000L) return false;

  if (st.chA_corr100_scaled < -10000L || st.chA_corr100_scaled > 10000L) return false;
  if (st.chA_corr120_scaled < -10000L || st.chA_corr120_scaled > 10000L) return false;
  if (st.chB_corr100_scaled < -10000L || st.chB_corr100_scaled > 10000L) return false;
  if (st.chB_corr120_scaled < -10000L || st.chB_corr120_scaled > 10000L) return false;

  const uint32_t crc = deviceStorageCrc((const uint8_t*)&st,
                                        sizeof(device_factory_cal_backup_t) - sizeof(uint32_t));
  return (crc == st.crc);
}

static bool FLASHMEM deviceSettingsReadSlot(uint8_t slot, device_settings_slot_t& out)
{
  EEPROM.get(deviceSettingsSlotAddr(slot), out);
  if (out.magic != DEVICE_SETTINGS_SLOT_MAGIC) return false;
  if (out.version != DEVICE_SETTINGS_SLOT_VERSION) return false;
  if (out.size != sizeof(device_settings_backup_t)) return false;
  const uint32_t slotCrc = deviceStorageCrc((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));
  if (slotCrc != out.crc) return false;
  return deviceSettingsBackupValidate(&out.payload);
}

static bool FLASHMEM deviceFactoryReadSlot(uint8_t slot, device_factory_slot_t& out)
{
  EEPROM.get(deviceFactorySlotAddr(slot), out);
  if (out.magic != DEVICE_FACTORY_SLOT_MAGIC) return false;
  if (out.version != DEVICE_FACTORY_SLOT_VERSION) return false;
  if (out.size != sizeof(device_factory_cal_backup_t)) return false;
  const uint32_t slotCrc = deviceStorageCrc((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));
  if (slotCrc != out.crc) return false;
  return deviceFactoryCalValidate(&out.payload);
}

static uint32_t FLASHMEM deviceStorageNextSeq(uint32_t baseSeq)
{
  baseSeq++;
  if (baseSeq == 0UL) baseSeq = 1UL;
  return baseSeq;
}

static void FLASHMEM deviceSettingsHealMissingSlot(const device_settings_backup_t& st,
                                                   uint32_t baseSeq,
                                                   bool validA,
                                                   bool validB)
{
  if (validA && validB) return;

  const uint8_t targetSlot = validA ? 1U : 0U;
  const uint32_t nextSeq = deviceStorageNextSeq(baseSeq);

  device_settings_slot_t out;
  memset(&out, 0, sizeof(out));
  out.magic = DEVICE_SETTINGS_SLOT_MAGIC;
  out.version = DEVICE_SETTINGS_SLOT_VERSION;
  out.size = sizeof(device_settings_backup_t);
  out.sequence = nextSeq;
  out.payload = st;
  out.crc = deviceStorageCrc((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));
  EEPROM.put(deviceSettingsSlotAddr(targetSlot), out);
  device_settings_sequence = nextSeq;
}

static void FLASHMEM deviceFactoryHealMissingSlot(const device_factory_cal_backup_t& st,
                                                  uint32_t baseSeq,
                                                  bool validA,
                                                  bool validB)
{
  if (validA && validB) return;

  const uint8_t targetSlot = validA ? 1U : 0U;
  const uint32_t nextSeq = deviceStorageNextSeq(baseSeq);

  device_factory_slot_t out;
  memset(&out, 0, sizeof(out));
  out.magic = DEVICE_FACTORY_SLOT_MAGIC;
  out.version = DEVICE_FACTORY_SLOT_VERSION;
  out.size = sizeof(device_factory_cal_backup_t);
  out.sequence = nextSeq;
  out.payload = st;
  out.crc = deviceStorageCrc((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));
  EEPROM.put(deviceFactorySlotAddr(targetSlot), out);
  device_factory_sequence = nextSeq;
}

static bool FLASHMEM deviceSettingsReadBest(device_settings_backup_t& st, uint32_t* seqOut = nullptr)
{
  device_settings_slot_t a;
  device_settings_slot_t b;
  const bool validA = deviceSettingsReadSlot(0, a);
  const bool validB = deviceSettingsReadSlot(1, b);
  if (!validA && !validB) return false;

  uint32_t bestSeq = 0UL;
  if (validA && (!validB || a.sequence >= b.sequence))
  {
    st = a.payload;
    bestSeq = a.sequence;
    device_settings_sequence = a.sequence;
    if (seqOut) *seqOut = a.sequence;
  }
  else
  {
    st = b.payload;
    bestSeq = b.sequence;
    device_settings_sequence = b.sequence;
    if (seqOut) *seqOut = b.sequence;
  }

  // A/B-Self-Heal fuer Backup-Bloecke: Wenn ein Slot weg/kaputt ist,
  // wird er beim naechsten Lesen sofort aus dem gueltigen Slot rekonstruiert.
  deviceSettingsHealMissingSlot(st, bestSeq, validA, validB);
  return true;
}

static bool FLASHMEM deviceFactoryReadBest(device_factory_cal_backup_t& st, uint32_t* seqOut = nullptr)
{
  device_factory_slot_t a;
  device_factory_slot_t b;
  const bool validA = deviceFactoryReadSlot(0, a);
  const bool validB = deviceFactoryReadSlot(1, b);
  if (!validA && !validB) return false;

  uint32_t bestSeq = 0UL;
  if (validA && (!validB || a.sequence >= b.sequence))
  {
    st = a.payload;
    bestSeq = a.sequence;
    device_factory_sequence = a.sequence;
    if (seqOut) *seqOut = a.sequence;
  }
  else
  {
    st = b.payload;
    bestSeq = b.sequence;
    device_factory_sequence = b.sequence;
    if (seqOut) *seqOut = b.sequence;
  }

  deviceFactoryHealMissingSlot(st, bestSeq, validA, validB);
  return true;
}

bool FLASHMEM deviceSettingsBackupValid()
{
  device_settings_backup_t st;
  return deviceSettingsReadBest(st);
}

bool FLASHMEM deviceFactoryCalBackupValid()
{
  device_factory_cal_backup_t st;
  return deviceFactoryReadBest(st);
}

uint32_t FLASHMEM deviceSettingsBackupDate()
{
  device_settings_backup_t st;
  return deviceSettingsReadBest(st) ? st.save_date_yyyymmdd : 0;
}

uint32_t FLASHMEM deviceFactoryCalBackupDate()
{
  device_factory_cal_backup_t st;
  return deviceFactoryReadBest(st) ? st.save_date_yyyymmdd : 0;
}

bool FLASHMEM deviceSettingsBackupSave()
{
  device_settings_backup_t st;
  memset(&st, 0, sizeof(st));

  st.magic = DEVICE_SETTINGS_MAGIC;
  st.version = DEVICE_SETTINGS_VERSION;
  st.size = sizeof(device_settings_backup_t);
  st.save_date_yyyymmdd = deviceStorageCurrentDate();
  deviceSerialNormalize(R.geraete_name, sizeof(R.geraete_name));
  st.settings = R;

  st.ui_language_value = (ui_language < LANG_COUNT) ? ui_language : LANG_DE;
  st.adc_filter_mode = adcFilterModeGet();
  st.adc1_sfocal_mode = adc1SfocalModeGet();

  if (!interfaceConfigExport(st.interface_data, DEVICE_SETTINGS_INTERFACE_BYTES, &st.interface_size)) return false;
  if (!alarmConfigExport(st.alarm_data, DEVICE_SETTINGS_ALARM_BYTES, &st.alarm_size)) return false;

  st.crc = deviceStorageCrc((const uint8_t*)&st,
                            sizeof(device_settings_backup_t) - sizeof(uint32_t));

  device_settings_slot_t slotA;
  device_settings_slot_t slotB;
  const bool validA = deviceSettingsReadSlot(0, slotA);
  const bool validB = deviceSettingsReadSlot(1, slotB);
  uint32_t nextSeq = device_settings_sequence;
  if (validA && slotA.sequence > nextSeq) nextSeq = slotA.sequence;
  if (validB && slotB.sequence > nextSeq) nextSeq = slotB.sequence;
  nextSeq++;
  if (nextSeq == 0UL) nextSeq = 1UL;

  uint8_t targetSlot;
  if (!validA) targetSlot = 0;
  else if (!validB) targetSlot = 1;
  else targetSlot = (slotA.sequence <= slotB.sequence) ? 0 : 1;

  device_settings_slot_t out;
  memset(&out, 0, sizeof(out));
  out.magic = DEVICE_SETTINGS_SLOT_MAGIC;
  out.version = DEVICE_SETTINGS_SLOT_VERSION;
  out.size = sizeof(device_settings_backup_t);
  out.sequence = nextSeq;
  out.payload = st;
  out.crc = deviceStorageCrc((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));
  EEPROM.put(deviceSettingsSlotAddr(targetSlot), out);
  device_settings_sequence = nextSeq;

  if (!validA && !validB)
  {
    // Erste Sicherung nach leerem EEPROM: den zweiten Slot direkt mitfuellen,
    // damit das Backup nicht bis zum naechsten Speichern nur einfach vorhanden ist.
    const uint8_t otherSlot = targetSlot ? 0U : 1U;
    out.sequence = deviceStorageNextSeq(nextSeq);
    out.crc = deviceStorageCrc((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));
    EEPROM.put(deviceSettingsSlotAddr(otherSlot), out);
    device_settings_sequence = out.sequence;
  }
  return true;
}

bool FLASHMEM deviceSettingsBackupLoad()
{
  device_settings_backup_t st;
  if (!deviceSettingsReadBest(st)) return false;

  if (controlParamsEditUnlocked())
  {
    controlParamsDiscardAndLock();
  }

  // Zuerst die externen Snapshot-Bloecke pruefen/importieren. Bei Fehler wird
  // der Hauptparameterblock nicht veraendert.
  if (!interfaceConfigImport(st.interface_data, st.interface_size)) return false;
  if (!alarmConfigImport(st.alarm_data, st.alarm_size)) return false;

  // Normale Einstellungen laden, aber kopfspezifische Kalibrier-/Regelwerte
  // bewusst erhalten. Diese Werte gehoeren zur Kopfdatei auf SD.
  const uint8_t  keep_head_type = R.head_type;
  const uint32_t keep_head_serial = R.head_serial;
  const uint16_t keep_pid_kp = R.pid_kp;
  const float    keep_pid_ki = R.pid_ki;
  const float    keep_pid_kd = R.pid_kd;
  const uint16_t keep_regler_ms = R.regler_intervall_ms;
  const uint8_t  keep_hbridge_ms = R.h_bruecke_totzeit;
  const uint8_t  keep_fan_percent = R.fan_percent;
  const uint16_t keep_optik_sollwert = R.optik_sollwert;

  R = st.settings;
  R.head_type = keep_head_type;
  R.head_serial = keep_head_serial;
  R.pid_kp = keep_pid_kp;
  R.pid_ki = keep_pid_ki;
  R.pid_kd = keep_pid_kd;
  R.regler_intervall_ms = keep_regler_ms;
  R.h_bruecke_totzeit = keep_hbridge_ms;
  R.fan_percent = keep_fan_percent;
  R.optik_sollwert = keep_optik_sollwert;

  deviceSerialNormalize(R.geraete_name, sizeof(R.geraete_name));
  tpMainConfigSave();

  ui_language = (st.ui_language_value < LANG_COUNT) ? st.ui_language_value : LANG_DE;
  uiLanguageSave();

  adcFilterModeSet(st.adc_filter_mode);
  adc1SfocalModeSet(st.adc1_sfocal_mode);

  fanApplyNormalSpeed();
  interfaceApplySerialBaud();

  return true;
}

bool FLASHMEM deviceFactoryCalBackupSave()
{
  device_factory_cal_backup_t st;
  memset(&st, 0, sizeof(st));

  st.magic = DEVICE_FACTORY_MAGIC;
  st.version = DEVICE_FACTORY_VERSION;
  st.size = sizeof(device_factory_cal_backup_t);
  st.save_date_yyyymmdd = deviceStorageCurrentDate();

  refCalGetAllScaled(&st.ref_date_yyyymmdd,
                     &st.ref100_scaled,
                     &st.ref120_scaled,
                     &st.chA_corr100_scaled,
                     &st.chA_corr120_scaled,
                     &st.chB_corr100_scaled,
                     &st.chB_corr120_scaled);

  if (st.ref_date_yyyymmdd == 0)
  {
    st.ref_date_yyyymmdd = st.save_date_yyyymmdd;
  }

  strncpy(st.device_name, deviceSerialGet(), sizeof(st.device_name) - 1);
  st.device_name[sizeof(st.device_name) - 1] = '\0';

  if (!deviceFactoryCalValidate(&st))
  {
    // deviceFactoryCalValidate prueft auch CRC; darum CRC erst danach setzen.
    // Diese Vorpruefung nur manuell fuer die eigentlichen Werte wiederholen.
    if (st.ref100_scaled < 6000000L || st.ref100_scaled > 11000000L) return false;
    if (st.ref120_scaled < 11500000L || st.ref120_scaled > 15200000L) return false;
    if ((st.ref120_scaled - st.ref100_scaled) < 500000L) return false;
    if (st.chA_corr100_scaled < -10000L || st.chA_corr100_scaled > 10000L) return false;
    if (st.chA_corr120_scaled < -10000L || st.chA_corr120_scaled > 10000L) return false;
    if (st.chB_corr100_scaled < -10000L || st.chB_corr100_scaled > 10000L) return false;
    if (st.chB_corr120_scaled < -10000L || st.chB_corr120_scaled > 10000L) return false;
  }

  st.crc = deviceStorageCrc((const uint8_t*)&st,
                            sizeof(device_factory_cal_backup_t) - sizeof(uint32_t));

  device_factory_slot_t slotA;
  device_factory_slot_t slotB;
  const bool validA = deviceFactoryReadSlot(0, slotA);
  const bool validB = deviceFactoryReadSlot(1, slotB);
  uint32_t nextSeq = device_factory_sequence;
  if (validA && slotA.sequence > nextSeq) nextSeq = slotA.sequence;
  if (validB && slotB.sequence > nextSeq) nextSeq = slotB.sequence;
  nextSeq++;
  if (nextSeq == 0UL) nextSeq = 1UL;

  uint8_t targetSlot;
  if (!validA) targetSlot = 0;
  else if (!validB) targetSlot = 1;
  else targetSlot = (slotA.sequence <= slotB.sequence) ? 0 : 1;

  device_factory_slot_t out;
  memset(&out, 0, sizeof(out));
  out.magic = DEVICE_FACTORY_SLOT_MAGIC;
  out.version = DEVICE_FACTORY_SLOT_VERSION;
  out.size = sizeof(device_factory_cal_backup_t);
  out.sequence = nextSeq;
  out.payload = st;
  out.crc = deviceStorageCrc((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));
  EEPROM.put(deviceFactorySlotAddr(targetSlot), out);
  device_factory_sequence = nextSeq;

  if (!validA && !validB)
  {
    const uint8_t otherSlot = targetSlot ? 0U : 1U;
    out.sequence = deviceStorageNextSeq(nextSeq);
    out.crc = deviceStorageCrc((const uint8_t*)&out, sizeof(out) - sizeof(out.crc));
    EEPROM.put(deviceFactorySlotAddr(otherSlot), out);
    device_factory_sequence = out.sequence;
  }
  return true;
}

bool FLASHMEM deviceFactoryCalBackupSaveForSerial(const char* newSerial)
{
  if (newSerial == nullptr) return false;

  char oldSerial[DEVICE_SERIAL_DIGITS + 1U];
  strncpy(oldSerial, deviceSerialGet(), sizeof(oldSerial) - 1U);
  oldSerial[sizeof(oldSerial) - 1U] = '\0';

  if (!deviceSerialSet(newSerial)) return false;

  const bool ok = deviceFactoryCalBackupSave();
  if (ok)
  {
    tpMainConfigSave();
    return true;
  }

  deviceSerialSet(oldSerial);
  return false;
}

bool FLASHMEM deviceFactoryCalBackupLoad()
{
  device_factory_cal_backup_t st;
  if (!deviceFactoryReadBest(st)) return false;

  if (controlParamsEditUnlocked())
  {
    controlParamsDiscardAndLock();
  }

  return refCalSetAllScaled(st.ref_date_yyyymmdd,
                            st.ref100_scaled,
                            st.ref120_scaled,
                            st.chA_corr100_scaled,
                            st.chA_corr120_scaled,
                            st.chB_corr100_scaled,
                            st.chB_corr120_scaled);
}
