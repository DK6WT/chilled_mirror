/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPheadCalibration.ino
 * Zweck: SD-Speichern/Laden der geraete- und kopfspezifischen Kopfjustierung.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <Arduino.h>
#include <SD.h>
#include <TimeLib.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "TP_T.h"
#include "EEPROMAnything.h"
#include "TPsignedCalibration.h"

extern var_t R;
extern void fanApplyNormalSpeed(void);
extern bool sdLogEnsureReadyForAccess(void);
extern const char* deviceSerialGet(void);
extern int32_t pt100R0GetScaled(uint8_t sensor);
extern bool pt100R0SetScaled(uint8_t sensor, int32_t r0_scaled);
extern void pt100Cal2GetScaled(uint8_t sensor, int32_t* soll1, int32_t* ist1, int32_t* soll2, int32_t* ist2, bool* aktiv);
extern bool pt100Cal2SetScaled(uint8_t sensor, int32_t soll1, int32_t ist1, int32_t soll2, int32_t ist2);
extern void pt100Cal2Disable(uint8_t sensor);
extern int32_t taupunktOffsetGetScaled(void);
extern bool taupunktOffsetSetScaled(int32_t offset_mC);

#define HEADCAL_DIR          "/HEADCAL"
#define HEADCAL_PATH_MAX     96U
#define HEADCAL_LINE_MAX     128U
#define HEADCAL_MAX_TYPES    8U
#define HEADCAL_MAX_SERIALS  16U

static const char* FLASHMEM headCalBaseName(const char* path)
{
  if (!path) return "";
  const char* s = strrchr(path, '/');
  return s ? (s + 1) : path;
}

static bool FLASHMEM headCalAllDigits(const char* s, uint8_t n)
{
  if (!s) return false;
  for (uint8_t i = 0; i < n; i++)
  {
    if (s[i] < '0' || s[i] > '9') return false;
  }
  return true;
}

static bool FLASHMEM headCalNormalizeTypeText(const char* token, char* out, size_t outSize)
{
  if (!token || !out || outSize < (HEAD_TYPE_TEXT_LEN + 1U)) return false;

  if (strncmp(token, "STP-", 4) == 0 && headCalAllDigits(token + 4, 4))
  {
    memcpy(out, token, HEAD_TYPE_TEXT_LEN);
    out[HEAD_TYPE_TEXT_LEN] = '\0';
    return headTypeTextValid(out);
  }

  // Rueckwaerts kompatibel: alte Kopfdateien/Dateinamen mit STP3001 ohne Bindestrich.
  if (strncmp(token, "STP", 3) == 0 && headCalAllDigits(token + 3, 4))
  {
    memcpy(out, "STP-", 4);
    memcpy(out + 4, token + 3, 4);
    out[HEAD_TYPE_TEXT_LEN] = '\0';
    return headTypeTextValid(out);
  }

  return false;
}

static bool FLASHMEM headCalParseFileName(const char* fileName,
                                          char* headTypeText,
                                          size_t headTypeTextSize,
                                          uint32_t* headSerial,
                                          char* deviceSn,
                                          uint32_t* dateYmd,
                                          uint32_t* timeHms)
{
  const char* n = headCalBaseName(fileName);
  if (!n) return false;

  char typeText[HEAD_TYPE_TEXT_LEN + 1U];
  uint8_t pos = 0;

  if (strncmp(n, "STP-", 4) == 0 && headCalAllDigits(n + 4, 4))
  {
    if (!headCalNormalizeTypeText(n, typeText, sizeof(typeText))) return false;
    pos = 8;
  }
  else if (strncmp(n, "STP", 3) == 0 && headCalAllDigits(n + 3, 4))
  {
    if (!headCalNormalizeTypeText(n, typeText, sizeof(typeText))) return false;
    pos = 7;
  }
  else
  {
    return false;
  }

  if (strncmp(n + pos, "_K", 2) != 0) return false;
  pos += 2;
  if (!headCalAllDigits(n + pos, 5)) return false;
  char tmp[10];
  memcpy(tmp, n + pos, 5); tmp[5] = '\0';
  uint32_t hs = strtoul(tmp, nullptr, 10);
  pos += 5;

  if (strncmp(n + pos, "_G", 2) != 0) return false;
  pos += 2;
  if (!headCalAllDigits(n + pos, 5)) return false;
  if (deviceSn)
  {
    memcpy(deviceSn, n + pos, 5);
    deviceSn[5] = '\0';
  }
  pos += 5;

  if (n[pos++] != '_') return false;
  if (!headCalAllDigits(n + pos, 8)) return false;
  memcpy(tmp, n + pos, 8); tmp[8] = '\0';
  uint32_t dt = strtoul(tmp, nullptr, 10);
  pos += 8;

  if (n[pos++] != '_') return false;
  if (!headCalAllDigits(n + pos, 6)) return false;
  memcpy(tmp, n + pos, 6); tmp[6] = '\0';
  uint32_t tm = strtoul(tmp, nullptr, 10);
  pos += 6;

  if (strcmp(n + pos, ".CAL") != 0) return false;

  if (headTypeText && headTypeTextSize)
  {
    strncpy(headTypeText, typeText, headTypeTextSize - 1U);
    headTypeText[headTypeTextSize - 1U] = '\0';
  }
  if (headSerial) *headSerial = hs;
  if (dateYmd) *dateYmd = dt;
  if (timeHms) *timeHms = tm;
  return true;
}

static uint32_t FLASHMEM headCalCrc32Update(uint32_t crc, const uint8_t* data, size_t len)
{
  crc = ~crc;
  for (size_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++)
    {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return ~crc;
}

static void FLASHMEM headCalCrcLine(uint32_t* crc, const char* line)
{
  if (!crc || !line) return;
  *crc = headCalCrc32Update(*crc, (const uint8_t*)line, strlen(line));
  const char lf = '\n';
  *crc = headCalCrc32Update(*crc, (const uint8_t*)&lf, 1U);
}

static bool FLASHMEM headCalEnsureDir(void)
{
  if (!sdLogEnsureReadyForAccess()) return false;
  if (!SD.exists(HEADCAL_DIR))
  {
    if (!SD.mkdir(HEADCAL_DIR)) return false;
  }
  return true;
}

static bool FLASHMEM headCalRtcValid(void)
{
  const int y = year();
  const int mo = month();
  const int d = day();
  const int h = hour();
  const int mi = minute();
  const int se = second();
  if (y < 2026 || y > 2079) return false;
  if (mo < 1 || mo > 12) return false;
  if (d < 1 || d > 31) return false;
  if (h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 || se > 59) return false;
  return true;
}

static void FLASHMEM headCalCurrentData(const char* headTypeText, uint32_t headSerial, head_cal_data_t* d)
{
  if (!d) return;
  memset(d, 0, sizeof(*d));

  char typeText[HEAD_TYPE_TEXT_LEN + 1U];
  if (!headCalNormalizeTypeText(headTypeText, typeText, sizeof(typeText)))
  {
    strncpy(typeText, HEAD_TYPE_DEFAULT_TEXT, sizeof(typeText) - 1U);
    typeText[sizeof(typeText) - 1U] = '\0';
  }

  const int8_t profile = headTypeProfileFromText(typeText);
  d->head_type = (profile >= 0) ? (uint8_t)profile : HEAD_TYPE_DEFAULT;
  const size_t typeLength = strnlen(typeText, sizeof(d->head_type_text) - 1U);
  memcpy(d->head_type_text, typeText, typeLength);
  d->head_type_text[typeLength] = '\0';
  d->head_serial = headSerial;
  strncpy(d->device_sn, deviceSerialGet(), sizeof(d->device_sn) - 1U);
  d->device_sn[sizeof(d->device_sn) - 1U] = '\0';
  d->cal_date_yyyymmdd = (uint32_t)year() * 10000UL + (uint32_t)month() * 100UL + (uint32_t)day();
  d->cal_time_hhmmss = (uint32_t)hour() * 10000UL + (uint32_t)minute() * 100UL + (uint32_t)second();

  d->r0_mirror_scaled = pt100R0GetScaled(0);
  d->r0_ambient_scaled = pt100R0GetScaled(1);

  bool active = false;
  pt100Cal2GetScaled(0, &d->p2_soll1_mC[0], &d->p2_ist1_mC[0], &d->p2_soll2_mC[0], &d->p2_ist2_mC[0], &active);
  d->p2_active[0] = active ? 1U : 0U;
  pt100Cal2GetScaled(1, &d->p2_soll1_mC[1], &d->p2_ist1_mC[1], &d->p2_soll2_mC[1], &d->p2_ist2_mC[1], &active);
  d->p2_active[1] = active ? 1U : 0U;

  d->tau_offset_mC = taupunktOffsetGetScaled();
  d->pid_kp = R.pid_kp;
  d->pid_ki_x1000 = (int32_t)lroundf(R.pid_ki * 1000.0f);
  d->pid_kd_x1000 = (int32_t)lroundf(R.pid_kd * 1000.0f);
  d->regler_intervall_ms = R.regler_intervall_ms;
  d->h_bruecke_totzeit_ms = R.h_bruecke_totzeit;
  d->fan_percent = R.fan_percent;
  d->optik_sollwert_x10 = R.optik_sollwert;
  d->peltier_current_limit_ma = peltierCurrentLimitGetMa();
}

static bool FLASHMEM headCalDataPlausible(const head_cal_data_t* d)
{
  if (!d) return false;
  if (!headTypeTextValid(d->head_type_text)) return false;
  if (d->head_serial > HEAD_SERIAL_MAX) return false;
  if (strlen(d->device_sn) != DEVICE_SERIAL_DIGITS) return false;
  if (!headCalAllDigits(d->device_sn, DEVICE_SERIAL_DIGITS)) return false;
  if (d->cal_date_yyyymmdd < 20260101UL || d->cal_date_yyyymmdd > 20791231UL) return false;
  if (d->cal_time_hhmmss > 235959UL) return false;
  if (d->r0_mirror_scaled < 800000L || d->r0_mirror_scaled > 1200000L) return false;
  if (d->r0_ambient_scaled < 800000L || d->r0_ambient_scaled > 1200000L) return false;
  if (d->tau_offset_mC < -2000L || d->tau_offset_mC > 2000L) return false;
  if (d->pid_kp > 1000U) return false;
  if (d->pid_ki_x1000 < 0L || d->pid_ki_x1000 > 50000L) return false;
  if (d->pid_kd_x1000 < 0L || d->pid_kd_x1000 > 50000L) return false;
  if (d->regler_intervall_ms < 50U || d->regler_intervall_ms > 1000U) return false;
  if (d->h_bruecke_totzeit_ms > 100U) return false;
  if (d->fan_percent < 50U || d->fan_percent > 100U) return false;
  if (d->optik_sollwert_x10 < 800U || d->optik_sollwert_x10 > 990U) return false;
  if (d->peltier_current_limit_ma < PELTIER_CURRENT_LIMIT_MIN_MA || d->peltier_current_limit_ma > PELTIER_CURRENT_LIMIT_MAX_MA) return false;

  for (uint8_t i = 0; i < 2; i++)
  {
    if (d->p2_active[i] > 1U) return false;
    if (d->p2_soll1_mC[i] < -30000L || d->p2_soll1_mC[i] > 90000L) return false;
    if (d->p2_ist1_mC[i]  < -30000L || d->p2_ist1_mC[i]  > 90000L) return false;
    if (d->p2_soll2_mC[i] < -30000L || d->p2_soll2_mC[i] > 90000L) return false;
    if (d->p2_ist2_mC[i]  < -30000L || d->p2_ist2_mC[i]  > 90000L) return false;
    if (d->p2_active[i])
    {
      if (labs(d->p2_soll2_mC[i] - d->p2_soll1_mC[i]) < 30000L) return false;
      if (labs(d->p2_ist2_mC[i]  - d->p2_ist1_mC[i])  < 25000L) return false;
    }
  }
  return true;
}

static bool FLASHMEM headCalApplyData(const head_cal_data_t* d)
{
  if (!headCalDataPlausible(d)) return false;
  if (strncmp(d->device_sn, deviceSerialGet(), DEVICE_SERIAL_DIGITS) != 0) return false;

  if (!pt100R0SetScaled(0, d->r0_mirror_scaled)) return false;
  if (!pt100R0SetScaled(1, d->r0_ambient_scaled)) return false;

  for (uint8_t i = 0; i < 2; i++)
  {
    if (d->p2_active[i])
    {
      if (!pt100Cal2SetScaled(i, d->p2_soll1_mC[i], d->p2_ist1_mC[i], d->p2_soll2_mC[i], d->p2_ist2_mC[i])) return false;
    }
    else
    {
      pt100Cal2Disable(i);
    }
  }

  if (!taupunktOffsetSetScaled(d->tau_offset_mC)) return false;

  headTypeTextSet(d->head_type_text);
  const int8_t profile = headTypeProfileFromText(d->head_type_text);
  R.head_type = (profile >= 0) ? (uint8_t)profile : HEAD_TYPE_DEFAULT;
  R.head_serial = d->head_serial;
  R.pid_kp = d->pid_kp;
  R.pid_ki = (float)d->pid_ki_x1000 / 1000.0f;
  R.pid_kd = (float)d->pid_kd_x1000 / 1000.0f;
  R.regler_intervall_ms = d->regler_intervall_ms;
  R.h_bruecke_totzeit = d->h_bruecke_totzeit_ms;
  R.fan_percent = d->fan_percent;
  R.optik_sollwert = d->optik_sollwert_x10;
  peltierCurrentLimitSetMa(d->peltier_current_limit_ma);
  fanApplyNormalSpeed();
  tpMainConfigSave();
  tpSignedCalibrationInvalidateCache();
  return true;
}

static bool FLASHMEM headCalWriteLine(File& f, uint32_t* crc, const char* fmt, ...)
{
  char line[HEADCAL_LINE_MAX];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= sizeof(line)) return false;
  if (f.print(line) == 0) return false;
  if (f.print('\n') == 0) return false;
  headCalCrcLine(crc, line);
  return true;
}

static bool FLASHMEM headCalBuildPath(char* path, size_t pathSize, const char* headTypeText, uint32_t headSerial, uint32_t dateYmd, uint32_t timeHms)
{
  if (!path || pathSize == 0 || !headTypeTextValid(headTypeText)) return false;
  int n = snprintf(path, pathSize, HEADCAL_DIR "/%s_K%05lu_G%s_%08lu_%06lu.CAL",
                   headTypeText,
                   (unsigned long)headSerial,
                   deviceSerialGet(),
                   (unsigned long)dateYmd,
                   (unsigned long)timeHms);
  return (n > 0 && (size_t)n < pathSize);
}

bool FLASHMEM headCalSave(const char* headTypeText, uint32_t headSerial, char* message, size_t messageSize)
{
  if (message && messageSize) message[0] = '\0';
  char typeText[HEAD_TYPE_TEXT_LEN + 1U];
  if (!headCalNormalizeTypeText(headTypeText, typeText, sizeof(typeText))) { if (message) snprintf(message, messageSize, "Kopftyp ungültig"); return false; }
  if (headSerial > HEAD_SERIAL_MAX) { if (message) snprintf(message, messageSize, "Kopf-SN ungültig"); return false; }
  if (!headCalRtcValid()) { if (message) snprintf(message, messageSize, "RTC-Datum ungültig"); return false; }
  if (!headCalEnsureDir()) { if (message) snprintf(message, messageSize, "SD/HEADCAL Fehler"); return false; }

  head_cal_data_t d;
  headCalCurrentData(typeText, headSerial, &d);
  if (!headCalDataPlausible(&d)) { if (message) snprintf(message, messageSize, "Kopfdaten ungültig"); return false; }

  char path[HEADCAL_PATH_MAX];
  if (!headCalBuildPath(path, sizeof(path), d.head_type_text, d.head_serial, d.cal_date_yyyymmdd, d.cal_time_hhmmss))
  {
    if (message) snprintf(message, messageSize, "Dateipfad zu lang");
    return false;
  }

  if (SD.exists(path))
  {
    if (message) snprintf(message, messageSize, "Datei existiert schon");
    return false;
  }

  File f = SD.open(path, FILE_WRITE);
  if (!f)
  {
    if (message) snprintf(message, messageSize, "Datei nicht schreibbar");
    return false;
  }

  uint32_t crc = 0UL;
  bool ok = true;
  ok = ok && headCalWriteLine(f, &crc, "TP3000_HEAD_CAL_V1");
  ok = ok && headCalWriteLine(f, &crc, "device_sn=%s", d.device_sn);
  ok = ok && headCalWriteLine(f, &crc, "head_type=%s", d.head_type_text);
  ok = ok && headCalWriteLine(f, &crc, "head_sn=%05lu", (unsigned long)d.head_serial);
  ok = ok && headCalWriteLine(f, &crc, "cal_date=%04lu-%02lu-%02lu", (unsigned long)(d.cal_date_yyyymmdd / 10000UL), (unsigned long)((d.cal_date_yyyymmdd / 100UL) % 100UL), (unsigned long)(d.cal_date_yyyymmdd % 100UL));
  ok = ok && headCalWriteLine(f, &crc, "cal_time=%02lu:%02lu:%02lu", (unsigned long)(d.cal_time_hhmmss / 10000UL), (unsigned long)((d.cal_time_hhmmss / 100UL) % 100UL), (unsigned long)(d.cal_time_hhmmss % 100UL));
  ok = ok && headCalWriteLine(f, &crc, "r0_mirror_scaled=%ld", (long)d.r0_mirror_scaled);
  ok = ok && headCalWriteLine(f, &crc, "r0_ambient_scaled=%ld", (long)d.r0_ambient_scaled);
  for (uint8_t i = 0; i < 2; i++)
  {
    const char* pfx = (i == 0) ? "mirror" : "ambient";
    ok = ok && headCalWriteLine(f, &crc, "pt100_%s_active=%u", pfx, (unsigned)d.p2_active[i]);
    ok = ok && headCalWriteLine(f, &crc, "pt100_%s_soll1_mC=%ld", pfx, (long)d.p2_soll1_mC[i]);
    ok = ok && headCalWriteLine(f, &crc, "pt100_%s_ist1_mC=%ld",  pfx, (long)d.p2_ist1_mC[i]);
    ok = ok && headCalWriteLine(f, &crc, "pt100_%s_soll2_mC=%ld", pfx, (long)d.p2_soll2_mC[i]);
    ok = ok && headCalWriteLine(f, &crc, "pt100_%s_ist2_mC=%ld",  pfx, (long)d.p2_ist2_mC[i]);
  }
  ok = ok && headCalWriteLine(f, &crc, "tau_offset_mC=%ld", (long)d.tau_offset_mC);
  ok = ok && headCalWriteLine(f, &crc, "pid_kp=%u", (unsigned)d.pid_kp);
  ok = ok && headCalWriteLine(f, &crc, "pid_ki_x1000=%ld", (long)d.pid_ki_x1000);
  ok = ok && headCalWriteLine(f, &crc, "pid_kd_x1000=%ld", (long)d.pid_kd_x1000);
  ok = ok && headCalWriteLine(f, &crc, "regler_intervall_ms=%u", (unsigned)d.regler_intervall_ms);
  ok = ok && headCalWriteLine(f, &crc, "hbridge_deadtime_ms=%u", (unsigned)d.h_bruecke_totzeit_ms);
  ok = ok && headCalWriteLine(f, &crc, "fan_percent=%u", (unsigned)d.fan_percent);
  ok = ok && headCalWriteLine(f, &crc, "optical_target_x10=%u", (unsigned)d.optik_sollwert_x10);
  ok = ok && headCalWriteLine(f, &crc, "peltier_current_limit_ma=%u", (unsigned)d.peltier_current_limit_ma);

  if (ok)
  {
    char crcLine[24];
    snprintf(crcLine, sizeof(crcLine), "crc32=%08lX", (unsigned long)crc);
    ok = (f.print(crcLine) > 0 && f.print('\n') > 0);
  }
  f.close();

  if (!ok)
  {
    SD.remove(path);
    if (message) snprintf(message, messageSize, "Schreibfehler");
    return false;
  }

  headTypeTextSet(d.head_type_text);
  R.head_serial = d.head_serial;
  tpMainConfigSave();
  tpSignedCalibrationInvalidateCache();

  if (message) snprintf(message, messageSize, "Gespeichert: %s K%05lu",
                      d.head_type_text,
                      (unsigned long)d.head_serial);
  return true;
}

static bool FLASHMEM headCalParseKv(head_cal_data_t* d, const char* key, const char* value)
{
  if (!d || !key || !value) return false;
  if (strcmp(key, "device_sn") == 0) { strncpy(d->device_sn, value, sizeof(d->device_sn)-1U); d->device_sn[sizeof(d->device_sn)-1U]='\0'; return true; }
  if (strcmp(key, "head_type") == 0) { if (!headCalNormalizeTypeText(value, d->head_type_text, sizeof(d->head_type_text))) return false; int8_t p=headTypeProfileFromText(d->head_type_text); d->head_type=(p>=0)?(uint8_t)p:HEAD_TYPE_DEFAULT; return true; }
  if (strcmp(key, "head_sn") == 0) { d->head_serial=strtoul(value,nullptr,10); return true; }
  if (strcmp(key, "cal_date") == 0) { d->cal_date_yyyymmdd=(uint32_t)atoi(value)*10000UL + (uint32_t)atoi(value+5)*100UL + (uint32_t)atoi(value+8); return true; }
  if (strcmp(key, "cal_time") == 0) { d->cal_time_hhmmss=(uint32_t)atoi(value)*10000UL + (uint32_t)atoi(value+3)*100UL + (uint32_t)atoi(value+6); return true; }
  if (strcmp(key, "r0_mirror_scaled") == 0) { d->r0_mirror_scaled=atol(value); return true; }
  if (strcmp(key, "r0_ambient_scaled") == 0) { d->r0_ambient_scaled=atol(value); return true; }
  if (strcmp(key, "tau_offset_mC") == 0) { d->tau_offset_mC=atol(value); return true; }
  if (strcmp(key, "pid_kp") == 0) { d->pid_kp=(uint16_t)strtoul(value,nullptr,10); return true; }
  if (strcmp(key, "pid_ki_x1000") == 0) { d->pid_ki_x1000=atol(value); return true; }
  if (strcmp(key, "pid_kd_x1000") == 0) { d->pid_kd_x1000=atol(value); return true; }
  if (strcmp(key, "regler_intervall_ms") == 0) { d->regler_intervall_ms=(uint16_t)strtoul(value,nullptr,10); return true; }
  if (strcmp(key, "hbridge_deadtime_ms") == 0) { d->h_bruecke_totzeit_ms=(uint8_t)strtoul(value,nullptr,10); return true; }
  if (strcmp(key, "fan_percent") == 0) { d->fan_percent=(uint8_t)strtoul(value,nullptr,10); return true; }
  if (strcmp(key, "optical_target_x10") == 0) { d->optik_sollwert_x10=(uint16_t)strtoul(value,nullptr,10); return true; }
  if (strcmp(key, "peltier_current_limit_ma") == 0) { d->peltier_current_limit_ma=(uint16_t)strtoul(value,nullptr,10); return true; }

  const char* pfx = nullptr; uint8_t idx = 0;
  if (strncmp(key, "pt100_mirror_", 13) == 0) { pfx = key + 13; idx = 0; }
  else if (strncmp(key, "pt100_ambient_", 14) == 0) { pfx = key + 14; idx = 1; }
  if (pfx)
  {
    if (strcmp(pfx, "active") == 0) { d->p2_active[idx]=(uint8_t)strtoul(value,nullptr,10); return true; }
    if (strcmp(pfx, "soll1_mC") == 0) { d->p2_soll1_mC[idx]=atol(value); return true; }
    if (strcmp(pfx, "ist1_mC") == 0) { d->p2_ist1_mC[idx]=atol(value); return true; }
    if (strcmp(pfx, "soll2_mC") == 0) { d->p2_soll2_mC[idx]=atol(value); return true; }
    if (strcmp(pfx, "ist2_mC") == 0) { d->p2_ist2_mC[idx]=atol(value); return true; }
  }
  return true; // unbekannte Zusatzfelder ignorieren
}

static bool FLASHMEM headCalReadFile(const char* path, head_cal_data_t* d)
{
  if (!path || !d) return false;
  memset(d, 0, sizeof(*d));
  d->peltier_current_limit_ma = 0; // Pflichtfeld

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  uint32_t calc = 0UL;
  uint32_t stored = 0UL;
  bool gotCrc = false;
  bool gotMagic = false;
  bool trailingData = false;
  char line[HEADCAL_LINE_MAX];
  size_t pos = 0;

  while (f.available())
  {
    char c = (char)f.read();
    if (c == '\r') continue;
    if (c != '\n' && pos < sizeof(line) - 1U)
    {
      line[pos++] = c;
      continue;
    }

    line[pos] = '\0';
    pos = 0;

    if (gotCrc)
    {
      if (line[0] != '\0') trailingData = true;
      continue;
    }

    if (strncmp(line, "crc32=", 6) == 0)
    {
      stored = strtoul(line + 6, nullptr, 16);
      gotCrc = true;
      continue;
    }

    if (!gotMagic)
    {
      if (strcmp(line, "TP3000_HEAD_CAL_V1") != 0) { f.close(); return false; }
      gotMagic = true;
      headCalCrcLine(&calc, line);
      continue;
    }

    char* eq = strchr(line, '=');
    if (eq)
    {
      *eq = '\0';
      if (!headCalParseKv(d, line, eq + 1)) { f.close(); return false; }
      *eq = '=';
    }
    headCalCrcLine(&calc, line);
  }
  f.close();

  if (pos > 0) return false;
  if (!gotMagic || !gotCrc || trailingData) return false;
  if (calc != stored) return false;
  return headCalDataPlausible(d);
}

uint8_t FLASHMEM headCalListTypes(char types[][HEAD_TYPE_TEXT_LEN + 1U], uint8_t maxCount)
{
  if (!types || maxCount == 0) return 0;
  if (!headCalEnsureDir()) return 0;
  uint8_t count = 0;
  File dir = SD.open(HEADCAL_DIR);
  if (!dir || !dir.isDirectory()) return 0;
  File entry;
  while ((entry = dir.openNextFile()))
  {
    if (!entry.isDirectory())
    {
      char ht[HEAD_TYPE_TEXT_LEN + 1U] = {0}; uint32_t hs = 0, dt = 0, tm = 0; char dev[DEVICE_SERIAL_DIGITS+1U] = {0};
      if (headCalParseFileName(entry.name(), ht, sizeof(ht), &hs, dev, &dt, &tm) && strncmp(dev, deviceSerialGet(), DEVICE_SERIAL_DIGITS) == 0)
      {
        bool exists = false;
        for (uint8_t i=0;i<count;i++) if (strncmp(types[i], ht, HEAD_TYPE_TEXT_LEN) == 0) exists=true;
        if (!exists && count < maxCount)
        {
          const size_t typeLength = strnlen(ht, HEAD_TYPE_TEXT_LEN);
          memcpy(types[count], ht, typeLength);
          types[count][typeLength] = '\0';
          count++;
        }
      }
    }
    entry.close();
  }
  dir.close();
  return count;
}

uint8_t FLASHMEM headCalListSerials(const char* headTypeText, uint32_t* serials, uint8_t maxCount)
{
  if (!serials || maxCount == 0) return 0;
  char want[HEAD_TYPE_TEXT_LEN + 1U];
  if (!headCalNormalizeTypeText(headTypeText, want, sizeof(want))) return 0;
  if (!headCalEnsureDir()) return 0;
  uint8_t count = 0;
  File dir = SD.open(HEADCAL_DIR);
  if (!dir || !dir.isDirectory()) return 0;
  File entry;
  while ((entry = dir.openNextFile()))
  {
    if (!entry.isDirectory())
    {
      char ht[HEAD_TYPE_TEXT_LEN + 1U] = {0}; uint32_t hs = 0, dt = 0, tm = 0; char dev[DEVICE_SERIAL_DIGITS+1U] = {0};
      if (headCalParseFileName(entry.name(), ht, sizeof(ht), &hs, dev, &dt, &tm) && strncmp(ht, want, HEAD_TYPE_TEXT_LEN) == 0 && strncmp(dev, deviceSerialGet(), DEVICE_SERIAL_DIGITS) == 0)
      {
        bool exists = false;
        for (uint8_t i=0;i<count;i++) if (serials[i]==hs) exists=true;
        if (!exists && count < maxCount) serials[count++] = hs;
      }
    }
    entry.close();
  }
  dir.close();
  return count;
}

bool FLASHMEM headCalLoadLatest(const char* headTypeText, uint32_t headSerial, char* message, size_t messageSize)
{
  if (message && messageSize) message[0] = '\0';
  char want[HEAD_TYPE_TEXT_LEN + 1U];
  if (!headCalNormalizeTypeText(headTypeText, want, sizeof(want)) || headSerial > HEAD_SERIAL_MAX) { if (message) snprintf(message, messageSize, "Auswahl ungültig"); return false; }
  if (!headCalEnsureDir()) { if (message) snprintf(message, messageSize, "SD/HEADCAL Fehler"); return false; }

  char bestPath[HEADCAL_PATH_MAX] = {0};
  uint64_t bestStamp = 0ULL;
  head_cal_data_t bestData;
  memset(&bestData, 0, sizeof(bestData));

  File dir = SD.open(HEADCAL_DIR);
  if (!dir || !dir.isDirectory()) { if (message) snprintf(message, messageSize, "HEADCAL fehlt"); return false; }
  File entry;
  while ((entry = dir.openNextFile()))
  {
    if (!entry.isDirectory())
    {
      char ht[HEAD_TYPE_TEXT_LEN + 1U] = {0}; uint32_t hs = 0, dt = 0, tm = 0; char dev[DEVICE_SERIAL_DIGITS+1U] = {0};
      if (headCalParseFileName(entry.name(), ht, sizeof(ht), &hs, dev, &dt, &tm) && strncmp(ht, want, HEAD_TYPE_TEXT_LEN) == 0 && hs == headSerial && strncmp(dev, deviceSerialGet(), DEVICE_SERIAL_DIGITS) == 0)
      {
        char path[HEADCAL_PATH_MAX];
        snprintf(path, sizeof(path), HEADCAL_DIR "/%s", headCalBaseName(entry.name()));
        head_cal_data_t d;
        if (headCalReadFile(path, &d) && strncmp(d.head_type_text, want, HEAD_TYPE_TEXT_LEN) == 0 && d.head_serial == headSerial && strncmp(d.device_sn, deviceSerialGet(), DEVICE_SERIAL_DIGITS) == 0)
        {
          uint64_t stamp = (uint64_t)d.cal_date_yyyymmdd * 1000000ULL + (uint64_t)d.cal_time_hhmmss;
          if (stamp > bestStamp)
          {
            bestStamp = stamp;
            strncpy(bestPath, path, sizeof(bestPath)-1U);
            bestPath[sizeof(bestPath)-1U] = '\0';
            bestData = d;
          }
        }
      }
    }
    entry.close();
  }
  dir.close();

  if (bestStamp == 0ULL)
  {
    if (message) snprintf(message, messageSize, "Keine gültige Datei");
    return false;
  }

  if (!headCalApplyData(&bestData))
  {
    if (message) snprintf(message, messageSize, "Laden abgebrochen");
    return false;
  }

  if (message) snprintf(message, messageSize, "Geladen: %s K%05lu",
                      bestData.head_type_text,
                      (unsigned long)bestData.head_serial);
  return true;
}

bool FLASHMEM headCalStatusText(char* line1, size_t line1Size, char* line2, size_t line2Size)
{
  if (line1 && line1Size) line1[0] = '\0';
  if (line2 && line2Size) line2[0] = '\0';
  char types[HEADCAL_MAX_TYPES][HEAD_TYPE_TEXT_LEN + 1U];
  uint8_t n = headCalListTypes(types, HEADCAL_MAX_TYPES);
  if (line1) snprintf(line1, line1Size, "G%s  %s K%05lu", deviceSerialGet(), headTypeTextGet(), (unsigned long)R.head_serial);
  if (line2) snprintf(line2, line2Size, "%u Kopftyp(en) auf SD", (unsigned)n);
  return (n > 0);
}
