/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TP_T.h
 * Zweck: Globale Datenstrukturen, Konfigurationen und EEPROM-kompatible Parameter.
 *
 * Abgeleitet aus: PSWR_T.h
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2015 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#ifndef TP_T_H
#define TP_T_H

#include <Arduino.h>

// =============================================================================
// 1. MESS- UND KALIBRIERUNGS-STRUKTUREN
// =============================================================================
typedef struct {
  int16_t  db10m;                     // Reserved: alter Kalibrierplatz, nicht aktiv genutzt
  double   Fwd;                       // Reserved/deprecated: alter R0-Platzhalter; R0 liegt separat im EEPROM
  double   Rev;                       // Reserved: alter Kalibrierplatz, nicht aktiv genutzt
} cal_t;

typedef struct {
  bool     tft_on;                    // Status Displayverbindung
  uint8_t  tft_backlight;             // Helligkeitsstufe (0 bis 10)
} disp_t;

// =============================================================================
// 2. TP-3000 / STP-300x SENSOR-KOPF-TYPEN
// =============================================================================

#define HEAD_TYPE_STP3001   0         // STP-3001 = grosser Kopf
#define HEAD_TYPE_STP3002   1         // STP-3002 = mittel-grosser Kopf
#define HEAD_TYPE_STP3003   2         // STP-3003 = Standard / mittlerer Kopf
#define HEAD_TYPE_STP3004   3         // STP-3004 = kleiner Kopf

#define HEAD_TYPE_COUNT     4
#define HEAD_TYPE_DEFAULT   HEAD_TYPE_STP3001

#define HEAD_TYPE_TEXT_LEN  8U        // "STP-" + genau 4 Ziffern
#define HEAD_TYPE_DEFAULT_TEXT "STP-3001"
#define HEAD_TYPE_NUMBER_MIN 0U
#define HEAD_TYPE_NUMBER_MAX 9999U

#define HEAD_SERIAL_MIN     0UL       // 00000 = Werksprofil / keine reale Kopf-SN gewaehlt
#define HEAD_SERIAL_MAX     99999UL
#define HEAD_SERIAL_DEFAULT 0UL

// LED-Autoadaption: 0=Aus, 1=System-/Hersteller-Grundkurve, 2=Grundkurve + Lerndaten.
#define LED_AUTOADAPT_OFF       0U
#define LED_AUTOADAPT_BASE      1U
#define LED_AUTOADAPT_SELF      2U
#define LED_AUTOADAPT_MAX       LED_AUTOADAPT_SELF
#define LED_AUTOADAPT_DEFAULT   LED_AUTOADAPT_BASE

#define DEVICE_SERIAL_DIGITS  5U
#define DEVICE_SERIAL_DEFAULT "00000"

bool deviceSerialNormalize(char* text, size_t textSize);
bool deviceSerialSet(const char* text);
const char* deviceSerialGet(void);

// Kryptografische Geraeteidentitaet / Test-Provisionierung ab V0.50.1.
void deviceIdentityBegin(void);
bool deviceIdentityHasKey(void);
bool deviceIdentityCertificateValid(void);
bool deviceIdentityCertificateStoredInvalid(void);
const char* deviceIdentityCertifiedSerial(void);
const char* deviceIdentityDeviceKeyId(void);
const char* deviceIdentityRootKeyId(void);
const char* deviceIdentityLastCertificateFile(void);
const char* deviceIdentityStatusText(void);
bool deviceIdentityGenerateKey(const char* confirmedSerial,
                               char* errorText, size_t errorTextSize);
bool deviceIdentityBuildRequestJson(char* output, size_t outputSize,
                                    size_t* outputLength,
                                    char* downloadName, size_t downloadNameSize,
                                    char* errorText, size_t errorTextSize);
bool deviceIdentitySaveRequestToSd(const char* json, size_t jsonLength,
                                   const char* downloadName,
                                   char* storedPath, size_t storedPathSize,
                                   char* errorText, size_t errorTextSize);
bool deviceIdentityImportCertificateFromSd(char* importedPath,
                                            size_t importedPathSize,
                                            char* errorText,
                                            size_t errorTextSize);
// Hilfsfunktionen fuer signierte Kalibrier- und Logdaten.
bool deviceIdentitySignHash(const uint8_t hash[32], uint8_t signature[64]);
bool deviceIdentityRandomBytes(uint8_t* output, size_t outputLength);
bool deviceIdentityVerifyRootSignedHash(const uint8_t hash[32],
                                        const uint8_t signature[64]);
bool deviceIdentityVerifyDeviceSignedHash(const uint8_t hash[32],
                                          const uint8_t signature[64]);
const char* deviceIdentityCertificateSerialText(void);
int64_t deviceIdentityCertificateIssuedUtc(void);
bool deviceIdentityCertificateManifestHash(uint8_t output[32]);
bool deviceIdentityBuildCertificateJson(char* output,
                                        size_t outputSize,
                                        size_t* outputLength);

// Bereinigte EEPROM-Hauptkonfiguration ab V0.50.0_18.
bool tpMainConfigLoad(void);
void tpMainConfigSave(void);
void tpMainConfigFactoryReset(void);
void tpDeviceUiConfigLoad(void);
void tpDeviceUiConfigSave(void);

// Die Geräte-RTC bleibt absichtlich in lokaler Bedienzeit. Für kryptografische
// Zeitstempel wird der beim PC-Zeitsync übertragene UTC-Offset separat
// gespeichert und von der lokalen RTC-Zeit abgezogen.
bool tpUtcOffsetValid(void);
int16_t tpUtcOffsetMinutesGet(void);
bool tpUtcOffsetSetMinutes(int16_t minutes);
int64_t tpCurrentUtcUnixTime(void);

bool headTypeTextValid(const char* text);
bool headTypeTextNormalize(char* text, size_t textSize);
bool headTypeTextSet(const char* text);
const char* headTypeTextGet(void);
bool headTypeTextFromNumber(uint16_t number, char* out, size_t outSize);
uint16_t headTypeTextNumber(const char* text);
int8_t headTypeProfileFromText(const char* text);

// Kopfbezogenes Peltier-Stromlimit. Separat gespeichert, damit das var_t-
// EEPROM-Layout fuer bestehende Geraete unveraendert bleibt.
#define PELTIER_CURRENT_LIMIT_MIN_MA        0U
#define PELTIER_CURRENT_LIMIT_MAX_MA     4000U
#define PELTIER_CURRENT_LIMIT_DEFAULT_MA 2500U

uint16_t peltierCurrentLimitGetMa(void);
uint16_t peltierCurrentLimitSanitizeMa(uint16_t ma);
void peltierCurrentLimitSetMa(uint16_t ma);
void peltierCurrentLimitLoad(void);

#define OPTIK_AUTOCAL_INTERVAL_MIN_INDEX     0
#define OPTIK_AUTOCAL_INTERVAL_MAX_INDEX     5
#define OPTIK_AUTOCAL_INTERVAL_DEFAULT_INDEX 1   // 15 min

// ADC/Pt100-Messfilter: getrennt vom var_t-EEPROM-Layout gespeichert.
#define ADC_MEAS_FILTER_NORMAL      0U
#define ADC_MEAS_FILTER_AUTO        1U
#define ADC_MEAS_FILTER_PRECISION   2U
#define ADC_MEAS_FILTER_DEFAULT     ADC_MEAS_FILTER_AUTO
#define ADC_MEAS_FILTER_MAX         ADC_MEAS_FILTER_PRECISION

uint8_t adcFilterModeGet();
void adcFilterModeSet(uint8_t mode);
void adcFilterModeLoad();

// ADC1 / ADS1263-SFOCAL: getrennt vom var_t-EEPROM-Layout gespeichert.
// 0 = nur beim Start, 1 = synchron mit Optik-/LED-Auto-Cal, 2/3/4 = zyklisch.
#define ADC1_SFOCAL_START_ONLY       0U
#define ADC1_SFOCAL_SYNC_AUTOCAL     1U
#define ADC1_SFOCAL_10MIN            2U
#define ADC1_SFOCAL_30MIN            3U
#define ADC1_SFOCAL_60MIN            4U
#define ADC1_SFOCAL_DEFAULT          ADC1_SFOCAL_SYNC_AUTOCAL
#define ADC1_SFOCAL_MAX              ADC1_SFOCAL_60MIN

uint8_t adc1SfocalModeGet();
void adc1SfocalModeSet(uint8_t mode);
void adc1SfocalModeLoad();
uint32_t adc1SfocalIntervalMs();

// Alternativer Hauptscreen: im Device-/UI-EEPROM-Block gespeichert.
#define MAIN_SCREEN_LAYOUT_STANDARD      0U
#define MAIN_SCREEN_LAYOUT_3VALUES       1U
#define MAIN_SCREEN_LAYOUT_DEFAULT       MAIN_SCREEN_LAYOUT_STANDARD
#define MAIN_SCREEN_LAYOUT_MAX           MAIN_SCREEN_LAYOUT_3VALUES

uint8_t mainScreenLayoutGet();
void mainScreenLayoutSet(uint8_t layout);
void mainScreenLayoutLoad();

// =============================================================================
// 3. DIE ZENTRALE KLIMA-SPEICHERSTRUKTUR (var_t)
// =============================================================================
// Hinweis:
// Diese Struktur liegt im EEPROM. Neue Felder deshalb moeglichst immer am Ende
// ergaenzen, damit bestehende Feldpositionen weitgehend erhalten bleiben.
// =============================================================================
typedef struct {
  cal_t    fühler_cal[2];             // Reserved/deprecated: alter R0-Platzhalter; R0 liegt separat bei +600
          
  uint8_t  skalierung_frei;           // Reserved: EEPROM-Platzhalter, aktuell nicht aktiv genutzt
  uint8_t  fan_percent;               // Lüfterleistung Bypass 50..100 %
  uint16_t optik_sollwert;            // Optik-Sollwert Prozent * 10, Bereich 800..990 (z.B. 985 = 98.5%)
  uint16_t luefter_start_temp;        // Reserved: alter Lüfter-Temperaturwert, aktuell nicht aktiv genutzt
  
  unsigned usb_report_cont     : 1;   // Kontinuierlicher USB-Report ein/aus
  unsigned usb_report_type     : 4;   // Report-Modus über die USB-Schnittstelle
                     #define  REPORT_DATA      1    
                     #define  REPORT_INST      2    
                     #define  REPORT_AD_DEBUG 13    
                     
  uint16_t pid_kp;                    // PID-Regler Proportionalanteil (kp)
  uint16_t pid_ki_alt;                // Reserved: altes Register, aktuell nicht aktiv genutzt
  uint8_t  freiheiz_ziel_temp[3];     // [0] aktiv: Freiheiztemperatur 40..76 °C; [1]/[2] reserved
  
  char     geraete_name[21];          // Geraete-Seriennummer als 5-stellige Ziffernfolge (00000..99999)
  float    pid_ki;                    // PID-Regler Integralanteil (ki), z.B. 4.0
  float    pid_kd;                    // PID-Regler Differenzialanteil (kd), z.B. 3.0
  
  unsigned bildschirm_modus    : 4;   // Reserved: alter gespeicherter Bildschirmmodus
                     #define  KLIMA_SCREEN     0    
                     #define  DIAGNOSE_SCREEN  1    
                     #define  MAX_MODE         1    
                     
  unsigned modus_default       : 4;   // Reserved: alter Default-Screen, Start erfolgt jetzt immer Hauptscreen
        
  uint8_t  h_bruecke_totzeit;         // H-Brücken Richtungswechsel-Totzeit in ms
  uint16_t regler_intervall_ms;       // Taktzeit des Regelungs-Intervalls in ms
  
  uint16_t akku_abschalt_spannung;    // Reserved: alter loBatt-Wert, aktuell nicht aktiv genutzt
  disp_t   display_konfig;            // Display-Helligkeit und Status

  // TP-3000 Sensorkopf-Auswahl
  uint8_t  head_type;                 // 0..3 = STP-3001..STP-3004
  uint32_t head_serial;               // 00000..99999, 00000 = Werksprofil / keine reale Kopf-SN

  // Optik-/LED-Auto-Cal Intervall: 0=10min, 1=15min, 2=30min, 3=60min, 4=120min, 5=180min
  uint8_t  optik_autocal_interval_index;
} var_t;

// =============================================================================
// 3b. KOPFKALIBRIERUNGSDATEI / SD-AUSTAUSCHFORMAT
// =============================================================================
typedef struct
{
  uint8_t  head_type;                 // bekanntes Profil 0..3, nur fuer STP-3001..3004
  char     head_type_text[HEAD_TYPE_TEXT_LEN + 1U];
  uint32_t head_serial;
  char     device_sn[DEVICE_SERIAL_DIGITS + 1U];
  uint32_t cal_date_yyyymmdd;
  uint32_t cal_time_hhmmss;

  int32_t  r0_mirror_scaled;
  int32_t  r0_ambient_scaled;

  int32_t  p2_soll1_mC[2];
  int32_t  p2_ist1_mC[2];
  int32_t  p2_soll2_mC[2];
  int32_t  p2_ist2_mC[2];
  uint8_t  p2_active[2];

  int32_t  tau_offset_mC;

  uint16_t pid_kp;
  int32_t  pid_ki_x1000;
  int32_t  pid_kd_x1000;
  uint16_t regler_intervall_ms;
  uint8_t  h_bruecke_totzeit_ms;
  uint8_t  fan_percent;
  uint16_t optik_sollwert_x10;
  uint16_t peltier_current_limit_ma;
} head_cal_data_t;

// =============================================================================
// 4. PUFFER-DEFINITIONEN & HARDWARE-SCHUTZ
// =============================================================================
// FIX: Von 64 auf 256 erhöht. Verhindert, dass lange Text-Kombinationen im RAM
// die direkt benachbarten Bit-Flags der Touchsteuerung oder Farbkanäle überschreiben.
extern char lcd_buf[256];              

#define ENACT_MIN 2   
#define ENACT_MAX 30  

// =============================================================================
// 5. SYSTEM- & TOUCH-FLAGS
// =============================================================================
typedef struct {
  unsigned short_push          : 1;   
  unsigned power_detected      : 1;   
  unsigned mode_change         : 1;   
  unsigned mode_display        : 1;   
  unsigned mode_newdefault     : 1;   
  unsigned swr_alarm           : 1;   // Genutzt als allgemeiner Hardware-Fehler-Alarm
  unsigned idle_refresh        : 1;   
  unsigned screensaver_on      : 1;   
  unsigned menu_lcd_upd        : 1;   
  unsigned config_mode         : 1;   
  unsigned picture             : 1;   
} flags;

// Global bereitgestellte System-Instanz aus der main
extern flags flag;

// --- MATHEMATISCHE HILFS-MAKROS ---
#ifndef SQR
#define SQR(x) ((x)*(x))
#endif
#ifndef ABS
#define ABS(x) ((x>0)?(x):(-x))
#endif

// --- TEENSY HARDWARE REBOOT BEFEHL ---
#define RESTART_ADDR       0xE000ED0C
#define RESTART_VAL        0x05FA0004
#define SOFT_RESET()       (*(volatile uint32_t *)RESTART_ADDR = RESTART_VAL)

// =============================================================================
// 6. SD-SPEICHER-DIAGNOSE
// =============================================================================
// Rein diagnostische Kennungen fuer potenziell blockierende SD-Zugriffe.
// Sie aendern den Ablauf nicht, sondern werden beim Safety-Timeout und im CSV
// mitgespeichert, damit lange Loop-Pausen eindeutig zugeordnet werden koennen.
#define SD_DIAG_IDLE             0U
#define SD_DIAG_BEGIN            1U
#define SD_DIAG_MKDIR            2U
#define SD_DIAG_CSV_OPEN         3U
#define SD_DIAG_CSV_HEADER       4U
#define SD_DIAG_CSV_WRITE        5U
#define SD_DIAG_CSV_FLUSH        6U
#define SD_DIAG_CSV_CLOSE        7U
#define SD_DIAG_SERIAL_PREPARE   8U
#define SD_DIAG_SERIAL_OPEN      9U
#define SD_DIAG_SERIAL_HEADER   10U
#define SD_DIAG_SERIAL_WRITE    11U
#define SD_DIAG_SERIAL_FLUSH    12U
#define SD_DIAG_SERIAL_CLOSE    13U
#define SD_DIAG_DOWNLOAD_OPEN   14U
#define SD_DIAG_DOWNLOAD_SEEK   15U
#define SD_DIAG_DOWNLOAD_READ   16U
#define SD_DIAG_DOWNLOAD_CLOSE  17U

// Download-I/O-Klassen fuer die Safety-Timeout-Unterscheidung. Nur waehrend
// eines unmittelbar potenziell blockierenden Downloadaufrufs gesetzt.
#define ETH_DOWNLOAD_IO_NONE     0U
#define ETH_DOWNLOAD_IO_SD       1U
#define ETH_DOWNLOAD_IO_NETWORK  2U

#define SAFETY_ORIGIN_NONE       0U
#define SAFETY_ORIGIN_SD         1U
#define SAFETY_ORIGIN_WEB        2U
#define SAFETY_ORIGIN_ADC        3U
#define SAFETY_ORIGIN_TFT        4U
#define SAFETY_ORIGIN_SER        5U
#define SAFETY_ORIGIN_LOOP       6U
#define SAFETY_ORIGIN_PELTIER    7U

// =============================================================================
// 7. AUSGABE-FILTER / DATENAUSGABE
// =============================================================================
typedef struct {
  uint32_t ms;
  uint32_t rawSeq;
  uint16_t nSamples;
  uint16_t nSeconds;
  float tMirror;
  float tAmbient;
  float dewpoint;
  float rh;
  float pressure;
  float tMirrorPpMk;
  float tAmbientPpMk;
} output_data_sample_t;

#endif // TP_T_H
