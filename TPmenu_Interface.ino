/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPmenu_Interface.ino
 * Zweck: Schnittstellen-Menues fuer RS232, Ethernet, USB und SD.
 *
 * Abgeleitet aus: PSWRmenu.ino
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2014 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
// TPmenu_Interface.ino
// Schnittstellen-Menüs: RS232-1 / RS232-2 / Ethernet / USB / SD-Karte
// =========================================================================
// Stand: Menüs und EEPROM-Konfiguration vorbereitet.
// Netzwerk-/Protokollfunktionen werden spaeter auf diese Einstellungen
// aufgesetzt. RS232-Baudrate wird bereits auf Serial7/Serial8 angewendet.

#include <EEPROM.h>
#include <string.h>
#include "TPsignedData.h"

// Selten benutzte Setup-/EEPROM-Hilfsfunktionen laufen aus Flash,
// damit der zeitkritische ITCM-Bereich fuer Loop, Regelung und Safety frei bleibt.
#define INTERFACE_FLASHMEM_NOINLINE FLASHMEM __attribute__((noinline))

// Lokale Texthelfer werden bereits vor ihrer Definition in kleinen
// Zugriffsfunktionen benutzt. Die expliziten Prototypen verhindern,
// dass der Arduino-Sketch-Preprozessor hier eine Funktion uebersieht.
static const char* FLASHMEM ifText(const char* de, const char* en);
static const char* FLASHMEM ifTextBack(void);
static const char* FLASHMEM ifTextOnOff(uint8_t value);

// SD-Logging aus TPsdLog.ino
extern uint8_t sdLogGetStatus(void);
extern const char* sdLogGetStatusTextDE(void);
extern const char* sdLogGetStatusTextEN(void);
extern const char* sdLogGetCurrentFile(void);
extern const char* sdLogGetLastError(void);
extern void sdLogBegin(void);
extern void sdLogCheckOnce(void);
extern void sdLogResetSchedule(void);
extern bool sdLogReadyForLogging(void);
extern bool sdLogEnsureReadyForAccess(void);
extern bool serialProtocolTraceApplyNow(void);

// Safety-Hold fuer bekannte blockierende SD-Pruefungen
extern void safetyBeginBlockingOperation(void);
extern void safetyEndBlockingOperation(void);

// Aktueller Ethernet-Status aus TPethernet.ino
extern bool ethernetIsEnabled(void);
extern bool ethernetHasValidIp(void);
extern void ethernetGetCurrentIp(uint8_t ip[4]);
extern void ethernetServiceApplyNow(void);

// =========================================================================
// SCHNITTSTELLEN-KONFIGURATION
// =========================================================================

#define IF_RS232_MODE_OFF        0
#define IF_RS232_MODE_PC_TEXT    1
#define IF_RS232_MODE_PC_CSV     2
#define IF_RS232_MODE_FLOW_IN    3
#define IF_RS232_MODE_FLOW_POLL  4
#define IF_RS232_MODE_ALMEMO     5
#define IF_RS232_MODE_WINCONTROL  6

#define IF_RS232_OUT_CYCLIC     0
#define IF_RS232_OUT_REQUEST    1
#define IF_RS232_OUT_BOTH       2

#define IF_RS232_INTERVAL_COUNT 4
#define IF_OUTPUT_INTERVAL_COUNT 5
#define IF_OUTPUT_FILTER_COUNT   5

#define IF_FLOW_DISPLAY_OFF      0
#define IF_FLOW_DISPLAY_L_MIN    1
#define IF_FLOW_DISPLAY_L_S      2
#define IF_FLOW_DISPLAY_M3_H     3
#define IF_FLOW_DISPLAY_T_EXT    4
#define IF_FLOW_DISPLAY_COUNT    5

#define IF_ALMEMO_INTERVAL_ADC    0
#define IF_ALMEMO_INTERVAL_1S     1
#define IF_ALMEMO_INTERVAL_10S    2
#define IF_ALMEMO_INTERVAL_COUNT  3

#define IF_ALMEMO_CHANNEL_COUNT   2
#define IF_ALMEMO_ROLE_OFF        0
#define IF_ALMEMO_ROLE_TEMP       1
#define IF_ALMEMO_ROLE_FLOW       2
#define IF_ALMEMO_ROLE_PRESSURE   3
#define IF_ALMEMO_ROLE_GENERAL    4
#define IF_ALMEMO_ROLE_COUNT      5

#define IF_WINCONTROL_CYCLE_COUNT  4
#define IF_WINCONTROL_CYCLE_1S     0
#define IF_WINCONTROL_CYCLE_10S    1
#define IF_WINCONTROL_CYCLE_20S    2
#define IF_WINCONTROL_CYCLE_60S    3

#define IF_USB_MODE_COMMANDS    0
#define IF_USB_MODE_CSV         1
#define IF_USB_MODE_DEBUG       2

#define IF_BAUD_COUNT           5

#define IF_SD_INTERVAL_COUNT    4
#define IF_CFG_MAGIC            0x54494635UL   // 'TIF5'
#define IF_CFG_VERSION          12
#define IF_CFG_PREVIOUS_VERSION 11

static const uint32_t interfaceBaudTable[IF_BAUD_COUNT] = {
  9600UL,
  19200UL,
  38400UL,
  57600UL,
  115200UL
};

static const uint32_t interfaceRs232IntervalMs[IF_RS232_INTERVAL_COUNT] = {
  1000UL,
  5000UL,
  10000UL,
  60000UL
};

static const uint32_t interfaceWincontrolCycleMs[IF_WINCONTROL_CYCLE_COUNT] = {
  1000UL,
  10000UL,
  20000UL,
  60000UL
};

static const uint32_t interfaceSdIntervalMs[IF_SD_INTERVAL_COUNT] = {
  10000UL,     // 10 s
  60000UL,     // 60 s
  180000UL,    // 3 min
  300000UL     // 5 min
};

static const uint32_t interfaceOutputIntervalMs[IF_OUTPUT_INTERVAL_COUNT] = {
  0UL,         // ADC / jeder fertige interne Wert
  1000UL,      // 1 s
  10000UL,     // 10 s
  30000UL,     // 30 s
  60000UL      // 60 s
};

static const uint8_t interfaceOutputFilterSec[IF_OUTPUT_FILTER_COUNT] = {
  0, 1, 3, 5, 10
};

typedef struct {
  uint32_t magic;
  uint16_t version;

  uint8_t  rs232_mode[2];
  uint8_t  rs232_baud_index[2];
  uint8_t  rs232_output_mode[2];
  uint8_t  rs232_output_interval_index[2];

  uint8_t  eth_enabled;
  uint8_t  eth_dhcp;
  uint8_t  eth_ip[4];
  uint8_t  eth_subnet[4];
  uint8_t  eth_gateway[4];
  uint8_t  eth_dns[4];
  uint16_t eth_tcp_port;

  uint8_t  web_enabled;
  uint16_t web_port;
  uint8_t  web_setup_enabled;

  uint8_t  usb_mode;

  uint8_t  output_interval_index;     // allgemein: 0=ADC, 1=1s, 2=10s, 3=30s, 4=60s
  uint8_t  output_filter_index;       // allgemein: 0=0s, 1=1s, 2=3s, 3=5s, 4=10s
  uint8_t  diagnostic_data_enabled;   // allgemein: 0=Produktionsdaten schlank, 1=volle Diagnose

  uint8_t  sd_output_interval_index;  // SD separat: 0=ADC, 1=1s, 2=10s, 3=30s, 4=60s
  uint8_t  sd_output_filter_index;    // SD separat: 0=0s, 1=1s, 2=3s, 3=5s, 4=10s
  uint8_t  sd_diagnostic_data_enabled;// SD separat: 0=Metrologie-CSV schlank, 1=volle Diagnose
  uint8_t  sd_logging_enabled;
  uint8_t  sd_interval_index;
  uint8_t  sd_header_enabled;
  uint8_t  flow_display_mode;   // 0=Aus, 1=l/min, 2=l/s, 3=m3/h, 4=T-Ext

  uint8_t  almemo_address;        // klassische ALMEMO-Geraeteadresse 00..99
  uint8_t  almemo_channel;        // klassische ALMEMO-Messstelle 00..99
  uint8_t  almemo_interval_index; // 1=1 s, 2=10 s; alter Wert 0 wird auf 1 s migriert
  uint8_t  almemo_trace_enabled;  // 0=Aus, 1=RX/TX nach /LOG/SERIALnnn.TXT
  uint8_t  almemo_active[IF_ALMEMO_CHANNEL_COUNT]; // 0=Aus, 1=Kanal zyklisch abfragen
  uint8_t  almemo_address2;       // zweiter universeller ALMEMO-Kanal
  uint8_t  almemo_channel2;
  uint8_t  almemo_role[IF_ALMEMO_CHANNEL_COUNT]; // Temperatur / Flow / Druck / Allgemein

  uint8_t  wincontrol_eth_enabled;   // raw TCP WinControl-Ausgabe
  uint16_t wincontrol_tcp_port;      // raw TCP-Port, Default 10001
  uint8_t  wincontrol_address;       // virtuelle ALMEMO-Geraeteadresse 00..99
  uint8_t  wincontrol_baud_index;    // RS232-Baudrate fuer WinControl-Ausgabe
  uint8_t  wincontrol_cycle_index;   // S2/S3-Ausgabezyklus

  // V12 nutzt das bisherige Padding-Byte vor CRC. Dadurch bleiben Groesse und
  // CRC-Position des V11-Payloads unveraendert und die A/B-Slots migrierbar.
  uint8_t  sd_log_integrity_mode;    // 0=Aus, 1=SHA-256, 2=CSV zertifiziert, 3=TPLOG zertifiziert

  uint32_t crc;
} interface_config_t;

// Die V11->V12-Migration setzt voraus, dass genau das bisherige Padding-Byte
// verwendet wird. Diese Compile-Zeit-Pruefungen verhindern eine unbemerkte
// Layoutaenderung durch spaetere Felder oder Compileroptionen.
static_assert(sizeof(interface_config_t) == 72U,
              "interface_config_t layout changed; update EEPROM migration");
static_assert(offsetof(interface_config_t, sd_log_integrity_mode) + 1U ==
              offsetof(interface_config_t, crc),
              "SD integrity byte must remain directly before interface CRC");

// EEPROM-Migrationsabbild des unmittelbar vorherigen V8-Formats.
// V8 kennt die zweikanalige ALMEMO-Eingabe, aber noch keine WinControl-Ausgabe.
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint8_t  rs232_mode[2];
  uint8_t  rs232_baud_index[2];
  uint8_t  rs232_output_mode[2];
  uint8_t  rs232_output_interval_index[2];
  uint8_t  eth_enabled;
  uint8_t  eth_dhcp;
  uint8_t  eth_ip[4];
  uint8_t  eth_subnet[4];
  uint8_t  eth_gateway[4];
  uint8_t  eth_dns[4];
  uint16_t eth_tcp_port;
  uint8_t  web_enabled;
  uint16_t web_port;
  uint8_t  web_setup_enabled;
  uint8_t  usb_mode;
  uint8_t  output_interval_index;
  uint8_t  output_filter_index;
  uint8_t  diagnostic_data_enabled;
  uint8_t  sd_output_interval_index;
  uint8_t  sd_output_filter_index;
  uint8_t  sd_diagnostic_data_enabled;
  uint8_t  sd_logging_enabled;
  uint8_t  sd_interval_index;
  uint8_t  sd_header_enabled;
  uint8_t  flow_display_mode;
  uint8_t  almemo_address;
  uint8_t  almemo_channel;
  uint8_t  almemo_interval_index;
  uint8_t  almemo_trace_enabled;
  uint8_t  almemo_active[IF_ALMEMO_CHANNEL_COUNT];
  uint8_t  almemo_address2;
  uint8_t  almemo_channel2;
  uint8_t  almemo_role[IF_ALMEMO_CHANNEL_COUNT];
  uint32_t crc;
} interface_config_v8_t;

// EEPROM-Migrationsabbild des unmittelbar vorherigen V7-Formats.
// V7 kannte nur einen ALMEMO-T-Ext-Kanal plus den Seriellog-Schalter.
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint8_t  rs232_mode[2];
  uint8_t  rs232_baud_index[2];
  uint8_t  rs232_output_mode[2];
  uint8_t  rs232_output_interval_index[2];
  uint8_t  eth_enabled;
  uint8_t  eth_dhcp;
  uint8_t  eth_ip[4];
  uint8_t  eth_subnet[4];
  uint8_t  eth_gateway[4];
  uint8_t  eth_dns[4];
  uint16_t eth_tcp_port;
  uint8_t  web_enabled;
  uint16_t web_port;
  uint8_t  web_setup_enabled;
  uint8_t  usb_mode;
  uint8_t  output_interval_index;
  uint8_t  output_filter_index;
  uint8_t  diagnostic_data_enabled;
  uint8_t  sd_output_interval_index;
  uint8_t  sd_output_filter_index;
  uint8_t  sd_diagnostic_data_enabled;
  uint8_t  sd_logging_enabled;
  uint8_t  sd_interval_index;
  uint8_t  sd_header_enabled;
  uint8_t  flow_display_mode;
  uint8_t  almemo_address;
  uint8_t  almemo_channel;
  uint8_t  almemo_interval_index;
  uint8_t  almemo_trace_enabled;
  uint32_t crc;
} interface_config_v7_t;

// EEPROM-Migrationsabbild des unmittelbar vorherigen V6-Formats.
// V6 enthaelt bereits die ALMEMO-Adresse, den Kanal und das Intervall,
// aber noch keinen separat schaltbaren Seriellog.
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint8_t  rs232_mode[2];
  uint8_t  rs232_baud_index[2];
  uint8_t  rs232_output_mode[2];
  uint8_t  rs232_output_interval_index[2];
  uint8_t  eth_enabled;
  uint8_t  eth_dhcp;
  uint8_t  eth_ip[4];
  uint8_t  eth_subnet[4];
  uint8_t  eth_gateway[4];
  uint8_t  eth_dns[4];
  uint16_t eth_tcp_port;
  uint8_t  web_enabled;
  uint16_t web_port;
  uint8_t  web_setup_enabled;
  uint8_t  usb_mode;
  uint8_t  output_interval_index;
  uint8_t  output_filter_index;
  uint8_t  diagnostic_data_enabled;
  uint8_t  sd_output_interval_index;
  uint8_t  sd_output_filter_index;
  uint8_t  sd_diagnostic_data_enabled;
  uint8_t  sd_logging_enabled;
  uint8_t  sd_interval_index;
  uint8_t  sd_header_enabled;
  uint8_t  flow_display_mode;
  uint8_t  almemo_address;
  uint8_t  almemo_channel;
  uint8_t  almemo_interval_index;
  uint32_t crc;
} interface_config_v6_t;

// EEPROM-Migrationsabbild des unmittelbar vorherigen V5-Formats.
// Die Feldreihenfolge ist Teil des gespeicherten Konfigurationsformats und muss unveraendert bleiben.
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint8_t  rs232_mode[2];
  uint8_t  rs232_baud_index[2];
  uint8_t  rs232_output_mode[2];
  uint8_t  rs232_output_interval_index[2];
  uint8_t  eth_enabled;
  uint8_t  eth_dhcp;
  uint8_t  eth_ip[4];
  uint8_t  eth_subnet[4];
  uint8_t  eth_gateway[4];
  uint8_t  eth_dns[4];
  uint16_t eth_tcp_port;
  uint8_t  web_enabled;
  uint16_t web_port;
  uint8_t  web_setup_enabled;
  uint8_t  usb_mode;
  uint8_t  output_interval_index;
  uint8_t  output_filter_index;
  uint8_t  diagnostic_data_enabled;
  uint8_t  sd_output_interval_index;
  uint8_t  sd_output_filter_index;
  uint8_t  sd_diagnostic_data_enabled;
  uint8_t  sd_logging_enabled;
  uint8_t  sd_interval_index;
  uint8_t  sd_header_enabled;
  uint8_t  flow_display_mode;
  uint32_t crc;
} interface_config_v5_t;

static interface_config_t interface_cfg;
static bool interface_cfg_loaded = false;

static uint8_t INTERFACE_FLASHMEM_NOINLINE interfaceSdMaxWriteIntervalIndexForOutput(uint8_t outputIndex);
static void INTERFACE_FLASHMEM_NOINLINE interfaceClampSdWriteIntervalToOutput(void);
static void FLASHMEM interfaceToggleMenu(const char* title, uint8_t& value, uint16_t return_level);
bool FLASHMEM interfaceWebSetAlmemoChannel(uint8_t index, uint8_t active, uint8_t address, uint8_t channel, uint8_t role);
bool FLASHMEM interfaceWebSetAlmemoInterval(uint8_t intervalIndex);
bool FLASHMEM interfaceWebSetAlmemoTrace(bool enabled);

// EEPROM V11: Schnittstellenkonfiguration mit A/B-Slots.
// Der alte Einzelblock wird ab diesem Stand nicht mehr benutzt.
#define IF_CFG_SLOT_MAGIC    0x54494641UL   // 'TIFA'
#define IF_CFG_SLOT_VERSION  1U
#define IF_CFG_SLOT_A_ADDR   1024
#define IF_CFG_SLOT_B_ADDR   1280
#define IF_CFG_SLOT_SIZE     256

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  interface_config_t payload;
  uint32_t crc;
} interface_config_slot_t;

static_assert(sizeof(interface_config_slot_t) <= IF_CFG_SLOT_SIZE,
              "Interface EEPROM slot too small");

static uint32_t interface_cfg_sequence = 0UL;
static uint8_t  interface_cfg_active_slot = 0U;

static int INTERFACE_FLASHMEM_NOINLINE interfaceConfigSlotAddr(uint8_t slot)
{
  return slot ? IF_CFG_SLOT_B_ADDR : IF_CFG_SLOT_A_ADDR;
}

static uint32_t INTERFACE_FLASHMEM_NOINLINE interfaceConfigCrcBytes(const void* data, size_t n)
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

static uint32_t INTERFACE_FLASHMEM_NOINLINE interfaceConfigCrc(void)
{
  return interfaceConfigCrcBytes(&interface_cfg,
                                 sizeof(interface_config_t) - sizeof(uint32_t));
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceConfigPayloadValid(const interface_config_t& cfg)
{
  if (cfg.magic != IF_CFG_MAGIC) return false;
  if (cfg.version != IF_CFG_VERSION && cfg.version != IF_CFG_PREVIOUS_VERSION) return false;
  const uint32_t crc = interfaceConfigCrcBytes(&cfg, sizeof(interface_config_t) - sizeof(uint32_t));
  return (cfg.crc == crc);
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceConfigReadSlot(uint8_t slot, interface_config_slot_t& out)
{
  EEPROM.get(interfaceConfigSlotAddr(slot), out);
  if (out.magic != IF_CFG_SLOT_MAGIC) return false;
  if (out.version != IF_CFG_SLOT_VERSION) return false;
  if (out.size != sizeof(interface_config_t)) return false;
  const uint32_t slotCrc = interfaceConfigCrcBytes(&out, sizeof(out) - sizeof(out.crc));
  if (slotCrc != out.crc) return false;
  return interfaceConfigPayloadValid(out.payload);
}

static int INTERFACE_FLASHMEM_NOINLINE interfaceConfigEepromAddr(void)
{
  // Legacy-Einzelblockadresse fuer die CRC-gepruefte Migration von V5 bis V8.
  return IF_CFG_SLOT_A_ADDR;
}

static void INTERFACE_FLASHMEM_NOINLINE interfaceConfigSetDefaults(void)
{
  memset(&interface_cfg, 0, sizeof(interface_cfg));

  interface_cfg.magic = IF_CFG_MAGIC;
  interface_cfg.version = IF_CFG_VERSION;

  interface_cfg.rs232_mode[0] = IF_RS232_MODE_OFF;
  interface_cfg.rs232_mode[1] = IF_RS232_MODE_OFF;
  interface_cfg.rs232_baud_index[0] = 0;   // 9600
  interface_cfg.rs232_baud_index[1] = 0;   // 9600
  interface_cfg.rs232_output_mode[0] = IF_RS232_OUT_CYCLIC;
  interface_cfg.rs232_output_mode[1] = IF_RS232_OUT_CYCLIC;
  interface_cfg.rs232_output_interval_index[0] = 0;  // 1 s
  interface_cfg.rs232_output_interval_index[1] = 0;  // 1 s

  interface_cfg.eth_enabled = 0;
  interface_cfg.eth_dhcp = 1;
  interface_cfg.eth_ip[0] = 192;
  interface_cfg.eth_ip[1] = 168;
  interface_cfg.eth_ip[2] = 0;
  interface_cfg.eth_ip[3] = 50;

  interface_cfg.eth_subnet[0] = 255;
  interface_cfg.eth_subnet[1] = 255;
  interface_cfg.eth_subnet[2] = 255;
  interface_cfg.eth_subnet[3] = 0;

  interface_cfg.eth_gateway[0] = 192;
  interface_cfg.eth_gateway[1] = 168;
  interface_cfg.eth_gateway[2] = 0;
  interface_cfg.eth_gateway[3] = 1;

  interface_cfg.eth_dns[0] = 8;
  interface_cfg.eth_dns[1] = 8;
  interface_cfg.eth_dns[2] = 8;
  interface_cfg.eth_dns[3] = 8;

  interface_cfg.eth_tcp_port = 5000;

  interface_cfg.web_enabled = 0;
  interface_cfg.web_port = 80;
  interface_cfg.web_setup_enabled = 0;

  interface_cfg.usb_mode = IF_USB_MODE_COMMANDS;

  interface_cfg.output_interval_index = 1;   // allgemein: 1 s
  interface_cfg.output_filter_index = 1;     // allgemein: 1 s
  interface_cfg.diagnostic_data_enabled = 0; // allgemein: Produktionsdaten schlank

  interface_cfg.sd_output_interval_index = 1;    // SD: 1 s
  interface_cfg.sd_output_filter_index = 1;      // SD: 1 s
  interface_cfg.sd_diagnostic_data_enabled = 0;  // SD: Metrologie-CSV schlank
  interface_cfg.sd_logging_enabled = 0;
  interface_cfg.sd_interval_index = 2;    // 3 Minuten SD-Schreiben (Default bei 1-s SD-Ausgabe)
  interface_cfg.sd_header_enabled = 1;
  interface_cfg.flow_display_mode = IF_FLOW_DISPLAY_OFF;

  interface_cfg.almemo_address = 0;
  interface_cfg.almemo_channel = 0;
  interface_cfg.almemo_interval_index = IF_ALMEMO_INTERVAL_1S;
  interface_cfg.almemo_trace_enabled = 0;
  interface_cfg.almemo_active[0] = 0;
  interface_cfg.almemo_active[1] = 0;
  interface_cfg.almemo_address2 = 0;
  interface_cfg.almemo_channel2 = 1;
  interface_cfg.almemo_role[0] = IF_ALMEMO_ROLE_TEMP;
  interface_cfg.almemo_role[1] = IF_ALMEMO_ROLE_TEMP;

  interface_cfg.wincontrol_eth_enabled = 0;
  interface_cfg.wincontrol_tcp_port = 10001;
  interface_cfg.wincontrol_address = 0;
  interface_cfg.wincontrol_baud_index = 0;      // 9600 Baud
  interface_cfg.wincontrol_cycle_index = IF_WINCONTROL_CYCLE_20S;

  interface_cfg.sd_log_integrity_mode = TP_LOG_INTEGRITY_OFF;

  interface_cfg.crc = interfaceConfigCrc();
}


static uint32_t INTERFACE_FLASHMEM_NOINLINE interfaceIpToU32(const uint8_t ip[4])
{
  return ((uint32_t)ip[0] << 24) |
         ((uint32_t)ip[1] << 16) |
         ((uint32_t)ip[2] << 8)  |
          (uint32_t)ip[3];
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceIpIsUsableHost(const uint8_t ip[4])
{
  const uint8_t first = ip[0];
  if (first == 0 || first == 127 || first >= 224) return false;
  const uint32_t raw = interfaceIpToU32(ip);
  if (raw == 0x00000000UL || raw == 0xFFFFFFFFUL) return false;
  return true;
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceSubnetIsValid(const uint8_t mask[4])
{
  const uint32_t raw = interfaceIpToU32(mask);
  if (raw == 0x00000000UL || raw == 0xFFFFFFFFUL) return false;

  bool seenZero = false;
  for (int8_t bit = 31; bit >= 0; bit--)
  {
    const bool one = (raw & (1UL << bit)) != 0;
    if (!one) seenZero = true;
    else if (seenZero) return false;
  }
  return true;
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceIpSameSubnet(const uint8_t a[4], const uint8_t b[4], const uint8_t mask[4])
{
  const uint32_t ma = interfaceIpToU32(mask);
  return (interfaceIpToU32(a) & ma) == (interfaceIpToU32(b) & ma);
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceStaticIpConfigValid(void)
{
  if (!interfaceIpIsUsableHost(interface_cfg.eth_ip)) return false;
  if (!interfaceSubnetIsValid(interface_cfg.eth_subnet)) return false;
  if (!interfaceIpIsUsableHost(interface_cfg.eth_gateway)) return false;
  if (!interfaceIpSameSubnet(interface_cfg.eth_ip, interface_cfg.eth_gateway, interface_cfg.eth_subnet)) return false;
  if (memcmp(interface_cfg.eth_ip, interface_cfg.eth_gateway, 4) == 0) return false;
  return true;
}

static void INTERFACE_FLASHMEM_NOINLINE interfaceConfigSanitize(void)
{
  for (uint8_t i = 0; i < 2; i++)
  {
    if (interface_cfg.rs232_mode[i] > IF_RS232_MODE_WINCONTROL)
    {
      interface_cfg.rs232_mode[i] = IF_RS232_MODE_OFF;
    }

    if (interface_cfg.rs232_baud_index[i] >= IF_BAUD_COUNT)
    {
      interface_cfg.rs232_baud_index[i] = 0;
    }

    if (interface_cfg.rs232_output_mode[i] > IF_RS232_OUT_BOTH)
    {
      interface_cfg.rs232_output_mode[i] = IF_RS232_OUT_CYCLIC;
    }

    if (interface_cfg.rs232_output_interval_index[i] >= IF_RS232_INTERVAL_COUNT)
    {
      interface_cfg.rs232_output_interval_index[i] = 0;
    }
  }

  interface_cfg.eth_enabled = interface_cfg.eth_enabled ? 1 : 0;
  interface_cfg.eth_dhcp = interface_cfg.eth_dhcp ? 1 : 0;
  interface_cfg.web_enabled = interface_cfg.web_enabled ? 1 : 0;
  interface_cfg.web_setup_enabled = interface_cfg.web_setup_enabled ? 1 : 0;

  if (interface_cfg.eth_tcp_port < 1) interface_cfg.eth_tcp_port = 5000;
  if (interface_cfg.web_port < 1) interface_cfg.web_port = 80;

  // Statische IP wird hier bewusst NICHT automatisch auf DHCP zurueckgesetzt.
  // Beim Editieren der IP/Subnet/Gateway/DNS-Felder koennen Zwischenstaende
  // kurzzeitig unplausibel sein. In _26 wurde dadurch DHCP scheinbar von selbst
  // wieder eingeschaltet und die Subnetzmaske als "DHCP aktiv" angezeigt.
  // Die harte Plausibilitaetspruefung passiert erst beim Ethernet-Start;
  // ungueltige Static-IP startet dann kein Ethernet.begin(), veraendert aber
  // nicht die gespeicherte DHCP-Auswahl.

  // DNS 0.0.0.0 ist fuer NativeEthernet unnoetig riskant: auf Gateway, sonst 8.8.8.8.
  if (interfaceIpToU32(interface_cfg.eth_dns) == 0UL)
  {
    if (interfaceIpIsUsableHost(interface_cfg.eth_gateway))
    {
      memcpy(interface_cfg.eth_dns, interface_cfg.eth_gateway, sizeof(interface_cfg.eth_dns));
    }
    else
    {
      interface_cfg.eth_dns[0] = 8;
      interface_cfg.eth_dns[1] = 8;
      interface_cfg.eth_dns[2] = 8;
      interface_cfg.eth_dns[3] = 8;
    }
  }

  if (interface_cfg.usb_mode > IF_USB_MODE_DEBUG)
  {
    interface_cfg.usb_mode = IF_USB_MODE_COMMANDS;
  }

  if (interface_cfg.output_interval_index >= IF_OUTPUT_INTERVAL_COUNT)
  {
    interface_cfg.output_interval_index = 1;
  }
  if (interface_cfg.output_filter_index >= IF_OUTPUT_FILTER_COUNT)
  {
    interface_cfg.output_filter_index = 1;
  }
  if (interface_cfg.output_interval_index == 0)
  {
    interface_cfg.output_filter_index = 0;
  }
  else if (interface_cfg.output_interval_index == 1 && interface_cfg.output_filter_index > 1)
  {
    interface_cfg.output_filter_index = 1;
  }
  interface_cfg.diagnostic_data_enabled = interface_cfg.diagnostic_data_enabled ? 1 : 0;

  if (interface_cfg.sd_output_interval_index >= IF_OUTPUT_INTERVAL_COUNT)
  {
    interface_cfg.sd_output_interval_index = 1;
  }
  if (interface_cfg.sd_output_filter_index >= IF_OUTPUT_FILTER_COUNT)
  {
    interface_cfg.sd_output_filter_index = 1;
  }
  if (interface_cfg.sd_output_interval_index == 0)
  {
    interface_cfg.sd_output_filter_index = 0;
  }
  else if (interface_cfg.sd_output_interval_index == 1 && interface_cfg.sd_output_filter_index > 1)
  {
    interface_cfg.sd_output_filter_index = 1;
  }
  interface_cfg.sd_diagnostic_data_enabled = interface_cfg.sd_diagnostic_data_enabled ? 1 : 0;

  interface_cfg.sd_logging_enabled = interface_cfg.sd_logging_enabled ? 1 : 0;
  if (interface_cfg.sd_interval_index >= IF_SD_INTERVAL_COUNT)
  {
    interface_cfg.sd_interval_index = 2;
  }
  interfaceClampSdWriteIntervalToOutput();
  interface_cfg.sd_header_enabled = interface_cfg.sd_header_enabled ? 1 : 0;
  if (interface_cfg.sd_log_integrity_mode >= TP_LOG_INTEGRITY_COUNT)
  {
    interface_cfg.sd_log_integrity_mode = TP_LOG_INTEGRITY_OFF;
  }
  if (interface_cfg.flow_display_mode >= IF_FLOW_DISPLAY_COUNT)
  {
    interface_cfg.flow_display_mode = IF_FLOW_DISPLAY_OFF;
  }

  if (interface_cfg.almemo_address > 99) interface_cfg.almemo_address = 0;
  if (interface_cfg.almemo_channel > 99) interface_cfg.almemo_channel = 0;
  if (interface_cfg.almemo_interval_index == IF_ALMEMO_INTERVAL_ADC ||
      interface_cfg.almemo_interval_index >= IF_ALMEMO_INTERVAL_COUNT)
  {
    interface_cfg.almemo_interval_index = IF_ALMEMO_INTERVAL_1S;
  }
  interface_cfg.almemo_trace_enabled = interface_cfg.almemo_trace_enabled ? 1 : 0;
  interface_cfg.almemo_active[0] = interface_cfg.almemo_active[0] ? 1 : 0;
  interface_cfg.almemo_active[1] = interface_cfg.almemo_active[1] ? 1 : 0;
  if (interface_cfg.almemo_address2 > 99) interface_cfg.almemo_address2 = interface_cfg.almemo_address;
  if (interface_cfg.almemo_channel2 > 99) interface_cfg.almemo_channel2 = 1;
  for (uint8_t i = 0; i < IF_ALMEMO_CHANNEL_COUNT; i++)
  {
    if (interface_cfg.almemo_role[i] == IF_ALMEMO_ROLE_OFF ||
        interface_cfg.almemo_role[i] >= IF_ALMEMO_ROLE_COUNT)
    {
      interface_cfg.almemo_role[i] = IF_ALMEMO_ROLE_TEMP;
    }
  }

  interface_cfg.wincontrol_eth_enabled = interface_cfg.wincontrol_eth_enabled ? 1 : 0;
  if (interface_cfg.wincontrol_tcp_port < 1) interface_cfg.wincontrol_tcp_port = 10001;
  if (interface_cfg.wincontrol_address > 99) interface_cfg.wincontrol_address = 0;
  if (interface_cfg.wincontrol_baud_index >= IF_BAUD_COUNT) interface_cfg.wincontrol_baud_index = 0;
  if (interface_cfg.wincontrol_cycle_index >= IF_WINCONTROL_CYCLE_COUNT)
  {
    interface_cfg.wincontrol_cycle_index = IF_WINCONTROL_CYCLE_20S;
  }

  if (interface_cfg.rs232_mode[0] == IF_RS232_MODE_ALMEMO &&
      interface_cfg.rs232_mode[1] == IF_RS232_MODE_ALMEMO)
  {
    interface_cfg.rs232_mode[1] = IF_RS232_MODE_OFF;
  }
  if (interface_cfg.rs232_mode[0] == IF_RS232_MODE_WINCONTROL &&
      interface_cfg.rs232_mode[1] == IF_RS232_MODE_WINCONTROL)
  {
    interface_cfg.rs232_mode[1] = IF_RS232_MODE_OFF;
  }

  // WinControl-Ausgabe ist bewusst exklusiv: genau eine Quelle
  // (Aus, RS232-1, RS232-2 oder Ethernet), keine parallelen Clients.
  if ((interface_cfg.rs232_mode[0] == IF_RS232_MODE_WINCONTROL ||
       interface_cfg.rs232_mode[1] == IF_RS232_MODE_WINCONTROL) &&
      interface_cfg.wincontrol_eth_enabled)
  {
    interface_cfg.wincontrol_eth_enabled = 0;
  }
}

void INTERFACE_FLASHMEM_NOINLINE interfaceConfigSave(void)
{
  interface_cfg.magic = IF_CFG_MAGIC;
  interface_cfg.version = IF_CFG_VERSION;
  interfaceConfigSanitize();
  interface_cfg.crc = interfaceConfigCrc();

  interface_config_slot_t slotA;
  interface_config_slot_t slotB;
  const bool validA = interfaceConfigReadSlot(0, slotA);
  const bool validB = interfaceConfigReadSlot(1, slotB);

  uint32_t nextSeq = interface_cfg_sequence;
  if (validA && slotA.sequence > nextSeq) nextSeq = slotA.sequence;
  if (validB && slotB.sequence > nextSeq) nextSeq = slotB.sequence;
  nextSeq++;
  if (nextSeq == 0UL) nextSeq = 1UL;

  uint8_t targetSlot;
  if (!validA) targetSlot = 0;
  else if (!validB) targetSlot = 1;
  else targetSlot = (slotA.sequence <= slotB.sequence) ? 0 : 1;

  interface_config_slot_t out;
  memset(&out, 0, sizeof(out));
  out.magic = IF_CFG_SLOT_MAGIC;
  out.version = IF_CFG_SLOT_VERSION;
  out.size = sizeof(interface_config_t);
  out.sequence = nextSeq;
  out.payload = interface_cfg;
  out.crc = interfaceConfigCrcBytes(&out, sizeof(out) - sizeof(out.crc));

  EEPROM.put(interfaceConfigSlotAddr(targetSlot), out);
  interface_cfg_sequence = nextSeq;
  interface_cfg_active_slot = targetSlot;
  interface_cfg_loaded = true;
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceConfigTryMigrateV8(void)
{
  interface_config_v8_t oldCfg;
  EEPROM.get(interfaceConfigEepromAddr(), oldCfg);

  const uint32_t oldCrc = interfaceConfigCrcBytes(&oldCfg,
                            sizeof(oldCfg) - sizeof(oldCfg.crc));
  if (oldCfg.magic != IF_CFG_MAGIC || oldCfg.version != 8 || oldCfg.crc != oldCrc)
  {
    return false;
  }

  interfaceConfigSetDefaults();
  memcpy(interface_cfg.rs232_mode, oldCfg.rs232_mode, sizeof(oldCfg.rs232_mode));
  memcpy(interface_cfg.rs232_baud_index, oldCfg.rs232_baud_index, sizeof(oldCfg.rs232_baud_index));
  memcpy(interface_cfg.rs232_output_mode, oldCfg.rs232_output_mode, sizeof(oldCfg.rs232_output_mode));
  memcpy(interface_cfg.rs232_output_interval_index, oldCfg.rs232_output_interval_index, sizeof(oldCfg.rs232_output_interval_index));
  interface_cfg.eth_enabled = oldCfg.eth_enabled;
  interface_cfg.eth_dhcp = oldCfg.eth_dhcp;
  memcpy(interface_cfg.eth_ip, oldCfg.eth_ip, sizeof(oldCfg.eth_ip));
  memcpy(interface_cfg.eth_subnet, oldCfg.eth_subnet, sizeof(oldCfg.eth_subnet));
  memcpy(interface_cfg.eth_gateway, oldCfg.eth_gateway, sizeof(oldCfg.eth_gateway));
  memcpy(interface_cfg.eth_dns, oldCfg.eth_dns, sizeof(oldCfg.eth_dns));
  interface_cfg.eth_tcp_port = oldCfg.eth_tcp_port;
  interface_cfg.web_enabled = oldCfg.web_enabled;
  interface_cfg.web_port = oldCfg.web_port;
  interface_cfg.web_setup_enabled = oldCfg.web_setup_enabled;
  interface_cfg.usb_mode = oldCfg.usb_mode;
  interface_cfg.output_interval_index = oldCfg.output_interval_index;
  interface_cfg.output_filter_index = oldCfg.output_filter_index;
  interface_cfg.diagnostic_data_enabled = oldCfg.diagnostic_data_enabled;
  interface_cfg.sd_output_interval_index = oldCfg.sd_output_interval_index;
  interface_cfg.sd_output_filter_index = oldCfg.sd_output_filter_index;
  interface_cfg.sd_diagnostic_data_enabled = oldCfg.sd_diagnostic_data_enabled;
  interface_cfg.sd_logging_enabled = oldCfg.sd_logging_enabled;
  interface_cfg.sd_interval_index = oldCfg.sd_interval_index;
  interface_cfg.sd_header_enabled = oldCfg.sd_header_enabled;
  interface_cfg.flow_display_mode = oldCfg.flow_display_mode;
  interface_cfg.almemo_address = oldCfg.almemo_address;
  interface_cfg.almemo_channel = oldCfg.almemo_channel;
  interface_cfg.almemo_interval_index = oldCfg.almemo_interval_index;
  interface_cfg.almemo_trace_enabled = oldCfg.almemo_trace_enabled;
  memcpy(interface_cfg.almemo_active, oldCfg.almemo_active, sizeof(oldCfg.almemo_active));
  interface_cfg.almemo_address2 = oldCfg.almemo_address2;
  interface_cfg.almemo_channel2 = oldCfg.almemo_channel2;
  memcpy(interface_cfg.almemo_role, oldCfg.almemo_role, sizeof(oldCfg.almemo_role));
  interfaceConfigSave();
  return true;
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceConfigTryMigrateV7(void)
{
  interface_config_v7_t oldCfg;
  EEPROM.get(interfaceConfigEepromAddr(), oldCfg);

  const uint32_t oldCrc = interfaceConfigCrcBytes(&oldCfg,
                            sizeof(oldCfg) - sizeof(oldCfg.crc));
  if (oldCfg.magic != IF_CFG_MAGIC || oldCfg.version != 7 || oldCfg.crc != oldCrc)
  {
    return false;
  }

  interfaceConfigSetDefaults();
  memcpy(interface_cfg.rs232_mode, oldCfg.rs232_mode, sizeof(oldCfg.rs232_mode));
  memcpy(interface_cfg.rs232_baud_index, oldCfg.rs232_baud_index, sizeof(oldCfg.rs232_baud_index));
  memcpy(interface_cfg.rs232_output_mode, oldCfg.rs232_output_mode, sizeof(oldCfg.rs232_output_mode));
  memcpy(interface_cfg.rs232_output_interval_index, oldCfg.rs232_output_interval_index, sizeof(oldCfg.rs232_output_interval_index));
  interface_cfg.eth_enabled = oldCfg.eth_enabled;
  interface_cfg.eth_dhcp = oldCfg.eth_dhcp;
  memcpy(interface_cfg.eth_ip, oldCfg.eth_ip, sizeof(oldCfg.eth_ip));
  memcpy(interface_cfg.eth_subnet, oldCfg.eth_subnet, sizeof(oldCfg.eth_subnet));
  memcpy(interface_cfg.eth_gateway, oldCfg.eth_gateway, sizeof(oldCfg.eth_gateway));
  memcpy(interface_cfg.eth_dns, oldCfg.eth_dns, sizeof(oldCfg.eth_dns));
  interface_cfg.eth_tcp_port = oldCfg.eth_tcp_port;
  interface_cfg.web_enabled = oldCfg.web_enabled;
  interface_cfg.web_port = oldCfg.web_port;
  interface_cfg.web_setup_enabled = oldCfg.web_setup_enabled;
  interface_cfg.usb_mode = oldCfg.usb_mode;
  interface_cfg.output_interval_index = oldCfg.output_interval_index;
  interface_cfg.output_filter_index = oldCfg.output_filter_index;
  interface_cfg.diagnostic_data_enabled = oldCfg.diagnostic_data_enabled;
  interface_cfg.sd_output_interval_index = oldCfg.sd_output_interval_index;
  interface_cfg.sd_output_filter_index = oldCfg.sd_output_filter_index;
  interface_cfg.sd_diagnostic_data_enabled = oldCfg.sd_diagnostic_data_enabled;
  interface_cfg.sd_logging_enabled = oldCfg.sd_logging_enabled;
  interface_cfg.sd_interval_index = oldCfg.sd_interval_index;
  interface_cfg.sd_header_enabled = oldCfg.sd_header_enabled;
  interface_cfg.flow_display_mode = oldCfg.flow_display_mode;
  interface_cfg.almemo_address = oldCfg.almemo_address;
  interface_cfg.almemo_channel = oldCfg.almemo_channel;
  interface_cfg.almemo_interval_index = oldCfg.almemo_interval_index;
  interface_cfg.almemo_trace_enabled = oldCfg.almemo_trace_enabled;
  interface_cfg.almemo_address2 = oldCfg.almemo_address;
  interface_cfg.almemo_channel2 = (oldCfg.almemo_channel < 99U) ? (uint8_t)(oldCfg.almemo_channel + 1U) : 1U;
  interface_cfg.almemo_role[0] = IF_ALMEMO_ROLE_TEMP;
  interface_cfg.almemo_role[1] = IF_ALMEMO_ROLE_TEMP;
  interface_cfg.almemo_active[0] = (oldCfg.flow_display_mode == IF_FLOW_DISPLAY_T_EXT ||
                                   oldCfg.rs232_mode[0] == IF_RS232_MODE_ALMEMO ||
                                   oldCfg.rs232_mode[1] == IF_RS232_MODE_ALMEMO) ? 1 : 0;
  interface_cfg.almemo_active[1] = 0;
  if (interface_cfg.flow_display_mode == IF_FLOW_DISPLAY_T_EXT)
  {
    interface_cfg.flow_display_mode = IF_FLOW_DISPLAY_OFF;
  }
  interfaceConfigSave();
  return true;
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceConfigTryMigrateV6(void)
{
  interface_config_v6_t oldCfg;
  EEPROM.get(interfaceConfigEepromAddr(), oldCfg);

  const uint32_t oldCrc = interfaceConfigCrcBytes(&oldCfg,
                            sizeof(oldCfg) - sizeof(oldCfg.crc));
  if (oldCfg.magic != IF_CFG_MAGIC || oldCfg.version != 6 || oldCfg.crc != oldCrc)
  {
    return false;
  }

  interfaceConfigSetDefaults();
  memcpy(interface_cfg.rs232_mode, oldCfg.rs232_mode, sizeof(oldCfg.rs232_mode));
  memcpy(interface_cfg.rs232_baud_index, oldCfg.rs232_baud_index, sizeof(oldCfg.rs232_baud_index));
  memcpy(interface_cfg.rs232_output_mode, oldCfg.rs232_output_mode, sizeof(oldCfg.rs232_output_mode));
  memcpy(interface_cfg.rs232_output_interval_index, oldCfg.rs232_output_interval_index, sizeof(oldCfg.rs232_output_interval_index));
  interface_cfg.eth_enabled = oldCfg.eth_enabled;
  interface_cfg.eth_dhcp = oldCfg.eth_dhcp;
  memcpy(interface_cfg.eth_ip, oldCfg.eth_ip, sizeof(oldCfg.eth_ip));
  memcpy(interface_cfg.eth_subnet, oldCfg.eth_subnet, sizeof(oldCfg.eth_subnet));
  memcpy(interface_cfg.eth_gateway, oldCfg.eth_gateway, sizeof(oldCfg.eth_gateway));
  memcpy(interface_cfg.eth_dns, oldCfg.eth_dns, sizeof(oldCfg.eth_dns));
  interface_cfg.eth_tcp_port = oldCfg.eth_tcp_port;
  interface_cfg.web_enabled = oldCfg.web_enabled;
  interface_cfg.web_port = oldCfg.web_port;
  interface_cfg.web_setup_enabled = oldCfg.web_setup_enabled;
  interface_cfg.usb_mode = oldCfg.usb_mode;
  interface_cfg.output_interval_index = oldCfg.output_interval_index;
  interface_cfg.output_filter_index = oldCfg.output_filter_index;
  interface_cfg.diagnostic_data_enabled = oldCfg.diagnostic_data_enabled;
  interface_cfg.sd_output_interval_index = oldCfg.sd_output_interval_index;
  interface_cfg.sd_output_filter_index = oldCfg.sd_output_filter_index;
  interface_cfg.sd_diagnostic_data_enabled = oldCfg.sd_diagnostic_data_enabled;
  interface_cfg.sd_logging_enabled = oldCfg.sd_logging_enabled;
  interface_cfg.sd_interval_index = oldCfg.sd_interval_index;
  interface_cfg.sd_header_enabled = oldCfg.sd_header_enabled;
  interface_cfg.flow_display_mode = oldCfg.flow_display_mode;
  interface_cfg.almemo_address = oldCfg.almemo_address;
  interface_cfg.almemo_channel = oldCfg.almemo_channel;
  interface_cfg.almemo_interval_index = oldCfg.almemo_interval_index;
  interface_cfg.almemo_trace_enabled = 0;
  interface_cfg.almemo_active[0] = (oldCfg.flow_display_mode == IF_FLOW_DISPLAY_T_EXT ||
                                   oldCfg.rs232_mode[0] == IF_RS232_MODE_ALMEMO ||
                                   oldCfg.rs232_mode[1] == IF_RS232_MODE_ALMEMO) ? 1 : 0;
  interface_cfg.almemo_active[1] = 0;
  interface_cfg.almemo_address2 = oldCfg.almemo_address;
  interface_cfg.almemo_channel2 = (oldCfg.almemo_channel < 99U) ? (uint8_t)(oldCfg.almemo_channel + 1U) : 1U;
  interface_cfg.almemo_role[0] = IF_ALMEMO_ROLE_TEMP;
  interface_cfg.almemo_role[1] = IF_ALMEMO_ROLE_TEMP;
  if (interface_cfg.flow_display_mode == IF_FLOW_DISPLAY_T_EXT) interface_cfg.flow_display_mode = IF_FLOW_DISPLAY_OFF;
  interfaceConfigSave();
  return true;
}

static bool INTERFACE_FLASHMEM_NOINLINE interfaceConfigTryMigrateV5(void)
{
  interface_config_v5_t oldCfg;
  EEPROM.get(interfaceConfigEepromAddr(), oldCfg);

  const uint32_t oldCrc = interfaceConfigCrcBytes(&oldCfg,
                            sizeof(oldCfg) - sizeof(oldCfg.crc));
  if (oldCfg.magic != IF_CFG_MAGIC || oldCfg.version != 5 || oldCfg.crc != oldCrc)
  {
    return false;
  }

  interfaceConfigSetDefaults();
  memcpy(interface_cfg.rs232_mode, oldCfg.rs232_mode, sizeof(oldCfg.rs232_mode));
  memcpy(interface_cfg.rs232_baud_index, oldCfg.rs232_baud_index, sizeof(oldCfg.rs232_baud_index));
  memcpy(interface_cfg.rs232_output_mode, oldCfg.rs232_output_mode, sizeof(oldCfg.rs232_output_mode));
  memcpy(interface_cfg.rs232_output_interval_index, oldCfg.rs232_output_interval_index, sizeof(oldCfg.rs232_output_interval_index));
  interface_cfg.eth_enabled = oldCfg.eth_enabled;
  interface_cfg.eth_dhcp = oldCfg.eth_dhcp;
  memcpy(interface_cfg.eth_ip, oldCfg.eth_ip, sizeof(oldCfg.eth_ip));
  memcpy(interface_cfg.eth_subnet, oldCfg.eth_subnet, sizeof(oldCfg.eth_subnet));
  memcpy(interface_cfg.eth_gateway, oldCfg.eth_gateway, sizeof(oldCfg.eth_gateway));
  memcpy(interface_cfg.eth_dns, oldCfg.eth_dns, sizeof(oldCfg.eth_dns));
  interface_cfg.eth_tcp_port = oldCfg.eth_tcp_port;
  interface_cfg.web_enabled = oldCfg.web_enabled;
  interface_cfg.web_port = oldCfg.web_port;
  interface_cfg.web_setup_enabled = oldCfg.web_setup_enabled;
  interface_cfg.usb_mode = oldCfg.usb_mode;
  interface_cfg.output_interval_index = oldCfg.output_interval_index;
  interface_cfg.output_filter_index = oldCfg.output_filter_index;
  interface_cfg.diagnostic_data_enabled = oldCfg.diagnostic_data_enabled;
  interface_cfg.sd_output_interval_index = oldCfg.sd_output_interval_index;
  interface_cfg.sd_output_filter_index = oldCfg.sd_output_filter_index;
  interface_cfg.sd_diagnostic_data_enabled = oldCfg.sd_diagnostic_data_enabled;
  interface_cfg.sd_logging_enabled = oldCfg.sd_logging_enabled;
  interface_cfg.sd_interval_index = oldCfg.sd_interval_index;
  interface_cfg.sd_header_enabled = oldCfg.sd_header_enabled;
  interface_cfg.flow_display_mode = oldCfg.flow_display_mode;
  interface_cfg.almemo_active[0] = (oldCfg.flow_display_mode == IF_FLOW_DISPLAY_T_EXT) ? 1 : 0;
  interface_cfg.almemo_active[1] = 0;
  interface_cfg.almemo_address = 0;
  interface_cfg.almemo_channel = 0;
  interface_cfg.almemo_address2 = 0;
  interface_cfg.almemo_channel2 = 1;
  interface_cfg.almemo_role[0] = IF_ALMEMO_ROLE_TEMP;
  interface_cfg.almemo_role[1] = IF_ALMEMO_ROLE_TEMP;
  if (interface_cfg.flow_display_mode == IF_FLOW_DISPLAY_T_EXT) interface_cfg.flow_display_mode = IF_FLOW_DISPLAY_OFF;
  interfaceConfigSave();
  return true;
}

void interfaceConfigLoad(void)
{
  if (interface_cfg_loaded) return;

  interface_config_slot_t slotA;
  interface_config_slot_t slotB;
  const bool validA = interfaceConfigReadSlot(0, slotA);
  const bool validB = interfaceConfigReadSlot(1, slotB);

  if (!validA && !validB)
  {
    interface_cfg_sequence = 0UL;
    interface_cfg_active_slot = 0U;

    // Vor dem Rueckfall auf Werkseinstellungen die noch vorhandenen, CRC-
    // geschuetzten Einzelblockformate V8 bis V5 uebernehmen. Die Helfer
    // schreiben den ersten aktuellen A/B-Slot; der zweite Save stellt die
    // Redundanz unmittelbar wieder her.
    const bool migrated = interfaceConfigTryMigrateV8() ||
                          interfaceConfigTryMigrateV7() ||
                          interfaceConfigTryMigrateV6() ||
                          interfaceConfigTryMigrateV5();
    if (migrated)
    {
      interfaceConfigSave();
      interface_cfg_loaded = true;
      return;
    }

    interfaceConfigSetDefaults();
    // Erstinitialisierung: beide A/B-Slots direkt anlegen.
    interfaceConfigSave();
    interfaceConfigSave();
    interface_cfg_loaded = true;
    return;
  }

  const interface_config_slot_t* best = nullptr;
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

  interface_cfg = best->payload;
  interface_cfg_sequence = best->sequence;
  interface_cfg_active_slot = bestSlot;

  // V11 hatte an derselben Byteposition lediglich ein durch memset() auf null
  // gesetztes Padding-Byte. Erst nach erfolgreicher alter CRC-Pruefung wird es
  // als neuer Integritaetsmodus initialisiert.
  const bool migratedFromV11 = interface_cfg.version == IF_CFG_PREVIOUS_VERSION;
  if (migratedFromV11)
  {
    interface_cfg.sd_log_integrity_mode = TP_LOG_INTEGRITY_OFF;
  }
  interfaceConfigSanitize();

  // Falls Sanitize Werte repariert hat, wird der zweite Slot sauber aktualisiert.
  interface_cfg.magic = IF_CFG_MAGIC;
  interface_cfg.version = IF_CFG_VERSION;
  const uint32_t oldCrc = interface_cfg.crc;
  interface_cfg.crc = interfaceConfigCrc();
  if (migratedFromV11)
  {
    // Beide Slots sofort auf V12 anheben, damit nach dem ersten Boot wieder ein
    // vollstaendig redundanter, einheitlicher A/B-Stand vorhanden ist.
    interfaceConfigSave();
    interfaceConfigSave();
  }
  else if (interface_cfg.crc != oldCrc || validA != validB)
  {
    // A/B-Self-Heal: bei nur einem gueltigen Schnittstellen-Slot wird der
    // defekte/leere Slot sofort wieder aufgebaut. Das ist besonders wichtig
    // fuer statische IP-Konfigurationen, damit nie laenger nur ein Slot traegt.
    interfaceConfigSave();
  }

  interface_cfg_loaded = true;
}

void interfaceConfigReloadFromEeprom(void)
{
  interface_cfg_loaded = false;
  interfaceConfigLoad();
}

void interfaceApplySerialBaud(void)
{
  interfaceConfigLoad();

  Serial7.begin(interfaceBaudTable[interface_cfg.rs232_baud_index[0]]);
  Serial8.begin(interfaceBaudTable[interface_cfg.rs232_baud_index[1]]);
}

// ============================================================================
// OEFFENTLICH: SNAPSHOT FUER GERAETE-EINSTELLUNGEN
// ============================================================================
// Wird vom Menue "Geraete-Speicher" benutzt, um die Schnittstellen-
// einstellungen zusammen mit den normalen Geraeteeinstellungen als Backup
// zu sichern bzw. wiederherzustellen. Die Kalibrier-/Justierdaten bleiben
// davon getrennt.

bool FLASHMEM interfaceConfigExport(uint8_t* dst, uint16_t dstSize, uint16_t* usedSize)
{
  interfaceConfigLoad();

  if (usedSize) *usedSize = (uint16_t)sizeof(interface_config_t);
  if (dst == nullptr) return false;
  if (dstSize < sizeof(interface_config_t)) return false;

  interface_cfg.magic = IF_CFG_MAGIC;
  interface_cfg.version = IF_CFG_VERSION;
  interfaceConfigSanitize();
  interface_cfg.crc = interfaceConfigCrc();

  memcpy(dst, &interface_cfg, sizeof(interface_config_t));
  return true;
}

bool FLASHMEM interfaceConfigImport(const uint8_t* src, uint16_t srcSize)
{
  if (src == nullptr) return false;
  if (srcSize != sizeof(interface_config_t)) return false;

  interface_config_t tmp;
  memcpy(&tmp, src, sizeof(tmp));

  const uint32_t tmpCrc = interfaceConfigCrcBytes(&tmp, sizeof(tmp) - sizeof(tmp.crc));
  if (tmp.magic != IF_CFG_MAGIC) return false;
  if (tmp.version != IF_CFG_VERSION && tmp.version != IF_CFG_PREVIOUS_VERSION) return false;
  if (tmp.crc != tmpCrc) return false;

  if (tmp.version == IF_CFG_PREVIOUS_VERSION)
  {
    tmp.sd_log_integrity_mode = TP_LOG_INTEGRITY_OFF;
    tmp.version = IF_CFG_VERSION;
  }
  interface_cfg = tmp;
  interfaceConfigSanitize();
  interfaceConfigSave();
  interfaceApplySerialBaud();

  return true;
}


uint8_t interfaceRs232Mode(uint8_t port)
{
  interfaceConfigLoad();
  if (port > 1) port = 0;
  return interface_cfg.rs232_mode[port];
}

uint8_t interfaceRs232OutputMode(uint8_t port)
{
  interfaceConfigLoad();
  if (port > 1) port = 0;
  return interface_cfg.rs232_output_mode[port];
}

// =========================================================================
// WEB-SETUP: kompakter, gepruefter Zugriff auf die bestehende Konfiguration
// =========================================================================
// Die Weboberflaeche darf die interne EEPROM-Struktur nicht direkt kennen.
// Diese Funktionen bilden deshalb nur die im Web sinnvollen Einstellungen ab.
// Ethernet/Webserver selbst bleiben absichtlich nur lesbar, damit die aktive
// Webverbindung nicht durch die eigene Bedienung abgeschaltet wird.

uint8_t FLASHMEM interfaceRs232BaudIndex(uint8_t port)
{
  interfaceConfigLoad();
  if (port > 1) port = 0;
  if (interface_cfg.rs232_baud_index[port] >= IF_BAUD_COUNT)
  {
    interface_cfg.rs232_baud_index[port] = 0;
  }
  return interface_cfg.rs232_baud_index[port];
}

uint8_t FLASHMEM interfaceUsbMode(void)
{
  interfaceConfigLoad();
  if (interface_cfg.usb_mode > IF_USB_MODE_DEBUG)
  {
    interface_cfg.usb_mode = IF_USB_MODE_COMMANDS;
  }
  return interface_cfg.usb_mode;
}

static bool FLASHMEM interfaceWebFilterAllowed(uint8_t outputIndex, uint8_t filterIndex)
{
  if (outputIndex >= IF_OUTPUT_INTERVAL_COUNT) return false;
  if (filterIndex >= IF_OUTPUT_FILTER_COUNT) return false;
  if (outputIndex == 0) return filterIndex == 0;       // ADC: nur 0 s
  if (outputIndex == 1) return filterIndex <= 1;       // 1 s: 0 s / 1 s
  return true;
}

bool FLASHMEM interfaceWebSetGeneralOutput(uint8_t outputIndex, uint8_t filterIndex, bool diagnosticEnabled)
{
  interfaceConfigLoad();
  if (!interfaceWebFilterAllowed(outputIndex, filterIndex)) return false;

  interface_cfg.output_interval_index = outputIndex;
  interface_cfg.output_filter_index = filterIndex;
  interface_cfg.diagnostic_data_enabled = diagnosticEnabled ? 1 : 0;
  interfaceConfigSave();
  sdLogResetSchedule();
  return true;
}

bool FLASHMEM interfaceWebSetRs232(uint8_t port,
                          uint8_t mode,
                          uint8_t baudIndex,
                          uint8_t outputMode)
{
  interfaceConfigLoad();
  if (port > 1) return false;
  if (mode > IF_RS232_MODE_WINCONTROL) return false;
  if (baudIndex >= IF_BAUD_COUNT) return false;
  if (outputMode > IF_RS232_OUT_BOTH) return false;

  if (mode == IF_RS232_MODE_ALMEMO || mode == IF_RS232_MODE_WINCONTROL)
  {
    interface_cfg.rs232_mode[port ^ 1U] = IF_RS232_MODE_OFF;
  }
  interface_cfg.rs232_mode[port] = mode;
  interface_cfg.rs232_baud_index[port] = baudIndex;
  if (mode == IF_RS232_MODE_WINCONTROL)
  {
    interface_cfg.wincontrol_baud_index = baudIndex;
  }
  interface_cfg.rs232_output_mode[port] = outputMode;
  interfaceConfigSave();
  interfaceApplySerialBaud();
  return true;
}

bool FLASHMEM interfaceWebSetUsb(uint8_t mode)
{
  interfaceConfigLoad();
  if (mode > IF_USB_MODE_DEBUG) return false;
  interface_cfg.usb_mode = mode;
  interfaceConfigSave();
  return true;
}

bool FLASHMEM interfaceWebSetFlowDisplay(uint8_t mode)
{
  interfaceConfigLoad();
  if (mode >= IF_FLOW_DISPLAY_COUNT) return false;
  interface_cfg.flow_display_mode = mode;
  interfaceConfigSave();
  return true;
}

bool FLASHMEM interfaceWebSetAlmemo(uint8_t address, uint8_t channel, uint8_t intervalIndex)
{
  return interfaceWebSetAlmemoChannel(0, 1, address, channel, IF_ALMEMO_ROLE_TEMP) &&
         interfaceWebSetAlmemoInterval(intervalIndex);
}

bool FLASHMEM interfaceWebSetAlmemoChannel(uint8_t index, uint8_t active, uint8_t address, uint8_t channel, uint8_t role)
{
  interfaceConfigLoad();
  if (index >= IF_ALMEMO_CHANNEL_COUNT) return false;
  if (address > 99 || channel > 99) return false;
  if (role == IF_ALMEMO_ROLE_OFF || role >= IF_ALMEMO_ROLE_COUNT) return false;

  interface_cfg.almemo_active[index] = active ? 1 : 0;
  if (index == 0)
  {
    interface_cfg.almemo_address = address;
    interface_cfg.almemo_channel = channel;
  }
  else
  {
    interface_cfg.almemo_address2 = address;
    interface_cfg.almemo_channel2 = channel;
  }
  interface_cfg.almemo_role[index] = role;
  interfaceConfigSave();
  return true;
}

bool FLASHMEM interfaceWebSetAlmemoInterval(uint8_t intervalIndex)
{
  interfaceConfigLoad();
  if (intervalIndex == IF_ALMEMO_INTERVAL_ADC) intervalIndex = IF_ALMEMO_INTERVAL_1S;
  if (intervalIndex >= IF_ALMEMO_INTERVAL_COUNT) return false;
  interface_cfg.almemo_interval_index = intervalIndex;
  interfaceConfigSave();
  return true;
}

bool FLASHMEM interfaceWebSetAlmemoTrace(bool enabled)
{
  interfaceConfigLoad();
  const uint8_t newValue = enabled ? 1U : 0U;
  if (interface_cfg.almemo_trace_enabled == newValue) return true;

  interface_cfg.almemo_trace_enabled = newValue;
  interfaceConfigSave();

  // Wie im TFT-Menue: SD-Dateioperationen nur bei ausdruecklicher
  // Bedienaktion ausfuehren und als bekannte blockierende Operation
  // kennzeichnen. Das normale CSV-Logging bleibt davon unabhaengig.
  safetyBeginBlockingOperation();
  if (interface_cfg.almemo_trace_enabled && !sdLogReadyForLogging())
  {
    sdLogEnsureReadyForAccess();
  }
  bool ok = serialProtocolTraceApplyNow();
  safetyEndBlockingOperation();

  if (interface_cfg.almemo_trace_enabled && !ok)
  {
    interface_cfg.almemo_trace_enabled = 0;
    interfaceConfigSave();
    serialProtocolTraceApplyNow();
    return false;
  }

  return true;
}

bool FLASHMEM interfaceWebSetSd(uint8_t outputIndex,
                                uint8_t filterIndex,
                                uint8_t writeIntervalIndex,
                                bool loggingEnabled,
                                bool diagnosticEnabled,
                                bool headerEnabled,
                                uint8_t integrityMode)
{
  interfaceConfigLoad();
  if (!interfaceWebFilterAllowed(outputIndex, filterIndex)) return false;
  if (writeIntervalIndex >= IF_SD_INTERVAL_COUNT) return false;
  if (integrityMode >= TP_LOG_INTEGRITY_COUNT) return false;
  if (tpLogIntegrityIsCertified(integrityMode) && !tpSignedDataCertifiedModeReady()) return false;

  const uint8_t maxWriteIndex = interfaceSdMaxWriteIntervalIndexForOutput(outputIndex);
  if (writeIntervalIndex > maxWriteIndex) return false;

  const bool wasLogging = interface_cfg.sd_logging_enabled != 0;

  interface_cfg.sd_output_interval_index = outputIndex;
  interface_cfg.sd_output_filter_index = filterIndex;
  interface_cfg.sd_interval_index = writeIntervalIndex;
  interface_cfg.sd_diagnostic_data_enabled = diagnosticEnabled ? 1 : 0;
  interface_cfg.sd_header_enabled = headerEnabled ? 1 : 0;
  interface_cfg.sd_log_integrity_mode = integrityMode;
  interface_cfg.sd_logging_enabled = loggingEnabled ? 1 : 0;
  interfaceConfigSave();
  sdLogResetSchedule();

  // Beim Einschalten wie im TFT-Menue genau einmal Karte/Dateisystem pruefen.
  if (!wasLogging && loggingEnabled)
  {
    safetyBeginBlockingOperation();
    sdLogBegin();
    safetyEndBlockingOperation();

    if (!sdLogReadyForLogging())
    {
      interface_cfg.sd_logging_enabled = 0;
      interfaceConfigSave();
      sdLogResetSchedule();
      return false;
    }
  }

  return true;
}

static uint8_t INTERFACE_FLASHMEM_NOINLINE interfaceDefaultFilterForOutput(uint8_t outputIndex)
{
  switch (outputIndex)
  {
    case 0: return 0; // ADC -> 0 s
    case 1: return 1; // 1 s  -> 1 s
    case 2: return 1; // 10 s -> 1 s
    case 3: return 2; // 30 s -> 3 s
    case 4: return 2; // 60 s -> 3 s
    default: return 1;
  }
}

static uint8_t INTERFACE_FLASHMEM_NOINLINE interfaceSdMaxWriteIntervalIndexForOutput(uint8_t outputIndex)
{
  // SD-Schreibpuffer bewusst klein halten:
  // ADC-Ausgabe ca. 3,3 Hz -> max. 60 s
  // 1-s-Ausgabe             -> max. 3 min
  // langsamere Ausgabe      -> max. 5 min
  if (outputIndex == 0) return 1; // 10 s / 60 s
  if (outputIndex == 1) return 2; // 10 s / 60 s / 3 min
  return 3;                       // + 5 min
}

static void INTERFACE_FLASHMEM_NOINLINE interfaceClampSdWriteIntervalToOutput(void)
{
  uint8_t maxIdx = interfaceSdMaxWriteIntervalIndexForOutput(interface_cfg.sd_output_interval_index);
  if (interface_cfg.sd_interval_index > maxIdx)
  {
    interface_cfg.sd_interval_index = maxIdx;
  }
}

uint8_t interfaceOutputIntervalIndex(void)
{
  interfaceConfigLoad();
  if (interface_cfg.output_interval_index >= IF_OUTPUT_INTERVAL_COUNT) interface_cfg.output_interval_index = 1;
  return interface_cfg.output_interval_index;
}

uint32_t interfaceOutputIntervalMsValue(void)
{
  interfaceConfigLoad();
  uint8_t idx = interfaceOutputIntervalIndex();
  return interfaceOutputIntervalMs[idx];
}

uint8_t interfaceOutputFilterIndex(void)
{
  interfaceConfigLoad();
  if (interface_cfg.output_filter_index >= IF_OUTPUT_FILTER_COUNT) interface_cfg.output_filter_index = 1;
  if (interface_cfg.output_interval_index == 0) interface_cfg.output_filter_index = 0;
  if (interface_cfg.output_interval_index == 1 && interface_cfg.output_filter_index > 1) interface_cfg.output_filter_index = 1;
  return interface_cfg.output_filter_index;
}

uint8_t interfaceOutputFilterSecondsValue(void)
{
  interfaceConfigLoad();
  uint8_t idx = interfaceOutputFilterIndex();
  return interfaceOutputFilterSec[idx];
}

bool interfaceDiagnosticDataEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.diagnostic_data_enabled != 0;
}

uint8_t interfaceSdOutputIntervalIndex(void)
{
  interfaceConfigLoad();
  if (interface_cfg.sd_output_interval_index >= IF_OUTPUT_INTERVAL_COUNT) interface_cfg.sd_output_interval_index = 1;
  return interface_cfg.sd_output_interval_index;
}

uint32_t interfaceSdOutputIntervalMsValue(void)
{
  interfaceConfigLoad();
  uint8_t idx = interfaceSdOutputIntervalIndex();
  return interfaceOutputIntervalMs[idx];
}

uint8_t interfaceSdOutputFilterIndex(void)
{
  interfaceConfigLoad();
  if (interface_cfg.sd_output_filter_index >= IF_OUTPUT_FILTER_COUNT) interface_cfg.sd_output_filter_index = 1;
  if (interface_cfg.sd_output_interval_index == 0) interface_cfg.sd_output_filter_index = 0;
  if (interface_cfg.sd_output_interval_index == 1 && interface_cfg.sd_output_filter_index > 1) interface_cfg.sd_output_filter_index = 1;
  return interface_cfg.sd_output_filter_index;
}

uint8_t interfaceSdOutputFilterSecondsValue(void)
{
  interfaceConfigLoad();
  uint8_t idx = interfaceSdOutputFilterIndex();
  return interfaceOutputFilterSec[idx];
}

bool interfaceSdDiagnosticDataEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.sd_diagnostic_data_enabled != 0;
}

uint32_t interfaceRs232OutputIntervalMsValue(uint8_t port)
{
  (void)port;
  return interfaceOutputIntervalMsValue();
}

bool interfaceSdLoggingEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.sd_logging_enabled != 0;
}

uint32_t interfaceSdLogIntervalMsValue(void)
{
  interfaceConfigLoad();
  interfaceClampSdWriteIntervalToOutput();
  uint8_t idx = interface_cfg.sd_interval_index;
  if (idx >= IF_SD_INTERVAL_COUNT) idx = 2;
  return interfaceSdIntervalMs[idx];
}

bool interfaceSdLogHeaderEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.sd_header_enabled != 0;
}

uint8_t interfaceSdLogIntegrityMode(void)
{
  interfaceConfigLoad();
  if (interface_cfg.sd_log_integrity_mode >= TP_LOG_INTEGRITY_COUNT)
  {
    interface_cfg.sd_log_integrity_mode = TP_LOG_INTEGRITY_OFF;
  }
  return interface_cfg.sd_log_integrity_mode;
}

bool interfaceSdCertifiedIntegrityReady(void)
{
  return tpSignedDataCertifiedModeReady();
}

void interfaceSdLoggingForceOff(void)
{
  interfaceConfigLoad();
  if (interface_cfg.sd_logging_enabled != 0)
  {
    interface_cfg.sd_logging_enabled = 0;
    interfaceConfigSave();
  }
}

uint8_t interfaceSdLogIntervalIndex(void)
{
  interfaceConfigLoad();
  if (interface_cfg.sd_interval_index >= IF_SD_INTERVAL_COUNT) interface_cfg.sd_interval_index = 2;
  interfaceClampSdWriteIntervalToOutput();
  return interface_cfg.sd_interval_index;
}

uint8_t interfaceFlowDisplayMode(void)
{
  interfaceConfigLoad();
  if (interface_cfg.flow_display_mode >= IF_FLOW_DISPLAY_COUNT) interface_cfg.flow_display_mode = IF_FLOW_DISPLAY_OFF;
  return interface_cfg.flow_display_mode;
}

bool interfaceFlowDisplayEnabled(void)
{
  uint8_t mode = interfaceFlowDisplayMode();
  return mode >= IF_FLOW_DISPLAY_L_MIN && mode <= IF_FLOW_DISPLAY_M3_H;
}

const char* interfaceFlowDisplayUnitText(void)
{
  switch (interfaceFlowDisplayMode())
  {
    case IF_FLOW_DISPLAY_L_MIN: return "l/min";
    case IF_FLOW_DISPLAY_L_S:   return "l/s";
    case IF_FLOW_DISPLAY_M3_H:  return "m3/h";
    case IF_FLOW_DISPLAY_OFF:
    default:                    return "";
  }
}

const char* interfaceFlowCsvUnitText(void)
{
  switch (interfaceFlowDisplayMode())
  {
    case IF_FLOW_DISPLAY_L_MIN: return "l/min";
    case IF_FLOW_DISPLAY_L_S:   return "l/s";
    case IF_FLOW_DISPLAY_M3_H:  return "m3/h";
    case IF_FLOW_DISPLAY_T_EXT:
    case IF_FLOW_DISPLAY_OFF:
    default:                    return "";
  }
}

bool interfaceAlmemoChannelEnabled(uint8_t index)
{
  interfaceConfigLoad();
  if (index >= IF_ALMEMO_CHANNEL_COUNT) return false;
  return interface_cfg.almemo_active[index] != 0;
}

bool interfaceAlmemoAnyChannelEnabled(void)
{
  return interfaceAlmemoChannelEnabled(0) || interfaceAlmemoChannelEnabled(1);
}

bool interfaceTExternalDisplayEnabled(void)
{
  return interfaceAlmemoAnyChannelEnabled();
}

uint8_t interfaceAlmemoAddressFor(uint8_t index)
{
  interfaceConfigLoad();
  if (index == 1) return interface_cfg.almemo_address2;
  return interface_cfg.almemo_address;
}

uint8_t interfaceAlmemoChannelFor(uint8_t index)
{
  interfaceConfigLoad();
  if (index == 1) return interface_cfg.almemo_channel2;
  return interface_cfg.almemo_channel;
}

uint8_t interfaceAlmemoRole(uint8_t index)
{
  interfaceConfigLoad();
  if (index >= IF_ALMEMO_CHANNEL_COUNT) index = 0;
  if (interface_cfg.almemo_role[index] == IF_ALMEMO_ROLE_OFF ||
      interface_cfg.almemo_role[index] >= IF_ALMEMO_ROLE_COUNT)
  {
    interface_cfg.almemo_role[index] = IF_ALMEMO_ROLE_TEMP;
  }
  return interface_cfg.almemo_role[index];
}

const char* interfaceAlmemoRoleText(uint8_t role)
{
  switch (role)
  {
    case IF_ALMEMO_ROLE_TEMP:     return ifText("Temperatur", "Temperature");
    case IF_ALMEMO_ROLE_FLOW:     return "Flow";
    case IF_ALMEMO_ROLE_PRESSURE: return ifText("Druck", "Pressure");
    case IF_ALMEMO_ROLE_GENERAL:  return ifText("Allgemein", "General");
    case IF_ALMEMO_ROLE_OFF:
    default:                      return ifText("Aus", "Off");
  }
}

const char* interfaceAlmemoDisplayLabel(uint8_t index)
{
  const bool second = (index == 1);
  switch (interfaceAlmemoRole(index))
  {
    case IF_ALMEMO_ROLE_FLOW:     return second ? "F-Ex2:" : "F-Ext:";
    case IF_ALMEMO_ROLE_PRESSURE: return second ? "P-Ex2:" : "P-Ext:";
    case IF_ALMEMO_ROLE_GENERAL:  return second ? "A-Ex2:" : "A-Ext:";
    case IF_ALMEMO_ROLE_TEMP:
    default:                      return second ? "T-Ex2:" : "T-Ext:";
  }
}

const char* interfaceAlmemoDisplayUnitShort(uint8_t index)
{
  switch (interfaceAlmemoRole(index))
  {
    case IF_ALMEMO_ROLE_TEMP:     return "\xB0" "C";
    case IF_ALMEMO_ROLE_FLOW:     return "L";
    case IF_ALMEMO_ROLE_PRESSURE: return "h";
    case IF_ALMEMO_ROLE_GENERAL:
    default:                      return "";
  }
}

const char* interfaceAlmemoCsvUnitText(uint8_t index)
{
  switch (interfaceAlmemoRole(index))
  {
    case IF_ALMEMO_ROLE_TEMP:     return "degC";
    case IF_ALMEMO_ROLE_FLOW:     return "l/min";
    case IF_ALMEMO_ROLE_PRESSURE: return "hPa";
    case IF_ALMEMO_ROLE_GENERAL:
    default:                      return "";
  }
}

uint8_t interfaceAlmemoDisplayIndex(void)
{
  const bool e0 = interfaceAlmemoChannelEnabled(0);
  const bool e1 = interfaceAlmemoChannelEnabled(1);
  if (e0 && e1) return ((millis() / 5000UL) & 1U) ? 1U : 0U;
  if (e1) return 1U;
  return 0U;
}

uint8_t interfaceAlmemoAddress(void)
{
  return interfaceAlmemoAddressFor(0);
}

uint8_t interfaceAlmemoChannel(void)
{
  return interfaceAlmemoChannelFor(0);
}

uint8_t interfaceAlmemoIntervalIndex(void)
{
  interfaceConfigLoad();
  if (interface_cfg.almemo_interval_index == IF_ALMEMO_INTERVAL_ADC ||
      interface_cfg.almemo_interval_index >= IF_ALMEMO_INTERVAL_COUNT)
  {
    interface_cfg.almemo_interval_index = IF_ALMEMO_INTERVAL_1S;
  }
  return interface_cfg.almemo_interval_index;
}

uint32_t interfaceAlmemoIntervalMsValue(void)
{
  switch (interfaceAlmemoIntervalIndex())
  {
    case IF_ALMEMO_INTERVAL_ADC: // alter EEPROM-/Web-Wert
      return 1000UL;
    case IF_ALMEMO_INTERVAL_10S: return 10000UL;
    case IF_ALMEMO_INTERVAL_1S:
    default:                     return 1000UL;
  }
}

bool interfaceAlmemoTraceEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.almemo_trace_enabled != 0;
}

int8_t interfaceAlmemoActivePort(void)
{
  interfaceConfigLoad();
  if (interface_cfg.rs232_mode[0] == IF_RS232_MODE_ALMEMO) return 0;
  if (interface_cfg.rs232_mode[1] == IF_RS232_MODE_ALMEMO) return 1;
  return -1;
}

int8_t interfaceWinControlRs232Port(void)
{
  interfaceConfigLoad();
  if (interface_cfg.rs232_mode[0] == IF_RS232_MODE_WINCONTROL) return 0;
  if (interface_cfg.rs232_mode[1] == IF_RS232_MODE_WINCONTROL) return 1;
  return -1;
}

bool interfaceWinControlEthernetEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.wincontrol_eth_enabled != 0;
}

uint16_t interfaceWinControlTcpPort(void)
{
  interfaceConfigLoad();
  if (interface_cfg.wincontrol_tcp_port < 1) interface_cfg.wincontrol_tcp_port = 10001;
  return interface_cfg.wincontrol_tcp_port;
}

uint8_t interfaceWinControlAddress(void)
{
  interfaceConfigLoad();
  if (interface_cfg.wincontrol_address > 99) interface_cfg.wincontrol_address = 0;
  return interface_cfg.wincontrol_address;
}

uint8_t interfaceWinControlBaudIndex(void)
{
  interfaceConfigLoad();
  if (interface_cfg.wincontrol_baud_index >= IF_BAUD_COUNT) interface_cfg.wincontrol_baud_index = 0;
  return interface_cfg.wincontrol_baud_index;
}

uint8_t interfaceWinControlCycleIndex(void)
{
  interfaceConfigLoad();
  if (interface_cfg.wincontrol_cycle_index >= IF_WINCONTROL_CYCLE_COUNT)
  {
    interface_cfg.wincontrol_cycle_index = IF_WINCONTROL_CYCLE_20S;
  }
  return interface_cfg.wincontrol_cycle_index;
}

uint32_t interfaceWinControlCycleMs(void)
{
  return interfaceWincontrolCycleMs[interfaceWinControlCycleIndex()];
}


bool interfaceEthEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.eth_enabled != 0;
}

bool interfaceEthDhcpEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.eth_dhcp != 0;
}

bool interfaceEthStaticConfigValid(void)
{
  interfaceConfigLoad();
  return interfaceStaticIpConfigValid();
}

bool interfaceWebServerEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.web_enabled != 0;
}

bool interfaceWebSetupEnabled(void)
{
  interfaceConfigLoad();
  return interface_cfg.web_setup_enabled != 0;
}

uint16_t interfaceEthWebPort(void)
{
  interfaceConfigLoad();
  return interface_cfg.web_port;
}

uint16_t interfaceEthTcpPort(void)
{
  interfaceConfigLoad();
  return interface_cfg.eth_tcp_port;
}

static void INTERFACE_FLASHMEM_NOINLINE interfaceCopyIp(uint8_t dst[4], const uint8_t src[4])
{
  if (dst == nullptr) return;
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

void interfaceEthGetStaticIp(uint8_t ip[4])
{
  interfaceConfigLoad();
  interfaceCopyIp(ip, interface_cfg.eth_ip);
}

void interfaceEthGetSubnet(uint8_t ip[4])
{
  interfaceConfigLoad();
  interfaceCopyIp(ip, interface_cfg.eth_subnet);
}

void interfaceEthGetGateway(uint8_t ip[4])
{
  interfaceConfigLoad();
  interfaceCopyIp(ip, interface_cfg.eth_gateway);
}

void interfaceEthGetDns(uint8_t ip[4])
{
  interfaceConfigLoad();
  interfaceCopyIp(ip, interface_cfg.eth_dns);
}

// Aktueller IP-Status fuer die kleine Hauptdisplay-Anzeige.
// Jetzt mit echter NativeEthernet-IP: DHCP zeigt die bezogene Adresse,
// statisch zeigt die aktive konfigurierte Adresse.
void interfaceGetDisplayIp(uint8_t ip[4], bool* valid, bool* enabled)
{
  interfaceConfigLoad();

  if (ip != nullptr)
  {
    ip[0] = 0;
    ip[1] = 0;
    ip[2] = 0;
    ip[3] = 0;
  }

  bool ethOn = (interface_cfg.eth_enabled != 0);
  bool ipValid = false;

  if (ethOn && ethernetHasValidIp())
  {
    if (ip != nullptr)
    {
      ethernetGetCurrentIp(ip);
    }
    ipValid = true;
  }

  if (valid != nullptr)
  {
    *valid = ipValid;
  }

  if (enabled != nullptr)
  {
    *enabled = ethOn;
  }
}

// =========================================================================
// TEXT-/MENUE-HELFER
// =========================================================================

static const char* FLASHMEM ifText(const char* de, const char* en)
{
  return (ui_language == LANG_EN) ? en : de;
}

static const char* FLASHMEM ifTextBack(void)
{
  return (ui_language == LANG_EN) ? "Back" : "Zurück";
}

static const char* FLASHMEM ifTextOnOff(uint8_t value)
{
  if (ui_language == LANG_EN)
  {
    return value ? "On" : "Off";
  }
  return value ? "Ein" : "Aus";
}

static const char* FLASHMEM ifRs232ModeText(uint8_t mode)
{
  switch (mode)
  {
    case IF_RS232_MODE_PC_TEXT: return ifText("PC lesbar", "PC readable");
    case IF_RS232_MODE_PC_CSV:  return "PC CSV";
    case IF_RS232_MODE_FLOW_IN: return ifText("Durchfluss Eingang", "Flow input");
    case IF_RS232_MODE_FLOW_POLL: return ifText("Durchfluss Polling", "Flow polling");
    case IF_RS232_MODE_ALMEMO: return "ALMEMO";
    case IF_RS232_MODE_WINCONTROL: return "WinControl";
    case IF_RS232_MODE_OFF:
    default:                    return ifText("Aus", "Off");
  }
}

static const char* FLASHMEM ifUsbModeText(uint8_t mode)
{
  switch (mode)
  {
    case IF_USB_MODE_CSV:      return "CSV";
    case IF_USB_MODE_DEBUG:    return "Debug";
    case IF_USB_MODE_COMMANDS:
    default:                   return ifText("Kommandos", "Commands");
  }
}

static const char* FLASHMEM ifOutputIntervalText(uint8_t index)
{
  switch (index)
  {
    case 0: return "ADC";
    case 1: return "1 s";
    case 2: return "10 s";
    case 3: return "30 s";
    case 4: return "60 s";
    default: return "1 s";
  }
}

static const char* FLASHMEM ifOutputFilterText(uint8_t index)
{
  switch (index)
  {
    case 0: return "0 s";
    case 1: return "1 s";
    case 2: return "3 s";
    case 3: return "5 s";
    case 4: return "10 s";
    default: return "1 s";
  }
}

static const char* FLASHMEM ifSdIntervalText(uint8_t index)
{
  switch (index)
  {
    case 0: return "10 s";
    case 1: return "60 s";
    case 2: return "3 min";
    case 3: return "5 min";
    default: return "5 min";
  }
}

static const char* FLASHMEM ifRs232OutputModeText(uint8_t mode)
{
  switch (mode)
  {
    case IF_RS232_OUT_REQUEST: return ifText("Anfrage", "Request");
    case IF_RS232_OUT_BOTH:    return ifText("Beides", "Both");
    case IF_RS232_OUT_CYCLIC:
    default:                   return ifText("Zyklisch", "Cyclic");
  }
}

static const char* FLASHMEM ifRs232IntervalText(uint8_t index)
{
  switch (index)
  {
    case 0: return "1 s";
    case 1: return "5 s";
    case 2: return "10 s";
    case 3: return "60 s";
    default: return "1 s";
  }
}

static const char* FLASHMEM ifFlowDisplayText(uint8_t mode)
{
  switch (mode)
  {
    case IF_FLOW_DISPLAY_L_MIN: return "l/min";
    case IF_FLOW_DISPLAY_L_S:   return "l/s";
    case IF_FLOW_DISPLAY_M3_H:  return "m3/h";
    case IF_FLOW_DISPLAY_T_EXT: return "T-Ext";
    case IF_FLOW_DISPLAY_OFF:
    default:                    return ifText("Aus", "Off");
  }
}

static const char* FLASHMEM ifAlmemoIntervalText(uint8_t index)
{
  switch (index)
  {
    case IF_ALMEMO_INTERVAL_ADC: return "ADC";
    case IF_ALMEMO_INTERVAL_10S: return "10 s";
    case IF_ALMEMO_INTERVAL_1S:
    default:                     return "1 s";
  }
}

static const char* FLASHMEM ifWinControlCycleText(uint8_t index)
{
  switch (index)
  {
    case IF_WINCONTROL_CYCLE_1S: return "1 s";
    case IF_WINCONTROL_CYCLE_10S: return "10 s";
    case IF_WINCONTROL_CYCLE_60S: return "60 s";
    case IF_WINCONTROL_CYCLE_20S:
    default: return "20 s";
  }
}

static const char* FLASHMEM ifWinControlOutputText(uint8_t mode)
{
  switch (mode)
  {
    case 1: return "RS232-1";
    case 2: return "RS232-2";
    case 3: return "Ethernet";
    case 0:
    default: return ifText("Aus", "Off");
  }
}




static uint8_t FLASHMEM interfaceRawTouchKey(void)
{
  if (!TouchZ) return MK_NONE;

  if (TouchX > 20 && TouchX < 130 && TouchY > 25 && TouchY < 145)
  {
    return MK_UP;
  }

  if (TouchX > 20 && TouchX < 130 && TouchY > 187 && TouchY < 307)
  {
    return MK_DOWN;
  }

  if (TouchX > 20 && TouchX < 130 && TouchY > 350 && TouchY < 470)
  {
    return MK_ENTER;
  }

  return MK_NONE;
}

static bool FLASHMEM interfaceValueHandleUInt16Fast(uint16_t& value,
                                                    uint16_t min_value,
                                                    uint16_t max_value,
                                                    uint16_t base_step,
                                                    void (*on_change)(void),
                                                    uint16_t debounce_ms)
{
  static uint8_t hold_key = MK_NONE;
  static uint32_t hold_start_ms = 0;

  ReadButtons(false);

  const uint32_t now = millis();
  const uint8_t raw_key = interfaceRawTouchKey();

  if (raw_key != hold_key)
  {
    hold_key = raw_key;
    hold_start_ms = now;
  }

  uint8_t key = menuReadKey(debounce_ms);

  if (key == MK_UP || key == MK_DOWN)
  {
    uint16_t effective_step = base_step;

    if (hold_key == key)
    {
      const uint32_t held_ms = now - hold_start_ms;

      // TCP-Portbereiche sind gross. Kurz bleibt 1 fein,
      // langes Halten beschleunigt auf 10 und danach 100 pro Schritt.
      if (held_ms > 6400UL)
      {
        effective_step = 100;
      }
      else if (held_ms > 3200UL)
      {
        effective_step = 10;
      }
    }

    if (key == MK_UP)
    {
      uint32_t next_value = (uint32_t)value + effective_step;
      if (next_value > max_value) next_value = max_value;

      if (next_value != value)
      {
        value = (uint16_t)next_value;
        if (on_change != nullptr) on_change();
      }
    }
    else
    {
      int32_t next_value = (int32_t)value - effective_step;
      if (next_value < min_value) next_value = min_value;

      if (next_value != value)
      {
        value = (uint16_t)next_value;
        if (on_change != nullptr) on_change();
      }
    }

    flag.menu_lcd_upd = false;
  }
  else if (key == MK_ENTER)
  {
    hold_key = MK_NONE;
    flag.short_push = false;
    return true;
  }

  return false;
}

static void FLASHMEM interfaceConsumeTouch(void)
{
  TouchZ = false;
  TouchX = 0;
  TouchY = 0;
}

static void FLASHMEM interfaceReturnTo(uint16_t return_level)
{
  extern uint32_t menu_global_debounce;

  menuDirectClearAllRows();
  menu_level = return_level;
  flag.short_push = false;
  flag.menu_lcd_upd = false;
  interfaceConsumeTouch();
  menu_global_debounce = millis();
}

static bool FLASHMEM interfaceRunStringList(const char* title,
                                   const char** items,
                                   uint8_t menu_size,
                                   int8_t& current_selection,
                                   uint8_t lines = 3,
                                   uint16_t debounce_ms = 250)
{
  if (VirtLCDMenu == nullptr) return false;
  if (menu_size == 0) return false;

  ReadButtons(false);

  uint8_t key = menuReadKey(debounce_ms);

  if (key == MK_UP)
  {
    current_selection--;
    if (current_selection < 0) current_selection = menu_size - 1;
    flag.menu_lcd_upd = false;
  }
  else if (key == MK_DOWN)
  {
    current_selection++;
    if (current_selection >= menu_size) current_selection = 0;
    flag.menu_lcd_upd = false;
  }
  else if (key == MK_ENTER)
  {
    flag.short_push = false;
    return true;
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    VirtLCDMenu->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(title);
    lcd_scroll_Menu((const char**)items, menu_size, current_selection, 4, 1, lines);
    VirtLCDMenu->transfer();
    ReadButtons(true);
  }

  return false;
}

static void FLASHMEM interfaceBuildBaudText(uint8_t index, char* out, size_t outSize)
{
  if (index >= IF_BAUD_COUNT) index = 0;
  snprintf(out, outSize, "%lu", (unsigned long)interfaceBaudTable[index]);
}

// =========================================================================
// RS232 MENUES
// =========================================================================

static void FLASHMEM interfaceRs232Menu(uint8_t port)
{
  static int8_t current_selection[2] = {0, 0};
  static bool frisch[2] = {true, true};

  interfaceConfigLoad();
  if (port > 1) port = 0;

  if (frisch[port])
  {
    current_selection[port] = 0;
    flag.menu_lcd_upd = false;
    frisch[port] = false;
  }

  const char* items[] = {
    ifText("Modus", "Mode"),
    ifText("Baudrate", "Baudrate"),
    ifText("Ausgabe", "Output"),
    ifTextBack()
  };

  const char* title = (port == 0) ? T(TXT_MENU_RS232_1) : T(TXT_MENU_RS232_2);

  if (interfaceRunStringList(title, items, 4, current_selection[port], 4))
  {
    if (current_selection[port] == 0)
    {
      menu_level = (port == 0) ? MENU_RS232_1_MODE : MENU_RS232_2_MODE;
    }
    else if (current_selection[port] == 1)
    {
      menu_level = (port == 0) ? MENU_RS232_1_BAUD : MENU_RS232_2_BAUD;
    }
    else if (current_selection[port] == 2)
    {
      menu_level = (port == 0) ? MENU_RS232_1_OUTPUT : MENU_RS232_2_OUTPUT;
    }
    else
    {
      menu_level = MENU_INTERFACES;
      frisch[port] = true;
    }

    flag.menu_lcd_upd = false;
  }
}

void FLASHMEM interface_rs232_1_menu(void)
{
  interfaceRs232Menu(0);
}

void FLASHMEM interface_rs232_2_menu(void)
{
  interfaceRs232Menu(1);
}

static void FLASHMEM interfaceRs232ModeMenu(uint8_t port)
{
  static int8_t current_selection[2] = {0, 0};
  static bool frisch[2] = {true, true};

  interfaceConfigLoad();
  if (port > 1) port = 0;

  if (frisch[port])
  {
    current_selection[port] = interface_cfg.rs232_mode[port];
    if (current_selection[port] < 0 || current_selection[port] > IF_RS232_MODE_WINCONTROL) current_selection[port] = 0;
    flag.menu_lcd_upd = false;
    frisch[port] = false;
  }

  const char* items[] = {
    ifRs232ModeText(IF_RS232_MODE_OFF),
    ifRs232ModeText(IF_RS232_MODE_PC_TEXT),
    ifRs232ModeText(IF_RS232_MODE_PC_CSV),
    ifRs232ModeText(IF_RS232_MODE_FLOW_IN),
    ifRs232ModeText(IF_RS232_MODE_FLOW_POLL),
    ifRs232ModeText(IF_RS232_MODE_ALMEMO),
    ifRs232ModeText(IF_RS232_MODE_WINCONTROL),
    ifTextBack()
  };

  const char* title = (port == 0) ? ifText("RS232-1 Modus", "RS232-1 mode")
                                  : ifText("RS232-2 Modus", "RS232-2 mode");

  if (interfaceRunStringList(title, items, 8, current_selection[port], 5))
  {
    if (current_selection[port] <= IF_RS232_MODE_WINCONTROL)
    {
      if ((uint8_t)current_selection[port] == IF_RS232_MODE_ALMEMO ||
          (uint8_t)current_selection[port] == IF_RS232_MODE_WINCONTROL)
      {
        interface_cfg.rs232_mode[port ^ 1U] = IF_RS232_MODE_OFF;
      }
      interface_cfg.rs232_mode[port] = (uint8_t)current_selection[port];
      if ((uint8_t)current_selection[port] == IF_RS232_MODE_WINCONTROL)
      {
        interface_cfg.rs232_baud_index[port] = interface_cfg.wincontrol_baud_index;
      }
      interfaceConfigSave();
      interfaceApplySerialBaud();
    }

    frisch[port] = true;
    interfaceReturnTo((port == 0) ? MENU_RS232_1 : MENU_RS232_2);
  }
}

void FLASHMEM interface_rs232_1_mode_menu(void)
{
  interfaceRs232ModeMenu(0);
}

void FLASHMEM interface_rs232_2_mode_menu(void)
{
  interfaceRs232ModeMenu(1);
}

static void FLASHMEM interfaceRs232BaudMenu(uint8_t port)
{
  static int8_t current_selection[2] = {0, 0};
  static bool frisch[2] = {true, true};
  static char baud_text[IF_BAUD_COUNT][12];

  interfaceConfigLoad();
  if (port > 1) port = 0;

  if (frisch[port])
  {
    current_selection[port] = interface_cfg.rs232_baud_index[port];
    if (current_selection[port] < 0 || current_selection[port] >= IF_BAUD_COUNT) current_selection[port] = 0;
    flag.menu_lcd_upd = false;
    frisch[port] = false;
  }

  for (uint8_t i = 0; i < IF_BAUD_COUNT; i++)
  {
    interfaceBuildBaudText(i, baud_text[i], sizeof(baud_text[i]));
  }

  const char* items[] = {
    baud_text[0],
    baud_text[1],
    baud_text[2],
    baud_text[3],
    baud_text[4],
    ifTextBack()
  };

  const char* title = (port == 0) ? ifText("RS232-1 Baudrate", "RS232-1 baudrate")
                                  : ifText("RS232-2 Baudrate", "RS232-2 baudrate");

  if (interfaceRunStringList(title, items, 6, current_selection[port], 5))
  {
    if (current_selection[port] < IF_BAUD_COUNT)
    {
      interface_cfg.rs232_baud_index[port] = (uint8_t)current_selection[port];
      interfaceConfigSave();
      interfaceApplySerialBaud();
    }

    frisch[port] = true;
    interfaceReturnTo((port == 0) ? MENU_RS232_1 : MENU_RS232_2);
  }
}

void FLASHMEM interface_rs232_1_baud_menu(void)
{
  interfaceRs232BaudMenu(0);
}

void FLASHMEM interface_rs232_2_baud_menu(void)
{
  interfaceRs232BaudMenu(1);
}

static void FLASHMEM interfaceRs232OutputMenu(uint8_t port)
{
  static int8_t current_selection[2] = {0, 0};
  static bool frisch[2] = {true, true};

  interfaceConfigLoad();
  if (port > 1) port = 0;

  if (frisch[port])
  {
    current_selection[port] = interface_cfg.rs232_output_mode[port];
    if (current_selection[port] < 0 || current_selection[port] > IF_RS232_OUT_BOTH) current_selection[port] = 0;
    flag.menu_lcd_upd = false;
    frisch[port] = false;
  }

  const char* items[] = {
    ifRs232OutputModeText(IF_RS232_OUT_CYCLIC),
    ifRs232OutputModeText(IF_RS232_OUT_REQUEST),
    ifRs232OutputModeText(IF_RS232_OUT_BOTH),
    ifTextBack()
  };

  const char* title = (port == 0) ? ifText("RS232-1 Ausgabe", "RS232-1 output")
                                  : ifText("RS232-2 Ausgabe", "RS232-2 output");

  if (interfaceRunStringList(title, items, 4, current_selection[port], 4))
  {
    if (current_selection[port] < 3)
    {
      interface_cfg.rs232_output_mode[port] = (uint8_t)current_selection[port];
      interfaceConfigSave();
    }

    frisch[port] = true;
    interfaceReturnTo((port == 0) ? MENU_RS232_1 : MENU_RS232_2);
  }
}

void FLASHMEM interface_rs232_1_output_menu(void)
{
  interfaceRs232OutputMenu(0);
}

void FLASHMEM interface_rs232_2_output_menu(void)
{
  interfaceRs232OutputMenu(1);
}

static void FLASHMEM interfaceRs232IntervalMenu(uint8_t port)
{
  static int8_t current_selection[2] = {0, 0};
  static bool frisch[2] = {true, true};

  interfaceConfigLoad();
  if (port > 1) port = 0;

  if (frisch[port])
  {
    current_selection[port] = interface_cfg.rs232_output_interval_index[port];
    if (current_selection[port] < 0 || current_selection[port] >= IF_RS232_INTERVAL_COUNT) current_selection[port] = 0;
    flag.menu_lcd_upd = false;
    frisch[port] = false;
  }

  const char* items[] = {
    ifRs232IntervalText(0),
    ifRs232IntervalText(1),
    ifRs232IntervalText(2),
    ifRs232IntervalText(3),
    ifTextBack()
  };

  const char* title = (port == 0) ? ifText("RS232-1 Intervall", "RS232-1 interval")
                                  : ifText("RS232-2 Intervall", "RS232-2 interval");

  if (interfaceRunStringList(title, items, 5, current_selection[port], 5))
  {
    if (current_selection[port] < IF_RS232_INTERVAL_COUNT)
    {
      interface_cfg.rs232_output_interval_index[port] = (uint8_t)current_selection[port];
      interfaceConfigSave();
    }

    frisch[port] = true;
    interfaceReturnTo((port == 0) ? MENU_RS232_1 : MENU_RS232_2);
  }
}

void FLASHMEM interface_rs232_1_interval_menu(void)
{
  interfaceRs232IntervalMenu(0);
}

void FLASHMEM interface_rs232_2_interval_menu(void)
{
  interfaceRs232IntervalMenu(1);
}

void FLASHMEM interface_flow_display_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  if (frisch)
  {
    current_selection = interface_cfg.flow_display_mode;
    if (current_selection < 0 || current_selection >= IF_FLOW_DISPLAY_COUNT) current_selection = IF_FLOW_DISPLAY_OFF;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    ifFlowDisplayText(IF_FLOW_DISPLAY_OFF),
    ifFlowDisplayText(IF_FLOW_DISPLAY_L_MIN),
    ifFlowDisplayText(IF_FLOW_DISPLAY_L_S),
    ifFlowDisplayText(IF_FLOW_DISPLAY_M3_H),
    ifFlowDisplayText(IF_FLOW_DISPLAY_T_EXT),
    ifTextBack()
  };

  if (interfaceRunStringList(ifText("Zusatzanzeige", "Additional display"), items, 6, current_selection, 5))
  {
    if (current_selection < IF_FLOW_DISPLAY_COUNT)
    {
      interface_cfg.flow_display_mode = (uint8_t)current_selection;
      interfaceConfigSave();
    }

    frisch = true;
    interfaceReturnTo(MENU_INTERFACES);
  }
}

// =========================================================================
// ALMEMO V5/V6 UNIVERSALMENUES
// =========================================================================

extern bool serialAlmemoIsValid(uint8_t index);
extern float serialAlmemoLastValue(uint8_t index);
extern uint32_t serialAlmemoRxErrors(uint8_t index);

static uint8_t interface_almemo_edit_channel = 0;

static void INTERFACE_FLASHMEM_NOINLINE interfaceAlmemoBuildStatus(uint8_t index, char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0) return;
  if (!interfaceAlmemoChannelEnabled(index))
  {
    snprintf(out, outSize, "%s: %s", ifText("Status", "Status"), ifText("Aus", "Off"));
  }
  else if (serialAlmemoIsValid(index))
  {
    snprintf(out, outSize, "Status: OK %.2f", (double)serialAlmemoLastValue(index));
  }
  else
  {
    snprintf(out, outSize, "Status: %s E%lu", ifText("Sucht", "Searching"),
             (unsigned long)serialAlmemoRxErrors(index));
  }
}

static void INTERFACE_FLASHMEM_NOINLINE interfaceAlmemoBuildChannelLine(uint8_t index, char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0) return;
  const char* state = interfaceAlmemoChannelEnabled(index) ? ifText("Ein", "On") : ifText("Aus", "Off");
  snprintf(out, outSize, "K%u %s %02u/%02u %s",
           (unsigned)(index + 1U), state,
           (unsigned)interfaceAlmemoAddressFor(index),
           (unsigned)interfaceAlmemoChannelFor(index),
           interfaceAlmemoDisplayLabel(index));
}

void FLASHMEM interface_almemo_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;
  static char ch1Line[40];
  static char ch2Line[40];
  static char traceLine[40];
  static char lastCh1Line[40] = "";
  static char lastCh2Line[40] = "";
  static char lastTraceLine[40] = "";

  interfaceConfigLoad();
  if (frisch)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  interfaceAlmemoBuildChannelLine(0, ch1Line, sizeof(ch1Line));
  interfaceAlmemoBuildChannelLine(1, ch2Line, sizeof(ch2Line));
  snprintf(traceLine, sizeof(traceLine), "%s: %s",
           ifText("Seriellog", "Serial trace"),
           ifTextOnOff(interface_cfg.almemo_trace_enabled));

  if (strncmp(lastCh1Line, ch1Line, sizeof(lastCh1Line)) != 0 ||
      strncmp(lastCh2Line, ch2Line, sizeof(lastCh2Line)) != 0 ||
      strncmp(lastTraceLine, traceLine, sizeof(lastTraceLine)) != 0)
  {
    snprintf(lastCh1Line, sizeof(lastCh1Line), "%s", ch1Line);
    snprintf(lastCh2Line, sizeof(lastCh2Line), "%s", ch2Line);
    snprintf(lastTraceLine, sizeof(lastTraceLine), "%s", traceLine);
    flag.menu_lcd_upd = false;
  }

  const char* items[] = {
    ch1Line,
    ch2Line,
    ifText("Messintervall", "Interval"),
    traceLine,
    ifTextBack()
  };

  if (interfaceRunStringList("ALMEMO", items, 5, current_selection, 5))
  {
    if (current_selection == 0)
    {
      interface_almemo_edit_channel = 0;
      menu_level = MENU_ALMEMO_CH1;
    }
    else if (current_selection == 1)
    {
      interface_almemo_edit_channel = 1;
      menu_level = MENU_ALMEMO_CH2;
    }
    else if (current_selection == 2) menu_level = MENU_ALMEMO_INTERVAL;
    else if (current_selection == 3) menu_level = MENU_ALMEMO_TRACE;
    else
    {
      menu_level = MENU_INTERFACES;
      frisch = true;
    }
    flag.menu_lcd_upd = false;
  }
}

static void FLASHMEM interfaceAlmemoChannelMenu(uint8_t index)
{
  static int8_t current_selection[IF_ALMEMO_CHANNEL_COUNT] = {0, 0};
  static bool frisch[IF_ALMEMO_CHANNEL_COUNT] = {true, true};
  static char statusLine[IF_ALMEMO_CHANNEL_COUNT][40];
  static char activeLine[IF_ALMEMO_CHANNEL_COUNT][24];
  static char addressLine[IF_ALMEMO_CHANNEL_COUNT][24];
  static char channelLine[IF_ALMEMO_CHANNEL_COUNT][24];
  static char roleLine[IF_ALMEMO_CHANNEL_COUNT][32];
  static char lastStatusLine[IF_ALMEMO_CHANNEL_COUNT][40] = {{0}, {0}};
  static char lastActiveLine[IF_ALMEMO_CHANNEL_COUNT][24] = {{0}, {0}};
  static char lastAddressLine[IF_ALMEMO_CHANNEL_COUNT][24] = {{0}, {0}};
  static char lastChannelLine[IF_ALMEMO_CHANNEL_COUNT][24] = {{0}, {0}};
  static char lastRoleLine[IF_ALMEMO_CHANNEL_COUNT][32] = {{0}, {0}};

  if (index >= IF_ALMEMO_CHANNEL_COUNT) index = 0;
  interface_almemo_edit_channel = index;
  interfaceConfigLoad();

  if (frisch[index])
  {
    current_selection[index] = 0;
    flag.menu_lcd_upd = false;
    frisch[index] = false;
  }

  snprintf(activeLine[index], sizeof(activeLine[index]), "%s: %s",
           ifText("Aktiv", "Active"),
           ifTextOnOff(interface_cfg.almemo_active[index]));
  snprintf(addressLine[index], sizeof(addressLine[index]), "%s: %02u",
           ifText("Adresse", "Address"),
           (unsigned)interfaceAlmemoAddressFor(index));
  snprintf(channelLine[index], sizeof(channelLine[index]), "%s: %02u",
           ifText("Messkanal", "Channel"),
           (unsigned)interfaceAlmemoChannelFor(index));
  snprintf(roleLine[index], sizeof(roleLine[index]), "%s: %s",
           ifText("Rolle", "Role"),
           interfaceAlmemoRoleText(interface_cfg.almemo_role[index]));
  interfaceAlmemoBuildStatus(index, statusLine[index], sizeof(statusLine[index]));

  if (strncmp(lastStatusLine[index], statusLine[index], sizeof(lastStatusLine[index])) != 0 ||
      strncmp(lastActiveLine[index], activeLine[index], sizeof(lastActiveLine[index])) != 0 ||
      strncmp(lastAddressLine[index], addressLine[index], sizeof(lastAddressLine[index])) != 0 ||
      strncmp(lastChannelLine[index], channelLine[index], sizeof(lastChannelLine[index])) != 0 ||
      strncmp(lastRoleLine[index], roleLine[index], sizeof(lastRoleLine[index])) != 0)
  {
    snprintf(lastStatusLine[index], sizeof(lastStatusLine[index]), "%s", statusLine[index]);
    snprintf(lastActiveLine[index], sizeof(lastActiveLine[index]), "%s", activeLine[index]);
    snprintf(lastAddressLine[index], sizeof(lastAddressLine[index]), "%s", addressLine[index]);
    snprintf(lastChannelLine[index], sizeof(lastChannelLine[index]), "%s", channelLine[index]);
    snprintf(lastRoleLine[index], sizeof(lastRoleLine[index]), "%s", roleLine[index]);
    flag.menu_lcd_upd = false;
  }

  const char* items[] = {
    activeLine[index],
    addressLine[index],
    channelLine[index],
    roleLine[index],
    statusLine[index],
    ifTextBack()
  };

  char title[24];
  snprintf(title, sizeof(title), "ALMEMO Kanal %u", (unsigned)(index + 1U));

  if (interfaceRunStringList(title, items, 6, current_selection[index], 5))
  {
    interface_almemo_edit_channel = index;
    if (current_selection[index] == 0) menu_level = MENU_ALMEMO_ACTIVE;
    else if (current_selection[index] == 1) menu_level = MENU_ALMEMO_ADDRESS;
    else if (current_selection[index] == 2) menu_level = MENU_ALMEMO_CHANNEL;
    else if (current_selection[index] == 3) menu_level = MENU_ALMEMO_ROLE;
    else if (current_selection[index] == 5)
    {
      menu_level = MENU_ALMEMO;
      frisch[index] = true;
    }
    flag.menu_lcd_upd = false;
  }
}

void FLASHMEM interface_almemo_ch1_menu(void)
{
  interfaceAlmemoChannelMenu(0);
}

void FLASHMEM interface_almemo_ch2_menu(void)
{
  interfaceAlmemoChannelMenu(1);
}

void FLASHMEM interface_almemo_active_menu(void)
{
  interfaceConfigLoad();
  uint8_t& active = interface_cfg.almemo_active[interface_almemo_edit_channel];
  interfaceToggleMenu(ifText("ALMEMO aktiv", "ALMEMO active"),
                      active,
                      (interface_almemo_edit_channel == 0) ? MENU_ALMEMO_CH1 : MENU_ALMEMO_CH2);
}

static void FLASHMEM interfaceAlmemoNumberMenu(uint8_t kind)
{
  static MenuPageDrawFlag page_drawn;
  static uint8_t active_kind = 255;
  static uint8_t active_channel = 255;
  static uint16_t editValue = 0;
  static uint16_t lastValue = 0xFFFF;

  interfaceConfigLoad();
  const uint8_t index = (interface_almemo_edit_channel < IF_ALMEMO_CHANNEL_COUNT) ? interface_almemo_edit_channel : 0;
  if (active_kind != kind || active_channel != index)
  {
    active_kind = kind;
    active_channel = index;
    editValue = (kind == 0) ? interfaceAlmemoAddressFor(index) : interfaceAlmemoChannelFor(index);
    page_drawn = false;
    lastValue = 0xFFFF;
    flag.menu_lcd_upd = false;
  }

  if (menuValueHandleUInt16(editValue, 0, 99, 1, nullptr, 170))
  {
    if (index == 0)
    {
      if (kind == 0) interface_cfg.almemo_address = (uint8_t)editValue;
      else interface_cfg.almemo_channel = (uint8_t)editValue;
    }
    else
    {
      if (kind == 0) interface_cfg.almemo_address2 = (uint8_t)editValue;
      else interface_cfg.almemo_channel2 = (uint8_t)editValue;
    }
    interfaceConfigSave();
    menuDirectClearAllRows();
    page_drawn = false;
    active_kind = 255;
    active_channel = 255;
    interfaceReturnTo((index == 0) ? MENU_ALMEMO_CH1 : MENU_ALMEMO_CH2);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_MENU_ALMEMO, false);
      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((kind == 0) ? ifText("Adresse:", "Address:")
                                     : ifText("Messkanal:", "Channel:"));
      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(ifText("UP/DOWN: +/- 1", "UP/DOWN: +/- 1"));
      VirtLCDMenu->setCursor(1, 10);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));
      VirtLCDMenu->transfer();
      ReadButtons(true);
      menuDirectResetValueCache();
      page_drawn = true;
      lastValue = 0xFFFF;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (lastValue != editValue)
    {
      snprintf(lcd_buf, 255, "%02u", (unsigned)editValue);
      menuDirectWriteFixedRow(0, 13, 4, lcd_buf, 2, WHITE);
      lastValue = editValue;
    }
    ReadButtons(true);
  }
}

void FLASHMEM interface_almemo_address_menu(void)
{
  interfaceAlmemoNumberMenu(0);
}

void FLASHMEM interface_almemo_channel_menu(void)
{
  interfaceAlmemoNumberMenu(1);
}

void FLASHMEM interface_almemo_role_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();
  const uint8_t index = (interface_almemo_edit_channel < IF_ALMEMO_CHANNEL_COUNT) ? interface_almemo_edit_channel : 0;
  if (frisch)
  {
    current_selection = (int8_t)interface_cfg.almemo_role[index] - 1;
    if (current_selection < 0 || current_selection > 3) current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    interfaceAlmemoRoleText(IF_ALMEMO_ROLE_TEMP),
    interfaceAlmemoRoleText(IF_ALMEMO_ROLE_FLOW),
    interfaceAlmemoRoleText(IF_ALMEMO_ROLE_PRESSURE),
    interfaceAlmemoRoleText(IF_ALMEMO_ROLE_GENERAL),
    ifTextBack()
  };

  if (interfaceRunStringList(ifText("ALMEMO Rolle", "ALMEMO role"), items, 5, current_selection, 5))
  {
    if (current_selection >= 0 && current_selection < 4)
    {
      interface_cfg.almemo_role[index] = (uint8_t)(current_selection + 1);
      interfaceConfigSave();
    }
    frisch = true;
    interfaceReturnTo((index == 0) ? MENU_ALMEMO_CH1 : MENU_ALMEMO_CH2);
  }
}

void FLASHMEM interface_almemo_interval_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();
  if (frisch)
  {
    current_selection = (interface_cfg.almemo_interval_index == IF_ALMEMO_INTERVAL_10S) ? 1 : 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    ifAlmemoIntervalText(IF_ALMEMO_INTERVAL_1S),
    ifAlmemoIntervalText(IF_ALMEMO_INTERVAL_10S),
    ifTextBack()
  };

  if (interfaceRunStringList(ifText("ALMEMO Intervall", "ALMEMO interval"),
                             items, 3, current_selection, 3))
  {
    if (current_selection == 0)
    {
      interface_cfg.almemo_interval_index = IF_ALMEMO_INTERVAL_1S;
      interfaceConfigSave();
    }
    else if (current_selection == 1)
    {
      interface_cfg.almemo_interval_index = IF_ALMEMO_INTERVAL_10S;
      interfaceConfigSave();
    }
    frisch = true;
    interfaceReturnTo(MENU_ALMEMO);
  }
}

void FLASHMEM interface_almemo_trace_menu(void)
{
  interfaceConfigLoad();
  uint8_t oldValue = interface_cfg.almemo_trace_enabled;

  interfaceToggleMenu(ifText("ALMEMO Seriellog", "ALMEMO serial trace"),
                      interface_cfg.almemo_trace_enabled,
                      MENU_ALMEMO);

  if (oldValue != interface_cfg.almemo_trace_enabled)
  {
    // Dateioperationen werden nur bei einer ausdruecklichen Bedienaktion
    // ausgefuehrt und fuer die Safety als bekannte blockierende Operation
    // markiert. Das normale CSV-Logging bleibt davon unabhaengig.
    safetyBeginBlockingOperation();
    if (interface_cfg.almemo_trace_enabled && !sdLogReadyForLogging())
    {
      sdLogEnsureReadyForAccess();
    }
    bool ok = serialProtocolTraceApplyNow();
    safetyEndBlockingOperation();

    if (interface_cfg.almemo_trace_enabled && !ok)
    {
      interface_cfg.almemo_trace_enabled = 0;
      interfaceConfigSave();
      serialProtocolTraceApplyNow();
    }
  }
}


// =========================================================================
// WINCONTROL / AMR-CONTROL AUSGABE
// =========================================================================

extern bool winControlOutEthernetClientConnected(void);
extern uint16_t winControlOutEthernetActivePort(void);

uint8_t INTERFACE_FLASHMEM_NOINLINE interfaceWinControlOutputMode(void)
{
  interfaceConfigLoad();
  const bool rs1 = (interface_cfg.rs232_mode[0] == IF_RS232_MODE_WINCONTROL);
  const bool rs2 = (interface_cfg.rs232_mode[1] == IF_RS232_MODE_WINCONTROL);
  const bool eth = (interface_cfg.wincontrol_eth_enabled != 0);
  if (rs1) return 1;
  if (rs2) return 2;
  if (eth) return 3;
  return 0;
}

static void INTERFACE_FLASHMEM_NOINLINE interfaceWinControlApplyOutputMode(uint8_t mode)
{
  if (interface_cfg.rs232_mode[0] == IF_RS232_MODE_WINCONTROL) interface_cfg.rs232_mode[0] = IF_RS232_MODE_OFF;
  if (interface_cfg.rs232_mode[1] == IF_RS232_MODE_WINCONTROL) interface_cfg.rs232_mode[1] = IF_RS232_MODE_OFF;
  interface_cfg.wincontrol_eth_enabled = 0;

  if (mode == 1)
  {
    interface_cfg.rs232_mode[0] = IF_RS232_MODE_WINCONTROL;
    interface_cfg.rs232_baud_index[0] = interface_cfg.wincontrol_baud_index;
  }
  else if (mode == 2)
  {
    interface_cfg.rs232_mode[1] = IF_RS232_MODE_WINCONTROL;
    interface_cfg.rs232_baud_index[1] = interface_cfg.wincontrol_baud_index;
  }
  else if (mode == 3)
  {
    interface_cfg.wincontrol_eth_enabled = 1;
    interface_cfg.eth_enabled = 1;
  }
}

bool FLASHMEM interfaceWebSetWinControl(uint8_t outputMode, uint8_t baudIndex, uint8_t address, uint16_t tcpPort, uint8_t cycleIndex)
{
  interfaceConfigLoad();
  if (outputMode > 3) return false;
  if (baudIndex >= IF_BAUD_COUNT) return false;
  if (address > 99) return false;
  if (tcpPort < 1) return false;
  if (cycleIndex >= IF_WINCONTROL_CYCLE_COUNT) return false;

  interface_cfg.wincontrol_baud_index = baudIndex;
  interface_cfg.wincontrol_address = address;
  interface_cfg.wincontrol_tcp_port = tcpPort;
  interface_cfg.wincontrol_cycle_index = cycleIndex;
  interfaceWinControlApplyOutputMode(outputMode);
  interfaceConfigSave();
  interfaceApplySerialBaud();
  // Web-Requests duerfen Ethernet nicht synchron neu initialisieren.
  // Sonst wird die aktive Browser-Verbindung waehrend /setup-wincontrol
  // abgerissen und der Browser meldet "Failed to fetch".
  // Der WinControl-raw-TCP-Server uebernimmt Port-/Aktiv-Aenderungen
  // nicht-blockierend in winControlOutEthernetTask().
  return true;
}

void FLASHMEM interface_wincontrol_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;
  static char outLine[40];
  static char baudLine[32];
  static char addrLine[32];
  static char portLine[32];
  static char cycleLine[32];

  interfaceConfigLoad();
  if (frisch)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  snprintf(outLine, sizeof(outLine), "%s: %s", ifText("Ausgabe", "Output"), ifWinControlOutputText(interfaceWinControlOutputMode()));
  snprintf(baudLine, sizeof(baudLine), "%s: %lu", ifText("Baudrate", "Baudrate"), (unsigned long)interfaceBaudTable[interfaceWinControlBaudIndex()]);
  snprintf(addrLine, sizeof(addrLine), "%s: %02u", ifText("Adresse", "Address"), (unsigned)interfaceWinControlAddress());
  snprintf(portLine, sizeof(portLine), "TCP-Port: %u", (unsigned)interfaceWinControlTcpPort());
  snprintf(cycleLine, sizeof(cycleLine), "%s: %s", ifText("Zyklus", "Cycle"), ifWinControlCycleText(interfaceWinControlCycleIndex()));

  const char* items[] = { outLine, baudLine, addrLine, portLine, cycleLine, ifText("Status", "Status"), ifTextBack() };

  if (interfaceRunStringList(ifText("WinControl-Ausgabe", "WinControl output"), items, 7, current_selection, 5))
  {
    if (current_selection == 0) menu_level = MENU_WINCONTROL_OUTPUT;
    else if (current_selection == 1) menu_level = MENU_WINCONTROL_BAUD;
    else if (current_selection == 2) menu_level = MENU_WINCONTROL_ADDRESS;
    else if (current_selection == 3) menu_level = MENU_WINCONTROL_TCP_PORT;
    else if (current_selection == 4) menu_level = MENU_WINCONTROL_CYCLE;
    else if (current_selection == 5) menu_level = MENU_WINCONTROL_STATUS;
    else { menu_level = MENU_INTERFACES; frisch = true; }
    flag.menu_lcd_upd = false;
  }
}

void FLASHMEM interface_wincontrol_output_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();
  if (frisch)
  {
    current_selection = interfaceWinControlOutputMode();
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = { ifWinControlOutputText(0), ifWinControlOutputText(1), ifWinControlOutputText(2), ifWinControlOutputText(3), ifTextBack() };

  if (interfaceRunStringList(ifText("WinControl Ausgabe", "WinControl output"), items, 5, current_selection, 4))
  {
    if (current_selection >= 0 && current_selection <= 3)
    {
      interfaceWinControlApplyOutputMode((uint8_t)current_selection);
      interfaceConfigSave();
      interfaceApplySerialBaud();
      ethernetServiceApplyNow();
    }
    frisch = true;
    interfaceReturnTo(MENU_WINCONTROL);
  }
}

void FLASHMEM interface_wincontrol_baud_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;
  static char baudText[IF_BAUD_COUNT][16];

  interfaceConfigLoad();
  if (frisch)
  {
    current_selection = interfaceWinControlBaudIndex();
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  for (uint8_t i = 0; i < IF_BAUD_COUNT; i++) interfaceBuildBaudText(i, baudText[i], sizeof(baudText[i]));
  const char* items[] = { baudText[0], baudText[1], baudText[2], baudText[3], baudText[4], ifTextBack() };

  if (interfaceRunStringList(ifText("WinControl Baud", "WinControl baud"), items, 6, current_selection, 5))
  {
    if (current_selection >= 0 && current_selection < IF_BAUD_COUNT)
    {
      interface_cfg.wincontrol_baud_index = (uint8_t)current_selection;
      if (interface_cfg.rs232_mode[0] == IF_RS232_MODE_WINCONTROL) interface_cfg.rs232_baud_index[0] = interface_cfg.wincontrol_baud_index;
      if (interface_cfg.rs232_mode[1] == IF_RS232_MODE_WINCONTROL) interface_cfg.rs232_baud_index[1] = interface_cfg.wincontrol_baud_index;
      interfaceConfigSave();
      interfaceApplySerialBaud();
    }
    frisch = true;
    interfaceReturnTo(MENU_WINCONTROL);
  }
}

static void FLASHMEM interfaceWinControlNumberMenu(uint8_t kind)
{
  static MenuPageDrawFlag page_drawn;
  static uint8_t active_kind = 255;
  static uint16_t editValue = 0;
  static uint16_t lastValue = 0xFFFF;

  interfaceConfigLoad();
  if (active_kind != kind)
  {
    active_kind = kind;
    editValue = (kind == 0) ? interfaceWinControlAddress() : interfaceWinControlTcpPort();
    page_drawn = false;
    lastValue = 0xFFFF;
    flag.menu_lcd_upd = false;
  }

  const uint16_t minValue = (kind == 0) ? 0 : 1;
  const uint16_t maxValue = (kind == 0) ? 99 : 65535;

  bool valueDone = false;
  if (kind == 0)
  {
    valueDone = menuValueHandleUInt16(editValue, minValue, maxValue, 1, nullptr, 130);
  }
  else
  {
    valueDone = interfaceValueHandleUInt16Fast(editValue, minValue, maxValue, 1, nullptr, 130);
  }

  if (valueDone)
  {
    if (kind == 0) interface_cfg.wincontrol_address = (uint8_t)editValue;
    else interface_cfg.wincontrol_tcp_port = editValue;
    interfaceConfigSave();
    menuDirectClearAllRows();
    page_drawn = false;
    active_kind = 255;
    interfaceReturnTo(MENU_WINCONTROL);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_MENU_WINCONTROL, false);
      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((kind == 0) ? ifText("Adresse:", "Address:") : "TCP-Port:");
      VirtLCDMenu->setCursor(1, 8);
      if (kind == 0) VirtLCDMenu->print(ifText("UP/DOWN: +/- 1", "UP/DOWN: +/- 1"));
      else VirtLCDMenu->print("UP/DOWN: +/- 1/10/100");
      VirtLCDMenu->setCursor(1, 10);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));
      VirtLCDMenu->transfer();
      ReadButtons(true);
      menuDirectResetValueCache();
      page_drawn = true;
      lastValue = 0xFFFF;
    }
    else flag.menu_lcd_upd = true;

    if (lastValue != editValue)
    {
      if (kind == 0) snprintf(lcd_buf, 255, "%02u", (unsigned)editValue);
      else snprintf(lcd_buf, 255, "%u", (unsigned)editValue);
      menuDirectWriteFixedRow(0, 13, 4, lcd_buf, (kind == 0) ? 2 : 5, WHITE);
      lastValue = editValue;
    }
    ReadButtons(true);
  }
}

void FLASHMEM interface_wincontrol_address_menu(void) { interfaceWinControlNumberMenu(0); }
void FLASHMEM interface_wincontrol_tcp_port_menu(void) { interfaceWinControlNumberMenu(1); }

void FLASHMEM interface_wincontrol_cycle_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();
  if (frisch)
  {
    current_selection = interfaceWinControlCycleIndex();
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = { ifWinControlCycleText(IF_WINCONTROL_CYCLE_1S), ifWinControlCycleText(IF_WINCONTROL_CYCLE_10S), ifWinControlCycleText(IF_WINCONTROL_CYCLE_20S), ifWinControlCycleText(IF_WINCONTROL_CYCLE_60S), ifTextBack() };

  if (interfaceRunStringList(ifText("WinControl Zyklus", "WinControl cycle"), items, 5, current_selection, 5))
  {
    if (current_selection >= 0 && current_selection < IF_WINCONTROL_CYCLE_COUNT)
    {
      interface_cfg.wincontrol_cycle_index = (uint8_t)current_selection;
      interfaceConfigSave();
    }
    frisch = true;
    interfaceReturnTo(MENU_WINCONTROL);
  }
}

void FLASHMEM interface_wincontrol_status_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;
  static char outLine[40], rsLine[40], ethLine[40], clientLine[40], addrLine[40];

  interfaceConfigLoad();
  if (frisch)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const int8_t rsPort = interfaceWinControlRs232Port();
  snprintf(outLine, sizeof(outLine), "%s: %s", ifText("Ausgabe", "Output"), ifWinControlOutputText(interfaceWinControlOutputMode()));
  snprintf(rsLine, sizeof(rsLine), "RS232: %s", (rsPort < 0) ? ifText("Aus", "Off") : ((rsPort == 0) ? "RS232-1" : "RS232-2"));
  snprintf(ethLine, sizeof(ethLine), "Eth %u: %s", (unsigned)interfaceWinControlTcpPort(), ifTextOnOff(interfaceWinControlEthernetEnabled()));
  snprintf(clientLine, sizeof(clientLine), "TCP Client: %s", winControlOutEthernetClientConnected() ? "OK" : "--");
  snprintf(addrLine, sizeof(addrLine), "Adresse: G%02u", (unsigned)interfaceWinControlAddress());

  const char* items[] = { outLine, rsLine, ethLine, clientLine, addrLine, ifTextBack() };
  if (interfaceRunStringList(ifText("WinControl Status", "WinControl status"), items, 6, current_selection, 5, 100))
  {
    frisch = true;
    interfaceReturnTo(MENU_WINCONTROL);
  }
}

// =========================================================================
// ETHERNET MENUES
// =========================================================================

void FLASHMEM interface_ethernet_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  if (frisch)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    ifText("Aktiv", "Enabled"),
    "DHCP",
    ifText("IP-Adresse", "IP address"),
    "Subnet",
    "Gateway",
    "DNS",
    "TCP-Port",
    ifText("Webserver", "Web server"),
    "Web-Port",
    ifText("Web-Setup", "Web setup"),
    ifTextBack()
  };

  if (interfaceRunStringList(T(TXT_MENU_ETHERNET), items, 11, current_selection, 5))
  {
    switch (current_selection)
    {
      case 0:  menu_level = MENU_ETH_ACTIVE; break;
      case 1:  menu_level = MENU_ETH_DHCP; break;
      case 2:  menu_level = MENU_ETH_IP; break;
      case 3:  menu_level = MENU_ETH_SUBNET; break;
      case 4:  menu_level = MENU_ETH_GATEWAY; break;
      case 5:  menu_level = MENU_ETH_DNS; break;
      case 6:  menu_level = MENU_ETH_TCP_PORT; break;
      case 7:  menu_level = MENU_ETH_WEBSERVER; break;
      case 8:  menu_level = MENU_ETH_WEB_PORT; break;
      case 9:  menu_level = MENU_ETH_WEB_SETUP; break;
      default:
        menu_level = MENU_INTERFACES;
        frisch = true;
        break;
    }

    flag.menu_lcd_upd = false;
  }
}

static void FLASHMEM interfaceToggleMenu(const char* title,
                                uint8_t& value,
                                uint16_t return_level)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  if (frisch)
  {
    current_selection = value ? 1 : 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    ifTextOnOff(0),
    ifTextOnOff(1),
    ifTextBack()
  };

  if (interfaceRunStringList(title, items, 3, current_selection))
  {
    if (current_selection < 2)
    {
      value = (uint8_t)current_selection;
      interfaceConfigSave();
    }

    frisch = true;
    interfaceReturnTo(return_level);
  }
}

// Kurze Warte-Seite fuer Ethernet-Initialisierung.
// Wichtig: Diese Seite wird VOR Ethernet.begin()/DHCP gezeichnet, damit
// das Geraet auch bei 3 s Timeout nicht scheinbar haengt.
static void FLASHMEM interfaceEthShowWaitScreen(const char* message)
{
  if (VirtLCDMenu == nullptr) return;

  menuDirectClearAllRows();
  flag.menu_lcd_upd = true;

  FirstBut = 0;
  LastBut = 3;

  VirtLCDMenu->clear();
  VirtLCDMenu->setCursor(1, 1);
  VirtLCDMenu->print(T(TXT_MENU_ETHERNET));

  VirtLCDMenu->setCursor(1, 5);
  VirtLCDMenu->print(message);

  VirtLCDMenu->setCursor(1, 7);
  VirtLCDMenu->print(T(TXT_PLEASE_WAIT));

  VirtLCDMenu->transfer();
  ReadButtons(true);

  // Das naechste Menuebild muss komplett neu aufgebaut werden.
  flag.menu_lcd_upd = false;
}


static void FLASHMEM interfaceEthRestartForModeChange()
{
  // NativeEthernet/FNET stellt keine belastbare end()-/Neuinitialisierungs-API
  // bereit. Ein Wechsel zwischen statischer Adresse und DHCP wird deshalb nach
  // dem Speichern kontrolliert durch einen sauberen Geraeteneustart wirksam.
  // Das ist reproduzierbar und verhindert den bisherigen 8-s-Watchdog-Haenger.
  interfaceEthShowWaitScreen(T(TXT_FACTORY_REBOOT));
  safetyBeginBlockingOperation();
  delay(250);
  SOFT_RESET();
  while (true) { delay(10); }
}

static void FLASHMEM interfaceEthApplyWithWaitScreen(void)
{
  interfaceEthShowWaitScreen(T(TXT_ETH_INIT_MESSAGE));

  // Wichtig:
  // Die kurze blockierende Ethernet-/DHCP-Pruefung wird sofort HIER ausgefuehrt,
  // waehrend die Warte-Seite sichtbar ist. Sonst zeichnet das Menue vorher neu
  // und die 3-s-Blockade passiert optisch im Ethernet-Menue.
  ethernetServiceApplyNow();

  // Nach der Pruefung wieder ins Ethernet-Menue und dort komplett neu zeichnen.
  menuDirectClearAllRows();
  menu_level = MENU_ETHERNET;
  flag.menu_lcd_upd = false;
  interfaceConsumeTouch();
}

void FLASHMEM interface_eth_active_menu(void)
{
  interfaceConfigLoad();
  uint8_t oldValue = interface_cfg.eth_enabled;

  interfaceToggleMenu(ifText("Ethernet aktiv", "Ethernet enabled"),
                      interface_cfg.eth_enabled,
                      MENU_ETHERNET);

  // Beim Einschalten erfolgt die echte Ethernet-Initialisierung direkt hinter
  // der Warte-Seite. Der Link wird erst nach Ethernet.begin() bewertet.
  if (oldValue != interface_cfg.eth_enabled && interface_cfg.eth_enabled)
  {
    interfaceEthApplyWithWaitScreen();
  }
}

void FLASHMEM interface_eth_dhcp_menu(void)
{
  interfaceConfigLoad();
  uint8_t oldValue = interface_cfg.eth_dhcp;
  interfaceToggleMenu("DHCP",
                      interface_cfg.eth_dhcp,
                      MENU_ETHERNET);

  // Der Wechsel DHCP <-> statisch benoetigt bei NativeEthernet/FNET einen
  // sauberen Stack-Neustart. Ein erneutes Ethernet.begin() im bereits
  // initialisierten Gegenmodus kann keine Lease starten und bis zum Watchdog
  // blockieren. Die Auswahl ist bereits persistent gespeichert.
  if (oldValue != interface_cfg.eth_dhcp)
  {
    interfaceEthRestartForModeChange();
  }
}

void FLASHMEM interface_eth_webserver_menu(void)
{
  interfaceToggleMenu(ifText("Webserver", "Web server"),
                      interface_cfg.web_enabled,
                      MENU_ETHERNET);
}

void FLASHMEM interface_eth_web_setup_menu(void)
{
  interfaceToggleMenu(ifText("Web-Setup", "Web setup"),
                      interface_cfg.web_setup_enabled,
                      MENU_ETHERNET);
}

static uint8_t* INTERFACE_FLASHMEM_NOINLINE interfaceIpPtr(uint8_t kind)
{
  switch (kind)
  {
    case 1: return interface_cfg.eth_subnet;
    case 2: return interface_cfg.eth_gateway;
    case 3: return interface_cfg.eth_dns;
    case 0:
    default: return interface_cfg.eth_ip;
  }
}

static const char* FLASHMEM interfaceIpTitle(uint8_t kind)
{
  switch (kind)
  {
    case 1: return "Subnet";
    case 2: return "Gateway";
    case 3: return "DNS";
    case 0:
    default: return ifText("IP-Adresse", "IP address");
  }
}

static void FLASHMEM interfaceIpMenu(uint8_t kind)
{
  static MenuPageDrawFlag page_drawn;
  static uint8_t active_kind = 255;
  static uint8_t octet = 0;
  static uint8_t edit_ip[4] = {0, 0, 0, 0};
  static uint8_t edit_dhcp = 1;
  static char last_value[24] = "";
  static uint8_t last_octet = 255;
  static uint8_t last_readonly = 255;
  static uint32_t last_runtime_ip_refresh_ms = 0;

  interfaceConfigLoad();

  if (active_kind != kind)
  {
    active_kind = kind;
    page_drawn = false;
    octet = 0;
    last_value[0] = '\0';
    last_octet = 255;
    last_readonly = 255;
    last_runtime_ip_refresh_ms = 0;

    // _28: IP/Subnet/Gateway/DNS werden lokal editiert. Dadurch kann ein
    // Zwischenzustand waehrend der Eingabe DHCP nicht mehr scheinbar von
    // selbst einschalten, und ein halbfertiger statischer IP-Satz wird nicht
    // vorzeitig in die globale Konfiguration geschrieben.
    edit_dhcp = interface_cfg.eth_dhcp ? 1 : 0;
    uint8_t* src = interfaceIpPtr(kind);
    edit_ip[0] = src[0];
    edit_ip[1] = src[1];
    edit_ip[2] = src[2];
    edit_ip[3] = src[3];

    flag.menu_lcd_upd = false;
  }

  const bool readonly = (edit_dhcp != 0);

  if (!readonly)
  {
    // Harte Bedien-Sperre gegen den beobachteten Fehler: Solange diese Seite
    // als statische Eingabeseite betreten wurde, bleibt DHCP AUS. Selbst wenn
    // ein anderer Pfad waehrend der Eingabe den RAM-Wert veraendern sollte,
    // wird er vor Anzeige und Speichern wieder auf statisch gesetzt.
    interface_cfg.eth_dhcp = 0;
  }

  if (last_readonly != (uint8_t)readonly)
  {
    last_readonly = (uint8_t)readonly;
    page_drawn = false;
    last_value[0] = '\0';
    last_octet = 255;
    flag.menu_lcd_upd = false;
  }

  // Bei DHCP zeigt die IP-Seite die wirklich aktive Adresse. Die reine
  // Link-/IP-Abfrage ist nicht blockierend; alle 500 ms wird nur geprueft,
  // ob sich Anzeige oder Kabelstatus geaendert haben.
  if (readonly && kind == 0)
  {
    uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - last_runtime_ip_refresh_ms) >= 500UL)
    {
      last_runtime_ip_refresh_ms = nowMs;
      flag.menu_lcd_upd = false;
    }
  }

  ReadButtons(false);
  uint8_t key = menuReadKey(170);

  if (readonly)
  {
    if (key == MK_ENTER)
    {
      flag.short_push = false;
      menuDirectClearAllRows();
      page_drawn = false;
      active_kind = 255;
      last_readonly = 255;
      interfaceReturnTo(MENU_ETHERNET);
      return;
    }
  }
  else
  {
    if (key == MK_UP)
    {
      edit_ip[octet]++;
      flag.menu_lcd_upd = false;
    }
    else if (key == MK_DOWN)
    {
      edit_ip[octet]--;
      flag.menu_lcd_upd = false;
    }
    else if (key == MK_ENTER)
    {
      flag.short_push = false;

      if (octet < 3)
      {
        octet++;
        flag.menu_lcd_upd = false;
      }
      else
      {
        uint8_t* dst = interfaceIpPtr(kind);
        dst[0] = edit_ip[0];
        dst[1] = edit_ip[1];
        dst[2] = edit_ip[2];
        dst[3] = edit_ip[3];
        interface_cfg.eth_dhcp = 0;
        interfaceConfigSave();
        menuDirectClearAllRows();
        page_drawn = false;
        active_kind = 255;
        last_readonly = 255;
        interfaceReturnTo(MENU_ETHERNET);
        return;
      }
    }
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_MENU_ETHERNET, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print(interfaceIpTitle(kind));
      VirtLCDMenu->print(":");

      if (readonly)
      {
        VirtLCDMenu->setCursor(1, 7);
        VirtLCDMenu->print(ifText("DHCP aktiv", "DHCP active"));

        VirtLCDMenu->setCursor(1, 8);
        VirtLCDMenu->print(ifText("Nur Anzeige", "Read only"));

        VirtLCDMenu->setCursor(1, 10);
        VirtLCDMenu->print(ifText("ENTER zurück", "ENTER back"));
      }
      else
      {
        VirtLCDMenu->setCursor(1, 7);
        VirtLCDMenu->print(ifText("Oktett:", "Octet:"));

        VirtLCDMenu->setCursor(1, 9);
        VirtLCDMenu->print(ifText("UP/DOWN ändern", "UP/DOWN change"));

        VirtLCDMenu->setCursor(1, 10);
        VirtLCDMenu->print(ifText("ENTER weiter/speichern", "ENTER next/save"));
      }

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_value[0] = '\0';
      last_octet = 255;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (readonly && kind == 0)
    {
      if (ethernetHasValidIp())
      {
        uint8_t activeIp[4];
        ethernetGetCurrentIp(activeIp);
        snprintf(lcd_buf, 255, "%3u.%3u.%3u.%3u",
                 activeIp[0], activeIp[1], activeIp[2], activeIp[3]);
      }
      else
      {
        snprintf(lcd_buf, 255, "---.---.---.---");
      }
    }
    else if (readonly)
    {
      uint8_t* ip = interfaceIpPtr(kind);
      snprintf(lcd_buf, 255, "%3u.%3u.%3u.%3u", ip[0], ip[1], ip[2], ip[3]);
    }
    else
    {
      snprintf(lcd_buf, 255, "%3u.%3u.%3u.%3u", edit_ip[0], edit_ip[1], edit_ip[2], edit_ip[3]);
    }

    if (strncmp(last_value, lcd_buf, sizeof(last_value)) != 0)
    {
      menuDirectWriteFixedRow(0, 1, 5, lcd_buf, 15, WHITE);
      snprintf(last_value, sizeof(last_value), "%.23s", lcd_buf);
    }

    if (!readonly && last_octet != octet)
    {
      snprintf(lcd_buf, 255, "%u / 4", (unsigned)(octet + 1));
      menuDirectWriteFixedRow(1, 10, 7, lcd_buf, 5, WHITE);
      last_octet = octet;
    }

    ReadButtons(true);
  }
}

void FLASHMEM interface_eth_ip_menu(void)
{
  interfaceIpMenu(0);
}

void FLASHMEM interface_eth_subnet_menu(void)
{
  interfaceIpMenu(1);
}

void FLASHMEM interface_eth_gateway_menu(void)
{
  interfaceIpMenu(2);
}

void FLASHMEM interface_eth_dns_menu(void)
{
  interfaceIpMenu(3);
}

static void FLASHMEM interfacePortMenu(uint8_t port_kind)
{
  static MenuPageDrawFlag page_drawn;
  static uint8_t active_kind = 255;
  static uint16_t last_port = 0xFFFF;

  interfaceConfigLoad();

  if (active_kind != port_kind)
  {
    active_kind = port_kind;
    page_drawn = false;
    last_port = 0xFFFF;
    flag.menu_lcd_upd = false;
  }

  uint16_t& value = (port_kind == 0) ? interface_cfg.eth_tcp_port : interface_cfg.web_port;

  if (interfaceValueHandleUInt16Fast(value, 1, 65535, 1, nullptr, 130))
  {
    interfaceConfigSave();
    menuDirectClearAllRows();
    page_drawn = false;
    active_kind = 255;
    interfaceReturnTo(MENU_ETHERNET);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_MENU_ETHERNET, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((port_kind == 0) ? "TCP-Port:" : "Web-Port:");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print("UP/DOWN: +/- 1/10/100");

      VirtLCDMenu->setCursor(1, 10);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_port = 0xFFFF;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_port != value)
    {
      snprintf(lcd_buf, 255, "%5u", value);
      menuDirectWriteFixedRow(0, 11, 4, lcd_buf, 5, WHITE);
      last_port = value;
    }

    ReadButtons(true);
  }
}

void FLASHMEM interface_eth_tcp_port_menu(void)
{
  interfacePortMenu(0);
}

void FLASHMEM interface_eth_web_port_menu(void)
{
  interfacePortMenu(1);
}



// Kurze Warte-Seite fuer blockierende SD-Pruefungen.
// Wichtig: Diese Seite wird VOR SD.begin()/SD.open() gezeichnet, damit
// das Geraet nicht scheinbar haengt, wenn der SD-Slot kurz blockiert.
static void FLASHMEM interfaceSdShowWaitScreen(const char* message)
{
  if (VirtLCDMenu == nullptr) return;

  menuDirectClearAllRows();
  flag.menu_lcd_upd = true;

  FirstBut = 0;
  LastBut = 3;

  VirtLCDMenu->clear();
  VirtLCDMenu->setCursor(1, 1);
  VirtLCDMenu->print(T(TXT_MENU_SD_CARD));

  VirtLCDMenu->setCursor(1, 5);
  VirtLCDMenu->print(message);

  VirtLCDMenu->setCursor(1, 7);
  VirtLCDMenu->print(T(TXT_PLEASE_WAIT));

  VirtLCDMenu->transfer();
  ReadButtons(false);

  // Nach der Warte-Seite muss das folgende Menuebild zwingend
  // komplett neu gezeichnet werden. Sonst bleibt "SD-Karte pruefen"
  // stehen, bis eine Taste/Touch ein Redraw ausloest.
  flag.menu_lcd_upd = false;
}

static bool interfaceSdStatusAlreadyChecked = false;


// =========================================================================
// ALLGEMEINE DATENAUSGABE-MENUES
// =========================================================================

void FLASHMEM interface_output_interval_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  if (frisch)
  {
    current_selection = interface_cfg.output_interval_index;
    if (current_selection < 0 || current_selection >= IF_OUTPUT_INTERVAL_COUNT) current_selection = 1;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    ifOutputIntervalText(0),
    ifOutputIntervalText(1),
    ifOutputIntervalText(2),
    ifOutputIntervalText(3),
    ifOutputIntervalText(4),
    ifTextBack()
  };

  if (interfaceRunStringList(T(TXT_MENU_OUTPUT_INTERVAL), items, 6, current_selection, 5))
  {
    if (current_selection < IF_OUTPUT_INTERVAL_COUNT)
    {
      interface_cfg.output_interval_index = (uint8_t)current_selection;
      interface_cfg.output_filter_index = interfaceDefaultFilterForOutput(interface_cfg.output_interval_index);
      interfaceConfigSave();
      sdLogResetSchedule();
    }

    frisch = true;
    interfaceReturnTo(MENU_INTERFACES);
  }
}

void FLASHMEM interface_output_filter_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  uint8_t outputIndex = interface_cfg.output_interval_index;
  uint8_t allowedCount = IF_OUTPUT_FILTER_COUNT;
  if (outputIndex == 0) allowedCount = 1;       // ADC: nur 0 s
  else if (outputIndex == 1) allowedCount = 2;  // 1 s: 0 s / 1 s

  if (frisch)
  {
    current_selection = interface_cfg.output_filter_index;
    if (current_selection < 0 || current_selection >= allowedCount) current_selection = interfaceDefaultFilterForOutput(outputIndex);
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items5[] = {
    ifOutputFilterText(0),
    ifOutputFilterText(1),
    ifOutputFilterText(2),
    ifOutputFilterText(3),
    ifOutputFilterText(4),
    ifTextBack()
  };
  const char* items2[] = {
    ifOutputFilterText(0),
    ifOutputFilterText(1),
    ifTextBack()
  };
  const char* items1[] = {
    ifOutputFilterText(0),
    ifTextBack()
  };

  const char** items = items5;
  uint8_t menuCount = 6;
  uint8_t lines = 5;
  if (allowedCount == 1)
  {
    items = items1;
    menuCount = 2;
    lines = 2;
  }
  else if (allowedCount == 2)
  {
    items = items2;
    menuCount = 3;
    lines = 3;
  }

  if (interfaceRunStringList(T(TXT_MENU_OUTPUT_FILTER), items, menuCount, current_selection, lines))
  {
    if (current_selection < allowedCount)
    {
      interface_cfg.output_filter_index = (uint8_t)current_selection;
      interfaceConfigSave();
      sdLogResetSchedule();
    }

    frisch = true;
    interfaceReturnTo(MENU_INTERFACES);
  }
}

void FLASHMEM interface_diagnostic_data_menu(void)
{
  interfaceConfigLoad();
  uint8_t oldValue = interface_cfg.diagnostic_data_enabled;

  interfaceToggleMenu(T(TXT_MENU_DIAGNOSTIC_DATA),
                      interface_cfg.diagnostic_data_enabled,
                      MENU_INTERFACES);

  if (oldValue != interface_cfg.diagnostic_data_enabled)
  {
    // Diagnose AN/AUS hat andere CSV-Spalten und anderen Dateinamen.
    // Puffer leeren, damit keine gemischten Formate in einer Datei landen.
    sdLogResetSchedule();
  }
}

// =========================================================================
// SD-KARTEN / LOGGING MENUES
// =========================================================================

void FLASHMEM interface_sd_output_interval_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  if (frisch)
  {
    current_selection = interface_cfg.sd_output_interval_index;
    if (current_selection < 0 || current_selection >= IF_OUTPUT_INTERVAL_COUNT) current_selection = 1;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    ifOutputIntervalText(0),
    ifOutputIntervalText(1),
    ifOutputIntervalText(2),
    ifOutputIntervalText(3),
    ifOutputIntervalText(4),
    ifTextBack()
  };

  if (interfaceRunStringList(ifText("SD Ausgabe Intervall", "SD Output Interval"), items, 6, current_selection, 5))
  {
    if (current_selection < IF_OUTPUT_INTERVAL_COUNT)
    {
      interface_cfg.sd_output_interval_index = (uint8_t)current_selection;
      interface_cfg.sd_output_filter_index = interfaceDefaultFilterForOutput(interface_cfg.sd_output_interval_index);
      interfaceClampSdWriteIntervalToOutput();
      interfaceConfigSave();
      sdLogResetSchedule();
    }

    frisch = true;
    interfaceReturnTo(MENU_SD_CARD);
  }
}

void FLASHMEM interface_sd_output_filter_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  uint8_t outputIndex = interface_cfg.sd_output_interval_index;
  uint8_t allowedCount = IF_OUTPUT_FILTER_COUNT;
  if (outputIndex == 0) allowedCount = 1;       // ADC: nur 0 s
  else if (outputIndex == 1) allowedCount = 2;  // 1 s: 0 s / 1 s

  if (frisch)
  {
    current_selection = interface_cfg.sd_output_filter_index;
    if (current_selection < 0 || current_selection >= allowedCount) current_selection = interfaceDefaultFilterForOutput(outputIndex);
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items5[] = {
    ifOutputFilterText(0),
    ifOutputFilterText(1),
    ifOutputFilterText(2),
    ifOutputFilterText(3),
    ifOutputFilterText(4),
    ifTextBack()
  };
  const char* items2[] = {
    ifOutputFilterText(0),
    ifOutputFilterText(1),
    ifTextBack()
  };
  const char* items1[] = {
    ifOutputFilterText(0),
    ifTextBack()
  };

  const char** items = items5;
  uint8_t menuCount = 6;
  uint8_t lines = 5;
  if (allowedCount == 1)
  {
    items = items1;
    menuCount = 2;
    lines = 2;
  }
  else if (allowedCount == 2)
  {
    items = items2;
    menuCount = 3;
    lines = 3;
  }

  if (interfaceRunStringList(ifText("SD Ausgabe Filter", "SD Output Filter"), items, menuCount, current_selection, lines))
  {
    if (current_selection < allowedCount)
    {
      interface_cfg.sd_output_filter_index = (uint8_t)current_selection;
      interfaceConfigSave();
      sdLogResetSchedule();
    }

    frisch = true;
    interfaceReturnTo(MENU_SD_CARD);
  }
}

void FLASHMEM interface_sd_diagnostic_data_menu(void)
{
  interfaceConfigLoad();
  uint8_t oldValue = interface_cfg.sd_diagnostic_data_enabled;

  interfaceToggleMenu(ifText("SD Diagnose Daten", "SD Diagnostic Data"),
                      interface_cfg.sd_diagnostic_data_enabled,
                      MENU_SD_CARD);

  if (oldValue != interface_cfg.sd_diagnostic_data_enabled)
  {
    // SD Diagnose AN/AUS hat andere CSV-Spalten und anderen Dateinamen.
    // Puffer leeren, damit keine gemischten Formate in einer Datei landen.
    sdLogResetSchedule();
  }
}

void FLASHMEM interface_sd_log_integrity_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();
  const bool certifiedReady = tpSignedDataCertifiedModeReady();

  if (frisch)
  {
    current_selection = interface_cfg.sd_log_integrity_mode;
    if (current_selection < 0 || current_selection >= TP_LOG_INTEGRITY_COUNT)
      current_selection = TP_LOG_INTEGRITY_OFF;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] =
  {
    ifText("Aus", "Off"),
    "SHA-256",
    certifiedReady
      ? ifText("CSV zertifiziert", "Certified CSV")
      : ifText("CSV zertifiziert (gesperrt)", "Certified CSV (locked)"),
    certifiedReady
      ? ifText("TPLOG zertifiziert", "Certified TPLOG")
      : ifText("TPLOG zertifiziert (gesperrt)", "Certified TPLOG (locked)"),
    ifTextBack()
  };

  if (interfaceRunStringList(ifText("Log-Integritaet", "Log integrity"),
                             items, 5, current_selection, 5))
  {
    if (current_selection < TP_LOG_INTEGRITY_COUNT)
    {
      const uint8_t requested = (uint8_t)current_selection;
      if (!tpLogIntegrityIsCertified(requested) || certifiedReady)
      {
        if (interface_cfg.sd_log_integrity_mode != requested)
        {
          interface_cfg.sd_log_integrity_mode = requested;
          interfaceConfigSave();
          // Ein Integritaetswechsel darf spaeter nie unbemerkt in derselben
          // Provenienzperiode fortlaufen. Der Logger bekommt deshalb bereits
          // jetzt denselben Reset-Hook wie Format-/Intervallaenderungen.
          sdLogResetSchedule();
        }
      }
    }

    frisch = true;
    interfaceReturnTo(MENU_SD_CARD);
  }
}

void FLASHMEM interface_sd_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  if (frisch)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    "Status",
    "Logging",
    ifText("SD Ausgabe Intervall", "SD Output Interval"),
    ifText("SD Ausgabe Filter", "SD Output Filter"),
    ifText("SD Schreiben", "SD Write"),
    ifText("SD Diagnose Daten", "SD Diagnostic Data"),
    ifText("Kopfzeile", "Header"),
    ifText("Log-Integritaet", "Log integrity"),
    ifTextBack()
  };

  if (interfaceRunStringList(T(TXT_MENU_SD_CARD), items, 9, current_selection, 5))
  {
    switch (current_selection)
    {
      case 0:  menu_level = MENU_SD_STATUS; break;
      case 1:  menu_level = MENU_SD_LOGGING; break;
      case 2:  menu_level = MENU_SD_OUTPUT_INTERVAL; break;
      case 3:  menu_level = MENU_SD_OUTPUT_FILTER; break;
      case 4:  menu_level = MENU_SD_INTERVAL; break;
      case 5:  menu_level = MENU_SD_DIAGNOSTIC_DATA; break;
      case 6:  menu_level = MENU_SD_HEADER; break;
      case 7:  menu_level = MENU_SD_LOG_INTEGRITY; break;
      default:
        menu_level = MENU_INTERFACES;
        frisch = true;
        break;
    }

    flag.menu_lcd_upd = false;
  }
}

void FLASHMEM interface_sd_status_menu(void)
{
  static MenuPageDrawFlag page_drawn;
  static char last_status[64] = "";
  static char last_file[32] = "";
  static char last_error[32] = "";
  static uint8_t last_logging = 255;
  static uint8_t last_interval = 255;

  interfaceConfigLoad();

  ReadButtons(false);
  if (menuReadKey(250) == MK_ENTER)
  {
    flag.short_push = false;
    menuDirectClearAllRows();
    page_drawn = false;
    interfaceReturnTo(MENU_SD_CARD);
    return;
  }

  if (!page_drawn)
  {
    // Beim Oeffnen der Statusseite eine noch nicht initialisierte Karte genau
    // einmal pruefen. Ist sie bereits fuer CSV, Web oder Seriellog bereit,
    // bleibt der bestehende Dateizustand unangetastet.
    // Vor einer potentiell blockierenden SD-Pruefung erst eine Warte-Seite zeichnen.
    if (!interfaceSdStatusAlreadyChecked)
    {
      interfaceSdShowWaitScreen(ifText("SD-Karte prüfen", "Checking SD card"));
      safetyBeginBlockingOperation();
      sdLogCheckOnce();
      safetyEndBlockingOperation();
    }
    interfaceSdStatusAlreadyChecked = false;

    menuValueBeginDraw(TXT_MENU_SD_CARD, false);

    VirtLCDMenu->setCursor(1, 4);
    VirtLCDMenu->print("Status:");

    VirtLCDMenu->setCursor(1, 5);
    VirtLCDMenu->print("Logging:");

    VirtLCDMenu->setCursor(1, 6);
    VirtLCDMenu->print(ifText("SD Schreiben:", "SD Write:"));

    VirtLCDMenu->setCursor(1, 7);
    VirtLCDMenu->print("Format: CSV-DE (;)");

    VirtLCDMenu->setCursor(1, 8);
    VirtLCDMenu->print("Datei:");

    VirtLCDMenu->setCursor(1, 10);
    VirtLCDMenu->print(ifText("ENTER zurück", "ENTER back"));

    VirtLCDMenu->transfer();
    ReadButtons(false);

    menuDirectResetValueCache();
    page_drawn = true;
    // Direktwerte direkt nach dem Seitenaufbau genau einmal schreiben.
    flag.menu_lcd_upd = false;
    last_status[0] = '\0';
    last_file[0] = '\0';
    last_error[0] = '\0';
    last_logging = 255;
    last_interval = 255;
  }

  if (page_drawn && !flag.menu_lcd_upd)
  {
    // Die SD-Statusseite ist eine reine Momentaufnahme.
    // Keine zyklische SD-Pruefung und keine dauernde Button-Neuzeichnung,
    // sonst flackern UP/DOWN/ENTER/EXIT und ohne Karte kann SD.begin blockieren.
    const char* statusText = (ui_language == LANG_EN) ? sdLogGetStatusTextEN() : sdLogGetStatusTextDE();
    if (strncmp(last_status, statusText, sizeof(last_status)) != 0)
    {
      menuDirectWriteFixedRow(0, 10, 4, statusText, 16, WHITE);
      snprintf(last_status, sizeof(last_status), "%s", statusText);
    }

    if (last_logging != interface_cfg.sd_logging_enabled)
    {
      menuDirectWriteFixedRow(1, 10, 5, ifTextOnOff(interface_cfg.sd_logging_enabled), 4, WHITE);
      last_logging = interface_cfg.sd_logging_enabled;
    }

    if (last_interval != interface_cfg.sd_interval_index)
    {
      menuDirectWriteFixedRow(2, 15, 6, ifSdIntervalText(interface_cfg.sd_interval_index), 6, WHITE);
      last_interval = interface_cfg.sd_interval_index;
    }

    const char* fileText = sdLogGetCurrentFile();
    if (fileText == nullptr || fileText[0] == '\0') fileText = "-";
    if (strncmp(last_file, fileText, sizeof(last_file)) != 0)
    {
      menuDirectWriteFixedRow(3, 7, 8, fileText, 17, WHITE);
      snprintf(last_file, sizeof(last_file), "%s", fileText);
    }

    const char* errText = sdLogGetLastError();
    if (errText == nullptr) errText = "";
    if (strncmp(last_error, errText, sizeof(last_error)) != 0)
    {
      menuDirectWriteFixedRow(4, 1, 9, errText, 18, YELLOW);
      snprintf(last_error, sizeof(last_error), "%s", errText);
    }

    flag.menu_lcd_upd = true;
  }
}

void FLASHMEM interface_sd_logging_menu(void)
{
  uint8_t oldValue = interface_cfg.sd_logging_enabled;

  interfaceToggleMenu(ifText("SD Logging", "SD logging"),
                      interface_cfg.sd_logging_enabled,
                      MENU_SD_CARD);

  if (oldValue != interface_cfg.sd_logging_enabled)
  {
    sdLogResetSchedule();

    if (interface_cfg.sd_logging_enabled)
    {
      // Beim Einschalten genau einmal nach der Karte suchen.
      // Vor der potentiell blockierenden SD-Pruefung erst eine Warte-Seite zeichnen.
      interfaceSdShowWaitScreen(ifText("SD-Karte prüfen", "Checking SD card"));
      safetyBeginBlockingOperation();
      sdLogBegin();
      safetyEndBlockingOperation();
      interfaceSdStatusAlreadyChecked = true;

      if (!sdLogReadyForLogging())
      {
        interface_cfg.sd_logging_enabled = 0;
        interfaceConfigSave();
        sdLogResetSchedule();
        interfaceReturnTo(MENU_SD_STATUS);
      }
      else
      {
        // Logging ist aktiv: nach der SD-Pruefung wieder sauber ins
        // SD-Menue zurueck. Die Warte-Seite darf nicht stehenbleiben.
        interfaceReturnTo(MENU_SD_CARD);
      }
    }
  }
}

void FLASHMEM interface_sd_interval_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();
  interfaceClampSdWriteIntervalToOutput();

  uint8_t maxIdx = interfaceSdMaxWriteIntervalIndexForOutput(interface_cfg.sd_output_interval_index);
  uint8_t allowedCount = (uint8_t)(maxIdx + 1);
  if (allowedCount > IF_SD_INTERVAL_COUNT) allowedCount = IF_SD_INTERVAL_COUNT;

  if (frisch)
  {
    current_selection = interface_cfg.sd_interval_index;
    if (current_selection < 0 || current_selection >= allowedCount) current_selection = maxIdx;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items4[] = {
    ifSdIntervalText(0),
    ifSdIntervalText(1),
    ifSdIntervalText(2),
    ifSdIntervalText(3),
    ifTextBack()
  };
  const char* items3[] = {
    ifSdIntervalText(0),
    ifSdIntervalText(1),
    ifSdIntervalText(2),
    ifTextBack()
  };
  const char* items2[] = {
    ifSdIntervalText(0),
    ifSdIntervalText(1),
    ifTextBack()
  };

  const char** items = items4;
  uint8_t menuCount = 5;
  uint8_t lines = 5;
  if (allowedCount == 2)
  {
    items = items2;
    menuCount = 3;
    lines = 3;
  }
  else if (allowedCount == 3)
  {
    items = items3;
    menuCount = 4;
    lines = 4;
  }

  if (interfaceRunStringList(ifText("SD Schreiben", "SD Write"), items, menuCount, current_selection, lines))
  {
    if (current_selection < allowedCount)
    {
      interface_cfg.sd_interval_index = (uint8_t)current_selection;
      interfaceClampSdWriteIntervalToOutput();
      interfaceConfigSave();
      sdLogResetSchedule();
    }

    frisch = true;
    interfaceReturnTo(MENU_SD_CARD);
  }
}

void FLASHMEM interface_sd_header_menu(void)
{
  interfaceToggleMenu(ifText("CSV Kopfzeile", "CSV header"),
                      interface_cfg.sd_header_enabled,
                      MENU_SD_CARD);
}

// =========================================================================
// USB MENUE
// =========================================================================

void FLASHMEM interface_usb_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  if (frisch)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    ifText("Modus", "Mode"),
    ifTextBack()
  };

  if (interfaceRunStringList(T(TXT_MENU_USB), items, 2, current_selection))
  {
    if (current_selection == 0)
    {
      menu_level = MENU_USB_MODE;
    }
    else
    {
      menu_level = MENU_INTERFACES;
      frisch = true;
    }

    flag.menu_lcd_upd = false;
  }
}

void FLASHMEM interface_usb_mode_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  interfaceConfigLoad();

  if (frisch)
  {
    current_selection = interface_cfg.usb_mode;
    if (current_selection < 0 || current_selection > 2) current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  const char* items[] = {
    ifUsbModeText(IF_USB_MODE_COMMANDS),
    ifUsbModeText(IF_USB_MODE_CSV),
    ifUsbModeText(IF_USB_MODE_DEBUG),
    ifTextBack()
  };

  if (interfaceRunStringList(ifText("USB Modus", "USB mode"), items, 4, current_selection, 3))
  {
    if (current_selection < 3)
    {
      interface_cfg.usb_mode = (uint8_t)current_selection;
      interfaceConfigSave();
    }

    frisch = true;
    interfaceReturnTo(MENU_USB);
  }
}
