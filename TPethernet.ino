/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPethernet.ino
 * Zweck: NativeEthernet-Statusseite und Web-Ausgabe.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
// TPethernet.ino
// Teensy 4.1 NativeEthernet + kleiner Web-Hauptscreen
// =========================================================================
// Web-Hauptscreen mit gemeinsamer TFT-Chart-Historie und TFT-aehnlichem
// Web-Setup. Diagnose-Screens sowie Einstellungen, die die aktive
// Ethernet-/Webverbindung abschalten oder umkonfigurieren koennten, bleiben
// bewusst ausserhalb der Webbedienung.
// Wichtige Endpunkte:
//   /            HTML/CSS/JS Hauptscreen
//   /data.json   aktuelle Messwerte als kleines JSON
//   /chart.json  TFT-Chart-Historie in kleinen JSON-Bloecken
//   /setup       TFT-aehnliche Setup-Oberflaeche
//   /setup.json?part=0..2  Setup-Werte in drei kleinen Bloecken
//   /setup-live.json  kleiner Live-/Sperrstatus fuer das Setup
//   /setup-device-status und /setup-device-storage  Geraete-Speicher Web
//   /setup-status.json?part=0..1  Statusinformationen in zwei Bloecken
//   /download    Dateiexplorer; aktive Logdatei als fester Snapshot
//   /files.json  gefilterte, absteigend sortierte 25er-Seiten aus /LOG
//   /download-file?name=...  adaptiver 512-Byte-Dateidownload
//     &snapshot=1 erlaubt den festen Stand der aktiven Logdatei
// HTTP-Annahme: vier nicht blockierende Slots, vollstaendige Header
// Grosse HTML-Seiten: 512-Byte-Haeppchen ueber mehrere Hauptloops
// Browser-Cache: ETag/304; Setup-EXIT kehrt bevorzugt per History zurueck
// Kleine Antworten: exakte Content-Length; Socket-Close erst nach TCP-Drain
// =========================================================================

#include <Arduino.h>
#include <NativeEthernet.h>
#include <SD.h>
#include <TimeLib.h>
#include <math.h>
#include <string.h>
#include <new>

#include "TPlanguage.h"
#include "EEPROMAnything.h"

// Web-/Dateifunktionen sind nicht zeitkritisch. noinline verhindert, dass der
// Compiler sie trotz FLASHMEM wieder in den schnellen ITCM-Hauptdispatcher
// einbettet und dadurch die naechste 32-kB-FlexRAM-Bank ausloest.
#define ETH_FLASHMEM_NOINLINE FLASHMEM __attribute__((noinline))

// Vorwaertsdeklarationen fuer die von Arduino automatisch erzeugten Prototypen.
class EthBoundedWriter;
static void FLASHMEM ethSendText(EthBoundedWriter& client, bool ok, const char* msg);
static void FLASHMEM ethFileIndexRequestRefresh(void);
static void FLASHMEM ethFileIndexService(void);
static void FLASHMEM ethFileIndexStop(void);
static bool FLASHMEM ethIpIsValid(IPAddress ip);

// Einstellungen aus TPmenu_Interface.ino
extern bool interfaceEthEnabled(void);
extern bool interfaceEthDhcpEnabled(void);
extern bool interfaceEthStaticConfigValid(void);
extern bool interfaceWebServerEnabled(void);
extern bool interfaceWebSetupEnabled(void);
extern uint16_t interfaceEthWebPort(void);
extern uint8_t mainScreenLayoutGet(void);
extern void mainScreenLayoutSet(uint8_t layout);
extern void interfaceEthGetStaticIp(uint8_t ip[4]);
extern void interfaceEthGetSubnet(uint8_t ip[4]);
extern void interfaceEthGetGateway(uint8_t ip[4]);
extern void interfaceEthGetDns(uint8_t ip[4]);
extern bool interfaceFlowDisplayEnabled(void);
extern const char* interfaceFlowDisplayUnitText(void);
extern bool interfaceTExternalDisplayEnabled(void);
extern bool interfaceAlmemoAnyChannelEnabled(void);
extern bool interfaceAlmemoChannelEnabled(uint8_t index);
extern uint8_t interfaceAlmemoDisplayIndex(void);
extern const char* interfaceAlmemoDisplayLabel(uint8_t index);
extern const char* interfaceAlmemoDisplayUnitShort(uint8_t index);
extern const char* interfaceAlmemoCsvUnitText(uint8_t index);
extern uint8_t interfaceAlmemoAddressFor(uint8_t index);
extern uint8_t interfaceAlmemoChannelFor(uint8_t index);
extern uint8_t interfaceAlmemoRole(uint8_t index);
extern bool interfaceAlmemoTraceEnabled(void);
extern uint8_t interfaceOutputFilterIndex(void);
extern bool interfaceDiagnosticDataEnabled(void);
extern bool interfaceSdDiagnosticDataEnabled(void);
extern uint8_t interfaceOutputIntervalIndex(void);
extern uint8_t interfaceSdOutputIntervalIndex(void);
extern uint8_t interfaceSdOutputFilterIndex(void);
extern uint8_t interfaceSdLogIntervalIndex(void);
extern bool interfaceSdLogHeaderEnabled(void);
extern uint8_t interfaceFlowDisplayMode(void);
extern uint8_t interfaceRs232Mode(uint8_t port);
extern uint8_t interfaceRs232BaudIndex(uint8_t port);
extern uint8_t interfaceRs232OutputMode(uint8_t port);
extern uint8_t interfaceUsbMode(void);
extern bool interfaceWebSetGeneralOutput(uint8_t outputIndex, uint8_t filterIndex, bool diagnosticEnabled);
extern bool interfaceWebSetRs232(uint8_t port, uint8_t mode, uint8_t baudIndex, uint8_t outputMode);
extern bool interfaceWebSetUsb(uint8_t mode);
extern bool interfaceWebSetFlowDisplay(uint8_t mode);
extern bool interfaceWebSetAlmemo(uint8_t address, uint8_t channel, uint8_t intervalIndex);
extern bool interfaceWebSetAlmemoChannel(uint8_t index, uint8_t active, uint8_t address, uint8_t channel, uint8_t role);
extern bool interfaceWebSetAlmemoInterval(uint8_t intervalIndex);
extern bool interfaceWebSetAlmemoTrace(bool enabled);
extern bool interfaceWebSetWinControl(uint8_t outputMode, uint8_t baudIndex, uint8_t address, uint16_t tcpPort, uint8_t cycleIndex);
extern int8_t interfaceWinControlRs232Port(void);
extern bool interfaceWinControlEthernetEnabled(void);
extern uint16_t interfaceWinControlTcpPort(void);
extern uint8_t interfaceWinControlAddress(void);
extern uint8_t interfaceWinControlBaudIndex(void);
extern uint8_t interfaceWinControlCycleIndex(void);
extern uint8_t interfaceWinControlOutputMode(void);
extern bool winControlOutEthernetClientConnected(void);
extern uint16_t winControlOutEthernetActivePort(void);
extern uint8_t interfaceAlmemoAddress(void);
extern uint8_t interfaceAlmemoChannel(void);
extern uint8_t interfaceAlmemoIntervalIndex(void);
extern bool interfaceWebSetSd(uint8_t outputIndex, uint8_t filterIndex, uint8_t writeIntervalIndex, bool loggingEnabled, bool diagnosticEnabled, bool headerEnabled);
extern bool outputDataGetSample(uint8_t filterIndex, output_data_sample_t* out);

// Messwerte / Status
extern float tempSpiegel;
extern float tempUmgebung;
extern float relativeFeuchte;
extern float präziserTaupunkt;
extern float baroDruckHPa;
extern double amp_avg;
extern uint8_t aktuellerModus;
extern uint8_t ablaufStatus;
extern unsigned long statusTimer;
extern bool ads1263_bereit;
extern bool mcp3202_fehler;
extern float serialFlowLastValueLMin(void);
extern float serialExternalTempLastValueC(void);
extern bool serialExternalTempIsValid(void);
extern uint32_t serialExternalTempAgeMs(void);
extern uint32_t serialExternalTempRxErrors(void);
extern int8_t serialExternalTempPort(void);
extern float serialAlmemoLastValue(uint8_t index);
extern bool serialAlmemoIsValid(uint8_t index);
extern uint32_t serialAlmemoAgeMs(uint8_t index);
extern uint32_t serialAlmemoRxErrors(uint8_t index);
extern bool serialProtocolTraceIsActive(void);
extern const char* serialProtocolTraceFileName(void);
extern bool safetyIsFaultActive(void);
extern const char* safetyGetStopStatusText(void);
extern uint32_t safetyGetTimeoutLoopAgeMs(void);
extern uint8_t safetyGetTimeoutEthStage(void);
extern uint8_t safetyGetTimeoutEthRequest(void);
extern uint32_t safetyGetTimeoutEthElapsedMs(void);
extern uint32_t safetyGetTimeoutEthLastRequestUs(void);
extern uint32_t safetyGetTimeoutEthMaxRequestUs(void);
extern uint8_t safetyGetTimeoutEthMaxRequestCode(void);
extern uint32_t safetyGetTimeoutEthMaxWriteUs(void);
extern bool alarmVisualActive(void);
extern bool interfaceSdLoggingEnabled(void);
extern uint8_t sdLogGetStatus(void);
extern bool sdLogReadyForLogging(void);
extern bool sdLogEnsureReadyForAccess(void);
extern const char* sdLogGetCurrentFile(void);
extern bool sdLogWriteBlinkActive(void);
extern uint8_t sdStorageActivityState(void);
extern bool sdStorageWriteBlinkActive(void);
extern void sdStorageDiagBegin(uint8_t stage);
extern void sdStorageDiagEnd(uint8_t stage);
extern void schreibeEchtzeitUhr(void);
extern bool rtc_online;
extern var_t R;
extern flags flag;
extern uint32_t systemGetBootResetStatusRaw(void);
extern bool systemGetBootCrashReportAvailable(void);
extern void fanApplyNormalSpeed(void);
extern const char* deviceSerialGet(void);
extern const char* getHeadModelName(uint8_t head_type);
extern void applySensorHeadProfile(uint8_t head_type);
extern uint16_t peltierCurrentLimitGetMa(void);
extern void peltierCurrentLimitSetMa(uint16_t ma);
extern uint8_t headCalListTypes(char types[][HEAD_TYPE_TEXT_LEN + 1U], uint8_t maxCount);
extern uint8_t headCalListSerials(const char* headTypeText, uint32_t* serials, uint8_t maxCount);
extern bool headCalSave(const char* headTypeText, uint32_t headSerial, char* message, size_t messageSize);
extern bool headCalLoadLatest(const char* headTypeText, uint32_t headSerial, char* message, size_t messageSize);
extern bool deviceFactoryCalBackupSaveForSerial(const char* newSerial);
extern bool headCalStatusText(char* line1, size_t line1Size, char* line2, size_t line2Size);
extern bool deviceSettingsBackupValid(void);
extern bool deviceFactoryCalBackupValid(void);
extern uint32_t deviceSettingsBackupDate(void);
extern uint32_t deviceFactoryCalBackupDate(void);
extern bool deviceSettingsBackupSave(void);
extern bool deviceSettingsBackupLoad(void);
extern bool deviceFactoryCalBackupSave(void);
extern bool deviceFactoryCalBackupLoad(void);
extern void sensorFanToggleEnabled(void);
extern bool sensorFanIsEnabled(void);
extern bool sensorFanTachoMissing(void);
extern const char* sensorFanStatusText(void);
extern bool ads1263RefIsOld(void);
extern bool ads1263RefIsError(void);
extern bool opticHealthMainWarningActive(void);
extern const char* opticHealthMainStatusTextDE(void);
extern const char* opticHealthMainStatusTextEN(void);
extern bool opticHealthIsValid(void);
extern uint8_t opticHealthGetTotal(void);
extern uint8_t opticHealthGetLed(void);
extern uint8_t opticHealthGetTarget(void);
extern uint8_t opticHealthGetStability(void);
extern uint8_t opticHealthGetDark(void);
extern uint8_t opticHealthGetTime(void);
extern uint8_t opticHealthGetMin(void);
extern uint8_t opticHealthGetStatus(void);
extern float opticHealthGetLedMA(void);
extern float opticHealthGetTargetRaw(void);
extern float opticHealthGetBrutto(void);
extern float opticHealthGetDarkRaw(void);
extern float opticHealthGetNetto(void);
extern float opticHealthGetTrockenRef(void);
extern float opticHealthGetRestError(void);
extern float opticHealthGetNoisePp(void);
extern uint32_t opticHealthGetDurationMs(void);
extern uint16_t opticHealthGetCoarseSteps(void);
extern uint16_t opticHealthGetFineSteps(void);

// Alarm-Konfiguration
extern uint8_t alarmGetMode(void);
extern void alarmSetMode(uint8_t mode);
extern bool alarmBuzzerEnabled(void);
extern void alarmSetBuzzerEnabled(bool enabled);
extern int16_t alarmGetTauLow10(void);
extern int16_t alarmGetTauHigh10(void);
extern uint16_t alarmGetRhLow10(void);
extern uint16_t alarmGetRhHigh10(void);
extern void alarmSetTauLow10(int16_t value10);
extern void alarmSetTauHigh10(int16_t value10);
extern void alarmSetRhLow10(uint16_t value10);
extern void alarmSetRhHigh10(uint16_t value10);
extern void alarmConfigSave(void);
extern void alarmResetAcknowledge(void);

// Kalibrierwerte
extern int32_t pt100R0GetScaled(uint8_t sensor);
extern bool pt100R0SetScaled(uint8_t sensor, int32_t r0_scaled);
extern void pt100Cal2GetScaled(uint8_t sensor, int32_t* soll1, int32_t* ist1, int32_t* soll2, int32_t* ist2, bool* aktiv);
extern bool pt100Cal2SetScaled(uint8_t sensor, int32_t soll1, int32_t ist1, int32_t soll2, int32_t ist2);
extern void refCalGetAllScaled(uint32_t* date_yyyymmdd, int32_t* ref100, int32_t* ref120, int32_t* chA_corr100, int32_t* chA_corr120, int32_t* chB_corr100, int32_t* chB_corr120);
extern bool refCalSetAllScaled(uint32_t date_yyyymmdd, int32_t ref100_scaled, int32_t ref120_scaled, int32_t chA_corr100_scaled, int32_t chA_corr120_scaled, int32_t chB_corr100_scaled, int32_t chB_corr120_scaled);
extern int32_t taupunktOffsetGetScaled(void);
extern bool taupunktOffsetSetScaled(int32_t offset_mC);

// Gemeinsame Chart-Historie aus TPdisplay.ino. Der Webserver liest nur;
// Erfassung, Ringpuffer und TFT-Darstellung bleiben im Displaymodul.
extern uint16_t mainChartHistoryCapacity(void);
extern uint8_t mainChartHistoryRangeCount(void);
extern uint8_t mainChartHistoryActiveRange(void);
extern uint32_t mainChartHistorySampleMs(uint8_t range);
extern uint16_t mainChartHistoryCountValue(uint8_t range);
extern uint32_t mainChartHistoryFirstSequence(uint8_t range);
extern uint32_t mainChartHistoryNextSequence(uint8_t range);
extern bool mainChartHistoryGetPoint(uint8_t range, uint32_t sequence, float* dew, float* rh, float* ambient);
extern bool mainChartHistoryGetScale(uint8_t range,
                                     uint8_t metric,
                                     bool* valid,
                                     float* dataMin,
                                     float* dataMax,
                                     float* axisMin,
                                     float* axisMax);

// Bekannte blockierende Operationen sicher machen
extern void safetyBeginBlockingOperation(void);
extern void safetyEndBlockingOperation(void);
extern volatile uint32_t safetyLastFeedMs;
extern volatile bool safetyPeltierCutDone;

#define ETH_SDLOG_STATUS_NO_CARD  1
#define ETH_SDLOG_STATUS_READY    2
#define ETH_SDLOG_STATUS_LOGGING  3
#define ETH_SDLOG_STATUS_FILE_ERR 4

// NativeEthernet/Ethernet.begin() wartet mit DHCP standardmaessig sehr lange.
// Fuer das TFT-Geraet wollen wir keinen 60-s-Freeze, wenn kein PHY/Kabel/DHCP da ist.
#define ETH_DHCP_TIMEOUT_MS          5000UL
#define ETH_DHCP_RESPONSE_TIMEOUT_MS 1000UL
#define ETH_DHCP_POST_BEGIN_GRACE_MS 30000UL

// Static-IP darf ohne Netzwerkkabel nicht in Ethernet.begin(mac, ip, ...)
// haengen bleiben. Vor dem Static-Begin wird NativeEthernet daher mit einem
// sehr kurzen DHCP-Probe nur soweit initialisiert, dass linkStatus() verwertbar
// wird. Das ist ein einmaliger Start-/Bedienpfad, kein Suchlauf im Messloop.
#define ETH_STATIC_LINK_PROBE_TIMEOUT_MS          1000UL
#define ETH_STATIC_LINK_PROBE_RESPONSE_TIMEOUT_MS 100UL
#define ETH_STATIC_LINK_SETTLE_MS                 750UL

// Nach dem Boot zuerst ADC/Regelung/Display einige Schleifen ungestoert laufen
// lassen. Ein bereits offener Browser pollt sonst unmittelbar nach Serverstart
// mehrere Endpunkte und kann die erste Messwertbildung unnoetig belasten.
#define ETH_STARTUP_REQUEST_DELAY_MS 2500UL

// EthernetServer nicht per new/delete vom Heap verwalten.
// Dadurch keine delete-non-virtual-dtor Warnung und kein Heap-Fragmenteffekt bei Portwechsel.
// Der Speicher liegt bewusst in RAM2 (DMAMEM).
static DMAMEM uint64_t ethWebServerStorage[(sizeof(EthernetServer) + sizeof(uint64_t) - 1) / sizeof(uint64_t)];
static EthernetServer* ethWebServer = nullptr;
static bool ethConfigured = false;
static bool ethDhcpPending = false;
static uint32_t ethDhcpPendingUntilMs = 0;
static bool ethMdnsStarted = false;
static bool ethLastEnabled = false;
static bool ethLastDhcp = true;
static bool ethLastWebEnabled = false;
static uint16_t ethLastWebPort = 80;
static uint8_t ethLastStaticIp[4] = {0, 0, 0, 0};
static uint8_t ethLastSubnet[4] = {0, 0, 0, 0};
static uint8_t ethLastGateway[4] = {0, 0, 0, 0};
static uint8_t ethLastDns[4] = {0, 0, 0, 0};
static uint32_t ethLastConfigCheckMs = 0;
static uint32_t ethLastMaintainMs = 0;
static uint32_t ethLastServerBeginMs = 0;
static uint32_t ethAcceptRequestsAfterMs = 0;
static bool ethStartupRequestGuardArmed = false;

// Ethernet-/mDNS-Kennung. Die MAC-Adresse bleibt bewusst die eindeutige
// Teensy/PJRC-Hardware-MAC. Fuer die menschenlesbare Kennung wird aus der
// Geraete-Seriennummer ein lokaler Hostname gebildet: TP-3000-12345.local.
static const char ETH_HOSTNAME_PREFIX[] = "TP-3000-";
static char ethHostname[(sizeof(ETH_HOSTNAME_PREFIX) - 1U) + DEVICE_SERIAL_DIGITS + 1U] = "TP-3000-00000";

static DMAMEM char ethJsonBuf[4096];

// Browser oeffnen besonders ueber WLAN oft mehrere TCP-Verbindungen bereits,
// bevor auf allen Verbindungen HTTP-Nutzdaten angekommen sind. Ein einzelner
// wartender Client darf deshalb nicht mehr alle echten Requests blockieren.
// Vier kleine Slots sammeln Requestzeile und Header parallel und vollstaendig
// bis zur leeren Abschlusszeile. Pro Slot werden je Hauptloop nur bereits
// vorhandene Bytes gelesen; innerhalb eines Loopdurchlaufs wird nie gewartet.
static const uint8_t  ETH_REQUEST_SLOT_COUNT             = 4U;
static const size_t   ETH_REQUEST_LINE_BYTES             = 256U;
// Fuer ETag/If-None-Match und HTTP-Range wird der relevante Anfang der Header
// gespeichert. Der gesamte Header darf weiterhin bis 4096 Byte lang sein und
// wird auch bei groesseren Browserheadern vollstaendig bis zur Leerzeile gelesen.
static const size_t   ETH_REQUEST_HEADER_CACHE_BYTES     = 2048U;
static const uint16_t ETH_REQUEST_MAX_HEADER_BYTES       = 4096U;
static const uint32_t ETH_REQUEST_FIRST_BYTE_TIMEOUT_MS  = 150UL;
static const uint32_t ETH_REQUEST_IDLE_TIMEOUT_MS        = 50UL;
static const uint32_t ETH_REQUEST_ABSOLUTE_TIMEOUT_MS    = 250UL;
static const uint8_t  ETH_REQUEST_READ_BYTES_PER_LOOP    = 64U;

struct EthRequestSlot
{
  EthernetClient client;
  bool active;
  bool sawAnyByte;
  bool firstLineComplete;
  bool headerLineHasData;
  bool headersComplete;
  bool responseDraining;
  size_t requestLength;
  size_t headerCacheLength;
  uint16_t headerBytes;
  uint16_t responseDrainTarget;
  uint32_t startedUs;
  uint32_t startedMs;
  uint32_t lastProgressMs;
  uint32_t responseDrainStartedMs;
};

static EthRequestSlot ethRequestSlots[ETH_REQUEST_SLOT_COUNT];
static DMAMEM char ethRequestLines[ETH_REQUEST_SLOT_COUNT][ETH_REQUEST_LINE_BYTES];
static DMAMEM char ethRequestHeaderCache[ETH_REQUEST_SLOT_COUNT][ETH_REQUEST_HEADER_CACHE_BYTES];
static uint8_t ethRequestRoundRobin = 0U;


// ---------------------------------------------------------------------------
// Ethernet-Laufzeitdiagnose
// ---------------------------------------------------------------------------
// Diese Werte werden bewusst als einfache volatile Skalare gehalten. Der
// Safety-Timer kann sie im Timeout-Moment ohne Netzwerkzugriff kopieren.
enum EthernetDiagStage : uint8_t
{
  ETH_DIAG_IDLE = 0,
  ETH_DIAG_CONFIG = 1,
  ETH_DIAG_MAINTAIN = 2,
  ETH_DIAG_AVAILABLE = 3,
  ETH_DIAG_READ_REQUEST = 4,
  ETH_DIAG_HANDLE_REQUEST = 5,
  ETH_DIAG_WRITE = 6,
  ETH_DIAG_DELAY = 7,
  ETH_DIAG_STOP = 8
};

enum EthernetDiagRequest : uint8_t
{
  ETH_REQ_NONE = 0,
  ETH_REQ_ROOT = 1,
  ETH_REQ_SETUP = 2,
  ETH_REQ_SETUP_JSON = 3,
  ETH_REQ_SETUP_STATUS = 4,
  ETH_REQ_SETUP_CONTROL = 5,
  ETH_REQ_SETUP_FAN_TOGGLE = 6,
  ETH_REQ_SETUP_FAN = 7,
  ETH_REQ_SETUP_ALARM = 8,
  ETH_REQ_SETUP_R0 = 9,
  ETH_REQ_SETUP_2PM = 10,
  ETH_REQ_SETUP_2PA = 11,
  ETH_REQ_SETUP_REF = 12,
  ETH_REQ_SETUP_OFFSET = 13,
  ETH_REQ_SETUP_GENERAL = 14,
  ETH_REQ_SETUP_RS1 = 15,
  ETH_REQ_SETUP_RS2 = 16,
  ETH_REQ_SETUP_USB = 17,
  ETH_REQ_SETUP_SD = 18,
  ETH_REQ_SETUP_FLOW = 19,
  ETH_REQ_SETUP_LANGUAGE = 20,
  ETH_REQ_SETUP_CLOSE = 21,
  ETH_REQ_DATA_JSON = 22,
  ETH_REQ_CHART_JSON = 23,
  ETH_REQ_SET_TIME = 24,
  ETH_REQ_FAVICON = 25,
  ETH_REQ_DOWNLOAD_PAGE = 26,
  ETH_REQ_FILES_JSON = 27,
  ETH_REQ_DOWNLOAD_FILE = 28,
  ETH_REQ_SETUP_LIVE = 29,
  ETH_REQ_NOT_FOUND = 30,
  ETH_REQ_SETUP_ALMEMO = 31,
  ETH_REQ_SETUP_WINCONTROL = 32,
  ETH_REQ_SETUP_CONTROL_UNLOCK = 33,
  ETH_REQ_SETUP_HEAD_STATUS = 34,
  ETH_REQ_SETUP_HEAD_LIST = 35,
  ETH_REQ_SETUP_HEAD_CURRENT = 36,
  ETH_REQ_SETUP_HEAD_LOAD = 37,
  ETH_REQ_SETUP_HEAD_SAVE = 38,
  ETH_REQ_SETUP_DEVICE_STATUS = 39,
  ETH_REQ_SETUP_DEVICE_STORAGE = 40,
  ETH_REQ_SETUP_DISPLAY = 41
};

volatile uint8_t  ethDiagStage = ETH_DIAG_IDLE;
volatile uint8_t  ethDiagRequest = ETH_REQ_NONE;
volatile uint32_t ethDiagRequestStartMs = 0;
volatile uint32_t ethDiagLastRequestUs = 0;
volatile uint32_t ethDiagMaxRequestUs = 0;
volatile uint8_t  ethDiagMaxRequestCode = ETH_REQ_NONE;
volatile uint32_t ethDiagLastWriteUs = 0;
volatile uint32_t ethDiagMaxWriteUs = 0;

// Nur waehrend eines einzelnen potenziell blockierenden Download-I/O-Aufrufs
// ungleich null. Der Safety-Timer kann damit einen echten Datei-/SD-Stall von
// einem TCP-Stall unterscheiden, den Peltierpfad sicher abschalten und den
// Download nach Rueckkehr kontrolliert abbrechen, ohne eine Safety zu latchen.
volatile uint8_t ethDownloadIoKind = ETH_DOWNLOAD_IO_NONE;
volatile uint8_t ethDownloadStallDetected = ETH_DOWNLOAD_IO_NONE;

uint8_t FLASHMEM ethernetDiagGetStage(void) { return ethDiagStage; }
uint8_t FLASHMEM ethernetDiagGetRequest(void) { return ethDiagRequest; }
uint32_t FLASHMEM ethernetDiagGetActiveElapsedMs(void)
{
  uint32_t start = ethDiagRequestStartMs;
  return start ? (uint32_t)(millis() - start) : 0UL;
}
uint32_t FLASHMEM ethernetDiagGetLastRequestUs(void) { return ethDiagLastRequestUs; }
uint32_t FLASHMEM ethernetDiagGetMaxRequestUs(void) { return ethDiagMaxRequestUs; }
uint8_t FLASHMEM ethernetDiagGetMaxRequestCode(void) { return ethDiagMaxRequestCode; }
uint32_t FLASHMEM ethernetDiagGetMaxWriteUs(void) { return ethDiagMaxWriteUs; }

const char* FLASHMEM ethernetDiagStageText(uint8_t stage)
{
  switch (stage)
  {
    case ETH_DIAG_CONFIG: return "CONFIG";
    case ETH_DIAG_MAINTAIN: return "MAINTAIN";
    case ETH_DIAG_AVAILABLE: return "AVAILABLE";
    case ETH_DIAG_READ_REQUEST: return "READ";
    case ETH_DIAG_HANDLE_REQUEST: return "HANDLE";
    case ETH_DIAG_WRITE: return "WRITE";
    case ETH_DIAG_DELAY: return "DELAY";
    case ETH_DIAG_STOP: return "STOP";
    default: return "IDLE";
  }
}

const char* FLASHMEM ethernetDiagRequestText(uint8_t request)
{
  switch (request)
  {
    case ETH_REQ_ROOT: return "/";
    case ETH_REQ_SETUP: return "/setup";
    case ETH_REQ_SETUP_JSON: return "/setup.json";
    case ETH_REQ_SETUP_STATUS: return "/setup-status.json";
    case ETH_REQ_SETUP_LIVE: return "/setup-live.json";
    case ETH_REQ_SETUP_CONTROL: return "/setup-control";
    case ETH_REQ_SETUP_CONTROL_UNLOCK: return "/setup-control-unlock";
    case ETH_REQ_SETUP_HEAD_STATUS: return "/setup-head-status";
    case ETH_REQ_SETUP_HEAD_LIST: return "/setup-head-list.json";
    case ETH_REQ_SETUP_HEAD_CURRENT: return "/setup-head-current";
    case ETH_REQ_SETUP_HEAD_LOAD: return "/setup-head-load";
    case ETH_REQ_SETUP_HEAD_SAVE: return "/setup-head-save";
    case ETH_REQ_SETUP_DEVICE_STATUS: return "/setup-device-status";
    case ETH_REQ_SETUP_DEVICE_STORAGE: return "/setup-device-storage";
    case ETH_REQ_SETUP_FAN_TOGGLE: return "/setup-fan-toggle";
    case ETH_REQ_SETUP_FAN: return "/setup-fan";
    case ETH_REQ_SETUP_ALARM: return "/setup-alarm";
    case ETH_REQ_SETUP_R0: return "/setup-r0";
    case ETH_REQ_SETUP_2PM: return "/setup-2pm";
    case ETH_REQ_SETUP_2PA: return "/setup-2pa";
    case ETH_REQ_SETUP_REF: return "/setup-ref";
    case ETH_REQ_SETUP_OFFSET: return "/setup-offset";
    case ETH_REQ_SETUP_GENERAL: return "/setup-general";
    case ETH_REQ_SETUP_RS1: return "/setup-rs1";
    case ETH_REQ_SETUP_RS2: return "/setup-rs2";
    case ETH_REQ_SETUP_USB: return "/setup-usb";
    case ETH_REQ_SETUP_SD: return "/setup-sd";
    case ETH_REQ_SETUP_FLOW: return "/setup-flow";
    case ETH_REQ_SETUP_ALMEMO: return "/setup-almemo";
    case ETH_REQ_SETUP_WINCONTROL: return "/setup-wincontrol";
    case ETH_REQ_SETUP_LANGUAGE: return "/setup-language";
    case ETH_REQ_SETUP_DISPLAY: return "/setup-display";
    case ETH_REQ_SETUP_CLOSE: return "/setup-close";
    case ETH_REQ_DATA_JSON: return "/data.json";
    case ETH_REQ_CHART_JSON: return "/chart.json";
    case ETH_REQ_SET_TIME: return "/set-time";
    case ETH_REQ_FAVICON: return "/favicon.ico";
    case ETH_REQ_DOWNLOAD_PAGE: return "/download";
    case ETH_REQ_FILES_JSON: return "/files.json";
    case ETH_REQ_DOWNLOAD_FILE: return "/download-file";
    case ETH_REQ_NOT_FOUND: return "NOT_FOUND";
    default: return "NONE";
  }
}

// Gegenseitige Bedien-Sperre: Solange die Web-Setup-Seite aktiv ist, wird
// der Einstieg ins TFT-Setup blockiert. Nach 120 s ohne Webkontakt wird die
// Sperre automatisch aufgehoben; EXIT im Web loest sie sofort.
// Nicht in DMAMEM ablegen: RAM2/DMAMEM kann nach Reset/Upload kurz
// alte Werte enthalten. Dann waere das Web-Setup nach dem Boot scheinbar
// 120 s aktiv, obwohl kein Browser im Setup ist. 4 Byte RAM1 sind unkritisch.
static uint32_t ethWebSetupLastActivityMs = 0;
static const uint32_t ETH_WEB_SETUP_SESSION_TIMEOUT_MS = 120000UL;

bool ethernetWebSetupSessionActive(void)
{
  if (!interfaceWebSetupEnabled()) return false;
  if (ethWebSetupLastActivityMs == 0) return false;
  return (uint32_t)(millis() - ethWebSetupLastActivityMs) < ETH_WEB_SETUP_SESSION_TIMEOUT_MS;
}

static void FLASHMEM ethWebSetupTouch(void)
{
  ethWebSetupLastActivityMs = millis();
  if (ethWebSetupLastActivityMs == 0) ethWebSetupLastActivityMs = 1;
}

static void FLASHMEM ethWebSetupCloseSession(void)
{
  ethWebSetupLastActivityMs = 0;
}

// Ein kompletter Chartpuffer wird bewusst nicht in einer einzigen
// Antwort uebertragen. Kleine Bloecke halten jeden Webzugriff deutlich
// unter dem 1-s-Safety-Timeout; der Browser holt beim ersten Oeffnen
// automatisch mehrere Bloecke nacheinander.
static const uint16_t ETH_CHART_POINTS_PER_RESPONSE = 40;
static const uint8_t ETH_CHART_METRIC_DEW = 0;
static const uint8_t ETH_CHART_METRIC_RH = 1;
static const uint8_t ETH_CHART_METRIC_T_AMBIENT = 2;

// Teensy 4 F() aendert nur den Zeigertyp; grosse Stringliterale bleiben
// ohne PROGMEM sonst in RAM1. Die statischen HTML/CSS/JS-Teile liegen
// deshalb ausdruecklich im QSPI-Flash. Dynamische Sprachtexte werden
// beim Senden wie bisher dazwischen eingefuegt.
static const char ethMainPagePart0[] PROGMEM =
  "<!doctype html><html><head><meta charset='utf-8'>\n<meta name='viewport' content='width=device-width,initial-scale=1'>\n<title>Taupunktspiegel</title>\n<style>\n"
  ":root{--bg:#000;--fg:#fff;--yellow:#ffff00;--green:#00ff00;--red:#ff0000;--blue:#0000ff;--purple:#ff00ff;--gray:#c5c2c5;--grid:#00006b;--orange:#ffa600;--cyan:#00ffff}\n"
  "html,body{margin:0;min-height:100%;background:#111;color:var(--fg);font-family:'Droid Sans Mono','Courier New',monospace}\n"
  "body{display:flex;flex-direction:column;align-items:center;justify-content:flex-start;gap:10px;min-height:100vh;padding:10px 0;box-sizing:border-box}\n"
  "#screenWrap{position:relative;width:min(100vw,800px);aspect-ratio:800/480;overflow:hidden}\n"
  "#screenWrap:fullscreen{width:100vw;height:100vh;aspect-ratio:auto;background:#000}\n#screenWrap:-webkit-full-screen{width:100vw;height:100vh;aspect-ratio:auto;background:#000}\n"
  "#screen{position:absolute;left:0;top:0;width:800px;height:480px;transform-origin:0 0;background:#000;overflow:hidden;box-shadow:0 0 35px #000;outline:1px solid #333}\n"
  ".title{position:absolute;left:0;top:15px;width:800px;text-align:center;font-size:24px;letter-spacing:.5px}\n.clock{position:absolute;left:660px;top:15px;font-size:24px;color:var(--green)}\n"
  ".measureFrame{position:absolute;left:115px;top:88px;width:570px;height:210px;border:3px solid var(--gray);border-radius:8px;box-sizing:border-box;pointer-events:none;z-index:3}\n"
  ".big{position:absolute;left:115px;top:110px;width:570px;font-size:60px;line-height:1.25;white-space:nowrap;color:#fff;z-index:2;display:flex;flex-direction:column;align-items:center;justify-content:center;transform:translateX(3ch)}\n.bigRow{display:grid;grid-template-columns:3ch 1ch 3ch 1ch 3ch;width:11ch;align-items:baseline}.bigInt{text-align:right}.bigDot{text-align:center}.bigFrac{text-align:left}.bigUnit{text-align:right}.mainRhPct{font-size:.70em;font-weight:550;position:relative;top:.01em}.big3{top:76px;font-size:56px;line-height:1.25}\n"
  "#chartCanvas{position:absolute;left:12px;top:50px;width:776px;height:286px;display:none;background:#000;z-index:1}\n"
  "#viewHit{position:absolute;left:115px;top:88px;width:570px;height:210px;background:transparent;z-index:4;cursor:pointer;outline:none}\n#rangeHit{position:absolute;left:628px;top:52px;width:128px;height:40px;background:transparent;z-index:5;cursor:pointer;outline:none;display:none}\n"
  ".metricLine{display:grid;grid-template-columns:9ch 1ch 7ch 4ch;align-items:baseline;white-space:pre}.metricLabel{text-align:right}.metricColon{text-align:center}.metricValue{text-align:right}.metricUnit{text-align:left}\n"
  ".chartAlt{position:absolute;left:calc(180px - 2ch);top:355px;font-size:20px;line-height:1.75;color:var(--yellow);display:none}\n.tExt{position:absolute;left:calc(180px - 2ch);top:391px;font-size:20px;line-height:1.75;color:var(--yellow);display:none}\n.alarmBox{display:none}\n"
  ".temps{position:absolute;right:31px;top:355px;width:520px;font-size:20px;line-height:1.75;color:var(--yellow)}\n.infoLine{display:grid;grid-template-columns:18ch 1ch 3ch 1ch 3ch 2.5ch;justify-content:end;align-items:baseline}.infoLabel{text-align:right}.infoColon{text-align:center}.numInt{text-align:right}.numDot{text-align:center}.numFrac{text-align:left}.numUnit{text-align:right}\n.status{position:absolute;font-size:20px;white-space:pre}\n.flow{left:180px;top:391px;color:var(--yellow);font-size:20px}\n"
  ".pi{left:auto;right:31px;top:427px;width:520px;color:#fff}\n"
  ".iIntCell{position:relative}.iModeDot{position:absolute;left:.25ch;top:50%;width:8px;height:8px;border-radius:50%;background:var(--gray);transform:translateY(-50%)}\n"
  ".setup{position:absolute;left:10px;top:395px;width:140px;height:75px;border:2px solid #fff;border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:24px;color:#fff;box-sizing:border-box;text-dec"
  "oration:none}\n.setup span{opacity:.55;font-size:13px;position:absolute;bottom:8px}\n"
  ".mini{position:absolute;left:0;top:0;font-size:15px}.item{position:absolute;display:flex;align-items:center;gap:6px;white-space:nowrap}.sq{width:8px;height:8px;background:#fff;display:inline-block}\n"
  "#fanItem{left:170px;top:459px}#measItem{left:258px;top:459px}#safetyItem{left:390px;top:459px}#sdItem{left:485px;top:459px}#ethItem{left:620px;top:459px}\n"
  ".green{background:var(--green)}.red{background:var(--red)}.white{background:#fff}.gray{background:var(--gray)}.yellow{background:var(--yellow)}.blue{background:var(--blue)}.purple{background:var(--purple)}.orange{background:var(--orange)}.cyan{background:var(--cyan)}\n"
  ".b-green{border-color:var(--green)}.b-red{border-color:var(--red)}.b-gray{border-color:var(--gray)}.b-yellow{border-color:var(--yellow)}.b-orange{border-color:var(--orange)}.b-cyan{border-color:var(--cyan)}\n"
  ".webPanel{width:min(100vw,800px);box-sizing:border-box;background:#1b1b1b;border:1px solid #333;border-radius:8px;padding:10px;color:#eee;font-family:Arial,sans-serif;font-size:14px}\n"
  ".webPanel button{background:#303030;color:#fff;border:1px solid #666;border-radius:6px;padding:8px 12px;cursor:pointer;margin-right:8px}.webPanel button:hover{background:#444}.msg{margin-left:4px;color:#bbb}\n"
  "@media(max-width:700px){.webPanel{font-size:13px}}\n</style></head><body>\n<div id='screenWrap'><div id='screen'>\n<div class='title' id='title'>TAUPUNKTSPIEGEL HYGROMETER</div>\n<div class='clock' id='time'>--:--:--</div>\n"
  "<canvas id='chartCanvas' width='776' height='286'></canvas>\n<div class='measureFrame b-gray' id='measureFrame'></div>\n<div id='viewHit' role='button' tabindex='0' aria-label='Ansicht wechseln'></div><div id='rangeHit' role='button' tabindex='0' aria-label='Zeitraum wechseln'></div>\n"
  "<div class='big' id='bigVals'><div class='bigRow'><span class='bigInt' id='rhInt'>--</span><span class='bigDot'>.</span><span class='bigFrac' id='rhFrac'>--</span><span></span><span class='bigUnit'><span class='mainRhPct'>%</span>rH</span></div><div class='bigRow'><span class='bigInt' id='dewInt'>--</span><span class='bigDot'>.</span><span class='bigFrac' id='dewFrac'>---</span><span></span><span class='bigUnit'>°C</span></div><div class='bigRow' id='mainTaRow' style='display:none'><span class='bigInt' id='taMainInt'>--</span><span class='bigDot'>.</span><span class='bigFrac' id='taMainFrac'>---</span><span></span><span class='bigUnit'>°C</span></div></div>\n<div class='chartAlt metricLine' id='chartAlt'><span class='metricLabel' id='chartAltLabel'></span><span class='metricColon'>:</span><span class='metricValue' id='chartAltValue'></span><span class='metricUnit' id='chartAltUnit'></span></div>\n<div class='alarmBox' id='alarmBox'>ALARM</div>\n"
  "<div class='temps' id='temps'><div class='infoLine' id='tmLine'><span class='infoLabel'>T-Spiegel</span><span class='infoColon'>:</span><span class='numInt' id='tmInt'>--</span><span class='numDot'>.</span><span class='numFrac' id='tmFrac'>---</span><span class='numUnit'>°C</span></div><div class='infoLine' id='taLine'><span class='infoLabel'>T-Umgeb.</span><span class='infoColon'>:</span><span class='numInt' id='taInt'>--</span><span class='numDot'>.</span><span class='numFrac' id='taFrac'>---</span><span class='numUnit'>°C</span></div></div>\n<div class='status flow' id='flowbox'>F: --.--</div>\n<div class='tExt metricLine' id='tExtBox'><span class='metricLabel' id='tExtLabel'>T-Ext</span><span class='metricColon'>:</span><span class='metricValue' id='tExtValue'>--.--</span><span class='metricUnit' id='tExtUnit'>°C</span></div>\n"
  "<div class='status pi'><div class='infoLine'><span class='infoLabel'>P: <span id='p'>--.--</span> hPa | I</span><span class='infoColon'>:</span><span class='numInt iIntCell'><span class='iModeDot' id='iModeDot' aria-hidden='true'></span><span id='iInt'>--</span></span><span class='numDot'>.</span><span class='numFrac' id='iFrac'>---</span><span class='numUnit'>A</span></div></div>\n"
  "<a class='setup' id='setupBtn' href='/setup'>Setup<span id='setupHint'></span></a>\n<div class='mini'>\n"
  "<div id='fanItem' class='item'><span id='fanSq' class='sq green'></span><span id='fanText'>FAN ON</span></div>\n"
  "<div id='measItem' class='item'><span id='measSq' class='sq gray'></span><span id='measText'>";

static const char ethMainPagePart1[] PROGMEM =
  "</span></div>\n<div id='safetyItem' class='item'><span id='safetySq' class='sq green'></span><span>Safety</span></div>\n<div id='sdItem' class='item'><span id='sdSq' class='sq white'></span><span>SD-Logging</span></div>\n"
  "<div id='ethItem' class='item'><span id='ethSq' class='sq red'></span><span id='ip'>---.---.---.---</span></div>\n</div></div></div>\n"
  "<div class='webPanel'><button id='filesBtn' onclick=\"location.href='/download'\">Dateien</button><button id='snapshotBtn' onclick='saveScreenSnapshot()'>Snapshot</button><button id='fullscreenBtn' onclick='toggleFullscreen()'>Vollbild</button><button id='syncTimeBtn' onclick='setDeviceTime()'>PC-Zeit auf Gerät übertragen</button><span id='syncMsg' class='msg'></span></div>\n<script>\nconst E=id=>document.getElementById(id);let uiLang=0;const tr=(de,en)=>uiLang===1?en:de;function renderWebUi(){document.documentElement.lang=uiLang===1?'en':'de';document.title=tr('Taupunktspiegel','Dew point mirror');E('filesBtn').textContent=tr('Dateien','Files');E('snapshotBtn').textContent='Snapshot';E('fullscreenBtn').textContent=tr('Vollbild','Fullscreen');E('syncTimeBtn').textContent=tr('PC-Zeit auf Gerät übertragen','Transfer PC time to device');E('viewHit').setAttribute('aria-label',tr('Ansicht wechseln','Change view'));E('rangeHit').setAttribute('aria-label',tr('Zeitraum wechseln','Change time range'))}\n"
  "const L={humidity:'";

static const char ethMainPagePart2[] PROGMEM =
  "',dewPoint:'";

static const char ethMainPagePart3[] PROGMEM =
  "',tPoint:'";

static const char ethMainPagePart4[] PROGMEM =
  "',now:'";

static const char ethMainPagePart5[] PROGMEM =
  "'};\nuiLang=L.humidity==='Humidity'?1:0;L.tAmbient=tr('T-Umgebung','T-Ambient');renderWebUi();\nconst canvas=E('chartCanvas'),ctx=canvas.getContext('2d');\nconst PX=96,PY=50,PW=640,PH=164;\nlet view=0,chartRange=0,lastData=null,dataBusy=false,pageActive=true;\nconst chartRangeLabels=['60 min','4 h','8 h','24 h'],chartRangeKey='tp3000ChartRange';\nfunction chartLoadStoredRange(){try{let v=Number(localStorage.getItem(chartRangeKey));if(Number.isInteger(v)&&v>=0&&v<chartRangeLabels.length)chartRange=v}catch(e){}}\nfunction chartSaveRange(){try{localStorage.setItem(chartRangeKey,String(chartRange))}catch(e){}}\nchartLoadStoredRange();\nlet chartDew=[],chartRh=[],chartTa=[],"
  "chartNext=null,chartCapacity=1440;\nlet chartScale={dew:null,rh:null,ta:null},chartPendingScale={dew:null,rh:null,ta:null};\nlet chartBusy=false,chartReady=false,chartLoading=false,"
  "chartAnchorFirst=0,chartAnchorTotal=0,chartFastTimer=0,chartGeneration=0;\nfunction f(n,d){return Number(n).toFixed(d)}\nfunction cls(id,c){E(id).className='sq '+c}\nfunction chartTimeLabels(){if(chartRange===1)return [['-4','-3','-2','-1',L.now],4];if(chartRange===2)return [['-8','-6','-4','-2',L.now],4];if(chartRange===3)return [['-24','-20','-16','-12','-8','-4',L.now],6];return [['-60','-45','-30','-15',L.now],4]}\n"
  "function resizeScreen(){let w=E('screenWrap'),sw=w.clientWidth,sh=w.clientHeight,s=Math.min(sw/800,sh/480),d=E('screen');d.style.left=((sw-800*s)/2)+'px';d.style.top=((sh-480*s)/2)+'px';d.style.transform='scale('+s+')'}\nfunction validNumber(v){return v!==null&&v!==undefined&&Number.isFinite(Number(v))}\n"
  "function padded(v,d,w,ok=true,placeholder=''){return ok&&validNumber(v)?f(v,d).padStart(w,' '):placeholder}\nfunction setSplitValue(prefix,v,digits,intPlaceholder='--'){let t=validNumber(v)?f(v,digits):((intPlaceholder||'--')+'.'+'-'.repeat(digits)),a=t.split('.');E(prefix+'Int').textContent=a[0];E(prefix+'Frac').textContent=a[1]||'-'.repeat(digits)}\n"
  "function directValue(d,name,validName,fallback){return d&&d[validName]&&validNumber(d[name])?Number(d[name]):fallback}\n"
  "function extraDisplayLabel(d){let lbl=String((d&&d.extraLabel)||'T-Ext');if(d&&d.extraType==='text'){let idx=Number(d.extraIndex),role=Number(d.extraRole)||1;if(idx===0||idx===1){let base=role===2?'F':(role===3?'P':(role===4?'A':'T'));return base+(idx===1?'-Ex2':'-Ext')}}return lbl.replace(':','')}\n"
  "function setBoxGeometry(el,x,y,w,h,r){el.style.left=x+'px';el.style.top=y+'px';el.style.width=w+'px';el.style.height=h+'px';if(r!==undefined)el.style.borderRadius=r+'px'}\nfunction isAltMain(){return view===0&&lastData&&Number(lastData.mainLayout)===1}\nfunction applyMainLayout(){if(view!==0)return;let alt=isAltMain();setBoxGeometry(E('measureFrame'),alt?12:115,alt?50:88,alt?776:570,alt?286:210,alt?10:8);setBoxGeometry(E('viewHit'),alt?12:115,alt?50:88,alt?776:570,alt?286:210);E('bigVals').className=alt?'big big3':'big';E('mainTaRow').style.display=alt?'grid':'none';E('temps').style.top=alt?'391px':'355px';E('taLine').style.display=alt?'none':'grid'}\n"
  "function applyView(){let chart=view!==0;E('chartCanvas').style.display=chart?'block':'none';E('bigVals').style.display=chart?'none':'block';E('chartAlt').style.display=chart?'grid':'none';E('rangeHit').style.display=chart?'block':'none';if(chart){setBoxGeometry(E('measureFrame'),12,50,776,286,10);setBoxGeometry(E('viewHit'),12,50,776,286);E('bigVals').className='big';E('mainTaRow').style.display='none';E('temps').style.top='355px';E('taLine').style.display='grid'}else applyMainLayout();renderValues();drawChart();if(chart)chartUpd()}\nfunction cycleView(){view=(view+1)%4;applyView()}\nfunction cycleRange(){chartGeneration++;chartRange=(chartRange+1)%4;chartSaveRange();chartNext=null;chartDew=[];chartRh=[];chartTa=[];chartLoading=false;chartReady=false;drawChart();chartUpd();chartScheduleFast();setTimeout(chartUpd,120)}\nfunction chartMetric(){return view===1?'rh':(view===2?'dew':'ta')}\n"
  "function chartCurrent(metric){if(!lastData)return null;if(metric==='rh')return lastData.directRhValid?Number(lastData.directRh):null;if(metric==='ta')return lastData.directTAmbientValid?Number(lastData.directTAmbient):null;return lastData.directDewValid?Number(lastData.directDew):null}\n"
  "function chartFormat(v,metric){let rh=metric==='rh';if(!validNumber(v))return rh?' --.--%rH':' --.---°C';return rh?f(v,2).padStart(6,' ')+'%rH':f(v,3).padStart(7,' ')+'°C'}\n"
  "function yFor(v,min,max){if(!validNumber(v)||!validNumber(min)||!validNumber(max)||max<=min)return PY+PH-1;let n=(Number(v)-min)/(max-min);n=Math.max(0,Math.min(1,n));return PY+Math.round((PH-1)*(1-n))}\n"
  "function mono16(t,x,y){for(let k=0;k<t.length;k++)ctx.fillText(t[k],x+k*13,y)}\n"
  "function drawChart(){if(view===0)return;let metric=chartMetric(),rhView=metric==='rh',s=chartScale[metric];let axisMin=s&&validNumber(s.axisMin)?Number(s.axisMin):(rhView?0:-.1);let axisMax=s&&validNumber(s.axisMax)?Number(s.axis"
  "Max):(rhView?1:.1);ctx.clearRect(0,0,776,286);ctx.fillStyle='#000';ctx.fillRect(0,0,776,286);ctx.lineWidth=1;ctx.strokeStyle='#00006b';for(let i=0;i<=4;i++){let y=PY+Math.floor(i*(PH-1)/4)+.5;ctx.beginPath();ctx.moveTo(P"
  "X,y);ctx.lineTo(PX+PW-1,y);ctx.stroke()}ctx.strokeStyle='#c5c2c5';ctx.strokeRect(PX+.5,PY+.5,PW-1,PH-1);ctx.font=\"14px 'Droid Sans Mono','Courier New',monospace\";ctx.fillStyle='#c5c2c5';ctx.textBaseline='middle';ctx."
  "textAlign='left';for(let i=0;i<=4;i++){let v=axisMax-(axisMax-axisMin)*(i/4),y=PY+Math.floor(i*(PH-1)/4)+.5;ctx.fillText(f(v,2).padStart(6,' '),16,y)}ctx.textBaseline='alphabetic';let tl=chartTimeLabels(),times=tl[0],den=tl[1];ctx.textAlign='center';for(let i=0;"
  "i<times.length;i++){if(i+1>=times.length){ctx.textAlign='right';ctx.fillText(times[i],PX+PW-1,226);ctx.textAlign='center'}else{let x=PX+Math.floor(i*(PW-1)/den);ctx.fillText(times[i],x,226)}}ctx.font=\"16px 'Droid Sans Mono','Courier New',monospace\";ctx.fillStyle='#fff';ctx.textAlign='left';ctx.textBaseline='top';let name=rhView?L.h"
  "umidity:(metric==='ta'?L.tAmbient:L.dewPoint),head=name+': ';mono16(head,94,17);mono16(chartFormat(chartCurrent(metric),metric),94+head.length*13,17);let rl=chartRangeLabels[chartRange]||'60 min';mono16(rl,734-rl.length*13,17);ctx.textBaseline='alphabetic';mono16('Min ',98,255);mono16('Max ',570,255);let minText=s&&s.valid?chartFor"
  "mat(s.dataMin,metric):chartFormat(null,metric);let maxText=s&&s.valid?chartFormat(s.dataMax,metric):chartFormat(null,metric);mono16(minText,150,255);mono16(maxText,622,255);"
  "if(chartCapacity<2)return;let vals=metric==='rh'?chartRh:(metric==='ta'?chartTa:chartDew),sum=new Float64Array(PW),mn=new Float64Array(PW),mx=new Float64Array(PW),cnt=new Uint16Array(PW);for(let slot=0;"
  "slot<Math.min(vals.length,chartCapacity);slot++){let v=vals[slot];if(!validNumber(v))continue;v=Number(v);let col=Math.floor(slot*(PW-1)/(chartCapacity-1));"
  "if(cnt[col]===0){mn[col]=v;mx[col]=v}else{if(v<mn[col])mn[col]=v;if(v>mx[col])mx[col]=v}sum[col]+=v;cnt[col]++}let avgY=new In"
  "t16Array(PW),minY=new Int16Array(PW),maxY=new Int16Array(PW),ok=new Uint8Array(PW);for(let x=0;x<PW;x++){if(!cnt[x])continue;ok[x]=1;avgY[x]=yFor(sum[x]/cnt[x],axisMin,axisMax);minY[x]=yFor(mn[x],axisMin,axisMax);maxY[x]"
  "=yFor(mx[x],axisMin,axisMax)}ctx.strokeStyle='#c5c2c5';ctx.beginPath();for(let x=0;x<PW;x++){if(!ok[x])continue;let y1=Math.min(minY[x],maxY[x]),y2=Math.max(minY[x],maxY[x]),sx=PX+x+.5;ctx.moveTo(sx,y1);ctx.lineTo(sx,y2)"
  "}ctx.stroke();ctx.strokeStyle='#fff';ctx.fillStyle='#fff';ctx.beginPath();let prev=false;for(let x=0;x<PW;x++){if(!ok[x]){prev=false;continue}let sx=PX+x+.5,sy=avgY[x]+.5;if(prev)ctx.lineTo(sx,sy);else{ctx.moveTo(sx,sy);"
  "ctx.fillRect(PX+x,avgY[x],1,1)}prev=true}ctx.stroke()}\n"
  "function renderValues(){if(!lastData)return;let d=lastData;uiLang=Number(d.lang)===1?1:0;renderWebUi();let chart=view!==0;E('title').textContent=d.lang===1?'DEW POINT MIRROR HYGROMETER':'TAUPUNKTSPIEGEL HYGROMETER';E('time').textContent=d.time;let rh=directValue(d,'rh','rhValid',null),dew=directValue(d,'dew','dewValid',null);setSplitValue('rh',rh,2,'--');setSplitValue('dew',dew,3,'--');let tm=chart"
  "?directValue(d,'directTMirror','directTMirrorValid',null):directValue(d,'tMirror','tMirrorValid',null),ta=chart?directValue(d,'directTAmbient','directTAmbientValid',null):directValue(d,'tAmbient','tAmbientValid',null),pr=directValue(d,'directPressure','directPressureValid',null),cur=directValue(d,'current','currentValid',null);setSplitValue('tm',tm,3,'--');setSplitValue('ta',ta,3,'--');setSplitValue('taMain',ta,3,'--');if(!chart)applyMainLayout();E('p').textContent=padded(pr,2,7,validNumber(pr),'--.--');setSplitValue('i',cur,3,'--');let iDot=E('iModeDot');if(iDot){let iv=validNumber(cur)?Math.abs(Number(cur)):0,m=String(d.mode||'');iDot.style.background=iv<0.020?'#c5c2c5':(m==='Kühlen'?'#0000ff':(m==='Heizen'?'#ff0000':'#c5c2c5'))}let fb=E('flowbox'),tb=E('tExtBox');if(d.extraEnabled){let col=d.extraColor==='red'?'#ff3030':'#ffe600';if(d.extraType==='text'){fb.style.display='none';tb.style.display='grid';tb.style.color=col;E('tExtLabel').textContent=extraDisplayLabel(d);E('tExtValue').textContent=d.extraValue;E('tExtUnit').textContent=d.extraUnit}else{tb.style.display='none';fb.style.display='block';fb.style.left='180px';fb.style.fontSize='20px';fb.style.color=col;fb.textContent=d.extraLabel+' '+d.extraValue+' '+d.extraUnit}}else{fb.style.display='none';tb.style.display='none'}let sc=d.measurementStatusColor||'gray';E('measureFrame').clas"
  "sName='measureFrame b-'+sc;cls('fanSq',d.fanColor||'white');E('fanText').textContent=d.fanText||'FAN OFF';cls('measSq',sc);E('measText').textContent=d.measurementStatusText||(d.alarmText||'INIT');cls('safetySq',d.safetyFault?'red':'green');cls('sdSq',d.sdColor);cls('ethSq',d.ethOk?'green':'red');E("
  "'ip').textContent=d.ip;let sb=E('setupBtn');if(sb){let en=!!d.webSetupEnabled,busy=!!(d.tftSetupActive||d.webSetupActive);sb.href=(en&&!busy)?'/setup':'#';sb.onclick=(en&&!busy)?null:()=>false;E('setupHint').textContent=en?'':(d.lang===1?'disabled':'deaktiviert');sb.style.opacity=en?'1':'.45';sb.style.borderColor=busy?'#ffff00':'#fff';sb.style.color=busy?'#ffff00':'#fff';sb.setAttribute('aria-disabled',(!en||busy)?'true':'false');}if(view===1||view===3){let v=d.directDewValid?d.directDew:null;E('chartAltLabel').textContent=L.tPoint;E('chartAltValue').textContent=validNumber(v)?f(v,3):'--.---';E('chartAltUnit').textContent='°C'}else if(view===2){let v=d.directRhValid?d.directRh:null;E('chartAltLabel').textContent=L.humidity;E('chartAltValue').textContent=validNumber(v)?f(v,2):'--.--';E('chartAltUnit').textContent='%rH'}drawChart()}\n"
  "function chartSetScale(dst,d){dst.dew=d.dewScale;dst.rh=d.rhScale;dst.ta=d.taScale}\nfunction chartBeginLoad(d){chartCapacity=Math.max(2,Number(d.capacity)||1440);"
  "chartDew=new Array(chartCapacity).fill(null);chartRh=new Array(chartCapacity).fill(null);chartTa=new Array(chartCapacity).fill(null);chartAnchorFirst=Number(d.first)||0;chartAnchorTotal=Number(d.total);"
  "if(!Number.isFinite(chartAnchorTotal)||chartAnchorTotal<chartAnchorFirst)chartAnchorTotal=chartAnchorFirst;chartNext=chartAnchorFirst;chartLoading=true;chartReady=false;"
  "chartSetScale(chartScale,d);chartSetScale(chartPendingScale,d)}\nfunction chartStoreInitial(d){let first=Number(d.first),pts=Array.isArray(d.points)?d.points:[],"
  "span=Math.min(chartCapacity,Math.max(0,chartAnchorTotal-chartAnchorFirst)),base=chartCapacity-span;chartSetScale(chartPendingScale,d);for(let i=0;i<pts.length;i++){let seq=first+i;"
  "if(seq<chartAnchorFirst||seq>=chartAnchorTotal)continue;let slot=base+(seq-chartAnchorFirst);if(slot<0||slot>=chartCapacity)continue;let p=pts[i];"
  "chartDew[slot]=Array.isArray(p)?p[0]:null;chartRh[slot]=Array.isArray(p)?p[1]:null;chartTa[slot]=Array.isArray(p)?p[2]:null}let n=Number(d.next);if(!Number.isFinite(n))n=first+pts.length;chartNext=Math.min(n,"
  "chartAnchorTotal);if(chartNext>=chartAnchorTotal){chartNext=chartAnchorTotal;chartLoading=false;chartReady=true;chartScale.dew=chartPendingScale.dew;"
  "chartScale.rh=chartPendingScale.rh;chartScale.ta=chartPendingScale.ta}return chartLoading}\nfunction chartStoreLive(d){let pts=Array.isArray(d.points)?d.points:[];"
  "if(chartDew.length!==chartCapacity||chartRh.length!==chartCapacity||chartTa.length!==chartCapacity){chartDew=new Array(chartCapacity).fill(null);chartRh=new Array(chartCapacity).fill(null);chartTa=new Array(chartCapacity).fill(null)}"
  "if(pts.length>=chartCapacity){let tail=pts.slice(pts.length-chartCapacity);chartDew=tail.map(p=>Array.isArray(p)?p[0]:null);chartRh=tail.map(p=>Array.isArray(p)?p[1]:null);chartTa=tail.map(p=>Array.isArray(p)?p[2]:null)}"
  "else if(pts.length){chartDew.splice(0,pts.length);chartRh.splice(0,pts.length);chartTa.splice(0,pts.length);for(let p of pts){chartDew.push(Array.isArray(p)?p[0]:null);chartRh.push(Array.isArray(p)?p[1]:null);chartTa.push(Array.isArray(p)?p[2]:null)}"
  "}let n=Number(d.next);chartNext=Number.isFinite(n)?n:Number(d.first)+pts.length;chartSetScale(chartScale,d);chartReady=true;return !!d.more}\n"
  "function chartScheduleFast(){if(chartFastTimer||view===0||!pageActive||document.hidden)return;chartFastTimer=setTimeout(()=>{chartFastTimer=0;chartUpd()},75)}\n"
  "async function chartUpd(){if(view===0||chartBusy||!pageActive||document.hidden)return;let reqRange=chartRange,reqGen=chartGeneration;chartBusy=true;let needMore=true,fast=false;try{for(let loops=0;loops<4&&needMore&&view!==0;"
  "loops++){let url='/chart.json?r='+reqRange+(chartNext===null?'':'&from='+chartNext),r=await fetch(url,{cache:'no-store'});if(!r.ok)throw new Error('chart');let d=await r.json();if(reqGen!==chartGeneration||reqRange!==chartRange){fast=true;break}"
  "if(Number(d.range)!==reqRange){fast=true;needMore=false;break}if(d.reset||chartNext===null){chartBeginLoad(d)}else if(Number(d.first)!==Number(chartNext)){chartNext=null;chartDew=[];chartRh=[];chartTa=[];chartLoading=false;chartReady=false;loops--;"
  "continue}needMore=chartLoading?chartStoreInitial(d):chartStoreLive(d);drawChart()}fast=fast||needMore}catch(e){fast=false}finally{chartBusy=false;if(fast||reqGen!==chartGeneration||reqRange!==chartRange)chartScheduleFast()}}\n"
  "async function setDeviceTime(){let n=new Date(),q='?y='+n.getFullYear()+'&mo='+(n.getMonth()+1)+'&d='+n.getDate()+'&h='+n.getHours()+'&mi='+n.getMinutes()+'&s='+n.getSeconds();E('syncMsg').textContent=tr('übertrage...','transferring...');try{"
  "let r=await fetch('/set-time'+q,{cache:'no-store'}),t=await r.text();E('syncMsg').textContent=(r.ok?'OK: ':tr('Fehler: ','Error: '))+t;upd()}catch(e){E('syncMsg').textContent=tr('Fehler: keine Verbindung','Error: no connection')}}\n"
  "const toggleFullscreen=()=>{let w=E('screenWrap'),active=document.fullscreenElement||document.webkitFullscreenElement;if(!active){let fn=w.requestFullscreen||w.webkitRequestFullscreen;if(fn){let r=fn.call(w);if(r&&r.catch)r.catch(()=>{E('syncMsg').textContent=tr('Vollbild nicht verfügbar','Fullscreen not available')})}}else{let fn=document.exitFullscreen||document.webkitExitFullscreen;if(fn)fn.call(document)}};\n"
  "const snapshotCss=()=>{let t='';for(let sh of Array.from(document.styleSheets)){try{for(let r of Array.from(sh.cssRules))t+=r.cssText+'\\n'}catch(e){}}return t};\n"
  "const snapshotFileName=()=>{let d=new Date(),p=n=>String(n).padStart(2,'0');return 'TP3000_'+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'_'+p(d.getHours())+p(d.getMinutes())+p(d.getSeconds())+'.png'};\n"
  "const saveScreenSnapshot=()=>{let m=E('syncMsg');m.textContent=tr('Snapshot wird erstellt...','Creating snapshot...');try{let src=E('screen'),cl=src.cloneNode(true),sc=E('chartCanvas'),cc=cl.querySelector('#chartCanvas');cl.style.position='relative';cl.style.left='0';cl.style.top='0';cl.style.transform='none';cl.style.boxShadow='none';cl.style.outline='none';if(cc){let im=document.createElement('img');im.id='chartCanvas';im.width=sc.width;im.height=sc.height;im.src=sc.toDataURL('image/png');im.setAttribute('style',cc.getAttribute('style')||'');cc.parentNode.replaceChild(im,cc)}let ns='http://www.w3.org/1999/xhtml',box=document.createElementNS(ns,'div'),st=document.createElementNS(ns,'style');box.setAttribute('style','width:800px;height:480px;background:#000;overflow:hidden');st.textContent=snapshotCss();box.appendChild(st);box.appendChild(cl);let body=new XMLSerializer().serializeToString(box),svg='<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"480\"><foreignObject width=\"800\" height=\"480\">'+body+'</foreignObject></svg>',url=URL.createObjectURL(new Blob([svg],{type:'image/svg+xml;charset=utf-8'})),im=new Image();im.onload=()=>{let out=document.createElement('canvas');out.width=800;out.height=480;let g=out.getContext('2d');g.fillStyle='#000';g.fillRect(0,0,800,480);g.drawImage(im,0,0);URL.revokeObjectURL(url);out.toBlob(b=>{if(!b){m.textContent=tr('Snapshot fehlgeschlagen','Snapshot failed');return}let u=URL.createObjectURL(b),a=document.createElement('a');a.href=u;a.download=snapshotFileName();document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(u),1000);m.textContent=tr('Snapshot gespeichert','Snapshot saved')},'image/png')};im.onerror=()=>{URL.revokeObjectURL(url);m.textContent=tr('Snapshot fehlgeschlagen','Snapshot failed')};im.src=url}catch(e){m.textContent=tr('Snapshot fehlgeschlagen','Snapshot failed')}};\n"
  "async function upd(){if(dataBusy||!pageActive||document.hidden)return;dataBusy=true;try{let r=await fetch('/data.json',{cache:'no-store'}),d=await r.json();lastData=d;renderValues()}catch(e){cls('ethSq','red')}finally{dataBusy=false}}\n"
  "E('viewHit').addEventListener('click',cycleView);E('viewHit').addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();cycleView()}});E('rangeHit').addEventListener('click',e=>{e.stopPropagation();cycleRange()});E('rangeHit').addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();e.stopPropagation();cycleRange()}});window.addEventListener('pagehide',()=>{pageActive=false});window.addEventListener('pageshow',()=>{pageActive=true;upd();if(view!==0)chartUpd()});window.addEventListener('resize',resizeScreen);document.addEventListener('fullscreenchange',()=>setTimeout(resizeScreen,0));document.addEventListener('webkitfullscreenchange',()=>setTimeout(resizeScreen,0));resizeScreen("
  ");applyView();setInterval(upd,1000);setInterval(chartUpd,2500);upd();\n"
  "</script></body></html>";


// TFT-aehnliche Web-Setup-Seite. Alle statischen Texte, CSS und JavaScript
// liegen ausdruecklich im QSPI-Flash und belegen kein RAM1.
static const char ethSetupPage[] PROGMEM = R"TPSETUP(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TP-3000 Setup</title>
<style>
:root{--black:#000;--white:#fff;--blue:#0019a8;--yellow:#ffe600;--gray:#c5c2c5;--red:#ff3030;--green:#00e000}
*{box-sizing:border-box}html,body{margin:0;min-height:100%;background:#111;color:#fff;font-family:'Droid Sans Mono','Courier New',monospace}
body{display:flex;justify-content:center;align-items:flex-start;padding:10px 0}
#wrap{position:relative;width:min(100vw,800px);aspect-ratio:800/480;overflow:hidden}
#screen{position:absolute;left:0;top:0;width:800px;height:480px;transform-origin:0 0;background:#000;overflow:hidden;box-shadow:0 0 35px #000;outline:1px solid #333}
#title{position:absolute;left:155px;top:18px;width:500px;font-size:22px;white-space:nowrap;overflow:hidden}
#body{position:absolute;left:155px;top:62px;width:500px;height:286px;font-size:20px;overflow:hidden}
.menuRow,.formRow{position:absolute;left:0;width:500px;height:48px;display:flex;align-items:center;white-space:nowrap}
.menuArrow,.formArrow{width:58px;flex:0 0 58px}.menuText{overflow:hidden;text-overflow:ellipsis}
.formLabel{width:190px;overflow:hidden;text-overflow:ellipsis}.formCtrl{width:250px;display:grid;grid-template-columns:198px 47px;column-gap:5px;align-items:center;justify-content:start}.editBlock{width:198px;display:flex;align-items:center;justify-content:space-between;gap:4px}
input,select{height:34px;background:#000;color:#fff;border:1px solid #fff;border-radius:3px;font:18px 'Droid Sans Mono','Courier New',monospace;padding:2px 6px;min-width:0}
input{width:112px;text-align:right}.dateInput{width:198px;text-align:center}select{width:198px}.miniBtn{height:34px;width:39px;min-width:39px;background:#0019a8;color:#fff;border:1px solid #fff;border-radius:4px;font:bold 20px Arial;cursor:pointer;padding:0}
.readVal{width:250px;text-align:right;color:var(--yellow);overflow:hidden;text-overflow:ellipsis;grid-column:1 / span 2}
.selected{color:var(--yellow)}.dirty input,.dirty select{border-color:var(--yellow)}
.nav{position:absolute;width:110px;height:120px;border:2px solid #fff;border-radius:8px;background:var(--blue);color:#fff;font:bold 23px Arial;display:flex;align-items:center;justify-content:center;cursor:pointer;user-select:none}
.nav:active,.miniBtn:active{background:var(--white);color:#000}.nav[disabled],.miniBtn[disabled]{opacity:.35;cursor:default}
#up{left:20px;top:25px}#down{left:20px;top:187px}#enter{left:20px;top:350px}#exit{left:675px;top:350px}#fanToggle{left:675px;top:187px;display:none;flex-direction:column;gap:3px;font:20px 'Droid Sans Mono','Courier New',monospace;line-height:1.15;padding-left:26px}#fanToggle .fanDot{position:absolute;left:16px;top:49px;width:18px;height:18px;border:4px solid #000;border-radius:50%;background:#fff}#fanToggle .fanLine{display:block}#fanToggle:active .fanDot{background:#000!important;border-color:#000}
#hint{position:absolute;left:155px;top:380px;width:500px;height:60px;font-size:16px;color:var(--gray);line-height:1.35;white-space:pre-wrap;display:flex;align-items:center}
#message{position:absolute;left:155px;top:430px;width:500px;height:36px;font-size:16px;color:var(--yellow);overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
#lock{display:none;position:absolute;left:155px;top:126px;width:500px;padding:24px;border:2px solid var(--red);background:#180000;font-size:20px;line-height:1.5;text-align:center;z-index:10}
#confirm{display:none;position:absolute;left:155px;top:112px;width:500px;height:190px;border:2px solid var(--yellow);background:#000;padding:28px 24px;font-size:20px;line-height:1.45;text-align:center;z-index:11}
#confirm small{display:block;margin-top:24px;color:var(--gray);font-size:16px}
.page{position:absolute;left:0;top:0;width:500px;height:286px;overflow:hidden}
.infoPage{font-size:15px;line-height:1.42;color:#fff;overflow-y:auto;padding-right:8px;box-sizing:border-box}.infoPage .infoHead{color:var(--yellow);font-size:18px;margin-bottom:10px}.infoPage .infoGap{height:8px}.infoPage .infoMuted{color:var(--gray)}.infoPage .infoLicense{color:var(--cyan)}.infoPage a{color:var(--cyan);text-decoration:underline}
.statusPage{font-size:14px;color:#fff}.statusGrid{display:grid;grid-template-columns:1fr 1fr;column-gap:18px;row-gap:3px}.statusPair{display:grid;grid-template-columns:112px 1fr;align-items:baseline;min-height:25px}.statusLabel{color:var(--cyan);white-space:nowrap}.statusValue{text-align:right;white-space:nowrap}.qGreen{color:var(--green)}.qYellow{color:var(--yellow)}.qOrange{color:#ffa600}.qRed{color:var(--red)}.statusTotal{margin-top:7px;padding-top:8px;border-top:1px solid #00006b;display:flex;justify-content:space-between;align-items:baseline;font-size:20px}.statusNoData{margin-top:32px;color:var(--cyan);font-size:17px;line-height:1.6}.ethDiagBox{margin-top:5px;padding-top:5px;border-top:1px solid #00006b;font-size:12px;line-height:1.35;color:var(--gray);white-space:nowrap}.ethDiagBox b{color:var(--cyan);font-weight:normal}
.unit{color:var(--gray);font-size:16px;width:47px;text-align:left;overflow:hidden;text-overflow:ellipsis}
@media(max-width:700px){body{padding:0}}
</style></head><body>
<div id="wrap"><div id="screen">
<div id="title">Setup</div><div id="body"></div>
<button id="up" class="nav">UP</button><button id="down" class="nav">DOWN</button>
<button id="enter" class="nav">ENTER</button><button id="exit" class="nav">EXIT</button>
<button id="fanToggle" class="nav" type="button"><span id="fanDot" class="fanDot"></span><span class="fanLine">FAN</span><span id="fanState" class="fanLine">OFF</span></button>
<div id="hint"></div><div id="message"></div><div id="lock"></div><div id="confirm"></div>
</div></div>
<script>

const E=id=>document.getElementById(id), DE=0, EN=1;
let S=null,statusData=null,lang=DE,mode='menu',sel=0,rows=[],current=null,parents=[],dirty=false,pendingConfirm=null;let webControlUnlocked=false,webControlPw='';let setupPollBusy=false,setupPageActive=true,setupClosing=false,setupInitialLang=null,setupLanguageChanged=false;const setupParts=[null,null,null],waitMs=ms=>new Promise(r=>setTimeout(r,ms));function webControlLock(){webControlUnlocked=false;webControlPw=''}
const tx=(de,en)=>lang===EN?en:de;
const opts={
 period:[[62,'62 ms (16 Hz)'],[100,'100 ms (10 Hz)'],[120,'120 ms (8 Hz)'],[142,'142 ms (7 Hz)'],[180,'180 ms (5.5 Hz)'],[250,'250 ms (4 Hz)']],
 auto:[[0,'10 min'],[1,'15 min'],[2,'30 min'],[3,'60 min'],[4,'120 min'],[5,'180 min']],
 adcFilter:[[0,'Normal'],[1,'Auto'],[2,'Praezise|Precision']],
 sfocal:[[0,'Nur beim Start|Start only'],[1,'Synchron mit Auto-Cal|Sync auto-cal'],[2,'Alle 10 min|Every 10 min'],[3,'Alle 30 min|Every 30 min'],[4,'Alle 60 min|Every 60 min']],
 alarmMode:[[0,'Aus|Off'],[1,'Grenzwert|Limit'],[2,'Bereich|Range']], onoff:[[0,'Aus|Off'],[1,'Ein|On']],
 outInt:[[0,'ADC'],[1,'1 s'],[2,'10 s'],[3,'30 s'],[4,'60 s']], filter:[[0,'0 s'],[1,'1 s'],[2,'3 s'],[3,'5 s'],[4,'10 s']],
 rsMode:[[0,'Aus|Off'],[1,'PC Text|PC text'],[2,'PC CSV|PC CSV'],[3,'Durchfluss Eingang|Flow input'],[4,'Durchfluss Poll|Flow poll'],[5,'ALMEMO'],[6,'WinControl|WinControl']],
 baud:[[0,'9600'],[1,'19200'],[2,'38400'],[3,'57600'],[4,'115200']], rsOut:[[0,'Zyklisch|Cyclic'],[1,'Auf Anfrage|On request'],[2,'Beides|Both']],
 usb:[[0,'Kommandos|Commands'],[1,'CSV'],[2,'Debug']], sdWrite:[[0,'10 s'],[1,'60 s'],[2,'3 min'],[3,'5 min']],
 flow:[[0,'Aus|Off'],[1,'l/min'],[2,'l/s'],[3,'m3/h'],[4,'T-Ext']], almemoInt:[[1,'1 s'],[2,'10 s']], almemoRole:[[1,'Temperatur|Temperature'],[2,'Flow'],[3,'Druck|Pressure'],[4,'Allgemein|General']], winOut:[[0,'Aus|Off'],[1,'RS232-1'],[2,'RS232-2'],[3,'Ethernet']], winCycle:[[0,'1 s'],[1,'10 s'],[2,'20 s'],[3,'60 s']], language:[[0,'Deutsch'],[1,'English']], screenLayout:[[0,'Standard'],[1,'3 Werte|3 values']], headType:[[0,'STP-3001'],[1,'STP-3002'],[2,'STP-3003'],[3,'STP-3004']]
};
function ol(list){return list.map(x=>[x[0],x[1].split('|')[lang===EN?1:0]||x[1].split('|')[0]])}
function resize(){E('screen').style.transform='scale('+(E('wrap').clientWidth/800)+')'}
window.addEventListener('resize',resize);resize();
function msg(t,ok=true){E('message').style.color=ok?'var(--yellow)':'var(--red)';E('message').textContent=t||''}
function markDirty(){dirty=true;renderHint()}
function rootItems(){return [
 [tx('Regelparameter','Control parameters'),()=>openControl()],
 [tx('Statusinformationen','Status information'),()=>openStatusInfo()],
 [tx('Alarm','Alarm'),()=>openAlarm()],
 [tx('Lüfter','Fan'),()=>openFan()],
 [tx('Sensorkopf','Sensor head'),()=>openHeadMenu()],
 [tx('Geraete-Speicher','Device storage'),()=>openDeviceStorageMenu()],
 [tx('Kalibrierung','Calibration'),()=>openCalibrationMenu()],
 [tx('Uhr / Datum','Clock / date'),()=>confirmAction(tx('PC-Zeit auf das Gerät übertragen?','Transfer PC time to device?'),setPcTime,false)],
 [tx('Anzeige','Display'),()=>openDisplay()],
 [tx('Schnittstellen','Interfaces'),()=>openInterfaceMenu()],
 [tx('Sprache','Language'),()=>openLanguage()],
 [tx('Lizenzen / Marken','Licenses / Trademarks'),()=>openInfoLicense()],
 [tx('Zurück','Back'),()=>closeSetup()]
]}
function openMenu(title,items,keepParent=false){if(!keepParent&&current)parents.push(current);current={kind:'menu',title,items};mode='menu';sel=0;rows=[];dirty=false;render()}
function back(){if(mode==='confirm'){cancelConfirm();return}if(mode==='form'&&dirty){confirmAction(tx('Änderungen verwerfen?','Discard changes?'),()=>{dirty=false;backNow()},false);return}backNow()}
function backNow(){if(current&&current.controlEdit)webControlLock();if(parents.length){current=parents.pop();mode=current.kind;sel=0;dirty=false;render()}else{webControlLock();closeSetup()}}
function isRootMenu(){return mode==='menu'&&current&&current.root===true}
function fanColor(){if(!S||!S.control||!S.control.fanEnabled)return 'white';return S.control.fanMissing?'yellow':'green'}
function updateFanButton(){let b=E('fanToggle');if(!b)return;let show=isRootMenu();b.style.display=show?'flex':'none';if(!show)return;let on=!!(S&&S.control&&S.control.fanEnabled);E('fanState').textContent=on?'ON':'OFF';let c=fanColor();E('fanDot').style.background=c==='green'?'#00e000':(c==='yellow'?'#ffe600':'#fff');b.disabled=!!(S&&S.tftBusy)}
function page(title,name){parents.push(current);current={kind:'page',title,page:name};mode='page';sel=0;dirty=false;render()}
function qClass(q){q=Number(q)||0;return q>=80?'qGreen':(q>=60?'qYellow':(q>=40?'qOrange':'qRed'))}
function qText(q){return String(Number(q)||0).padStart(3,' ')+' %'}
function statusName(code){let de=['OPTIK OK','PRÜFEN','REINIGEN','LED HOCH','LED LIMIT','INSTABIL','DUNKEL','AUTO-CAL'],en=['OPTIC OK','CHECK','CLEAN','LED HIGH','LED LIMIT','UNSTABLE','DARK','AUTO-CAL'];let a=lang===EN?en:de;return a[Number(code)]||a[1]}
function statusPair(label,value,q){return '<div class="statusPair"><span class="statusLabel">'+label+'</span><span class="statusValue '+(q===undefined?'':qClass(q))+'">'+value+'</span></div>'}
function ethDiagHtml(d){if(!d||d.ethStageText===undefined)return '';let a=(Number(d.ethActiveMs)||0),max=(Number(d.ethMaxUs)||0)/1000,mw=(Number(d.ethMaxWriteUs)||0)/1000,ta=Number(d.timeoutLoopAgeMs)||0,te=Number(d.timeoutEthElapsedMs)||0;return '<div class="ethDiagBox"><div><b>WEB:</b> '+d.ethStageText+' '+d.ethRequestText+'  '+a+' ms</div><div><b>MAX:</b> '+max.toFixed(1)+' ms '+d.ethMaxRequestText+'  WRITE '+mw.toFixed(1)+' ms  ABORT '+(Number(d.ethAborts)||0)+'</div><div><b>TIMEOUT:</b> Loop '+ta+' ms  '+d.timeoutEthStageText+' '+d.timeoutEthRequestText+'  '+te+' ms</div><div><b>RESET:</b> 0x'+Number(d.resetRaw).toString(16).toUpperCase().padStart(8,'0')+'  CrashReport '+(d.crashReport?'JA':'NEIN')+'</div></div>'}
function renderStatusPage(){let b=E('body');b.innerHTML='';let p=document.createElement('div');p.className='page statusPage';if(!statusData){p.innerHTML='<div class="statusNoData">'+tx('Statusdaten werden geladen ...','Loading status data ...')+'</div>';b.appendChild(p);return}let d=statusData,diag=ethDiagHtml(d);if(!d.valid){p.innerHTML='<div class="statusNoData">'+tx('Noch keine Auto-Cal-Daten.<br>Bitte Freiheizen/Auto-Cal abwarten.','No auto-cal data yet.<br>Please wait for heat-clean/auto-cal.')+'</div>'+diag;b.appendChild(p);return}let h='';h+=statusPair(tx('LED-Reserve','LED reserve'),qText(d.led),d.led);h+=statusPair(tx('LED-Strom','LED current'),(Number(d.ledMa10)/10).toFixed(1)+' mA');h+=statusPair(tx('Zielwert','Target'),qText(d.target),d.target);h+=statusPair(tx('Restfehler','Residual'),String(d.restError).padStart(7,' '));h+=statusPair(tx('Stabilität','Stability'),qText(d.stability),d.stability);h+=statusPair(tx('Rauschen','Noise'),String(d.noisePp).padStart(7,' '));h+=statusPair(tx('Dunkelwert','Dark value'),qText(d.dark),d.dark);h+=statusPair(tx('Dunkel ADC2','Dark ADC2'),String(d.darkRaw).padStart(7,' '));h+=statusPair(tx('Auto-Cal-Zeit','Auto-cal time'),qText(d.time),d.time);h+=statusPair(tx('Dauer','Duration'),(Number(d.durationMs)/1000).toFixed(1)+' s');h+=statusPair(tx('Trockenref','Dry reference'),String(d.dryRef).padStart(8,' '));h+=statusPair('ADC2 netto',String(d.net).padStart(8,' '));h+=statusPair(tx('Brutto','Gross'),String(d.gross).padStart(8,' '));h+=statusPair(tx('Schritte','Steps'),d.coarse+' / '+d.fine);h+=statusPair('ADC2 '+tx('Ziel','target'),String(d.targetRaw).padStart(8,' '));h+=statusPair(tx('Schw. Wert','Min. value'),qText(d.min),d.min);p.innerHTML='<div class="statusGrid">'+h+'</div><div class="statusTotal '+qClass(d.total)+'"><span>'+tx('GESAMT','TOTAL')+': '+qText(d.total)+'</span><span>'+statusName(d.status)+'</span></div>'+diag;b.appendChild(p)}
function renderInfoPage(){let b=E('body');b.innerHTML='';let p=document.createElement('div');p.className='page infoPage';p.innerHTML='<div class="infoHead">'+tx('LIZENZEN / MARKEN','LICENSES / TRADEMARKS')+'</div><div>'+tx('TP-3000 TAUPUNKTSPIEGEL-FIRMWARE','TP-3000 DEW POINT MIRROR FIRMWARE')+'</div><div>Version 0.50.0 | '+tx('11.07.2026','2026-07-11')+'</div><div>Copyright (C) 2025/2026 S. Brachtl</div><div class="infoGap"></div><div>'+tx('Basiert auf LJ2000M T_2.06c von:','Based on LJ2000M T_2.06c by:')+'</div><div>&nbsp;&nbsp;Loftur E. Jonasson</div><div>&nbsp;&nbsp;J.G. Holstein</div><div class="infoGap"></div><div class="infoLicense">GNU General Public License Version 3 only</div><div>SPDX-License-Identifier: GPL-3.0-only</div><div>'+tx('Freie Software: Weitergabe und Änderung nach GNU GPLv3-only erlaubt.','Free software: redistribution and modification are permitted under GPLv3-only.')+'</div><div>'+tx('Komponenten Dritter behalten ihre Lizenzen.','Third-party components retain their licenses.')+'</div><div>'+tx('Dieses Programm kommt OHNE JEDE GEWÄHRLEISTUNG.','This program comes with ABSOLUTELY NO WARRANTY.')+'</div><div class="infoMuted">'+tx('Quellcode und vollständige Lizenz:','Source code and complete license:')+'</div><div><a href="https://github.com/DK6WT/chilled_mirror" target="_blank" rel="noopener">github.com/DK6WT/chilled_mirror</a></div><div class="infoMuted"><a href="https://github.com/DK6WT/chilled_mirror/blob/main/LICENSE" target="_blank" rel="noopener">LICENSE</a> | <a href="https://github.com/DK6WT/chilled_mirror/blob/main/THIRD_PARTY_NOTICES.md" target="_blank" rel="noopener">THIRD_PARTY_NOTICES.md</a></div><div class="infoGap"></div><div class="infoLicense">ALMEMO&reg; / WinControl</div><div>'+tx('ALMEMO ist eine von AHLBORN verwendete Produktbezeichnung und Marke. AMR WinControl (im Gerät kurz WinControl) wird von akrobit entwickelt und für ALMEMO-Systeme über AHLBORN vertrieben.','ALMEMO is a product name and trademark used by AHLBORN. AMR WinControl (shown as WinControl in the device) is developed by akrobit and distributed for ALMEMO systems through AHLBORN.')+'</div><div>'+tx('Die Bezeichnungen ALMEMO und WinControl werden ausschließlich zur sachlichen Beschreibung optionaler Schnittstellen verwendet: optionale serielle Anbindung an ALMEMO-Messgeräte und Messwertausgabe im ALMEMO-V6-Format zur Nutzung mit WinControl über RS232 oder Ethernet/TCP.','The ALMEMO and WinControl names are used solely to describe optional interfaces: optional serial connection to ALMEMO measuring instruments and measurement output in ALMEMO V6 format for use with WinControl over RS232 or Ethernet/TCP.')+'</div><div>'+tx('TP-3000 ist ein unabhängiges Projekt. Keine Verbindung, Unterstützung, Freigabe, Prüfung oder Zertifizierung durch AHLBORN oder akrobit.','TP-3000 is an independent project. No affiliation, support, endorsement, approval, testing or certification by AHLBORN or akrobit.')+'</div><div>'+tx('Es wird kein ALMEMO-, WinControl-, AHLBORN- oder akrobit-Logo verwendet. Die Beschreibung ist keine allgemeine Kompatibilitätszusage.','No ALMEMO, WinControl, AHLBORN or akrobit logo is used. This is not a general compatibility claim.')+'</div><div class="infoMuted">LICENSES/ALMEMO-TRADEMARK-NOTICE.txt</div>';b.appendChild(p)}
function renderPage(){if(current.page==='status')renderStatusPage();else renderInfoPage()}
function updateNavButtons(){let infoPage=mode==='page'&&current&&current.page==='info';let pageMode=mode==='page'&&!infoPage;E('up').disabled=pageMode;E('down').disabled=pageMode}
function render(){E('body').innerHTML='';E('confirm').style.display='none';E('title').textContent=isRootMenu()?tx('--- TAUPUNKTSPIEGEL SETUP ---','--- DEW POINT MIRROR SETUP ---'):(current?current.title:'Setup');if(mode==='menu')renderMenu();else if(mode==='form')renderForm();else if(mode==='page')renderPage();renderHint();applyLock();updateFanButton();updateNavButtons()}
function visibleWindow(n){let count=Math.min(5,n),mid=count===2?0:Math.floor(count/2),out=[];for(let i=0;i<count;i++)out.push((sel+i-mid+n)%n);return out}
function rowStart(count){if(count>3)return 0;let h=((count-1)*52)+48;return Math.max(0,Math.round((286-h)/2))}
function renderMenu(){let idxs=visibleWindow(current.items.length),start=rowStart(idxs.length);idxs.forEach((idx,i)=>{let d=document.createElement('div');d.className='menuRow'+(idx===sel?' selected':'');d.style.top=(start+i*52)+'px';d.innerHTML='<span class="menuArrow">'+(idx===sel?' -&gt;  ':'     ')+'</span><span class="menuText"></span>';d.querySelector('.menuText').textContent=current.items[idx][0];d.onclick=()=>{sel=idx;render()};E('body').appendChild(d)})}
function valText(v,f){if(f.type==='num')return (Number(v)/f.scale).toFixed(f.digits);if(f.type==='date')return formatDateYmd(v);return String(v)}
function parseLocale(s){return Number(String(s).trim().replace(',','.'))}
function clamp(n,a,b){return Math.max(a,Math.min(b,n))}
function pad2(n){return String(n).padStart(2,'0')}
function formatDateYmd(v){v=Number(v)||0;if(v<=0)return '';let y=Math.floor(v/10000),m=Math.floor((v%10000)/100),d=v%100;if(y<1900||m<1||m>12||d<1||d>31)return '';return pad2(d)+'.'+pad2(m)+'.'+String(y).padStart(4,'0')}
function daysInMonth(y,m){return new Date(y,m,0).getDate()}
function parseDateInput(s){s=String(s||'').trim();if(!s)return null;let m=s.match(/^(\d{1,2})[.](\d{1,2})[.](\d{4})$/);if(m){let d=Number(m[1]),mo=Number(m[2]),y=Number(m[3]);if(y<1900||y>2099||mo<1||mo>12||d<1||d>daysInMonth(y,mo))return null;return y*10000+mo*100+d}let digits=s.replace(/\D/g,'');if(digits.length===8){if(Number(digits.slice(0,4))>=1900){let y=Number(digits.slice(0,4)),mo=Number(digits.slice(4,6)),d=Number(digits.slice(6,8));if(y>=1900&&y<=2099&&mo>=1&&mo<=12&&d>=1&&d<=daysInMonth(y,mo))return y*10000+mo*100+d}else{let d=Number(digits.slice(0,2)),mo=Number(digits.slice(2,4)),y=Number(digits.slice(4,8));if(y>=1900&&y<=2099&&mo>=1&&mo<=12&&d>=1&&d<=daysInMonth(y,mo))return y*10000+mo*100+d}}return null}
function makeUnitSpan(text){let u=document.createElement('span');u.className='unit';u.textContent=text||'';return u}
function bindTextInput(q,idx,onCommit){q.onclick=e=>e.stopPropagation();q.onfocus=()=>{sel=idx;renderHint()};q.onkeydown=e=>{if(e.key==='Enter'){e.preventDefault();q.blur();setTimeout(saveForm,0)}else if(e.key==='Escape'){e.preventDefault();q.blur();back()}};q.onchange=()=>onCommit(q)}
function normFilter(i,f){i=Number(i);f=Number(f);if(i===0)return 0;if(i===1&&f>1)return 1;return f}
function normSdWrite(i,w){i=Number(i);w=Number(w);return Math.min(w,i===0?1:(i===1?2:3))}
function sdFilterOptions(i){i=Number(i);return i===0?opts.filter.slice(0,1):(i===1?opts.filter.slice(0,2):opts.filter.slice())}
function sdWriteOptions(i){i=Number(i);return i===0?opts.sdWrite.slice(0,2):(i===1?opts.sdWrite.slice(0,3):opts.sdWrite.slice())}
function renderForm(){let idxs=visibleWindow(current.fields.length),start=rowStart(idxs.length);idxs.forEach((idx,i)=>{let f=current.fields[idx],d=document.createElement('div');d.className='formRow'+(idx===sel?' selected':'')+(dirty?' dirty':'');d.style.top=(start+i*52)+'px';let a=document.createElement('span');a.className='formArrow';a.innerHTML=idx===sel?' -&gt;  ':'     ';let l=document.createElement('span');l.className='formLabel';l.textContent=f.label;let c=document.createElement('span');c.className='formCtrl';if(f.type==='read'){let r=document.createElement('span');r.className='readVal';r.textContent=f.value;c.appendChild(r)}else if(f.type==='select'){let q=document.createElement('select');ol(f.options).forEach(o=>{let z=document.createElement('option');z.value=o[0];z.textContent=o[1];q.appendChild(z)});q.value=f.value;q.onclick=e=>e.stopPropagation();q.onfocus=()=>{sel=idx;renderHint()};q.onchange=()=>{f.value=Number(q.value);dirty=true;if(typeof f.onChange==='function'){f.onChange(f.value);render()}else renderHint()};c.appendChild(q);c.appendChild(makeUnitSpan(f.unit||''))}else if(f.type==='date'){let q=document.createElement('input');q.type='text';q.inputMode='numeric';q.className='dateInput';q.placeholder='TT.MM.JJJJ';q.value=valText(f.value,f);bindTextInput(q,idx,(el)=>{let n=parseDateInput(el.value);if(n===null){el.value=valText(f.value,f);msg(tx('Ungültiges Datum (TT.MM.JJJJ)','Invalid date (DD.MM.YYYY)'),false);return}f.value=n;el.value=valText(f.value,f);markDirty()});c.appendChild(q);c.appendChild(makeUnitSpan(f.unit||''))}else{let wrap=document.createElement('span');wrap.className='editBlock';let m=document.createElement('button');m.className='miniBtn';m.textContent='−';m.onclick=e=>{e.stopPropagation();stepField(f,-1)};let q=document.createElement('input');q.type='text';q.inputMode='decimal';q.value=valText(f.value,f);bindTextInput(q,idx,(el)=>{let n=parseLocale(el.value);if(!Number.isFinite(n)){el.value=valText(f.value,f);msg(tx('Ungültige Eingabe','Invalid input'),false);return}f.value=Math.round(clamp(n*f.scale,f.min,f.max));el.value=valText(f.value,f);markDirty()});let p=document.createElement('button');p.className='miniBtn';p.textContent='+';p.onclick=e=>{e.stopPropagation();stepField(f,1)};wrap.append(m,q,p);c.append(wrap,makeUnitSpan(f.unit||''))}d.append(a,l,c);d.onclick=e=>{if(e.target===d||e.target===a||e.target===l){sel=idx;render()}};E('body').appendChild(d)})}
function stepField(f,dir){if(f.type==='select'){let a=ol(f.options),i=a.findIndex(x=>Number(x[0])===Number(f.value));i=(i+dir+a.length)%a.length;f.value=a[i][0]}else if(f.type==='num'){f.value=clamp(Number(f.value)+dir*f.step,f.min,f.max)}else{return}markDirty();render()}
function renderHint(){if(!current)return;if(mode==='menu')E('hint').textContent=tx('UP/DOWN: Auswahl   ENTER: Öffnen   EXIT: Zurück','UP/DOWN: Select   ENTER: Open   EXIT: Back');else if(mode==='page')E('hint').textContent=current.page==='info'?tx('UP/DOWN: 2 Zeilen   ENTER/EXIT: Zurück','UP/DOWN: 2 lines   ENTER/EXIT: Back'):tx('Nur Anzeige   ENTER/EXIT: Zurück zum Menü','Read only   ENTER/EXIT: Back to menu');else if(mode==='form'){if(current.readOnly)E('hint').textContent=tx('Nur Anzeige   ENTER/EXIT: Zurück','Read only   ENTER/EXIT: Back');else E('hint').textContent=(current.cal?tx('Tastatur oder +/-; ENTER: prüfen und speichern','Keyboard or +/-; ENTER: validate and save'):tx('Tastatur oder +/-; ENTER: speichern','Keyboard or +/-; ENTER: save'))+(dirty?tx('\nGeänderte Werte noch nicht gespeichert.','\nChanged values are not saved yet.'):'')}}
function nav(d){if(mode==='confirm')return;if(mode==='page'){if(current&&current.page==='info'){let p=document.querySelector('.infoPage');if(p)p.scrollTop+=d*43}return}if(mode==='menu'||mode==='form'){let n=mode==='menu'?current.items.length:current.fields.length;sel=(sel+d+n)%n;render()}}
function enter(){if(mode==='confirm'){if(pendingConfirm){let f=pendingConfirm;pendingConfirm=null;E('confirm').style.display='none';f()}return}if(S&&S.tftBusy){msg(tx('TFT-Setup ist aktiv','TFT setup is active'),false);return}if(mode==='menu'){current.items[sel][1]()}else if(mode==='page'){back()}else if(mode==='form'){if(current.readOnly)back();else saveForm()}}
function confirmAction(text,fn,cal=true){mode='confirm';pendingConfirm=fn;E('confirm').innerHTML=text+'<small>'+tx('ENTER = Ja     EXIT = Nein','ENTER = Yes     EXIT = No')+'</small>';E('confirm').style.display='block';if(cal)msg(tx('Kalibrierwerte werden erst nach Bestätigung gespeichert.','Calibration values are saved only after confirmation.'))}
function cancelConfirm(){pendingConfirm=null;mode=current.kind;E('confirm').style.display='none';render()}
async function api(url){msg(tx('Übertrage...','Transferring...'));let r=await fetch(url,{cache:'no-store'}),t=await r.text();if(!r.ok)throw new Error(t||'HTTP '+r.status);msg(t||'OK');dirty=false;setupParts.fill(null);await loadSettings(false);return t}
function q(o){return Object.keys(o).map(k=>encodeURIComponent(k)+'='+encodeURIComponent(o[k])).join('&')}
function pad5(v){return String(Math.max(0,Math.min(99999,Number(v)||0))).padStart(5,'0')}
function saveForm(){let formState=current,go=async()=>{current=formState;mode=formState.kind;try{await formState.save();render()}catch(e){render();msg(e.message||String(e),false)}};if(formState.cal)confirmAction(tx('Kalibrierwerte wirklich übernehmen?','Really apply calibration values?'),go,true);else go()}
function form(title,fields,save,cal=false,readOnly=false){parents.push(current);current={kind:'form',title,fields,save,cal,readOnly};mode='form';sel=0;dirty=false;render()}
function nf(label,value,scale,min,max,step,digits,unit=''){return{type:'num',label,value,scale,min,max,step,digits,unit}}
function df(label,value,unit=''){return{type:'date',label,value,unit}}
function sf(label,value,options,unit=''){return{type:'select',label,value,options,unit}}
function rf(label,value){return{type:'read',label,value}}
function openStatusInfo(){statusData=null;page('STATUS INFORMATION','status');loadStatusInfo()}
function openInfoLicense(){page(tx('LIZENZEN / MARKEN','LICENSES / TRADEMARKS'),'info')}
function controlSummaryItems(){let c=S.control;return [[tx('Status: gesperrt','Status: locked'),()=>msg(tx('Zum Aendern Werte aendern waehlen','Select edit values to change'))],['Peltier max: '+(Number(c.peltier)/1000).toFixed(3)+' A',()=>{}],['Kp/Ki/Kd: '+c.kp+' / '+(Number(c.ki10)/10).toFixed(1)+' / '+(Number(c.kd10)/10).toFixed(1),()=>{}],['Takt: '+c.period+' ms  '+tx('Luefter','Fan')+': '+c.fan+'%',()=>{}],[tx('Werte aendern','Edit values'),openControlPassword],[tx('Zurueck','Back'),back]]}
function openControl(){if(webControlUnlocked)openControlEdit();else openMenu(tx('Regelparameter','Control parameters'),controlSummaryItems(),false)}
function openControlPassword(){let f=nf(tx('Passwort','Password'),0,1,0,99999,1,0);form(tx('Passwort eingeben','Enter password'),[f],async()=>{let pw=pad5(f.value);await api('/setup-control-unlock?'+q({pw}));webControlUnlocked=true;webControlPw=pw;parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;openControlEdit()})}
function openControlEdit(){let c=S.control,fs=[nf('Peltier max',c.peltier,1000,0,4000,10,3,'A'),nf('Kp',c.kp,1,0,1000,1,0),nf('Ki',c.ki10,10,0,500,1,1),nf('Kd',c.kd10,10,0,500,1,1),sf(tx('Regeltakt','Control period'),c.period,opts.period),nf(tx('Optik-Ziel','Optical target'),c.optik10,10,800,990,1,1,'%'),nf(tx('Freiheizen','Heat-clean'),c.freeTemp,1,40,76,1,0,'C'),sf(tx('Auto-Cal Intervall','Auto-cal interval'),c.autoIdx,opts.auto),sf(tx('Messfilter','Measurement filter'),c.adcFilter,opts.adcFilter),sf('ADC1 SFOCAL',c.sfocal,opts.sfocal)];form(tx('Regelparameter','Control parameters'),fs,async()=>{await api('/setup-control?'+q({pw:webControlPw,peltier:fs[0].value,kp:fs[1].value,ki:fs[2].value,kd:fs[3].value,period:fs[4].value,optik:fs[5].value,free:fs[6].value,auto:fs[7].value,filter:fs[8].value,sfocal:fs[9].value}));webControlLock();parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;render()});current.controlEdit=true}
function openFan(){let f=nf(tx('Lüfter','Fan'),S.control.fan,1,50,100,1,0,'%');form(tx('Lüfter','Fan'),[f],()=>api('/setup-fan?'+q({v:f.value})))}
function openAlarm(){let a=S.alarm,fs=[sf(tx('Betriebsart','Mode'),a.mode,opts.alarmMode),nf(tx('Taupunkt min','Dew point min'),a.tauL10,10,-800,800,1,1,'C'),nf(tx('Taupunkt max','Dew point max'),a.tauH10,10,-800,800,1,1,'C'),nf(tx('rF min','RH min'),a.rhL10,10,0,1000,1,1,'%rH'),nf(tx('rF max','RH max'),a.rhH10,10,0,1000,1,1,'%rH'),sf('Buzzer',a.buzzer?1:0,opts.onoff)];form('Alarm',fs,()=>{if(fs[1].value>fs[2].value||fs[3].value>fs[4].value)throw new Error(tx('Min muss kleiner/gleich Max sein','Min must be less than or equal to max'));return api('/setup-alarm?'+q({mode:fs[0].value,tl:fs[1].value,th:fs[2].value,rl:fs[3].value,rh:fs[4].value,b:fs[5].value}))})}

function pad4(v){return String(Math.max(0,Math.min(9999,Number(v)||0))).padStart(4,'0')}
function stpText(v){return 'STP-'+pad4(v)}
function stpNum(t){let m=String(t||'STP-3001').match(/STP-?(\d{4})/);return m?Number(m[1]):3001}
function headName(t){if(typeof t==='string')return stpText(stpNum(t));let h=S.head||{};return h.typeText||stpText(stpNum(h.model||'STP-3001'))}
function headCurrentText(){let h=S.head||{typeText:'STP-3001',serial:0,device:'00000'};return headName(h.typeText)+' K'+pad5(h.serial)+'  G'+(h.device||'00000')}
function openHeadMenu(){let h=S.head||{typeText:'STP-3001',serial:0};openMenu(tx('Sensorkopf','Sensor head'),[[tx('Kopftyp','Head type')+': '+headName(h.typeText),openHeadCurrent],[tx('Kopf-Seriennummer','Head serial')+': K'+pad5(h.serial),openHeadCurrent],[tx('Kopfdaten Status','Head data status'),openHeadStatus],[tx('Kopfdaten laden','Load head data'),openHeadLoadTypes],[tx('Kopfdaten speichern','Save head data'),openHeadSave],[tx('Zurueck','Back'),back]],false)}
function openHeadCurrent(){let h=S.head||{typeText:'STP-3001',serial:0},fs=[nf(tx('Kopftyp STP-','Head type STP-'),stpNum(h.typeText),1,0,9999,1,0),nf(tx('Kopf-SN','Head SN'),h.serial,1,0,99999,1,0)];form(tx('Sensorkopf','Sensor head'),fs,async()=>{await api('/setup-head-current?'+q({t:stpText(fs[0].value),s:fs[1].value}));parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;openHeadMenu()})}
async function openHeadStatus(){try{let r=await fetch('/setup-head-status',{cache:'no-store'}),t=await r.text();if(!r.ok)throw new Error(t||('HTTP '+r.status));let a=t.split('\n');form(tx('Kopfdaten Status','Head data status'),[rf(tx('Aktuell','Current'),a[0]||''),rf('SD',a[1]||'')],null,false,true)}catch(e){msg(e.message||String(e),false)}}
async function openHeadLoadTypes(){try{let d=await fetchSetupJson('/setup-head-list.json');if(!d.types||!d.types.length){msg(tx('Keine Kopfdaten fuer dieses Geraet','No head data for this device'),false);return}let items=d.types.map(t=>[headName(t),()=>openHeadLoadSerials(t)]);items.push([tx('Zurueck','Back'),back]);openMenu(tx('Kopftyp von SD','Head type from SD'),items,false)}catch(e){msg(e.message||String(e),false)}}
async function openHeadLoadSerials(t){try{let d=await fetchSetupJson('/setup-head-list.json?type='+encodeURIComponent(t));if(!d.serials||!d.serials.length){msg(tx('Keine Kopf-SN fuer diesen Typ','No head serial for this type'),false);return}let items=d.serials.map(sn=>['K'+pad5(sn),()=>confirmAction(tx('Kopfdaten laden?','Load head data?')+'<br>'+headName(t)+' K'+pad5(sn),async()=>{await api('/setup-head-load?'+q({t, s:sn}));parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;render()},false)]);items.push([tx('Zurueck','Back'),back]);openMenu(headName(t)+' '+tx('Kopf-SN','Head SN'),items,false)}catch(e){msg(e.message||String(e),false)}}
function openHeadSave(){let h=S.head||{typeText:'STP-3001',serial:0},fs=[nf(tx('Kopftyp STP-','Head type STP-'),stpNum(h.typeText),1,0,9999,1,0),nf(tx('Kopf-SN','Head SN'),h.serial,1,0,99999,1,0),nf(tx('Passwort','Password'),0,1,0,99999,1,0)];form(tx('Kopfdaten speichern','Save head data'),fs,async()=>{await api('/setup-head-save?'+q({t:stpText(fs[0].value),s:fs[1].value,pw:pad5(fs[2].value)}));parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;render()},true)}
function openDeviceStorageMenu(){openMenu(tx('Geraete-Speicher','Device storage'),[[tx('Speicherstatus','Storage status'),openDeviceStorageStatus],[tx('Einstellungen laden','Load settings'),()=>openDeviceStorageAction('settings_load',tx('Einstellungen laden','Load settings'),tx('Gespeicherte Einstellungen laden','Load saved settings'))],[tx('Werksjustierung laden','Load factory calibration'),()=>openDeviceStorageAction('factory_load',tx('Werksjustierung laden','Load factory calibration'),tx('Grundgeraet-Justierung laden','Load base-unit calibration'))],[tx('Einstellungen speichern','Save settings'),()=>openDeviceStorageAction('settings_save',tx('Einstellungen speichern','Save settings'),tx('Aktuelle Einstellungen speichern','Save current settings'))],[tx('Werksjustierung speichern','Save factory calibration'),()=>openDeviceStorageAction('factory_save',tx('Werksjustierung speichern','Save factory calibration'),tx('Aktuelle Grundgeraet-Justierung speichern','Save current base-unit calibration'))],[tx('Zurueck','Back'),back]],false)}
async function openDeviceStorageStatus(){try{let r=await fetch('/setup-device-status',{cache:'no-store'}),t=await r.text();if(!r.ok)throw new Error(t||('HTTP '+r.status));let a=t.split('\n');form(tx('Speicherstatus','Storage status'),[rf(tx('Geraet','Device'),a[0]||''),rf(tx('Einstellungen','Settings'),a[1]||''),rf(tx('Werksjustierung','Factory cal'),a[2]||''),rf(tx('Hinweis','Note'),a[3]||'')],null,false,true)}catch(e){msg(e.message||String(e),false)}}
function openDeviceStorageAction(a,title,line){let needsPw=(a==='factory_save'),f=nf(tx('Passwort','Password'),0,1,0,99999,1,0),sn=nf(tx('Geraete-SN','Device SN'),Number((S.head&&S.head.device)||0),1,0,99999,1,0),fields=[rf(tx('Aktion','Action'),line)];if(needsPw){fields.push(f);fields.push(sn)}form(title,fields,async()=>{let p={a};if(needsPw){p.pw=pad5(f.value);p.sn=pad5(sn.value)}await api('/setup-device-storage?'+q(p));if(needsPw){S.head=S.head||{};S.head.device=pad5(sn.value)}webControlLock();parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;openDeviceStorageMenu()},true)}
function openCalibrationMenu(){openMenu(tx('Kalibrierung','Calibration'),[[tx('Pt100 R0','Pt100 R0'),openR0],[tx('Pt100 2-Punkt Spiegel','Pt100 2-point mirror'),()=>open2P(0)],[tx('Pt100 2-Punkt Umgebung','Pt100 2-point ambient'),()=>open2P(1)],[tx('Referenz-/Kanalwerte','Reference/channel values'),openRef],[tx('Taupunkt-Offset','Dew-point offset'),openOffset],[tx('Zurück','Back'),back]],false)}
function openR0(){let c=S.cal,fs=[nf(tx('Spiegel Pt100 R0','Mirror Pt100 R0'),c.r0m,10000,800000,1200000,1,4,'Ohm'),nf(tx('Umgebung Pt100 R0','Ambient Pt100 R0'),c.r0a,10000,800000,1200000,1,4,'Ohm')];form('Pt100 R0',fs,()=>api('/setup-r0?'+q({m:fs[0].value,a:fs[1].value})),true)}
function open2P(sensor){let a=sensor?S.cal.p2a:S.cal.p2m,fs=[nf(tx('Soll 1','Set 1'),a[0],1000,-30000,90000,1,3,'C'),nf(tx('Ist 1','Actual 1'),a[1],1000,-30000,90000,1,3,'C'),nf(tx('Soll 2','Set 2'),a[2],1000,-30000,90000,1,3,'C'),nf(tx('Ist 2','Actual 2'),a[3],1000,-30000,90000,1,3,'C')];form(sensor?tx('Umgebung Pt100 2-Punkt','Ambient Pt100 2-point'):tx('Spiegel Pt100 2-Punkt','Mirror Pt100 2-point'),fs,()=>api((sensor?'/setup-2pa?':'/setup-2pm?')+q({s1:fs[0].value,i1:fs[1].value,s2:fs[2].value,i2:fs[3].value})),true)}
function openRef(){let c=S.cal,fs=[df(tx('Kalibrierdatum','Calibration date'),c.refDate),nf('Ref Low',c.ref100,100000,6000000,11000000,1,5,'Ohm'),nf('Ref High',c.ref120,100000,11500000,15200000,1,5,'Ohm'),nf(tx('Korr A Low','Corr A Low'),c.a100,100000,-10000,10000,1,5,'Ohm'),nf(tx('Korr A High','Corr A High'),c.a120,100000,-10000,10000,1,5,'Ohm'),nf(tx('Korr B Low','Corr B Low'),c.b100,100000,-10000,10000,1,5,'Ohm'),nf(tx('Korr B High','Corr B High'),c.b120,100000,-10000,10000,1,5,'Ohm')];form(tx('Referenz-/Kanalwerte','Reference/channel values'),fs,()=>api('/setup-ref?'+q({date:fs[0].value,r100:fs[1].value,r120:fs[2].value,a100:fs[3].value,a120:fs[4].value,b100:fs[5].value,b120:fs[6].value})),true)}
function openOffset(){let f=nf(tx('Taupunkt-/Frostpunkt-Offset','Dew/frost-point offset'),S.cal.offset,1000,-2000,2000,1,3,'C');form(tx('Taupunkt-Offset','Dew-point offset'),[f],()=>api('/setup-offset?'+q({v:f.value})),true)}
function openInterfaceMenu(){openMenu(tx('Schnittstellen','Interfaces'),[[tx('Ausgabe allgemein','General output'),openGeneral],[tx('RS232-1','RS232-1'),()=>openRs(0)],[tx('RS232-2','RS232-2'),()=>openRs(1)],['USB',openUsb],[tx('SD-Karte','SD card'),openSd],[tx('Zusatzanzeige','Additional display'),openFlow],['ALMEMO',openAlmemo],[tx('WinControl-Ausgabe','WinControl output'),openWinControl],[tx('Ethernet Information','Ethernet information'),openEthInfo],[tx('Zurück','Back'),back]],false)}
function openGeneral(){let i=S.iface,fs=[sf(tx('Ausgabe Intervall','Output interval'),i.outInt,opts.outInt),sf(tx('Ausgabe Filter','Output filter'),i.outFilter,opts.filter),sf(tx('Diagnose Daten','Diagnostic data'),i.diag?1:0,opts.onoff)];form(tx('Ausgabe allgemein','General output'),fs,()=>{let oi=Number(fs[0].value),of=normFilter(oi,fs[1].value);fs[1].value=of;return api('/setup-general?'+q({i:oi,f:of,d:fs[2].value}))})}
function openRs(p){let r=S.iface.rs[p],fs=[sf(tx('Betriebsart','Mode'),r.mode,opts.rsMode),sf('Baud',r.baud,opts.baud),sf(tx('Ausgabe','Output'),r.output,opts.rsOut)];form('RS232-'+(p+1),fs,()=>api('/setup-rs'+(p+1)+'?'+q({m:fs[0].value,b:fs[1].value,o:fs[2].value})))}
function openUsb(){let f=sf('USB',S.iface.usb,opts.usb);form('USB',[f],()=>api('/setup-usb?'+q({v:f.value})))}
function openSd(){let i=S.iface,log=sf('Logging',i.sdLogging?1:0,opts.onoff),out=sf(tx('SD Ausgabe Intervall','SD output interval'),i.sdOutInt,opts.outInt),filter=sf(tx('SD Ausgabe Filter','SD output filter'),normFilter(i.sdOutInt,i.sdFilter),sdFilterOptions(i.sdOutInt)),write=sf(tx('SD Schreiben','SD write'),normSdWrite(i.sdOutInt,i.sdWrite),sdWriteOptions(i.sdOutInt)),diag=sf(tx('SD Diagnose Daten','SD diagnostic data'),i.sdDiag?1:0,opts.onoff),head=sf(tx('Kopfzeile','Header'),i.sdHeader?1:0,opts.onoff),fs=[log,out,filter,write,diag,head];out.onChange=()=>{let oi=Number(out.value);filter.options=sdFilterOptions(oi);filter.value=normFilter(oi,filter.value);write.options=sdWriteOptions(oi);write.value=normSdWrite(oi,write.value)};form(tx('SD-Karte','SD card'),fs,()=>{let oi=Number(out.value),of=normFilter(oi,filter.value),ow=normSdWrite(oi,write.value);filter.value=of;write.value=ow;return api('/setup-sd?'+q({log:log.value,i:oi,f:of,w:ow,d:diag.value,h:head.value}))})}
function openFlow(){let f=sf(tx('Zusatzanzeige','Additional display'),S.iface.flow,opts.flow);form(tx('Zusatzanzeige','Additional display'),[f],()=>api('/setup-flow?'+q({v:f.value})))}
function almemoIntText(v){return Number(v)===2?'10 s':'1 s'}
function openAlmemo(){let a=S.iface.almemo;openMenu('ALMEMO',[[tx('ALMEMO Kanal 1','ALMEMO channel 1'),()=>openAlmemoChan(0)],[tx('ALMEMO Kanal 2','ALMEMO channel 2'),()=>openAlmemoChan(1)],[tx('Messintervall','Interval')+': '+almemoIntText(a.interval),openAlmemoInterval],[tx('Seriellog','Serial trace')+': '+(a.trace?tx('Ein','On'):tx('Aus','Off')),openAlmemoTrace],[tx('Zurück','Back'),back]],false)}
function openAlmemoChan(i){let a=S.iface.almemo.ch[i],fs=[sf(tx('Aktiv','Active'),a.active?1:0,opts.onoff),nf(tx('Adresse','Address'),a.address,1,0,99,1,0),nf(tx('Messkanal','Channel'),a.channel,1,0,99,1,0),sf(tx('Rolle','Role'),a.role,opts.almemoRole),rf('Status',a.status),rf(tx('Wert','Value'),a.value)];form(tx('ALMEMO Kanal ','ALMEMO channel ')+(i+1),fs,()=>api('/setup-almemo?'+q({idx:i,en:fs[0].value,a:fs[1].value,c:fs[2].value,r:fs[3].value})))}
function openAlmemoInterval(){let a=S.iface.almemo,f=sf(tx('Messintervall','Interval'),a.interval,opts.almemoInt);form(tx('ALMEMO Intervall','ALMEMO interval'),[f],()=>api('/setup-almemo?'+q({i:f.value})))}
function openAlmemoTrace(){let a=S.iface.almemo,f=sf(tx('Seriellog','Serial trace'),a.trace?1:0,opts.onoff);form(tx('ALMEMO Seriellog','ALMEMO serial trace'),[f],()=>api('/setup-almemo?'+q({trace:f.value})))}
function winOutText(v){let o=opts.winOut.find(x=>x[0]===Number(v));return o?ol([o])[0][1]:'?'}
function winCycleText(v){let o=opts.winCycle.find(x=>x[0]===Number(v));return o?ol([o])[0][1]:'?'}
function openWinControl(){let w=S.iface.win||{out:0,baud:0,address:0,port:10001,cycle:2,client:false};openMenu(tx('WinControl-Ausgabe','WinControl output'),[[tx('Ausgabe','Output')+': '+winOutText(w.out),openWinControlForm],[tx('Baudrate','Baudrate')+': '+(ol(opts.baud).find(x=>x[0]===Number(w.baud))||['','?'])[1],openWinControlForm],[tx('Adresse','Address')+': '+String(w.address).padStart(2,'0'),openWinControlForm],['TCP-Port: '+w.port,openWinControlForm],[tx('Zyklus','Cycle')+': '+winCycleText(w.cycle),openWinControlForm],['Status',openWinControlStatus],[tx('Zurück','Back'),back]],false)}
function openWinControlForm(){let w=S.iface.win||{out:0,baud:0,address:0,port:10001,cycle:2},fs=[sf(tx('Ausgabe','Output'),w.out,opts.winOut),sf('Baud',w.baud,opts.baud),nf(tx('Adresse','Address'),w.address,1,0,99,1,0),nf('TCP-Port',w.port,1,1,65535,1,0),sf(tx('Zyklus','Cycle'),w.cycle,opts.winCycle)];form(tx('WinControl-Ausgabe','WinControl output'),fs,()=>api('/setup-wincontrol?'+q({out:fs[0].value,b:fs[1].value,a:fs[2].value,p:fs[3].value,c:fs[4].value})))}
function openWinControlStatus(){let w=S.iface.win||{},rs=w.rsPort>=0?'RS232-'+(Number(w.rsPort)+1):tx('Aus','Off'),eth=w.eth?tx('Ein','On'):tx('Aus','Off'),cl=w.client?tx('Verbunden','Connected'):'--';let fs=[rf(tx('Ausgabe','Output'),winOutText(w.out||0)),rf('RS232',rs),rf('Ethernet',eth+' / '+(w.port||10001)),rf('TCP',cl),rf(tx('Adresse','Address'),'G'+String(w.address||0).padStart(2,'0'))];form(tx('WinControl Status','WinControl status'),fs,null)}
function openEthInfo(){let e=S.eth,fs=[rf('Status',e.ok?'OK':'---'),rf('Name',e.host||'--'),rf('DHCP',e.dhcp?tx('Ein','On'):tx('Aus','Off')),rf('IP',e.ip),rf('Web Port',e.webPort)];form(tx('Ethernet Information','Ethernet information'),fs,async()=>{},false,true)}
function openDisplay(){let u=S.ui||{mainLayout:0},f=sf(tx('Hauptscreen Layout','Main screen layout'),u.mainLayout,opts.screenLayout);form(tx('Anzeige','Display'),[f],()=>api('/setup-display?'+q({layout:f.value})))}
function openLanguage(){let f=sf(tx('Sprache','Language'),S.lang,opts.language);form(tx('Sprache','Language'),[f],async()=>{await api('/setup-language?'+q({v:f.value}));lang=Number(f.value);parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;render()})}
async function setPcTime(){let n=new Date(),u='/set-time?'+q({y:n.getFullYear(),mo:n.getMonth()+1,d:n.getDate(),h:n.getHours(),mi:n.getMinutes(),s:n.getSeconds()});try{await api(u)}catch(e){msg(e.message||String(e),false)}mode=current.kind;render()}
function applyLock(){let l=E('lock'),locked=S&&S.tftBusy;l.style.display=locked?'block':'none';if(locked)l.textContent=tx('Setup am Display ist aktiv. Web-Einstellungen sind bis zum Verlassen des TFT-Setups gesperrt.','Setup is active on the display. Web settings are locked until TFT setup is closed.');E('enter').disabled=!!locked;E('fanToggle').disabled=!!locked;document.querySelectorAll('#body input,#body select,#body button').forEach(x=>x.disabled=!!locked)}
async function fetchSetupJson(url){let r=await fetch(url,{cache:'no-store'});if(!r.ok)throw new Error((await r.text())||('HTTP '+r.status));return r.json()}
async function loadSetupPart(part){let delays=[0,250,500,1500],attempt=0;while(setupPageActive&&!setupClosing){try{let d=await fetchSetupJson('/setup.json?part='+part);if(Number(d.part)!==part)throw new Error('Setup part mismatch');delete d.part;setupParts[part]=d;return d}catch(e){msg(tx('Verbindung zum Gerät wird hergestellt ...','Connecting to device ...'));let delay=delays[Math.min(attempt,delays.length-1)];attempt++;if(delay)await waitMs(delay)}}throw new Error('Setup closed')}
async function loadSettings(redraw=true){for(let part=0;part<3;part++)await loadSetupPart(part);S=Object.assign({},setupParts[0],setupParts[1],setupParts[2]);lang=Number(S.lang)||0;if(setupInitialLang===null)setupInitialLang=lang;setupLanguageChanged=lang!==setupInitialLang;if(redraw&&current)render();applyLock()}
async function loadSetupLive(){if(!S)return;let d=await fetchSetupJson('/setup-live.json'),oldLang=lang;S.tftBusy=!!d.tftBusy;if(S.control){S.control.fan=Number(d.fan)||0;S.control.fanEnabled=!!d.fanEnabled;S.control.fanMissing=!!d.fanMissing}if(S.iface){S.iface.sdLogging=!!d.sdLogging;if(S.iface.win){S.iface.win.client=!!d.winClient;S.iface.win.activePort=Number(d.winPort)||0}}lang=Number(d.lang)||0;applyLock();updateFanButton();if(oldLang!==lang&&current)render()}
async function loadStatusInfo(){if(mode!=='page'||!current||current.page!=='status')return;try{let d0=await fetchSetupJson('/setup-status.json?part=0');delete d0.part;statusData=Object.assign({},statusData||{},d0);renderStatusPage();if(mode!=='page'||!current||current.page!=='status')return;let d1=await fetchSetupJson('/setup-status.json?part=1');delete d1.part;statusData=Object.assign({},statusData||{},d1);renderStatusPage()}catch(e){msg(e.message||String(e),false)}}
async function closeSetup(){if(setupClosing)return;setupClosing=true;setupPageActive=false;let closed=false;try{let c=new AbortController(),t=setTimeout(()=>c.abort(),1000),r=await fetch('/setup-close',{cache:'no-store',signal:c.signal});clearTimeout(t);closed=r.ok}catch(e){}if(!closed){location.href='/setup-close?redirect=1';return}if(setupLanguageChanged){location.replace('/');return}let sameOriginRef=false;try{sameOriginRef=!!document.referrer&&new URL(document.referrer).origin===location.origin}catch(e){}if(sameOriginRef&&history.length>1){history.back();setTimeout(()=>{if(document.visibilityState==='visible')location.replace('/')},1000)}else location.replace('/')}
async function toggleFan(){if(!isRootMenu()||(S&&S.tftBusy))return;try{await api('/setup-fan-toggle');updateFanButton()}catch(e){msg(e.message||String(e),false)}}
E('up').onclick=()=>nav(-1);E('down').onclick=()=>nav(1);E('enter').onclick=enter;E('exit').onclick=back;E('fanToggle').onclick=toggleFan;
window.addEventListener('keydown',e=>{if(e.target&&['INPUT','SELECT'].includes(e.target.tagName)){if(e.key==='Escape'){e.preventDefault();e.target.blur();back()}return}if(e.key==='ArrowUp'){e.preventDefault();nav(-1)}else if(e.key==='ArrowDown'){e.preventDefault();nav(1)}else if(e.key==='Enter'){e.preventDefault();enter()}else if(e.key==='Escape'){e.preventDefault();back()}});
async function setupPoll(){if(setupPollBusy||!setupPageActive||setupClosing||document.hidden)return;setupPollBusy=true;try{await loadSetupLive();await loadStatusInfo()}catch(e){}finally{setupPollBusy=false}}
window.addEventListener('pagehide',()=>{setupPageActive=false});window.addEventListener('pageshow',()=>{setupPageActive=true;setupPoll()});
(async()=>{try{msg(tx('Verbindung zum Gerät wird hergestellt ...','Connecting to device ...'));await loadSettings(false);current={kind:'menu',title:'Setup',items:rootItems(),root:true};msg('');render();setInterval(setupPoll,1500)}catch(e){if(!setupClosing)msg(e.message||String(e),false)}})();
</script></body></html>
)TPSETUP";

// Reine Web-Service-Seite fuer CSV- und ALMEMO-Seriellog-Dateien. Eine
// aktive Datei kann als fester Snapshot bis zu ihrer beim Downloadstart
// vorhandenen Groesse geladen
// werden. Die Seite startet keine
// Messwert- oder Chart-Polls. Die eigentliche Dateiuebertragung laeuft danach
// im Hauptloop in genau einem 1024-Byte-Schritt pro Schleifendurchlauf weiter.
static const char ethDownloadPage[] PROGMEM = R"TPDOWNLOAD(
<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TP-3000 Log-Dateien</title>
<style>
*{box-sizing:border-box}html,body{margin:0;min-height:100%;background:#111;color:#eee;font-family:Arial,sans-serif}
body{display:flex;justify-content:center;padding:18px}.wrap{width:min(100%,1100px)}
h1{font-family:'Droid Sans Mono','Courier New',monospace;font-size:26px;font-weight:normal;margin:0 0 14px;color:#fff}
.toolbar,.notice,.panel,.pager{background:#1b1b1b;border:1px solid #3a3a3a;border-radius:8px;padding:12px;margin-bottom:12px}
.toolbar{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.toolbar a,.toolbar button,.filter button,.pager button,.dl{background:#303030;color:#fff;border:1px solid #666;border-radius:6px;padding:8px 12px;text-decoration:none;cursor:pointer;font:inherit}.toolbar a:hover,.toolbar button:hover,.filter button:hover,.pager button:hover,.dl:hover{background:#444}.filter{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.filter span[id$="Buttons"]{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.filter button.selected{background:#555;border-color:#ddd}.filter button:disabled,.pager button:disabled{opacity:.4;cursor:default}.notice{line-height:1.45;border-color:#856800;color:#fff3a0}.status{margin-left:auto;color:#bbb}
table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:10px 8px;border-bottom:1px solid #333}th{color:#bbb;font-weight:normal}td.date{white-space:nowrap;color:#bbb;font-variant-numeric:tabular-nums}td.size{text-align:right;font-variant-numeric:tabular-nums}td.action{text-align:right;width:130px}.active{color:#ffe600}.fault{color:#ff4545;font-weight:bold}.faultRow td:first-child{color:#ff4545}.muted{color:#888}.empty{padding:24px 8px;color:#bbb}.snapshot{border-color:#856800;color:#fff3a0}.dl.disabled{opacity:.45;pointer-events:none}.pager{display:flex;gap:8px;align-items:center;justify-content:center}.pageText{min-width:140px;text-align:center;color:#bbb}
@media(max-width:760px){body{padding:8px}h1{font-size:21px}th:nth-child(2),td:nth-child(2),th:nth-child(3),td:nth-child(3){display:none}.status{width:100%;margin-left:0}.pager{flex-wrap:wrap}.pageText{order:-1;width:100%}}
</style></head><body><div class="wrap">
<h1 id="pageTitle">TP-3000 Log-Dateien</h1>
<div class="toolbar"><a id="homeLink" href="/">Zur Hauptanzeige</a><button id="reload">Aktualisieren</button><span id="status" class="status">Dateiliste wird aufgebaut ...</span></div>
<div class="toolbar filter"><span id="filterLabel">Filter:</span><button id="filterAll" data-filter="all">Alle</button><button id="filterCsv" data-filter="csv" class="selected">Messdaten</button><button id="filterSerial" data-filter="serial">Seriellog</button></div>
<div id="yearFilter" class="toolbar filter"><span id="yearLabel">Jahr:</span><span id="yearButtons"></span></div>
<div id="monthFilter" class="toolbar filter"><span id="monthLabel">Monat:</span><span id="monthButtons"></span></div>
<div id="notice" class="notice"><b>Messung, Regelung und Safety bleiben aktiv.</b> Während des Dateidownloads werden keine weiteren Webanfragen bearbeitet. SD- und Seriellogging haben weiterhin Vorrang.<br>Bei einer aktiven Datei lädt <b>Snapshot</b> genau den beim Start bereits auf der SD-Karte vorhandenen Stand. Später angehängte und noch im RAM gepufferte Messwerte sind nicht enthalten.</div>
<div class="panel"><table><thead><tr><th id="thFile">Datei</th><th id="thModified">Geändert</th><th id="thStatus">Status</th><th id="thSize" style="text-align:right">Größe</th><th></th></tr></thead><tbody id="rows"><tr><td colspan="5" class="empty">Dateien werden gelesen ...</td></tr></tbody></table></div>
<div class="pager"><button id="first">Erste</button><button id="prev">Zurück</button><span id="pageText" class="pageText">Seite 1 von 1</span><button id="next">Weiter</button><button id="last">Letzte</button></div>
<iframe id="downloadFrame" name="downloadFrame" title="Download" style="display:none"></iframe>
<script>
const E=id=>document.getElementById(id);let lang=0,filter='csv',year=0,month=0,page=0,pages=1,total=0,loading=false,loadQueued=false,retryTimer=0,refreshPending=true,faultSeq=0,pendingDownload=null,downloadCheckBusy=false,downloadCheckTimer=0;
const tx=(de,en)=>lang===1?en:de;
const esc=t=>String(t).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const sizeText=n=>{n=Number(n)||0;if(n>=1073741824)return(n/1073741824).toFixed(2)+' GB';if(n>=1048576)return(n/1048576).toFixed(2)+' MB';if(n>=1024)return(n/1024).toFixed(1)+' kB';return n+' B'};
const monthNamesDE=['Jan','Feb','Mär','Apr','Mai','Jun','Jul','Aug','Sep','Okt','Nov','Dez'],monthNamesEN=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
const monthName=m=>{m=Number(m)||0;return m>=1&&m<=12?(lang===1?monthNamesEN[m-1]:monthNamesDE[m-1]):''};
const applyLanguage=()=>{document.documentElement.lang=lang===1?'en':'de';document.title=tx('TP-3000 Log-Dateien','TP-3000 log files');E('pageTitle').textContent=tx('TP-3000 Log-Dateien','TP-3000 log files');E('homeLink').textContent=tx('Zur Hauptanzeige','Back to main display');E('reload').textContent=tx('Aktualisieren','Refresh');E('filterLabel').textContent=tx('Filter:','Filter:');E('filterAll').textContent=tx('Alle','All');E('filterCsv').textContent=tx('Messdaten','Measurement data');E('filterSerial').textContent=tx('Seriellog','Serial log');E('yearLabel').textContent=tx('Jahr:','Year:');E('monthLabel').textContent=tx('Monat:','Month:');E('notice').innerHTML='<b>'+tx('Messung, Regelung und Safety bleiben aktiv.','Measurement, control and safety remain active.')+'</b> '+tx('Während des Dateidownloads werden keine weiteren Webanfragen bearbeitet. SD- und Seriellogging haben weiterhin Vorrang.','No further web requests are processed during a file download. SD and serial logging retain priority.')+'<br>'+tx('Bei einer aktiven Datei lädt','For an active file,')+' <b>Snapshot</b> '+tx('genau den beim Start bereits auf der SD-Karte vorhandenen Stand. Später angehängte und noch im RAM gepufferte Messwerte sind nicht enthalten.','downloads exactly the data already present on the SD card when the download starts. Values appended later and values still buffered in RAM are not included.');E('thFile').textContent=tx('Datei','File');E('thModified').textContent=tx('Geändert','Modified');E('thStatus').textContent=tx('Status','Status');E('thSize').textContent=tx('Größe','Size');E('first').textContent=tx('Erste','First');E('prev').textContent=tx('Zurück','Previous');E('next').textContent=tx('Weiter','Next');E('last').textContent=tx('Letzte','Last');E('downloadFrame').title=tx('Download','Download')};
const setLanguageFromData=d=>{let next=Number(d&&d.lang)===1?1:0;if(next!==lang){lang=next;applyLanguage()}else if(!document.documentElement.lang)applyLanguage()};
const setFilterButtons=()=>document.querySelectorAll('[data-filter]').forEach(b=>b.classList.toggle('selected',b.dataset.filter===filter));
const btn=(label,sel,click)=>'<button class="'+(sel?'selected':'')+'" type="button" onclick="'+click+'">'+esc(label)+'</button>';
const renderDateFilters=d=>{let ys=Array.isArray(d.years)?d.years.map(Number).filter(x=>x>=2020&&x<=2099):[],ms=Array.isArray(d.months)?d.months.map(Number).filter(x=>x>=1&&x<=12):[];year=Number(d.year)||0;month=Number(d.month)||0;if(!ys.length){year=0;E('yearFilter').style.display='none'}else{E('yearFilter').style.display='flex';if(ys.length===1)year=ys[0];else if(!ys.includes(year))year=0;let h=ys.length>1?btn(tx('Alle','All'),year===0,"pickYear(0)"):' ';ys.forEach(y=>h+=btn(String(y),year===y,"pickYear("+y+")"));E('yearButtons').innerHTML=h}if(!ms.length){month=0;E('monthFilter').style.display='none'}else{E('monthFilter').style.display='flex';if(ms.length===1)month=ms[0];else if(!ms.includes(month))month=0;let h=ms.length>1?btn(tx('Alle','All'),month===0,"pickMonth(0)"):' ';ms.forEach(m=>h+=btn(monthName(m),month===m,"pickMonth("+m+")"));E('monthButtons').innerHTML=h}};
window.pickYear=y=>{year=Number(y)||0;month=0;page=0;load()};window.pickMonth=m=>{month=Number(m)||0;page=0;load()};
const setPager=()=>{E('pageText').textContent=tx('Seite ','Page ')+(pages?Number(page)+1:0)+tx(' von ',' of ')+pages;E('first').disabled=page<=0;E('prev').disabled=page<=0;E('next').disabled=page>=pages-1;E('last').disabled=page>=pages-1};
const render=d=>{let r=E('rows'),a=Array.isArray(d.files)?d.files:[];if(!a.length){r.innerHTML='<tr><td colspan="5" class="empty">'+tx('Keine passenden Dateien im Verzeichnis /LOG gefunden.','No matching files found in /LOG.')+'</td></tr>'}else r.innerHTML=a.map(f=>{let st=f.fault?tx('Lesefehler','Read error'):(f.active?tx('wird beschrieben','being written'):tx('geschlossen','closed')),sc=f.fault?'fault':(f.active?'active':'muted'),ac=f.fault?'<span class="fault">'+tx('Gesperrt','Locked')+'</span>':(f.active?'<a class="dl snapshot" target="downloadFrame" data-snapshot="1" data-name="'+esc(f.name)+'" href="/download-file?name='+encodeURIComponent(f.name)+'&snapshot=1&limit='+encodeURIComponent(String(Number(f.size)||0))+'" onclick="downloadStarted(this)">Snapshot</a>':'<a class="dl" target="downloadFrame" data-name="'+esc(f.name)+'" href="/download-file?name='+encodeURIComponent(f.name)+'" onclick="downloadStarted(this)">'+tx('Download','Download')+'</a>');return '<tr class="'+(f.fault?'faultRow':'')+'"><td>'+esc(f.name)+'</td><td class="date">'+esc(f.modified||'---')+'</td><td class="'+sc+'">'+st+'</td><td class="size">'+sizeText(f.size)+'</td><td class="action">'+ac+'</td></tr>'}).join('');setPager()};
const applyListData=d=>{setLanguageFromData(d);renderDateFilters(d);page=Number(d.page)||0;pages=Math.max(1,Number(d.pages)||1);total=Number(d.total)||0;render(d);E('status').textContent=total+' '+(total===1?tx('Datei','file'):tx('Dateien','files'))+', '+tx('neueste zuerst','newest first')+(d.truncated?tx(' – Index auf die neuesten Einträge begrenzt',' – index limited to the newest entries'):'')};
const fileQuery=extra=>'/files.json?filter='+filter+'&page='+page+(year?'&year='+year:'')+(month?'&month='+month:'')+extra+'&_='+Date.now();
const checkDownloadResult=async()=>{if(!pendingDownload||downloadCheckBusy)return;downloadCheckBusy=true;let retry=false;try{let c=new AbortController(),t=setTimeout(()=>c.abort(),2500),u=fileQuery(''),r=await fetch(u,{cache:'no-store',headers:{'Cache-Control':'no-cache'},signal:c.signal});clearTimeout(t);if(!r.ok)throw new Error(await r.text()||('HTTP '+r.status));let d=await r.json();setLanguageFromData(d);if(d.indexing){retry=true}else{let seq=Number(d.faultSeq)||0,name=String(d.faultName||''),same=seq>pendingDownload.seq&&name.toUpperCase()===pendingDownload.name.toUpperCase(),active=!!d.downloadActive,activeName=String(d.downloadName||'');if(same){let m=tx('Dateilesefehler: ','File read error: ')+name+'\n\n';m+=d.faultPeltierCut?tx('Die Peltierregelung wurde kurz unterbrochen und anschließend automatisch fortgesetzt.\nEs wurde keine Safety ausgelöst.','The Peltier control loop was briefly interrupted and then resumed automatically.\nNo safety was triggered.'):tx('Die Peltierregelung blieb aktiv.\nEs wurde keine Safety ausgelöst.','The Peltier control remained active.\nNo safety was triggered.');m+='\n\n'+tx('Die Datei wurde rot markiert und für weitere Downloads gesperrt.','The file was marked in red and locked for further downloads.');alert(m);faultSeq=seq;pendingDownload=null;applyListData(d)}else if(active&&activeName.toUpperCase()===pendingDownload.name.toUpperCase()){retry=true}else{faultSeq=Math.max(faultSeq,seq);pendingDownload=null;document.querySelectorAll('.dl').forEach(x=>x.classList.remove('disabled'));applyListData(d)}}}catch(e){retry=true}finally{downloadCheckBusy=false;if(retry&&pendingDownload)downloadCheckTimer=setTimeout(checkDownloadResult,700)}};
window.downloadStarted=a=>{let u=new URL(a.href,location.href);u.searchParams.set('_',String(Date.now()));a.href=u.pathname+u.search;E('status').textContent=a.dataset.snapshot==='1'?tx('Snapshot wurde gestartet. Regelung und Logging bleiben aktiv.','Snapshot started. Control and logging remain active.'):tx('Download wurde gestartet. Regelung bleibt aktiv.','Download started. Control remains active.');document.querySelectorAll('.dl').forEach(x=>{if(x!==a)x.classList.add('disabled')});pendingDownload={name:String(a.dataset.name||''),seq:faultSeq};clearTimeout(downloadCheckTimer);downloadCheckTimer=setTimeout(checkDownloadResult,900)};
const requestPage=async()=>{let u=fileQuery(refreshPending?'&refresh=1':'');refreshPending=false;let r=await fetch(u,{cache:'no-store',headers:{'Cache-Control':'no-cache'}});if(!r.ok)throw new Error(await r.text()||('HTTP '+r.status));let d=await r.json();setLanguageFromData(d);if(!d.ready)throw new Error(d.message||tx('SD-Karte nicht bereit','SD card not ready'));if(d.indexing){E('status').textContent=tx('Dateiliste wird aufgebaut: ','Building file list: ')+(Number(d.scanned)||0)+tx(' Einträge geprüft ...',' entries checked ...');retryTimer=setTimeout(load,120);return false}faultSeq=Math.max(faultSeq,Number(d.faultSeq)||0);applyListData(d);return true};
const load=async()=>{if(loading){loadQueued=true;return}loading=true;clearTimeout(retryTimer);try{await requestPage()}catch(e){E('status').textContent=tx('Fehler: ','Error: ')+(e.message||String(e));retryTimer=setTimeout(load,1000)}finally{loading=false;if(loadQueued){loadQueued=false;load()}}};
const changePage=n=>{let v=Math.max(0,Math.min(pages-1,n));if(v===page)return;page=v;load()};
document.querySelectorAll('[data-filter]').forEach(b=>b.onclick=()=>{filter=b.dataset.filter;year=0;month=0;page=0;setFilterButtons();load()});
E('reload').onclick=()=>{page=0;refreshPending=true;E('rows').innerHTML='<tr><td colspan="5" class="empty">'+tx('Dateien werden neu eingelesen ...','Reloading files ...')+'</td></tr>';load()};E('first').onclick=()=>changePage(0);E('prev').onclick=()=>changePage(page-1);E('next').onclick=()=>changePage(page+1);E('last').onclick=()=>changePage(pages-1);window.addEventListener('pageshow',()=>{refreshPending=true;load()});applyLanguage();setFilterButtons();setPager();
</script></div></body></html>
)TPDOWNLOAD";

static bool FLASHMEM ethPathEquals(const char* requestLine, const char* path);


// Jede HTTP-Antwort hat ein hartes Zeitbudget deutlich unterhalb des
// 1-s-Safety-Timeouts. NativeEthernet stellt availableForWrite() bereit;
// geschrieben wird nur, wenn der TCP-Sendepuffer freien Platz meldet.
// Ein langsamer oder abgebrochener Browser kann dadurch nicht mehr die
// Mess-/Regel-/Display-Schleife minutenlang festhalten.
static const uint32_t ETH_HTTP_RESPONSE_BUDGET_MS = 120UL;
static const size_t   ETH_HTTP_WRITE_CHUNK_BYTES  = 512U;
static const uint32_t ETH_HTTP_DRAIN_TIMEOUT_MS   = 250UL;
static const uint16_t ETH_HTTP_FALLBACK_SEND_BUFFER_BYTES = 2048U;
static volatile uint32_t ethDiagAbortedResponses = 0;

// Kleiner RAM-Writer fuer vollstaendige JSON-Antworten. Dadurch kann vor dem
// ersten TCP-Byte eine exakte Content-Length gesendet werden. Ein Ueberlauf
// wird erkannt; es wird niemals ein abgeschnittenes JSON ausgeliefert.
class EthMemoryWriter : public Print
{
public:
  EthMemoryWriter(char* target, size_t targetSize)
    : buffer(target), capacity(targetSize), used(0U), overflow(false)
  {
    if (buffer != nullptr && capacity > 0U) buffer[0] = '\0';
  }

  using Print::write;
  size_t write(uint8_t value) override;
  size_t write(const uint8_t* data, size_t length) override;
  bool ok() const { return !overflow; }
  size_t length() const { return used; }

private:
  char* buffer;
  size_t capacity;
  size_t used;
  bool overflow;
};

size_t FLASHMEM EthMemoryWriter::write(uint8_t value)
{
  return write(&value, 1U);
}

size_t FLASHMEM EthMemoryWriter::write(const uint8_t* data, size_t length)
{
  if (overflow || buffer == nullptr || capacity == 0U ||
      data == nullptr || length == 0U)
  {
    return 0U;
  }

  // Ein Byte bleibt fuer den Nullabschluss reserviert.
  if (used + length >= capacity)
  {
    overflow = true;
    return 0U;
  }

  memcpy(buffer + used, data, length);
  used += length;
  buffer[used] = '\0';
  return length;
}

class EthBoundedWriter : public Print
{
public:
  EthBoundedWriter(EthernetClient& rawClient, uint32_t requestStartMs)
    : client(rawClient), startedMs(requestStartMs), aborted(false)
  {
  }

  using Print::write;
  size_t write(uint8_t value) override;
  size_t write(const uint8_t* data, size_t length) override;
  bool ok() const { return !aborted; }

private:
  EthernetClient& client;
  uint32_t startedMs;
  bool aborted;
  void abortWrite();
};

static void ethDownloadIoBegin(uint8_t kind)
{
  noInterrupts();
  ethDownloadIoKind = kind;
  interrupts();
}

// Der Safety-Timer kann waehrend des blockierenden Aufrufs den Peltierpfad
// sicher abschalten und ethDownloadStallDetected setzen. Das Ende wird atomar
// quittiert, bevor die I/O-Kennung verschwindet; so kann kein normaler
// Peltier-Timeout zwischen Rueckkehr und Fehlerbehandlung einrasten.
static uint8_t ethDownloadIoEnd(void)
{
  noInterrupts();
  uint8_t stall = ethDownloadStallDetected;
  if (stall != ETH_DOWNLOAD_IO_NONE)
  {
    // Der Timer hat den Peltierpfad bereits sicher abgeschaltet, aber bewusst
    // keine allgemeine Safety gelatcht. Feed und Abschaltmerker direkt
    // atomar zuruecksetzen, damit die Regelung danach wieder uebernehmen kann.
    safetyLastFeedMs = millis();
    safetyPeltierCutDone = false;
  }
  ethDownloadStallDetected = ETH_DOWNLOAD_IO_NONE;
  ethDownloadIoKind = ETH_DOWNLOAD_IO_NONE;
  interrupts();
  return stall;
}

size_t FLASHMEM EthBoundedWriter::write(uint8_t value)
{
  return write(&value, 1U);
}

size_t FLASHMEM EthBoundedWriter::write(const uint8_t* data, size_t length)
{
  if (aborted || data == nullptr || length == 0U) return 0U;

  size_t total = 0U;
  while (total < length)
  {
    if ((uint32_t)(millis() - startedMs) >= ETH_HTTP_RESPONSE_BUDGET_MS)
    {
      abortWrite();
      break;
    }

    const bool downloadResponse = (ethDiagRequest == ETH_REQ_DOWNLOAD_FILE);
    if (downloadResponse) ethDownloadIoBegin(ETH_DOWNLOAD_IO_NETWORK);
    int available = client.availableForWrite();
    uint8_t ioStall = downloadResponse ? ethDownloadIoEnd() : ETH_DOWNLOAD_IO_NONE;
    if (ioStall != ETH_DOWNLOAD_IO_NONE)
    {
      abortWrite();
      break;
    }
    if (available <= 0)
    {
      // Kleine JSON-/Textantworten duerfen innerhalb des getesteten Budgets
      // warten. Die grossen HTML-Seiten verwenden diesen Writer nicht mehr.
      yield();
      continue;
    }

    size_t chunk = length - total;
    if (chunk > ETH_HTTP_WRITE_CHUNK_BYTES) chunk = ETH_HTTP_WRITE_CHUNK_BYTES;
    if (chunk > (size_t)available) chunk = (size_t)available;

    uint8_t previousStage = ethDiagStage;
    ethDiagStage = ETH_DIAG_WRITE;
    if (downloadResponse) ethDownloadIoBegin(ETH_DOWNLOAD_IO_NETWORK);
    uint32_t startedUs = micros();
    size_t written = client.write(data + total, chunk);
    uint32_t durationUs = (uint32_t)(micros() - startedUs);
    ioStall = downloadResponse ? ethDownloadIoEnd() : ETH_DOWNLOAD_IO_NONE;
    ethDiagLastWriteUs = durationUs;
    if (durationUs > ethDiagMaxWriteUs) ethDiagMaxWriteUs = durationUs;
    ethDiagStage = previousStage;

    if (ioStall != ETH_DOWNLOAD_IO_NONE)
    {
      abortWrite();
      break;
    }

    if (written == 0U)
    {
      yield();
      continue;
    }

    total += written;
  }

  if (total < length) abortWrite();
  return total;
}

void FLASHMEM EthBoundedWriter::abortWrite()
{
  if (aborted) return;
  aborted = true;
  ethDiagAbortedResponses++;
  setWriteError();
}

uint32_t FLASHMEM ethernetDiagGetAbortedResponses(void)
{
  return ethDiagAbortedResponses;
}

// ---------------------------------------------------------------------------
// Nicht blockierende HTML-Seitenuebertragung und Browser-Cache
// ---------------------------------------------------------------------------
// Die drei grossen HTML-Seiten werden nicht mehr in einem einzigen
// ethernetServiceTask()-Aufruf in den TCP-Puffer gedrueckt. Genau wie beim
// Dateidownload wird pro Hauptloop hoechstens ein 512-Byte-Stueck gesendet.
// Dadurch darf ein WLAN-Transfer insgesamt mehrere hundert Millisekunden
// dauern, ohne einen einzelnen Regel-/Safety-Loop entsprechend zu verlaengern.
static const size_t   ETH_PAGE_CHUNK_BYTES = 512U;
static const uint32_t ETH_PAGE_STALL_TIMEOUT_MS = 10000UL;
static const uint8_t  ETH_PAGE_MAX_SEGMENTS = 12U;
static const size_t   ETH_PAGE_HEADER_BYTES = 256U;

// Der Build-Teil des ETags wird bei jeder Web-Aenderung bewusst geaendert.
// Die Hauptseite ist sprachabhaengig; Setup und Download enthalten beide
// Sprachen bzw. nur statischen Text und benoetigen deshalb je einen ETag.
static const char ETH_ETAG_MAIN_DE[] = "\"TP3000-0.50.0-MAIN-DE-CHR5\"";
static const char ETH_ETAG_MAIN_EN[] = "\"TP3000-0.50.0-MAIN-EN-CHR5\"";
static const char ETH_ETAG_SETUP[]   = "\"TP3000-0.50.0-SETUP-PH7\"";
static const char ETH_ETAG_DOWNLOAD[]= "\"TP3000-0.50.0-DOWNLOAD\"";

struct EthPageSegment
{
  const char* data;
  size_t length;
};

static EthernetClient ethPageClient;
static bool ethPageActive = false;
static bool ethPageFinishing = false;
static uint8_t ethPageRequestCode = ETH_REQ_NONE;
static uint8_t ethPageSegmentCount = 0U;
static uint8_t ethPageSegmentIndex = 0U;
static size_t ethPageSegmentOffset = 0U;
static size_t ethPageHeaderLength = 0U;
static size_t ethPageHeaderOffset = 0U;
static size_t ethPageRemaining = 0U;
static uint16_t ethPageDrainTarget = ETH_HTTP_FALLBACK_SEND_BUFFER_BYTES;
static uint32_t ethPageLastProgressMs = 0UL;
static uint32_t ethPageDrainStartedMs = 0UL;
static EthPageSegment ethPageSegments[ETH_PAGE_MAX_SEGMENTS];
static DMAMEM char ethPageHeader[ETH_PAGE_HEADER_BYTES];
static DMAMEM uint8_t ethPageBuffer[ETH_PAGE_CHUNK_BYTES];

static bool FLASHMEM ethAsciiEqualIgnoreCase(char a, char b)
{
  if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
  if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
  return a == b;
}

static bool FLASHMEM ethHeaderNameEquals(const char* line,
                                         size_t lineLength,
                                         const char* name)
{
  if (line == nullptr || name == nullptr) return false;
  size_t nameLength = strlen(name);
  if (lineLength < nameLength) return false;
  for (size_t i = 0U; i < nameLength; i++)
  {
    if (!ethAsciiEqualIgnoreCase(line[i], name[i])) return false;
  }
  return true;
}

static bool FLASHMEM ethLineContainsToken(const char* line,
                                          size_t lineLength,
                                          const char* token)
{
  if (line == nullptr || token == nullptr) return false;
  size_t tokenLength = strlen(token);
  if (tokenLength == 0U || tokenLength > lineLength) return false;
  for (size_t i = 0U; i + tokenLength <= lineLength; i++)
  {
    if (memcmp(line + i, token, tokenLength) == 0) return true;
  }
  return false;
}

static bool FLASHMEM ethRequestHeaderHasEtag(const char* headers,
                                             const char* etag)
{
  if (headers == nullptr || etag == nullptr || *etag == '\0') return false;

  const char* line = headers;
  while (*line != '\0')
  {
    const char* end = strchr(line, '\n');
    size_t length = end != nullptr ? (size_t)(end - line) : strlen(line);
    while (length > 0U && (line[length - 1U] == '\r' || line[length - 1U] == '\n'))
    {
      length--;
    }

    static const char headerName[] = "If-None-Match:";
    if (ethHeaderNameEquals(line, length, headerName) &&
        ethLineContainsToken(line, length, etag))
    {
      return true;
    }

    if (end == nullptr) break;
    line = end + 1;
  }

  return false;
}

static void FLASHMEM ethSendNotModified(EthBoundedWriter& client,
                                        const char* etag)
{
  client.print(F("HTTP/1.1 304 Not Modified\r\n"));
  client.print(F("Connection: close\r\n"));
  client.print(F("Cache-Control: private, no-cache\r\n"));
  client.print(F("ETag: "));
  client.print(etag);
  client.print(F("\r\nContent-Length: 0\r\n\r\n"));
}

static void FLASHMEM ethSendPageBusy(EthBoundedWriter& client)
{
  static const char body[] = "Webseite wird bereits übertragen / Page transfer busy";
  client.print(F("HTTP/1.1 503 Service Unavailable\r\n"));
  client.print(F("Connection: close\r\n"));
  client.print(F("Content-Type: text/plain; charset=utf-8\r\n"));
  client.print(F("Cache-Control: no-store\r\n"));
  client.print(F("Retry-After: 1\r\n"));
  client.print(F("Content-Length: "));
  client.print((unsigned long)(sizeof(body) - 1U));
  client.print(F("\r\n\r\n"));
  client.write((const uint8_t*)body, sizeof(body) - 1U);
}

static void FLASHMEM ethPageTransferClose(bool stopClient)
{
  if (stopClient && ethPageClient) ethPageClient.stop();
  ethPageClient = EthernetClient();
  ethPageActive = false;
  ethPageFinishing = false;
  ethPageRequestCode = ETH_REQ_NONE;
  ethPageSegmentCount = 0U;
  ethPageSegmentIndex = 0U;
  ethPageSegmentOffset = 0U;
  ethPageHeaderLength = 0U;
  ethPageHeaderOffset = 0U;
  ethPageRemaining = 0U;
  ethPageDrainTarget = ETH_HTTP_FALLBACK_SEND_BUFFER_BYTES;
  ethPageLastProgressMs = 0UL;
  ethPageDrainStartedMs = 0UL;
}

static void ETH_FLASHMEM_NOINLINE ethPageTransferAbort(void)
{
  if (ethPageActive) ethDiagAbortedResponses++;
  ethPageTransferClose(true);
}

static bool FLASHMEM ethBeginPageTransfer(EthernetClient& rawClient,
                                          EthBoundedWriter& immediateClient,
                                          const char* requestHeaders,
                                          const char* etag,
                                          uint8_t requestCode,
                                          const EthPageSegment* segments,
                                          uint8_t segmentCount,
                                          size_t contentLength,
                                          bool* keepOpen)
{
  if (keepOpen != nullptr) *keepOpen = false;

  const bool browserCacheAllowed = requestCode != ETH_REQ_DOWNLOAD_PAGE;
  if (browserCacheAllowed && ethRequestHeaderHasEtag(requestHeaders, etag))
  {
    ethSendNotModified(immediateClient, etag);
    return immediateClient.ok();
  }

  if (ethPageActive || segments == nullptr || segmentCount == 0U ||
      segmentCount > ETH_PAGE_MAX_SEGMENTS)
  {
    ethSendPageBusy(immediateClient);
    return immediateClient.ok();
  }

  const char* cacheControl = browserCacheAllowed
                           ? "private, no-cache"
                           : "no-store, no-cache, must-revalidate, max-age=0";
  int headerLength = snprintf(ethPageHeader, sizeof(ethPageHeader),
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: %lu\r\n"
    "Cache-Control: %s\r\n"
    "Pragma: no-cache\r\n"
    "Expires: 0\r\n"
    "ETag: %s\r\n\r\n",
    (unsigned long)contentLength, cacheControl, etag);
  if (headerLength <= 0 || (size_t)headerLength >= sizeof(ethPageHeader))
  {
    ethSendText(immediateClient, false, "HTTP-Header zu gross / HTTP header too large");
    return immediateClient.ok();
  }

  for (uint8_t i = 0U; i < segmentCount; i++)
  {
    ethPageSegments[i] = segments[i];
  }

  ethPageClient = rawClient;
  ethPageActive = true;
  ethPageFinishing = false;
  ethPageRequestCode = requestCode;
  ethPageSegmentCount = segmentCount;
  ethPageSegmentIndex = 0U;
  ethPageSegmentOffset = 0U;
  ethPageHeaderLength = (size_t)headerLength;
  ethPageHeaderOffset = 0U;
  ethPageRemaining = ethPageHeaderLength + contentLength;
  int sendCapacity = ethPageClient.availableForWrite();
  ethPageDrainTarget = sendCapacity > 0
                     ? (uint16_t)sendCapacity
                     : ETH_HTTP_FALLBACK_SEND_BUFFER_BYTES;
  ethPageLastProgressMs = millis();
  ethPageDrainStartedMs = 0UL;

  if (keepOpen != nullptr) *keepOpen = true;
  return true;
}

static size_t FLASHMEM ethPageFillNextChunk(size_t wanted)
{
  size_t copied = 0U;
  while (copied < wanted)
  {
    if (ethPageHeaderOffset < ethPageHeaderLength)
    {
      size_t available = ethPageHeaderLength - ethPageHeaderOffset;
      size_t take = wanted - copied;
      if (take > available) take = available;
      memcpy(ethPageBuffer + copied,
             ethPageHeader + ethPageHeaderOffset,
             take);
      ethPageHeaderOffset += take;
      copied += take;
      continue;
    }

    while (ethPageSegmentIndex < ethPageSegmentCount &&
           ethPageSegmentOffset >= ethPageSegments[ethPageSegmentIndex].length)
    {
      ethPageSegmentIndex++;
      ethPageSegmentOffset = 0U;
    }

    if (ethPageSegmentIndex >= ethPageSegmentCount) break;

    const EthPageSegment& segment = ethPageSegments[ethPageSegmentIndex];
    size_t available = segment.length - ethPageSegmentOffset;
    size_t take = wanted - copied;
    if (take > available) take = available;
    memcpy(ethPageBuffer + copied,
           segment.data + ethPageSegmentOffset,
           take);
    ethPageSegmentOffset += take;
    copied += take;
  }
  return copied;
}

static void ETH_FLASHMEM_NOINLINE ethPageTransferService(void)
{
  if (!ethPageActive) return;

  if (!ethPageClient || (!ethPageClient.connected() && ethPageClient.available() <= 0))
  {
    ethPageTransferAbort();
    return;
  }

  const uint32_t nowMs = millis();

  if (ethPageFinishing)
  {
    int freeBytes = ethPageClient.availableForWrite();
    if (freeBytes >= 0 && (uint16_t)freeBytes >= ethPageDrainTarget)
    {
      ethPageTransferClose(true);
      return;
    }

    if ((uint32_t)(nowMs - ethPageDrainStartedMs) >= ETH_HTTP_DRAIN_TIMEOUT_MS)
    {
      ethPageTransferClose(true);
    }
    return;
  }

  if ((uint32_t)(nowMs - ethPageLastProgressMs) >= ETH_PAGE_STALL_TIMEOUT_MS)
  {
    ethPageTransferAbort();
    return;
  }

  size_t wanted = ethPageRemaining > ETH_PAGE_CHUNK_BYTES
                    ? ETH_PAGE_CHUNK_BYTES
                    : ethPageRemaining;
  if (wanted == 0U)
  {
    ethPageFinishing = true;
    ethPageDrainStartedMs = nowMs;
    return;
  }

  // Niemals auf TCP-Puffer warten. Pro Hauptloop wird hoechstens genau ein
  // Seitenstueck gesendet.
  if (ethPageClient.availableForWrite() < (int)wanted) return;

  size_t prepared = ethPageFillNextChunk(wanted);
  if (prepared != wanted)
  {
    ethPageTransferAbort();
    return;
  }

  ethDiagRequest = ethPageRequestCode;
  ethDiagRequestStartMs = nowMs;
  ethDiagStage = ETH_DIAG_WRITE;
  uint32_t startedUs = micros();
  size_t written = ethPageClient.write(ethPageBuffer, prepared);
  uint32_t durationUs = (uint32_t)(micros() - startedUs);
  ethDiagLastWriteUs = durationUs;
  if (durationUs > ethDiagMaxWriteUs) ethDiagMaxWriteUs = durationUs;
  ethDiagLastRequestUs = durationUs;
  if (durationUs > ethDiagMaxRequestUs)
  {
    ethDiagMaxRequestUs = durationUs;
    ethDiagMaxRequestCode = ethPageRequestCode;
  }

  ethDiagRequestStartMs = 0UL;
  ethDiagRequest = ETH_REQ_NONE;
  ethDiagStage = ETH_DIAG_IDLE;

  if (written != prepared || prepared > ethPageRemaining)
  {
    ethPageTransferAbort();
    return;
  }

  ethPageRemaining -= prepared;
  ethPageLastProgressMs = millis();
  if (ethPageRemaining == 0U)
  {
    ethPageFinishing = true;
    ethPageDrainStartedMs = ethPageLastProgressMs;
  }
}

// ---------------------------------------------------------------------------
// Asynchroner SD-Dateidownload
// ---------------------------------------------------------------------------
// Die Datei bleibt zwischen den loop()-Durchlaeufen geoeffnet. Grundeinheit ist
// genau ein 512-Byte-SD-Sektor. Pro Aufruf von ethernetServiceTask() darf die
// adaptive Stufe maximal 1, 2, 3 oder 4 Sektoren uebertragen. Gestartet wird
// mit 1 Sektor. Vor jedem Sektor werden TCP-Puffer und Zeitbudget erneut
// geprueft; es gibt weder eine Warteschleife noch ein Safety-Hold. Langsame
// Download-Schritte reduzieren die Stufe, schnelle vollstaendige Schritte
// erhoehen sie nach einer kurzen Bewaehrungsphase.
static const size_t   ETH_DOWNLOAD_BLOCK_BYTES = 512U;
static const uint8_t  ETH_DOWNLOAD_LEVEL_BLOCKS[] = { 1U, 2U, 3U, 4U };
static const uint8_t  ETH_DOWNLOAD_LEVEL_COUNT =
  (uint8_t)(sizeof(ETH_DOWNLOAD_LEVEL_BLOCKS) / sizeof(ETH_DOWNLOAD_LEVEL_BLOCKS[0]));
static const uint8_t  ETH_DOWNLOAD_START_LEVEL_INDEX = 1U; // mit 2 x 512 B starten
static const uint16_t ETH_DOWNLOAD_RAMP_GOOD_CALLS = 30U;
static const uint32_t ETH_DOWNLOAD_TIME_BUDGET_US = 2000UL;
static const uint32_t ETH_DOWNLOAD_SLOW_US = 3000UL;
static const uint32_t ETH_DOWNLOAD_VERY_SLOW_US = 8000UL;
static const uint32_t ETH_DOWNLOAD_STALL_TIMEOUT_MS = 15000UL;
static const uint32_t ETH_DOWNLOAD_CLOSE_DELAY_MS = 10UL;
// NativeEthernet kann bei grossen Writes trotz availableForWrite() unguenstig
// reagieren. Deshalb wird weiterhin in begrenzten TCP-Teilstuecken geschrieben;
// Range-Resume und Content-Length bleiben unveraendert. WLAN-Tests duerfen
// den Download abbrechen, aber die Hauptloop nicht blockieren.
static const size_t   ETH_DOWNLOAD_WRITE_CHUNK_BYTES = 512U;
static const uint8_t  ETH_FILE_PAGE_SIZE = 25U;
static const uint16_t ETH_FILE_INDEX_MAX = 512U;

static DMAMEM uint8_t ethDownloadBuffer[ETH_DOWNLOAD_BLOCK_BYTES];
static EthernetClient ethDownloadClient;
static File ethDownloadFile;
static bool ethDownloadActive = false;
static bool ethDownloadFinishing = false;
static uint32_t ethDownloadRemaining = 0;
static uint32_t ethDownloadLastProgressMs = 0;
static uint32_t ethDownloadCloseAfterMs = 0;
static size_t ethDownloadPendingLength = 0U;
static size_t ethDownloadPendingOffset = 0U;
static uint8_t ethDownloadLevelIndex = ETH_DOWNLOAD_START_LEVEL_INDEX;
static uint16_t ethDownloadGoodCalls = 0;
static char ethDownloadCurrentName[32] = {0};
static uint32_t ethDownloadCurrentOffset = 0U;

static const uint8_t ETH_FILE_READ_FAULT_MAX = 8U;
struct EthFileReadFault
{
  char name[32];
  uint32_t offset;
  uint8_t stage;
};
static DMAMEM EthFileReadFault ethFileReadFaults[ETH_FILE_READ_FAULT_MAX];
static uint8_t ethFileReadFaultCount = 0U;
static uint8_t ethFileReadFaultNext = 0U;

// Letzter neu erkannter Dateilesefehler fuer die Download-Webseite. Die
// Sequenz ermoeglicht es dem Browser, nur Fehler anzuzeigen, die nach dem
// Klick auf Download entstanden sind. Die Daten bleiben bewusst nur im RAM.
static uint32_t ethDownloadFaultNoticeSeq = 0U;
static char ethDownloadFaultNoticeName[32] = {0};
static uint32_t ethDownloadFaultNoticeOffset = 0U;
static uint8_t ethDownloadFaultNoticeStage = SD_DIAG_IDLE;
static bool ethDownloadFaultNoticePeltierCut = false;

static bool FLASHMEM ethFileHasReadFault(const char* name, uint32_t* offsetOut, uint8_t* stageOut);
static void FLASHMEM ethFileMarkReadFault(const char* name, uint32_t offset,
                                         uint8_t stage, bool peltierCut);

static bool ethDownloadHandleIoStall(uint8_t stall, uint8_t sdStage, uint32_t offset)
{
  if (stall == ETH_DOWNLOAD_IO_SD)
  {
    // Ein vom Safety-Timer erkannter SD-Stall hat den Peltierpfad bereits
    // kurz auf HIGH/HIGH sicher AUS geschaltet, ohne eine Safety zu latchen.
    ethFileMarkReadFault(ethDownloadCurrentName, offset, sdStage, true);
  }
  return stall != ETH_DOWNLOAD_IO_NONE;
}

enum EthFileIndexState : uint8_t
{
  ETH_FILE_INDEX_EMPTY = 0,
  ETH_FILE_INDEX_NEED_SD,
  ETH_FILE_INDEX_NEED_DIR,
  ETH_FILE_INDEX_SCANNING,
  ETH_FILE_INDEX_READY,
  ETH_FILE_INDEX_ERROR
};

enum EthFileKind : uint8_t
{
  ETH_FILE_KIND_CSV = 0,
  ETH_FILE_KIND_SERIAL = 1
};

struct EthFileIndexEntry
{
  char name[32];
  uint32_t size;
  uint16_t year;
  uint16_t order;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t kind;
  uint8_t hasTime;
};

static DMAMEM EthFileIndexEntry ethFileIndex[ETH_FILE_INDEX_MAX];
static File ethFileIndexDir;
static uint16_t ethFileIndexCount = 0U;
static uint16_t ethFileIndexScanned = 0U;
static uint16_t ethFileIndexValidSeen = 0U;
static uint16_t ethFileIndexOrder = 0U;
static uint8_t ethFileIndexState = ETH_FILE_INDEX_EMPTY;
static uint8_t ethFileIndexErrorCode = 0U;
static bool ethFileIndexTruncated = false;

static void FLASHMEM ethDownloadResetAdaptive(void)
{
  ethDownloadLevelIndex = ETH_DOWNLOAD_START_LEVEL_INDEX;
  ethDownloadGoodCalls = 0;
}

static void FLASHMEM ethDownloadAdapt(uint8_t blocksSent,
                                      uint8_t targetBlocks,
                                      uint32_t serviceUs)
{
  if (blocksSent == 0U) return;

  // Ein sehr langsamer Einzelaufruf faellt sofort auf die kleinste Stufe.
  if (serviceUs >= ETH_DOWNLOAD_VERY_SLOW_US)
  {
    ethDownloadLevelIndex = 0U;
    ethDownloadGoodCalls = 0U;
    return;
  }

  // Wird das normale Zeitbudget merklich ueberschritten, eine Stufe zurueck.
  if (serviceUs >= ETH_DOWNLOAD_SLOW_US ||
      (serviceUs >= ETH_DOWNLOAD_TIME_BUDGET_US && blocksSent < targetBlocks))
  {
    if (ethDownloadLevelIndex > 0U) ethDownloadLevelIndex--;
    ethDownloadGoodCalls = 0U;
    return;
  }

  // Ein voller TCP-Puffer ist kein Fehler. Nur wenn die aktuelle Zielstufe
  // komplett und innerhalb des Budgets erreicht wurde, zaehlt der Aufruf zum
  // langsamen Hochregeln.
  if (blocksSent == targetBlocks && serviceUs < ETH_DOWNLOAD_TIME_BUDGET_US)
  {
    if (ethDownloadGoodCalls < ETH_DOWNLOAD_RAMP_GOOD_CALLS)
    {
      ethDownloadGoodCalls++;
    }

    if (ethDownloadGoodCalls >= ETH_DOWNLOAD_RAMP_GOOD_CALLS &&
        (uint8_t)(ethDownloadLevelIndex + 1U) < ETH_DOWNLOAD_LEVEL_COUNT)
    {
      ethDownloadLevelIndex++;
      ethDownloadGoodCalls = 0U;
    }
  }
  else
  {
    ethDownloadGoodCalls = 0U;
  }
}

static void FLASHMEM ethDownloadReleaseStateOnly(void)
{
  // Keine Netzwerk-/SD-Library-Aufrufe in diesem Pfad: Er wird bewusst auch
  // nach einem blockierten Download benutzt. Ein stop() oder close() darf hier
  // nicht erneut die Hauptloop festhalten. Browser erkennen das Ende ueber den
  // gesendeten Content-Length; offene TCP-Reste laufen im Stack/Browser aus.
  ethDownloadActive = false;
  ethDownloadFinishing = false;
  ethDownloadRemaining = 0;
  ethDownloadLastProgressMs = 0;
  ethDownloadCloseAfterMs = 0;
  ethDownloadPendingLength = 0U;
  ethDownloadPendingOffset = 0U;
  ethDownloadCurrentName[0] = '\0';
  ethDownloadCurrentOffset = 0U;
  ethDownloadResetAdaptive();
  ethDiagRequestStartMs = 0;
  ethDiagRequest = ETH_REQ_NONE;
  ethDiagStage = ETH_DIAG_IDLE;
}

static void FLASHMEM ethDownloadStopClient(void)
{
  if (ethDownloadClient)
  {
    // Nach einem fertigen oder abgebrochenen Download muss der TCP-Socket
    // wirklich freigegeben werden. Sonst kann ein halb offener WLAN-/Browser-
    // Request im NativeEthernet-Stack liegen bleiben und spaetere Setup-/JSON-
    // Requests sichtbar ausbremsen. stop() wird nur beim Abschluss/Abbruch
    // aufgerufen, niemals in der normalen Uebertragungsschleife.
    ethDiagStage = ETH_DIAG_STOP;
    ethDownloadClient.stop();
  }
  ethDownloadClient = EthernetClient();
}

static void FLASHMEM ethDownloadCloseFile(void)
{
  if (ethDownloadFile)
  {
    sdStorageDiagBegin(SD_DIAG_DOWNLOAD_CLOSE);
    ethDownloadIoBegin(ETH_DOWNLOAD_IO_SD);
    ethDownloadFile.close();
    uint8_t stall = ethDownloadIoEnd();
    sdStorageDiagEnd(SD_DIAG_DOWNLOAD_CLOSE);
    ethDownloadHandleIoStall(stall, SD_DIAG_DOWNLOAD_CLOSE, ethDownloadCurrentOffset);
  }
}

static void FLASHMEM ethDownloadClose(void)
{
  ethDownloadCloseFile();
  ethDownloadStopClient();
  ethDownloadReleaseStateOnly();
}

static void ETH_FLASHMEM_NOINLINE ethDownloadAbort(void)
{
  ethDiagAbortedResponses++;
  ethDownloadCloseFile();
  ethDownloadStopClient();
  ethDownloadReleaseStateOnly();
}

static void ETH_FLASHMEM_NOINLINE ethDownloadService(void)
{
  if (!ethDownloadActive) return;

  if (!ethDownloadClient)
  {
    ethDownloadAbort();
    return;
  }

  ethDiagStage = ETH_DIAG_AVAILABLE;
  ethDownloadIoBegin(ETH_DOWNLOAD_IO_NETWORK);
  bool connected = ethDownloadClient.connected();
  uint8_t ioStall = ethDownloadIoEnd();
  ethDiagStage = ETH_DIAG_HANDLE_REQUEST;
  if (ethDownloadHandleIoStall(ioStall, SD_DIAG_IDLE, ethDownloadCurrentOffset) || !connected)
  {
    ethDownloadAbort();
    return;
  }

  uint32_t nowMs = millis();

  if (ethDownloadFinishing)
  {
    // Nach dem letzten uebergebenen Byte wird nicht mehr auf einen vollstaendig
    // freien TCP-Puffer gewartet. Dieses Warten bzw. anschliessendes stop()
    // kann den NativeEthernet-Pfad festhalten. Content-Length ist bereits
    // gesendet; der Browser kann den Download dadurch sauber abschliessen.
    if ((int32_t)(nowMs - ethDownloadCloseAfterMs) < 0) return;

    ethDownloadClose();
    return;
  }

  if ((uint32_t)(nowMs - ethDownloadLastProgressMs) >= ETH_DOWNLOAD_STALL_TIMEOUT_MS)
  {
    ethDownloadAbort();
    return;
  }

  uint8_t targetBlocks = ETH_DOWNLOAD_LEVEL_BLOCKS[ethDownloadLevelIndex];
  uint8_t blocksSent = 0U;
  uint32_t serviceStartedUs = micros();

  ethDiagRequest = ETH_REQ_DOWNLOAD_FILE;
  ethDiagRequestStartMs = nowMs;
  ethDiagStage = ETH_DIAG_HANDLE_REQUEST;

  while (blocksSent < targetBlocks)
  {
    uint32_t elapsedUs = (uint32_t)(micros() - serviceStartedUs);
    if (blocksSent > 0U && elapsedUs >= ETH_DOWNLOAD_TIME_BUDGET_US) break;

    // Ein bereits gelesener Sektor bleibt so lange im RAM, bis auch ein
    // Teilwrite vollstaendig an NativeEthernet uebergeben wurde.
    if (ethDownloadPendingOffset < ethDownloadPendingLength)
    {
      size_t pending = ethDownloadPendingLength - ethDownloadPendingOffset;

      ethDiagStage = ETH_DIAG_AVAILABLE;
      ethDownloadIoBegin(ETH_DOWNLOAD_IO_NETWORK);
      int available = ethDownloadClient.availableForWrite();
      ioStall = ethDownloadIoEnd();
      ethDiagStage = ETH_DIAG_HANDLE_REQUEST;
      if (ethDownloadHandleIoStall(ioStall, SD_DIAG_IDLE, ethDownloadCurrentOffset))
      {
        ethDownloadAbort();
        return;
      }
      if (available <= 0) break;

      size_t wantedWrite = pending;
      if (wantedWrite > ETH_DOWNLOAD_WRITE_CHUNK_BYTES) wantedWrite = ETH_DOWNLOAD_WRITE_CHUNK_BYTES;
      if (wantedWrite > (size_t)available) wantedWrite = (size_t)available;

      ethDiagStage = ETH_DIAG_WRITE;
      ethDownloadIoBegin(ETH_DOWNLOAD_IO_NETWORK);
      uint32_t writeStartedUs = micros();
      size_t written = ethDownloadClient.write(
          ethDownloadBuffer + ethDownloadPendingOffset, wantedWrite);
      uint32_t writeUs = (uint32_t)(micros() - writeStartedUs);
      ioStall = ethDownloadIoEnd();
      ethDiagLastWriteUs = writeUs;
      if (writeUs > ethDiagMaxWriteUs) ethDiagMaxWriteUs = writeUs;
      ethDiagStage = ETH_DIAG_HANDLE_REQUEST;

      if (ethDownloadHandleIoStall(ioStall, SD_DIAG_IDLE, ethDownloadCurrentOffset))
      {
        ethDownloadAbort();
        return;
      }
      if (written == 0U) break;
      if (written > wantedWrite || written > ethDownloadRemaining)
      {
        ethDownloadAbort();
        return;
      }

      ethDownloadPendingOffset += written;
      ethDownloadRemaining -= (uint32_t)written;
      ethDownloadLastProgressMs = millis();

      if (ethDownloadPendingOffset < ethDownloadPendingLength) break;

      ethDownloadPendingLength = 0U;
      ethDownloadPendingOffset = 0U;
      blocksSent++;

      if (ethDownloadRemaining == 0U)
      {
        ethDownloadFinishing = true;
        ethDownloadCloseAfterMs = ethDownloadLastProgressMs + ETH_DOWNLOAD_CLOSE_DELAY_MS;
        break;
      }
      continue;
    }

    size_t wanted = ethDownloadRemaining > ETH_DOWNLOAD_BLOCK_BYTES
                      ? ETH_DOWNLOAD_BLOCK_BYTES
                      : (size_t)ethDownloadRemaining;

    if (wanted == 0U)
    {
      ethDownloadFinishing = true;
      ethDownloadCloseAfterMs = millis() + ETH_DOWNLOAD_CLOSE_DELAY_MS;
      break;
    }

    // Ohne Platz fuer wenigstens ein Byte wird nicht auf die SD-Karte
    // zugegriffen. Bei TCP-Rueckstau bleibt die Loop deshalb sofort frei.
    ethDiagStage = ETH_DIAG_AVAILABLE;
    ethDownloadIoBegin(ETH_DOWNLOAD_IO_NETWORK);
    int available = ethDownloadClient.availableForWrite();
    ioStall = ethDownloadIoEnd();
    ethDiagStage = ETH_DIAG_HANDLE_REQUEST;
    if (ethDownloadHandleIoStall(ioStall, SD_DIAG_IDLE, ethDownloadCurrentOffset))
    {
      ethDownloadAbort();
      return;
    }
    if (available <= 0) break;

    const uint32_t readOffset = ethDownloadCurrentOffset;
    sdStorageDiagBegin(SD_DIAG_DOWNLOAD_READ);
    ethDownloadIoBegin(ETH_DOWNLOAD_IO_SD);
    int bytesRead = ethDownloadFile.read(ethDownloadBuffer, wanted);
    ioStall = ethDownloadIoEnd();
    sdStorageDiagEnd(SD_DIAG_DOWNLOAD_READ);
    if (ethDownloadHandleIoStall(ioStall, SD_DIAG_DOWNLOAD_READ, readOffset))
    {
      ethDownloadAbort();
      return;
    }
    if (bytesRead <= 0 || (size_t)bytesRead != wanted)
    {
      ethFileMarkReadFault(ethDownloadCurrentName, readOffset, SD_DIAG_DOWNLOAD_READ, false);
      ethDownloadAbort();
      return;
    }

    ethDownloadCurrentOffset += (uint32_t)bytesRead;
    ethDownloadPendingLength = (size_t)bytesRead;
    ethDownloadPendingOffset = 0U;
  }

  uint32_t serviceUs = (uint32_t)(micros() - serviceStartedUs);
  ethDiagLastRequestUs = serviceUs;
  if (serviceUs > ethDiagMaxRequestUs)
  {
    ethDiagMaxRequestUs = serviceUs;
    ethDiagMaxRequestCode = ETH_REQ_DOWNLOAD_FILE;
  }

  ethDiagRequestStartMs = 0;
  ethDiagRequest = ETH_REQ_NONE;
  ethDiagStage = ETH_DIAG_IDLE;

  ethDownloadAdapt(blocksSent, targetBlocks, serviceUs);
}

static uint8_t FLASHMEM ethDiagRequestCode(const char* requestLine)
{
  if (ethPathEquals(requestLine, "/")) return ETH_REQ_ROOT;
  if (ethPathEquals(requestLine, "/setup")) return ETH_REQ_SETUP;
  if (ethPathEquals(requestLine, "/setup.json")) return ETH_REQ_SETUP_JSON;
  if (ethPathEquals(requestLine, "/setup-status.json")) return ETH_REQ_SETUP_STATUS;
  if (ethPathEquals(requestLine, "/setup-live.json")) return ETH_REQ_SETUP_LIVE;
  if (ethPathEquals(requestLine, "/setup-control")) return ETH_REQ_SETUP_CONTROL;
  if (ethPathEquals(requestLine, "/setup-control-unlock")) return ETH_REQ_SETUP_CONTROL_UNLOCK;
  if (ethPathEquals(requestLine, "/setup-head-status")) return ETH_REQ_SETUP_HEAD_STATUS;
  if (ethPathEquals(requestLine, "/setup-head-list.json")) return ETH_REQ_SETUP_HEAD_LIST;
  if (ethPathEquals(requestLine, "/setup-head-current")) return ETH_REQ_SETUP_HEAD_CURRENT;
  if (ethPathEquals(requestLine, "/setup-head-load")) return ETH_REQ_SETUP_HEAD_LOAD;
  if (ethPathEquals(requestLine, "/setup-head-save")) return ETH_REQ_SETUP_HEAD_SAVE;
  if (ethPathEquals(requestLine, "/setup-device-status")) return ETH_REQ_SETUP_DEVICE_STATUS;
  if (ethPathEquals(requestLine, "/setup-device-storage")) return ETH_REQ_SETUP_DEVICE_STORAGE;
  if (ethPathEquals(requestLine, "/setup-fan-toggle")) return ETH_REQ_SETUP_FAN_TOGGLE;
  if (ethPathEquals(requestLine, "/setup-fan")) return ETH_REQ_SETUP_FAN;
  if (ethPathEquals(requestLine, "/setup-alarm")) return ETH_REQ_SETUP_ALARM;
  if (ethPathEquals(requestLine, "/setup-r0")) return ETH_REQ_SETUP_R0;
  if (ethPathEquals(requestLine, "/setup-2pm")) return ETH_REQ_SETUP_2PM;
  if (ethPathEquals(requestLine, "/setup-2pa")) return ETH_REQ_SETUP_2PA;
  if (ethPathEquals(requestLine, "/setup-ref")) return ETH_REQ_SETUP_REF;
  if (ethPathEquals(requestLine, "/setup-offset")) return ETH_REQ_SETUP_OFFSET;
  if (ethPathEquals(requestLine, "/setup-general")) return ETH_REQ_SETUP_GENERAL;
  if (ethPathEquals(requestLine, "/setup-rs1")) return ETH_REQ_SETUP_RS1;
  if (ethPathEquals(requestLine, "/setup-rs2")) return ETH_REQ_SETUP_RS2;
  if (ethPathEquals(requestLine, "/setup-usb")) return ETH_REQ_SETUP_USB;
  if (ethPathEquals(requestLine, "/setup-sd")) return ETH_REQ_SETUP_SD;
  if (ethPathEquals(requestLine, "/setup-flow")) return ETH_REQ_SETUP_FLOW;
  if (ethPathEquals(requestLine, "/setup-almemo")) return ETH_REQ_SETUP_ALMEMO;
  if (ethPathEquals(requestLine, "/setup-wincontrol")) return ETH_REQ_SETUP_WINCONTROL;
  if (ethPathEquals(requestLine, "/setup-language")) return ETH_REQ_SETUP_LANGUAGE;
  if (ethPathEquals(requestLine, "/setup-display")) return ETH_REQ_SETUP_DISPLAY;
  if (ethPathEquals(requestLine, "/setup-close")) return ETH_REQ_SETUP_CLOSE;
  if (ethPathEquals(requestLine, "/data.json")) return ETH_REQ_DATA_JSON;
  if (ethPathEquals(requestLine, "/chart.json")) return ETH_REQ_CHART_JSON;
  if (ethPathEquals(requestLine, "/set-time")) return ETH_REQ_SET_TIME;
  if (ethPathEquals(requestLine, "/favicon.ico")) return ETH_REQ_FAVICON;
  if (ethPathEquals(requestLine, "/download")) return ETH_REQ_DOWNLOAD_PAGE;
  if (ethPathEquals(requestLine, "/files.json")) return ETH_REQ_FILES_JSON;
  if (ethPathEquals(requestLine, "/download-file")) return ETH_REQ_DOWNLOAD_FILE;
  return ETH_REQ_NOT_FOUND;
}

static void FLASHMEM ethReadMac(uint8_t mac[6])
{
#if defined(__IMXRT1062__)
  mac[0] = (HW_OCOTP_MAC1 >> 8) & 0xFF;
  mac[1] = (HW_OCOTP_MAC1 >> 0) & 0xFF;
  mac[2] = (HW_OCOTP_MAC0 >> 24) & 0xFF;
  mac[3] = (HW_OCOTP_MAC0 >> 16) & 0xFF;
  mac[4] = (HW_OCOTP_MAC0 >> 8) & 0xFF;
  mac[5] = (HW_OCOTP_MAC0 >> 0) & 0xFF;
#else
  // Fallback fuer Nicht-Teensy-Builds/Testumgebungen.
  mac[0] = 0x04;
  mac[1] = 0xE9;
  mac[2] = 0xE5;
  mac[3] = 0x00;
  mac[4] = 0x00;
  mac[5] = 0x01;
#endif
}

static const char* FLASHMEM ethGetHostname(void)
{
  const char* sn = deviceSerialGet();

  // deviceSerialGet() normalisiert bereits auf exakt fuenf Ziffern. Zur
  // Sicherheit werden trotzdem nur gueltige Ziffern uebernommen; ansonsten
  // erscheint die Default-SN 00000 im Hostnamen.
  char safeSn[DEVICE_SERIAL_DIGITS + 1U];
  for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
  {
    char c = sn ? sn[i] : '\0';
    safeSn[i] = (c >= '0' && c <= '9') ? c : '0';
  }
  safeSn[DEVICE_SERIAL_DIGITS] = '\0';

  snprintf(ethHostname, sizeof(ethHostname), "%s%s", ETH_HOSTNAME_PREFIX, safeSn);
  ethHostname[sizeof(ethHostname) - 1U] = '\0';
  return ethHostname;
}

const char* FLASHMEM ethernetHostname(void)
{
  return ethGetHostname();
}

static void FLASHMEM ethBeginMdnsIfNeeded(void)
{
  if (ethMdnsStarted) return;
  if (!ethLastEnabled || !ethConfigured) return;
  if (!ethIpIsValid(Ethernet.localIP())) return;

#if defined(FNET_CFG_MDNS) && FNET_CFG_MDNS
  // NativeEthernet bietet kein DHCP-Hostname-API. mDNS liefert jedoch die
  // lokal sichtbare Kennung TP-3000-xxxxx.local und kuendigt den Webserver
  // als HTTP-Service an, wenn er aktiv ist.
  const char* host = ethGetHostname();
  MDNS.begin(host, 1);
  if (ethLastWebEnabled)
  {
    MDNS.addService("_http._tcp", ethLastWebPort);
  }
  ethMdnsStarted = true;
#else
  // Falls die installierte NativeEthernet/FNET-Kombination ohne mDNS gebaut
  // ist, soll der Messloop nicht in jedem Durchlauf erneut hier landen.
  ethMdnsStarted = true;
#endif
}

static IPAddress FLASHMEM ethMakeIp(const uint8_t ip[4])
{
  return IPAddress(ip[0], ip[1], ip[2], ip[3]);
}

static bool FLASHMEM ethIpIsValid(IPAddress ip)
{
  return (ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0);
}

static bool FLASHMEM ethArrayChanged(const uint8_t a[4], const uint8_t b[4])
{
  return a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3];
}

static void FLASHMEM ethCopyIp(uint8_t dst[4], const uint8_t src[4])
{
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

static void ETH_FLASHMEM_NOINLINE ethAbortAllPendingRequests(void);

static void FLASHMEM ethDestroyServer()
{
  if (ethPageActive) ethPageTransferAbort();
  ethAbortAllPendingRequests();
  if (ethWebServer != nullptr)
  {
    // Objekt wurde mit placement-new in ethWebServerStorage erzeugt.
    // Darum kein delete verwenden.
    ethWebServer->~EthernetServer();
    ethWebServer = nullptr;
  }
}

static void ETH_FLASHMEM_NOINLINE ethBeginServerIfNeeded()
{
  if (!ethConfigured || !ethLastWebEnabled) return;
  if (!ethIpIsValid(Ethernet.localIP())) return;
  if (ethWebServer != nullptr) return;

  void* storage = static_cast<void*>(ethWebServerStorage);
  ethWebServer = new (storage) EthernetServer(ethLastWebPort);
  ethWebServer->begin();
  ethLastServerBeginMs = millis();
}

static bool FLASHMEM ethHardwareLooksPresent()
{
  // Ab _25 nur noch Diagnose-/Plausibilitaetsfunktion, KEINE harte Startbremse.
  // NativeEthernet kann hardwareStatus() vor dem ersten Ethernet.begin() bzw. nach
  // spaeter Initialisierung als EthernetNoHardware/Unknown liefern, obwohl der
  // Teensy-4.1-PHY vorhanden ist. In _24 fuehrte das dazu, dass die Warte-Seite
  // nur kurz erschien und DHCP gar nicht gestartet wurde.
  // Darum wird der Startversuch trotzdem ausgefuehrt; der kurze DHCP-Timeout bzw.
  // die gesetzte Static-IP entscheidet danach ueber ethConfigured.
  (void)Ethernet.hardwareStatus();
  return true;
}

// PHY-Linkabfrage nach Ethernet-Initialisierung.
// Wichtig: Bei NativeEthernet ist linkStatus() vor dem ersten Ethernet.begin()
// nicht als Startbedingung verwendbar. In _22 wurde dadurch DHCP trotz
// gestecktem Kabel uebersprungen. Darum wird der Link nur nach einem
// Startversuch zur Statusanzeige/Request-Freigabe bewertet.
static inline bool ethLinkIsUp()
{
  return Ethernet.linkStatus() == LinkON;
}

static bool FLASHMEM ethStaticLinkAvailable(uint8_t mac[6])
{
  // Nach einem frueheren Ethernet.begin() kann der Linkstatus bereits stimmen.
  if (Ethernet.linkStatus() == LinkON) return true;

  // NativeEthernet liefert linkStatus() vor der ersten Initialisierung teils
  // Unknown/LinkOFF. Fuer Static-IP ist Ethernet.begin(mac, ip, ...) ohne
  // Kabel jedoch gefaehrlich blockierend. Darum initialisieren wir den PHY/Stack
  // mit einem sehr kurzen DHCP-Probe. Der Probe darf fehlschlagen; wichtig ist
  // nur ein belastbarer Linkstatus bzw. eine bereits sichtbare lokale IP.
  (void)Ethernet.begin(mac,
                       ETH_STATIC_LINK_PROBE_TIMEOUT_MS,
                       ETH_STATIC_LINK_PROBE_RESPONSE_TIMEOUT_MS);

  uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < ETH_STATIC_LINK_SETTLE_MS)
  {
    if (Ethernet.linkStatus() == LinkON) return true;
    if (ethIpIsValid(Ethernet.localIP())) return true;
    yield();
  }

  return Ethernet.linkStatus() == LinkON || ethIpIsValid(Ethernet.localIP());
}

static void FLASHMEM ethConfigureNow()
{
  ethDestroyServer();
  ethConfigured = false;
  ethDhcpPending = false;
  ethDhcpPendingUntilMs = 0;

  if (!ethLastEnabled)
  {
    return;
  }

  uint8_t mac[6];
  ethReadMac(mac);

  safetyBeginBlockingOperation();

  if (!ethHardwareLooksPresent())
  {
    ethConfigured = false;
    safetyEndBlockingOperation();
    return;
  }

  // Boot-Safe-Korrektur _23:
  // linkStatus() darf hier NICHT vor Ethernet.begin() als harte Startbedingung
  // benutzt werden. NativeEthernet meldet sonst je nach Initialisierungszustand
  // trotz gestecktem Kabel LinkOFF/Unknown und DHCP wird gar nicht gestartet.
  // Der Startversuch erfolgt nur an dieser Stelle oder nach Bedienaktion,
  // nicht zyklisch im Mess-Loop.

  if (ethLastDhcp)
  {
    // DHCP wieder bewusst nahe am letzten funktionierenden Pfad halten:
    // genau ein Ethernet.begin() mit kurzem Timeout. Manche NativeEthernet-
    // Kombinationen liefern kurz nach begin() noch keine gueltige localIP(),
    // obwohl der DHCP-Prozess/Stack danach noch nachlaeuft. Darum bleibt der
    // Stack fuer eine begrenzte Nachlaufzeit aktiv und Ethernet.maintain() darf
    // im normalen Servicepfad nachhelfen. Es gibt weiterhin keinen zyklischen
    // Ethernet.begin()-Suchlauf im Messloop.
    if (Ethernet.begin(mac, ETH_DHCP_TIMEOUT_MS, ETH_DHCP_RESPONSE_TIMEOUT_MS) != 0 &&
        ethIpIsValid(Ethernet.localIP()))
    {
      ethConfigured = true;
      ethDhcpPending = false;
    }
    else
    {
      ethConfigured = true;
      ethDhcpPending = true;
      ethDhcpPendingUntilMs = millis() + ETH_DHCP_POST_BEGIN_GRACE_MS;
    }
  }
  else
  {
    // Static-IP nur starten, wenn die gespeicherte Konfiguration vollstaendig
    // plausibel ist. Wichtig: Die Plausibilitaetspruefung veraendert DHCP nicht
    // mehr automatisch. Dadurch kann der Benutzer IP/Subnet/Gateway/DNS in Ruhe
    // editieren, ohne dass DHCP beim naechsten interfaceConfigLoad() wieder auf
    // EIN springt. Ungueltige Static-IP blockiert den Boot nicht und ruft kein
    // Ethernet.begin() auf.
    if (!interfaceEthStaticConfigValid())
    {
      ethConfigured = false;
    }
    else if (!ethStaticLinkAvailable(mac))
    {
      // Kein PHY-Link: Static-IP nicht starten. Dadurch kann ein fehlendes
      // Netzwerkkabel nicht mehr in Ethernet.begin(mac, ip, ...) festhaengen.
      // Es gibt keinen automatischen Retry im Messloop; Neustart oder
      // Setup-Aktion startet Ethernet erneut.
      ethConfigured = false;
    }
    else
    {
      IPAddress ip      = ethMakeIp(ethLastStaticIp);
      IPAddress dns     = ethMakeIp(ethLastDns);
      IPAddress gateway = ethMakeIp(ethLastGateway);
      IPAddress subnet  = ethMakeIp(ethLastSubnet);

      Ethernet.begin(mac, ip, dns, gateway, subnet);
      // Statische IP blockiert nicht auf DHCP und ist nach begin() sofort eine
      // gueltige lokale Konfiguration. linkStatus() wird hier nicht als harte
      // Erfolgsbedingung benutzt, weil NativeEthernet den PHY-Status kurz
      // Unknown/LinkOFF melden kann. Der gefaehrliche Fall ohne Kabel wurde
      // bereits vorher durch ethStaticLinkAvailable() abgefangen.
      ethConfigured = ethIpIsValid(Ethernet.localIP());
    }
  }

  safetyEndBlockingOperation();

  ethBeginServerIfNeeded();
  ethBeginMdnsIfNeeded();
}

static void ETH_FLASHMEM_NOINLINE ethReadConfigSnapshot(bool force)
{
  uint32_t nowMs = millis();
  if (!force && (nowMs - ethLastConfigCheckMs) < 1000UL) return;
  ethLastConfigCheckMs = nowMs;

  bool enabled = interfaceEthEnabled();
  bool dhcp = interfaceEthDhcpEnabled();
  bool webEnabled = interfaceWebServerEnabled();
  uint16_t webPort = interfaceEthWebPort();
  if (webPort < 1) webPort = 80;

  uint8_t ip[4], subnet[4], gateway[4], dns[4];
  interfaceEthGetStaticIp(ip);
  interfaceEthGetSubnet(subnet);
  interfaceEthGetGateway(gateway);
  interfaceEthGetDns(dns);

  // Netzwerkparameter und Webserver-Parameter getrennt behandeln.
  // Webserver Ein/Aus darf Ethernet.begin() NICHT erneut starten, sonst gibt es bei
  // fehlendem Ethernet wieder den langen DHCP-Block.
  bool networkChanged = force ||
                        enabled != ethLastEnabled ||
                        dhcp != ethLastDhcp ||
                        ethArrayChanged(ip, ethLastStaticIp) ||
                        ethArrayChanged(subnet, ethLastSubnet) ||
                        ethArrayChanged(gateway, ethLastGateway) ||
                        ethArrayChanged(dns, ethLastDns);

  bool webChanged = force ||
                    webEnabled != ethLastWebEnabled ||
                    webPort != ethLastWebPort;

  ethLastEnabled = enabled;
  ethLastDhcp = dhcp;
  ethLastWebEnabled = webEnabled;
  ethLastWebPort = webPort;
  ethCopyIp(ethLastStaticIp, ip);
  ethCopyIp(ethLastSubnet, subnet);
  ethCopyIp(ethLastGateway, gateway);
  ethCopyIp(ethLastDns, dns);

  if (networkChanged)
  {
    ethConfigureNow();
    return;
  }

  if (webChanged)
  {
    ethDestroyServer();
    ethBeginServerIfNeeded();
  }
}

void FLASHMEM ethernetServiceBegin(void)
{
  // Beim Boot niemals eine alte Web-Setup-Sperre uebernehmen.
  // Die echte Sperre beginnt erst wieder mit einem neuen /setup-Request.
  ethWebSetupCloseSession();
  ethReadConfigSnapshot(true);
  // Die Netzwerkschnittstelle wird ab V0.50.0_18 erst nach sichtbarem
  // TFT-Bootscreen gestartet. Der Ethernet-Startversuch passiert nur hier
  // bzw. nach Setup-Aktion; im Mess-Loop gibt es keinen automatischen
  // Ethernet.begin()-Suchlauf.
  ethAcceptRequestsAfterMs = 0;
  ethStartupRequestGuardArmed = false;
}

// Wird vom Ethernet-Menue nach der Warte-Seite aufgerufen.
// Dadurch passiert die kurze blockierende Initialisierung sofort, solange
// "Bitte warten" sichtbar ist - und nicht erst spaeter im Ethernet-Menue.
void FLASHMEM ethernetServiceApplyNow(void)
{
  ethReadConfigSnapshot(true);
}

bool FLASHMEM ethernetIsEnabled(void)
{
  return ethLastEnabled;
}

bool FLASHMEM ethernetHasValidIp(void)
{
  // Bedeutet ab _24: Es existiert eine gueltige lokale IP-Konfiguration.
  // Der PHY-Link wird separat bewertet. linkStatus() darf hier nicht mehr
  // hart eingerechnet werden, weil sonst eine gueltige DHCP-Adresse unsichtbar
  // bleibt, wenn NativeEthernet LinkOFF/Unknown meldet.
  if (!ethLastEnabled || !ethConfigured) return false;
  return ethIpIsValid(Ethernet.localIP());
}

void FLASHMEM ethernetGetCurrentIp(uint8_t ip[4])
{
  if (ip == nullptr) return;

  ip[0] = 0;
  ip[1] = 0;
  ip[2] = 0;
  ip[3] = 0;

  if (!ethernetHasValidIp()) return;

  IPAddress local = Ethernet.localIP();
  ip[0] = local[0];
  ip[1] = local[1];
  ip[2] = local[2];
  ip[3] = local[3];
}

static void FLASHMEM ethPrintHttpHeader(EthBoundedWriter& client,
                                             const char* contentType,
                                             bool noCache,
                                             size_t contentLength)
{
  client.print(F("HTTP/1.1 200 OK\r\n"));
  client.print(F("Connection: close\r\n"));
  client.print(F("Content-Type: "));
  client.print(contentType);
  client.print(F("\r\nContent-Length: "));
  client.print((unsigned long)contentLength);
  client.print(F("\r\n"));
  if (noCache)
  {
    client.print(F("Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"));
    client.print(F("Pragma: no-cache\r\n"));
    client.print(F("Expires: 0\r\n"));
  }
  client.print(F("\r\n"));
}

static void FLASHMEM ethSendBufferedBody(EthBoundedWriter& client,
                                          const char* contentType,
                                          bool noCache,
                                          const char* body,
                                          size_t bodyLength)
{
  ethPrintHttpHeader(client, contentType, noCache, bodyLength);
  if (body != nullptr && bodyLength > 0U)
  {
    client.write((const uint8_t*)body, bodyLength);
  }
}

static void FLASHMEM ethSendNotFound(EthBoundedWriter& client)
{
  static const char body[] = "404";
  client.print(F("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n"));
  client.write((const uint8_t*)body, sizeof(body) - 1U);
}

static void FLASHMEM ethSendNoContent(EthBoundedWriter& client)
{
  client.print(F("HTTP/1.1 204 No Content\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"));
}

static void FLASHMEM ethSendText(EthBoundedWriter& client, bool ok, const char* msg)
{
  const size_t bodyLength = (msg != nullptr) ? strlen(msg) : 0U;
  client.print(ok ? F("HTTP/1.1 200 OK\r\n") : F("HTTP/1.1 400 Bad Request\r\n"));
  client.print(F("Connection: close\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nContent-Length: "));
  client.print((unsigned long)bodyLength);
  client.print(F("\r\n\r\n"));
  if (bodyLength > 0U) client.write((const uint8_t*)msg, bodyLength);
}

static bool FLASHMEM ethGetQueryInt(const char* requestLine, const char* key, int* value)
{
  if (requestLine == nullptr || key == nullptr || value == nullptr) return false;

  const char* q = strchr(requestLine, '?');
  const char* end = strchr(requestLine + 4, ' ');
  if (q == nullptr || end == nullptr || q >= end) return false;

  size_t keyLen = strlen(key);
  const char* p = q + 1;

  while (p < end)
  {
    const char* next = strchr(p, '&');
    if (next == nullptr || next > end) next = end;

    if ((size_t)(next - p) > keyLen && strncmp(p, key, keyLen) == 0 && p[keyLen] == '=')
    {
      const char* v = p + keyLen + 1;
      bool neg = false;
      if (v < next && *v == '-')
      {
        neg = true;
        v++;
      }
      if (v >= next || *v < '0' || *v > '9') return false;

      long n = 0;
      while (v < next && *v >= '0' && *v <= '9')
      {
        n = (n * 10) + (*v - '0');
        if (n > 3000) return false;
        v++;
      }
      if (v != next) return false;
      *value = neg ? -(int)n : (int)n;
      return true;
    }

    p = next + 1;
  }

  return false;
}

static bool FLASHMEM ethGetQueryU32(const char* requestLine, const char* key, uint32_t* value)
{
  if (requestLine == nullptr || key == nullptr || value == nullptr) return false;

  const char* q = strchr(requestLine, '?');
  const char* end = strchr(requestLine + 4, ' ');
  if (q == nullptr || end == nullptr || q >= end) return false;

  const size_t keyLen = strlen(key);
  const char* p = q + 1;

  while (p < end)
  {
    const char* next = strchr(p, '&');
    if (next == nullptr || next > end) next = end;

    if ((size_t)(next - p) > keyLen && strncmp(p, key, keyLen) == 0 && p[keyLen] == '=')
    {
      const char* v = p + keyLen + 1;
      if (v >= next || *v < '0' || *v > '9') return false;

      uint32_t n = 0;
      while (v < next && *v >= '0' && *v <= '9')
      {
        const uint8_t digit = (uint8_t)(*v - '0');
        if (n > (UINT32_MAX - digit) / 10UL) return false;
        n = (n * 10UL) + digit;
        v++;
      }
      if (v != next) return false;
      *value = n;
      return true;
    }

    p = next + 1;
  }

  return false;
}

static bool FLASHMEM ethGetQueryI32(const char* requestLine, const char* key, int32_t* value)
{
  if (requestLine == nullptr || key == nullptr || value == nullptr) return false;

  const char* q = strchr(requestLine, '?');
  const char* end = strchr(requestLine + 4, ' ');
  if (q == nullptr || end == nullptr || q >= end) return false;

  const size_t keyLen = strlen(key);
  const char* p = q + 1;

  while (p < end)
  {
    const char* next = strchr(p, '&');
    if (next == nullptr || next > end) next = end;

    if ((size_t)(next - p) > keyLen && strncmp(p, key, keyLen) == 0 && p[keyLen] == '=')
    {
      const char* v = p + keyLen + 1;
      bool negative = false;
      if (v < next && *v == '-')
      {
        negative = true;
        v++;
      }
      if (v >= next || *v < '0' || *v > '9') return false;

      const uint32_t limit = negative ? 2147483648UL : 2147483647UL;
      uint32_t n = 0;
      while (v < next && *v >= '0' && *v <= '9')
      {
        const uint8_t digit = (uint8_t)(*v - '0');
        if (n > (limit - digit) / 10UL) return false;
        n = (n * 10UL) + digit;
        v++;
      }
      if (v != next) return false;

      if (negative)
      {
        *value = (n == 2147483648UL) ? INT32_MIN : -(int32_t)n;
      }
      else
      {
        *value = (int32_t)n;
      }
      return true;
    }

    p = next + 1;
  }

  return false;
}

static const char* FLASHMEM ethBaseName(const char* path)
{
  if (path == nullptr) return "";
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static bool FLASHMEM ethFileNameEquals(const char* a, const char* b)
{
  if (a == nullptr || b == nullptr) return false;

  while (*a != '\0' && *b != '\0')
  {
    char ca = *a++;
    char cb = *b++;
    if (ca >= 'a' && ca <= 'z') ca = (char)(ca - ('a' - 'A'));
    if (cb >= 'a' && cb <= 'z') cb = (char)(cb - ('a' - 'A'));
    if (ca != cb) return false;
  }

  return *a == '\0' && *b == '\0';
}

static int8_t FLASHMEM ethFileReadFaultIndex(const char* name)
{
  if (name == nullptr || *name == '\0') return -1;
  for (uint8_t i = 0U; i < ethFileReadFaultCount; i++)
  {
    if (ethFileNameEquals(name, ethFileReadFaults[i].name)) return (int8_t)i;
  }
  return -1;
}

static bool FLASHMEM ethFileHasReadFault(const char* name, uint32_t* offsetOut, uint8_t* stageOut)
{
  int8_t index = ethFileReadFaultIndex(name);
  if (index < 0) return false;
  const EthFileReadFault& fault = ethFileReadFaults[(uint8_t)index];
  if (offsetOut) *offsetOut = fault.offset;
  if (stageOut) *stageOut = fault.stage;
  return true;
}

static void FLASHMEM ethFileMarkReadFault(const char* name, uint32_t offset,
                                         uint8_t stage, bool peltierCut)
{
  if (name == nullptr || *name == '\0') return;

  int8_t existing = ethFileReadFaultIndex(name);
  uint8_t index;
  if (existing >= 0)
  {
    index = (uint8_t)existing;
  }
  else if (ethFileReadFaultCount < ETH_FILE_READ_FAULT_MAX)
  {
    index = ethFileReadFaultCount++;
  }
  else
  {
    index = ethFileReadFaultNext;
    ethFileReadFaultNext = (uint8_t)((ethFileReadFaultNext + 1U) % ETH_FILE_READ_FAULT_MAX);
  }

  size_t nameLen = strlen(name);
  if (nameLen >= sizeof(ethFileReadFaults[index].name))
    nameLen = sizeof(ethFileReadFaults[index].name) - 1U;
  memcpy(ethFileReadFaults[index].name, name, nameLen);
  ethFileReadFaults[index].name[nameLen] = '\0';
  ethFileReadFaults[index].offset = offset;
  ethFileReadFaults[index].stage = stage;

  size_t noticeNameLen = strlen(name);
  if (noticeNameLen >= sizeof(ethDownloadFaultNoticeName))
    noticeNameLen = sizeof(ethDownloadFaultNoticeName) - 1U;
  memcpy(ethDownloadFaultNoticeName, name, noticeNameLen);
  ethDownloadFaultNoticeName[noticeNameLen] = '\0';
  ethDownloadFaultNoticeOffset = offset;
  ethDownloadFaultNoticeStage = stage;
  ethDownloadFaultNoticePeltierCut = peltierCut;
  ethDownloadFaultNoticeSeq++;
  if (ethDownloadFaultNoticeSeq == 0U) ethDownloadFaultNoticeSeq = 1U;
}

static void FLASHMEM ethGetActiveLogBaseName(char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0U) return;
  out[0] = '\0';

  int currentYear = year();
  if (currentYear >= 2020 && currentYear <= 2099)
  {
    snprintf(out, outSize, "%02u%02u%02u%s.CSV",
             (unsigned)(currentYear % 100),
             (unsigned)month(),
             (unsigned)day(),
             interfaceSdDiagnosticDataEnabled() ? "D" : "");
    return;
  }

  const char* fallback = ethBaseName(sdLogGetCurrentFile());
  size_t copyLen = strlen(fallback);
  if (copyLen >= outSize) copyLen = outSize - 1U;
  if (copyLen > 0U) memcpy(out, fallback, copyLen);
  out[copyLen] = '\0';
}

// Gemeinsame Aktiv-Erkennung fuer Dateiliste und Download-Handler.
// Bei gueltiger RTC ist ausschliesslich die aktuell aus Datum und Modus
// gebildete Tagesdatei aktiv. Ein zuvor verwendetes LOG000.CSV bleibt damit
// nach dem Setzen der Uhr geschlossen und kann normal heruntergeladen werden.
static bool FLASHMEM ethCsvFileIsActive(const char* name)
{
  if (!interfaceSdLoggingEnabled() || name == nullptr || *name == '\0') return false;

  char activeName[32];
  ethGetActiveLogBaseName(activeName, sizeof(activeName));
  return *activeName != '\0' && ethFileNameEquals(name, activeName);
}

static bool FLASHMEM ethNameHasExtension(const char* name, const char* extUpper)
{
  if (name == nullptr || extUpper == nullptr) return false;
  size_t len = strlen(name);
  size_t extLen = strlen(extUpper);
  if (len < extLen) return false;

  const char* p = name + len - extLen;
  for (size_t i = 0; i < extLen; i++)
  {
    char c = p[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
    if (c != extUpper[i]) return false;
  }
  return true;
}

static bool FLASHMEM ethSafeSerialTraceName(const char* name)
{
  if (name == nullptr || strlen(name) != 13U) return false;

  static const char prefix[] = "SERIAL";
  for (size_t i = 0; i < sizeof(prefix) - 1U; i++)
  {
    char c = name[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
    if (c != prefix[i]) return false;
  }

  if (name[6] < '0' || name[6] > '9' ||
      name[7] < '0' || name[7] > '9' ||
      name[8] < '0' || name[8] > '9') return false;

  return ethNameHasExtension(name, ".TXT");
}

static bool FLASHMEM ethSafeLogFileName(const char* name)
{
  if (name == nullptr) return false;
  size_t len = strlen(name);
  if (len < 5U || len > 31U) return false;

  for (size_t i = 0; i < len; i++)
  {
    char c = name[i];
    bool allowed = (c >= '0' && c <= '9') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z') ||
                   c == '_' || c == '-' || c == '.';
    if (!allowed) return false;
  }

  if (ethNameHasExtension(name, ".CSV")) return true;
  return ethSafeSerialTraceName(name);
}

static bool FLASHMEM ethGetQueryFilename(const char* requestLine,
                                         const char* key,
                                         char* out,
                                         size_t outSize)
{
  if (requestLine == nullptr || key == nullptr || out == nullptr || outSize < 2U) return false;
  out[0] = '\0';

  const char* q = strchr(requestLine, '?');
  const char* end = strchr(requestLine + 4, ' ');
  if (q == nullptr || end == nullptr || q >= end) return false;

  size_t keyLen = strlen(key);
  const char* p = q + 1;
  while (p < end)
  {
    const char* next = strchr(p, '&');
    if (next == nullptr || next > end) next = end;

    if ((size_t)(next - p) > keyLen && strncmp(p, key, keyLen) == 0 && p[keyLen] == '=')
    {
      const char* v = p + keyLen + 1;
      size_t len = (size_t)(next - v);
      if (len == 0U || len >= outSize) return false;
      memcpy(out, v, len);
      out[len] = '\0';
      return ethSafeLogFileName(out);
    }
    p = next + 1;
  }
  return false;
}

static bool FLASHMEM ethEnsureSdReadyForAccess(void)
{
  if (sdLogReadyForLogging()) return true;

  // Ein bewusst angeforderter Dateizugriff darf die Karte einmalig
  // initialisieren, auch wenn das normale SD-Logging ausgeschaltet ist.
  // Die Safety kennt diesen moeglicherweise laengeren SD.begin()-Abschnitt.
  safetyBeginBlockingOperation();
  bool ready = sdLogEnsureReadyForAccess();
  safetyEndBlockingOperation();
  return ready;
}

static bool FLASHMEM ethGetQueryToken(const char* requestLine,
                                       const char* key,
                                       char* out,
                                       size_t outSize)
{
  if (requestLine == nullptr || key == nullptr || out == nullptr || outSize < 2U) return false;
  out[0] = '\0';

  const char* q = strchr(requestLine, '?');
  const char* end = strchr(requestLine + 4, ' ');
  if (q == nullptr || end == nullptr || q >= end) return false;

  const size_t keyLen = strlen(key);
  const char* p = q + 1;
  while (p < end)
  {
    const char* next = strchr(p, '&');
    if (next == nullptr || next > end) next = end;

    if ((size_t)(next - p) > keyLen && strncmp(p, key, keyLen) == 0 && p[keyLen] == '=')
    {
      const char* v = p + keyLen + 1;
      const size_t len = (size_t)(next - v);
      if (len == 0U || len >= outSize) return false;
      memcpy(out, v, len);
      out[len] = '\0';
      return true;
    }
    p = next + 1;
  }
  return false;
}

static int FLASHMEM ethFileNameCompareIgnoreCase(const char* a, const char* b)
{
  if (a == nullptr) a = "";
  if (b == nullptr) b = "";
  while (*a != '\0' && *b != '\0')
  {
    char ca = *a++;
    char cb = *b++;
    if (ca >= 'a' && ca <= 'z') ca = (char)(ca - ('a' - 'A'));
    if (cb >= 'a' && cb <= 'z') cb = (char)(cb - ('a' - 'A'));
    if (ca < cb) return -1;
    if (ca > cb) return 1;
  }
  if (*a != '\0') return 1;
  if (*b != '\0') return -1;
  return 0;
}

static bool FLASHMEM ethLeapYear(uint16_t y)
{
  return ((y % 4U) == 0U && ((y % 100U) != 0U || (y % 400U) == 0U));
}

static uint8_t FLASHMEM ethDaysInMonth(uint16_t y, uint8_t m)
{
  switch (m)
  {
    case 1U: case 3U: case 5U: case 7U: case 8U: case 10U: case 12U:
      return 31U;
    case 4U: case 6U: case 9U: case 11U:
      return 30U;
    case 2U:
      return ethLeapYear(y) ? 29U : 28U;
    default:
      return 0U;
  }
}

static bool FLASHMEM ethValidCalendarDate(uint16_t y, uint8_t m, uint8_t d)
{
  if (y < 2020U || y > 2099U) return false;
  uint8_t maxDay = ethDaysInMonth(y, m);
  return maxDay != 0U && d >= 1U && d <= maxDay;
}

static bool FLASHMEM ethDecodeCsvDateFromName(const char* name,
                                              uint16_t* yearOut,
                                              uint8_t* monthOut,
                                              uint8_t* dayOut)
{
  if (name == nullptr || !ethNameHasExtension(name, ".CSV")) return false;

  const size_t len = strlen(name);
  if (!(len == 10U || len == 11U)) return false;

  for (uint8_t i = 0U; i < 6U; i++)
  {
    if (name[i] < '0' || name[i] > '9') return false;
  }

  if (len == 10U)
  {
    if (name[6] != '.') return false;
  }
  else
  {
    if ((name[6] != 'D' && name[6] != 'd') || name[7] != '.') return false;
  }

  uint16_t yy = (uint16_t)((name[0] - '0') * 10 + (name[1] - '0'));
  uint8_t mo = (uint8_t)((name[2] - '0') * 10 + (name[3] - '0'));
  uint8_t da = (uint8_t)((name[4] - '0') * 10 + (name[5] - '0'));
  uint16_t fullYear = (uint16_t)(2000U + yy);

  if (!ethValidCalendarDate(fullYear, mo, da)) return false;

  if (yearOut) *yearOut = fullYear;
  if (monthOut) *monthOut = mo;
  if (dayOut) *dayOut = da;
  return true;
}

static void FLASHMEM ethFileIndexRepairCsvDateFromName(EthFileIndexEntry& item)
{
  uint16_t nameYear = 0U;
  uint8_t nameMonth = 0U;
  uint8_t nameDay = 0U;
  if (!ethDecodeCsvDateFromName(item.name, &nameYear, &nameMonth, &nameDay)) return;

  // Die SD/FAT-Zeit kann beim Start kurz ein altes Default-Datum bekommen
  // (z.B. 01.01.2019), obwohl Logger/CSV-Zeit und Dateiname bereits stimmen.
  // Fuer die Web-Dateiliste wird deshalb bei CSV-Tagesdateien nur die
  // aeussere Anzeige/Sortierung korrigiert: Datum aus YYMMDD[D].CSV,
  // Uhrzeit soweit vorhanden weiter aus dem FAT-Eintrag. Gueltige FAT-Daten
  // bleiben unveraendert; eine Datei kann z.B. um 00:00 am Folgetag
  // geschlossen werden.
  if (!item.hasTime || !ethValidCalendarDate(item.year, item.month, item.day))
  {
    item.hasTime = 1U;
    item.year = nameYear;
    item.month = nameMonth;
    item.day = nameDay;
  }
}

static bool FLASHMEM ethFileIndexEntryNewer(const EthFileIndexEntry& a,
                                             const EthFileIndexEntry& b)
{
  if (a.hasTime != b.hasTime) return a.hasTime > b.hasTime;
  if (a.hasTime)
  {
    if (a.year != b.year) return a.year > b.year;
    if (a.month != b.month) return a.month > b.month;
    if (a.day != b.day) return a.day > b.day;
    if (a.hour != b.hour) return a.hour > b.hour;
    if (a.minute != b.minute) return a.minute > b.minute;
    if (a.second != b.second) return a.second > b.second;
  }

  const int nameCmp = ethFileNameCompareIgnoreCase(a.name, b.name);
  if (nameCmp != 0) return nameCmp > 0;
  return a.order > b.order;
}

static void FLASHMEM ethFileIndexInsert(const EthFileIndexEntry& candidate)
{
  uint16_t pos = 0U;
  while (pos < ethFileIndexCount && !ethFileIndexEntryNewer(candidate, ethFileIndex[pos])) pos++;

  if (ethFileIndexCount < ETH_FILE_INDEX_MAX)
  {
    if (pos < ethFileIndexCount)
    {
      memmove(&ethFileIndex[pos + 1U], &ethFileIndex[pos],
              (size_t)(ethFileIndexCount - pos) * sizeof(EthFileIndexEntry));
    }
    ethFileIndex[pos] = candidate;
    ethFileIndexCount++;
    return;
  }

  ethFileIndexTruncated = true;
  if (pos >= ETH_FILE_INDEX_MAX) return;
  if (pos + 1U < ETH_FILE_INDEX_MAX)
  {
    memmove(&ethFileIndex[pos + 1U], &ethFileIndex[pos],
            (size_t)(ETH_FILE_INDEX_MAX - pos - 1U) * sizeof(EthFileIndexEntry));
  }
  ethFileIndex[pos] = candidate;
}

static void FLASHMEM ethFileIndexCloseDirectory(void)
{
  if (ethFileIndexDir) ethFileIndexDir.close();
}

static void FLASHMEM ethFileIndexStop(void)
{
  ethFileIndexCloseDirectory();
  ethFileIndexState = ETH_FILE_INDEX_EMPTY;
  ethFileIndexErrorCode = 0U;
  ethFileIndexCount = 0U;
  ethFileIndexScanned = 0U;
  ethFileIndexValidSeen = 0U;
  ethFileIndexOrder = 0U;
  ethFileIndexTruncated = false;
}

static void FLASHMEM ethFileIndexRequestRefresh(void)
{
  ethFileIndexCloseDirectory();
  ethFileIndexState = ETH_FILE_INDEX_NEED_SD;
  ethFileIndexErrorCode = 0U;
  ethFileIndexCount = 0U;
  ethFileIndexScanned = 0U;
  ethFileIndexValidSeen = 0U;
  ethFileIndexOrder = 0U;
  ethFileIndexTruncated = false;
}

static void FLASHMEM ethFileIndexService(void)
{
  if (ethFileIndexState == ETH_FILE_INDEX_EMPTY ||
      ethFileIndexState == ETH_FILE_INDEX_READY ||
      ethFileIndexState == ETH_FILE_INDEX_ERROR) return;

  if (ethFileIndexState == ETH_FILE_INDEX_NEED_SD)
  {
    if (!ethEnsureSdReadyForAccess())
    {
      ethFileIndexErrorCode = 1U;
      ethFileIndexState = ETH_FILE_INDEX_ERROR;
      return;
    }
    ethFileIndexState = ETH_FILE_INDEX_NEED_DIR;
    return;
  }

  if (ethFileIndexState == ETH_FILE_INDEX_NEED_DIR)
  {
    ethFileIndexDir = SD.open("/LOG");
    if (!ethFileIndexDir || !ethFileIndexDir.isDirectory())
    {
      ethFileIndexCloseDirectory();
      ethFileIndexErrorCode = 2U;
      ethFileIndexState = ETH_FILE_INDEX_ERROR;
      return;
    }
    ethFileIndexState = ETH_FILE_INDEX_SCANNING;
    return;
  }

  // Pro Hauptloop genau einen Verzeichniseintrag bearbeiten. Damit wird auch
  // eine sehr grosse /LOG-Liste nie als langer, zusammenhaengender SD-Zugriff
  // in die Mess-/Regel-Schleife geschoben.
  File entry = ethFileIndexDir.openNextFile();
  if (!entry)
  {
    ethFileIndexCloseDirectory();
    ethFileIndexState = ETH_FILE_INDEX_READY;
    return;
  }

  ethFileIndexScanned++;
  const char* base = ethBaseName(entry.name());
  if (!entry.isDirectory() && ethSafeLogFileName(base))
  {
    EthFileIndexEntry item = {};
    strncpy(item.name, base, sizeof(item.name) - 1U);
    item.name[sizeof(item.name) - 1U] = '\0';
    item.size = (uint32_t)entry.size();
    item.kind = ethSafeSerialTraceName(base) ? ETH_FILE_KIND_SERIAL : ETH_FILE_KIND_CSV;
    item.order = ethFileIndexOrder++;

    DateTimeFields tm = {};
    if (entry.getModifyTime(tm) && tm.year >= 80U && tm.mon < 12U &&
        tm.mday >= 1U && tm.mday <= 31U && tm.hour < 24U &&
        tm.min < 60U && tm.sec < 60U)
    {
      item.hasTime = 1U;
      item.year = (uint16_t)(tm.year + 1900U);
      item.month = (uint8_t)(tm.mon + 1U);
      item.day = tm.mday;
      item.hour = tm.hour;
      item.minute = tm.min;
      item.second = tm.sec;
    }

    ethFileIndexRepairCsvDateFromName(item);

    ethFileIndexValidSeen++;
    if (ethFileIndexValidSeen > ETH_FILE_INDEX_MAX) ethFileIndexTruncated = true;
    ethFileIndexInsert(item);
  }
  entry.close();
}

static bool FLASHMEM ethFileIndexMatchesKind(const EthFileIndexEntry& item, uint8_t filter)
{
  if (filter == 1U) return item.kind == ETH_FILE_KIND_CSV;
  if (filter == 2U) return item.kind == ETH_FILE_KIND_SERIAL;
  return true;
}

static bool FLASHMEM ethFileIndexMatchesDate(const EthFileIndexEntry& item,
                                             uint16_t yearFilter,
                                             uint8_t monthFilter)
{
  if (yearFilter == 0U && monthFilter == 0U) return true;
  if (!item.hasTime) return false;
  if (yearFilter != 0U && item.year != yearFilter) return false;
  if (monthFilter != 0U && item.month != monthFilter) return false;
  return true;
}

static bool FLASHMEM ethFileIndexMatches(const EthFileIndexEntry& item,
                                         uint8_t filter,
                                         uint16_t yearFilter,
                                         uint8_t monthFilter)
{
  return ethFileIndexMatchesKind(item, filter) &&
         ethFileIndexMatchesDate(item, yearFilter, monthFilter);
}

static void FLASHMEM ethSendFilesJson(EthBoundedWriter& client, const char* requestLine)
{
  int pageInt = 0;
  if (strchr(requestLine, '?') != nullptr &&
      ethGetQueryInt(requestLine, "page", &pageInt) && pageInt < 0)
  {
    ethSendText(client, false, "Ungültige Seitennummer / Invalid page number");
    return;
  }

  char filterText[12] = "all";
  const bool hasFilter = ethGetQueryToken(requestLine, "filter", filterText, sizeof(filterText));
  uint8_t filter = 0U;
  if (hasFilter)
  {
    if (strcmp(filterText, "all") == 0) filter = 0U;
    else if (strcmp(filterText, "csv") == 0) filter = 1U;
    else if (strcmp(filterText, "serial") == 0) filter = 2U;
    else
    {
      ethSendText(client, false, "Ungültiger Filter / Invalid filter");
      return;
    }
  }

  int yearInt = 0;
  if (ethGetQueryInt(requestLine, "year", &yearInt) &&
      yearInt != 0 && (yearInt < 2020 || yearInt > 2099))
  {
    ethSendText(client, false, "Ungültiger Jahresfilter / Invalid year filter");
    return;
  }
  uint16_t yearFilter = (yearInt >= 2020 && yearInt <= 2099) ? (uint16_t)yearInt : 0U;

  int monthInt = 0;
  if (ethGetQueryInt(requestLine, "month", &monthInt) &&
      monthInt != 0 && (monthInt < 1 || monthInt > 12))
  {
    ethSendText(client, false, "Ungültiger Monatsfilter / Invalid month filter");
    return;
  }
  uint8_t monthFilter = (monthInt >= 1 && monthInt <= 12) ? (uint8_t)monthInt : 0U;

  int refreshInt = 0;
  if (ethGetQueryInt(requestLine, "refresh", &refreshInt) && refreshInt != 0)
  {
    ethFileIndexRequestRefresh();
  }
  else if (ethFileIndexState == ETH_FILE_INDEX_EMPTY)
  {
    ethFileIndexRequestRefresh();
  }

  if (ethFileIndexState == ETH_FILE_INDEX_ERROR)
  {
    const bool english = (ui_language == LANG_EN);
    const char* message = ethFileIndexErrorCode == 2U
                        ? (english ? "Directory /LOG is not available"
                                   : "Verzeichnis /LOG nicht verfügbar")
                        : (english ? "SD card not ready"
                                   : "SD-Karte nicht bereit");
    int n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
                     "{\"ready\":false,\"lang\":%u,\"message\":\"%s\",\"files\":[]}",
                     (unsigned)ui_language, message);
    if (n <= 0 || (size_t)n >= sizeof(ethJsonBuf))
    {
      ethSendText(client, false, "Dateilistenfehler / File list error");
      return;
    }
    ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                        ethJsonBuf, (size_t)n);
    return;
  }

  if (ethFileIndexState != ETH_FILE_INDEX_READY)
  {
    int n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
                     "{\"ready\":true,\"lang\":%u,\"indexing\":true,\"scanned\":%u,\"found\":%u,\"files\":[]}",
                     (unsigned)ui_language,
                     (unsigned)ethFileIndexScanned,
                     (unsigned)ethFileIndexValidSeen);
    if (n <= 0 || (size_t)n >= sizeof(ethJsonBuf))
    {
      ethSendText(client, false, "Dateilistenfehler / File list error");
      return;
    }
    ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                        ethJsonBuf, (size_t)n);
    return;
  }

  bool yearOptions[80] = {};
  bool monthOptions[12] = {};
  uint16_t total = 0U;
  for (uint16_t i = 0U; i < ethFileIndexCount; i++)
  {
    const EthFileIndexEntry& item = ethFileIndex[i];
    if (!ethFileIndexMatchesKind(item, filter)) continue;

    if (item.hasTime && ethValidCalendarDate(item.year, item.month, item.day))
    {
      yearOptions[item.year - 2020U] = true;
      if (yearFilter == 0U || item.year == yearFilter)
      {
        monthOptions[item.month - 1U] = true;
      }
    }

    if (ethFileIndexMatchesDate(item, yearFilter, monthFilter)) total++;
  }

  uint16_t pages = total == 0U ? 1U
                              : (uint16_t)((total + ETH_FILE_PAGE_SIZE - 1U) / ETH_FILE_PAGE_SIZE);
  uint16_t page = pageInt > 0 ? (uint16_t)pageInt : 0U;
  if (page >= pages) page = (uint16_t)(pages - 1U);
  const uint16_t skip = (uint16_t)(page * ETH_FILE_PAGE_SIZE);

  const bool serialTraceActive = serialProtocolTraceIsActive();
  const char* serialTraceBase = ethBaseName(serialProtocolTraceFileName());

  EthMemoryWriter body(ethJsonBuf, sizeof(ethJsonBuf));
  body.print(F("{\"ready\":true,\"lang\":"));
  body.print((unsigned)ui_language);
  body.print(F(",\"indexing\":false,\"page\":"));
  body.print((unsigned)page);
  body.print(F(",\"pages\":"));
  body.print((unsigned)pages);
  body.print(F(",\"total\":"));
  body.print((unsigned)total);
  body.print(F(",\"year\":"));
  body.print((unsigned)yearFilter);
  body.print(F(",\"month\":"));
  body.print((unsigned)monthFilter);
  body.print(F(",\"years\":["));
  bool firstOption = true;
  for (uint8_t yi = 0U; yi < 80U; yi++)
  {
    if (!yearOptions[yi]) continue;
    if (!firstOption) body.print(',');
    firstOption = false;
    body.print((unsigned)(2020U + yi));
  }
  body.print(F("],\"months\":["));
  firstOption = true;
  for (uint8_t mi = 0U; mi < 12U; mi++)
  {
    if (!monthOptions[mi]) continue;
    if (!firstOption) body.print(',');
    firstOption = false;
    body.print((unsigned)(mi + 1U));
  }
  body.print(F("]"));
  body.print(F(",\"truncated\":"));
  body.print(ethFileIndexTruncated ? F("true") : F("false"));
  body.print(F(",\"faultSeq\":"));
  body.print((unsigned long)ethDownloadFaultNoticeSeq);
  body.print(F(",\"faultName\":\""));
  body.print(ethDownloadFaultNoticeName);
  body.print(F("\",\"faultOffset\":"));
  body.print((unsigned long)ethDownloadFaultNoticeOffset);
  body.print(F(",\"faultStage\":"));
  body.print((unsigned)ethDownloadFaultNoticeStage);
  body.print(F(",\"faultPeltierCut\":"));
  body.print(ethDownloadFaultNoticePeltierCut ? F("true") : F("false"));
  body.print(F(",\"downloadActive\":"));
  body.print(ethDownloadActive ? F("true") : F("false"));
  body.print(F(",\"downloadName\":\""));
  body.print(ethDownloadActive ? ethDownloadCurrentName : "");
  body.print(F("\",\"files\":["));

  uint16_t matched = 0U;
  uint8_t returned = 0U;
  bool first = true;
  for (uint16_t i = 0U; i < ethFileIndexCount && returned < ETH_FILE_PAGE_SIZE; i++)
  {
    const EthFileIndexEntry& item = ethFileIndex[i];
    if (!ethFileIndexMatches(item, filter, yearFilter, monthFilter)) continue;
    if (matched++ < skip) continue;

    const bool active = ethCsvFileIsActive(item.name) ||
                        (serialTraceActive && ethFileNameEquals(item.name, serialTraceBase));
    uint32_t faultOffset = 0U;
    uint8_t faultStage = SD_DIAG_IDLE;
    const bool readFault = ethFileHasReadFault(item.name, &faultOffset, &faultStage);
    char modified[32] = "---";
    if (item.hasTime)
    {
      snprintf(modified, sizeof(modified), "%02u.%02u.%04u %02u:%02u:%02u",
               (unsigned)item.day, (unsigned)item.month, (unsigned)item.year,
               (unsigned)item.hour, (unsigned)item.minute, (unsigned)item.second);
    }

    if (!first) body.print(',');
    first = false;
    char jsonItem[256];
    int n = snprintf(jsonItem, sizeof(jsonItem),
                     "{\"name\":\"%s\",\"size\":%lu,\"active\":%s,\"fault\":%s,\"faultOffset\":%lu,\"faultStage\":%u,\"type\":\"%s\",\"modified\":\"%s\"}",
                     item.name,
                     (unsigned long)item.size,
                     active ? "true" : "false",
                     readFault ? "true" : "false",
                     (unsigned long)faultOffset,
                     (unsigned)faultStage,
                     item.kind == ETH_FILE_KIND_SERIAL ? "serial" : "csv",
                     modified);
    if (n <= 0 || (size_t)n >= sizeof(jsonItem))
    {
      ethSendText(client, false, "Dateieintrag zu gross / File entry too large");
      return;
    }
    body.write((const uint8_t*)jsonItem, (size_t)n);
    returned++;
  }

  body.print(F("]}"));
  if (!body.ok())
  {
    ethSendText(client, false, "Dateiliste zu gross / File list too large");
    return;
  }
  ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                      ethJsonBuf, body.length());
}

enum EthRangeParseResult : uint8_t
{
  ETH_RANGE_NONE = 0,
  ETH_RANGE_VALID,
  ETH_RANGE_INVALID
};

static bool FLASHMEM ethParseUint32Span(const char* begin,
                                        const char* end,
                                        uint32_t* value)
{
  if (begin == nullptr || end == nullptr || value == nullptr || begin >= end) return false;
  uint64_t n = 0ULL;
  for (const char* p = begin; p < end; p++)
  {
    if (*p < '0' || *p > '9') return false;
    n = n * 10ULL + (uint64_t)(*p - '0');
    if (n > 0xFFFFFFFFULL) return false;
  }
  *value = (uint32_t)n;
  return true;
}

static bool FLASHMEM ethGetQueryUint32(const char* requestLine,
                                       const char* key,
                                       uint32_t* value)
{
  if (requestLine == nullptr || key == nullptr || value == nullptr) return false;
  const char* q = strchr(requestLine, '?');
  const char* end = strchr(requestLine + 4, ' ');
  if (q == nullptr || end == nullptr || q >= end) return false;

  const size_t keyLen = strlen(key);
  const char* p = q + 1;
  while (p < end)
  {
    const char* next = strchr(p, '&');
    if (next == nullptr || next > end) next = end;
    if ((size_t)(next - p) > keyLen && strncmp(p, key, keyLen) == 0 && p[keyLen] == '=')
    {
      return ethParseUint32Span(p + keyLen + 1, next, value);
    }
    p = next + 1;
  }
  return false;
}

static EthRangeParseResult FLASHMEM ethParseRangeHeader(const char* headers,
                                                         uint32_t totalSize,
                                                         uint32_t* startOut,
                                                         uint32_t* endOut)
{
  if (startOut == nullptr || endOut == nullptr) return ETH_RANGE_INVALID;
  if (headers == nullptr) return ETH_RANGE_NONE;

  const char* line = headers;
  while (*line != '\0')
  {
    const char* next = strchr(line, '\n');
    const char* lineEnd = next != nullptr ? next : line + strlen(line);
    while (lineEnd > line && (lineEnd[-1] == '\r' || lineEnd[-1] == '\n')) lineEnd--;

    static const char rangeName[] = "Range:";
    size_t lineLength = (size_t)(lineEnd - line);
    if (ethHeaderNameEquals(line, lineLength, rangeName))
    {
      const char* p = line + sizeof(rangeName) - 1U;
      while (p < lineEnd && (*p == ' ' || *p == '\t')) p++;
      static const char bytesPrefix[] = "bytes=";
      if ((size_t)(lineEnd - p) < sizeof(bytesPrefix) - 1U ||
          strncmp(p, bytesPrefix, sizeof(bytesPrefix) - 1U) != 0)
      {
        return ETH_RANGE_INVALID;
      }
      p += sizeof(bytesPrefix) - 1U;

      // Nur ein einzelner Bereich wird benoetigt. Suffix-Ranges und mehrere
      // durch Komma getrennte Bereiche werden bewusst abgelehnt.
      const char* dash = (const char*)memchr(p, '-', (size_t)(lineEnd - p));
      if (dash == nullptr || dash == p) return ETH_RANGE_INVALID;
      if (memchr(p, ',', (size_t)(lineEnd - p)) != nullptr) return ETH_RANGE_INVALID;

      uint32_t start = 0U;
      if (!ethParseUint32Span(p, dash, &start)) return ETH_RANGE_INVALID;
      if (totalSize == 0U || start >= totalSize) return ETH_RANGE_INVALID;

      uint32_t end = totalSize - 1U;
      const char* endBegin = dash + 1;
      while (endBegin < lineEnd && (*endBegin == ' ' || *endBegin == '\t')) endBegin++;
      if (endBegin < lineEnd)
      {
        if (!ethParseUint32Span(endBegin, lineEnd, &end)) return ETH_RANGE_INVALID;
        if (end < start) return ETH_RANGE_INVALID;
        if (end >= totalSize) end = totalSize - 1U;
      }

      *startOut = start;
      *endOut = end;
      return ETH_RANGE_VALID;
    }

    if (next == nullptr) break;
    line = next + 1;
  }
  return ETH_RANGE_NONE;
}

static void FLASHMEM ethSendRangeNotSatisfiable(EthBoundedWriter& client,
                                                 uint32_t totalSize)
{
  client.print(F("HTTP/1.1 416 Range Not Satisfiable\r\nConnection: close\r\n"));
  client.print(F("Accept-Ranges: bytes\r\nContent-Range: bytes */"));
  client.print((unsigned long)totalSize);
  client.print(F("\r\nContent-Length: 0\r\n\r\n"));
}

static bool FLASHMEM ethStartDownload(EthBoundedWriter& client,
                                      EthernetClient& rawClient,
                                      const char* requestLine,
                                      const char* requestHeaders)
{
  char name[32];
  if (!ethGetQueryFilename(requestLine, "name", name, sizeof(name)))
  {
    ethSendText(client, false, "Ungültiger Dateiname / Invalid filename");
    return false;
  }

  if (ethFileHasReadFault(name, nullptr, nullptr))
  {
    ethSendText(client, false, "Datei wegen Lesefehler gesperrt / File locked after read error");
    return false;
  }

  if (!ethEnsureSdReadyForAccess())
  {
    ethSendText(client, false, "SD-Karte nicht bereit / SD card not ready");
    return false;
  }

  int snapshotInt = 0;
  bool snapshotRequested = ethGetQueryInt(requestLine, "snapshot", &snapshotInt) && snapshotInt == 1;

  const bool activeCsv = ethCsvFileIsActive(name);
  const bool activeSerialTrace = serialProtocolTraceIsActive() &&
      ethFileNameEquals(name, ethBaseName(serialProtocolTraceFileName()));
  bool activeFile = activeCsv || activeSerialTrace;
  if (activeFile && !snapshotRequested)
  {
    ethSendText(client, false, "Aktive Datei nur als Snapshot verfügbar / Active file only available as snapshot");
    return false;
  }

  strncpy(ethDownloadCurrentName, name, sizeof(ethDownloadCurrentName) - 1U);
  ethDownloadCurrentName[sizeof(ethDownloadCurrentName) - 1U] = '\0';
  ethDownloadCurrentOffset = 0U;

  char path[40];
  int pathLen = snprintf(path, sizeof(path), "/LOG/%s", name);
  if (pathLen <= 0 || (size_t)pathLen >= sizeof(path))
  {
    ethDownloadCurrentName[0] = '\0';
    ethSendText(client, false, "Dateipfad zu lang / File path too long");
    return false;
  }

  sdStorageDiagBegin(SD_DIAG_DOWNLOAD_OPEN);
  ethDownloadIoBegin(ETH_DOWNLOAD_IO_SD);
  ethDownloadFile = SD.open(path, FILE_READ);
  uint8_t ioStall = ethDownloadIoEnd();
  sdStorageDiagEnd(SD_DIAG_DOWNLOAD_OPEN);
  if (ethDownloadHandleIoStall(ioStall, SD_DIAG_DOWNLOAD_OPEN, 0U))
  {
    ethDownloadClose();
    ethSendText(client, false, "Datei nicht lesbar / File read error");
    return false;
  }

  if (!ethDownloadFile || ethDownloadFile.isDirectory())
  {
    ethFileMarkReadFault(name, 0U, SD_DIAG_DOWNLOAD_OPEN, false);
    ethDownloadClose();
    ethSendText(client, false, "Datei nicht lesbar / File read error");
    return false;
  }

  sdStorageDiagBegin(SD_DIAG_DOWNLOAD_OPEN);
  ethDownloadIoBegin(ETH_DOWNLOAD_IO_SD);
  uint64_t fullSize = (uint64_t)ethDownloadFile.size();
  ioStall = ethDownloadIoEnd();
  sdStorageDiagEnd(SD_DIAG_DOWNLOAD_OPEN);
  if (ethDownloadHandleIoStall(ioStall, SD_DIAG_DOWNLOAD_OPEN, 0U))
  {
    ethDownloadClose();
    ethSendText(client, false, "Dateigröße nicht lesbar / File size read error");
    return false;
  }

  if (fullSize > 0xFFFFFFFFULL)
  {
    ethDownloadClose();
    ethSendText(client, false, "Datei ist zu groß / File is too large");
    return false;
  }

  uint32_t fileSize = (uint32_t)fullSize;

  // Bei aktiven Snapshot-Dateien wird die beim Aufbau der Dateiliste sichtbare
  // Groesse in der URL festgehalten. Ein spaeterer Range-Retry verwendet so
  // exakt denselben Snapshot, auch wenn das Logging inzwischen weiterlief.
  uint32_t snapshotLimit = 0U;
  if (activeFile && snapshotRequested &&
      ethGetQueryUint32(requestLine, "limit", &snapshotLimit) &&
      snapshotLimit < fileSize)
  {
    fileSize = snapshotLimit;
  }

  uint32_t rangeStart = 0U;
  uint32_t rangeEnd = fileSize > 0U ? fileSize - 1U : 0U;
  EthRangeParseResult rangeResult = ethParseRangeHeader(
      requestHeaders, fileSize, &rangeStart, &rangeEnd);
  if (rangeResult == ETH_RANGE_INVALID)
  {
    ethDownloadClose();
    ethSendRangeNotSatisfiable(client, fileSize);
    return false;
  }

  const bool partialResponse = rangeResult == ETH_RANGE_VALID;
  uint32_t responseLength = partialResponse
                          ? (rangeEnd - rangeStart + 1U)
                          : fileSize;

  if (rangeStart > 0U)
  {
    sdStorageDiagBegin(SD_DIAG_DOWNLOAD_SEEK);
    ethDownloadIoBegin(ETH_DOWNLOAD_IO_SD);
    bool seekOk = ethDownloadFile.seek(rangeStart);
    ioStall = ethDownloadIoEnd();
    sdStorageDiagEnd(SD_DIAG_DOWNLOAD_SEEK);
    const bool seekStalled = ethDownloadHandleIoStall(
        ioStall, SD_DIAG_DOWNLOAD_SEEK, rangeStart);
    if (seekStalled || !seekOk)
    {
      if (!seekStalled)
        ethFileMarkReadFault(name, rangeStart, SD_DIAG_DOWNLOAD_SEEK, false);
      ethDownloadClose();
      ethSendText(client, false, "Dateiposition nicht lesbar / File seek error");
      return false;
    }
  }
  ethDownloadCurrentOffset = rangeStart;

  char downloadName[48];
  const char* responseName = name;
  if (activeFile && snapshotRequested)
  {
    size_t sourceLen = strlen(name);
    size_t extLen = sourceLen >= 4U ? 4U : 0U;
    size_t stemLen = sourceLen - extLen;
    const char* ext = extLen ? name + stemLen : "";
    static const char suffix[] = "_SNAPSHOT";
    size_t maxStem = sizeof(downloadName) - sizeof(suffix) - extLen;
    if (stemLen > maxStem) stemLen = maxStem;
    memcpy(downloadName, name, stemLen);
    memcpy(downloadName + stemLen, suffix, sizeof(suffix) - 1U);
    memcpy(downloadName + stemLen + sizeof(suffix) - 1U, ext, extLen);
    downloadName[stemLen + sizeof(suffix) - 1U + extLen] = '\0';
    responseName = downloadName;
  }

  const char* contentType = ethNameHasExtension(name, ".TXT")
                          ? "text/plain; charset=utf-8"
                          : "text/csv; charset=utf-8";

  client.print(partialResponse ? F("HTTP/1.1 206 Partial Content\r\nContent-Type: ")
                               : F("HTTP/1.1 200 OK\r\nContent-Type: "));
  client.print(contentType);
  client.print(F("\r\nAccept-Ranges: bytes\r\nETag: \"TP3000-"));
  client.print((unsigned long)fileSize);
  client.print('-');
  client.print(name);
  client.print(F("\"\r\n"));
  if (partialResponse)
  {
    client.print(F("Content-Range: bytes "));
    client.print((unsigned long)rangeStart);
    client.print('-');
    client.print((unsigned long)rangeEnd);
    client.print('/');
    client.print((unsigned long)fileSize);
    client.print(F("\r\n"));
  }
  client.print(F("Content-Disposition: attachment; filename=\""));
  client.print(responseName);
  client.print(F("\"\r\nContent-Length: "));
  client.print((unsigned long)responseLength);
  if (activeFile && snapshotRequested)
  {
    client.print(F("\r\nX-TP3000-Snapshot: 1"));
  }
  client.print(F("\r\nCache-Control: private, no-cache\r\nConnection: close\r\n\r\n"));

  if (!client.ok())
  {
    ethDownloadClose();
    return false;
  }

  ethDownloadClient = rawClient;
  ethDownloadResetAdaptive();
  ethDownloadActive = true;
  ethDownloadFinishing = false;
  ethDownloadRemaining = responseLength;
  ethDownloadPendingLength = 0U;
  ethDownloadPendingOffset = 0U;
  ethDownloadLastProgressMs = millis();
  ethDownloadCloseAfterMs = 0;
  if (responseLength == 0U)
  {
    ethDownloadFinishing = true;
    ethDownloadCloseAfterMs = ethDownloadLastProgressMs + ETH_DOWNLOAD_CLOSE_DELAY_MS;
  }

  return true;
}

static bool FLASHMEM ethDateValid(int y, int m, int d)
{
  // RV-3129-C3 Watch-Year: 20xx, BCD-Zaehler 00..79.
  // TP-3000 akzeptiert fuer das Stellen der Uhr bewusst 2026..2079.
  if (y < 2026 || y > 2079) return false;
  if (m < 1 || m > 12) return false;

  int dim = 31;
  if (m == 4 || m == 6 || m == 9 || m == 11)
  {
    dim = 30;
  }
  else if (m == 2)
  {
    bool leap = ((y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0));
    dim = leap ? 29 : 28;
  }

  return d >= 1 && d <= dim;
}

static void FLASHMEM ethHandleSetTime(EthBoundedWriter& client, const char* requestLine)
{
  int y = 0;
  int mo = 0;
  int d = 0;
  int h = 0;
  int mi = 0;
  int sec = 0;

  bool ok = ethGetQueryInt(requestLine, "y", &y) &&
            ethGetQueryInt(requestLine, "mo", &mo) &&
            ethGetQueryInt(requestLine, "d", &d) &&
            ethGetQueryInt(requestLine, "h", &h) &&
            ethGetQueryInt(requestLine, "mi", &mi) &&
            ethGetQueryInt(requestLine, "s", &sec);

  if (!ok || !ethDateValid(y, mo, d) || h < 0 || h > 23 || mi < 0 || mi > 59 || sec < 0 || sec > 59)
  {
    ethSendText(client, false, (ui_language == LANG_EN)
                              ? "Invalid time parameters"
                              : "Ungültige Zeitparameter");
    return;
  }

  setTime(h, mi, sec, d, mo, y);
  schreibeEchtzeitUhr();

  char msg[112];
  snprintf(msg, sizeof(msg), "OK %02d.%02d.%04d %02d:%02d:%02d%s",
           d, mo, y, h, mi, sec,
           rtc_online ? " RTC"
                      : ((ui_language == LANG_EN)
                         ? " TimeLib (RTC offline)"
                         : " TimeLib (RTC nicht erreichbar)"));
  ethSendText(client, true, msg);
}

static const char* FLASHMEM ethModeText()
{
  if (aktuellerModus == 1) return "Kühlen";
  if (aktuellerModus == 2) return "Heizen";
  return "Aus";
}

static const uint8_t ETH_SD_ACTIVITY_NONE   = 0;
static const uint8_t ETH_SD_ACTIVITY_CSV    = 1;
static const uint8_t ETH_SD_ACTIVITY_SERIAL = 2;
static const uint8_t ETH_SD_ACTIVITY_BOTH   = 3;
static const uint8_t ETH_SD_ACTIVITY_ERROR  = 4;

static const char* FLASHMEM ethSdStateText()
{
  switch (sdStorageActivityState())
  {
    case ETH_SD_ACTIVITY_CSV:    return "csv";
    case ETH_SD_ACTIVITY_SERIAL: return "serial";
    case ETH_SD_ACTIVITY_BOTH:   return "csv+serial";
    case ETH_SD_ACTIVITY_ERROR:  return "fehler";
    case ETH_SD_ACTIVITY_NONE:
    default:                     return "aus";
  }
}

static const char* FLASHMEM ethSdColorText()
{
  if (sdStorageWriteBlinkActive()) return "yellow";

  switch (sdStorageActivityState())
  {
    case ETH_SD_ACTIVITY_CSV:    return "green";
    case ETH_SD_ACTIVITY_SERIAL: return "blue";
    case ETH_SD_ACTIVITY_BOTH:   return "purple";
    case ETH_SD_ACTIVITY_ERROR:  return "red";
    case ETH_SD_ACTIVITY_NONE:
    default:                     return "white";
  }
}

static const char* FLASHMEM ethFanColorText()
{
  if (!sensorFanIsEnabled()) return "white";
  return sensorFanTachoMissing() ? "yellow" : "green";
}

// Muss zur Statuslogik im TFT-Hauptscreen passen.
// Die Webansicht zeigt damit denselben Messwert-Rahmen und denselben
// Status vor "Safety" wie das Display.
static const uint32_t ETH_MESSWERT_INIT_MIN_MS = 10000UL;
static const uint32_t ETH_MESSWERT_STABILISIERZEIT_MS = 30000UL;

static bool FLASHMEM ethMeasurementStatusValuesFinite()
{
  return isfinite(relativeFeuchte) && isfinite(präziserTaupunkt) &&
         isfinite(tempSpiegel) && isfinite(tempUmgebung);
}

static bool FLASHMEM ethMeasurementStatusHardError()
{
  if (mcp3202_fehler) return true;
  if (ads1263_bereit && !ethMeasurementStatusValuesFinite()) return true;
  return false;
}

static bool FLASHMEM ethMeasurementStatusValuesValid()
{
  return ethMeasurementStatusValuesFinite() && ads1263_bereit && !mcp3202_fehler;
}

static bool FLASHMEM ethMeasurementStatusInInitMinTime()
{
  return (uint32_t)millis() < ETH_MESSWERT_INIT_MIN_MS;
}

static const char* FLASHMEM ethMeasurementStatusText()
{
  if (safetyIsFaultActive()) return safetyGetStopStatusText();
  if (alarmVisualActive())   return T(TXT_STATUS_LIMIT);
  if (ethMeasurementStatusInInitMinTime()) return T(TXT_STATUS_INIT);
  if (ethMeasurementStatusHardError()) return T(TXT_STATUS_ERROR);
  if (ads1263_bereit && ads1263RefIsError()) return T(TXT_STATUS_REF_ERROR);
  if (!ethMeasurementStatusValuesValid()) return T(TXT_STATUS_INIT);
  if (ablaufStatus == 1) return T(TXT_STATUS_PREHEAT);
  if (ablaufStatus == 2) return T(TXT_STATUS_AUTOCAL);
  if ((uint32_t)(millis() - statusTimer) < ETH_MESSWERT_STABILISIERZEIT_MS) return T(TXT_STATUS_SETTLING);
  if (ads1263_bereit && ads1263RefIsOld()) return T(TXT_STATUS_REF_OLD);
  if (opticHealthMainWarningActive())
  {
    return (ui_language == LANG_EN) ? opticHealthMainStatusTextEN() : opticHealthMainStatusTextDE();
  }
  return T(TXT_STATUS_STABLE);
}

static const char* FLASHMEM ethMeasurementStatusColorText()
{
  if (safetyIsFaultActive() || alarmVisualActive()) return "red";
  if (ethMeasurementStatusInInitMinTime()) return "gray";
  if (ethMeasurementStatusHardError()) return "red";
  if (ads1263_bereit && ads1263RefIsError()) return "red";
  if (!ethMeasurementStatusValuesValid()) return "gray";
  if (ablaufStatus == 1) return "orange";
  if (ablaufStatus == 2) return "cyan";
  if ((uint32_t)(millis() - statusTimer) < ETH_MESSWERT_STABILISIERZEIT_MS) return "yellow";
  if (opticHealthMainWarningActive()) return "orange";
  return "green";
}

static void FLASHMEM ethIpToText(char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0) return;

  uint8_t ip[4];
  ethernetGetCurrentIp(ip);

  if (ethernetHasValidIp())
  {
    snprintf(out, outSize, "%03u.%03u.%03u.%03u",
             (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
  }
  else
  {
    snprintf(out, outSize, "---.---.---.---");
  }
}

static float FLASHMEM ethSafeFloat(float v, float fallback)
{
  return isfinite(v) ? v : fallback;
}

static float FLASHMEM ethOpticPercent()
{
  extern float optikReflexion;
  extern float optikTrockenReferenz;
  if (isfinite(optikTrockenReferenz) && fabsf(optikTrockenReferenz) > 1.0f)
  {
    return (optikReflexion / optikTrockenReferenz) * 100.0f;
  }
  return NAN;
}

static const char* FLASHMEM ethAlmemoDisplayUnit(uint8_t index)
{
  switch (interfaceAlmemoRole(index))
  {
    case 1: return "°C";
    case 2: return "L";
    case 3: return "h";
    default: return "";
  }
}

static void FLASHMEM ethSendJson(EthBoundedWriter& client)
{
  char ipText[20];
  ethIpToText(ipText, sizeof(ipText));

  const bool flowEnabled = interfaceFlowDisplayEnabled();
  const char* flowUnit = interfaceFlowDisplayUnitText();
  if (flowUnit == nullptr) flowUnit = "";

  float flow = serialFlowLastValueLMin();
  bool flowValid = flowEnabled && isfinite(flow);

  float opticPct = ethOpticPercent();
  bool opticValid = isfinite(opticPct);

  char flowText[16];
  if (flowValid) snprintf(flowText, sizeof(flowText), "%.2f", flow);
  else snprintf(flowText, sizeof(flowText), "--.--");

  const bool almemoExtraEnabled = interfaceAlmemoAnyChannelEnabled();
  uint8_t almemoExtraIndex = interfaceAlmemoDisplayIndex();
  float almemoExtraValue = serialAlmemoLastValue(almemoExtraIndex);
  const bool almemoExtraValid = almemoExtraEnabled && isfinite(almemoExtraValue);
  const bool extraEnabled = flowEnabled || almemoExtraEnabled;
  const char* extraType = almemoExtraEnabled ? "text" : "flow";
  const char* extraLabel = almemoExtraEnabled ? interfaceAlmemoDisplayLabel(almemoExtraIndex) : "F:";
  const char* extraUnit = almemoExtraEnabled ? ethAlmemoDisplayUnit(almemoExtraIndex) : flowUnit;
  const char* extraColor = (almemoExtraEnabled && !almemoExtraValid) ? "red" : "yellow";
  char extraValue[16];
  if (almemoExtraEnabled)
  {
    if (almemoExtraValid) snprintf(extraValue, sizeof(extraValue), "%.2f", (double)almemoExtraValue);
    else snprintf(extraValue, sizeof(extraValue), "--.--");
  }
  else
  {
    snprintf(extraValue, sizeof(extraValue), "%s", flowText);
  }

  char opticText[16];
  if (opticValid) snprintf(opticText, sizeof(opticText), "%.1f", opticPct);
  else snprintf(opticText, sizeof(opticText), "--.-");

  output_data_sample_t outSample;
  if (!outputDataGetSample(interfaceOutputFilterIndex(), &outSample))
  {
    memset(&outSample, 0, sizeof(outSample));
    outSample.tMirror = tempSpiegel;
    outSample.tAmbient = tempUmgebung;
    outSample.dewpoint = präziserTaupunkt;
    outSample.rh = relativeFeuchte;
    outSample.pressure = baroDruckHPa;
  }

  const bool alarmDisplayActive = safetyIsFaultActive() || alarmVisualActive();
  const char* alarmDisplayText = safetyIsFaultActive() ? safetyGetStopStatusText() :
                                 (alarmVisualActive() ? T(TXT_STATUS_LIMIT) : "");
  const char* measurementStatusText = ethMeasurementStatusText();
  const char* measurementStatusColor = ethMeasurementStatusColorText();
  const char* fanStatusText = sensorFanStatusText();
  const char* fanStatusColor = ethFanColorText();

  const bool rhValid = isfinite(outSample.rh);
  const bool dewValid = isfinite(outSample.dewpoint);
  const bool tMirrorValid = isfinite(outSample.tMirror);
  const bool tAmbientValid = isfinite(outSample.tAmbient);
  const bool pressureValid = isfinite(outSample.pressure);
  const bool currentValid = isfinite((float)amp_avg);

  const bool directRhValid = isfinite(relativeFeuchte);
  const bool directDewValid = isfinite(präziserTaupunkt);
  const bool directTMirrorValid = isfinite(tempSpiegel);
  const bool directTAmbientValid = isfinite(tempUmgebung);
  const bool directPressureValid = isfinite(baroDruckHPa);

  int n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
           "{"
           "\"time\":\"%02u:%02u:%02u\","
           "\"date\":\"%04u-%02u-%02u\","
           "\"rh\":%.2f,"
           "\"dew\":%.3f,"
           "\"tMirror\":%.3f,"
           "\"tAmbient\":%.3f,"
           "\"pressure\":%.2f,"
           "\"rhValid\":%s,"
           "\"dewValid\":%s,"
           "\"tMirrorValid\":%s,"
           "\"tAmbientValid\":%s,"
           "\"pressureValid\":%s,"
           "\"currentValid\":%s,"
           "\"directRhValid\":%s,"
           "\"directDewValid\":%s,"
           "\"directTMirrorValid\":%s,"
           "\"directTAmbientValid\":%s,"
           "\"directPressureValid\":%s,"
           "\"directRh\":%.4f,"
           "\"directDew\":%.4f,"
           "\"directTMirror\":%.4f,"
           "\"directTAmbient\":%.4f,"
           "\"directPressure\":%.3f,"
           "\"current\":%.3f,"
           "\"mode\":\"%s\","
           "\"optic\":\"%s\","
           "\"flowEnabled\":%s,"
           "\"flowValid\":%s,"
           "\"flow\":\"%s\","
           "\"flowUnit\":\"%s\","
           "\"extraEnabled\":%s,"
           "\"extraType\":\"%s\","
           "\"extraLabel\":\"%s\","
           "\"extraValue\":\"%s\","
           "\"extraUnit\":\"%s\","
           "\"extraColor\":\"%s\","
           "\"extraIndex\":%d,"
           "\"extraRole\":%u,"
           "\"safetyFault\":%s,"
           "\"sdState\":\"%s\","
           "\"sdColor\":\"%s\","
           "\"ethOk\":%s,"
           "\"alarmActive\":%s,"
           "\"alarmText\":\"%s\","
           "\"measurementStatusText\":\"%s\","
           "\"measurementStatusColor\":\"%s\","
           "\"fanText\":\"%s\","
           "\"fanColor\":\"%s\","
           "\"webSetupEnabled\":%s,"
           "\"tftSetupActive\":%s,"
           "\"webSetupActive\":%s,"
           "\"lang\":%u,"
           "\"mainLayout\":%u,"
           "\"ip\":\"%s\""
           "}",
           (unsigned)hour(), (unsigned)minute(), (unsigned)second(),
           (unsigned)year(), (unsigned)month(), (unsigned)day(),
           (double)ethSafeFloat(outSample.rh, 0.0f),
           (double)ethSafeFloat(outSample.dewpoint, 0.0f),
           (double)ethSafeFloat(outSample.tMirror, 0.0f),
           (double)ethSafeFloat(outSample.tAmbient, 0.0f),
           (double)ethSafeFloat(outSample.pressure, 0.0f),
           rhValid ? "true" : "false",
           dewValid ? "true" : "false",
           tMirrorValid ? "true" : "false",
           tAmbientValid ? "true" : "false",
           pressureValid ? "true" : "false",
           currentValid ? "true" : "false",
           directRhValid ? "true" : "false",
           directDewValid ? "true" : "false",
           directTMirrorValid ? "true" : "false",
           directTAmbientValid ? "true" : "false",
           directPressureValid ? "true" : "false",
           (double)ethSafeFloat(relativeFeuchte, 0.0f),
           (double)ethSafeFloat(präziserTaupunkt, 0.0f),
           (double)ethSafeFloat(tempSpiegel, 0.0f),
           (double)ethSafeFloat(tempUmgebung, 0.0f),
           (double)ethSafeFloat(baroDruckHPa, 0.0f),
           (double)ethSafeFloat((float)amp_avg, 0.0f),
           ethModeText(),
           opticText,
           flowEnabled ? "true" : "false",
           flowValid ? "true" : "false",
           flowText,
           flowUnit,
           extraEnabled ? "true" : "false",
           extraType,
           extraLabel,
           extraValue,
           extraUnit,
           extraColor,
           almemoExtraEnabled ? (int)almemoExtraIndex : -1,
           almemoExtraEnabled ? (unsigned)interfaceAlmemoRole(almemoExtraIndex) : 0U,
           safetyIsFaultActive() ? "true" : "false",
           ethSdStateText(),
           ethSdColorText(),
           ethernetHasValidIp() ? "true" : "false",
           alarmDisplayActive ? "true" : "false",
           alarmDisplayText,
           measurementStatusText,
           measurementStatusColor,
           fanStatusText,
           fanStatusColor,
           interfaceWebSetupEnabled() ? "true" : "false",
           flag.config_mode ? "true" : "false",
           ethernetWebSetupSessionActive() ? "true" : "false",
           (unsigned)ui_language,
           (unsigned)mainScreenLayoutGet(),
           ipText);

  if (n < 0 || (size_t)n >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Daten-JSON zu gross / Data JSON too large");
    return;
  }

  ethSendBufferedBody(client, "application/json", true,
                      ethJsonBuf, (size_t)n);
}

static void FLASHMEM ethPrintJsonFloatOrNull(Print& client, float value, uint8_t digits)
{
  if (isfinite(value))
  {
    client.print(value, digits);
  }
  else
  {
    client.print(F("null"));
  }
}

static void FLASHMEM ethPrintChartScale(Print& client, uint8_t range, uint8_t metric)
{
  bool valid = false;
  float dataMin = 0.0f;
  float dataMax = 0.0f;
  float axisMin = 0.0f;
  const bool rhMetric = (metric == ETH_CHART_METRIC_RH);
  float axisMax = rhMetric ? 1.0f : 0.2f;

  mainChartHistoryGetScale(range,
                           metric,
                           &valid,
                           &dataMin,
                           &dataMax,
                           &axisMin,
                           &axisMax);

  client.print(F("{\"valid\":"));
  client.print(valid ? F("true") : F("false"));
  client.print(F(",\"dataMin\":"));
  ethPrintJsonFloatOrNull(client, dataMin, rhMetric ? 4 : 5);
  client.print(F(",\"dataMax\":"));
  ethPrintJsonFloatOrNull(client, dataMax, rhMetric ? 4 : 5);
  client.print(F(",\"axisMin\":"));
  ethPrintJsonFloatOrNull(client, axisMin, rhMetric ? 4 : 5);
  client.print(F(",\"axisMax\":"));
  ethPrintJsonFloatOrNull(client, axisMax, rhMetric ? 4 : 5);
  client.print('}');
}

static void FLASHMEM ethSendChartJson(EthBoundedWriter& client, const char* requestLine)
{
  const uint16_t capacity = mainChartHistoryCapacity();
  const uint8_t rangeCount = mainChartHistoryRangeCount();

  int rangeInt = (int)mainChartHistoryActiveRange();
  if (ethGetQueryInt(requestLine, "r", &rangeInt))
  {
    if (rangeInt < 0) rangeInt = 0;
    if (rangeInt >= (int)rangeCount) rangeInt = (int)rangeCount - 1;
  }
  const uint8_t range = (uint8_t)rangeInt;

  const uint32_t firstAvailable = mainChartHistoryFirstSequence(range);
  const uint32_t total = mainChartHistoryNextSequence(range);

  uint32_t requested = 0;
  const bool hasRequested = ethGetQueryU32(requestLine, "from", &requested);
  const bool reset = !hasRequested || requested < firstAvailable || requested > total;
  const uint32_t start = reset ? firstAvailable : requested;

  uint32_t remaining = total - start;
  uint16_t sendCount = (remaining > ETH_CHART_POINTS_PER_RESPONSE)
                     ? ETH_CHART_POINTS_PER_RESPONSE
                     : (uint16_t)remaining;
  const uint32_t next = start + (uint32_t)sendCount;

  EthMemoryWriter body(ethJsonBuf, sizeof(ethJsonBuf));
  body.print(F("{\"range\":"));
  body.print(range);
  body.print(F(",\"rangeCount\":"));
  body.print(rangeCount);
  body.print(F(",\"sampleMs\":"));
  body.print(mainChartHistorySampleMs(range));
  body.print(F(",\"capacity\":"));
  body.print(capacity);
  body.print(F(",\"count\":"));
  body.print(mainChartHistoryCountValue(range));
  body.print(F(",\"first\":"));
  body.print(start);
  body.print(F(",\"next\":"));
  body.print(next);
  body.print(F(",\"total\":"));
  body.print(total);
  body.print(F(",\"reset\":"));
  body.print(reset ? F("true") : F("false"));
  body.print(F(",\"more\":"));
  body.print(next < total ? F("true") : F("false"));
  body.print(F(",\"dewScale\":"));
  ethPrintChartScale(body, range, ETH_CHART_METRIC_DEW);
  body.print(F(",\"rhScale\":"));
  ethPrintChartScale(body, range, ETH_CHART_METRIC_RH);
  body.print(F(",\"taScale\":"));
  ethPrintChartScale(body, range, ETH_CHART_METRIC_T_AMBIENT);
  body.print(F(",\"points\":["));

  for (uint16_t i = 0; i < sendCount && body.ok(); i++)
  {
    if (i > 0) body.print(',');

    float dew = NAN;
    float rh = NAN;
    float ambient = NAN;
    const bool ok = mainChartHistoryGetPoint(range, start + (uint32_t)i, &dew, &rh, &ambient);

    body.print('[');
    if (ok) ethPrintJsonFloatOrNull(body, dew, 5);
    else body.print(F("null"));
    body.print(',');
    if (ok) ethPrintJsonFloatOrNull(body, rh, 4);
    else body.print(F("null"));
    body.print(',');
    if (ok) ethPrintJsonFloatOrNull(body, ambient, 5);
    else body.print(F("null"));
    body.print(']');
  }

  body.print(F("]}"));
  if (!body.ok())
  {
    ethSendText(client, false, "Chart-JSON zu gross / Chart JSON too large");
    return;
  }

  ethSendBufferedBody(client, "application/json", true,
                      ethJsonBuf, body.length());
}

static bool FLASHMEM ethSetupAccessAllowed(EthBoundedWriter& client, bool requireTftIdle)
{
  if (!interfaceWebSetupEnabled())
  {
    ethSendText(client, false, "Web-Setup deaktiviert / Web setup disabled");
    return false;
  }

  ethWebSetupTouch();

  if (requireTftIdle && flag.config_mode)
  {
    ethSendText(client, false, "TFT-Setup ist aktiv / TFT setup is active");
    return false;
  }

  return true;
}

static bool FLASHMEM ethSetupControlPeriodValid(int value)
{
  return value == 62 || value == 100 || value == 120 ||
         value == 142 || value == 180 || value == 250;
}

static void FLASHMEM ethSendSetupPage(EthBoundedWriter& client,
                                      EthernetClient& rawClient,
                                      const char* requestHeaders,
                                      bool* keepOpen)
{
  if (!ethSetupAccessAllowed(client, false)) return;

  const EthPageSegment segments[] = {
    { ethSetupPage, sizeof(ethSetupPage) - 1U }
  };
  ethBeginPageTransfer(rawClient, client, requestHeaders, ETH_ETAG_SETUP,
                       ETH_REQ_SETUP, segments,
                       (uint8_t)(sizeof(segments) / sizeof(segments[0])),
                       sizeof(ethSetupPage) - 1U, keepOpen);
}

static void FLASHMEM ethSendSetupJson(EthBoundedWriter& client,
                                             const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, false)) return;

  int part = -1;
  if (!ethGetQueryInt(requestLine, "part", &part) || part < 0 || part > 2)
  {
    ethSendText(client, false, "Setup-Teil ungültig / Invalid setup part");
    return;
  }

  int n = -1;

  if (part == 0)
  {
    const float kiValue = isfinite(R.pid_ki) ? R.pid_ki : 0.0f;
    const float kdValue = isfinite(R.pid_kd) ? R.pid_kd : 0.0f;
    const int ki10 = (int)lroundf(kiValue * 10.0f);
    const int kd10 = (int)lroundf(kdValue * 10.0f);

    n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
      "{"
        "\"part\":0,\"lang\":%u,\"tftBusy\":%s,"
        "\"ui\":{\"mainLayout\":%u},"
        "\"control\":{"
          "\"kp\":%u,\"ki10\":%d,\"kd10\":%d,"
          "\"period\":%u,\"optik10\":%u,\"freeTemp\":%u,"
          "\"autoIdx\":%u,\"adcFilter\":%u,\"sfocal\":%u,\"fan\":%u,\"fanEnabled\":%s,\"fanMissing\":%s,\"peltier\":%u},"
        "\"alarm\":{"
          "\"mode\":%u,\"tauL10\":%d,\"tauH10\":%d,"
          "\"rhL10\":%u,\"rhH10\":%u,\"buzzer\":%s},"
        "\"head\":{\"type\":%u,\"typeText\":\"%s\",\"serial\":%lu,\"device\":\"%s\",\"model\":\"%s\"}"
      "}",
      (unsigned)ui_language,
      flag.config_mode ? "true" : "false",
      (unsigned)mainScreenLayoutGet(),
      (unsigned)R.pid_kp, ki10, kd10,
      (unsigned)R.regler_intervall_ms,
      (unsigned)R.optik_sollwert,
      (unsigned)R.freiheiz_ziel_temp[0],
      (unsigned)R.optik_autocal_interval_index,
      (unsigned)adcFilterModeGet(),
      (unsigned)adc1SfocalModeGet(),
      (unsigned)R.fan_percent,
      sensorFanIsEnabled() ? "true" : "false",
      sensorFanTachoMissing() ? "true" : "false",
      (unsigned)peltierCurrentLimitGetMa(),
      (unsigned)alarmGetMode(),
      (int)alarmGetTauLow10(),
      (int)alarmGetTauHigh10(),
      (unsigned)alarmGetRhLow10(),
      (unsigned)alarmGetRhHigh10(),
      alarmBuzzerEnabled() ? "true" : "false",
      (unsigned)((R.head_type < HEAD_TYPE_COUNT) ? R.head_type : HEAD_TYPE_DEFAULT),
      headTypeTextGet(),
      (unsigned long)((R.head_serial <= HEAD_SERIAL_MAX) ? R.head_serial : HEAD_SERIAL_DEFAULT),
      deviceSerialGet(),
      getHeadModelName(R.head_type));
  }
  else if (part == 1)
  {
    int32_t p2mS1 = 0, p2mI1 = 0, p2mS2 = 0, p2mI2 = 0;
    int32_t p2aS1 = 0, p2aI1 = 0, p2aS2 = 0, p2aI2 = 0;
    bool p2mActive = false;
    bool p2aActive = false;
    pt100Cal2GetScaled(0, &p2mS1, &p2mI1, &p2mS2, &p2mI2, &p2mActive);
    pt100Cal2GetScaled(1, &p2aS1, &p2aI1, &p2aS2, &p2aI2, &p2aActive);

    uint32_t refDate = 0;
    int32_t ref100 = 0, ref120 = 0;
    int32_t a100 = 0, a120 = 0, b100 = 0, b120 = 0;
    refCalGetAllScaled(&refDate, &ref100, &ref120,
                       &a100, &a120, &b100, &b120);

    n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
      "{"
        "\"part\":1,\"cal\":{"
          "\"r0m\":%ld,\"r0a\":%ld,"
          "\"p2m\":[%ld,%ld,%ld,%ld],"
          "\"p2a\":[%ld,%ld,%ld,%ld],"
          "\"p2mActive\":%s,\"p2aActive\":%s,"
          "\"refDate\":%lu,\"ref100\":%ld,\"ref120\":%ld,"
          "\"a100\":%ld,\"a120\":%ld,\"b100\":%ld,\"b120\":%ld,"
          "\"offset\":%ld}"
      "}",
      (long)pt100R0GetScaled(0),
      (long)pt100R0GetScaled(1),
      (long)p2mS1, (long)p2mI1, (long)p2mS2, (long)p2mI2,
      (long)p2aS1, (long)p2aI1, (long)p2aS2, (long)p2aI2,
      p2mActive ? "true" : "false",
      p2aActive ? "true" : "false",
      (unsigned long)refDate,
      (long)ref100, (long)ref120,
      (long)a100, (long)a120, (long)b100, (long)b120,
      (long)taupunktOffsetGetScaled());
  }
  else
  {
    char ipText[20];
    ethIpToText(ipText, sizeof(ipText));
    const char* hostText = ethGetHostname();

    char almemoStatus[2][18];
    char almemoValue[2][24];
    int8_t almemoPort = serialExternalTempPort();
    for (uint8_t ai = 0; ai < 2; ai++)
    {
      if (!interfaceAlmemoChannelEnabled(ai))
      {
        snprintf(almemoStatus[ai], sizeof(almemoStatus[ai]), "AUS");
        snprintf(almemoValue[ai], sizeof(almemoValue[ai]), "--.-- %s", interfaceAlmemoCsvUnitText(ai));
      }
      else if (serialAlmemoIsValid(ai))
      {
        snprintf(almemoStatus[ai], sizeof(almemoStatus[ai]), "OK RS232-%u", (unsigned)((almemoPort >= 0) ? (almemoPort + 1) : 0));
        snprintf(almemoValue[ai], sizeof(almemoValue[ai]), "%.2f %s", (double)serialAlmemoLastValue(ai), interfaceAlmemoCsvUnitText(ai));
      }
      else
      {
        snprintf(almemoStatus[ai], sizeof(almemoStatus[ai]), "SUCHE RS232-%u", (unsigned)((almemoPort >= 0) ? (almemoPort + 1) : 0));
        snprintf(almemoValue[ai], sizeof(almemoValue[ai]), "--.-- %s", interfaceAlmemoCsvUnitText(ai));
      }
    }

    n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
      "{"
        "\"part\":2,\"iface\":{"
          "\"outInt\":%u,\"outFilter\":%u,\"diag\":%s,"
          "\"rs\":[{\"mode\":%u,\"baud\":%u,\"output\":%u},"
                     "{\"mode\":%u,\"baud\":%u,\"output\":%u}],"
          "\"usb\":%u,\"sdLogging\":%s,\"sdOutInt\":%u,"
          "\"sdFilter\":%u,\"sdWrite\":%u,\"sdDiag\":%s,"
          "\"sdHeader\":%s,\"flow\":%u,"
          "\"win\":{\"out\":%u,\"baud\":%u,\"address\":%u,\"port\":%u,\"cycle\":%u,\"rsPort\":%d,\"eth\":%s,\"client\":%s,\"activePort\":%u},"
          "\"almemo\":{\"interval\":%u,\"trace\":%s,"
          "\"ch\":[{\"active\":%s,\"address\":%u,\"channel\":%u,\"role\":%u,\"label\":\"%s\",\"status\":\"%s\",\"value\":\"%s\",\"age\":%lu,\"errors\":%lu},"
                 "{\"active\":%s,\"address\":%u,\"channel\":%u,\"role\":%u,\"label\":\"%s\",\"status\":\"%s\",\"value\":\"%s\",\"age\":%lu,\"errors\":%lu}]}},"
        "\"eth\":{\"ok\":%s,\"host\":\"%s\",\"dhcp\":%s,\"ip\":\"%s\",\"webPort\":%u}"
      "}",
      (unsigned)interfaceOutputIntervalIndex(),
      (unsigned)interfaceOutputFilterIndex(),
      interfaceDiagnosticDataEnabled() ? "true" : "false",
      (unsigned)interfaceRs232Mode(0),
      (unsigned)interfaceRs232BaudIndex(0),
      (unsigned)interfaceRs232OutputMode(0),
      (unsigned)interfaceRs232Mode(1),
      (unsigned)interfaceRs232BaudIndex(1),
      (unsigned)interfaceRs232OutputMode(1),
      (unsigned)interfaceUsbMode(),
      interfaceSdLoggingEnabled() ? "true" : "false",
      (unsigned)interfaceSdOutputIntervalIndex(),
      (unsigned)interfaceSdOutputFilterIndex(),
      (unsigned)interfaceSdLogIntervalIndex(),
      interfaceSdDiagnosticDataEnabled() ? "true" : "false",
      interfaceSdLogHeaderEnabled() ? "true" : "false",
      (unsigned)interfaceFlowDisplayMode(),
      (unsigned)interfaceWinControlOutputMode(),
      (unsigned)interfaceWinControlBaudIndex(),
      (unsigned)interfaceWinControlAddress(),
      (unsigned)interfaceWinControlTcpPort(),
      (unsigned)interfaceWinControlCycleIndex(),
      (int)interfaceWinControlRs232Port(),
      interfaceWinControlEthernetEnabled() ? "true" : "false",
      winControlOutEthernetClientConnected() ? "true" : "false",
      (unsigned)winControlOutEthernetActivePort(),
      (unsigned)interfaceAlmemoIntervalIndex(),
      interfaceAlmemoTraceEnabled() ? "true" : "false",
      interfaceAlmemoChannelEnabled(0) ? "true" : "false",
      (unsigned)interfaceAlmemoAddressFor(0),
      (unsigned)interfaceAlmemoChannelFor(0),
      (unsigned)interfaceAlmemoRole(0),
      interfaceAlmemoDisplayLabel(0),
      almemoStatus[0],
      almemoValue[0],
      (unsigned long)((serialAlmemoAgeMs(0) == 0xFFFFFFFFUL) ? 0UL : serialAlmemoAgeMs(0)),
      (unsigned long)serialAlmemoRxErrors(0),
      interfaceAlmemoChannelEnabled(1) ? "true" : "false",
      (unsigned)interfaceAlmemoAddressFor(1),
      (unsigned)interfaceAlmemoChannelFor(1),
      (unsigned)interfaceAlmemoRole(1),
      interfaceAlmemoDisplayLabel(1),
      almemoStatus[1],
      almemoValue[1],
      (unsigned long)((serialAlmemoAgeMs(1) == 0xFFFFFFFFUL) ? 0UL : serialAlmemoAgeMs(1)),
      (unsigned long)serialAlmemoRxErrors(1),
      ethernetHasValidIp() ? "true" : "false",
      hostText,
      interfaceEthDhcpEnabled() ? "true" : "false",
      ipText,
      (unsigned)interfaceEthWebPort());
  }

  if (n < 0 || (size_t)n >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Setup JSON zu gross / Setup JSON too large");
    return;
  }

  ethSendBufferedBody(client, "application/json", true,
                      ethJsonBuf, (size_t)n);
}

static void FLASHMEM ethSendSetupLiveJson(EthBoundedWriter& client)
{
  if (!ethSetupAccessAllowed(client, false)) return;

  int n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
    "{"
      "\"lang\":%u,\"tftBusy\":%s,"
      "\"fan\":%u,\"fanEnabled\":%s,\"fanMissing\":%s,"
      "\"sdLogging\":%s,\"winClient\":%s,\"winPort\":%u"
    "}",
    (unsigned)ui_language,
    flag.config_mode ? "true" : "false",
    (unsigned)R.fan_percent,
    sensorFanIsEnabled() ? "true" : "false",
    sensorFanTachoMissing() ? "true" : "false",
    interfaceSdLoggingEnabled() ? "true" : "false",
    winControlOutEthernetClientConnected() ? "true" : "false",
    (unsigned)winControlOutEthernetActivePort());

  if (n < 0 || (size_t)n >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Setup Live JSON zu gross / Setup live JSON too large");
    return;
  }

  ethSendBufferedBody(client, "application/json", true,
                      ethJsonBuf, (size_t)n);
}

static void FLASHMEM ethSendSetupStatusJson(EthBoundedWriter& client,
                                                   const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, false)) return;

  int part = -1;
  if (!ethGetQueryInt(requestLine, "part", &part) || part < 0 || part > 1)
  {
    ethSendText(client, false, "Status-Teil ungültig / Invalid status part");
    return;
  }

  int n = -1;

  if (part == 0)
  {
    const int ledMa10 = (int)lroundf(opticHealthGetLedMA() * 10.0f);
    n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
      "{"
        "\"part\":0,\"valid\":%s,"
        "\"total\":%u,\"led\":%u,\"target\":%u,"
        "\"stability\":%u,\"dark\":%u,\"time\":%u,"
        "\"min\":%u,\"status\":%u,\"ledMa10\":%d,"
        "\"targetRaw\":%ld,\"gross\":%ld,\"darkRaw\":%ld,"
        "\"net\":%ld,\"dryRef\":%ld,\"restError\":%ld,"
        "\"noisePp\":%ld,\"durationMs\":%lu,"
        "\"coarse\":%u,\"fine\":%u"
      "}",
      opticHealthIsValid() ? "true" : "false",
      (unsigned)opticHealthGetTotal(),
      (unsigned)opticHealthGetLed(),
      (unsigned)opticHealthGetTarget(),
      (unsigned)opticHealthGetStability(),
      (unsigned)opticHealthGetDark(),
      (unsigned)opticHealthGetTime(),
      (unsigned)opticHealthGetMin(),
      (unsigned)opticHealthGetStatus(),
      ledMa10,
      (long)opticHealthGetTargetRaw(),
      (long)opticHealthGetBrutto(),
      (long)opticHealthGetDarkRaw(),
      (long)opticHealthGetNetto(),
      (long)opticHealthGetTrockenRef(),
      (long)opticHealthGetRestError(),
      (long)opticHealthGetNoisePp(),
      (unsigned long)opticHealthGetDurationMs(),
      (unsigned)opticHealthGetCoarseSteps(),
      (unsigned)opticHealthGetFineSteps());
  }
  else
  {
    const uint8_t ethStage = ethernetDiagGetStage();
    const uint8_t ethRequest = ethernetDiagGetRequest();
    const uint8_t ethMaxRequest = ethernetDiagGetMaxRequestCode();
    const uint8_t timeoutStage = safetyGetTimeoutEthStage();
    const uint8_t timeoutRequest = safetyGetTimeoutEthRequest();
    const uint8_t timeoutMaxRequest = safetyGetTimeoutEthMaxRequestCode();

    n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
      "{"
        "\"part\":1,"
        "\"ethStage\":%u,\"ethStageText\":\"%s\","
        "\"ethRequest\":%u,\"ethRequestText\":\"%s\","
        "\"ethActiveMs\":%lu,\"ethLastUs\":%lu,\"ethMaxUs\":%lu,"
        "\"ethMaxRequest\":%u,\"ethMaxRequestText\":\"%s\","
        "\"ethMaxWriteUs\":%lu,\"ethAborts\":%lu,"
        "\"timeoutLoopAgeMs\":%lu,"
        "\"timeoutEthStage\":%u,\"timeoutEthStageText\":\"%s\","
        "\"timeoutEthRequest\":%u,\"timeoutEthRequestText\":\"%s\","
        "\"timeoutEthElapsedMs\":%lu,\"timeoutEthLastUs\":%lu,"
        "\"timeoutEthMaxUs\":%lu,\"timeoutEthMaxRequest\":%u,"
        "\"timeoutEthMaxRequestText\":\"%s\",\"timeoutEthMaxWriteUs\":%lu,"
        "\"resetRaw\":%lu,\"crashReport\":%s"
      "}",
      (unsigned)ethStage,
      ethernetDiagStageText(ethStage),
      (unsigned)ethRequest,
      ethernetDiagRequestText(ethRequest),
      (unsigned long)ethernetDiagGetActiveElapsedMs(),
      (unsigned long)ethernetDiagGetLastRequestUs(),
      (unsigned long)ethernetDiagGetMaxRequestUs(),
      (unsigned)ethMaxRequest,
      ethernetDiagRequestText(ethMaxRequest),
      (unsigned long)ethernetDiagGetMaxWriteUs(),
      (unsigned long)ethernetDiagGetAbortedResponses(),
      (unsigned long)safetyGetTimeoutLoopAgeMs(),
      (unsigned)timeoutStage,
      ethernetDiagStageText(timeoutStage),
      (unsigned)timeoutRequest,
      ethernetDiagRequestText(timeoutRequest),
      (unsigned long)safetyGetTimeoutEthElapsedMs(),
      (unsigned long)safetyGetTimeoutEthLastRequestUs(),
      (unsigned long)safetyGetTimeoutEthMaxRequestUs(),
      (unsigned)timeoutMaxRequest,
      ethernetDiagRequestText(timeoutMaxRequest),
      (unsigned long)safetyGetTimeoutEthMaxWriteUs(),
      (unsigned long)systemGetBootResetStatusRaw(),
      systemGetBootCrashReportAvailable() ? "true" : "false");
  }

  if (n < 0 || (size_t)n >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Status JSON zu gross / Status JSON too large");
    return;
  }

  ethSendBufferedBody(client, "application/json", true,
                      ethJsonBuf, (size_t)n);
}



static bool FLASHMEM ethQueryPasswordMatchesDevice(const char* requestLine)
{
  char pw[DEVICE_SERIAL_DIGITS + 1U];
  if (!ethGetQueryToken(requestLine, "pw", pw, sizeof(pw))) return false;
  if (strlen(pw) != DEVICE_SERIAL_DIGITS) return false;
  for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
  {
    if (pw[i] < '0' || pw[i] > '9') return false;
  }
  return strncmp(pw, deviceSerialGet(), DEVICE_SERIAL_DIGITS) == 0;
}

static void FLASHMEM ethHandleSetupControlUnlock(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;
  if (!ethQueryPasswordMatchesDevice(requestLine))
  {
    ethSendText(client, false, "Passwort falsch / Wrong password");
    return;
  }
  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupHeadStatus(EthBoundedWriter& client)
{
  if (!ethSetupAccessAllowed(client, false)) return;
  char l1[64], l2[64];
  headCalStatusText(l1, sizeof(l1), l2, sizeof(l2));
  int n = snprintf(ethJsonBuf, sizeof(ethJsonBuf), "%s\n%s", l1, l2);
  if (n < 0 || (size_t)n >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Kopfdaten-Status zu gross / Head status too large");
    return;
  }
  ethSendBufferedBody(client, "text/plain; charset=utf-8", true, ethJsonBuf, (size_t)n);
}

static void FLASHMEM ethHandleSetupHeadList(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, false)) return;
  char typeText[HEAD_TYPE_TEXT_LEN + 1U];
  bool hasType = ethGetQueryToken(requestLine, "type", typeText, sizeof(typeText));
  int n = 0;
  if (!hasType)
  {
    char types[8][HEAD_TYPE_TEXT_LEN + 1U];
    uint8_t count = headCalListTypes(types, (uint8_t)(sizeof(types) / sizeof(types[0])));
    n = snprintf(ethJsonBuf, sizeof(ethJsonBuf), "{\"types\":[");
    for (uint8_t i = 0; i < count && n > 0 && (size_t)n < sizeof(ethJsonBuf); i++)
    {
      n += snprintf(ethJsonBuf + n, sizeof(ethJsonBuf) - (size_t)n, "%s\"%s\"", (i ? "," : ""), types[i]);
    }
    if (n > 0 && (size_t)n < sizeof(ethJsonBuf)) n += snprintf(ethJsonBuf + n, sizeof(ethJsonBuf) - (size_t)n, "]}");
  }
  else
  {
    const bool oldStyle = (strncmp(typeText, "STP", 3) == 0 &&
                           typeText[3] >= '0' && typeText[3] <= '9' &&
                           typeText[4] >= '0' && typeText[4] <= '9' &&
                           typeText[5] >= '0' && typeText[5] <= '9' &&
                           typeText[6] >= '0' && typeText[6] <= '9' &&
                           typeText[7] == '\0');
    if (!headTypeTextValid(typeText) && !oldStyle)
    {
      ethSendText(client, false, "Kopftyp ungültig / Invalid head type");
      return;
    }
    char norm[HEAD_TYPE_TEXT_LEN + 1U];
    strncpy(norm, typeText, sizeof(norm) - 1U);
    norm[sizeof(norm) - 1U] = '\0';
    headTypeTextNormalize(norm, sizeof(norm));
    if (!headTypeTextValid(norm))
    {
      ethSendText(client, false, "Kopftyp ungültig / Invalid head type");
      return;
    }
    strncpy(typeText, norm, sizeof(typeText) - 1U);
    typeText[sizeof(typeText) - 1U] = '\0';
    uint32_t serials[16];
    uint8_t count = headCalListSerials(typeText, serials, (uint8_t)(sizeof(serials) / sizeof(serials[0])));
    n = snprintf(ethJsonBuf, sizeof(ethJsonBuf), "{\"type\":\"%s\",\"serials\":[", typeText);
    for (uint8_t i = 0; i < count && n > 0 && (size_t)n < sizeof(ethJsonBuf); i++)
    {
      n += snprintf(ethJsonBuf + n, sizeof(ethJsonBuf) - (size_t)n, "%s%lu", (i ? "," : ""), (unsigned long)serials[i]);
    }
    if (n > 0 && (size_t)n < sizeof(ethJsonBuf)) n += snprintf(ethJsonBuf + n, sizeof(ethJsonBuf) - (size_t)n, "]}");
  }
  if (n < 0 || (size_t)n >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Kopfdaten-Liste zu gross / Head list too large");
    return;
  }
  ethSendBufferedBody(client, "application/json", true, ethJsonBuf, (size_t)n);
}

static bool FLASHMEM ethGetHeadTypeSerial(const char* requestLine, char* typeOut, size_t typeOutSize, uint32_t* serialOut)
{
  char typeText[HEAD_TYPE_TEXT_LEN + 1U];
  uint32_t serial = 0;
  if (!ethGetQueryToken(requestLine, "t", typeText, sizeof(typeText)) || !ethGetQueryU32(requestLine, "s", &serial)) return false;

  const bool oldStyle = (strncmp(typeText, "STP", 3) == 0 &&
                         typeText[3] >= '0' && typeText[3] <= '9' &&
                         typeText[4] >= '0' && typeText[4] <= '9' &&
                         typeText[5] >= '0' && typeText[5] <= '9' &&
                         typeText[6] >= '0' && typeText[6] <= '9' &&
                         typeText[7] == '\0');
  if (!headTypeTextValid(typeText) && !oldStyle) return false;

  char norm[HEAD_TYPE_TEXT_LEN + 1U];
  strncpy(norm, typeText, sizeof(norm) - 1U);
  norm[sizeof(norm) - 1U] = '\0';
  headTypeTextNormalize(norm, sizeof(norm));
  if (!headTypeTextValid(norm) || serial > HEAD_SERIAL_MAX) return false;

  if (typeOut && typeOutSize)
  {
    strncpy(typeOut, norm, typeOutSize - 1U);
    typeOut[typeOutSize - 1U] = '\0';
  }
  if (serialOut) *serialOut = serial;
  return true;
}

static void FLASHMEM ethHandleSetupHeadCurrent(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;
  char typeText[HEAD_TYPE_TEXT_LEN + 1U];
  uint32_t serial = HEAD_SERIAL_DEFAULT;
  if (!ethGetHeadTypeSerial(requestLine, typeText, sizeof(typeText), &serial))
  {
    ethSendText(client, false, "Kopfdaten ungültig / Invalid head data");
    return;
  }
  const int8_t profile = headTypeProfileFromText(typeText);
  if (profile >= 0) applySensorHeadProfile((uint8_t)profile);
  else headTypeTextSet(typeText);
  R.head_serial = serial;
  tpMainConfigSave();
  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupHeadLoad(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;
  char typeText[HEAD_TYPE_TEXT_LEN + 1U];
  uint32_t serial = HEAD_SERIAL_DEFAULT;
  if (!ethGetHeadTypeSerial(requestLine, typeText, sizeof(typeText), &serial))
  {
    ethSendText(client, false, "Kopfdaten ungültig / Invalid head data");
    return;
  }
  char msg[96];
  bool ok = headCalLoadLatest(typeText, serial, msg, sizeof(msg));
  ethSendText(client, ok, msg);
}

static void FLASHMEM ethHandleSetupHeadSave(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;
  if (!ethQueryPasswordMatchesDevice(requestLine))
  {
    ethSendText(client, false, "Passwort falsch / Wrong password");
    return;
  }
  char typeText[HEAD_TYPE_TEXT_LEN + 1U];
  uint32_t serial = HEAD_SERIAL_DEFAULT;
  if (!ethGetHeadTypeSerial(requestLine, typeText, sizeof(typeText), &serial))
  {
    ethSendText(client, false, "Kopfdaten ungültig / Invalid head data");
    return;
  }
  char msg[96];
  bool ok = headCalSave(typeText, serial, msg, sizeof(msg));
  ethSendText(client, ok, msg);
}

static void FLASHMEM ethFormatYmd(uint32_t yyyymmdd, char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0U) return;
  if (yyyymmdd == 0UL)
  {
    snprintf(out, outSize, "--.--.----");
    return;
  }
  const uint16_t y = (uint16_t)(yyyymmdd / 10000UL);
  const uint8_t m = (uint8_t)((yyyymmdd / 100UL) % 100UL);
  const uint8_t d = (uint8_t)(yyyymmdd % 100UL);
  snprintf(out, outSize, "%02u.%02u.%04u", (unsigned)d, (unsigned)m, (unsigned)y);
}

static void FLASHMEM ethHandleSetupDeviceStatus(EthBoundedWriter& client)
{
  if (!ethSetupAccessAllowed(client, false)) return;

  char settingsDate[16];
  char factoryDate[16];
  const bool settingsOk = deviceSettingsBackupValid();
  const bool factoryOk = deviceFactoryCalBackupValid();
  ethFormatYmd(settingsOk ? deviceSettingsBackupDate() : 0UL, settingsDate, sizeof(settingsDate));
  ethFormatYmd(factoryOk ? deviceFactoryCalBackupDate() : 0UL, factoryDate, sizeof(factoryDate));

  int n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
                   "G%s\n%s %s\n%s %s\n%s",
                   deviceSerialGet(),
                   settingsOk ? "OK" : "--", settingsDate,
                   factoryOk ? "OK" : "--", factoryDate,
                   "Kopfdaten bleiben separat");
  if (n < 0 || (size_t)n >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Speicherstatus zu gross / Storage status too large");
    return;
  }
  ethSendBufferedBody(client, "text/plain; charset=utf-8", true, ethJsonBuf, (size_t)n);
}

static void FLASHMEM ethHandleSetupDeviceStorage(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  char action[18];
  if (!ethGetQueryToken(requestLine, "a", action, sizeof(action)))
  {
    ethSendText(client, false, "Aktion fehlt / Missing action");
    return;
  }

  char newDeviceSn[DEVICE_SERIAL_DIGITS + 1U];
  newDeviceSn[0] = '\0';
  if (strcmp(action, "factory_save") == 0)
  {
    if (!ethQueryPasswordMatchesDevice(requestLine))
    {
      ethSendText(client, false, "Passwort falsch / Wrong password");
      return;
    }
    if (!ethGetQueryToken(requestLine, "sn", newDeviceSn, sizeof(newDeviceSn)) || strlen(newDeviceSn) != DEVICE_SERIAL_DIGITS)
    {
      ethSendText(client, false, "Geräte-SN ungültig / Invalid device SN");
      return;
    }
    for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
    {
      if (newDeviceSn[i] < '0' || newDeviceSn[i] > '9')
      {
        ethSendText(client, false, "Geräte-SN ungültig / Invalid device SN");
        return;
      }
    }
  }

  bool ok = false;
  const char* okMsg = "OK";
  const char* errMsg = "Fehler / Error";

  if (strcmp(action, "settings_load") == 0)
  {
    ok = deviceSettingsBackupLoad();
    okMsg = "Einstellungen geladen / Settings loaded";
    errMsg = "Kein gültiges Einstellungsbackup / No valid settings backup";
  }
  else if (strcmp(action, "factory_load") == 0)
  {
    ok = deviceFactoryCalBackupLoad();
    okMsg = "Werksjustierung geladen / Factory calibration loaded";
    errMsg = "Keine gültige Werksjustierung / No valid factory calibration";
  }
  else if (strcmp(action, "settings_save") == 0)
  {
    ok = deviceSettingsBackupSave();
    okMsg = "Einstellungen gespeichert / Settings saved";
    errMsg = "Einstellungen speichern fehlgeschlagen / Saving settings failed";
  }
  else if (strcmp(action, "factory_save") == 0)
  {
    ok = deviceFactoryCalBackupSaveForSerial(newDeviceSn);
    okMsg = "Werksjustierung gespeichert / Factory calibration saved";
    errMsg = "Werksjustierung ungültig / Invalid factory calibration";
  }
  else
  {
    ethSendText(client, false, "Aktion ungültig / Invalid action");
    return;
  }

  ethSendText(client, ok, ok ? okMsg : errMsg);
}

static void FLASHMEM ethHandleSetupControl(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  if (!ethQueryPasswordMatchesDevice(requestLine))
  {
    ethSendText(client, false, "Passwort falsch / Wrong password");
    return;
  }

  uint32_t peltierMa = 0;
  int kp = 0, ki = 0, kd = 0, period = 0, optik = 0, freeTemp = 0, autoIdx = 0, filterMode = 0;
  int sfocalMode = (int)adc1SfocalModeGet();
  bool ok = ethGetQueryU32(requestLine, "peltier", &peltierMa) &&
            ethGetQueryInt(requestLine, "kp", &kp) &&
            ethGetQueryInt(requestLine, "ki", &ki) &&
            ethGetQueryInt(requestLine, "kd", &kd) &&
            ethGetQueryInt(requestLine, "period", &period) &&
            ethGetQueryInt(requestLine, "optik", &optik) &&
            ethGetQueryInt(requestLine, "free", &freeTemp) &&
            ethGetQueryInt(requestLine, "auto", &autoIdx) &&
            ethGetQueryInt(requestLine, "filter", &filterMode);
  // Abwaertskompatibel: alte gecachte Web-Setup-Seiten ohne sfocal-Parameter
  // behalten den aktuellen ADC1-SFOCAL-Modus bei.
  ethGetQueryInt(requestLine, "sfocal", &sfocalMode);

  if (!ok || peltierMa < (uint32_t)PELTIER_CURRENT_LIMIT_MIN_MA ||
      peltierMa > (uint32_t)PELTIER_CURRENT_LIMIT_MAX_MA ||
      kp < 0 || kp > 1000 || ki < 0 || ki > 500 ||
      kd < 0 || kd > 500 || !ethSetupControlPeriodValid(period) ||
      optik < 800 || optik > 990 || freeTemp < 40 || freeTemp > 76 ||
      autoIdx < 0 || autoIdx > 5 || filterMode < 0 || filterMode > (int)ADC_MEAS_FILTER_MAX ||
      sfocalMode < 0 || sfocalMode > (int)ADC1_SFOCAL_MAX)
  {
    ethSendText(client, false, "Ungültige Regelparameter / Invalid control parameters");
    return;
  }

  R.pid_kp = (uint16_t)kp;
  R.pid_ki = (float)ki / 10.0f;
  R.pid_kd = (float)kd / 10.0f;
  R.regler_intervall_ms = (uint16_t)period;
  R.optik_sollwert = (uint16_t)optik;
  R.freiheiz_ziel_temp[0] = (uint8_t)freeTemp;
  R.optik_autocal_interval_index = (uint8_t)autoIdx;
  peltierCurrentLimitSetMa((uint16_t)peltierMa);
  adcFilterModeSet((uint8_t)filterMode);
  adc1SfocalModeSet((uint8_t)sfocalMode);
  tpMainConfigSave();

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupFanToggle(EthBoundedWriter& client)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  sensorFanToggleEnabled();
  ethSendText(client, true, sensorFanStatusText());
}

static void FLASHMEM ethHandleSetupFan(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int value = 0;
  if (!ethGetQueryInt(requestLine, "v", &value) || value < 50 || value > 100)
  {
    ethSendText(client, false, "Ungültiger Lüfterwert / Invalid fan value");
    return;
  }

  R.fan_percent = (uint8_t)value;
  tpMainConfigSave();
  fanApplyNormalSpeed();
  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupAlarm(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int mode = 0, tauLow = 0, tauHigh = 0, rhLow = 0, rhHigh = 0, buzzer = 0;
  bool ok = ethGetQueryInt(requestLine, "mode", &mode) &&
            ethGetQueryInt(requestLine, "tl", &tauLow) &&
            ethGetQueryInt(requestLine, "th", &tauHigh) &&
            ethGetQueryInt(requestLine, "rl", &rhLow) &&
            ethGetQueryInt(requestLine, "rh", &rhHigh) &&
            ethGetQueryInt(requestLine, "b", &buzzer);

  if (!ok || mode < 0 || mode > 2 || tauLow < -800 || tauLow > 800 ||
      tauHigh < -800 || tauHigh > 800 || rhLow < 0 || rhLow > 1000 ||
      rhHigh < 0 || rhHigh > 1000 || buzzer < 0 || buzzer > 1 ||
      tauLow > tauHigh || rhLow > rhHigh)
  {
    ethSendText(client, false, "Ungültige Alarmwerte / Invalid alarm values");
    return;
  }

  alarmSetMode((uint8_t)mode);
  alarmSetTauLow10((int16_t)tauLow);
  alarmSetTauHigh10((int16_t)tauHigh);
  alarmSetRhLow10((uint16_t)rhLow);
  alarmSetRhHigh10((uint16_t)rhHigh);
  alarmSetBuzzerEnabled(buzzer != 0);
  alarmConfigSave();
  alarmResetAcknowledge();
  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupR0(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int32_t mirror = 0, ambient = 0;
  if (!ethGetQueryI32(requestLine, "m", &mirror) ||
      !ethGetQueryI32(requestLine, "a", &ambient) ||
      mirror < 800000L || mirror > 1200000L ||
      ambient < 800000L || ambient > 1200000L)
  {
    ethSendText(client, false, "Ungültige R0-Werte / Invalid R0 values");
    return;
  }

  if (!pt100R0SetScaled(0, mirror) || !pt100R0SetScaled(1, ambient))
  {
    ethSendText(client, false, "R0 konnte nicht gespeichert werden / R0 save failed");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetup2Point(EthBoundedWriter& client,
                                           const char* requestLine,
                                           uint8_t sensor)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int32_t s1 = 0, i1 = 0, s2 = 0, i2 = 0;
  if (!ethGetQueryI32(requestLine, "s1", &s1) ||
      !ethGetQueryI32(requestLine, "i1", &i1) ||
      !ethGetQueryI32(requestLine, "s2", &s2) ||
      !ethGetQueryI32(requestLine, "i2", &i2))
  {
    ethSendText(client, false, "Ungültige 2-Punkt-Werte / Invalid 2-point values");
    return;
  }

  if (!pt100Cal2SetScaled(sensor, s1, i1, s2, i2))
  {
    ethSendText(client, false, "2-Punkt-Werte unplausibel oder Punkte zu dicht / Invalid 2-point spacing");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupRef(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  uint32_t date = 0;
  int32_t ref100 = 0, ref120 = 0, a100 = 0, a120 = 0, b100 = 0, b120 = 0;
  bool ok = ethGetQueryU32(requestLine, "date", &date) &&
            ethGetQueryI32(requestLine, "r100", &ref100) &&
            ethGetQueryI32(requestLine, "r120", &ref120) &&
            ethGetQueryI32(requestLine, "a100", &a100) &&
            ethGetQueryI32(requestLine, "a120", &a120) &&
            ethGetQueryI32(requestLine, "b100", &b100) &&
            ethGetQueryI32(requestLine, "b120", &b120);

  if (!ok || !refCalSetAllScaled(date, ref100, ref120,
                                  a100, a120, b100, b120))
  {
    ethSendText(client, false, "Referenz-/Kanalwerte unplausibel / Invalid reference or channel values");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupOffset(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int32_t value = 0;
  if (!ethGetQueryI32(requestLine, "v", &value) ||
      !taupunktOffsetSetScaled(value))
  {
    ethSendText(client, false, "Ungültiger Taupunkt-Offset / Invalid dew-point offset");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupGeneral(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int interval = 0, filter = 0, diagnostic = 0;
  if (!ethGetQueryInt(requestLine, "i", &interval) ||
      !ethGetQueryInt(requestLine, "f", &filter) ||
      !ethGetQueryInt(requestLine, "d", &diagnostic) ||
      diagnostic < 0 || diagnostic > 1 ||
      !interfaceWebSetGeneralOutput((uint8_t)interval,
                                    (uint8_t)filter,
                                    diagnostic != 0))
  {
    ethSendText(client, false, "Ungültige Ausgabeeinstellung / Invalid output setting");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupRs232(EthBoundedWriter& client,
                                          const char* requestLine,
                                          uint8_t port)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int mode = 0, baud = 0, output = 0;
  if (!ethGetQueryInt(requestLine, "m", &mode) ||
      !ethGetQueryInt(requestLine, "b", &baud) ||
      !ethGetQueryInt(requestLine, "o", &output) ||
      !interfaceWebSetRs232(port, (uint8_t)mode, (uint8_t)baud, (uint8_t)output))
  {
    ethSendText(client, false, "Ungültige RS232-Einstellung / Invalid RS232 setting");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupUsb(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int value = 0;
  if (!ethGetQueryInt(requestLine, "v", &value) ||
      !interfaceWebSetUsb((uint8_t)value))
  {
    ethSendText(client, false, "Ungültige USB-Einstellung / Invalid USB setting");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupSd(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int logging = 0, interval = 0, filter = 0, writeInterval = 0;
  int diagnostic = 0, header = 0;
  bool ok = ethGetQueryInt(requestLine, "log", &logging) &&
            ethGetQueryInt(requestLine, "i", &interval) &&
            ethGetQueryInt(requestLine, "f", &filter) &&
            ethGetQueryInt(requestLine, "w", &writeInterval) &&
            ethGetQueryInt(requestLine, "d", &diagnostic) &&
            ethGetQueryInt(requestLine, "h", &header);

  if (!ok || logging < 0 || logging > 1 || diagnostic < 0 || diagnostic > 1 ||
      header < 0 || header > 1 ||
      !interfaceWebSetSd((uint8_t)interval,
                         (uint8_t)filter,
                         (uint8_t)writeInterval,
                         logging != 0,
                         diagnostic != 0,
                         header != 0))
  {
    ethSendText(client, false, "Ungültige SD-Einstellung oder SD nicht bereit / Invalid SD setting or card not ready");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupFlow(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int value = 0;
  if (!ethGetQueryInt(requestLine, "v", &value) ||
      !interfaceWebSetFlowDisplay((uint8_t)value))
  {
    ethSendText(client, false, "Ungültige Zusatzanzeige / Invalid additional display");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupAlmemo(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int trace = -1;
  if (ethGetQueryInt(requestLine, "trace", &trace))
  {
    if (trace < 0 || trace > 1 || !interfaceWebSetAlmemoTrace(trace != 0))
    {
      ethSendText(client, false, "ALMEMO-Seriellog konnte nicht geschaltet werden / ALMEMO serial trace could not be switched");
      return;
    }
    ethSendText(client, true, "OK");
    return;
  }

  int interval = 0;
  if (ethGetQueryInt(requestLine, "i", &interval))
  {
    if (interval < 0 || interval > 2 ||
        !interfaceWebSetAlmemoInterval((uint8_t)interval))
    {
      ethSendText(client, false, "Ungültiges ALMEMO-Intervall / Invalid ALMEMO interval");
      return;
    }
    ethSendText(client, true, "OK");
    return;
  }

  int idx = 0, enabled = 0, address = 0, channel = 0, role = 0;
  if (!ethGetQueryInt(requestLine, "idx", &idx) ||
      !ethGetQueryInt(requestLine, "en", &enabled) ||
      !ethGetQueryInt(requestLine, "a", &address) ||
      !ethGetQueryInt(requestLine, "c", &channel) ||
      !ethGetQueryInt(requestLine, "r", &role) ||
      idx < 0 || idx > 1 || address < 0 || address > 99 ||
      channel < 0 || channel > 99 || role < 1 || role > 4 ||
      !interfaceWebSetAlmemoChannel((uint8_t)idx, (uint8_t)(enabled != 0),
                                    (uint8_t)address, (uint8_t)channel, (uint8_t)role))
  {
    ethSendText(client, false, "Ungültige ALMEMO-Einstellung / Invalid ALMEMO setting");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupWinControl(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int out = 0, baud = 0, address = 0, cycle = 0;
  uint32_t port = 0;

  // ethGetQueryInt() begrenzt Zahlen absichtlich auf 3000, damit normale
  // Web-Setup-Parameter klein bleiben. Der WinControl-TCP-Port darf aber
  // z.B. 10001 sein und muss deshalb ueber den 32-Bit-Parser gelesen werden.
  if (!ethGetQueryInt(requestLine, "out", &out) ||
      !ethGetQueryInt(requestLine, "b", &baud) ||
      !ethGetQueryInt(requestLine, "a", &address) ||
      !ethGetQueryU32(requestLine, "p", &port) ||
      !ethGetQueryInt(requestLine, "c", &cycle) ||
      out < 0 || out > 3 || baud < 0 || baud >= 5 ||
      address < 0 || address > 99 || port < 1U || port > 65535U ||
      cycle < 0 || cycle > 3 ||
      !interfaceWebSetWinControl((uint8_t)out, (uint8_t)baud,
                                 (uint8_t)address, (uint16_t)port,
                                 (uint8_t)cycle))
  {
    ethSendText(client, false, "Ungültige WinControl-Einstellung / Invalid WinControl setting");
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupDisplay(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int layout = 0;
  if (!ethGetQueryInt(requestLine, "layout", &layout) ||
      layout < 0 || layout > (int)MAIN_SCREEN_LAYOUT_MAX)
  {
    ethSendText(client, false, "Ungültige Anzeige-Einstellung / Invalid display setting");
    return;
  }

  mainScreenLayoutSet((uint8_t)layout);
  flag.mode_change = true;
  flag.menu_lcd_upd = false;
  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupLanguage(EthBoundedWriter& client, const char* requestLine)
{
  if (!ethSetupAccessAllowed(client, true)) return;

  int value = 0;
  if (!ethGetQueryInt(requestLine, "v", &value) || value < 0 || value >= LANG_COUNT)
  {
    ethSendText(client, false, "Ungültige Sprache / Invalid language");
    return;
  }

  ui_language = (uint8_t)value;
  uiLanguageSave();
  flag.mode_change = true;
  flag.menu_lcd_upd = false;
  ethSendText(client, true, "OK");
}

static void FLASHMEM ethHandleSetupClose(EthBoundedWriter& client, const char* requestLine)
{
  ethWebSetupCloseSession();

  int redirect = 0;
  if (ethGetQueryInt(requestLine, "redirect", &redirect) && redirect != 0)
  {
    client.print(F("HTTP/1.1 303 See Other\r\n"));
    client.print(F("Location: /\r\n"));
    client.print(F("Cache-Control: no-store, no-cache, must-revalidate\r\n"));
    client.print(F("Connection: close\r\n"));
    client.print(F("Content-Length: 0\r\n\r\n"));
    return;
  }

  ethSendText(client, true, "OK");
}

static void FLASHMEM ethSendDownloadPage(EthBoundedWriter& client,
                                         EthernetClient& rawClient,
                                         const char* requestHeaders,
                                         bool* keepOpen)
{
  ethFileIndexRequestRefresh();
  const EthPageSegment segments[] = {
    { ethDownloadPage, sizeof(ethDownloadPage) - 1U }
  };
  ethBeginPageTransfer(rawClient, client, requestHeaders, ETH_ETAG_DOWNLOAD,
                       ETH_REQ_DOWNLOAD_PAGE, segments,
                       (uint8_t)(sizeof(segments) / sizeof(segments[0])),
                       sizeof(ethDownloadPage) - 1U, keepOpen);
}

static void FLASHMEM ethSendMainPage(EthBoundedWriter& client,
                                     EthernetClient& rawClient,
                                     const char* requestHeaders,
                                     bool* keepOpen)
{
  const char* statusInit = T(TXT_STATUS_INIT);
  const char* chartHumidity = T(TXT_MAIN_CHART_HUMIDITY);
  const char* chartDewPoint = T(TXT_MAIN_CHART_DEW_POINT);
  const char* chartTPoint = T(TXT_MAIN_CHART_T_POINT);
  const char* chartNow = T(TXT_MAIN_CHART_NOW);

  const EthPageSegment segments[] = {
    { ethMainPagePart0, sizeof(ethMainPagePart0) - 1U },
    { statusInit, strlen(statusInit) },
    { ethMainPagePart1, sizeof(ethMainPagePart1) - 1U },
    { chartHumidity, strlen(chartHumidity) },
    { ethMainPagePart2, sizeof(ethMainPagePart2) - 1U },
    { chartDewPoint, strlen(chartDewPoint) },
    { ethMainPagePart3, sizeof(ethMainPagePart3) - 1U },
    { chartTPoint, strlen(chartTPoint) },
    { ethMainPagePart4, sizeof(ethMainPagePart4) - 1U },
    { chartNow, strlen(chartNow) },
    { ethMainPagePart5, sizeof(ethMainPagePart5) - 1U }
  };

  size_t contentLength = 0U;
  for (uint8_t i = 0U; i < (uint8_t)(sizeof(segments) / sizeof(segments[0])); i++)
  {
    contentLength += segments[i].length;
  }

  const char* etag = (ui_language == LANG_EN) ? ETH_ETAG_MAIN_EN : ETH_ETAG_MAIN_DE;
  ethBeginPageTransfer(rawClient, client, requestHeaders, etag,
                       ETH_REQ_ROOT, segments,
                       (uint8_t)(sizeof(segments) / sizeof(segments[0])),
                       contentLength, keepOpen);
}

enum EthRequestReadResult : uint8_t
{
  ETH_REQUEST_WAITING = 0,
  ETH_REQUEST_COMPLETE = 1,
  ETH_REQUEST_ABORT = 2
};

static bool FLASHMEM ethAnyRequestSlotActive(void)
{
  for (uint8_t i = 0U; i < ETH_REQUEST_SLOT_COUNT; i++)
  {
    if (ethRequestSlots[i].active) return true;
  }
  return false;
}

static int8_t FLASHMEM ethFindFreeRequestSlot(void)
{
  for (uint8_t offset = 0U; offset < ETH_REQUEST_SLOT_COUNT; offset++)
  {
    uint8_t index = (uint8_t)((ethRequestRoundRobin + offset) % ETH_REQUEST_SLOT_COUNT);
    if (!ethRequestSlots[index].active) return (int8_t)index;
  }
  return -1;
}

static void FLASHMEM ethResetRequestSlot(uint8_t index, bool stopClient)
{
  if (index >= ETH_REQUEST_SLOT_COUNT) return;

  EthRequestSlot& slot = ethRequestSlots[index];
  if (stopClient && slot.active && slot.client)
  {
    ethDiagStage = ETH_DIAG_STOP;
    slot.client.stop();
  }

  slot.client = EthernetClient();
  slot.active = false;
  slot.sawAnyByte = false;
  slot.firstLineComplete = false;
  slot.headerLineHasData = false;
  slot.headersComplete = false;
  slot.responseDraining = false;
  slot.requestLength = 0U;
  slot.headerCacheLength = 0U;
  slot.headerBytes = 0U;
  slot.responseDrainTarget = 0U;
  slot.startedUs = 0UL;
  slot.startedMs = 0UL;
  slot.lastProgressMs = 0UL;
  slot.responseDrainStartedMs = 0UL;
  ethRequestLines[index][0] = '\0';
  ethRequestHeaderCache[index][0] = '\0';
}

static void ETH_FLASHMEM_NOINLINE ethAbortAllPendingRequests(void)
{
  for (uint8_t i = 0U; i < ETH_REQUEST_SLOT_COUNT; i++)
  {
    if (ethRequestSlots[i].active) ethResetRequestSlot(i, true);
  }

  ethDiagRequestStartMs = 0UL;
  ethDiagRequest = ETH_REQ_NONE;
  ethDiagStage = ETH_DIAG_IDLE;
}

static void ETH_FLASHMEM_NOINLINE ethBeginPendingRequest(uint8_t index, EthernetClient& client)
{
  if (index >= ETH_REQUEST_SLOT_COUNT) return;

  EthRequestSlot& slot = ethRequestSlots[index];
  slot.client = client;
  slot.active = true;
  slot.sawAnyByte = false;
  slot.firstLineComplete = false;
  slot.headerLineHasData = false;
  slot.headersComplete = false;
  slot.responseDraining = false;
  slot.requestLength = 0U;
  slot.headerCacheLength = 0U;
  slot.headerBytes = 0U;
  int sendCapacity = slot.client.availableForWrite();
  slot.responseDrainTarget = (sendCapacity > 0)
                           ? (uint16_t)sendCapacity
                           : ETH_HTTP_FALLBACK_SEND_BUFFER_BYTES;
  slot.startedUs = micros();
  slot.startedMs = millis();
  slot.lastProgressMs = slot.startedMs;
  slot.responseDrainStartedMs = 0UL;
  ethRequestLines[index][0] = '\0';
  ethRequestHeaderCache[index][0] = '\0';
}

static uint8_t ETH_FLASHMEM_NOINLINE ethReadRequestSlotStep(uint8_t index)
{
  if (index >= ETH_REQUEST_SLOT_COUNT) return ETH_REQUEST_ABORT;

  EthRequestSlot& slot = ethRequestSlots[index];
  char* requestLine = ethRequestLines[index];
  if (!slot.active) return ETH_REQUEST_ABORT;
  if (slot.headersComplete) return ETH_REQUEST_COMPLETE;

  ethDiagStage = ETH_DIAG_READ_REQUEST;
  ethDiagRequestStartMs = slot.startedMs;
  ethDiagRequest = slot.firstLineComplete ? ethDiagRequestCode(requestLine) : (uint8_t)ETH_REQ_NONE;

  uint8_t bytesThisLoop = 0U;
  while (bytesThisLoop < ETH_REQUEST_READ_BYTES_PER_LOOP &&
         slot.client.available() > 0)
  {
    int raw = slot.client.read();
    if (raw < 0) break;

    bytesThisLoop++;
    slot.sawAnyByte = true;
    slot.lastProgressMs = millis();
    if (slot.headerBytes >= ETH_REQUEST_MAX_HEADER_BYTES) return ETH_REQUEST_ABORT;
    slot.headerBytes++;

    char c = (char)raw;
    if (!slot.firstLineComplete)
    {
      if (c == '\r') continue;

      if (c == '\n')
      {
        if (slot.requestLength == 0U) return ETH_REQUEST_ABORT;
        requestLine[slot.requestLength] = '\0';
        slot.firstLineComplete = true;
        slot.headerLineHasData = false;
        ethDiagRequest = ethDiagRequestCode(requestLine);
        continue;
      }

      if (slot.requestLength >= (ETH_REQUEST_LINE_BYTES - 1U))
      {
        return ETH_REQUEST_ABORT;
      }

      requestLine[slot.requestLength++] = c;
      continue;
    }

    // Einen kleinen Anfang der Header fuer If-None-Match/ETag sichern. Auch
    // wenn der Cache voll ist, werden die restlichen Header weiterhin bis zur
    // Leerzeile eingelesen und verworfen.
    if (slot.headerCacheLength < (ETH_REQUEST_HEADER_CACHE_BYTES - 1U))
    {
      ethRequestHeaderCache[index][slot.headerCacheLength++] = c;
      ethRequestHeaderCache[index][slot.headerCacheLength] = '\0';
    }

    // Nach der gespeicherten Requestzeile werden die restlichen HTTP-Header
    // bis zur ersten leeren Zeile eingelesen. Erst danach darf die Antwort
    // beginnen; so bleibt der TCP-Abschluss auch bei WLAN-Paketierung sauber.
    if (c == '\n')
    {
      if (!slot.headerLineHasData)
      {
        slot.headersComplete = true;
        return ETH_REQUEST_COMPLETE;
      }
      slot.headerLineHasData = false;
    }
    else if (c != '\r')
    {
      slot.headerLineHasData = true;
    }
  }

  // Zuerst alle bereits gepufferten Bytes auswerten. Erst danach werden
  // Verbindungsende und die drei nicht blockierenden Zeitgrenzen geprueft.
  if (!slot.client.connected() && slot.client.available() <= 0)
  {
    return ETH_REQUEST_ABORT;
  }

  const uint32_t nowMs = millis();
  const uint32_t totalMs = (uint32_t)(nowMs - slot.startedMs);

  // Die absolute Grenze gilt nur fuer das Einsammeln des Requests. Sobald die
  // Header vollstaendig sind, darf der fertige Request in der kleinen Queue
  // auf seine Antwort warten, ohne durch einen anderen langsamen Response zu
  // verfallen.
  if (totalMs >= ETH_REQUEST_ABSOLUTE_TIMEOUT_MS)
  {
    return ETH_REQUEST_ABORT;
  }

  if (!slot.sawAnyByte)
  {
    if (totalMs >= ETH_REQUEST_FIRST_BYTE_TIMEOUT_MS)
    {
      return ETH_REQUEST_ABORT;
    }
  }
  else if ((uint32_t)(nowMs - slot.lastProgressMs) >= ETH_REQUEST_IDLE_TIMEOUT_MS)
  {
    return ETH_REQUEST_ABORT;
  }

  return ETH_REQUEST_WAITING;
}

static bool FLASHMEM ethPathEquals(const char* requestLine, const char* path)
{
  if (requestLine == nullptr || path == nullptr) return false;
  if (strncmp(requestLine, "GET ", 4) != 0) return false;

  const char* p = requestLine + 4;
  size_t len = strlen(path);
  if (strncmp(p, path, len) != 0) return false;

  char next = p[len];
  return next == ' ' || next == '?' || next == '\0';
}

static bool ETH_FLASHMEM_NOINLINE ethHandleReadyClient(EthernetClient& rawClient,
                                              const char* requestLine,
                                              const char* requestHeaders,
                                              bool* keepOpen)
{
  if (keepOpen != nullptr) *keepOpen = false;

  ethDiagRequest = ethDiagRequestCode(requestLine);
  ethDiagStage = ETH_DIAG_HANDLE_REQUEST;
  EthBoundedWriter client(rawClient, millis());

  if (ethPathEquals(requestLine, "/"))
  {
    ethSendMainPage(client, rawClient, requestHeaders, keepOpen);
  }
  else if (ethPathEquals(requestLine, "/setup"))
  {
    ethSendSetupPage(client, rawClient, requestHeaders, keepOpen);
  }
  else if (ethPathEquals(requestLine, "/setup.json"))
  {
    ethSendSetupJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-live.json"))
  {
    ethSendSetupLiveJson(client);
  }
  else if (ethPathEquals(requestLine, "/setup-status.json"))
  {
    ethSendSetupStatusJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-control-unlock"))
  {
    ethHandleSetupControlUnlock(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-head-status"))
  {
    ethHandleSetupHeadStatus(client);
  }
  else if (ethPathEquals(requestLine, "/setup-head-list.json"))
  {
    ethHandleSetupHeadList(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-head-current"))
  {
    ethHandleSetupHeadCurrent(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-head-load"))
  {
    ethHandleSetupHeadLoad(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-head-save"))
  {
    ethHandleSetupHeadSave(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-device-status"))
  {
    ethHandleSetupDeviceStatus(client);
  }
  else if (ethPathEquals(requestLine, "/setup-device-storage"))
  {
    ethHandleSetupDeviceStorage(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-control"))
  {
    ethHandleSetupControl(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-fan-toggle"))
  {
    ethHandleSetupFanToggle(client);
  }
  else if (ethPathEquals(requestLine, "/setup-fan"))
  {
    ethHandleSetupFan(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-alarm"))
  {
    ethHandleSetupAlarm(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-r0"))
  {
    ethHandleSetupR0(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-2pm"))
  {
    ethHandleSetup2Point(client, requestLine, 0);
  }
  else if (ethPathEquals(requestLine, "/setup-2pa"))
  {
    ethHandleSetup2Point(client, requestLine, 1);
  }
  else if (ethPathEquals(requestLine, "/setup-ref"))
  {
    ethHandleSetupRef(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-offset"))
  {
    ethHandleSetupOffset(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-general"))
  {
    ethHandleSetupGeneral(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-rs1"))
  {
    ethHandleSetupRs232(client, requestLine, 0);
  }
  else if (ethPathEquals(requestLine, "/setup-rs2"))
  {
    ethHandleSetupRs232(client, requestLine, 1);
  }
  else if (ethPathEquals(requestLine, "/setup-usb"))
  {
    ethHandleSetupUsb(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-sd"))
  {
    ethHandleSetupSd(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-flow"))
  {
    ethHandleSetupFlow(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-almemo"))
  {
    ethHandleSetupAlmemo(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-wincontrol"))
  {
    ethHandleSetupWinControl(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-language"))
  {
    ethHandleSetupLanguage(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-display"))
  {
    ethHandleSetupDisplay(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/setup-close"))
  {
    ethHandleSetupClose(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/data.json"))
  {
    ethSendJson(client);
  }
  else if (ethPathEquals(requestLine, "/chart.json"))
  {
    ethSendChartJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/download"))
  {
    ethSendDownloadPage(client, rawClient, requestHeaders, keepOpen);
  }
  else if (ethPathEquals(requestLine, "/files.json"))
  {
    ethSendFilesJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/download-file"))
  {
    bool started = ethStartDownload(client, rawClient, requestLine, requestHeaders);
    if (started && keepOpen != nullptr) *keepOpen = true;
  }
  else if (ethPathEquals(requestLine, "/set-time"))
  {
    ethHandleSetTime(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/favicon.ico"))
  {
    ethSendNoContent(client);
  }
  else
  {
    ethSendNotFound(client);
  }

  return client.ok();
}

static void FLASHMEM ethRecordRequestDuration(uint8_t index,
                                               uint8_t requestCode)
{
  if (index >= ETH_REQUEST_SLOT_COUNT) return;
  const uint32_t durationUs =
    (uint32_t)(micros() - ethRequestSlots[index].startedUs);
  ethDiagLastRequestUs = durationUs;
  if (durationUs > ethDiagMaxRequestUs)
  {
    ethDiagMaxRequestUs = durationUs;
    ethDiagMaxRequestCode = requestCode;
  }
}

static bool ETH_FLASHMEM_NOINLINE ethServiceResponseDrainSlot(uint8_t index)
{
  if (index >= ETH_REQUEST_SLOT_COUNT) return true;

  EthRequestSlot& slot = ethRequestSlots[index];
  if (!slot.active || !slot.responseDraining) return true;

  // Der Browser kennt durch Content-Length bereits das exakte Antwortende.
  // Der Socket wird trotzdem erst geschlossen, wenn NativeEthernet den vor
  // der Antwort freien Sendepuffer wieder meldet. Es wird dabei nie gewartet.
  if (!slot.client || (!slot.client.connected() && slot.client.available() <= 0))
  {
    ethResetRequestSlot(index, true);
    return true;
  }

  int freeBytes = slot.client.availableForWrite();
  if (freeBytes >= 0 &&
      (uint16_t)freeBytes >= slot.responseDrainTarget)
  {
    ethResetRequestSlot(index, true);
    return true;
  }

  if ((uint32_t)(millis() - slot.responseDrainStartedMs) >=
      ETH_HTTP_DRAIN_TIMEOUT_MS)
  {
    ethResetRequestSlot(index, true);
    return true;
  }

  return false;
}

static void ETH_FLASHMEM_NOINLINE ethServiceReadyRequestSlot(uint8_t index)
{
  if (index >= ETH_REQUEST_SLOT_COUNT) return;

  EthRequestSlot& slot = ethRequestSlots[index];
  if (!slot.active || !slot.headersComplete || slot.responseDraining) return;

  char* requestLine = ethRequestLines[index];
  uint8_t requestCode = ethDiagRequestCode(requestLine);
  ethDiagRequest = requestCode;
  ethDiagRequestStartMs = slot.startedMs;
  ethDiagStage = ETH_DIAG_HANDLE_REQUEST;

  // Vor dem ersten Antwortbyte ist der Sendepuffer normalerweise vollstaendig
  // frei. Genau diesen Wert verwenden wir als individuelles Drain-Ziel.
  int sendCapacity = slot.client.availableForWrite();
  if (sendCapacity > 0)
  {
    slot.responseDrainTarget = (uint16_t)sendCapacity;
  }
  else if (slot.responseDrainTarget == 0U)
  {
    slot.responseDrainTarget = ETH_HTTP_FALLBACK_SEND_BUFFER_BYTES;
  }

  bool keepOpen = false;
  bool responseComplete = ethHandleReadyClient(slot.client, requestLine,
                                               ethRequestHeaderCache[index],
                                               &keepOpen);
  ethRecordRequestDuration(index, requestCode);

  // Dateidownload und grosse HTML-Seiten uebernehmen eine eigene Kopie des
  // NativeEthernet-Sockets. Den Request-Slot deshalb ohne stop() freigeben.
  // Nur der Dateidownload bleibt weiterhin bewusst exklusiv; bei einer
  // HTML-Seite duerfen andere Slots bereits Header einsammeln.
  if (keepOpen)
  {
    ethResetRequestSlot(index, false);
    if (ethDownloadActive) ethAbortAllPendingRequests();
  }
  else if (!responseComplete)
  {
    // Eine innerhalb des Antwortbudgets nicht vollstaendig in den TCP-Puffer
    // uebernommene Antwort ist unbrauchbar. Den betroffenen Socket sofort
    // freigeben; Content-Length verhindert, dass der Browser sie akzeptiert.
    ethResetRequestSlot(index, true);
  }
  else
  {
    // Kein festes delay(1) und kein sofortiges stop() mehr. Der Slot bleibt nur
    // als kleiner Drain-Zustand erhalten; alle anderen Request-Slots laufen
    // normal weiter.
    slot.headersComplete = false;
    slot.responseDraining = true;
    slot.responseDrainStartedMs = millis();
  }

  ethDiagRequestStartMs = 0UL;
  ethDiagRequest = ETH_REQ_NONE;
  ethDiagStage = ETH_DIAG_IDLE;
}

static void ETH_FLASHMEM_NOINLINE ethServiceRequestSlots(void)
{
  int8_t readyIndex = -1;
  bool anyActive = false;

  // Alle belegten Slots bekommen in jedem Hauptloop einen kleinen Leseschritt.
  // Dadurch blockiert eine leere Chrome-Vorverbindung weder /data.json noch
  // /chart.json. Antworten bleiben trotzdem strikt seriell: pro Hauptloop wird
  // hoechstens ein vollstaendiger Request beantwortet.
  for (uint8_t offset = 0U; offset < ETH_REQUEST_SLOT_COUNT; offset++)
  {
    uint8_t index = (uint8_t)((ethRequestRoundRobin + offset) % ETH_REQUEST_SLOT_COUNT);
    EthRequestSlot& slot = ethRequestSlots[index];
    if (!slot.active) continue;
    anyActive = true;

    if (slot.responseDraining)
    {
      ethServiceResponseDrainSlot(index);
      continue;
    }

    if (!slot.headersComplete)
    {
      uint8_t readResult = ethReadRequestSlotStep(index);
      if (readResult == ETH_REQUEST_ABORT)
      {
        uint8_t abortedCode = slot.firstLineComplete
                                ? ethDiagRequestCode(ethRequestLines[index])
                                : (uint8_t)ETH_REQ_NONE;
        ethRecordRequestDuration(index, abortedCode);
        ethResetRequestSlot(index, true);
        continue;
      }
    }

    if (slot.active && slot.headersComplete && readyIndex < 0)
    {
      readyIndex = (int8_t)index;
    }
  }

  // Solange eine grosse HTML-Seite in 512-Byte-Stuecken laeuft, werden neue
  // Requests bereits vollstaendig eingelesen, aber noch nicht beantwortet.
  // Dadurch gibt es pro Hauptloop hoechstens einen Netzwerk-Schreibschritt.
  if (readyIndex >= 0 && !ethPageActive)
  {
    uint8_t index = (uint8_t)readyIndex;
    ethRequestRoundRobin = (uint8_t)((index + 1U) % ETH_REQUEST_SLOT_COUNT);
    ethServiceReadyRequestSlot(index);
    return;
  }

  if (!anyActive || !ethAnyRequestSlotActive())
  {
    ethDiagRequestStartMs = 0UL;
    ethDiagRequest = ETH_REQ_NONE;
  }
  ethDiagStage = ETH_DIAG_IDLE;
}

void ethernetServiceTask(void)
{
  ethDiagStage = ETH_DIAG_CONFIG;
  ethReadConfigSnapshot(false);

  uint32_t nowMs = millis();
  bool haveIp = ethIpIsValid(Ethernet.localIP());

  if (!ethLastEnabled || !ethConfigured)
  {
    ethFileIndexStop();
    if (ethDownloadActive) ethDownloadAbort();
    if (ethPageActive) ethPageTransferAbort();
    if (ethAnyRequestSlotActive()) ethAbortAllPendingRequests();
    ethDiagStage = ETH_DIAG_IDLE;
    return;
  }

  // DHCP-Nachlauf nach genau einem expliziten Ethernet.begin(). Das ist kein
  // automatischer Re-Init-Suchlauf: Es wird nur maintain() aufgerufen und nach
  // Ablauf der Frist abgeschaltet, falls keine IP kommt.
  if (ethLastDhcp && !haveIp && ethDhcpPending)
  {
    if ((nowMs - ethLastMaintainMs) >= 1000UL)
    {
      ethDiagStage = ETH_DIAG_MAINTAIN;
      ethLastMaintainMs = nowMs;
      Ethernet.maintain();
      haveIp = ethIpIsValid(Ethernet.localIP());
      if (haveIp)
      {
        ethDhcpPending = false;
        ethBeginServerIfNeeded();
        ethBeginMdnsIfNeeded();
      }
    }

    if (!haveIp && (int32_t)(nowMs - ethDhcpPendingUntilMs) >= 0)
    {
      ethConfigured = false;
      ethDhcpPending = false;
    }
  }

  if (!ethConfigured || !haveIp)
  {
    ethFileIndexStop();
    if (ethDownloadActive) ethDownloadAbort();
    if (ethPageActive) ethPageTransferAbort();
    if (ethAnyRequestSlotActive()) ethAbortAllPendingRequests();
    ethDiagStage = ETH_DIAG_IDLE;
    return;
  }

  // DHCP-Maintain muss auch dann laufen, wenn linkStatus() gerade nicht LinkON
  // meldet. Sonst kann ein kurzzeitig Unknown/LinkOFF gemeldeter PHY-Status
  // die DHCP-/IP-Anzeige dauerhaft blockieren.
  if (ethLastDhcp && (nowMs - ethLastMaintainMs) >= 1000UL)
  {
    ethDiagStage = ETH_DIAG_MAINTAIN;
    ethLastMaintainMs = nowMs;
    Ethernet.maintain();
  }

  // Erst ab dem ersten echten Hauptloop-Aufruf zaehlen. ethernetServiceBegin()
  // findet bewusst schon vor Display/Touch/ADC statt; wuerde die Frist dort
  // beginnen, waere sie nach dem Splashscreen bereits abgelaufen.
  if (!ethStartupRequestGuardArmed)
  {
    ethAcceptRequestsAfterMs = nowMs + ETH_STARTUP_REQUEST_DELAY_MS;
    ethStartupRequestGuardArmed = true;
    ethDiagStage = ETH_DIAG_IDLE;
    return;
  }

  // Beim Start keine Browser-Anfrage bedienen, bis ADC, Regelung und Anzeige
  // mindestens 2,5 s normale Loopzeit erhalten haben. Die signed-Differenz ist
  // auch beim millis()-Ueberlauf korrekt.
  if ((int32_t)(nowMs - ethAcceptRequestsAfterMs) < 0)
  {
    ethDiagStage = ETH_DIAG_IDLE;
    return;
  }

  // Solange eine Datei laeuft, wird kein zweiter Webclient angenommen.
  // Der adaptive Download bearbeitet maximal 1/2/3/4 Sektoren zu
  // je 512 Byte innerhalb seines Zeitbudgets und kehrt danach sofort in den
  // normalen Mess-/Regel-Hauptloop zurueck. Bei Abschluss oder 15 s ohne
  // Fortschritt werden Datei und TCP-Client hart freigegeben.
  if (ethDownloadActive)
  {
    ethDownloadService();
    return;
  }

  // Haupt-, Setup- und Downloadseite werden blockweise gesendet. Ein Aufruf
  // uebertraegt hoechstens 512 Byte und wartet niemals auf freien TCP-Puffer.
  if (ethPageActive)
  {
    ethPageTransferService();
  }

  if (ethFileIndexState != ETH_FILE_INDEX_EMPTY &&
      ethFileIndexState != ETH_FILE_INDEX_READY &&
      ethFileIndexState != ETH_FILE_INDEX_ERROR)
  {
    // Falls ein einzelner Verzeichniszugriff wider Erwarten lange blockiert,
    // weist der Safety-Snapshot ihn eindeutig der Dateiliste zu.
    ethDiagStage = ETH_DIAG_HANDLE_REQUEST;
    ethDiagRequest = ETH_REQ_FILES_JSON;
    ethFileIndexService();
    ethDiagRequest = ETH_REQ_NONE;
  }

  ethDiagStage = ETH_DIAG_AVAILABLE;
  ethBeginServerIfNeeded();
  ethBeginMdnsIfNeeded();
  if (ethWebServer == nullptr)
  {
    ethDiagStage = ETH_DIAG_IDLE;
    return;
  }

  // Pro Hauptloop hoechstens eine neue Verbindung uebernehmen. accept() wartet
  // nicht auf Nutzdaten. Solange ein Slot frei ist, duerfen deshalb bis zu vier
  // Chrome-/WLAN-Verbindungen parallel ihre vollstaendigen HTTP-Header liefern.
  int8_t freeIndex = ethFindFreeRequestSlot();
  if (freeIndex >= 0)
  {
    EthernetClient client = ethWebServer->accept();
    if (client)
    {
      ethBeginPendingRequest((uint8_t)freeIndex, client);
    }
  }

  ethServiceRequestSlots();
}
