/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPwincontrolOut.ino
 * Zweck: ALMEMO-V6-Format-Ausgabe für WinControl / AMR Control.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
// TPwincontrolOut.ino
// Virtuelle Messbox fuer WinControl / AMR-Control:
// - gleicher ASCII-Kommandohandler fuer RS232 und raw TCP
// - keine HTTP-/Telnet-Schicht; Ethernet ist ein reiner TCP-Bytestream
// - Ausgabe bewusst auf das beobachtete ALMEMO-V6-Kommandoformat und die
//   Kommandos begrenzt
// =========================================================================

#include <Arduino.h>
#include <NativeEthernet.h>
#include <TimeLib.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <new>
#include "TP_T.h"

#define WINCONTROL_FLASHMEM_NOINLINE FLASHMEM __attribute__((noinline))

// Interface-Konfiguration aus TPmenu_Interface.ino
extern uint8_t interfaceRs232Mode(uint8_t port);
extern bool interfaceEthEnabled(void);
extern bool ethernetHasValidIp(void);
extern bool interfaceWinControlEthernetEnabled(void);
extern uint16_t interfaceWinControlTcpPort(void);
extern uint8_t interfaceWinControlAddress(void);
extern uint32_t interfaceWinControlCycleMs(void);

// Messwerte / Kopfnummer
extern float tempSpiegel;
extern float tempUmgebung;
extern float relativeFeuchte;
extern float präziserTaupunkt;
extern float baroDruckHPa;
extern double amp_avg;
extern float optikReflexion;
extern float optikTrockenReferenz;
extern var_t R;

// Zahlenwerte muessen zu TPmenu_Interface.ino / TPserial.ino passen.
#define WIN_RS232_MODE_WINCONTROL 6

#define WIN_TARGET_RS232_1 0
#define WIN_TARGET_RS232_2 1
#define WIN_TARGET_ETH     2
#define WIN_TARGET_COUNT   3

#define WIN_RX_BUF_LEN      96
#define WIN_TX_BUF_LEN    4096
#define WIN_TX_DRAIN_RS232 160
#define WIN_TX_DRAIN_ETH   512
#define WIN_ETH_RX_WORK     96
#define WIN_SER_RX_WORK     96

#define WIN_FMT_N0          0
#define WIN_FMT_N1          1
#define WIN_FMT_N2          2

#define WIN_PREFIX_NONE     0
#define WIN_PREFIX_F1       1
#define WIN_PREFIX_F2       2
#define WIN_PREFIX_F3       3
#define WIN_PREFIX_F4       4
#define WIN_PREFIX_F8       8

struct WinRuntime
{
  char rx[WIN_RX_BUF_LEN];
  uint8_t rxPos;
  uint32_t lastByteMs;

  char tx[WIN_TX_BUF_LEN];
  volatile uint16_t txHead;
  volatile uint16_t txTail;

  uint8_t outputFormat;
  uint8_t functionPrefix;
  uint8_t selectedChannel;
  bool cyclicActive;
  uint32_t lastCyclicMs;
  uint32_t lastConfigSeenMs;
};

static DMAMEM WinRuntime win_rt[WIN_TARGET_COUNT];

// Ethernet-Server als placement-new, damit beim Portwechsel kein Heap benutzt wird.
static DMAMEM uint64_t winEthServerStorage[(sizeof(EthernetServer) + sizeof(uint64_t) - 1U) / sizeof(uint64_t)];
static EthernetServer* winEthServer = nullptr;
static EthernetClient winEthClient;
static uint16_t winEthActivePort = 0;
static bool winEthConfigured = false;
static bool winEthLastEnabled = false;
static uint16_t winEthLastPort = 0;

struct WinChannelDef
{
  uint8_t ms;
  const char* range;
  const char* unit;
  const char* comment;
  uint8_t digits;
  uint8_t width;
};

static const WinChannelDef winChannels[] = {
  { 0, "P304", "\xB0" "C", "T-Spiegel", 3, 8 },
  { 1, "P304", "\xB0" "C", "T-Umg",     3, 8 },
  { 2, "P304", "\xB0" "C", "Taupunkt",  3, 8 },
  { 3, "P RH", "%H",      "rH",        2, 8 },
  { 4, "AP",   "mb",      "Druck",     2, 8 },
  { 5, "P%",   "%",       "Optik",     2, 8 },
  { 6, "mA",   "mA",      "Peltier",   1, 8 }
};
static const uint8_t WIN_CHANNEL_COUNT = sizeof(winChannels) / sizeof(winChannels[0]);

static inline uint16_t winTxNext(uint16_t v)
{
  return (uint16_t)((v + 1U) % WIN_TX_BUF_LEN);
}

static bool WINCONTROL_FLASHMEM_NOINLINE winTxEmpty(const WinRuntime& rt)
{
  return rt.txHead == rt.txTail;
}

static void WINCONTROL_FLASHMEM_NOINLINE winTxClear(WinRuntime& rt)
{
  rt.txHead = 0;
  rt.txTail = 0;
}

static bool WINCONTROL_FLASHMEM_NOINLINE winTxQueueChar(WinRuntime& rt, char c)
{
  uint16_t next = winTxNext(rt.txHead);
  if (next == rt.txTail) return false;
  rt.tx[rt.txHead] = c;
  rt.txHead = next;
  return true;
}

static bool WINCONTROL_FLASHMEM_NOINLINE winTxQueueText(WinRuntime& rt, const char* text)
{
  if (text == nullptr) return false;
  bool ok = true;
  while (*text)
  {
    if (!winTxQueueChar(rt, *text++)) ok = false;
  }
  return ok;
}

static bool WINCONTROL_FLASHMEM_NOINLINE winTxQueueFmt(WinRuntime& rt, const char* fmt, ...)
{
  char tmp[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  tmp[sizeof(tmp) - 1U] = '\0';
  return winTxQueueText(rt, tmp);
}

static void WINCONTROL_FLASHMEM_NOINLINE winTxQueueEtx(WinRuntime& rt)
{
  winTxQueueChar(rt, (char)0x03);
}

static void WINCONTROL_FLASHMEM_NOINLINE winAck(WinRuntime& rt, const char* cmd)
{
  if (cmd != nullptr && cmd[0] != '\0')
  {
    winTxQueueText(rt, cmd);
    winTxQueueText(rt, "\r\n");
  }
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winError(WinRuntime& rt)
{
  winTxQueueText(rt, "ERROR\r\n");
  winTxQueueEtx(rt);
}

static float WINCONTROL_FLASHMEM_NOINLINE winOptikPercent(void)
{
  if (isfinite(optikReflexion) && isfinite(optikTrockenReferenz) && optikTrockenReferenz > 1000.0f)
  {
    return (optikReflexion / optikTrockenReferenz) * 100.0f;
  }
  return NAN;
}

static float WINCONTROL_FLASHMEM_NOINLINE winChannelValue(uint8_t index)
{
  switch (index)
  {
    case 0: return tempSpiegel;
    case 1: return tempUmgebung;
    case 2: return präziserTaupunkt;
    case 3: return relativeFeuchte;
    case 4: return baroDruckHPa;
    case 5: return winOptikPercent();
    case 6: return (float)(amp_avg * 1000.0); // Ausgabe in mA
    default: return NAN;
  }
}

static void WINCONTROL_FLASHMEM_NOINLINE winFormatSigned(char* out, size_t outSize, float value, uint8_t digits, uint8_t width)
{
  if (out == nullptr || outSize == 0U) return;
  if (!isfinite(value))
  {
    snprintf(out, outSize, "---");
    return;
  }
  char fmt[12];
  snprintf(fmt, sizeof(fmt), "%%+0%u.%uf", (unsigned)width, (unsigned)digits);
  snprintf(out, outSize, fmt, (double)value);
}

static void WINCONTROL_FLASHMEM_NOINLINE winFormatPlain(char* out, size_t outSize, float value, uint8_t digits)
{
  if (out == nullptr || outSize == 0U) return;
  if (!isfinite(value))
  {
    snprintf(out, outSize, "-");
    return;
  }
  char fmt[12];
  snprintf(fmt, sizeof(fmt), "%%.%uf", (unsigned)digits);
  snprintf(out, outSize, fmt, (double)value);
}

static void WINCONTROL_FLASHMEM_NOINLINE winFormatCsvComma(char* out, size_t outSize, float value, uint8_t digits)
{
  winFormatPlain(out, outSize, value, digits);
  for (char* p = out; *p; ++p)
  {
    if (*p == '.') *p = ',';
  }
}

static void WINCONTROL_FLASHMEM_NOINLINE winDateText(char* out, size_t outSize)
{
  snprintf(out, outSize, "%02u.%02u.%02u",
           (unsigned)day(), (unsigned)month(), (unsigned)(year() % 100));
}

static void WINCONTROL_FLASHMEM_NOINLINE winTimeText(char* out, size_t outSize)
{
  snprintf(out, outSize, "%02u:%02u:%02u",
           (unsigned)hour(), (unsigned)minute(), (unsigned)second());
}

static void WINCONTROL_FLASHMEM_NOINLINE winCycleText(char* out, size_t outSize)
{
  uint32_t cyc = interfaceWinControlCycleMs() / 1000UL;
  if (cyc < 1UL) cyc = 1UL;
  uint32_t h = cyc / 3600UL;
  uint32_t m = (cyc / 60UL) % 60UL;
  uint32_t sec = cyc % 60UL;
  snprintf(out, outSize, "%02lu:%02lu:%02lu",
           (unsigned long)h, (unsigned long)m, (unsigned long)sec);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueP15Main(WinRuntime& rt)
{
  winTxQueueText(rt, "\r\nAMR ALMEMO 2590-4S\r\n");
  winTxQueueChar(rt, (char)0x0F);
  winTxQueueText(rt, "MS BER. GW-MAX GW-MIN BASIS D FAKTOR EXP MITTEL KOMMENTAR.\r\n");
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    const WinChannelDef& c = winChannels[i];
    winTxQueueFmt(rt, "%02u:%s - - - - - - - - - %s - - - E+0 - - - %s\r\n",
                  (unsigned)c.ms, c.range, c.unit, c.comment);
  }
  winTxQueueChar(rt, (char)0x12);
  winTxQueueText(rt, "MESSZYKLUS:  00:00:00  S0060.0 F0059.8 AR- W010 -----\r\n");
  char cycText[12];
  winCycleText(cycText, sizeof(cycText));
  winTxQueueFmt(rt, "DRUCKZYKLUS: %s Un 57.6 kbd CRC\r\n", cycText);
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueP15Extended(WinRuntime& rt)
{
  winTxQueueText(rt, "\r\nAMR ALMEMO 2590-4S\r\n");
  winTxQueueChar(rt, (char)0x0F);
  winTxQueueText(rt, "MS NULLPKT STEIGNG VM K FUNK EOFSET EFAKT ANA-ANF ANA-END B1 MX EF AH AL ZF UMIN\r\n");
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    const WinChannelDef& c = winChannels[i];
    uint8_t dig = c.digits;
    winTxQueueFmt(rt, "%02u: - - - - - - 5 %u Mess - - - - - - - - - - - - -- -- -- -- -- -- --\r\n",
                  (unsigned)c.ms, (unsigned)dig);
  }
  winTxQueueChar(rt, (char)0x12);
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueP15Dg(WinRuntime& rt)
{
  winTxQueueText(rt, "\r\nMS BER. GW-MAX GW-MIN BASIS D FAKTOR EXP MITTEL KOMMENTAR. DG QUERS RH RL\r\n");
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    const WinChannelDef& c = winChannels[i];
    winTxQueueFmt(rt, "%02u:%s - - - - - - - - - %s - - - E+0 - - - %s -- 00000. -- --\r\n",
                  (unsigned)c.ms, c.range, c.unit, c.comment);
  }
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueSensorTable(WinRuntime& rt)
{
  winTxQueueText(rt, "\r\nST SENSOR SERIENNR KAL-DAT. ZY\r\n");
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    const WinChannelDef& c = winChannels[i];
    winTxQueueFmt(rt, "%02u:TP3000 %-8s %08lu 00.00.00 -- \r\n",
                  (unsigned)c.ms, c.comment, (unsigned long)R.head_serial);
  }
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueP19(WinRuntime& rt)
{
  float vk = isfinite(tempUmgebung) ? tempUmgebung : 23.0f;
  float p = isfinite(baroDruckHPa) ? baroDruckHPa : 1013.0f;
  winTxQueueText(rt, "\r\n");
  winTxQueueFmt(rt, "GERAET: G%02u M20 A06 P10/40/00\r\n", (unsigned)interfaceWinControlAddress());
  winTxQueueFmt(rt, "LUFTDRUCK: %+07.0f. mb\r\n", (double)p);
  winTxQueueFmt(rt, "VK-TEMP: %+07.2f \xB0" "C\r\n", (double)vk);
  winTxQueueText(rt, "U-SENSOR: 12.1 V\r\n");
  winTxQueueText(rt, "HYSTERESE: 10\r\n");
  winTxQueueText(rt, "KONFIG: -CR-A--- ---- \r\n");
  winTxQueueText(rt, "ALARM: ----\r\n");
  winTxQueueText(rt, "A1: DK0 U\r\n");
  winTxQueueText(rt, "A2: --\r\n");
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueP20(WinRuntime& rt)
{
  winTxQueueText(rt, "U1:Datenlogger\r\n");
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    winTxQueueFmt(rt, "%02u:%02u\r\n", (unsigned)i, (unsigned)(50U + i));
  }
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueP18(WinRuntime& rt)
{
  char v[24];
  winTxQueueText(rt, "MS MESSWERT MAXWERT MINWERT MITTELW ANZAHL\r\n");
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    const WinChannelDef& c = winChannels[i];
    winFormatSigned(v, sizeof(v), winChannelValue(i), c.digits, c.width);
    winTxQueueFmt(rt, "%02u: %s - - - 00000.\r\n", (unsigned)c.ms, v);
  }
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueMeasurementN0(WinRuntime& rt, bool withDate)
{
  char d[12], t[12], v[24];
  winDateText(d, sizeof(d));
  winTimeText(t, sizeof(t));
  if (withDate)
  {
    winTxQueueFmt(rt, "DATUM:       %s\r\n", d);
  }
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    const WinChannelDef& c = winChannels[i];
    winFormatSigned(v, sizeof(v), winChannelValue(i), c.digits, c.width);
    if (i == 0)
    {
      winTxQueueFmt(rt, "%s %02u: %s %s %s %s\r\n", t, (unsigned)c.ms, v, c.unit, c.range, c.comment);
    }
    else
    {
      winTxQueueFmt(rt, "         %02u: %s %s %s %s\r\n", (unsigned)c.ms, v, c.unit, c.range, c.comment);
    }
  }
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueMeasurementN1(WinRuntime& rt, bool withDate)
{
  char d[12], t[12], v[24];
  winDateText(d, sizeof(d));
  winTimeText(t, sizeof(t));
  winTxQueueChar(rt, (char)0x0F);
  if (withDate)
  {
    winTxQueueFmt(rt, "DATUM:       %s\r\n", d);
    winTxQueueChar(rt, (char)0x0F);
  }
  winTxQueueText(rt, t);
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    const WinChannelDef& c = winChannels[i];
    winFormatSigned(v, sizeof(v), winChannelValue(i), c.digits, c.width);
    winTxQueueFmt(rt, " %02u: %s %s", (unsigned)c.ms, v, c.unit);
  }
  winTxQueueChar(rt, (char)0x12);
  winTxQueueText(rt, "\r\n");
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueMeasurementN2(WinRuntime& rt)
{
  char d[12], t[12], v[24];
  winDateText(d, sizeof(d));
  winTimeText(t, sizeof(t));
  winTxQueueFmt(rt, "\"%s\";\"%s\"", d, t);
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    const WinChannelDef& c = winChannels[i];
    winFormatCsvComma(v, sizeof(v), winChannelValue(i), c.digits);
    winTxQueueChar(rt, ';');
    winTxQueueText(rt, v);
  }
  winTxQueueText(rt, "\r\n");
  winTxQueueEtx(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueMeasurementFrame(WinRuntime& rt, bool withDate)
{
  if (rt.outputFormat == WIN_FMT_N2)
  {
    winQueueMeasurementN2(rt);
  }
  else if (rt.outputFormat == WIN_FMT_N1)
  {
    winQueueMeasurementN1(rt, withDate);
  }
  else
  {
    winQueueMeasurementN0(rt, withDate);
  }
}

static void WINCONTROL_FLASHMEM_NOINLINE winQueueSingleSelected(WinRuntime& rt)
{
  char v[24];
  uint8_t idx = 0;
  for (uint8_t i = 0; i < WIN_CHANNEL_COUNT; i++)
  {
    if (winChannels[i].ms == rt.selectedChannel)
    {
      idx = i;
      break;
    }
  }
  const WinChannelDef& c = winChannels[idx];
  winFormatSigned(v, sizeof(v), winChannelValue(idx), c.digits, c.width);
  winTxQueueFmt(rt, "%02u: %s %s %s %s\r\n", (unsigned)c.ms, v, c.unit, c.range, c.comment);
  winTxQueueEtx(rt);
}

static bool WINCONTROL_FLASHMEM_NOINLINE winParseNumber2(const char* s, uint8_t* out)
{
  if (s == nullptr || out == nullptr) return false;
  if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1])) return false;
  *out = (uint8_t)((s[0] - '0') * 10 + (s[1] - '0'));
  return true;
}

static void WINCONTROL_FLASHMEM_NOINLINE winHandleCommand(WinRuntime& rt, const char* rawCmd)
{
  if (rawCmd == nullptr) return;
  while (*rawCmd == ' ' || *rawCmd == '\t') rawCmd++;
  if (*rawCmd == '\0') return;

  char cmd[WIN_RX_BUF_LEN];
  uint8_t n = 0;
  while (rawCmd[n] != '\0' && n < (WIN_RX_BUF_LEN - 1U))
  {
    cmd[n] = rawCmd[n];
    n++;
  }
  cmd[n] = '\0';
  while (n > 0 && (cmd[n - 1U] == ' ' || cmd[n - 1U] == '\t'))
  {
    cmd[--n] = '\0';
  }
  if (n == 0) return;

  if (!strcmp(cmd, "X"))
  {
    rt.cyclicActive = false;
    winAck(rt, "X");
    return;
  }

  if (cmd[0] == 'G' && isdigit((unsigned char)cmd[1]) && isdigit((unsigned char)cmd[2]))
  {
    uint8_t addr = 0;
    if (winParseNumber2(cmd + 1, &addr) && addr == interfaceWinControlAddress())
    {
      winAck(rt, cmd);
    }
    // Andere Adressen bewusst ignorieren: WinControl darf scannen.
    return;
  }

  if (cmd[0] == 'M' && isdigit((unsigned char)cmd[1]) && isdigit((unsigned char)cmd[2]))
  {
    uint8_t ch = 0;
    if (winParseNumber2(cmd + 1, &ch)) rt.selectedChannel = ch;
    winAck(rt, cmd);
    return;
  }

  if (cmd[0] == 'N' && cmd[1] >= '0' && cmd[1] <= '9' && cmd[2] == '\0')
  {
    if (cmd[1] == '0')
    {
      rt.outputFormat = WIN_FMT_N0;
      winAck(rt, "N0");
    }
    else if (cmd[1] == '1')
    {
      rt.outputFormat = WIN_FMT_N1;
      winAck(rt, "N1");
    }
    else if (cmd[1] == '2')
    {
      rt.outputFormat = WIN_FMT_N2;
      winAck(rt, "N2");
    }
    else
    {
      winError(rt);
    }
    return;
  }

  if (cmd[0] == 'f')
  {
    if (!strcmp(cmd, "f1")) rt.functionPrefix = WIN_PREFIX_F1;
    else if (!strcmp(cmd, "f2")) rt.functionPrefix = WIN_PREFIX_F2;
    else if (!strcmp(cmd, "f3")) rt.functionPrefix = WIN_PREFIX_F3;
    else if (!strcmp(cmd, "f4")) rt.functionPrefix = WIN_PREFIX_F4;
    else if (!strcmp(cmd, "f8")) rt.functionPrefix = WIN_PREFIX_F8;
    else rt.functionPrefix = WIN_PREFIX_NONE;
    winAck(rt, cmd);
    return;
  }

  if (!strcmp(cmd, "t0"))
  {
    if (rt.functionPrefix == WIN_PREFIX_F2)
    {
      winTxQueueFmt(rt, "H%08lu\r\n", (unsigned long)R.head_serial);
    }
    else
    {
      // Geraetkennung bewusst wie im V6-Mitschnitt. Diese Zeile entscheidet
      // bei AMR-/WinControl oft darueber, ob die Messbox erkannt wird.
      winTxQueueText(rt, "A2590-4S 6.49\r\n");
    }
    rt.functionPrefix = WIN_PREFIX_NONE;
    winTxQueueEtx(rt);
    return;
  }

  if (!strcmp(cmd, "t5"))
  {
    winAck(rt, "t5");
    return;
  }

  if (!strcmp(cmd, "t6"))
  {
    winTxQueueText(rt, "S-ARLC-525-IW|4\r\n");
    winTxQueueEtx(rt);
    return;
  }

  if (!strcmp(cmd, "P05") || !strcmp(cmd, "P5"))
  {
    winTxQueueFmt(rt, "NUMMER: %06lu\r\n", (unsigned long)(R.head_serial % 1000000UL));
    winTxQueueEtx(rt);
    return;
  }

  if (!strcmp(cmd, "P10"))
  {
    char t[12];
    winTimeText(t, sizeof(t));
    winTxQueueFmt(rt, "UHRZEIT: %s\r\n", t);
    winTxQueueEtx(rt);
    return;
  }

  if (!strcmp(cmd, "P11"))
  {
    char cycText[12];
    winCycleText(cycText, sizeof(cycText));
    winTxQueueFmt(rt, "DRUCKZYKLUS: %s U s\r\n", cycText);
    winTxQueueEtx(rt);
    return;
  }

  if (!strcmp(cmd, "P13"))
  {
    if (rt.functionPrefix == WIN_PREFIX_F8)
    {
      winTxQueueText(rt, "KG:00.00.00\r\n");
    }
    else
    {
      char d[12];
      winDateText(d, sizeof(d));
      winTxQueueFmt(rt, "DATUM: %s\r\n", d);
    }
    rt.functionPrefix = WIN_PREFIX_NONE;
    winTxQueueEtx(rt);
    return;
  }

  if (!strcmp(cmd, "P15"))
  {
    if (rt.functionPrefix == WIN_PREFIX_F1) winQueueP15Extended(rt);
    else if (rt.functionPrefix == WIN_PREFIX_F3) winQueueP15Dg(rt);
    else if (rt.functionPrefix == WIN_PREFIX_F4) winQueueSensorTable(rt);
    else winQueueP15Main(rt);
    rt.functionPrefix = WIN_PREFIX_NONE;
    return;
  }

  if (!strcmp(cmd, "P18"))
  {
    winQueueP18(rt);
    return;
  }

  if (!strcmp(cmd, "P19"))
  {
    winQueueP19(rt);
    return;
  }

  if (!strcmp(cmd, "P20"))
  {
    winQueueP20(rt);
    return;
  }

  if (!strcmp(cmd, "P44"))
  {
    float comp = isfinite(tempUmgebung) ? tempUmgebung : 25.0f;
    winTxQueueFmt(rt, "KOMPENSATION: %+07.1f \xB0" "C\r\n", (double)comp);
    winTxQueueEtx(rt);
    return;
  }

  if (!strcmp(cmd, "S1"))
  {
    winQueueMeasurementFrame(rt, true);
    return;
  }

  if (!strcmp(cmd, "S2"))
  {
    rt.cyclicActive = true;
    rt.lastCyclicMs = millis();
    winQueueMeasurementFrame(rt, true);
    return;
  }

  if (!strcmp(cmd, "S3"))
  {
    rt.cyclicActive = true;
    rt.lastCyclicMs = millis();
    winQueueP15Main(rt);
    winQueueMeasurementFrame(rt, true);
    return;
  }

  if (!strcmp(cmd, "s") || !strcmp(cmd, "p"))
  {
    winQueueSingleSelected(rt);
    return;
  }

  winError(rt);
}

static void WINCONTROL_FLASHMEM_NOINLINE winReadByte(WinRuntime& rt, uint8_t raw)
{
  char c = (char)raw;
  rt.lastByteMs = millis();

  if (raw == 0x03U)
  {
    if (rt.rxPos > 0U)
    {
      rt.rx[rt.rxPos] = '\0';
      winHandleCommand(rt, rt.rx);
      rt.rxPos = 0U;
    }
    return;
  }

  if (c == '\r' || c == '\n')
  {
    if (rt.rxPos > 0U)
    {
      rt.rx[rt.rxPos] = '\0';
      winHandleCommand(rt, rt.rx);
      rt.rxPos = 0U;
    }
    return;
  }

  if (rt.rxPos < (WIN_RX_BUF_LEN - 1U))
  {
    rt.rx[rt.rxPos++] = c;
  }
  else
  {
    rt.rxPos = 0U;
  }
}

static void WINCONTROL_FLASHMEM_NOINLINE winCyclicTask(WinRuntime& rt)
{
  if (!rt.cyclicActive) return;
  if (!winTxEmpty(rt)) return;

  uint32_t nowMs = millis();
  uint32_t cycleMs = interfaceWinControlCycleMs();
  if (cycleMs < 1000UL) cycleMs = 1000UL;
  if (rt.lastCyclicMs == 0UL || (uint32_t)(nowMs - rt.lastCyclicMs) >= cycleMs)
  {
    rt.lastCyclicMs = nowMs;
    winQueueMeasurementFrame(rt, false);
  }
}

static void WINCONTROL_FLASHMEM_NOINLINE winResetRuntime(WinRuntime& rt)
{
  rt.rxPos = 0;
  rt.lastByteMs = 0;
  winTxClear(rt);
  rt.outputFormat = WIN_FMT_N0;
  rt.functionPrefix = WIN_PREFIX_NONE;
  rt.selectedChannel = 0;
  rt.cyclicActive = false;
  rt.lastCyclicMs = 0;
  rt.lastConfigSeenMs = 0;
}

void winControlOutBegin(void)
{
  for (uint8_t i = 0; i < WIN_TARGET_COUNT; i++)
  {
    winResetRuntime(win_rt[i]);
  }
  winEthClient = EthernetClient();
  winEthConfigured = false;
  winEthLastEnabled = false;
  winEthLastPort = 0;
}

void winControlOutSerialTask(uint8_t port, HardwareSerial& io)
{
  if (port > 1) return;
  WinRuntime& rt = win_rt[port];
  uint8_t work = 0;

  while (io.available() > 0 && work < WIN_SER_RX_WORK)
  {
    work++;
    winReadByte(rt, (uint8_t)io.read());
  }

  if (rt.rxPos > 0U && (uint32_t)(millis() - rt.lastByteMs) > 1000UL)
  {
    rt.rxPos = 0U;
  }

  winCyclicTask(rt);

  uint16_t drained = 0;
  while (!winTxEmpty(rt) && drained < WIN_TX_DRAIN_RS232)
  {
    int room = io.availableForWrite();
    if (room <= 0) break;
    io.write((uint8_t)rt.tx[rt.txTail]);
    rt.txTail = winTxNext(rt.txTail);
    drained++;
  }
}

static void WINCONTROL_FLASHMEM_NOINLINE winEthDestroyServer(void)
{
  if (winEthClient) winEthClient.stop();
  winEthClient = EthernetClient();
  if (winEthServer != nullptr)
  {
    winEthServer->~EthernetServer();
    winEthServer = nullptr;
  }
  winEthConfigured = false;
}

static void WINCONTROL_FLASHMEM_NOINLINE winEthConfigTask(void)
{
  bool enabled = interfaceEthEnabled() && interfaceWinControlEthernetEnabled();
  uint16_t port = interfaceWinControlTcpPort();
  if (port < 1U) port = 10001U;

  if (enabled == winEthLastEnabled && port == winEthLastPort && winEthConfigured)
  {
    return;
  }

  winEthLastEnabled = enabled;
  winEthLastPort = port;
  winEthDestroyServer();
  winResetRuntime(win_rt[WIN_TARGET_ETH]);

  if (!enabled) return;

  void* storage = static_cast<void*>(winEthServerStorage);
  winEthServer = new (storage) EthernetServer(port);
  winEthServer->begin();
  winEthActivePort = port;
  winEthConfigured = true;
}

void winControlOutEthernetTask(void)
{
  winEthConfigTask();
  if (!winEthConfigured || winEthServer == nullptr) return;
  if (!interfaceEthEnabled() || !interfaceWinControlEthernetEnabled() || !ethernetHasValidIp()) return;

  WinRuntime& rt = win_rt[WIN_TARGET_ETH];

  if (!winEthClient || !winEthClient.connected())
  {
    if (winEthClient) winEthClient.stop();
    winEthClient = EthernetClient();

    EthernetClient c = winEthServer->available();
    if (c)
    {
      winEthClient = c;
      winResetRuntime(rt);
    }
    else
    {
      return;
    }
  }

  uint8_t work = 0;
  while (winEthClient.available() > 0 && work < WIN_ETH_RX_WORK)
  {
    work++;
    winReadByte(rt, (uint8_t)winEthClient.read());
  }

  if (rt.rxPos > 0U && (uint32_t)(millis() - rt.lastByteMs) > 1000UL)
  {
    rt.rxPos = 0U;
  }

  winCyclicTask(rt);

  uint16_t drained = 0;
  while (!winTxEmpty(rt) && drained < WIN_TX_DRAIN_ETH && winEthClient.connected())
  {
    int room = winEthClient.availableForWrite();
    if (room <= 0) break;
    winEthClient.write((uint8_t)rt.tx[rt.txTail]);
    rt.txTail = winTxNext(rt.txTail);
    drained++;
  }
}

bool winControlOutEthernetClientConnected(void)
{
  return winEthClient && winEthClient.connected();
}

uint16_t winControlOutEthernetActivePort(void)
{
  return winEthActivePort;
}
