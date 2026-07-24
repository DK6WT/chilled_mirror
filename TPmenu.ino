/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPmenu.ino
 * Zweck: Menue-Kern, Navigation und gemeinsame Setup-Infrastruktur.
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

#include <Arduino.h>
#include <RA8875.h>
#include <EEPROM.h>
#include <TimeLib.h>
#include <math.h>
#include "TP_T.h"
#include "TPlanguage.h"
#include "TPtft.h"
#include "EEPROMAnything.h"

#define SSD1306 0

// Externe Objekte aus deiner main.cpp
extern uint16_t menu_level;
extern var_t R;
extern uint8_t led_autoadaptation_mode;
extern char lcd_buf[];

// =========================================================================
// MENUE-SEITEN-REDRAW-EPOCH
// =========================================================================
// Einige Einstellseiten zeichnen feste Texte nur einmal und aktualisieren danach
// nur noch Wertefelder. Wenn das TFT per EXIT/Timeout geleert wurde, muessen
// diese Seiten beim naechsten Betreten trotzdem wieder komplett zeichnen.
static uint16_t menu_page_draw_epoch = 1;

uint16_t menuGetPageDrawEpoch(void)
{
  return menu_page_draw_epoch;
}

void menuInvalidatePageDrawCache(void)
{
  menu_page_draw_epoch++;
  if (menu_page_draw_epoch == 0) menu_page_draw_epoch = 1;
}

struct MenuPageDrawFlag
{
  bool drawn;
  uint16_t epoch;

  MenuPageDrawFlag() : drawn(false), epoch(0) {}
  MenuPageDrawFlag(bool v) : drawn(v), epoch(v ? menuGetPageDrawEpoch() : 0) {}

  operator bool() const
  {
    return drawn && epoch == menuGetPageDrawEpoch();
  }

  MenuPageDrawFlag& operator=(bool v)
  {
    drawn = v;
    epoch = v ? menuGetPageDrawEpoch() : 0;
    return *this;
  }
};

extern uint16_t Menu_exit_timer;
extern uint8_t mode_display;
extern RA8875 tft;

// Externe Touch-Hardware-Rohwerte
extern uint16_t TouchX, TouchY;
extern bool TouchZ;

extern TextBox* VirtLCDMenu;
extern TextBox* VirtLCDMessage;

extern bool touch_scroll_aktiv;
extern int FirstBut;
extern int LastBut;
extern bool refresh;

// Globale Steuervariable für den Kaltstart von außen
extern bool frisch_geoeffnet;

// Funktionen aus anderen Tabs
void ReadButtons(bool redraw);
bool menuExitRequested(void);
void menuExitClearRequest(void);
void menuButtonsResetAfterExit(void);
void eraseDisplay(void);
extern void schreibeEchtzeitUhr(void);
extern uint16_t fanPercentToPwm(uint8_t percent);
extern void fanApplyNormalSpeed(void);
extern void invalidateDiagnosisDisplay(void);
extern void invalidateAdcInfoDisplay(void);

// Externe 2-Punkt-Pt100-Kalibrierung aus PT100_2P_Calibration.ino
extern void pt100Cal2GetScaled(uint8_t sensor, int32_t* soll1, int32_t* ist1, int32_t* soll2, int32_t* ist2, bool* aktiv);
extern bool pt100Cal2SetScaled(uint8_t sensor, int32_t soll1, int32_t ist1, int32_t soll2, int32_t ist2);
extern void pt100Cal2Disable(uint8_t sensor);

// Externe Referenzwiderstands-Kalibrierung aus Ref100_120_Calibration.ino
extern void refCalGetScaled(int32_t* ref100, int32_t* ref120);
extern bool refCalSet100Scaled(int32_t ref100_scaled);
extern bool refCalSet120Scaled(int32_t ref120_scaled);
extern void extRefCalAbort(void);
extern void refCalGetAllScaled(uint32_t* date_yyyymmdd, int32_t* ref100, int32_t* ref120, int32_t* chA_corr100, int32_t* chA_corr120, int32_t* chB_corr100, int32_t* chB_corr120);
extern bool refCalSetAllScaled(uint32_t date_yyyymmdd, int32_t ref100_scaled, int32_t ref120_scaled, int32_t chA_corr100_scaled, int32_t chA_corr120_scaled, int32_t chB_corr100_scaled, int32_t chB_corr120_scaled);

// Externer Taupunkt-/Frostpunkt-Offset aus PT100_2P_Calibration.ino
extern int32_t taupunktOffsetGetScaled(void);
extern bool taupunktOffsetSetScaled(int32_t offset_mC);

// Externe Pt100-R0-Kalibrierung aus PT100_2P_Calibration.ino
extern int32_t pt100R0GetScaled(uint8_t sensor);
extern bool pt100R0SetScaled(uint8_t sensor, int32_t r0_scaled);

// Vorwärtsdeklarationen
void FLASHMEM menu_level0(void);
void FLASHMEM regler_parameter_auswahl_menu(void);
void FLASHMEM displaydim_menu(void);
void FLASHMEM rtc_stellen_menu(void);
void FLASHMEM pid_kp_menu(void);
void FLASHMEM pid_ki_menu(void);
void FLASHMEM pid_kd_menu(void);
void FLASHMEM regler_takt_menu(void);
void FLASHMEM peltier_current_limit_menu(void);
void FLASHMEM optik_sollwert_menu(void);
void FLASHMEM freiheiz_temp_menu(void);
void FLASHMEM autocal_interval_menu(void);
void FLASHMEM led_autoadaptation_mode_menu(void);
void FLASHMEM led_learning_reset_menu(void);
void FLASHMEM adc_filter_mode_menu(void);
void FLASHMEM adc1_sfocal_mode_menu(void);
void FLASHMEM fan_speed_menu(void);
void FLASHMEM sensorkopf_menu(void);
void FLASHMEM head_type_menu(void);
void FLASHMEM head_serial_menu(void);
void FLASHMEM head_cal_status_menu(void);
void FLASHMEM head_cal_load_type_menu(void);
void FLASHMEM head_cal_load_serial_menu(void);
void FLASHMEM head_cal_save_menu(void);
void FLASHMEM head_cal_save_password_menu(void);
void FLASHMEM head_cal_message_menu(void);
void FLASHMEM control_password_menu(void);
void FLASHMEM kalibrierung_haupt_menu(void);
void FLASHMEM fühler_wahl_menu(void);
void FLASHMEM fühler_eichung_menu(void);
void FLASHMEM pt100_2p_wahl_menu(void);
void FLASHMEM pt100_2punkt_menu(void);
void FLASHMEM referenz_wahl_menu(void);
void FLASHMEM referenz_eichung_menu(void);
void FLASHMEM referenz_werte_menu(void);
void FLASHMEM referenz_ext_auto_menu(void);
void FLASHMEM taupunkt_offset_menu(void);
void FLASHMEM factory_menu(void);
void FLASHMEM device_storage_menu(void);
void FLASHMEM device_storage_status_menu(void);
void FLASHMEM device_settings_load_menu(void);
void FLASHMEM device_factory_cal_load_menu(void);
void FLASHMEM device_settings_save_menu(void);
void FLASHMEM device_factory_cal_save_menu(void);

// Geraete-Speicher Backup-/Restore-Funktionen aus TPdeviceStorage.ino
bool FLASHMEM deviceSettingsBackupValid(void);
bool FLASHMEM deviceFactoryCalBackupValid(void);
uint32_t FLASHMEM deviceSettingsBackupDate(void);
uint32_t FLASHMEM deviceFactoryCalBackupDate(void);
bool FLASHMEM deviceSettingsBackupSave(void);
bool FLASHMEM deviceSettingsBackupLoad(void);
bool FLASHMEM deviceFactoryCalBackupSave(void);
bool FLASHMEM deviceFactoryCalBackupLoad(void);

// Kopfdaten SD-Funktionen aus TPheadCalibration.ino
uint8_t FLASHMEM headCalListTypes(char types[][HEAD_TYPE_TEXT_LEN + 1U], uint8_t maxCount);
uint8_t FLASHMEM headCalListSerials(const char* headTypeText, uint32_t* serials, uint8_t maxCount);
bool FLASHMEM headCalSave(const char* headTypeText, uint32_t headSerial, char* message, size_t messageSize);
bool FLASHMEM headCalLoadLatest(const char* headTypeText, uint32_t headSerial, char* message, size_t messageSize);
bool FLASHMEM headCalStatusText(char* line1, size_t line1Size, char* line2, size_t line2Size);

void FLASHMEM language_menu(void);
void FLASHMEM display_mode_menu(void);
void FLASHMEM display_main_layout_menu(void);
void FLASHMEM display_loop_debug_menu(void);
void FLASHMEM info_license_menu(void);
void FLASHMEM status_information_menu(void);
void FLASHMEM validity_information_menu(void);
void FLASHMEM calibration_certificate_qr_menu(void);
void FLASHMEM signed_calibration_menu(void);
void FLASHMEM signed_calibration_action_menu(void);
void FLASHMEM interfaces_menu(void);
void FLASHMEM interface_placeholder_menu(TextId title);
void FLASHMEM alarm_haupt_menu(void);
void FLASHMEM alarm_mode_menu(void);
void FLASHMEM alarm_tau_low_menu(void);
void FLASHMEM alarm_tau_high_menu(void);
void FLASHMEM alarm_rh_low_menu(void);
void FLASHMEM alarm_rh_high_menu(void);
void FLASHMEM alarm_buzzer_menu(void);

// Schnittstellen-Menüs aus TPmenu_Interface.ino
void interfaceConfigLoad(void);
void interfaceApplySerialBaud(void);
void interfaceConfigReloadFromEeprom(void);
void FLASHMEM interface_rs232_1_menu(void);
void FLASHMEM interface_rs232_2_menu(void);
void FLASHMEM interface_rs232_1_mode_menu(void);
void FLASHMEM interface_rs232_1_baud_menu(void);
void FLASHMEM interface_rs232_1_output_menu(void);
void FLASHMEM interface_rs232_1_interval_menu(void);
void FLASHMEM interface_rs232_2_mode_menu(void);
void FLASHMEM interface_rs232_2_baud_menu(void);
void FLASHMEM interface_rs232_2_output_menu(void);
void FLASHMEM interface_rs232_2_interval_menu(void);
void FLASHMEM interface_ethernet_menu(void);
void FLASHMEM interface_eth_active_menu(void);
void FLASHMEM interface_eth_dhcp_menu(void);
void FLASHMEM interface_eth_ip_menu(void);
void FLASHMEM interface_eth_subnet_menu(void);
void FLASHMEM interface_eth_gateway_menu(void);
void FLASHMEM interface_eth_dns_menu(void);
void FLASHMEM interface_eth_tcp_port_menu(void);
void FLASHMEM interface_eth_webserver_menu(void);
void FLASHMEM interface_eth_web_port_menu(void);
void FLASHMEM interface_eth_web_setup_menu(void);
void FLASHMEM interface_usb_menu(void);
void FLASHMEM interface_usb_mode_menu(void);
void FLASHMEM interface_sd_menu(void);
void FLASHMEM interface_sd_status_menu(void);
void FLASHMEM interface_sd_logging_menu(void);
void FLASHMEM interface_sd_output_interval_menu(void);
void FLASHMEM interface_sd_output_filter_menu(void);
void FLASHMEM interface_sd_interval_menu(void);
void FLASHMEM interface_sd_diagnostic_data_menu(void);
void FLASHMEM interface_sd_header_menu(void);
void FLASHMEM interface_sd_log_integrity_menu(void);
void FLASHMEM interface_output_interval_menu(void);
void FLASHMEM interface_output_filter_menu(void);
void FLASHMEM interface_diagnostic_data_menu(void);
void FLASHMEM interface_flow_display_menu(void);
void FLASHMEM interface_almemo_menu(void);
void FLASHMEM interface_almemo_ch1_menu(void);
void FLASHMEM interface_almemo_ch2_menu(void);
void FLASHMEM interface_almemo_active_menu(void);
void FLASHMEM interface_almemo_address_menu(void);
void FLASHMEM interface_almemo_channel_menu(void);
void FLASHMEM interface_almemo_role_menu(void);
void FLASHMEM interface_almemo_interval_menu(void);
void FLASHMEM interface_almemo_trace_menu(void);
void FLASHMEM interface_wincontrol_menu(void);
void FLASHMEM interface_wincontrol_output_menu(void);
void FLASHMEM interface_wincontrol_baud_menu(void);
void FLASHMEM interface_wincontrol_address_menu(void);
void FLASHMEM interface_wincontrol_tcp_port_menu(void);
void FLASHMEM interface_wincontrol_cycle_menu(void);
void FLASHMEM interface_wincontrol_status_menu(void);

// SD-Logging aus TPsdLog.ino
void sdLogBegin(void);
void sdLogTask(void);



// =========================================================================
// MENUE-LEVEL NAMEN
// =========================================================================
// menu_level bleibt uint16_t, aber im Code werden keine nackten Zahlen mehr
// benutzt. Das macht ConfigMenu() und die Ruecksprungziele lesbarer.
enum
{
  MENU_MAIN                 = 0,
  MENU_CONTROL_PARAMETERS   = 1,
  MENU_SENSOR_R0_SELECT     = 2,
  MENU_RTC_SET              = 3,
  MENU_FACTORY_RESET        = 4,

  MENU_DISPLAY_DIMMER       = 11,
  MENU_PID_KP               = 12,
  MENU_PID_KI               = 13,
  MENU_PID_KD               = 14,
  MENU_CONTROL_PERIOD       = 15,
  MENU_OPTIK_TARGET         = 17,
  MENU_FREIHEIZ_TEMP        = 18,
  MENU_AUTOCAL_INTERVAL     = 19,
  MENU_ADC_FILTER_MODE      = 34,
  MENU_ADC1_SFOCAL_MODE     = 35,
  MENU_FAN_SPEED            = 16,

  MENU_CALIBRATION          = 20,
  MENU_SENSOR_R0_MIRROR     = 21,
  MENU_SENSOR_R0_AMBIENT    = 22,
  MENU_PT100_2P_SELECT      = 23,
  MENU_PT100_2P_MIRROR      = 24,
  MENU_PT100_2P_AMBIENT     = 25,
  MENU_REFERENCES           = 26,
  MENU_REF_100R             = 27,
  MENU_REF_120R             = 28,
  MENU_TAUPOINT_OFFSET      = 29,
  MENU_REF_VALUES           = 32,
  MENU_REF_EXT_AUTO         = 33,

  MENU_SENSOR_HEAD          = 30,
  MENU_HEAD_SERIAL          = 31,

  MENU_LANGUAGE             = 40,
  MENU_DISPLAY_MODE         = 41,
  MENU_INFO_LICENSE         = 42,
  MENU_LOOP_DEBUG_DISPLAY   = 43,
  MENU_STATUS_INFO          = 44,
  MENU_MAIN_SCREEN_LAYOUT   = 45,
  MENU_VALIDITY_INFO        = 46,

  MENU_DEVICE_STORAGE       = 109,
  MENU_DEVICE_STORAGE_STATUS= 110,
  MENU_DEVICE_SETTINGS_LOAD = 111,
  MENU_FACTORY_CAL_LOAD     = 112,
  MENU_DEVICE_SETTINGS_SAVE = 113,
  MENU_FACTORY_CAL_SAVE     = 114,
  MENU_CONTROL_UNLOCK       = 115,
  MENU_PELTIER_CURRENT_LIMIT= 116,
  MENU_HEAD_TYPE_SELECT     = 117,
  MENU_HEAD_CAL_STATUS      = 118,
  MENU_HEAD_CAL_LOAD_TYPE   = 119,
  MENU_HEAD_CAL_LOAD_SERIAL = 120,
  MENU_HEAD_CAL_SAVE        = 121,
  MENU_HEAD_CAL_SAVE_PASSWORD = 122,
  MENU_HEAD_CAL_MESSAGE     = 123,

  MENU_ALARM                = 90,
  MENU_ALARM_MODE           = 91,
  MENU_ALARM_TAU_LOW        = 92,
  MENU_ALARM_TAU_HIGH       = 93,
  MENU_ALARM_RH_LOW         = 94,
  MENU_ALARM_RH_HIGH        = 95,
  MENU_ALARM_BUZZER         = 96,

  MENU_INTERFACES           = 50,
  MENU_RS232_1              = 51,
  MENU_RS232_2              = 52,
  MENU_ETHERNET             = 53,
  MENU_USB                  = 54,

  MENU_RS232_1_MODE         = 55,
  MENU_RS232_1_BAUD         = 56,
  MENU_RS232_2_MODE         = 57,
  MENU_RS232_2_BAUD         = 58,

  MENU_ETH_ACTIVE           = 59,
  MENU_ETH_DHCP             = 60,
  MENU_ETH_IP               = 61,
  MENU_ETH_SUBNET           = 62,
  MENU_ETH_GATEWAY          = 63,
  MENU_ETH_DNS              = 64,
  MENU_ETH_TCP_PORT         = 65,
  MENU_ETH_WEBSERVER        = 66,
  MENU_ETH_WEB_PORT         = 67,
  MENU_ETH_WEB_SETUP        = 68,

  MENU_USB_MODE             = 69,

  MENU_SD_CARD              = 70,
  MENU_SD_STATUS            = 71,
  MENU_SD_LOGGING           = 72,
  MENU_SD_INTERVAL          = 73,
  MENU_SD_HEADER            = 74,
  MENU_SD_OUTPUT_INTERVAL   = 83,
  MENU_SD_OUTPUT_FILTER     = 84,
  MENU_SD_DIAGNOSTIC_DATA   = 85,
  MENU_SD_LOG_INTEGRITY      = 124,
  MENU_LED_AUTOADAPTATION     = 125,
  MENU_LED_LEARNING_RESET     = 126,
  MENU_CAL_CERT_QR             = 127,

  MENU_SIGNED_CALIBRATION    = 130,
  MENU_SIGNED_CAL_DEVICE_REQ = 131,
  MENU_SIGNED_CAL_HEAD_REQ   = 132,
  MENU_SIGNED_CAL_DEVICE_IMP = 133,
  MENU_SIGNED_CAL_HEAD_IMP   = 134,

  MENU_RS232_1_OUTPUT       = 75,
  MENU_RS232_1_INTERVAL     = 76,
  MENU_RS232_2_OUTPUT       = 77,
  MENU_RS232_2_INTERVAL     = 78,
  MENU_FLOW_DISPLAY         = 79,
  MENU_OUTPUT_INTERVAL      = 80,
  MENU_OUTPUT_FILTER        = 81,
  MENU_DIAGNOSTIC_DATA      = 82,
  MENU_ALMEMO               = 86,
  MENU_ALMEMO_ADDRESS       = 87,
  MENU_ALMEMO_CHANNEL       = 88,
  MENU_ALMEMO_INTERVAL      = 89,
  MENU_ALMEMO_TRACE         = 97,
  MENU_ALMEMO_CH1           = 98,
  MENU_ALMEMO_CH2           = 99,
  MENU_ALMEMO_ACTIVE        = 100,
  MENU_ALMEMO_ROLE          = 101,

  MENU_WINCONTROL           = 102,
  MENU_WINCONTROL_OUTPUT    = 103,
  MENU_WINCONTROL_BAUD      = 104,
  MENU_WINCONTROL_ADDRESS   = 105,
  MENU_WINCONTROL_TCP_PORT  = 106,
  MENU_WINCONTROL_CYCLE     = 107,
  MENU_WINCONTROL_STATUS    = 108
};



// =========================================================================
// TP-3000 / STP-300x SENSORKOPF-PROFILE
// =========================================================================
typedef struct
{
  const char *modell;
  const char *beschreibung;
  uint16_t    kp;
  float       ki;
  float       kd;
  uint16_t    regler_ms;
  uint8_t     h_bruecke_ms;
  uint8_t     fan_percent;
  uint16_t    peltier_limit_ma;
} head_profile_t;

const head_profile_t head_profiles[HEAD_TYPE_COUNT] =
{
  { "STP-3001", "Grosser Kopf",      120, 1.5f, 5.0f, 250, 15, 75, 2500 },
  { "STP-3002", "Mittel-gross",      180, 2.5f, 4.0f, 180, 12, 70, 2000 },
  { "STP-3003", "Standard Kopf",     260, 4.0f, 3.0f, 120, 10, 65, 1500 },
  { "STP-3004", "Kleiner Kopf",      380, 7.0f, 2.0f,  62,  8, 55, 1000 }
};

const uint32_t head_serial_dummy[HEAD_TYPE_COUNT][4] =
{
  { 1,   2,   3,   4   },   // STP-3001
  { 101, 102, 103, 104 },   // STP-3002
  { 201, 202, 203, 204 },   // STP-3003
  { 301, 302, 303, 304 }    // STP-3004
};

const char* getHeadModelName(uint8_t head_type)
{
  if (head_type >= HEAD_TYPE_COUNT) head_type = HEAD_TYPE_DEFAULT;
  return head_profiles[head_type].modell;
}

const char* getHeadDescription(uint8_t head_type)
{
  if (head_type >= HEAD_TYPE_COUNT) head_type = HEAD_TYPE_DEFAULT;
  return head_profiles[head_type].beschreibung;
}

void applySensorHeadProfile(uint8_t head_type)
{
  if (head_type >= HEAD_TYPE_COUNT) head_type = HEAD_TYPE_DEFAULT;

  R.head_type = head_type;
  headTypeTextSet(head_profiles[head_type].modell);

  R.pid_kp              = head_profiles[head_type].kp;
  R.pid_ki              = head_profiles[head_type].ki;
  R.pid_kd              = head_profiles[head_type].kd;
  R.regler_intervall_ms = head_profiles[head_type].regler_ms;
  R.h_bruecke_totzeit   = head_profiles[head_type].h_bruecke_ms;
  R.fan_percent         = head_profiles[head_type].fan_percent;
  peltierCurrentLimitSetMa(head_profiles[head_type].peltier_limit_ma);

  fanApplyNormalSpeed();
}


// =========================================================================
// Geschuetzte Regelparameter-Bearbeitung
// =========================================================================
static bool control_params_unlocked = false;
static var_t control_params_backup;
static uint16_t control_params_backup_peltier_ma = PELTIER_CURRENT_LIMIT_DEFAULT_MA;
static uint8_t control_params_backup_led_mode = LED_AUTOADAPT_DEFAULT;

bool controlParamsEditUnlocked(void)
{
  return control_params_unlocked;
}

bool controlParamsDeferValueSave(uint16_t return_level, bool return_to_main)
{
  return control_params_unlocked && return_level == MENU_CONTROL_PARAMETERS && !return_to_main;
}

void controlParamsBeginEdit(void)
{
  control_params_backup = R;
  control_params_backup_peltier_ma = peltierCurrentLimitGetMa();
  control_params_backup_led_mode = led_autoadaptation_mode;
  control_params_unlocked = true;
}

void controlParamsSaveAndLock(void)
{
  tpMainConfigSave();
  control_params_backup = R;
  control_params_backup_peltier_ma = peltierCurrentLimitGetMa();
  control_params_backup_led_mode = led_autoadaptation_mode;
  control_params_unlocked = false;
}

void controlParamsDiscardAndLock(void)
{
  R = control_params_backup;
  peltierCurrentLimitSetMa(control_params_backup_peltier_ma);
  led_autoadaptation_mode = control_params_backup_led_mode;
  fanApplyNormalSpeed();
  control_params_unlocked = false;
}

void controlParamsLockOnly(void)
{
  control_params_backup = R;
  control_params_backup_peltier_ma = peltierCurrentLimitGetMa();
  control_params_backup_led_mode = led_autoadaptation_mode;
  control_params_unlocked = false;
}

// =========================================================================
// MENÜEINTRÄGE
// =========================================================================

const uint8_t level0_menu_size = 15;
const TextId level0_menu_items[] = {
  TXT_MENU_CONTROL_PARAMETERS,
  TXT_MENU_STATUS_INFO,
  TXT_SIGNED_CAL_STATUS,
  TXT_MENU_CAL_CERT_QR,
  TXT_MENU_ALARM,
  TXT_MENU_FAN_SPEED,
  TXT_MENU_SENSOR_HEAD,
  TXT_MENU_CALIBRATION,
  TXT_MENU_CLOCK_DATE,
  TXT_MENU_DISPLAY_DIMMER,
  TXT_MENU_DISPLAY_MODE,
  TXT_MENU_INTERFACES,
  TXT_MENU_DEVICE_STORAGE,
  TXT_MENU_LANGUAGE,
  // Platzhalter: Der eigentliche Text wird in lcd_scroll_MenuT()
  // als lokalisierten Eintrag ausgegeben. Dadurch kompiliert TPmenu.ino
  // auch dann sauber, wenn eine aeltere TPlanguage.h im Sketch-Ordner liegt.
  TXT_EMPTY
};

const uint16_t level0_menu_next[] = {
  MENU_CONTROL_PARAMETERS,   // Regelparameter
  MENU_STATUS_INFO,          // Status Information
  MENU_VALIDITY_INFO,         // Info / Gueltigkeit
  MENU_CAL_CERT_QR,            // Kalibrierschein-QR am TFT
  MENU_ALARM,                // Alarm
  MENU_FAN_SPEED,            // Fan-Speed
  MENU_SENSOR_HEAD,          // Sensorkopf
  MENU_CALIBRATION,          // Kalibrierung
  MENU_RTC_SET,              // Uhr/Datum
  MENU_DISPLAY_DIMMER,       // Display Dimmer
  MENU_DISPLAY_MODE,         // Anzeige / Diagnose
  MENU_INTERFACES,           // Schnittstellen
  MENU_DEVICE_STORAGE,       // Geraeteeinstellungen Laden/Speichern
  MENU_LANGUAGE,             // Sprache
  MENU_INFO_LICENSE          // Lizenzen / Marken
};


const uint8_t alarm_sub_menu_size = 7;
const TextId alarm_sub_menu_items[] = {
  TXT_ALARM_MODE,
  TXT_ALARM_TAU_LOW,
  TXT_ALARM_TAU_HIGH,
  TXT_ALARM_RH_LOW,
  TXT_ALARM_RH_HIGH,
  TXT_ALARM_BUZZER,
  TXT_BACK
};

const uint16_t alarm_sub_menu_next[] = {
  MENU_ALARM_MODE,
  MENU_ALARM_TAU_LOW,
  MENU_ALARM_TAU_HIGH,
  MENU_ALARM_RH_LOW,
  MENU_ALARM_RH_HIGH,
  MENU_ALARM_BUZZER,
  MENU_MAIN
};

const uint8_t interfaces_sub_menu_size = 12;
const TextId interfaces_sub_menu_items[] = {
  // Allgemeine Ausgabe gilt fuer RS232 / USB / Web / Ethernet.
  // SD hat im SD-Kartenmenue eigene Einstellungen.
  TXT_MENU_OUTPUT_INTERVAL,
  TXT_MENU_OUTPUT_FILTER,
  TXT_MENU_DIAGNOSTIC_DATA,
  TXT_MENU_RS232_1,
  TXT_MENU_RS232_2,
  TXT_MENU_ETHERNET,
  TXT_MENU_USB,
  TXT_MENU_SD_CARD,
  TXT_MENU_FLOW_DISPLAY,
  TXT_MENU_ALMEMO,
  TXT_MENU_WINCONTROL,
  TXT_BACK
};

const uint16_t interfaces_sub_menu_next[] = {
  MENU_OUTPUT_INTERVAL,
  MENU_OUTPUT_FILTER,
  MENU_DIAGNOSTIC_DATA,
  MENU_RS232_1,
  MENU_RS232_2,
  MENU_ETHERNET,
  MENU_USB,
  MENU_SD_CARD,
  MENU_FLOW_DISPLAY,
  MENU_ALMEMO,
  MENU_WINCONTROL,
  MENU_MAIN
};

const uint8_t regler_sub_menu_size = 13;
const TextId regler_sub_menu_items[] = {
  TXT_EMPTY,  // Peltier Maxstrom (dynamischer Klartext)
  TXT_MENU_PID_KP,
  TXT_MENU_PID_KI,
  TXT_MENU_PID_KD,
  TXT_MENU_CONTROL_PERIOD,
  TXT_MENU_OPTIK_TARGET,
  TXT_MENU_FREIHEIZ_TEMP,
  TXT_MENU_AUTOCAL_INTERVAL,
  TXT_EMPTY,  // LED-Autoadaption (dynamischer Klartext)
  TXT_EMPTY,  // LED-Lerndaten zuruecksetzen
  TXT_MENU_ADC_FILTER_MODE,
  TXT_MENU_ADC1_SFOCAL_MODE,
  TXT_BACK
};

const uint16_t regler_sub_menu_next[] = {
  MENU_PELTIER_CURRENT_LIMIT, // Peltier Maxstrom
  MENU_PID_KP,              // PID Kp
  MENU_PID_KI,              // PID Ki
  MENU_PID_KD,              // PID Kd
  MENU_CONTROL_PERIOD,      // Regelungs-Takt
  MENU_OPTIK_TARGET,        // Optik-Zielwert
  MENU_FREIHEIZ_TEMP,       // Freiheiztemperatur
  MENU_AUTOCAL_INTERVAL,    // Auto-Cal Intervall
  MENU_LED_AUTOADAPTATION,  // LED-Temperaturvorsteuerung
  MENU_LED_LEARNING_RESET,  // Lerndaten des aktuellen Kopfes loeschen
  MENU_ADC_FILTER_MODE,     // ADC/Pt100-Messfilter
  MENU_ADC1_SFOCAL_MODE,    // ADC1 SFOCAL
  MENU_MAIN                 // Zurueck
};


const uint8_t kalibrierung_menu_size = 7;
const TextId kalibrierung_menu_items[] = {
  TXT_MENU_SIGNED_CALIBRATION,
  TXT_MENU_PT100_R0,
  TXT_MENU_PT100_2POINT,
  TXT_MENU_REF_EXT_AUTO,    // automatischer externer 4R-Abgleich
  TXT_MENU_REF_VALUES,      // Ref-/Kanalwerte manuell editieren
  TXT_MENU_TAUPOINT_OFFSET,
  TXT_BACK
};

const uint16_t kalibrierung_menu_next[] = {
  MENU_SIGNED_CALIBRATION,
  MENU_SENSOR_R0_SELECT,    // Pt100 R0 Sensorwahl
  MENU_PT100_2P_SELECT,     // Pt100 2-Punkt Sensorwahl
  MENU_REF_EXT_AUTO,        // automatischer externer 4R-Abgleich
  MENU_REF_VALUES,          // Ref-/Kanalwerte manuell editieren
  MENU_TAUPOINT_OFFSET,     // Taupunkt-/Frostpunkt-Offset
  MENU_MAIN                 // Zurueck
};

const uint8_t signed_calibration_menu_size = 5;
const TextId signed_calibration_menu_items[] = {
  TXT_SIGNED_CAL_DEVICE_REQUEST,
  TXT_SIGNED_CAL_HEAD_REQUEST,
  TXT_SIGNED_CAL_DEVICE_IMPORT,
  TXT_SIGNED_CAL_HEAD_IMPORT,
  TXT_BACK
};

const uint16_t signed_calibration_menu_next[] = {
  MENU_SIGNED_CAL_DEVICE_REQ,
  MENU_SIGNED_CAL_HEAD_REQ,
  MENU_SIGNED_CAL_DEVICE_IMP,
  MENU_SIGNED_CAL_HEAD_IMP,
  MENU_CALIBRATION
};

const uint8_t pt100_2p_wahl_menu_size = 3;
const TextId pt100_2p_wahl_menu_items[] = {
  TXT_MENU_MIRROR_PT100_2P,
  TXT_MENU_AMBIENT_PT100_2P,
  TXT_BACK
};

const uint16_t pt100_2p_wahl_menu_next[] = {
  MENU_PT100_2P_MIRROR,     // Spiegel
  MENU_PT100_2P_AMBIENT,    // Umgebung
  MENU_CALIBRATION          // Zurueck Kalibrierung
};

const uint8_t referenz_wahl_menu_size = 3;
const TextId referenz_wahl_menu_items[] = {
  TXT_MENU_REF_EXT_AUTO,
  TXT_MENU_REF_VALUES,
  TXT_BACK
};

const uint16_t referenz_wahl_menu_next[] = {
  MENU_REF_EXT_AUTO,        // automatischer externer 4R-Abgleich
  MENU_REF_VALUES,          // Ref-/Kanalwerte manuell editieren
  MENU_CALIBRATION          // Zurueck Kalibrierung
};

const uint8_t language_menu_size = 3;
const TextId language_menu_items[] = {
  TXT_LANGUAGE_DE,
  TXT_LANGUAGE_EN,
  TXT_BACK
};

const uint8_t display_mode_menu_size = 7;
const TextId display_mode_menu_items[] = {
  TXT_DISPLAY_MODE_MAIN_SCREEN,
  TXT_DISPLAY_MAIN_LAYOUT_MENU,
  TXT_DISPLAY_MODE_DIAG_OVERVIEW,
  TXT_DISPLAY_MODE_DIAG_DETAILS,
  TXT_DISPLAY_MODE_ADC_INFO,
  TXT_DISPLAY_LOOP_DEBUG_MENU,
  TXT_BACK
};

const uint8_t sensor_select_menu_size = 3;
const TextId sensor_select_menu_items[] = {
  TXT_SENSOR_ITEM_MIRROR_PT100,
  TXT_SENSOR_ITEM_AMBIENT_PT100,
  TXT_BACK
};

const uint16_t sensor_select_menu_next[] = {
  MENU_SENSOR_R0_MIRROR,    // Spiegel Pt100 R0
  MENU_SENSOR_R0_AMBIENT,   // Umgebung Pt100 R0
  MENU_CALIBRATION          // Zurueck Kalibrierung
};

const uint8_t device_storage_menu_size = 6;
const TextId device_storage_menu_items[] = {
  TXT_DEVICE_STORAGE_STATUS,
  TXT_DEVICE_SETTINGS_LOAD,
  TXT_DEVICE_FACTORY_CAL_LOAD,
  TXT_DEVICE_SETTINGS_SAVE,
  TXT_DEVICE_FACTORY_CAL_SAVE,
  TXT_BACK
};

const uint16_t device_storage_menu_next[] = {
  MENU_DEVICE_STORAGE_STATUS,
  MENU_DEVICE_SETTINGS_LOAD,
  MENU_FACTORY_CAL_LOAD,
  MENU_DEVICE_SETTINGS_SAVE,
  MENU_FACTORY_CAL_SAVE,
  MENU_MAIN
};

const uint8_t factory_menu_size = 2;
const TextId factory_menu_items[] = {
  TXT_FACTORY_NO,     // sichere Vorgabe: Abbrechen zuerst/markiert
  TXT_FACTORY_YES     // Ausfuehren nur nach bewusstem Umschalten
};


// =========================================================================
// DYNAMISCHE SCROLL-ENGINE
// =========================================================================
void lcd_scroll_Menu(const char **menu,
                     uint8_t menu_size,
                     uint8_t current_choice,
                     uint8_t begin_row,
                     uint8_t begin_col,
                     uint8_t lines)
{
  (void)lines;

  uint8_t a;
  int16_t x;

  if (VirtLCDMenu == nullptr) return;

  // Dynamische Zeilenanzahl berechnen
  uint8_t dyn_lines = menu_size;

  if (dyn_lines > 5)
  {
    dyn_lines = 5;
  }
  else if (dyn_lines > 2 && (dyn_lines % 2 == 0))
  {
    dyn_lines = dyn_lines - 1;
  }

  // Textbereich leeren
  for (uint8_t i = 0; i < dyn_lines; i++)
  {
    VirtLCDMenu->setCursor(begin_col, begin_row + i);

    for (a = begin_col; a < 42; a++)
    {
      VirtLCDMenu->print(" ");
    }
  }

  uint8_t mid_line = (dyn_lines == 2) ? 0 : (dyn_lines / 2);

  for (uint8_t i = 0; i < dyn_lines; i++)
  {
    int8_t offset = i - mid_line;

    x = (int16_t)current_choice + offset;

    while (x < 0)
    {
      x += menu_size;
    }

    while (x >= menu_size)
    {
      x -= menu_size;
    }

    VirtLCDMenu->setCursor(begin_col, begin_row + i);

    if (i == mid_line)
    {
      VirtLCDMenu->print(" ->  ");
    }
    else
    {
      VirtLCDMenu->print("     ");
    }

    snprintf(lcd_buf, 255, "%s", menu[x]);
    VirtLCDMenu->print(lcd_buf);
  }
}

// Gleiche Optik wie lcd_scroll_Menu(), aber die Texte kommen aus der
// Sprach-Tabelle über TextId + T(...).
void lcd_scroll_MenuT(const TextId *menu,
                      uint8_t menu_size,
                      uint8_t current_choice,
                      uint8_t begin_row,
                      uint8_t begin_col,
                      uint8_t lines)
{
  (void)lines;

  uint8_t a;
  int16_t x;

  if (VirtLCDMenu == nullptr) return;

  uint8_t dyn_lines = menu_size;

  if (dyn_lines > 5)
  {
    dyn_lines = 5;
  }
  else if (dyn_lines > 2 && (dyn_lines % 2 == 0))
  {
    dyn_lines = dyn_lines - 1;
  }

  for (uint8_t i = 0; i < dyn_lines; i++)
  {
    VirtLCDMenu->setCursor(begin_col, begin_row + i);

    for (a = begin_col; a < 42; a++)
    {
      VirtLCDMenu->print(" ");
    }
  }

  uint8_t mid_line = (dyn_lines == 2) ? 0 : (dyn_lines / 2);

  for (uint8_t i = 0; i < dyn_lines; i++)
  {
    int8_t offset = i - mid_line;

    x = (int16_t)current_choice + offset;

    while (x < 0)
    {
      x += menu_size;
    }

    while (x >= menu_size)
    {
      x -= menu_size;
    }

    VirtLCDMenu->setCursor(begin_col, begin_row + i);

    if (i == mid_line)
    {
      VirtLCDMenu->print(" ->  ");
    }
    else
    {
      VirtLCDMenu->print("     ");
    }

    if (menu[x] == TXT_EMPTY)
    {
      snprintf(lcd_buf, 255, "%s", T(TXT_MENU_INFO_LICENSE));
    }
    else
    {
      snprintf(lcd_buf, 255, "%s", T(menu[x]));
    }

    VirtLCDMenu->print(lcd_buf);
  }
}


// =========================================================================
// KLEINES MENUE-FRAMEWORK FUER TEXTID-LISTEN
// =========================================================================
// Ziel: Optik unveraendert lassen, aber Touch-/Scroll-/Zeichenlogik nur noch
// an einer Stelle pflegen. Die bestehenden TextBoxen, Fonts, Farben und
// Buttonpositionen bleiben dadurch unangetastet.

// Arduino IDE erzeugt fuer .ino-Dateien automatisch Funktions-Prototypen.
// Eigene Rueckgabetypen wie "MenuKey" koennen dabei vor ihrer Definition
// auftauchen. Darum verwenden die Framework-Hilfsfunktionen hier bewusst
// uint8_t statt eines eigenen Enum-Typs.
#define MK_NONE  0
#define MK_UP    1
#define MK_DOWN  2
#define MK_ENTER 3

static uint8_t menuReadKey(uint16_t debounce_ms)
{
  extern uint32_t menu_global_debounce;

  // ENTER wird in ReadButtons() bereits als echte Touch-Flanke erkannt:
  // flag.short_push wird nur beim ersten Druck gesetzt und erst nach dem
  // Loslassen kann eine neue ENTER-Flanke entstehen. Dadurch kann ein noch
  // aufliegender Finger nach einem Seitenwechsel nicht ein zweites ENTER
  // im neuen Menue ausloesen.
  if (!TouchZ)
  {
    // Eine zu kurze Beruehrung innerhalb der Debounce-Zeit darf nicht als
    // spaeteres, verzoegertes ENTER im naechsten Touch weiterleben.
    flag.short_push = false;
    return MK_NONE;
  }

  if (millis() - menu_global_debounce <= debounce_ms) return MK_NONE;

  uint8_t key = MK_NONE;

  if (flag.short_push)
  {
    flag.short_push = false;
    key = MK_ENTER;
  }
  else if (TouchX > 20 && TouchX < 130 && TouchY > 25 && TouchY < 145)
  {
    key = MK_UP;
  }
  else if (TouchX > 20 && TouchX < 130 && TouchY > 187 && TouchY < 307)
  {
    key = MK_DOWN;
  }

  if (key != MK_NONE)
  {
    menu_global_debounce = millis();
  }

  return key;
}

static bool menuListHandleInput(int8_t& current_selection,
                                uint8_t menu_size,
                                uint16_t debounce_ms = 250)
{
  if (menu_size == 0) return false;

  ReadButtons(false);

  uint8_t key = menuReadKey(debounce_ms);

  if (key == MK_UP)
  {
    current_selection--;

    if (current_selection < 0)
    {
      current_selection = menu_size - 1;
    }

    flag.menu_lcd_upd = false;
  }
  else if (key == MK_DOWN)
  {
    current_selection++;

    if (current_selection >= menu_size)
    {
      current_selection = 0;
    }

    flag.menu_lcd_upd = false;
  }
  else if (key == MK_ENTER)
  {
    flag.short_push = false;
    return true;
  }

  return false;
}

static void menuListDrawIfNeeded(TextId title,
                                 const TextId* items,
                                 uint8_t menu_size,
                                 int8_t current_selection,
                                 uint8_t lines = 3,
                                 bool clear_message = false,
                                 bool do_transfer = true)
{
  if (VirtLCDMenu == nullptr) return;
  if (flag.menu_lcd_upd) return;

  flag.menu_lcd_upd = true;

  VirtLCDMenu->clear();

  if (clear_message && VirtLCDMessage != nullptr)
  {
    VirtLCDMessage->clear();
  }

  VirtLCDMenu->setCursor(1, 1);
  VirtLCDMenu->print(T(title));

  lcd_scroll_MenuT(items, menu_size, current_selection, 4, 1, lines);

  if (do_transfer)
  {
    VirtLCDMenu->transfer();
  }

  ReadButtons(true);
}

static bool menuRunTextList(TextId title,
                            const TextId* items,
                            uint8_t menu_size,
                            int8_t& current_selection,
                            uint8_t lines = 3,
                            bool clear_message = false,
                            bool do_transfer = true,
                            uint16_t debounce_ms = 250)
{
  bool enter_pressed = menuListHandleInput(current_selection, menu_size, debounce_ms);

  menuListDrawIfNeeded(title,
                       items,
                       menu_size,
                       current_selection,
                       lines,
                       clear_message,
                       do_transfer);

  return enter_pressed;
}



// =========================================================================
// AUFGETEILTE MENUE-DATEIEN - GEMEINSAME HILFSFUNKTIONEN
// =========================================================================
// Die eigentlichen Definitionen stehen in TPmenu_ValueFields.ino.
// Die Prototypen bleiben hier, damit alle ausgelagerten Menue-Seiten sie
// unabhaengig von der Arduino-Tab-Reihenfolge sauber verwenden koennen.
#ifndef MDVA_NONE
#define MDVA_NONE       0
#define MDVA_CHANGED    1
#define MDVA_NEXT_DIGIT 2
#define MDVA_DONE       3
#endif

static void menuValueConsumeTouch(void);
static void menuValueSaveAndReturn(uint16_t return_level, bool return_to_main);
static void menuValueBeginDraw(TextId title, bool clear_message);
static void menuValueEndDraw(void);

static void menuDirectResetValueCache(void);
static void menuDirectWriteFixedRow(uint8_t slot,
                                    uint8_t col,
                                    uint8_t row,
                                    const char* text,
                                    uint8_t width,
                                    uint16_t color = WHITE);
static void menuDirectClearFixedRow(uint8_t slot,
                                    uint8_t col,
                                    uint8_t row,
                                    uint8_t width);
static void menuDirectClearAllRows(void);
static void menuDirectClearFanRows(void);

static bool menuValueHandleUInt8(uint8_t& value,
                                 uint8_t min_value,
                                 uint8_t max_value,
                                 uint8_t step,
                                 void (*on_change)(void),
                                 uint16_t debounce_ms);
static bool menuValueHandleUInt16(uint16_t& value,
                                  uint16_t min_value,
                                  uint16_t max_value,
                                  uint16_t step,
                                  void (*on_change)(void),
                                  uint16_t debounce_ms);
static bool menuValueHandleFloat(float& value,
                                 float min_value,
                                 float max_value,
                                 float step,
                                 void (*on_change)(void),
                                 uint16_t debounce_ms);

static uint8_t menuDigitValueHandle(int32_t& value,
                                    uint8_t& digit_index,
                                    uint8_t digit_count,
                                    const int32_t* digit_steps,
                                    int32_t min_value,
                                    int32_t max_value,
                                    uint16_t debounce_ms);
static void menuDigitResetTouchDebounce(void);
static void menuDigitBeginDraw(TextId title);
static void menuDigitEndDraw(void);
static void menuPrintTextIdForIndex(uint8_t index,
                                    const TextId* text_ids,
                                    uint8_t count);

// =========================================================================
// MENUE-INAKTIVITAETS-TIMEOUT
// =========================================================================
// Wenn das TFT-Setup offen bleibt und 120 s keine Bedienung erfolgt, wird
// automatisch zum Hauptbildschirm zurueckgekehrt. Nicht gespeicherte Aenderungen
// werden verworfen, indem die EEPROM-Staende erneut geladen werden.
#define MENU_IDLE_TIMEOUT_MS 120000UL

static uint32_t menu_last_activity_ms = 0;
static bool     menu_timeout_was_active = false;

static uint8_t menuTimeoutClampBacklight(uint8_t level)
{
  if (level > 10) level = 10;
  return level;
}

static uint8_t menuTimeoutBacklightToBrightness(uint8_t level)
{
  level = menuTimeoutClampBacklight(level);
  return (uint8_t)map(level, 0, 10, 5, 230);
}

void menuTimeoutResetActivity(void)
{
  menu_last_activity_ms = millis();
  menu_timeout_was_active = false;
}

void menuTimeoutNotifyTouch(bool touched)
{
  if (touched)
  {
    menuTimeoutResetActivity();
  }
}

static void menuTimeoutRevertUnsavedSettings(void)
{
  // Haupt-Konfiguration (PID, Display, Fan, Regler, Sensorkopf usw.)
  tpMainConfigLoad();

  if (control_params_unlocked)
  {
    peltierCurrentLimitSetMa(control_params_backup_peltier_ma);
    control_params_unlocked = false;
  }

  R.display_konfig.tft_backlight =
    menuTimeoutClampBacklight(R.display_konfig.tft_backlight);
  tft.brightness(menuTimeoutBacklightToBrightness(R.display_konfig.tft_backlight));

  R.fan_percent = constrain(R.fan_percent, 50, 100);
  fanApplyNormalSpeed();

  // Schnittstellen-Konfiguration auf gespeicherten Stand zuruecksetzen.
  interfaceConfigReloadFromEeprom();
  interfaceApplySerialBaud();
}

static void menuTimeoutLeaveToMain(void)
{
  extern int FirstBut;
  extern int LastBut;
  extern bool refresh;

  menuDirectClearAllRows();
  menuTimeoutRevertUnsavedSettings();

  TouchZ = false;
  TouchX = 0;
  TouchY = 0;
  touch_scroll_aktiv = false;
  refresh = true;

  menu_level = MENU_MAIN;
  current_selection = 0;
  flag.short_push = false;
  menuInvalidatePageDrawCache();
  flag.menu_lcd_upd = true;
  flag.config_mode = false;
  flag.mode_change = true;
  frisch_geoeffnet = true;

  FirstBut = -1;
  LastBut = -1;

  eraseDisplay();

  menu_timeout_was_active = true;
  menu_last_activity_ms = millis();
}

void menuTimeoutTask(void)
{
  if (!flag.config_mode)
  {
    menu_timeout_was_active = false;
    return;
  }

  if (menu_last_activity_ms == 0)
  {
    menuTimeoutResetActivity();
    return;
  }

  if (!menu_timeout_was_active &&
      (uint32_t)(millis() - menu_last_activity_ms) >= MENU_IDLE_TIMEOUT_MS)
  {
    menuTimeoutLeaveToMain();
  }
}


// =========================================================================
// CONFIGMENU / STATE-MACHINE
// =========================================================================
void ConfigMenu(void)
{
  switch (menu_level)
  {
    // Hauptmenue
    case MENU_MAIN:
      menu_level0();
      break;

    // Regelparameter
    case MENU_CONTROL_PARAMETERS:
      regler_parameter_auswahl_menu();
      break;

    case MENU_CONTROL_UNLOCK:
      control_password_menu();
      break;

    case MENU_ALARM:
      alarm_haupt_menu();
      break;

    case MENU_ALARM_MODE:
      alarm_mode_menu();
      break;

    case MENU_ALARM_TAU_LOW:
      alarm_tau_low_menu();
      break;

    case MENU_ALARM_TAU_HIGH:
      alarm_tau_high_menu();
      break;

    case MENU_ALARM_RH_LOW:
      alarm_rh_low_menu();
      break;

    case MENU_ALARM_RH_HIGH:
      alarm_rh_high_menu();
      break;

    case MENU_ALARM_BUZZER:
      alarm_buzzer_menu();
      break;

    case MENU_PID_KP:
      pid_kp_menu();
      break;

    case MENU_PID_KI:
      pid_ki_menu();
      break;

    case MENU_PID_KD:
      pid_kd_menu();
      break;

    case MENU_PELTIER_CURRENT_LIMIT:
      peltier_current_limit_menu();
      break;

    case MENU_CONTROL_PERIOD:
      regler_takt_menu();
      break;

    case MENU_OPTIK_TARGET:
      optik_sollwert_menu();
      break;

    case MENU_FREIHEIZ_TEMP:
      freiheiz_temp_menu();
      break;

    case MENU_AUTOCAL_INTERVAL:
      autocal_interval_menu();
      break;

    case MENU_LED_AUTOADAPTATION:
      led_autoadaptation_mode_menu();
      break;

    case MENU_LED_LEARNING_RESET:
      led_learning_reset_menu();
      break;

    case MENU_ADC_FILTER_MODE:
      adc_filter_mode_menu();
      break;

    case MENU_ADC1_SFOCAL_MODE:
      adc1_sfocal_mode_menu();
      break;

    // Geraete-/Anzeige-Einstellungen
    case MENU_FAN_SPEED:
      fan_speed_menu();
      break;

    case MENU_SENSOR_HEAD:
      sensorkopf_menu();
      break;

    case MENU_HEAD_TYPE_SELECT:
      head_type_menu();
      break;

    case MENU_HEAD_SERIAL:
      head_serial_menu();
      break;

    case MENU_HEAD_CAL_STATUS:
      head_cal_status_menu();
      break;

    case MENU_HEAD_CAL_LOAD_TYPE:
      head_cal_load_type_menu();
      break;

    case MENU_HEAD_CAL_LOAD_SERIAL:
      head_cal_load_serial_menu();
      break;

    case MENU_HEAD_CAL_SAVE:
      head_cal_save_menu();
      break;

    case MENU_HEAD_CAL_SAVE_PASSWORD:
      head_cal_save_password_menu();
      break;

    case MENU_HEAD_CAL_MESSAGE:
      head_cal_message_menu();
      break;

    case MENU_RTC_SET:
      rtc_stellen_menu();
      break;

    case MENU_DISPLAY_DIMMER:
      displaydim_menu();
      break;

    case MENU_DISPLAY_MODE:
      display_mode_menu();
      break;

    case MENU_LOOP_DEBUG_DISPLAY:
      display_loop_debug_menu();
      break;

    case MENU_MAIN_SCREEN_LAYOUT:
      display_main_layout_menu();
      break;

    case MENU_DEVICE_STORAGE:
      device_storage_menu();
      break;

    case MENU_DEVICE_STORAGE_STATUS:
      device_storage_status_menu();
      break;

    case MENU_DEVICE_SETTINGS_LOAD:
      device_settings_load_menu();
      break;

    case MENU_FACTORY_CAL_LOAD:
      device_factory_cal_load_menu();
      break;

    case MENU_DEVICE_SETTINGS_SAVE:
      device_settings_save_menu();
      break;

    case MENU_FACTORY_CAL_SAVE:
      device_factory_cal_save_menu();
      break;

    case MENU_FACTORY_RESET:
      factory_menu();
      break;

    case MENU_LANGUAGE:
      language_menu();
      break;

    case MENU_INFO_LICENSE:
      info_license_menu();
      break;

    case MENU_STATUS_INFO:
      status_information_menu();
      break;

    case MENU_VALIDITY_INFO:
      validity_information_menu();
      break;

    case MENU_CAL_CERT_QR:
      calibration_certificate_qr_menu();
      break;

    // Schnittstellen
    case MENU_INTERFACES:
      interfaces_menu();
      break;

    case MENU_RS232_1:
      interface_rs232_1_menu();
      break;

    case MENU_RS232_1_MODE:
      interface_rs232_1_mode_menu();
      break;

    case MENU_RS232_1_BAUD:
      interface_rs232_1_baud_menu();
      break;

    case MENU_RS232_1_OUTPUT:
      interface_rs232_1_output_menu();
      break;

    case MENU_RS232_1_INTERVAL:
      interface_rs232_1_interval_menu();
      break;

    case MENU_RS232_2:
      interface_rs232_2_menu();
      break;

    case MENU_RS232_2_MODE:
      interface_rs232_2_mode_menu();
      break;

    case MENU_RS232_2_BAUD:
      interface_rs232_2_baud_menu();
      break;

    case MENU_RS232_2_OUTPUT:
      interface_rs232_2_output_menu();
      break;

    case MENU_RS232_2_INTERVAL:
      interface_rs232_2_interval_menu();
      break;

    case MENU_FLOW_DISPLAY:
      interface_flow_display_menu();
      break;

    case MENU_ALMEMO:
      interface_almemo_menu();
      break;

    case MENU_ALMEMO_CH1:
      interface_almemo_ch1_menu();
      break;

    case MENU_ALMEMO_CH2:
      interface_almemo_ch2_menu();
      break;

    case MENU_ALMEMO_ACTIVE:
      interface_almemo_active_menu();
      break;

    case MENU_ALMEMO_ADDRESS:
      interface_almemo_address_menu();
      break;

    case MENU_ALMEMO_CHANNEL:
      interface_almemo_channel_menu();
      break;

    case MENU_ALMEMO_ROLE:
      interface_almemo_role_menu();
      break;

    case MENU_ALMEMO_INTERVAL:
      interface_almemo_interval_menu();
      break;

    case MENU_ALMEMO_TRACE:
      interface_almemo_trace_menu();
      break;

    case MENU_WINCONTROL:
      interface_wincontrol_menu();
      break;

    case MENU_WINCONTROL_OUTPUT:
      interface_wincontrol_output_menu();
      break;

    case MENU_WINCONTROL_BAUD:
      interface_wincontrol_baud_menu();
      break;

    case MENU_WINCONTROL_ADDRESS:
      interface_wincontrol_address_menu();
      break;

    case MENU_WINCONTROL_TCP_PORT:
      interface_wincontrol_tcp_port_menu();
      break;

    case MENU_WINCONTROL_CYCLE:
      interface_wincontrol_cycle_menu();
      break;

    case MENU_WINCONTROL_STATUS:
      interface_wincontrol_status_menu();
      break;

    case MENU_OUTPUT_INTERVAL:
      interface_output_interval_menu();
      break;

    case MENU_OUTPUT_FILTER:
      interface_output_filter_menu();
      break;

    case MENU_DIAGNOSTIC_DATA:
      interface_diagnostic_data_menu();
      break;

    case MENU_ETHERNET:
      interface_ethernet_menu();
      break;

    case MENU_ETH_ACTIVE:
      interface_eth_active_menu();
      break;

    case MENU_ETH_DHCP:
      interface_eth_dhcp_menu();
      break;

    case MENU_ETH_IP:
      interface_eth_ip_menu();
      break;

    case MENU_ETH_SUBNET:
      interface_eth_subnet_menu();
      break;

    case MENU_ETH_GATEWAY:
      interface_eth_gateway_menu();
      break;

    case MENU_ETH_DNS:
      interface_eth_dns_menu();
      break;

    case MENU_ETH_TCP_PORT:
      interface_eth_tcp_port_menu();
      break;

    case MENU_ETH_WEBSERVER:
      interface_eth_webserver_menu();
      break;

    case MENU_ETH_WEB_PORT:
      interface_eth_web_port_menu();
      break;

    case MENU_ETH_WEB_SETUP:
      interface_eth_web_setup_menu();
      break;

    case MENU_USB:
      interface_usb_menu();
      break;

    case MENU_USB_MODE:
      interface_usb_mode_menu();
      break;

    case MENU_SD_CARD:
      interface_sd_menu();
      break;

    case MENU_SD_STATUS:
      interface_sd_status_menu();
      break;

    case MENU_SD_LOGGING:
      interface_sd_logging_menu();
      break;

    case MENU_SD_OUTPUT_INTERVAL:
      interface_sd_output_interval_menu();
      break;

    case MENU_SD_OUTPUT_FILTER:
      interface_sd_output_filter_menu();
      break;

    case MENU_SD_INTERVAL:
      interface_sd_interval_menu();
      break;

    case MENU_SD_DIAGNOSTIC_DATA:
      interface_sd_diagnostic_data_menu();
      break;

    case MENU_SD_HEADER:
      interface_sd_header_menu();
      break;

    case MENU_SD_LOG_INTEGRITY:
      interface_sd_log_integrity_menu();
      break;

    // Kalibrierung
    case MENU_CALIBRATION:
      kalibrierung_haupt_menu();
      break;

    case MENU_SIGNED_CALIBRATION:
      signed_calibration_menu();
      break;

    case MENU_SIGNED_CAL_DEVICE_REQ:
    case MENU_SIGNED_CAL_HEAD_REQ:
    case MENU_SIGNED_CAL_DEVICE_IMP:
    case MENU_SIGNED_CAL_HEAD_IMP:
      signed_calibration_action_menu();
      break;

    case MENU_SENSOR_R0_SELECT:
      fühler_wahl_menu();
      break;

    case MENU_SENSOR_R0_MIRROR:
    case MENU_SENSOR_R0_AMBIENT:
      fühler_eichung_menu();
      break;

    case MENU_PT100_2P_SELECT:
      pt100_2p_wahl_menu();
      break;

    case MENU_PT100_2P_MIRROR:
    case MENU_PT100_2P_AMBIENT:
      pt100_2punkt_menu();
      break;

    case MENU_REFERENCES:
      referenz_wahl_menu();
      break;

    case MENU_REF_100R:
    case MENU_REF_120R:
    case MENU_REF_VALUES:
      referenz_werte_menu();
      break;

    case MENU_REF_EXT_AUTO:
      referenz_ext_auto_menu();
      break;

    case MENU_TAUPOINT_OFFSET:
      taupunkt_offset_menu();
      break;

    default:
      menu_level = MENU_MAIN;
      flag.menu_lcd_upd = false;
      break;
  }


  // EXIT wird in ReadButtons() nur angefordert. Erst hier, nachdem die
  // aktuelle Menuefunktion komplett zurueckgekehrt ist, wird wirklich
  // auf den Hauptscreen umgeschaltet. Dadurch kann kein Seiten-Code nach
  // fillScreen(BLACK) noch alte Menue-Texte in den Hauptscreen zeichnen.
  if (menuExitRequested())
  {
    extRefCalAbort();
    if (controlParamsEditUnlocked())
    {
      controlParamsDiscardAndLock();
    }
    menuExitClearRequest();

    // Button-Zustaende und den alten FAN-Button noch im Setup-Kontext
    // bereinigen. Danach loescht eraseDisplay() den ganzen Bildschirm und
    // setzt die Hauptscreen-Caches als letzten Schritt definiert zurueck.
    menuButtonsResetAfterExit();

    // Nicht nur den realen TFT schwarz loeschen: eraseDisplay() setzt auch
    // alle Hauptscreen-TextBoxen und direkten Sichtcaches zurueck. Ein blosses
    // fillScreen(BLACK) liess die Caches auf "bereits gezeichnet" stehen; beim
    // folgenden Hauptscreen wurden deshalb Rahmen, Loop-Anzeige und statische
    // Zeichen teilweise nicht neu aufgebaut und es blieb ein grosses schwarzes
    // Rechteck aus der Setup-Flaeche sichtbar.
    eraseDisplay();

    if (VirtLCDMenu != nullptr) VirtLCDMenu->invalidate();
    if (VirtLCDMessage != nullptr) VirtLCDMessage->invalidate();

    menuInvalidatePageDrawCache();

    // Wenn Setup direkt aus DIAG 1/2 oder DIAG 2/2 verlassen wird,
    // ist der echte TFT-Bildinhalt durch den EXIT-Clear leer.
    // Die Diagnose-Seite darf dann nicht nur ihre geaenderten Werte
    // aktualisieren, sondern muss Rahmen + Labels einmal komplett neu zeichnen.
    if (mode_display == 1 || mode_display == 2)
    {
      invalidateDiagnosisDisplay();
    }
    else if (mode_display == 3)
    {
      invalidateAdcInfoDisplay();
    }

    menu_level = MENU_MAIN;
    flag.config_mode = false;
    flag.mode_change = true;
    flag.menu_lcd_upd = false;
    frisch_geoeffnet = true;
    touch_scroll_aktiv = false;
    flag.short_push = false;

    return;
  }

  if (flag.config_mode == true && VirtLCDMenu != nullptr)
  {
    VirtLCDMenu->transfer();
  }
}
