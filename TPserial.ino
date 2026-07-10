/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPserial.ino
 * Zweck: RS232-Protokoll und CSV-Ausgabe.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
// TPserial.ino
// RS232-Protokoll fuer Serial7 / Serial8
// - PC lesbar / PC CSV: zyklisch, auf Anfrage oder beides
// - Durchfluss Eingang: passiv oder per einfachem Polling
// - RX und TX non-blocking: keine readString()/parseFloat(), keine langen print()-Bursts
// =========================================================================

#include <Arduino.h>
#include <TimeLib.h>
#include <SD.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "TP_T.h"

// Text-/CSV-Renderer, Parser und Kommandohilfen sind nicht zeitkritisch.
// Sie laufen aus Flash, damit Loop/Regelung/Safety mehr ITCM-Reserve behalten.
#define SERIAL_FLASHMEM_NOINLINE FLASHMEM __attribute__((noinline))

// Interface-Konfiguration aus TPmenu_Interface.ino
extern uint8_t interfaceRs232Mode(uint8_t port);
extern uint8_t interfaceRs232OutputMode(uint8_t port);
extern uint32_t interfaceRs232OutputIntervalMsValue(uint8_t port);
extern uint8_t interfaceOutputFilterIndex(void);
extern bool outputDataGetSample(uint8_t filterIndex, output_data_sample_t* out);
extern uint32_t outputDataRawSequence(void);
extern bool interfaceFlowDisplayEnabled(void);
extern const char* interfaceFlowDisplayUnitText(void);
extern bool interfaceTExternalDisplayEnabled(void);
extern bool interfaceAlmemoChannelEnabled(uint8_t index);
extern bool interfaceAlmemoAnyChannelEnabled(void);
extern uint8_t interfaceAlmemoAddress(void);
extern uint8_t interfaceAlmemoChannel(void);
extern uint8_t interfaceAlmemoAddressFor(uint8_t index);
extern uint8_t interfaceAlmemoChannelFor(uint8_t index);
extern uint8_t interfaceAlmemoRole(uint8_t index);
extern const char* interfaceAlmemoDisplayLabel(uint8_t index);
extern const char* interfaceAlmemoDisplayUnitShort(uint8_t index);
extern const char* interfaceAlmemoCsvUnitText(uint8_t index);
extern uint8_t interfaceAlmemoIntervalIndex(void);
extern uint32_t interfaceAlmemoIntervalMsValue(void);
extern bool interfaceAlmemoTraceEnabled(void);
extern int8_t interfaceAlmemoActivePort(void);
extern bool sdLogReadyForLogging(void);
extern bool sdLogEnsureReadyForAccess(void);
extern void sdStorageDiagBegin(uint8_t stage);
extern void sdStorageDiagEnd(uint8_t stage);
extern void winControlOutSerialTask(uint8_t port, HardwareSerial& io);

// Messwerte aus Regelung / Anzeige
extern float tempSpiegel;
extern float tempUmgebung;
extern float relativeFeuchte;
extern float präziserTaupunkt;
extern float baroDruckHPa;
extern double amp_avg;
extern float optikReflexion;
extern float optikTrockenReferenz;
extern uint8_t aktuellerModus;
extern uint8_t ablaufStatus;

// Gleiche Zahlenwerte wie in TPmenu_Interface.ino, bewusst lokal gehalten,
// damit diese Datei auch bei anderer INO-Reihenfolge sicher kompiliert.
#define SER_RS232_MODE_OFF        0
#define SER_RS232_MODE_PC_TEXT    1
#define SER_RS232_MODE_PC_CSV     2
#define SER_RS232_MODE_FLOW_IN    3
#define SER_RS232_MODE_FLOW_POLL  4
#define SER_RS232_MODE_ALMEMO     5
#define SER_RS232_MODE_WINCONTROL 6

#define SER_RS232_OUT_CYCLIC      0
#define SER_RS232_OUT_REQUEST     1
#define SER_RS232_OUT_BOTH        2

#define SER_ALMEMO_CHANNEL_COUNT   2
#define SER_ALMEMO_ROLE_TEMP       1
#define SER_ALMEMO_ROLE_FLOW       2
#define SER_ALMEMO_ROLE_PRESSURE   3
#define SER_ALMEMO_ROLE_GENERAL    4

#define SER_LINE_BUF_LEN          96
#define SER_TX_BUF_LEN            640
#define SER_RENDER_BUF_LEN        384
#define SER_FLOW_TIMEOUT_MS       5000UL
#define SER_FLOW_POLL_MS          1000UL
#define SER_ALMEMO_REPLY_TIMEOUT_MS  800UL
#define SER_ALMEMO_REINIT_ERRORS    3
#define SER_ALMEMO_INIT_GAP_MS     1000UL
#define SER_ALMEMO_PROTO_UNKNOWN     0
#define SER_ALMEMO_PROTO_V5V6        1
#define SER_ALMEMO_PROTO_V7          2
#define SER_ALMEMO_DETECT_TIMEOUT_MS 800UL
#define SER_RX_WORK_LIMIT         64
#define SER_TX_WORK_LIMIT         96

// Temporäre Rohprotokollierung für die ALMEMO-Fehlersuche.
// Die Datei wird beim ersten aktiven ALMEMO-Port als
// /LOG/SERIAL001.TXT, SERIAL002.TXT, ... angelegt.
#define SER_TRACE_QUEUE_LEN        4096
#define SER_TRACE_RX_TEXT_LEN       256
#define SER_TRACE_WRITE_CHUNK       512
#define SER_TRACE_RX_IDLE_MS        100UL
#define SER_TRACE_FLUSH_MS         5000UL

struct Rs232Runtime
{
  char line[SER_LINE_BUF_LEN];
  uint8_t pos;
  uint32_t lastByteMs;
  uint32_t lastOutputMs;
  uint32_t lastOutputSeq;
  uint32_t lastPollMs;

  uint8_t lastMode;
  uint8_t almemoInitStep;
  bool almemoInitialized;
  bool almemoRequestPending;
  bool almemoDetectPending;
  uint8_t almemoProtocol;
  uint32_t almemoNextActionMs;
  uint32_t almemoRequestMs;
  uint32_t almemoDetectMs;
  uint32_t almemoLastSeq;
  uint8_t almemoConfigAddress;
  uint8_t almemoConfigChannel;
  uint8_t almemoConfigInterval;
  uint8_t almemoConsecutiveErrors;
  uint8_t almemoPollStep;
  bool almemoPollActive;
  uint8_t almemoConfigMask;
  uint8_t almemoConfigAddress2;
  uint8_t almemoConfigChannel2;
  uint8_t almemoConfigRole[2];
  uint8_t almemoPollListIndex;
  uint8_t almemoPendingIndex;

  char tx[SER_TX_BUF_LEN];
  volatile uint16_t txHead;
  volatile uint16_t txTail;
};

// RX/TX-Puffer der RS232-Ports liegen in RAM2.
// Das entlastet RAM1, ohne die non-blocking Arbeitsweise zu aendern.
static DMAMEM Rs232Runtime rs232_rt[2];

static float rs232_flow_l_min[2] = {NAN, NAN};
static uint32_t rs232_flow_last_ms[2] = {0, 0};
static bool rs232_flow_valid[2] = {false, false};

static float rs232_almemo_temp_c[2] = {NAN, NAN};
static uint32_t rs232_almemo_last_ms[2] = {0, 0};
static bool rs232_almemo_valid[2] = {false, false};
static uint32_t rs232_almemo_rx_errors[2] = {0, 0};

// ALMEMO-Rohprotokoll. TX-Kommandos und alle empfangenen Bytes werden
// zeitlich geordnet in einen RAM2-Ringpuffer geschrieben. Die SD-Karte
// wird erst anschließend in kleinen Blöcken bedient, damit kein einzelnes
// RX-Byte durch unmittelbare Datei-I/O verloren geht.
static DMAMEM char serial_trace_queue[SER_TRACE_QUEUE_LEN];
static volatile uint16_t serial_trace_head = 0;
static volatile uint16_t serial_trace_tail = 0;
static uint32_t serial_trace_dropped = 0;
static bool serial_trace_init_attempted = false;
static bool serial_trace_ready = false;
static bool serial_trace_capture_enabled = false;
static bool serial_trace_error = false;
static uint32_t serial_trace_last_blink_ms = 0;
static File serial_trace_file;
static char serial_trace_filename[32] = {0};
static uint32_t serial_trace_last_flush_ms = 0;

static bool SERIAL_FLASHMEM_NOINLINE serialTraceBuildFatDateTime(DateTimeFields& tm)
{
  int y = year();
  if (y < 2020 || y > 2099) return false;

  uint8_t mo = (uint8_t)month();
  uint8_t d  = (uint8_t)day();
  uint8_t h  = (uint8_t)hour();
  uint8_t mi = (uint8_t)minute();
  uint8_t se = (uint8_t)second();

  if (mo < 1U || mo > 12U) return false;
  if (d < 1U || d > 31U) return false;
  if (h > 23U || mi > 59U || se > 59U) return false;

  tm.year = y - 1900;
  tm.mon  = mo - 1U;
  tm.mday = d;
  tm.hour = h;
  tm.min  = mi;
  tm.sec  = se;
  return true;
}

static bool SERIAL_FLASHMEM_NOINLINE serialTraceFatDateTimeLooksInvalid(const DateTimeFields& tm)
{
  int y = (int)tm.year + 1900;
  if (y < 2020 || y > 2099) return true;
  if (tm.mon > 11U) return true;
  if (tm.mday < 1U || tm.mday > 31U) return true;
  if (tm.hour > 23U || tm.min > 59U || tm.sec > 59U) return true;
  return false;
}

static void SERIAL_FLASHMEM_NOINLINE serialTraceApplyFatTimestamp(File& f, bool fileWasNew)
{
  DateTimeFields tm = {};
  if (!serialTraceBuildFatDateTime(tm)) return;

  DateTimeFields createTm = {};
  bool setCreate = fileWasNew || !f.getCreateTime(createTm) || serialTraceFatDateTimeLooksInvalid(createTm);
  if (setCreate)
  {
    f.setCreateTime(tm);
  }

  f.setModifyTime(tm);
}

static DMAMEM char serial_trace_rx_text[2][SER_TRACE_RX_TEXT_LEN];
static uint16_t serial_trace_rx_len[2] = {0, 0};
static uint32_t serial_trace_rx_last_byte_ms[2] = {0, 0};

class SerialRenderBuffer : public Print
{
public:
  SerialRenderBuffer(char* buffer, size_t capacity)
  : buf(buffer), cap(capacity), pos(0)
  {
    if (buf && cap) buf[0] = '\0';
  }

  size_t write(uint8_t c) override
  {
    if (buf && cap && pos < cap - 1)
    {
      buf[pos++] = (char)c;
      buf[pos] = '\0';
    }
    return 1;
  }

  size_t write(const uint8_t* data, size_t len) override
  {
    size_t written = 0;
    while (len--)
    {
      write(*data++);
      written++;
    }
    return written;
  }

private:
  char* buf;
  size_t cap;
  size_t pos;
};

static uint16_t serialTraceQueueUsed(void)
{
  uint16_t head = serial_trace_head;
  uint16_t tail = serial_trace_tail;
  if (head >= tail) return head - tail;
  return SER_TRACE_QUEUE_LEN - tail + head;
}

static uint16_t serialTraceQueueFree(void)
{
  return (SER_TRACE_QUEUE_LEN - 1U) - serialTraceQueueUsed();
}

static bool serialTraceQueueChar(char c)
{
  uint16_t next = (uint16_t)((serial_trace_head + 1U) % SER_TRACE_QUEUE_LEN);
  if (next == serial_trace_tail)
  {
    serial_trace_dropped++;
    return false;
  }
  serial_trace_queue[serial_trace_head] = c;
  serial_trace_head = next;
  return true;
}

static bool serialTraceQueueText(const char* text)
{
  if (text == nullptr) return false;
  size_t len = strlen(text);
  if (len > serialTraceQueueFree())
  {
    serial_trace_dropped += (uint32_t)len;
    return false;
  }
  while (*text)
  {
    if (!serialTraceQueueChar(*text++)) return false;
  }
  return true;
}

static void serialTraceAppendEscapedByte(char* out, size_t outSize, size_t* pos, uint8_t value)
{
  if (out == nullptr || pos == nullptr || outSize == 0) return;

  const char* token = nullptr;
  char one[2] = {0, 0};
  char hexText[5] = {0, 0, 0, 0, 0};

  if (value == '\r') token = "<CR>";
  else if (value == '\n') token = "<LF>";
  else if (value >= 32U && value <= 126U)
  {
    one[0] = (char)value;
    token = one;
  }
  else
  {
    static const char hex[] = "0123456789ABCDEF";
    hexText[0] = '<';
    hexText[1] = hex[(value >> 4) & 0x0F];
    hexText[2] = hex[value & 0x0F];
    hexText[3] = '>';
    token = hexText;
  }

  while (*token && *pos < outSize - 1U)
  {
    out[(*pos)++] = *token++;
  }
  out[*pos] = '\0';
}

static void serialTraceQueueRecord(uint8_t port, const char* direction, const char* data)
{
  if (!serial_trace_capture_enabled) return;
  if (port > 1 || direction == nullptr || data == nullptr) return;

  char line[384];
  uint32_t nowMs = millis();
  uint16_t y = (uint16_t)year();
  bool timeValid = (y >= 2020U && y <= 2099U);

  if (timeValid)
  {
    snprintf(line, sizeof(line),
             "%04u-%02u-%02u %02u:%02u:%02u.%03lu;%010lu;RS232-%u;%s;%s\r\n",
             (unsigned)y, (unsigned)month(), (unsigned)day(),
             (unsigned)hour(), (unsigned)minute(), (unsigned)second(),
             (unsigned long)(nowMs % 1000UL),
             (unsigned long)nowMs,
             (unsigned)(port + 1U), direction, data);
  }
  else
  {
    snprintf(line, sizeof(line),
             "---- -- -- --:--:--.---;%010lu;RS232-%u;%s;%s\r\n",
             (unsigned long)nowMs,
             (unsigned)(port + 1U), direction, data);
  }
  serialTraceQueueText(line);
}

static void serialTraceTx(uint8_t port, const char* text)
{
  if (!serial_trace_capture_enabled) return;
  if (port > 1 || text == nullptr) return;
  if (interfaceRs232Mode(port) != SER_RS232_MODE_ALMEMO) return;

  char escaped[96];
  size_t pos = 0;
  escaped[0] = '\0';
  const uint8_t* p = (const uint8_t*)text;
  while (*p && pos < sizeof(escaped) - 1U)
  {
    serialTraceAppendEscapedByte(escaped, sizeof(escaped), &pos, *p++);
  }
  serialTraceQueueRecord(port, "TX", escaped);
}

static void serialTraceRxFlush(uint8_t port)
{
  if (port > 1 || serial_trace_rx_len[port] == 0) return;
  serial_trace_rx_text[port][serial_trace_rx_len[port]] = '\0';
  serialTraceQueueRecord(port, "RX", serial_trace_rx_text[port]);
  serial_trace_rx_len[port] = 0;
  serial_trace_rx_text[port][0] = '\0';
  serial_trace_rx_last_byte_ms[port] = 0;
}

static void serialTraceRxByte(uint8_t port, uint8_t value, uint32_t nowMs)
{
  if (!serial_trace_capture_enabled) return;
  if (port > 1 || interfaceRs232Mode(port) != SER_RS232_MODE_ALMEMO) return;

  // Eine neue Antwort nach Leitungspause getrennt protokollieren.
  if (serial_trace_rx_len[port] > 0 && serial_trace_rx_last_byte_ms[port] != 0 &&
      (uint32_t)(nowMs - serial_trace_rx_last_byte_ms[port]) > SER_TRACE_RX_IDLE_MS)
  {
    serialTraceRxFlush(port);
  }

  char encoded[5];
  size_t encPos = 0;
  encoded[0] = '\0';
  serialTraceAppendEscapedByte(encoded, sizeof(encoded), &encPos, value);

  size_t encLen = strlen(encoded);
  if ((size_t)serial_trace_rx_len[port] + encLen >= SER_TRACE_RX_TEXT_LEN)
  {
    serialTraceRxFlush(port);
  }

  for (size_t i = 0; i < encLen && serial_trace_rx_len[port] < SER_TRACE_RX_TEXT_LEN - 1U; i++)
  {
    serial_trace_rx_text[port][serial_trace_rx_len[port]++] = encoded[i];
  }
  serial_trace_rx_text[port][serial_trace_rx_len[port]] = '\0';
  serial_trace_rx_last_byte_ms[port] = nowMs;

  // LF beendet normalerweise eine komplette ALMEMO-Antwort. Dadurch steht
  // im Log direkt nach dem TX genau der zugehörige RX-Datensatz.
  if (value == '\n') serialTraceRxFlush(port);
}

static bool serialTraceEnsureReady(void)
{
  if (serial_trace_ready) return true;
  if (serial_trace_init_attempted) return false;
  serial_trace_init_attempted = true;

  // SD.begin() wird ausschließlich in setup() oder im bewusst bedienten
  // Menuepfad ausgefuehrt. Der schnelle Protokolltask darf niemals selbst
  // eine potenziell sekundenlang blockierende Karteninitialisierung starten.
  if (!sdLogReadyForLogging()) return false;

  sdStorageDiagBegin(SD_DIAG_SERIAL_PREPARE);
  bool logDirOk = SD.exists("/LOG") || SD.mkdir("/LOG");
  if (logDirOk)
  {
    for (uint16_t index = 1; index <= 999; index++)
    {
      snprintf(serial_trace_filename, sizeof(serial_trace_filename),
               "/LOG/SERIAL%03u.TXT", (unsigned)index);
      if (!SD.exists(serial_trace_filename)) break;
      serial_trace_filename[0] = '\0';
    }
  }
  sdStorageDiagEnd(SD_DIAG_SERIAL_PREPARE);
  if (!logDirOk || serial_trace_filename[0] == '\0') return false;

  sdStorageDiagBegin(SD_DIAG_SERIAL_OPEN);
  serial_trace_file = SD.open(serial_trace_filename, FILE_WRITE);
  sdStorageDiagEnd(SD_DIAG_SERIAL_OPEN);
  if (!serial_trace_file) return false;

  sdStorageDiagBegin(SD_DIAG_SERIAL_HEADER);
  serial_trace_file.println("TP-3000 ALMEMO SERIAL TRACE");
  serial_trace_file.println("Steuerzeichen: <CR>, <LF>, sonstige Bytes: <HH>");
  serial_trace_file.println("Datum Zeit;Millis;Port;Richtung;Daten");
  sdStorageDiagEnd(SD_DIAG_SERIAL_HEADER);

  sdStorageDiagBegin(SD_DIAG_SERIAL_FLUSH);
  serialTraceApplyFatTimestamp(serial_trace_file, true);
  serial_trace_file.flush();
  sdStorageDiagEnd(SD_DIAG_SERIAL_FLUSH);
  serial_trace_last_flush_ms = millis();
  serial_trace_last_blink_ms = serial_trace_last_flush_ms;
  serial_trace_error = false;
  serial_trace_ready = true;
  return true;
}

static size_t serialTraceWriteOneChunk(void)
{
  if (!serial_trace_ready) return 0;
  uint16_t available = serialTraceQueueUsed();
  if (available == 0) return 0;

  uint16_t contiguous = (serial_trace_head >= serial_trace_tail)
                      ? (uint16_t)(serial_trace_head - serial_trace_tail)
                      : (uint16_t)(SER_TRACE_QUEUE_LEN - serial_trace_tail);
  uint16_t count = contiguous;
  if (count > SER_TRACE_WRITE_CHUNK) count = SER_TRACE_WRITE_CHUNK;

  sdStorageDiagBegin(SD_DIAG_SERIAL_WRITE);
  size_t written = serial_trace_file.write(
      (const uint8_t*)&serial_trace_queue[serial_trace_tail], count);
  sdStorageDiagEnd(SD_DIAG_SERIAL_WRITE);
  serial_trace_tail = (uint16_t)((serial_trace_tail + written) % SER_TRACE_QUEUE_LEN);
  return written;
}

static void serialTraceCloseNow(void)
{
  // Bereits begonnene RX-Antworten noch vollständig als Datensatz sichern.
  for (uint8_t port = 0; port < 2; port++) serialTraceRxFlush(port);
  serial_trace_capture_enabled = false;

  if (serial_trace_ready)
  {
    if (serial_trace_dropped > 0)
    {
      char warning[96];
      snprintf(warning, sizeof(warning),
               "%010lu;TRACE;WARN;%lu Zeichen wegen vollem RAM-Puffer verworfen\r\n",
               (unsigned long)millis(), (unsigned long)serial_trace_dropped);
      serial_trace_dropped = 0;
      // Beim Schließen direkt in den noch vorhandenen Puffer schreiben.
      serialTraceQueueText(warning);
    }

    while (serialTraceQueueUsed() > 0)
    {
      if (serialTraceWriteOneChunk() == 0) break;
    }
    sdStorageDiagBegin(SD_DIAG_SERIAL_FLUSH);
    serialTraceApplyFatTimestamp(serial_trace_file, false);
    serial_trace_file.flush();
    sdStorageDiagEnd(SD_DIAG_SERIAL_FLUSH);

    sdStorageDiagBegin(SD_DIAG_SERIAL_CLOSE);
    serial_trace_file.close();
    sdStorageDiagEnd(SD_DIAG_SERIAL_CLOSE);
  }

  serial_trace_ready = false;
  serial_trace_init_attempted = false;
  serial_trace_error = false;
  serial_trace_last_flush_ms = 0;
  serial_trace_last_blink_ms = 0;
  serial_trace_head = 0;
  serial_trace_tail = 0;
  serial_trace_dropped = 0;
  for (uint8_t port = 0; port < 2; port++)
  {
    serial_trace_rx_len[port] = 0;
    serial_trace_rx_text[port][0] = '\0';
    serial_trace_rx_last_byte_ms[port] = 0;
  }
}

static bool serialTraceStartNow(void)
{
  if (serial_trace_ready)
  {
    serial_trace_capture_enabled = true;
    return true;
  }

  serial_trace_head = 0;
  serial_trace_tail = 0;
  serial_trace_dropped = 0;
  serial_trace_init_attempted = false;
  serial_trace_error = false;
  serial_trace_last_blink_ms = 0;
  serial_trace_filename[0] = '\0';
  for (uint8_t port = 0; port < 2; port++)
  {
    serial_trace_rx_len[port] = 0;
    serial_trace_rx_text[port][0] = '\0';
    serial_trace_rx_last_byte_ms[port] = 0;
  }

  if (!serialTraceEnsureReady())
  {
    serial_trace_capture_enabled = false;
    serial_trace_error = true;
    return false;
  }

  serial_trace_capture_enabled = true;
  return true;
}

static void serialTraceCaptureTask(void)
{
  uint32_t nowMs = millis();
  for (uint8_t port = 0; port < 2; port++)
  {
    if (serial_trace_rx_len[port] > 0 && serial_trace_rx_last_byte_ms[port] != 0 &&
        (uint32_t)(nowMs - serial_trace_rx_last_byte_ms[port]) > SER_TRACE_RX_IDLE_MS)
    {
      serialTraceRxFlush(port);
    }
  }
}

void serialProtocolTraceStorageTask(void)
{
  // Bewusst erst nach sdLogTask() aufrufen: Das normale Messwert-CSV hat
  // immer Vorrang. Der schnelle RS232-Task sammelt nur in RAM2 und fuehrt
  // keinerlei SD-Dateizugriff aus.
  if (!serial_trace_ready) return;

  uint32_t nowMs = millis();
  if (serialTraceQueueUsed() > 0)
  {
    serialTraceWriteOneChunk();
  }

  if ((uint32_t)(nowMs - serial_trace_last_flush_ms) >= SER_TRACE_FLUSH_MS)
  {
    if (serial_trace_dropped > 0)
    {
      char warning[96];
      snprintf(warning, sizeof(warning),
               "%010lu;TRACE;WARN;%lu Zeichen wegen vollem RAM-Puffer verworfen\r\n",
               (unsigned long)nowMs, (unsigned long)serial_trace_dropped);
      serial_trace_dropped = 0;
      serialTraceQueueText(warning);
    }
    sdStorageDiagBegin(SD_DIAG_SERIAL_FLUSH);
    serialTraceApplyFatTimestamp(serial_trace_file, false);
    serial_trace_file.flush();
    sdStorageDiagEnd(SD_DIAG_SERIAL_FLUSH);
    serial_trace_last_flush_ms = nowMs;
    serial_trace_last_blink_ms = nowMs;
  }
}

static uint16_t serialTxUsed(uint8_t port)
{
  if (port > 1) return 0;
  uint16_t head = rs232_rt[port].txHead;
  uint16_t tail = rs232_rt[port].txTail;
  if (head >= tail) return head - tail;
  return SER_TX_BUF_LEN - tail + head;
}

static uint16_t serialTxFree(uint8_t port)
{
  if (port > 1) return 0;
  return (SER_TX_BUF_LEN - 1) - serialTxUsed(port);
}

static void serialTxClear(uint8_t port)
{
  if (port > 1) return;
  rs232_rt[port].txHead = 0;
  rs232_rt[port].txTail = 0;
}

static bool serialTxQueueChar(uint8_t port, char c)
{
  if (port > 1) return false;
  Rs232Runtime& rt = rs232_rt[port];
  uint16_t next = (uint16_t)((rt.txHead + 1) % SER_TX_BUF_LEN);
  if (next == rt.txTail) return false;
  rt.tx[rt.txHead] = c;
  rt.txHead = next;
  return true;
}

static bool serialTxQueueText(uint8_t port, const char* s)
{
  if (port > 1 || s == nullptr) return false;

  size_t len = strlen(s);
  if (len > serialTxFree(port))
  {
    // Wenn der PC nicht schnell genug liest, keine Loop blockieren.
    // Neue Antwort verwerfen, alte Daten werden noch sauber ausgesendet.
    return false;
  }

  while (*s)
  {
    if (!serialTxQueueChar(port, *s++)) return false;
  }
  return true;
}

static void serialTxDrainPort(uint8_t port, HardwareSerial& out)
{
  if (port > 1) return;
  Rs232Runtime& rt = rs232_rt[port];

  int writable = out.availableForWrite();
  if (writable <= 0) return;

  uint16_t work = 0;
  while (writable > 0 && rt.txTail != rt.txHead && work < SER_TX_WORK_LIMIT)
  {
    out.write((uint8_t)rt.tx[rt.txTail]);
    rt.txTail = (uint16_t)((rt.txTail + 1) % SER_TX_BUF_LEN);
    writable--;
    work++;
  }
}

float serialFlowLastValueLMin(void)
{
  uint32_t nowMs = millis();

  for (uint8_t i = 0; i < 2; i++)
  {
    if (rs232_flow_valid[i] && (uint32_t)(nowMs - rs232_flow_last_ms[i]) <= SER_FLOW_TIMEOUT_MS)
    {
      return rs232_flow_l_min[i];
    }
  }

  return NAN;
}

bool serialFlowIsValid(void)
{
  return isfinite(serialFlowLastValueLMin());
}

static uint32_t serialAlmemoValidTimeoutMs(void)
{
  uint32_t intervalMs = interfaceAlmemoIntervalMsValue();
  if (intervalMs < 1000UL) intervalMs = 1000UL;
  // Bei zwei universellen Kanaelen wird aus Sicherheitsgruenden vor jeder
  // Messstelle Gxx/Mxx/p gesendet. Dadurch kann ein Kanal im 1-s-Modus
  // einige Sekunden alt werden, ohne ungueltig zu sein.
  uint32_t timeoutMs = intervalMs * 8UL + 5000UL;
  return (timeoutMs < 15000UL) ? 15000UL : timeoutMs;
}

static uint8_t serialAlmemoFirstTempIndex(void)
{
  for (uint8_t i = 0; i < SER_ALMEMO_CHANNEL_COUNT; i++)
  {
    if (interfaceAlmemoChannelEnabled(i) && interfaceAlmemoRole(i) == SER_ALMEMO_ROLE_TEMP) return i;
  }
  return 0;
}

float serialAlmemoLastValue(uint8_t index)
{
  if (index >= SER_ALMEMO_CHANNEL_COUNT) return NAN;
  uint32_t nowMs = millis();
  if (rs232_almemo_valid[index] &&
      (uint32_t)(nowMs - rs232_almemo_last_ms[index]) <= serialAlmemoValidTimeoutMs())
  {
    return rs232_almemo_temp_c[index];
  }
  return NAN;
}

bool serialAlmemoIsValid(uint8_t index)
{
  return isfinite(serialAlmemoLastValue(index));
}

uint32_t serialAlmemoAgeMs(uint8_t index)
{
  if (index >= SER_ALMEMO_CHANNEL_COUNT || rs232_almemo_last_ms[index] == 0) return 0xFFFFFFFFUL;
  return (uint32_t)(millis() - rs232_almemo_last_ms[index]);
}

uint32_t serialAlmemoRxErrors(uint8_t index)
{
  if (index >= SER_ALMEMO_CHANNEL_COUNT) return 0;
  return rs232_almemo_rx_errors[index];
}

float serialExternalTempLastValueC(void)
{
  return serialAlmemoLastValue(serialAlmemoFirstTempIndex());
}

bool serialExternalTempIsValid(void)
{
  return isfinite(serialExternalTempLastValueC());
}

uint32_t serialExternalTempAgeMs(void)
{
  return serialAlmemoAgeMs(serialAlmemoFirstTempIndex());
}

uint32_t serialExternalTempRxErrors(void)
{
  return serialAlmemoRxErrors(serialAlmemoFirstTempIndex());
}

int8_t serialExternalTempPort(void)
{
  return interfaceAlmemoActivePort();
}

static const char* SERIAL_FLASHMEM_NOINLINE serialModeText(void)
{
  if (aktuellerModus == 1) return "KUEHLEN";
  if (aktuellerModus == 2) return "HEIZEN";
  return "AUS";
}

static const char* SERIAL_FLASHMEM_NOINLINE serialStatusText(void)
{
  if (ablaufStatus == 1) return "FREIHEIZEN";
  if (ablaufStatus == 2) return "LED_AUTO";
  return "REGELT";
}

static float SERIAL_FLASHMEM_NOINLINE serialOptikPct(void)
{
  if (isfinite(optikTrockenReferenz) && fabsf(optikTrockenReferenz) > 1.0f)
  {
    return (optikReflexion / optikTrockenReferenz) * 100.0f;
  }
  return 0.0f;
}

static void SERIAL_FLASHMEM_NOINLINE serialGetOutputSample(output_data_sample_t* s)
{
  if (s == nullptr) return;
  if (!outputDataGetSample(interfaceOutputFilterIndex(), s))
  {
    memset(s, 0, sizeof(*s));
    s->ms = millis();
    s->tMirror = tempSpiegel;
    s->tAmbient = tempUmgebung;
    s->dewpoint = präziserTaupunkt;
    s->rh = relativeFeuchte;
    s->pressure = baroDruckHPa;
  }
}

static void SERIAL_FLASHMEM_NOINLINE serialWriteDate(Print& out, bool csvSep)
{
  int y = year();
  if (y >= 2020 && y <= 2099)
  {
    char b[32];
    if (csvSep)
    {
      snprintf(b, sizeof(b), "%04u-%02u-%02u;%02u:%02u:%02u",
               (unsigned)y, (unsigned)month(), (unsigned)day(),
               (unsigned)hour(), (unsigned)minute(), (unsigned)second());
    }
    else
    {
      snprintf(b, sizeof(b), "%04u-%02u-%02u %02u:%02u:%02u",
               (unsigned)y, (unsigned)month(), (unsigned)day(),
               (unsigned)hour(), (unsigned)minute(), (unsigned)second());
    }
    out.print(b);
  }
  else
  {
    out.print(csvSep ? "0000-00-00;00:00:00" : "0000-00-00 00:00:00");
  }
}

static void SERIAL_FLASHMEM_NOINLINE serialPrintHeaderCsv(Print& out)
{
  out.println("Datum;Uhrzeit;T_Spiegel_C;T_Umgebung_C;Taupunkt_C;rF_pct;Druck_hPa;Peltier_A;Optik_pct;Modus;Status;Flow;Flow_Unit;T_Ext_C;T_ExtValid;T_ExtAge_ms;T_ExtRxErrors;T_ExtPort;T_ExtAddress;T_ExtChannel;Almemo1_Value;Almemo1_Unit;Almemo1_Valid;Almemo1_Age_ms;Almemo1_RxErrors;Almemo1_Address;Almemo1_Channel;Almemo1_Label;Almemo2_Value;Almemo2_Unit;Almemo2_Valid;Almemo2_Age_ms;Almemo2_RxErrors;Almemo2_Address;Almemo2_Channel;Almemo2_Label");
}

static void SERIAL_FLASHMEM_NOINLINE serialPrintAlmemoCsvFields(Print& out)
{
  for (uint8_t ai = 0; ai < SER_ALMEMO_CHANNEL_COUNT; ai++)
  {
    const bool enabled = interfaceAlmemoChannelEnabled(ai);
    const float value = serialAlmemoLastValue(ai);
    const bool valid = enabled && isfinite(value);
    out.print(';');
    if (valid) out.print(value, 3);
    out.print(';');
    if (enabled) out.print(interfaceAlmemoCsvUnitText(ai));
    out.print(';'); out.print(valid ? 1 : 0);
    out.print(';');
    uint32_t age = serialAlmemoAgeMs(ai);
    if (age != 0xFFFFFFFFUL) out.print(age);
    out.print(';'); out.print(serialAlmemoRxErrors(ai));
    out.print(';'); out.print((unsigned)interfaceAlmemoAddressFor(ai));
    out.print(';'); out.print((unsigned)interfaceAlmemoChannelFor(ai));
    out.print(';');
    if (enabled) out.print(interfaceAlmemoDisplayLabel(ai));
  }
}

static void SERIAL_FLASHMEM_NOINLINE serialPrintCsvLine(Print& out)
{
  output_data_sample_t s;
  serialGetOutputSample(&s);

  serialWriteDate(out, true);
  out.print(';'); out.print(s.tMirror, 3);
  out.print(';'); out.print(s.tAmbient, 3);
  out.print(';'); out.print(s.dewpoint, 3);
  out.print(';'); out.print(s.rh, 2);
  out.print(';'); out.print(s.pressure, 1);
  out.print(';'); out.print(amp_avg, 3);
  out.print(';'); out.print(serialOptikPct(), 1);
  out.print(';'); out.print(serialModeText());
  out.print(';'); out.print(serialStatusText());
  out.print(';');
  float flow = serialFlowLastValueLMin();
  if (interfaceFlowDisplayEnabled() && isfinite(flow)) out.print(flow, 3);
  out.print(';');
  if (interfaceFlowDisplayEnabled()) out.print(interfaceFlowDisplayUnitText());
  float tExt = serialExternalTempLastValueC();
  bool tExtValid = isfinite(tExt);
  out.print(';'); if (tExtValid) out.print(tExt, 2);
  out.print(';'); out.print(tExtValid ? 1 : 0);
  out.print(';');
  uint32_t tExtAge = serialExternalTempAgeMs();
  if (tExtAge != 0xFFFFFFFFUL) out.print(tExtAge);
  out.print(';'); out.print(serialExternalTempRxErrors());
  out.print(';');
  int8_t tExtPort = serialExternalTempPort();
  if (tExtPort >= 0) out.print((unsigned)(tExtPort + 1));
  out.print(';'); out.print((unsigned)interfaceAlmemoAddress());
  out.print(';'); out.print((unsigned)interfaceAlmemoChannel());
  serialPrintAlmemoCsvFields(out);
  out.println();
}

static void SERIAL_FLASHMEM_NOINLINE serialPrintReadableLine(Print& out)
{
  output_data_sample_t s;
  serialGetOutputSample(&s);

  serialWriteDate(out, false);
  out.print(" | T-Spiegel: "); out.print(s.tMirror, 3); out.print(" C");
  out.print(" | T-Umgebung: "); out.print(s.tAmbient, 3); out.print(" C");
  out.print(" | Taupunkt: "); out.print(s.dewpoint, 3); out.print(" C");
  out.print(" | rF: "); out.print(s.rh, 2); out.print(" %");
  out.print(" | Druck: "); out.print(s.pressure, 1); out.print(" hPa");
  out.print(" | I: "); out.print(amp_avg, 3); out.print(" A");
  out.print(" | Optik: "); out.print(serialOptikPct(), 1); out.print(" %");
  float flow = serialFlowLastValueLMin();
  if (interfaceFlowDisplayEnabled())
  {
    out.print(" | Flow: ");
    if (isfinite(flow)) out.print(flow, 3);
    else out.print("XX.XX");
    out.print(' ');
    out.print(interfaceFlowDisplayUnitText());
  }
  for (uint8_t ai = 0; ai < SER_ALMEMO_CHANNEL_COUNT; ai++)
  {
    if (!interfaceAlmemoChannelEnabled(ai)) continue;
    out.print(" | ");
    out.print(interfaceAlmemoDisplayLabel(ai));
    out.print(' ');
    float av = serialAlmemoLastValue(ai);
    if (isfinite(av)) out.print(av, 2);
    else out.print("XX.XX");
    out.print(' ');
    out.print(interfaceAlmemoCsvUnitText(ai));
  }
  out.print(" | "); out.print(serialModeText());
  out.print(" | "); out.println(serialStatusText());
}

static void SERIAL_FLASHMEM_NOINLINE serialPrintHelp(Print& out)
{
  out.println("TP-3000 RS232 commands:");
  out.println("READ?   readable line");
  out.println("CSV?    CSV data line");
  out.println("HEAD?   CSV header");
  out.println("FLOW?   last flow value");
  out.println("T-EXT?  first external temperature");
  out.println("ALMEMO? last ALMEMO channel values");
  out.println("HELP?   this help");
}

static void SERIAL_FLASHMEM_NOINLINE serialRenderQueue(uint8_t port, void (*renderFn)(Print&))
{
  char tmp[SER_RENDER_BUF_LEN];
  SerialRenderBuffer rb(tmp, sizeof(tmp));
  renderFn(rb);
  serialTxQueueText(port, tmp);
}

static void SERIAL_FLASHMEM_NOINLINE serialQueueCStringLine(uint8_t port, const char* s)
{
  char tmp[SER_RENDER_BUF_LEN];
  snprintf(tmp, sizeof(tmp), "%s\r\n", s ? s : "");
  serialTxQueueText(port, tmp);
}

static void SERIAL_FLASHMEM_NOINLINE serialTrimUpperCommand(const char* in, char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0) return;
  out[0] = '\0';
  if (in == nullptr) return;

  while (*in && isspace((unsigned char)*in)) in++;
  if (*in == '$') in++;

  size_t n = 0;
  while (*in && n < outSize - 1)
  {
    char c = *in++;
    if (c == '\r' || c == '\n') break;
    out[n++] = (char)toupper((unsigned char)c);
  }
  out[n] = '\0';

  while (n > 0 && isspace((unsigned char)out[n - 1]))
  {
    out[--n] = '\0';
  }
}

static bool SERIAL_FLASHMEM_NOINLINE serialLineToFlow(const char* line, float* value)
{
  if (line == nullptr || value == nullptr) return false;

  const char* p = line;
  while (*p)
  {
    if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.' || *p == ',') break;
    p++;
  }

  if (!*p) return false;

  char num[24];
  size_t n = 0;
  while (*p && n < sizeof(num) - 1)
  {
    char c = *p;
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == ',')
    {
      num[n++] = (c == ',') ? '.' : c;
      p++;
    }
    else
    {
      break;
    }
  }
  num[n] = '\0';
  if (n == 0) return false;

  char* endp = nullptr;
  float v = strtof(num, &endp);
  if (endp == num || !isfinite(v)) return false;

  *value = v;
  return true;
}

static void serialStoreFlow(uint8_t port, float value)
{
  if (port > 1) return;
  rs232_flow_l_min[port] = value;
  rs232_flow_last_ms[port] = millis();
  rs232_flow_valid[port] = true;
}


static uint8_t SERIAL_FLASHMEM_NOINLINE serialParseAlmemoProtocolFromVersionLine(const char* line)
{
  if (line == nullptr) return SER_ALMEMO_PROTO_UNKNOWN;

  // t0 liefert je nach Geraetetyp eine Textzeile mit Softwareversion, z.B.
  // "... 6.52" oder "... 7.xx". Fuer unsere Entscheidung reicht die
  // Hauptversion vor dem ersten Dezimalpunkt. Andere Zahlen wie Kanal 0.0
  // werden hier nicht als Messwert verarbeitet, sondern nur waehrend der
  // t0-Erkennungsphase betrachtet.
  for (const char* p = line; *p; p++)
  {
    if (*p != '5' && *p != '6' && *p != '7') continue;
    if (p[1] != '.') continue;
    if (!isdigit((unsigned char)p[2])) continue;
    if (p != line && isdigit((unsigned char)p[-1])) continue;

    if (*p == '7') return SER_ALMEMO_PROTO_V7;
    return SER_ALMEMO_PROTO_V5V6;
  }

  // Einige Anzeigen/Terminalausgaben enthalten nur eine Kurzkennung wie V7.
  // Das ist weniger eindeutig als x.yy, aber waehrend einer t0-Antwort als
  // Fallback ausreichend.
  for (const char* p = line; *p; p++)
  {
    if ((p[0] == 'V' || p[0] == 'v') && p[1] == '7') return SER_ALMEMO_PROTO_V7;
    if ((p[0] == 'V' || p[0] == 'v') && (p[1] == '5' || p[1] == '6')) return SER_ALMEMO_PROTO_V5V6;
  }

  return SER_ALMEMO_PROTO_UNKNOWN;
}

static void serialAlmemoFinishProtocolDetect(uint8_t port, uint8_t protocol)
{
  if (port > 1) return;
  Rs232Runtime& rt = rs232_rt[port];
  rt.almemoDetectPending = false;
  if (protocol == SER_ALMEMO_PROTO_V7 || protocol == SER_ALMEMO_PROTO_V5V6)
  {
    rt.almemoProtocol = protocol;
  }
  else
  {
    // Sicherheits-Fallback: ohne eindeutige t0-Antwort bleibt das bisherige
    // V5/V6-Verhalten erhalten, damit vorhandene getestete ALMEMO-Geraete
    // unveraendert weiterlaufen.
    rt.almemoProtocol = SER_ALMEMO_PROTO_V5V6;
  }
}

static void serialAlmemoFormatV7ChannelCommand(char* out, size_t outSize, uint8_t channel)
{
  if (out == nullptr || outSize == 0) return;
  if (channel > 99U) channel = 0U;
  snprintf(out, outSize, "M%u.%u P35", (unsigned)(channel / 10U), (unsigned)(channel % 10U));
}

static bool serialAlmemoValueInRange(float v)
{
  return isfinite(v) && v >= -200.0f && v <= 1000.0f;
}

static bool SERIAL_FLASHMEM_NOINLINE serialParseAlmemoNumberToken(const char* start, const char* end, float* value)
{
  if (start == nullptr || value == nullptr) return false;
  if (end == nullptr) end = start + strlen(start);

  while (start < end && (isspace((unsigned char)*start) || *start == '"')) start++;
  while (end > start && (isspace((unsigned char)end[-1]) || end[-1] == '"')) end--;
  if (start >= end) return false;

  // Uhrzeiten und Datumsfelder aus Tabellenformaten duerfen nicht als
  // Messwerte gelten. Ein echter ALMEMO-Zahlenwert enthaelt keinen Doppelpunkt
  // und maximal ein Dezimaltrennzeichen.
  uint8_t decimalCount = 0;
  for (const char* q = start; q < end; q++)
  {
    if (*q == ':') return false;
    if (*q == '.' || *q == ',') decimalCount++;
  }
  if (decimalCount > 1U) return false;

  const char* p = start;
  if (*p == '+' || *p == '-') p++;
  if (p >= end || !isdigit((unsigned char)*p)) return false;

  char num[24];
  size_t n = 0;
  bool hasDigit = false;
  bool hasDecimal = false;
  p = start;
  while (p < end && n < sizeof(num) - 1)
  {
    char c = *p;
    if (c >= '0' && c <= '9')
    {
      hasDigit = true;
      num[n++] = c;
      p++;
    }
    else if ((c == '-' || c == '+') && n == 0)
    {
      num[n++] = c;
      p++;
    }
    else if (c == '.' || c == ',')
    {
      hasDecimal = true;
      num[n++] = '.';
      p++;
    }
    else
    {
      break;
    }
  }
  num[n] = '\0';
  if (!hasDigit) return false;

  // Nach dem Zahlenfeld sind Leerzeichen, Anfuehrungszeichen oder Einheiten
  // erlaubt. Neue Kanal-/Bereichsbezeichner wie M0.0 oder P304 werden durch
  // die obige Startpruefung bereits ausgesiebt.
  while (p < end)
  {
    unsigned char c = (unsigned char)*p;
    if (!(isspace(c) || c == '"' || isalpha(c) || c == '%' || c == 0xB0 || c == 0xC2 || c == 0xC3 || c == 0xF8))
    {
      // UTF-8 Gradzeichen besteht je nach Font/Quelle aus zwei Bytes; CP437/ALMEMO
      // liefert das Gradzeichen im Rohlog teilweise als 0xF8; ausserdem
      // sind Semikolonfelder hier bereits begrenzt. Andere Zeichen sprechen
      // gegen einen reinen Messwert.
      return false;
    }
    p++;
  }

  char* endp = nullptr;
  float v = strtof(num, &endp);
  if (endp == num || !serialAlmemoValueInRange(v)) return false;

  (void)hasDecimal;
  *value = v;
  return true;
}

static bool SERIAL_FLASHMEM_NOINLINE serialParseAlmemoNumberAfter(const char* search, float* value)
{
  if (search == nullptr || value == nullptr) return false;

  const char* p = search;
  while (*p)
  {
    if (((*p == '+' || *p == '-') && isdigit((unsigned char)p[1])) ||
        isdigit((unsigned char)*p))
    {
      const char* end = p;
      while (*end && *end != ';' && *end != '\r' && *end != '\n') end++;
      if (serialParseAlmemoNumberToken(p, end, value)) return true;
    }
    p++;
  }
  return false;
}

static bool SERIAL_FLASHMEM_NOINLINE serialAlmemoLineStartsWithV7Channel(const char* line, uint8_t expectedChannel, const char** afterMarker)
{
  if (line == nullptr || expectedChannel > 99U) return false;

  const char* p = line;
  while (*p && (isspace((unsigned char)*p) || *p == '"')) p++;
  if (*p == 'M') p++; // toleriert Kopf-/Diagnoseausgaben mit M0.0

  const char major = (char)('0' + (expectedChannel / 10U));
  const char minor = (char)('0' + (expectedChannel % 10U));
  if (p[0] != major || p[1] != '.' || p[2] != minor) return false;

  char next = p[3];
  if (!(next == ';' || next == '\0' || isspace((unsigned char)next) || next == '"')) return false;
  if (afterMarker != nullptr) *afterMarker = p + 3;
  return true;
}

static bool SERIAL_FLASHMEM_NOINLINE serialAlmemoLineStartsWithAnyV7Channel(const char* line)
{
  if (line == nullptr) return false;

  const char* p = line;
  while (*p && (isspace((unsigned char)*p) || *p == '"')) p++;
  if (*p == 'M') p++;

  if (!isdigit((unsigned char)p[0]) || p[1] != '.' || !isdigit((unsigned char)p[2])) return false;
  char next = p[3];
  return (next == ';' || next == '\0' || isspace((unsigned char)next) || next == '"');
}

static bool SERIAL_FLASHMEM_NOINLINE serialParseAlmemoV7P35Line(const char* line, uint8_t expectedChannel, float* value)
{
  const char* p = nullptr;
  if (!serialAlmemoLineStartsWithV7Channel(line, expectedChannel, &p)) return false;

  // V7 P35: Kanal;Ueberlaufzeichen;Wert;Einheit;Bereich;Kommentar
  // Beispiele:
  //   0.0;;27,044;°C;P304;HT
  //   1.1;>;100;%H;P2RH;r. Feuchte
  while (*p && *p != ';') p++;
  if (*p != ';') return false;
  p++; // Ueberlauf-/Statusfeld
  while (*p && *p != ';') p++;
  if (*p != ';') return false;
  p++; // Messwertfeld

  const char* end = p;
  while (*end && *end != ';' && *end != '\r' && *end != '\n') end++;
  return serialParseAlmemoNumberToken(p, end, value);
}

static bool SERIAL_FLASHMEM_NOINLINE serialParseAlmemoSemicolonTable(const char* line, uint8_t expectedChannel, float* value)
{
  if (line == nullptr || value == nullptr || strchr(line, ';') == nullptr) return false;

  // Tabellenformate liefern keine Kanalnummer je Feld. Fuer unsere typische
  // Zweikanal-Nutzung wird die Einerstelle als Tabellenindex verwendet:
  // Kanal 00 -> erstes Zahlenfeld, 01 -> zweites Zahlenfeld, 10 -> erstes
  // Zahlenfeld eines per P35 besser adressierbaren V7-Kanals.
  const uint8_t wantedIndex = (uint8_t)(expectedChannel % 10U);
  uint8_t numericIndex = 0;

  const char* p = line;
  while (p != nullptr && *p)
  {
    const char* end = strchr(p, ';');
    if (end == nullptr) end = p + strlen(p);

    float v = NAN;
    if (serialParseAlmemoNumberToken(p, end, &v))
    {
      if (numericIndex == wantedIndex)
      {
        *value = v;
        return true;
      }
      if (numericIndex < 255U) numericIndex++;
    }

    if (*end == '\0') break;
    p = end + 1;
  }

  return false;
}

static bool SERIAL_FLASHMEM_NOINLINE serialParseAlmemoClassicChannelLine(const char* line, uint8_t expectedChannel, float* value)
{
  if (line == nullptr || value == nullptr || expectedChannel > 99U) return false;

  char channelText[3];
  channelText[0] = (char)('0' + (expectedChannel / 10U));
  channelText[1] = (char)('0' + (expectedChannel % 10U));
  channelText[2] = '\0';

  const char* colon = nullptr;
  for (const char* p = line; p[0] != '\0' && p[1] != '\0' && p[2] != '\0'; p++)
  {
    if (p[0] != channelText[0] || p[1] != channelText[1] || p[2] != ':') continue;

    // Kanalnummern stehen am Zeilenanfang oder nach einem Trennzeichen.
    // Ein vorangehender Doppelpunkt kennzeichnet dagegen Minuten/Sekunden.
    if (p != line)
    {
      unsigned char prev = (unsigned char)p[-1];
      if (!(isspace(prev) || prev == ';' || prev == ',' || prev == '"')) continue;
    }

    // Auch eine am Zeilenanfang stehende Uhrzeit 00:12:34 darf nicht als
    // Kanal 00 gelten.
    if (p[3] != '\0' && p[4] != '\0' && p[5] != '\0' &&
        isdigit((unsigned char)p[3]) &&
        isdigit((unsigned char)p[4]) &&
        p[5] == ':')
    {
      continue;
    }

    colon = p + 2;
    break;
  }

  return (colon != nullptr) ? serialParseAlmemoNumberAfter(colon + 1, value) : false;
}

static bool SERIAL_FLASHMEM_NOINLINE serialParseAlmemoNakedValue(const char* line, float* value)
{
  if (line == nullptr || value == nullptr) return false;

  // Einige ALMEMO-Ausgabeformen liefern nach Mxx/p nur den nackten Messwert
  // ohne Kanalpraefix. Einfache Quittungen/Echos wie "0", "1", "p" oder
  // "M00" duerfen dabei nicht als Temperatur gelten.
  const char* search = line;
  while (*search && (isspace((unsigned char)*search) || *search == '"')) search++;
  if (!((*search == '+' || *search == '-') && isdigit((unsigned char)search[1])) &&
      !isdigit((unsigned char)*search))
  {
    return false;
  }

  bool hasSign = false;
  bool hasDecimal = false;
  bool hasUnit = false;
  uint8_t digitCount = 0;
  for (const char* q = search; *q; q++)
  {
    unsigned char c = (unsigned char)*q;
    if (c == '+' || c == '-') hasSign = true;
    else if (c == '.' || c == ',') hasDecimal = true;
    else if (isdigit(c) && digitCount < 255U) digitCount++;
    else if (isalpha(c) || c == '%' || c == 0xB0 || c == 0xC2 || c == 0xC3 || c == 0xF8) hasUnit = true;
  }
  if (digitCount < 2U || !(hasSign || hasDecimal || hasUnit)) return false;

  return serialParseAlmemoNumberAfter(search, value);
}

static bool SERIAL_FLASHMEM_NOINLINE serialLineToAlmemoValue(const char* line, uint8_t expectedChannel, float* value)
{
  if (line == nullptr || value == nullptr || expectedChannel > 99U) return false;

  // Ziel: ein Parser fuer klassische V5/V6-Ausgaben und die wichtigsten
  // V7-Ausgabeformen. Die Abfrage bleibt bewusst unveraendert; es wird nur die
  // empfangene Antwort robuster ausgewertet. Unterstuetzte Formen:
  //   00: +0023.5 °C ...            klassische Listen-/Spaltenausgabe
  //   +0023.5 °C oder 23,5          nackter Wert nach Mxx/p
  //   ;;12,;9,9 oder ;X;23,5;54,6  Tabellenformat ohne Zeit/Datum
  //   0.0;;27,044;°C;P304;HT       V7 P35-Liste
  float v = NAN;

  if (serialParseAlmemoV7P35Line(line, expectedChannel, &v))
  {
    *value = v;
    return true;
  }

  // Eine P35-Zeile eines anderen V7-Kanals darf nicht als Tabellenzeile oder
  // nackter Wert fehlinterpretiert werden.
  if (serialAlmemoLineStartsWithAnyV7Channel(line)) return false;

  if (serialParseAlmemoClassicChannelLine(line, expectedChannel, &v) ||
      serialParseAlmemoSemicolonTable(line, expectedChannel, &v) ||
      serialParseAlmemoNakedValue(line, &v))
  {
    *value = v;
    return true;
  }

  return false;
}

static void serialStoreAlmemoValue(uint8_t port, uint8_t index, float value)
{
  if (port > 1 || index >= SER_ALMEMO_CHANNEL_COUNT) return;
  rs232_almemo_temp_c[index] = value;
  rs232_almemo_last_ms[index] = millis();
  rs232_almemo_valid[index] = true;
  rs232_rt[port].almemoRequestPending = false;
  rs232_rt[port].almemoConsecutiveErrors = 0;
}

static void SERIAL_FLASHMEM_NOINLINE serialProcessPcCommand(uint8_t port, const char* line)
{
  char cmd[48];
  serialTrimUpperCommand(line, cmd, sizeof(cmd));
  if (cmd[0] == '\0') return;

  if (!strcmp(cmd, "READ?") || !strcmp(cmd, "TEXT?") || !strcmp(cmd, "MEAS?") || !strcmp(cmd, "DATA?"))
  {
    serialRenderQueue(port, serialPrintReadableLine);
  }
  else if (!strcmp(cmd, "CSV?"))
  {
    serialRenderQueue(port, serialPrintCsvLine);
  }
  else if (!strcmp(cmd, "HEAD?") || !strcmp(cmd, "HEADER?"))
  {
    serialRenderQueue(port, serialPrintHeaderCsv);
  }
  else if (!strcmp(cmd, "FLOW?"))
  {
    char tmp[64];
    float flow = serialFlowLastValueLMin();
    if (interfaceFlowDisplayEnabled() && isfinite(flow))
    {
      snprintf(tmp, sizeof(tmp), "FLOW=%.3f %s\r\n", flow, interfaceFlowDisplayUnitText());
    }
    else if (interfaceFlowDisplayEnabled())
    {
      snprintf(tmp, sizeof(tmp), "FLOW=XX.XX %s\r\n", interfaceFlowDisplayUnitText());
    }
    else
    {
      snprintf(tmp, sizeof(tmp), "FLOW=OFF\r\n");
    }
    serialTxQueueText(port, tmp);
  }
  else if (!strcmp(cmd, "T-EXT?"))
  {
    char tmp[80];
    float tExt = serialExternalTempLastValueC();
    if (isfinite(tExt))
      snprintf(tmp, sizeof(tmp), "T-EXT=%.2f C AGE=%lu ms ERR=%lu\r\n",
               (double)tExt,
               (unsigned long)serialExternalTempAgeMs(),
               (unsigned long)serialExternalTempRxErrors());
    else
      snprintf(tmp, sizeof(tmp), "T-EXT=XX.XX C ERR=%lu\r\n",
               (unsigned long)serialExternalTempRxErrors());
    serialTxQueueText(port, tmp);
  }
  else if (!strcmp(cmd, "ALMEMO?"))
  {
    char tmp[160];
    snprintf(tmp, sizeof(tmp), "ALMEMO1=%s %.2f %s AGE=%lu ERR=%lu; ALMEMO2=%s %.2f %s AGE=%lu ERR=%lu\r\n",
             serialAlmemoIsValid(0) ? "OK" : "--",
             (double)(serialAlmemoIsValid(0) ? serialAlmemoLastValue(0) : NAN),
             interfaceAlmemoCsvUnitText(0),
             (unsigned long)((serialAlmemoAgeMs(0) == 0xFFFFFFFFUL) ? 0UL : serialAlmemoAgeMs(0)),
             (unsigned long)serialAlmemoRxErrors(0),
             serialAlmemoIsValid(1) ? "OK" : "--",
             (double)(serialAlmemoIsValid(1) ? serialAlmemoLastValue(1) : NAN),
             interfaceAlmemoCsvUnitText(1),
             (unsigned long)((serialAlmemoAgeMs(1) == 0xFFFFFFFFUL) ? 0UL : serialAlmemoAgeMs(1)),
             (unsigned long)serialAlmemoRxErrors(1));
    serialTxQueueText(port, tmp);
  }
  else if (!strcmp(cmd, "HELP?") || !strcmp(cmd, "HELP") || !strcmp(cmd, "?"))
  {
    serialRenderQueue(port, serialPrintHelp);
  }
  else
  {
    serialQueueCStringLine(port, "ERR unknown command. Send HELP?");
  }
}

static void serialProcessLine(uint8_t port, const char* line)
{
  uint8_t mode = interfaceRs232Mode(port);

  if (mode == SER_RS232_MODE_PC_TEXT || mode == SER_RS232_MODE_PC_CSV)
  {
    serialProcessPcCommand(port, line);
  }
  else if (mode == SER_RS232_MODE_FLOW_IN || mode == SER_RS232_MODE_FLOW_POLL)
  {
    float v = NAN;
    if (serialLineToFlow(line, &v))
    {
      serialStoreFlow(port, v);
    }
  }
  else if (mode == SER_RS232_MODE_ALMEMO)
  {
    Rs232Runtime& rt = rs232_rt[port];

    if (rt.almemoDetectPending)
    {
      uint8_t detected = serialParseAlmemoProtocolFromVersionLine(line);
      if (detected != SER_ALMEMO_PROTO_UNKNOWN)
      {
        serialAlmemoFinishProtocolDetect(port, detected);
      }
      return;
    }

    if (rt.almemoRequestPending)
    {
      const uint8_t idx = rt.almemoPendingIndex;
      float v = NAN;
      if (idx < SER_ALMEMO_CHANNEL_COUNT &&
          serialLineToAlmemoValue(line, interfaceAlmemoChannelFor(idx), &v))
      {
        serialStoreAlmemoValue(port, idx, v);
      }
    }
  }
}

static void serialResetAlmemoRuntime(uint8_t port, bool clearValue)
{
  if (port > 1) return;
  Rs232Runtime& rt = rs232_rt[port];
  rt.almemoInitStep = 0;
  rt.almemoInitialized = false;
  rt.almemoRequestPending = false;
  rt.almemoDetectPending = false;
  rt.almemoProtocol = SER_ALMEMO_PROTO_UNKNOWN;
  rt.lastPollMs = 0;
  rt.almemoNextActionMs = 0;
  rt.almemoRequestMs = 0;
  rt.almemoDetectMs = 0;
  rt.almemoLastSeq = 0;
  rt.almemoConsecutiveErrors = 0;
  rt.almemoPollStep = 0;
  rt.almemoPollActive = false;
  rt.almemoPollListIndex = 0;
  rt.almemoPendingIndex = 0;
  if (clearValue)
  {
    for (uint8_t i = 0; i < SER_ALMEMO_CHANNEL_COUNT; i++)
    {
      rs232_almemo_temp_c[i] = NAN;
      rs232_almemo_last_ms[i] = 0;
      rs232_almemo_valid[i] = false;
    }
  }
}

static uint8_t serialAlmemoConfigMask(void)
{
  uint8_t mask = 0;
  for (uint8_t i = 0; i < SER_ALMEMO_CHANNEL_COUNT; i++)
  {
    if (interfaceAlmemoChannelEnabled(i)) mask |= (uint8_t)(1U << i);
  }
  return mask;
}

static uint8_t serialAlmemoBuildActiveList(uint8_t* out, uint8_t outSize)
{
  uint8_t count = 0;
  for (uint8_t i = 0; i < SER_ALMEMO_CHANNEL_COUNT; i++)
  {
    if (!interfaceAlmemoChannelEnabled(i)) continue;
    if (count < outSize && out != nullptr) out[count] = i;
    count++;
  }
  return count;
}

static void serialHandleModeAndAlmemoConfig(uint8_t port)
{
  if (port > 1) return;
  Rs232Runtime& rt = rs232_rt[port];
  uint8_t mode = interfaceRs232Mode(port);
  uint8_t address = interfaceAlmemoAddressFor(0);
  uint8_t channel = interfaceAlmemoChannelFor(0);
  uint8_t address2 = interfaceAlmemoAddressFor(1);
  uint8_t channel2 = interfaceAlmemoChannelFor(1);
  uint8_t role0 = interfaceAlmemoRole(0);
  uint8_t role1 = interfaceAlmemoRole(1);
  uint8_t activeMask = serialAlmemoConfigMask();
  uint8_t interval = interfaceAlmemoIntervalIndex();

  bool modeChanged = (rt.lastMode != mode);
  bool almemoCfgChanged = (mode == SER_RS232_MODE_ALMEMO) &&
                          (rt.almemoConfigAddress != address ||
                           rt.almemoConfigChannel != channel ||
                           rt.almemoConfigAddress2 != address2 ||
                           rt.almemoConfigChannel2 != channel2 ||
                           rt.almemoConfigRole[0] != role0 ||
                           rt.almemoConfigRole[1] != role1 ||
                           rt.almemoConfigMask != activeMask ||
                           rt.almemoConfigInterval != interval);

  if (modeChanged)
  {
    rt.pos = 0;
    rt.lastOutputMs = 0;
    rt.lastOutputSeq = 0;
    rt.lastPollMs = 0;
    serialTxClear(port);
    if (rt.lastMode == SER_RS232_MODE_ALMEMO || mode == SER_RS232_MODE_ALMEMO)
    {
      serialResetAlmemoRuntime(port, true);
    }
    rt.lastMode = mode;
  }

  if (almemoCfgChanged)
  {
    serialTxClear(port);
    serialResetAlmemoRuntime(port, true);
  }

  rt.almemoConfigAddress = address;
  rt.almemoConfigChannel = channel;
  rt.almemoConfigAddress2 = address2;
  rt.almemoConfigChannel2 = channel2;
  rt.almemoConfigRole[0] = role0;
  rt.almemoConfigRole[1] = role1;
  rt.almemoConfigMask = activeMask;
  rt.almemoConfigInterval = interval;
}

static void serialReadPort(uint8_t port, Stream& in)
{
  Rs232Runtime& rt = rs232_rt[port];
  uint8_t work = 0;

  while (in.available() > 0 && work < SER_RX_WORK_LIMIT)
  {
    work++;
    uint8_t raw = (uint8_t)in.read();
    char c = (char)raw;
    rt.lastByteMs = millis();
    serialTraceRxByte(port, raw, rt.lastByteMs);

    // ALMEMO beendet Antworten zusaetzlich mit ETX (0x03). Im Rohlog bleibt
    // das Byte sichtbar, fuer den Zeilenparser ist es jedoch nur ein Abschluss.
    if (raw == 0x03U)
    {
      if (rt.pos > 0)
      {
        rt.line[rt.pos] = '\0';
        serialProcessLine(port, rt.line);
        rt.pos = 0;
      }
      continue;
    }

    if (c == '\r' || c == '\n')
    {
      if (rt.pos > 0)
      {
        rt.line[rt.pos] = '\0';
        serialProcessLine(port, rt.line);
        rt.pos = 0;
      }
    }
    else if (rt.pos < SER_LINE_BUF_LEN - 1)
    {
      rt.line[rt.pos++] = c;
    }
    else
    {
      // Ueberlange Zeile verwerfen.
      rt.pos = 0;
    }
  }

  // Angefangene Zeile nach laengerer Pause verwerfen.
  if (rt.pos > 0 && (uint32_t)(millis() - rt.lastByteMs) > 1000UL)
  {
    rt.pos = 0;
  }
}

static void serialPeriodicOutput(uint8_t port)
{
  uint8_t mode = interfaceRs232Mode(port);
  if (mode != SER_RS232_MODE_PC_TEXT && mode != SER_RS232_MODE_PC_CSV) return;

  uint8_t outMode = interfaceRs232OutputMode(port);
  if (outMode == SER_RS232_OUT_REQUEST) return;

  uint32_t intervalMs = interfaceRs232OutputIntervalMsValue(port);

  Rs232Runtime& rt = rs232_rt[port];
  uint32_t nowMs = millis();

  bool due = false;
  if (intervalMs == 0UL)
  {
    uint32_t seq = outputDataRawSequence();
    if (seq != 0 && seq != rt.lastOutputSeq)
    {
      rt.lastOutputSeq = seq;
      due = true;
    }
  }
  else
  {
    if (intervalMs < 250UL) intervalMs = 1000UL;
    due = (rt.lastOutputMs == 0 || (uint32_t)(nowMs - rt.lastOutputMs) >= intervalMs);
  }

  if (due)
  {
    rt.lastOutputMs = nowMs;
    if (mode == SER_RS232_MODE_PC_CSV) serialRenderQueue(port, serialPrintCsvLine);
    else serialRenderQueue(port, serialPrintReadableLine);
  }
}

static void serialFlowPolling(uint8_t port)
{
  uint8_t mode = interfaceRs232Mode(port);
  if (mode != SER_RS232_MODE_FLOW_POLL) return;

  Rs232Runtime& rt = rs232_rt[port];
  uint32_t nowMs = millis();
  if (rt.lastPollMs == 0 || (uint32_t)(nowMs - rt.lastPollMs) >= SER_FLOW_POLL_MS)
  {
    rt.lastPollMs = nowMs;
    serialTxQueueText(port, "?\r\n");
  }
}

static void serialAlmemoTask(uint8_t port)
{
  if (port > 1 || interfaceRs232Mode(port) != SER_RS232_MODE_ALMEMO) return;

  uint8_t active[2] = {0, 0};
  uint8_t activeCount = serialAlmemoBuildActiveList(active, 2);
  if (activeCount == 0) return;

  Rs232Runtime& rt = rs232_rt[port];
  uint32_t nowMs = millis();

  // t0-Versionserkennung: Auf V7 wird danach der dokumentierte
  // Mx.y-P35-Pfad verwendet, bei V5/V6 bleibt der getestete Gxx/Mxx/p-Pfad.
  // Antwortet t0 nicht eindeutig, wird aus Kompatibilitaetsgruenden nach
  // kurzer Wartezeit automatisch V5/V6 angenommen.
  if (rt.almemoDetectPending &&
      (uint32_t)(nowMs - rt.almemoDetectMs) >= SER_ALMEMO_DETECT_TIMEOUT_MS)
  {
    serialAlmemoFinishProtocolDetect(port, SER_ALMEMO_PROTO_UNKNOWN);
  }
  if (rt.almemoDetectPending) return;

  // Eine echte Messwertantwort ist am getesteten ALMEMO nach rund 25 ms da
  // und nach etwa 140 ms komplett. Bleibt sie 800 ms aus, zaehlt genau ein
  // Fehlversuch. Nach drei aufeinanderfolgenden Fehlversuchen wird der gerade
  // abgefragte universelle ALMEMO-Kanal ungueltig und die Sequenz startet neu.
  if (rt.almemoRequestPending &&
      (uint32_t)(nowMs - rt.almemoRequestMs) >= SER_ALMEMO_REPLY_TIMEOUT_MS)
  {
    rt.almemoRequestPending = false;
    uint8_t errIndex = rt.almemoPendingIndex;
    if (errIndex >= SER_ALMEMO_CHANNEL_COUNT) errIndex = 0;
    rs232_almemo_rx_errors[errIndex]++;
    if (rt.almemoConsecutiveErrors < 255U) rt.almemoConsecutiveErrors++;

    if (rt.almemoConsecutiveErrors >= SER_ALMEMO_REINIT_ERRORS)
    {
      rs232_almemo_valid[errIndex] = false;
      rs232_almemo_temp_c[errIndex] = NAN;
      rt.almemoPollStep = 0U;
      rt.almemoInitialized = false;
      rt.almemoProtocol = SER_ALMEMO_PROTO_UNKNOWN;
      rt.almemoDetectPending = false;
      rt.almemoConsecutiveErrors = 0U;
      rt.almemoNextActionMs = nowMs + SER_ALMEMO_INIT_GAP_MS;
    }
  }

  if ((int32_t)(nowMs - rt.almemoNextActionMs) < 0) return;

  if (rt.almemoPollListIndex >= activeCount) rt.almemoPollListIndex = 0;
  const uint8_t index = active[rt.almemoPollListIndex];

  // Universelle Zweikanalabfrage:
  //   Schritt 0: Gaa  -> Geraet adressieren
  //   Schritt 1: t0   -> einmalige Protokollerkennung, falls unbekannt
  //              Mcc  -> V5/V6 Messstelle waehlen
  //              Mx.y P35 -> V7 Einzelkanal mit Wert/Einheit/Bereich/Kommentar
  //   Schritt 2: p    -> V5/V6 Messwert lesen
  // Gesendet wird weiterhin bytegenau ohne CR/LF, passend zum erfolgreichen
  // PuTTY-Test mit G00, M00 und p. Nur die Antwort auf p bzw. P35 wird als
  // Messwert geparst.
  char cmd[16];
  bool expectsMeasurement = false;

  if (rt.almemoPollStep == 0U)
  {
    snprintf(cmd, sizeof(cmd), "G%02u", (unsigned)interfaceAlmemoAddressFor(index));
    rt.almemoPollStep = 1U;
  }
  else if (rt.almemoPollStep == 1U)
  {
    if (rt.almemoProtocol == SER_ALMEMO_PROTO_UNKNOWN)
    {
      strcpy(cmd, "t0");
      rt.almemoDetectPending = true;
      rt.almemoDetectMs = nowMs;
      // PollStep bleibt 1: Nach erfolgreicher/abgelaufener Erkennung wird
      // im naechsten Durchlauf fuer denselben Kanal Mxx oder Mx.y P35 gesendet.
    }
    else if (rt.almemoProtocol == SER_ALMEMO_PROTO_V7)
    {
      serialAlmemoFormatV7ChannelCommand(cmd, sizeof(cmd), interfaceAlmemoChannelFor(index));
      expectsMeasurement = true;
      rt.almemoPendingIndex = index;
    }
    else
    {
      snprintf(cmd, sizeof(cmd), "M%02u", (unsigned)interfaceAlmemoChannelFor(index));
      rt.almemoPollStep = 2U;
    }
  }
  else
  {
    strcpy(cmd, "p");
    expectsMeasurement = true;
    rt.almemoPendingIndex = index;
  }

  if (serialTxQueueText(port, cmd))
  {
    serialTraceTx(port, cmd);
    rt.almemoInitialized = true;
    rt.almemoRequestPending = expectsMeasurement;
    if (expectsMeasurement)
    {
      rt.almemoRequestMs = nowMs;
      rt.almemoPollStep = 0U;
      rt.almemoPollListIndex = (uint8_t)((rt.almemoPollListIndex + 1U) % activeCount);
      uint32_t intervalMs = interfaceAlmemoIntervalMsValue();
      if (intervalMs < 1000UL) intervalMs = 1000UL;
      rt.almemoNextActionMs = nowMs + intervalMs;
    }
    else
    {
      rt.almemoNextActionMs = nowMs + SER_ALMEMO_INIT_GAP_MS;
    }
  }
}

static void serialUpdateFlowTimeouts(void)
{
  uint32_t nowMs = millis();
  uint32_t almemoTimeoutMs = serialAlmemoValidTimeoutMs();
  for (uint8_t i = 0; i < 2; i++)
  {
    if (rs232_flow_valid[i] && (uint32_t)(nowMs - rs232_flow_last_ms[i]) > SER_FLOW_TIMEOUT_MS)
    {
      rs232_flow_valid[i] = false;
      rs232_flow_l_min[i] = NAN;
    }
    if (rs232_almemo_valid[i] && (uint32_t)(nowMs - rs232_almemo_last_ms[i]) > almemoTimeoutMs)
    {
      rs232_almemo_valid[i] = false;
      rs232_almemo_temp_c[i] = NAN;
    }
  }
}

void serialProtocolBegin(void)
{
  memset(rs232_rt, 0, sizeof(rs232_rt));
  serial_trace_head = 0;
  serial_trace_tail = 0;
  serial_trace_dropped = 0;
  serial_trace_init_attempted = false;
  serial_trace_ready = false;
  serial_trace_capture_enabled = false;
  serial_trace_error = false;
  serial_trace_filename[0] = '\0';
  serial_trace_last_flush_ms = 0;
  serial_trace_last_blink_ms = 0;
  for (uint8_t i = 0; i < 2; i++)
  {
    serial_trace_rx_len[i] = 0;
    serial_trace_rx_text[i][0] = '\0';
    serial_trace_rx_last_byte_ms[i] = 0;
  }
  rs232_rt[0].lastMode = 0xFF;
  rs232_rt[1].lastMode = 0xFF;
  rs232_flow_l_min[0] = NAN;
  rs232_flow_l_min[1] = NAN;
  rs232_flow_last_ms[0] = 0;
  rs232_flow_last_ms[1] = 0;
  rs232_flow_valid[0] = false;
  rs232_flow_valid[1] = false;
  for (uint8_t i = 0; i < 2; i++)
  {
    rs232_almemo_temp_c[i] = NAN;
    rs232_almemo_last_ms[i] = 0;
    rs232_almemo_valid[i] = false;
    rs232_almemo_rx_errors[i] = 0;
  }

}

bool serialProtocolTraceIsActive(void)
{
  return serial_trace_ready && serial_trace_capture_enabled &&
         serial_trace_filename[0] != '\0';
}

bool serialProtocolTraceHasError(void)
{
  return interfaceAlmemoTraceEnabled() && serial_trace_error;
}

bool serialProtocolTraceWriteBlinkActive(void)
{
  if (!serialProtocolTraceIsActive()) return false;
  if (serial_trace_last_blink_ms == 0) return false;
  return ((uint32_t)(millis() - serial_trace_last_blink_ms) < 250UL);
}

const char* serialProtocolTraceFileName(void)
{
  return serial_trace_filename;
}

bool serialProtocolTraceApplyNow(void)
{
  if (interfaceAlmemoTraceEnabled())
  {
    return serialTraceStartNow();
  }

  serialTraceCloseNow();
  return true;
}

void serialProtocolTraceBegin(void)
{
  // Beim Boot darf der Seriellog auch dann starten, wenn das normale
  // SD-CSV-Logging ausgeschaltet ist. Die Karteninitialisierung erfolgt
  // dabei ohne Ruecksetzen der CSV-Puffer oder des CSV-Zeitplans.
  if (interfaceAlmemoTraceEnabled() && !sdLogReadyForLogging())
  {
    sdLogEnsureReadyForAccess();
  }
  serialProtocolTraceApplyNow();
}

void serialProtocolTask(void)
{
  serialHandleModeAndAlmemoConfig(0);
  serialHandleModeAndAlmemoConfig(1);

  if (interfaceRs232Mode(0) == SER_RS232_MODE_WINCONTROL) winControlOutSerialTask(0, Serial7);
  else serialReadPort(0, Serial7);
  if (interfaceRs232Mode(1) == SER_RS232_MODE_WINCONTROL) winControlOutSerialTask(1, Serial8);
  else serialReadPort(1, Serial8);

  serialFlowPolling(0);
  serialFlowPolling(1);
  serialAlmemoTask(0);
  serialAlmemoTask(1);

  serialPeriodicOutput(0);
  serialPeriodicOutput(1);

  if (interfaceRs232Mode(0) == SER_RS232_MODE_OFF || interfaceRs232Mode(0) == SER_RS232_MODE_WINCONTROL) serialTxClear(0);
  if (interfaceRs232Mode(1) == SER_RS232_MODE_OFF || interfaceRs232Mode(1) == SER_RS232_MODE_WINCONTROL) serialTxClear(1);

  if (interfaceRs232Mode(0) != SER_RS232_MODE_WINCONTROL) serialTxDrainPort(0, Serial7);
  if (interfaceRs232Mode(1) != SER_RS232_MODE_WINCONTROL) serialTxDrainPort(1, Serial8);

  // Auch Konfigurationsänderungen außerhalb des lokalen Menüs werden
  // übernommen. Der normale Fall wird bereits direkt im Menü angewendet.
  if (interfaceAlmemoTraceEnabled())
  {
    if (!serial_trace_capture_enabled && !serial_trace_init_attempted)
    {
      serialTraceStartNow();
    }
  }
  else if (serial_trace_capture_enabled || serial_trace_ready)
  {
    serialTraceCloseNow();
  }

  serialTraceCaptureTask();
  serialUpdateFlowTimeouts();
}
