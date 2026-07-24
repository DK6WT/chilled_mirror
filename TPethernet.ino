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
//   /identity     Geraeteidentitaet und Zertifikatsimport
//   /calibration  Export/Import signierter Geraete-/Kopfjustierung und Systemkalibrierung
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
#include <stdarg.h>
#include <new>

#include "TPlanguage.h"
#include "EEPROMAnything.h"
#include "TPsignedData.h"
#include "TPsignedCalibration.h"
#include "TPexternalCalibration.h"
#include "TPcertifiedLog.h"
#include "TPfirmwareIntegrity.h"
#include "TPsystemFirmwareContext.h"

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
static void FLASHMEM ethFormatValidityYmd(uint32_t ymd, char* output, size_t outputSize);
static bool FLASHMEM ethGetQueryToken(const char* requestLine,
                                      const char* key,
                                      char* out,
                                      size_t outSize);

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
extern uint8_t interfaceSdLogIntegrityMode(void);
extern bool interfaceSdCertifiedIntegrityReady(void);
extern bool sdLogSealActiveSegment(const char* expectedBaseName, char* sealedBaseName, size_t sealedBaseNameSize, char* errorText, size_t errorTextSize);
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
extern bool interfaceWebSetSd(uint8_t outputIndex, uint8_t filterIndex, uint8_t writeIntervalIndex, bool loggingEnabled, bool diagnosticEnabled, bool headerEnabled, uint8_t integrityMode);
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
extern bool ledAdaptationSdAvailable(void);
extern uint8_t ledAdaptationModeGet(void);
extern bool ledAdaptationSetMode(uint8_t mode);
extern bool ledAdaptationResetCurrentHead(char* message, size_t messageSize);
extern uint32_t ledAdaptationAcceptedCount(void);
extern uint16_t ledAdaptationValidBinCount(void);
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
#define ETH_DHCP_FIRST_TIMEOUT_MS    3500UL
#define ETH_DHCP_RETRY_TIMEOUT_MS    1800UL
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
// NativeEthernet/FNET besitzt keine saubere end()-API. Deshalb merken wir,
// in welchem Modus der bereits initialisierte Stack laeuft und verwenden eine
// vorhandene gueltige Konfiguration beim Aus-/Einschalten weiter, statt
// Ethernet.begin() unnoetig erneut aufzurufen.
static bool ethRuntimeModeKnown = false;
static bool ethRuntimeDhcp = true;
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

// Maximaler signierter System-JSON-Bereich (16 KiB) plus transportabler
// Anwendbarkeitsanhang; ausgeliefert wird weiterhin nur das reine JSON.
DMAMEM char ethJsonBuf[16896];

// Temporäre Ausgabepuffer der externen Kalibrierscheinanzeige liegen in
// RAM2. Die Webausgabe verwendet ohnehin den gemeinsamen ethJsonBuf und wird
// nicht parallel abgearbeitet; deshalb können diese Puffer sicher geteilt
// werden und belasten den RAM1-Stack nicht.
static DMAMEM char ethExternalSystemRows[1300];
static DMAMEM char ethExternalNumberEscaped[196];
static DMAMEM char ethExternalFileEscaped[420];
static DMAMEM char ethExternalDocumentIdEscaped[80];
static DMAMEM char ethExternalHashEscaped[140];
static DMAMEM char ethExternalUploadCertificateNumber[33];
static DMAMEM char ethExternalUploadOriginalFileName[81];
static DMAMEM char ethExternalUploadError[240];
static DMAMEM char ethExternalUploadResult[300];
static const size_t ETH_CAL_CERT_SEARCH_MAX = 24U;
static DMAMEM TpCalibrationCertificateRecord ethCalibrationCertificateRecords[ETH_CAL_CERT_SEARCH_MAX];
static DMAMEM TpExternalCalibrationMetadata ethExternalArchiveMetadata;
static DMAMEM char ethCalibrationManifestQuery[65];
static DMAMEM char ethCalibrationArchiveError[240];
static DMAMEM TpSystemFirmwareContextStatus ethSystemFirmwareContextStatus;
static DMAMEM char ethFirmwareContextRequestId[33];
static DMAMEM char ethFirmwareContextRequestHash[65];

// Kalibrier-Endpunkte liegen ebenfalls im QSPI-Flash und werden fuer
// Diagnose, Request-Erkennung und Dispatch gemeinsam wiederverwendet.
static const char ETH_PATH_VALIDITY[] PROGMEM = "/validity";
static const char ETH_PATH_CALIBRATION[] PROGMEM = "/calibration";
static const char ETH_PATH_CAL_DEVICE_REQUEST[] PROGMEM = "/calibration-device-request.tpdcalreq";
static const char ETH_PATH_CAL_HEAD_REQUEST[] PROGMEM = "/calibration-head-request.tphcalreq";
static const char ETH_PATH_CAL_SYSTEM_REQUEST[] PROGMEM = "/calibration-system-request.tpscalreq";
static const char ETH_PATH_CAL_SYSTEM_UPLOAD[] PROGMEM = "/calibration-system-upload";
static const char ETH_PATH_CAL_DEVICE_UPLOAD[] PROGMEM = "/calibration-device-upload";
static const char ETH_PATH_CAL_HEAD_UPLOAD[] PROGMEM = "/calibration-head-upload";
static const char ETH_PATH_CAL_DEVICE_IMPORT[] PROGMEM = "/calibration-device-import";
static const char ETH_PATH_CAL_HEAD_IMPORT[] PROGMEM = "/calibration-head-import";
static const char ETH_PATH_CAL_SYSTEM_IMPORT[] PROGMEM = "/calibration-system-import";
static const char ETH_PATH_CAL_CERTIFICATE[] PROGMEM = "/calibration-certificate";
static const char ETH_PATH_CAL_CERTIFICATE_VIEW[] PROGMEM = "/calibration-certificate-view";
static const char ETH_PATH_CAL_CERTIFICATES_JSON[] PROGMEM = "/calibration-certificates.json";
static const char ETH_PATH_CAL_ACTIVE_DEVICE[] PROGMEM = "/calibration-active-device.json";
static const char ETH_PATH_CAL_ACTIVE_HEAD[] PROGMEM = "/calibration-active-head.json";
static const char ETH_PATH_CAL_ACTIVE_SYSTEM[] PROGMEM = "/calibration-active-system.json";
static const char ETH_PATH_CAL_APPLICABILITY[] PROGMEM = "/calibration-applicability.json";
static const char ETH_PATH_CAL_FIRMWARE_CONTEXT[] PROGMEM = "/calibration-firmware-context.json";
static const char ETH_PATH_CAL_EXTERNAL_BEGIN[] PROGMEM = "/calibration-external-pdf-begin";
static const char ETH_PATH_CAL_EXTERNAL_CHUNK[] PROGMEM = "/calibration-external-pdf-chunk";
static const char ETH_PATH_CAL_EXTERNAL_FINISH[] PROGMEM = "/calibration-external-pdf-finish";
static const char ETH_PATH_CAL_EXTERNAL_ABORT[] PROGMEM = "/calibration-external-pdf-abort";
static const char ETH_PATH_CAL_EXTERNAL_DOWNLOAD[] PROGMEM = "/calibration-external-pdf";
static const char ETH_PATH_CAL_EXTERNAL_ACTIVE[] PROGMEM = "/calibration-external-active.json";
static const char ETH_PATH_IDENTITY_ACTIVE_CERT[] PROGMEM = "/identity-active-certificate.json";
static const char ETH_PATH_FIRMWARE_REQUEST[] PROGMEM = "/firmware-request.tpfwreq";
static const char ETH_PATH_FIRMWARE_CERT_UPLOAD[] PROGMEM = "/firmware-certificate-upload";
static const char ETH_PATH_FIRMWARE_CERT_IMPORT[] PROGMEM = "/firmware-certificate-import";

// Nur fuer den externen PDF-Ablauf benoetigte Texte und Formate verbleiben
// im Program-Flash. Sie werden weder in ISRs noch in zeitkritischen Reglern
// benutzt und duerfen auf dem Teensy 4.1 direkt aus dem gemappten Flash
// gelesen werden.
static const char ETH_EXTERNAL_SOURCE_OWN[] PROGMEM = "Eigener TP-3000-Kalibrierschein";
static const char ETH_EXTERNAL_SOURCE_PDF[] PROGMEM = "Externer PDF-Kalibrierschein";
static const char ETH_EXTERNAL_STATUS_NONE[] PROGMEM = "Kein externer PDF-Kalibrierschein auf SD aktiv";
static const char ETH_EXTERNAL_STATUS_VALID[] PROGMEM = "Auf SD gespeichert – innerhalb Gültigkeitszeitraum";
static const char ETH_EXTERNAL_STATUS_EXPIRED[] PROGMEM = "Auf SD gespeichert – Gültigkeitszeitraum abgelaufen";
static const char ETH_EXTERNAL_STATUS_FUTURE[] PROGMEM = "Auf SD gespeichert – Gültigkeitszeitraum beginnt später";
static const char ETH_EXTERNAL_STATUS_UNKNOWN[] PROGMEM = "Auf SD gespeichert – Zeitstatus nicht beurteilbar";
static const char ETH_EXTERNAL_ACTIVE_NONE[] PROGMEM = "Kein aktiver externer PDF-Kalibrierschein";
static const char ETH_EXTERNAL_METADATA_TOO_LARGE[] PROGMEM = "Metadaten sind zu groß";
static const char ETH_EXTERNAL_METADATA_INCOMPLETE[] PROGMEM = "PDF-Metadaten sind unvollständig";
static const char ETH_EXTERNAL_BEGIN_FAILED[] PROGMEM = "PDF-Upload konnte nicht gestartet werden";
static const char ETH_EXTERNAL_BEGIN_OK[] PROGMEM = "PDF-Upload auf SD vorbereitet";
static const char ETH_EXTERNAL_CHUNK_MISSING[] PROGMEM = "PDF-Block oder Offset fehlt";
static const char ETH_EXTERNAL_CHUNK_REJECTED[] PROGMEM = "PDF-Block wurde abgewiesen";
static const char ETH_EXTERNAL_CHUNK_OK[] PROGMEM = "OK";
static const char ETH_EXTERNAL_FINISH_FAILED[] PROGMEM = "PDF-Upload konnte nicht abgeschlossen werden";
static const char ETH_EXTERNAL_ABORT_OK[] PROGMEM = "PDF-Upload abgebrochen und temporäre Datei entfernt";
static const char ETH_EXTERNAL_SD_NOT_READY[] PROGMEM = "SD-Karte nicht bereit";
static const char ETH_EXTERNAL_PDF_NOT_READABLE[] PROGMEM = "Externer PDF-Kalibrierschein ist nicht lesbar";
static const char ETH_EXTERNAL_SIZE_MISMATCH[] PROGMEM = "PDF-Dateigröße stimmt nicht mit den geprüften Metadaten überein";
static const char ETH_EXTERNAL_QUERY_NUMBER[] PROGMEM = "number";
static const char ETH_EXTERNAL_QUERY_FROM[] PROGMEM = "from";
static const char ETH_EXTERNAL_QUERY_UNTIL[] PROGMEM = "until";
static const char ETH_EXTERNAL_QUERY_NAME[] PROGMEM = "name";
static const char ETH_EXTERNAL_QUERY_SIZE[] PROGMEM = "size";
static const char ETH_EXTERNAL_QUERY_OFFSET[] PROGMEM = "offset";
static const char ETH_EXTERNAL_ROWS_TEMPLATE[] PROGMEM =
  "<tr><td>Kalibrierschein-Nr. / Zeichen</td><td>%s</td></tr>"
  "<tr><td>PDF-Gültig von</td><td>%s</td></tr>"
  "<tr><td>PDF-Gültig bis</td><td>%s</td></tr>"
  "<tr><td>PDF-Datei</td><td>%s</td></tr>"
  "<tr><td>PDF-SHA-256</td><td><code>%s</code></td></tr>";
static const char ETH_EXTERNAL_JSON_TEMPLATE[] PROGMEM =
  "{\n"
  "  \"Format\": \"TP3000-EXTERNAL-CALIBRATION-PDF-1\",\n"
  "  \"DocumentId\": \"%s\",\n"
  "  \"CertificateNumber\": \"%s\",\n"
  "  \"ValidFromYmd\": %lu,\n"
  "  \"ValidUntilYmd\": %lu,\n"
  "  \"OriginalFileName\": \"%s\",\n"
  "  \"FileSize\": %lu,\n"
  "  \"Sha256\": \"%s\"\n"
  "}\n";

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
static const uint16_t ETH_UPLOAD_READ_BYTES_PER_LOOP     = 512U;
static const size_t   ETH_UPLOAD_MAX_BYTES               = 16383U;
static const uint32_t ETH_UPLOAD_IDLE_TIMEOUT_MS         = 2000UL;
static const uint32_t ETH_UPLOAD_ABSOLUTE_TIMEOUT_MS     = 15000UL;

struct EthRequestSlot
{
  EthernetClient client;
  bool active;
  bool sawAnyByte;
  bool firstLineComplete;
  bool headerLineHasData;
  bool headersComplete;
  bool bodyExpected;
  bool responseDraining;
  size_t requestLength;
  size_t headerCacheLength;
  size_t contentLength;
  size_t bodyLength;
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
static int8_t ethUploadOwnerSlot = -1;
static DMAMEM char ethUploadBody[ETH_UPLOAD_MAX_BYTES + 1U];


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
  ETH_REQ_SETUP_DISPLAY = 41,
  ETH_REQ_IDENTITY_PAGE = 42,
  ETH_REQ_IDENTITY_KEYGEN = 43,
  ETH_REQ_IDENTITY_REQUEST = 44,
  ETH_REQ_IDENTITY_CERT_IMPORT = 45,
  ETH_REQ_CALIBRATION_PAGE = 46,
  ETH_REQ_CALIBRATION_DEVICE_REQUEST = 47,
  ETH_REQ_CALIBRATION_HEAD_REQUEST = 48,
  ETH_REQ_CALIBRATION_DEVICE_IMPORT = 49,
  ETH_REQ_CALIBRATION_HEAD_IMPORT = 50,
  ETH_REQ_CALIBRATION_CERTIFICATE = 51,
  ETH_REQ_CALIBRATION_ACTIVE_DEVICE = 52,
  ETH_REQ_CALIBRATION_ACTIVE_HEAD = 53,
  ETH_REQ_CALIBRATION_SYSTEM_REQUEST = 54,
  ETH_REQ_CALIBRATION_SYSTEM_UPLOAD = 55,
  ETH_REQ_CALIBRATION_ACTIVE_SYSTEM = 56,
  ETH_REQ_CALIBRATION_DEVICE_UPLOAD = 57,
  ETH_REQ_CALIBRATION_HEAD_UPLOAD = 58,
  ETH_REQ_IDENTITY_ACTIVE_CERTIFICATE = 59,
  ETH_REQ_CALIBRATION_EXTERNAL_BEGIN = 60,
  ETH_REQ_CALIBRATION_EXTERNAL_CHUNK = 61,
  ETH_REQ_CALIBRATION_EXTERNAL_FINISH = 62,
  ETH_REQ_CALIBRATION_EXTERNAL_ABORT = 63,
  ETH_REQ_CALIBRATION_EXTERNAL_DOWNLOAD = 64,
  ETH_REQ_CALIBRATION_EXTERNAL_ACTIVE = 65,
  ETH_REQ_CALIBRATION_CERTIFICATE_VIEW = 66,
  ETH_REQ_CALIBRATION_CERTIFICATES_JSON = 67,
  ETH_REQ_VALIDITY_PAGE = 68,
  ETH_REQ_FIRMWARE_REQUEST = 69,
  ETH_REQ_FIRMWARE_CERT_UPLOAD = 70,
  ETH_REQ_FIRMWARE_CERT_IMPORT = 71,
  ETH_REQ_CALIBRATION_SYSTEM_IMPORT = 72
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
    case ETH_REQ_IDENTITY_PAGE: return "/identity";
    case ETH_REQ_IDENTITY_KEYGEN: return "/identity-keygen";
    case ETH_REQ_IDENTITY_REQUEST: return "/identity-request.tpreq";
    case ETH_REQ_IDENTITY_CERT_IMPORT: return "/identity-cert-import";
    case ETH_REQ_CALIBRATION_PAGE: return ETH_PATH_CALIBRATION;
    case ETH_REQ_CALIBRATION_DEVICE_REQUEST: return ETH_PATH_CAL_DEVICE_REQUEST;
    case ETH_REQ_CALIBRATION_HEAD_REQUEST: return ETH_PATH_CAL_HEAD_REQUEST;
    case ETH_REQ_CALIBRATION_DEVICE_IMPORT: return ETH_PATH_CAL_DEVICE_IMPORT;
    case ETH_REQ_CALIBRATION_HEAD_IMPORT: return ETH_PATH_CAL_HEAD_IMPORT;
    case ETH_REQ_CALIBRATION_SYSTEM_IMPORT: return ETH_PATH_CAL_SYSTEM_IMPORT;
    case ETH_REQ_CALIBRATION_CERTIFICATE: return ETH_PATH_CAL_CERTIFICATE;
    case ETH_REQ_CALIBRATION_CERTIFICATE_VIEW: return ETH_PATH_CAL_CERTIFICATE_VIEW;
    case ETH_REQ_CALIBRATION_CERTIFICATES_JSON: return ETH_PATH_CAL_CERTIFICATES_JSON;
    case ETH_REQ_CALIBRATION_ACTIVE_DEVICE: return ETH_PATH_CAL_ACTIVE_DEVICE;
    case ETH_REQ_CALIBRATION_ACTIVE_HEAD: return ETH_PATH_CAL_ACTIVE_HEAD;
    case ETH_REQ_CALIBRATION_SYSTEM_REQUEST: return ETH_PATH_CAL_SYSTEM_REQUEST;
    case ETH_REQ_CALIBRATION_SYSTEM_UPLOAD: return ETH_PATH_CAL_SYSTEM_UPLOAD;
    case ETH_REQ_CALIBRATION_ACTIVE_SYSTEM: return ETH_PATH_CAL_ACTIVE_SYSTEM;
    case ETH_REQ_CALIBRATION_DEVICE_UPLOAD: return ETH_PATH_CAL_DEVICE_UPLOAD;
    case ETH_REQ_CALIBRATION_HEAD_UPLOAD: return ETH_PATH_CAL_HEAD_UPLOAD;
    case ETH_REQ_IDENTITY_ACTIVE_CERTIFICATE: return ETH_PATH_IDENTITY_ACTIVE_CERT;
    case ETH_REQ_CALIBRATION_EXTERNAL_BEGIN: return ETH_PATH_CAL_EXTERNAL_BEGIN;
    case ETH_REQ_CALIBRATION_EXTERNAL_CHUNK: return ETH_PATH_CAL_EXTERNAL_CHUNK;
    case ETH_REQ_CALIBRATION_EXTERNAL_FINISH: return ETH_PATH_CAL_EXTERNAL_FINISH;
    case ETH_REQ_CALIBRATION_EXTERNAL_ABORT: return ETH_PATH_CAL_EXTERNAL_ABORT;
    case ETH_REQ_CALIBRATION_EXTERNAL_DOWNLOAD: return ETH_PATH_CAL_EXTERNAL_DOWNLOAD;
    case ETH_REQ_CALIBRATION_EXTERNAL_ACTIVE: return ETH_PATH_CAL_EXTERNAL_ACTIVE;
    case ETH_REQ_VALIDITY_PAGE: return ETH_PATH_VALIDITY;
    case ETH_REQ_FIRMWARE_REQUEST: return ETH_PATH_FIRMWARE_REQUEST;
    case ETH_REQ_FIRMWARE_CERT_UPLOAD: return ETH_PATH_FIRMWARE_CERT_UPLOAD;
    case ETH_REQ_FIRMWARE_CERT_IMPORT: return ETH_PATH_FIRMWARE_CERT_IMPORT;
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
  ".measureFrame{position:absolute;left:12px;top:50px;width:776px;height:286px;border:3px solid var(--gray);border-radius:10px;box-sizing:border-box;pointer-events:none;z-index:3}\n"
  ".big{position:absolute;left:115px;top:110px;width:570px;font-size:60px;line-height:1.25;white-space:nowrap;color:#fff;z-index:2;display:flex;flex-direction:column;align-items:stretch;justify-content:center}\n.bigRow{display:grid;grid-template-columns:132px 13px 280px 1fr;width:100%;align-items:baseline}.mainLabel{font-size:16px;text-align:right;line-height:1}.mainColon{font-size:16px;text-align:center;line-height:1}.mainValue{display:grid;grid-template-columns:3ch 1ch 3ch;width:7ch;align-items:baseline;justify-self:center}.bigInt{text-align:right}.bigDot{text-align:center}.bigFrac{text-align:left}.bigUnit{text-align:left}.mainRhPct{font-size:.70em;font-weight:550;position:relative;top:.01em}.big3{left:20px;top:76px;width:760px;font-size:56px;line-height:1.25}.big3 .bigRow{grid-template-columns:192px 13px 350px 1fr}\n"
  "#chartCanvas{position:absolute;left:12px;top:50px;width:776px;height:286px;display:none;background:#000;z-index:1}\n"
  "#viewHit{position:absolute;left:12px;top:50px;width:776px;height:286px;background:transparent;z-index:4;cursor:pointer;outline:none}\n#rangeHit{position:absolute;left:628px;top:52px;width:128px;height:40px;background:transparent;z-index:5;cursor:pointer;outline:none;display:none}\n"
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
  ".webPanel{width:min(100vw,800px);box-sizing:border-box;background:#1b1b1b;border:1px solid #333;border-radius:8px;padding:10px;color:#eee;font-family:Arial,sans-serif;font-size:14px;display:flex;align-items:center;flex-wrap:wrap}\n"
  ".webPanel button{background:#303030;color:#fff;border:1px solid #666;border-radius:6px;padding:8px 12px;cursor:pointer;margin-right:8px}.webPanel button:hover{background:#444}.webPanel .calSheetBtn{margin-left:auto}.webPanel .infoBtn{margin-left:0;margin-right:0}.msg{margin-left:4px;color:#bbb}\n"
  "@media(max-width:700px){.webPanel{font-size:13px}}\n</style></head><body>\n<div id='screenWrap'><div id='screen'>\n<div class='title' id='title'>TAUPUNKTSPIEGEL HYGROMETER</div>\n<div class='clock' id='time'>--:--:--</div>\n"
  "<canvas id='chartCanvas' width='776' height='286'></canvas>\n<div class='measureFrame b-gray' id='measureFrame'></div>\n<div id='viewHit' role='button' tabindex='0' aria-label='Ansicht wechseln'></div><div id='rangeHit' role='button' tabindex='0' aria-label='Zeitraum wechseln'></div>\n"
  "<div class='big' id='bigVals'><div class='bigRow'><span class='mainLabel' id='rhLabel'>Rel. Feuchte</span><span class='mainColon'>:</span><span class='mainValue'><span class='bigInt' id='rhInt'>--</span><span class='bigDot'>.</span><span class='bigFrac' id='rhFrac'>--</span></span><span class='bigUnit'><span class='mainRhPct'>%</span>rH</span></div><div class='bigRow'><span class='mainLabel' id='dewLabel'>Taupunkt</span><span class='mainColon'>:</span><span class='mainValue'><span class='bigInt' id='dewInt'>--</span><span class='bigDot'>.</span><span class='bigFrac' id='dewFrac'>---</span></span><span class='bigUnit'>°C</span></div><div class='bigRow' id='mainTaRow' style='display:none'><span class='mainLabel' id='taMainLabel'>T-Umgebung</span><span class='mainColon'>:</span><span class='mainValue'><span class='bigInt' id='taMainInt'>--</span><span class='bigDot'>.</span><span class='bigFrac' id='taMainFrac'>---</span></span><span class='bigUnit'>°C</span></div></div>\n<div class='chartAlt metricLine' id='chartAlt'><span class='metricLabel' id='chartAltLabel'></span><span class='metricColon'>:</span><span class='metricValue' id='chartAltValue'></span><span class='metricUnit' id='chartAltUnit'></span></div>\n<div class='alarmBox' id='alarmBox'>ALARM</div>\n"
  "<div class='temps' id='temps'><div class='infoLine' id='tmLine'><span class='infoLabel'>T-Spiegel</span><span class='infoColon'>:</span><span class='numInt' id='tmInt'>--</span><span class='numDot'>.</span><span class='numFrac' id='tmFrac'>---</span><span class='numUnit'>°C</span></div><div class='infoLine' id='taLine'><span class='infoLabel'>T-Umgeb.</span><span class='infoColon'>:</span><span class='numInt' id='taInt'>--</span><span class='numDot'>.</span><span class='numFrac' id='taFrac'>---</span><span class='numUnit'>°C</span></div></div>\n<div class='status flow' id='flowbox'>F: --.--</div>\n<div class='tExt metricLine' id='tExtBox'><span class='metricLabel' id='tExtLabel'>T-Ext</span><span class='metricColon'>:</span><span class='metricValue' id='tExtValue'>--.--</span><span class='metricUnit' id='tExtUnit'>°C</span></div>\n"
  "<div class='status pi'><div class='infoLine'><span class='infoLabel'>P: <span id='p'>--.--</span> hPa | I</span><span class='infoColon'>:</span><span class='numInt iIntCell'><span class='iModeDot' id='iModeDot' aria-hidden='true'></span><span id='iInt'>--</span></span><span class='numDot'>.</span><span class='numFrac' id='iFrac'>---</span><span class='numUnit'>A</span></div></div>\n"
  "<a class='setup' id='setupBtn' href='/setup'>Setup<span id='setupHint'></span></a>\n<div class='mini'>\n"
  "<div id='fanItem' class='item'><span id='fanSq' class='sq green'></span><span id='fanText'>FAN ON</span></div>\n"
  "<div id='measItem' class='item'><span id='measSq' class='sq gray'></span><span id='measText'>";

static const char ethMainPagePart1[] PROGMEM =
  "</span></div>\n<div id='safetyItem' class='item'><span id='safetySq' class='sq green'></span><span>Safety</span></div>\n<div id='sdItem' class='item'><span id='sdSq' class='sq white'></span><span>SD-Logging</span></div>\n"
  "<div id='ethItem' class='item'><span id='ethSq' class='sq red'></span><span id='ip'>---.---.---.---</span></div>\n</div></div></div>\n"
  "<div class='webPanel'><button id='filesBtn' onclick=\"location.href='/download'\">Dateien</button><button id='certBtn' onclick=\"location.href='/identity'\">Zertifizierung</button><button id='snapshotBtn' onclick='saveScreenSnapshot()'>Snapshot</button><button id='fullscreenBtn' onclick='toggleFullscreen()'>Vollbild</button><button id='syncTimeBtn' onclick='setDeviceTime()'>PC-Zeit auf Gerät übertragen</button><span id='syncMsg' class='msg'></span><button id='calSheetBtn' class='calSheetBtn' onclick=\"location.href='/calibration-certificate'\">Kalibrierschein</button><button id='infoBtn' class='infoBtn' onclick=\"location.href='/validity'\">Info</button></div>\n<script>\nconst E=id=>document.getElementById(id);let uiLang=0;const tr=(de,en)=>uiLang===1?en:de;function renderWebUi(){document.documentElement.lang=uiLang===1?'en':'de';document.title=tr('Taupunktspiegel','Dew point mirror');E('filesBtn').textContent=tr('Dateien','Files');E('certBtn').textContent=tr('Zertifizierung','Certification');E('snapshotBtn').textContent='Snapshot';E('fullscreenBtn').textContent=tr('Vollbild','Fullscreen');E('syncTimeBtn').textContent=tr('PC-Zeit auf Gerät übertragen','Transfer PC time to device');E('calSheetBtn').textContent=tr('Kalibrierschein','Calibration certificate');E('infoBtn').textContent='Info';E('viewHit').setAttribute('aria-label',tr('Ansicht wechseln','Change view'));E('rangeHit').setAttribute('aria-label',tr('Zeitraum wechseln','Change time range'));E('rhLabel').textContent=tr('Rel. Feuchte','Rel. humidity');E('dewLabel').textContent=tr('Taupunkt','Dew point');E('taMainLabel').textContent=tr('T-Umgebung','T-Ambient')}\n"
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
  "function setBoxGeometry(el,x,y,w,h,r){el.style.left=x+'px';el.style.top=y+'px';el.style.width=w+'px';el.style.height=h+'px';if(r!==undefined)el.style.borderRadius=r+'px'}\nfunction isAltMain(){return view===0&&lastData&&Number(lastData.mainLayout)===1}\nfunction applyMainLayout(){if(view!==0)return;let alt=isAltMain();setBoxGeometry(E('measureFrame'),12,50,776,286,10);setBoxGeometry(E('viewHit'),12,50,776,286);E('bigVals').className=alt?'big big3':'big';E('mainTaRow').style.display=alt?'grid':'none';E('temps').style.top=alt?'391px':'355px';E('taLine').style.display=alt?'none':'grid'}\n"
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
  "async function setDeviceTime(){let n=new Date(),q='?y='+n.getFullYear()+'&mo='+(n.getMonth()+1)+'&d='+n.getDate()+'&h='+n.getHours()+'&mi='+n.getMinutes()+'&s='+n.getSeconds()+'&tz='+(-n.getTimezoneOffset());E('syncMsg').textContent=tr('übertrage...','transferring...');try{"
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
 ledAdapt:[[0,'Aus|Off'],[1,'Ein - Grundkurve|On - base curve'],[2,'Selbstlernend|Self-learning']],
 alarmMode:[[0,'Aus|Off'],[1,'Grenzwert|Limit'],[2,'Bereich|Range']], onoff:[[0,'Aus|Off'],[1,'Ein|On']],
 outInt:[[0,'ADC'],[1,'1 s'],[2,'10 s'],[3,'30 s'],[4,'60 s']], filter:[[0,'0 s'],[1,'1 s'],[2,'3 s'],[3,'5 s'],[4,'10 s']],
 rsMode:[[0,'Aus|Off'],[1,'PC Text|PC text'],[2,'PC CSV|PC CSV'],[3,'Durchfluss Eingang|Flow input'],[4,'Durchfluss Poll|Flow poll'],[5,'ALMEMO'],[6,'WinControl|WinControl']],
 baud:[[0,'9600'],[1,'19200'],[2,'38400'],[3,'57600'],[4,'115200']], rsOut:[[0,'Zyklisch|Cyclic'],[1,'Auf Anfrage|On request'],[2,'Beides|Both']],
 usb:[[0,'Kommandos|Commands'],[1,'CSV'],[2,'Debug']], sdWrite:[[0,'10 s'],[1,'60 s'],[2,'3 min'],[3,'5 min']], logIntegrity:[[0,'Aus|Off'],[1,'SHA-256'],[2,'CSV zertifiziert|Certified CSV'],[3,'TPLOG zertifiziert|Certified TPLOG']],
 flow:[[0,'Aus|Off'],[1,'l/min'],[2,'l/s'],[3,'m3/h'],[4,'T-Ext']], almemoInt:[[1,'1 s'],[2,'10 s']], almemoRole:[[1,'Temperatur|Temperature'],[2,'Flow'],[3,'Druck|Pressure'],[4,'Allgemein|General']], winOut:[[0,'Aus|Off'],[1,'RS232-1'],[2,'RS232-2'],[3,'Ethernet']], winCycle:[[0,'1 s'],[1,'10 s'],[2,'20 s'],[3,'60 s']], language:[[0,'Deutsch'],[1,'English']], screenLayout:[[0,'2 Werte|2 values'],[1,'3 Werte|3 values']], headType:[[0,'STP-3001'],[1,'STP-3002'],[2,'STP-3003'],[3,'STP-3004']]
};
function ol(list){return list.map(x=>[x[0],x[1].split('|')[lang===EN?1:0]||x[1].split('|')[0]])}
function resize(){E('screen').style.transform='scale('+(E('wrap').clientWidth/800)+')'}
window.addEventListener('resize',resize);resize();
function msg(t,ok=true){E('message').style.color=ok?'var(--yellow)':'var(--red)';E('message').textContent=t||''}
function markDirty(){dirty=true;renderHint()}
function rootItems(){return [
 [tx('Regelparameter','Control parameters'),()=>openControl()],
 [tx('Statusinformationen','Status information'),()=>openStatusInfo()],
 [tx('Info / Gültigkeit','Info / validity'),()=>openValidityInfoWeb()],
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
function renderInfoPage(){let b=E('body');b.innerHTML='';let p=document.createElement('div');p.className='page infoPage';p.innerHTML='<div class="infoHead">'+tx('LIZENZEN / MARKEN','LICENSES / TRADEMARKS')+'</div><div>'+tx('TP-3000 TAUPUNKTSPIEGEL-FIRMWARE','TP-3000 DEW POINT MIRROR FIRMWARE')+'</div><div>Version 0.50.1 | Build 0.50.1_87 | '+tx('24.07.2026','2026-07-24')+'</div><div>Copyright (C) 2025/2026 S. Brachtl</div><div class="infoGap"></div><div>'+tx('Basiert auf LJ2000M T_2.06c von:','Based on LJ2000M T_2.06c by:')+'</div><div>&nbsp;&nbsp;Loftur E. Jonasson</div><div>&nbsp;&nbsp;J.G. Holstein</div><div class="infoGap"></div><div class="infoLicense">GNU General Public License Version 3 only</div><div>SPDX-License-Identifier: GPL-3.0-only</div><div>'+tx('Freie Software: Weitergabe und Änderung nach GNU GPLv3-only erlaubt.','Free software: redistribution and modification are permitted under GPLv3-only.')+'</div><div>'+tx('Komponenten Dritter behalten ihre Lizenzen.','Third-party components retain their licenses.')+'</div><div>'+tx('Dieses Programm kommt OHNE JEDE GEWÄHRLEISTUNG.','This program comes with ABSOLUTELY NO WARRANTY.')+'</div><div class="infoMuted">'+tx('Quellcode und vollständige Lizenz:','Source code and complete license:')+'</div><div><a href="https://github.com/DK6WT/chilled_mirror" target="_blank" rel="noopener">github.com/DK6WT/chilled_mirror</a></div><div class="infoMuted"><a href="https://github.com/DK6WT/chilled_mirror/blob/main/LICENSE" target="_blank" rel="noopener">LICENSE</a> | <a href="https://github.com/DK6WT/chilled_mirror/blob/main/THIRD_PARTY_NOTICES.md" target="_blank" rel="noopener">THIRD_PARTY_NOTICES.md</a></div><div class="infoGap"></div><div class="infoLicense">ALMEMO&reg; / WinControl</div><div>'+tx('ALMEMO ist eine von AHLBORN verwendete Produktbezeichnung und Marke. AMR WinControl (im Gerät kurz WinControl) wird von akrobit entwickelt und für ALMEMO-Systeme über AHLBORN vertrieben.','ALMEMO is a product name and trademark used by AHLBORN. AMR WinControl (shown as WinControl in the device) is developed by akrobit and distributed for ALMEMO systems through AHLBORN.')+'</div><div>'+tx('Die Bezeichnungen ALMEMO und WinControl werden ausschließlich zur sachlichen Beschreibung optionaler Schnittstellen verwendet: optionale serielle Anbindung an ALMEMO-Messgeräte und Messwertausgabe im ALMEMO-V6-Format zur Nutzung mit WinControl über RS232 oder Ethernet/TCP.','The ALMEMO and WinControl names are used solely to describe optional interfaces: optional serial connection to ALMEMO measuring instruments and measurement output in ALMEMO V6 format for use with WinControl over RS232 or Ethernet/TCP.')+'</div><div>'+tx('TP-3000 ist ein unabhängiges Projekt. Keine Verbindung, Unterstützung, Freigabe, Prüfung oder Zertifizierung durch AHLBORN oder akrobit.','TP-3000 is an independent project. No affiliation, support, endorsement, approval, testing or certification by AHLBORN or akrobit.')+'</div><div>'+tx('Es wird kein ALMEMO-, WinControl-, AHLBORN- oder akrobit-Logo verwendet. Die Beschreibung ist keine allgemeine Kompatibilitätszusage.','No ALMEMO, WinControl, AHLBORN or akrobit logo is used. This is not a general compatibility claim.')+'</div><div class="infoMuted">LICENSES/ALMEMO-TRADEMARK-NOTICE.txt</div>';b.appendChild(p)}
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
async function openValidityInfoWeb(){if(setupClosing)return;setupClosing=true;setupPageActive=false;try{await fetch('/setup-close',{cache:'no-store'})}catch(e){}location.href='/validity'}
function openInfoLicense(){page(tx('LIZENZEN / MARKEN','LICENSES / TRADEMARKS'),'info')}
function controlSummaryItems(){let c=S.control,m=Number(c.ledAdaptMode)||0,mt=m===2?tx('selbstlernend','self-learning'):(m===1?tx('Grundkurve','base curve'):tx('aus','off'));return [[tx('Status: gesperrt','Status: locked'),()=>msg(tx('Zum Aendern Werte aendern waehlen','Select edit values to change'))],['Peltier max: '+(Number(c.peltier)/1000).toFixed(3)+' A',()=>{}],['Kp/Ki/Kd: '+c.kp+' / '+(Number(c.ki10)/10).toFixed(1)+' / '+(Number(c.kd10)/10).toFixed(1),()=>{}],['Takt: '+c.period+' ms  '+tx('Luefter','Fan')+': '+c.fan+'%',()=>{}],[tx('LED-Autoadaption','LED auto-adaptation')+': '+mt+'  n='+(Number(c.ledAdaptCount)||0),()=>{}],[tx('Werte aendern','Edit values'),openControlPassword],[tx('Zurueck','Back'),back]]}
function openControl(){if(webControlUnlocked)openControlEdit();else openMenu(tx('Regelparameter','Control parameters'),controlSummaryItems(),false)}
function openControlPassword(){let f=nf(tx('Passwort','Password'),0,1,0,99999,1,0);form(tx('Passwort eingeben','Enter password'),[f],async()=>{let pw=pad5(f.value);await api('/setup-control-unlock?'+q({pw}));webControlUnlocked=true;webControlPw=pw;parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;openControlEdit()})}
function openControlEdit(){let c=S.control,adaptOpts=c.ledAdaptSd?opts.ledAdapt:[[0,'Aus|Off'],[1,'Ein - Grundkurve|On - base curve']],fs=[nf('Peltier max',c.peltier,1000,0,4000,10,3,'A'),nf('Kp',c.kp,1,0,1000,1,0),nf('Ki',c.ki10,10,0,500,1,1),nf('Kd',c.kd10,10,0,500,1,1),sf(tx('Regeltakt','Control period'),c.period,opts.period),nf(tx('Optik-Ziel','Optical target'),c.optik10,10,800,990,1,1,'%'),nf(tx('Freiheizen','Heat-clean'),c.freeTemp,1,40,76,1,0,'C'),sf(tx('Auto-Cal Intervall','Auto-cal interval'),c.autoIdx,opts.auto),sf(tx('LED-Autoadaption','LED auto-adaptation'),c.ledAdaptMode,adaptOpts),sf(tx('LED-Lerndaten zurücksetzen','Reset LED learning data'),0,c.ledAdaptSd?[[0,'Nein|No'],[1,'JA - aktueller Kopf|YES - current head']]:[[0,'Keine SD-Karte|No SD card']]),sf(tx('Messfilter','Measurement filter'),c.adcFilter,opts.adcFilter),sf('ADC1 SFOCAL',c.sfocal,opts.sfocal)];form(tx('Regelparameter','Control parameters'),fs,async()=>{if(Number(fs[9].value)===1&&!confirm(tx('LED-Lerndaten nur für den aktuell gewählten Kopf löschen?','Delete LED learning data only for the currently selected head?')))return;await api('/setup-control?'+q({pw:webControlPw,peltier:fs[0].value,kp:fs[1].value,ki:fs[2].value,kd:fs[3].value,period:fs[4].value,optik:fs[5].value,free:fs[6].value,auto:fs[7].value,ledadapt:fs[8].value,resetlearn:fs[9].value,filter:fs[10].value,sfocal:fs[11].value}));webControlLock();parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;render()});current.controlEdit=true}
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
function openDeviceStorageAction(a,title,line){let needsPw=(a==='factory_save'),certified=!!(S.head&&S.head.certified),f=nf(tx('Passwort','Password'),0,1,0,99999,1,0),sn=nf(tx('Geraete-SN','Device SN'),Number((S.head&&S.head.device)||0),1,0,99999,1,0),fields=[rf(tx('Aktion','Action'),line)];if(needsPw){fields.push(f);if(!certified)fields.push(sn)}form(title,fields,async()=>{let p={a};if(needsPw){p.pw=pad5(f.value);if(!certified)p.sn=pad5(sn.value)}await api('/setup-device-storage?'+q(p));if(needsPw&&!certified){S.head=S.head||{};S.head.device=pad5(sn.value)}webControlLock();parents=[];current={kind:'menu',title:'Setup',items:rootItems(),root:true};mode='menu';sel=0;openDeviceStorageMenu()},true)}
function openCalibrationMenu(){openMenu(tx('Kalibrierung','Calibration'),[[tx('Pt100 R0','Pt100 R0'),openR0],[tx('Pt100 2-Punkt Spiegel','Pt100 2-point mirror'),()=>open2P(0)],[tx('Pt100 2-Punkt Umgebung','Pt100 2-point ambient'),()=>open2P(1)],[tx('Referenz-/Kanalwerte','Reference/channel values'),openRef],[tx('Taupunkt-Offset','Dew-point offset'),openOffset],[tx('Zurück','Back'),back]],false)}
function openR0(){let c=S.cal,fs=[nf(tx('Spiegel Pt100 R0','Mirror Pt100 R0'),c.r0m,10000,800000,1200000,1,4,'Ohm'),nf(tx('Umgebung Pt100 R0','Ambient Pt100 R0'),c.r0a,10000,800000,1200000,1,4,'Ohm')];form('Pt100 R0',fs,()=>api('/setup-r0?'+q({m:fs[0].value,a:fs[1].value})),true)}
function open2P(sensor){let a=sensor?S.cal.p2a:S.cal.p2m,fs=[nf(tx('Soll 1','Set 1'),a[0],1000,-30000,90000,1,3,'C'),nf(tx('Ist 1','Actual 1'),a[1],1000,-30000,90000,1,3,'C'),nf(tx('Soll 2','Set 2'),a[2],1000,-30000,90000,1,3,'C'),nf(tx('Ist 2','Actual 2'),a[3],1000,-30000,90000,1,3,'C')];form(sensor?tx('Umgebung Pt100 2-Punkt','Ambient Pt100 2-point'):tx('Spiegel Pt100 2-Punkt','Mirror Pt100 2-point'),fs,()=>api((sensor?'/setup-2pa?':'/setup-2pm?')+q({s1:fs[0].value,i1:fs[1].value,s2:fs[2].value,i2:fs[3].value})),true)}
function openRef(){let c=S.cal,fs=[df(tx('Kalibrierdatum','Calibration date'),c.refDate),nf('Ref Low',c.ref100,100000,6000000,11000000,1,5,'Ohm'),nf('Ref High',c.ref120,100000,11500000,15200000,1,5,'Ohm'),nf(tx('Korr A Low','Corr A Low'),c.a100,100000,-10000,10000,1,5,'Ohm'),nf(tx('Korr A High','Corr A High'),c.a120,100000,-10000,10000,1,5,'Ohm'),nf(tx('Korr B Low','Corr B Low'),c.b100,100000,-10000,10000,1,5,'Ohm'),nf(tx('Korr B High','Corr B High'),c.b120,100000,-10000,10000,1,5,'Ohm')];form(tx('Referenz-/Kanalwerte','Reference/channel values'),fs,()=>api('/setup-ref?'+q({date:fs[0].value,r100:fs[1].value,r120:fs[2].value,a100:fs[3].value,a120:fs[4].value,b100:fs[5].value,b120:fs[6].value})),true)}
function openOffset(){let f=nf(tx('Taupunkt-/Frostpunkt-Offset','Dew/frost-point offset'),S.cal.offset,1000,-2000,2000,1,3,'C');form(tx('Taupunkt-Offset','Dew-point offset'),[f],()=>api('/setup-offset?'+q({v:f.value})),true)}
function openInterfaceMenu(){openMenu(tx('Schnittstellen','Interfaces'),[[tx('Ausgabe allgemein','General output'),openGeneral],[tx('RS232-1','RS232-1'),()=>openRs(0)],[tx('RS232-2','RS232-2'),()=>openRs(1)],['USB',openUsb],[tx('SD-Karte','SD card'),openSd],[tx('Zusatzanzeige','Additional display'),openFlow],['ALMEMO',openAlmemo],[tx('WinControl-Ausgabe','WinControl output'),openWinControl],[tx('Ethernet Information','Ethernet information'),openEthInfo],[tx('Zurück','Back'),back]],false)}
function openGeneral(){let i=S.iface,fs=[sf(tx('Ausgabe Intervall','Output interval'),i.outInt,opts.outInt),sf(tx('Ausgabe Filter','Output filter'),i.outFilter,opts.filter),sf(tx('Diagnose Daten','Diagnostic data'),i.diag?1:0,opts.onoff)];form(tx('Ausgabe allgemein','General output'),fs,()=>{let oi=Number(fs[0].value),of=normFilter(oi,fs[1].value);fs[1].value=of;return api('/setup-general?'+q({i:oi,f:of,d:fs[2].value}))})}
function openRs(p){let r=S.iface.rs[p],fs=[sf(tx('Betriebsart','Mode'),r.mode,opts.rsMode),sf('Baud',r.baud,opts.baud),sf(tx('Ausgabe','Output'),r.output,opts.rsOut)];form('RS232-'+(p+1),fs,()=>api('/setup-rs'+(p+1)+'?'+q({m:fs[0].value,b:fs[1].value,o:fs[2].value})))}
function openUsb(){let f=sf('USB',S.iface.usb,opts.usb);form('USB',[f],()=>api('/setup-usb?'+q({v:f.value})))}
function openSd(){let i=S.iface,log=sf('Logging',i.sdLogging?1:0,opts.onoff),out=sf(tx('SD Ausgabe Intervall','SD output interval'),i.sdOutInt,opts.outInt),filter=sf(tx('SD Ausgabe Filter','SD output filter'),normFilter(i.sdOutInt,i.sdFilter),sdFilterOptions(i.sdOutInt)),write=sf(tx('SD Schreiben','SD write'),normSdWrite(i.sdOutInt,i.sdWrite),sdWriteOptions(i.sdOutInt)),diag=sf(tx('SD Diagnose Daten','SD diagnostic data'),i.sdDiag?1:0,opts.onoff),head=sf(tx('Kopfzeile','Header'),i.sdHeader?1:0,opts.onoff),io=opts.logIntegrity.map(x=>[x[0],x[1]]);if(!i.sdCertifiedReady){io[2]=[2,tx('CSV zertifiziert (gesperrt)','Certified CSV (locked)')];io[3]=[3,tx('TPLOG zertifiziert (gesperrt)','Certified TPLOG (locked)')];}let integ=sf(tx('Log-Integritaet','Log integrity'),Number(i.sdIntegrity)||0,io),fs=[log,out,filter,write,diag,head,integ];out.onChange=()=>{let oi=Number(out.value);filter.options=sdFilterOptions(oi);filter.value=normFilter(oi,filter.value);write.options=sdWriteOptions(oi);write.value=normSdWrite(oi,write.value)};form(tx('SD-Karte','SD card'),fs,()=>{let oi=Number(out.value),of=normFilter(oi,filter.value),ow=normSdWrite(oi,write.value);filter.value=of;write.value=ow;if(Number(integ.value)>=2&&!i.sdCertifiedReady)throw new Error(tx('Zertifizierte Logmodi sind noch gesperrt: Geraetezertifikat sowie signierte Geraete-, Kopf- und Systemkalibrierung sind erforderlich.','Certified log modes are still locked: device certificate plus signed device, head and system calibration are required.'));return api('/setup-sd?'+q({log:log.value,i:oi,f:of,w:ow,d:diag.value,h:head.value,g:integ.value}))})}
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
async function setPcTime(){let n=new Date(),u='/set-time?'+q({y:n.getFullYear(),mo:n.getMonth()+1,d:n.getDate(),h:n.getHours(),mi:n.getMinutes(),s:n.getSeconds(),tz:-n.getTimezoneOffset()});try{await api(u)}catch(e){msg(e.message||String(e),false)}mode=current.kind;render()}
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
window.addEventListener('pagehide',()=>{setupPageActive=false});window.addEventListener('pageshow',async e=>{setupClosing=false;setupPageActive=true;setupPollBusy=false;if(e.persisted){setupParts.fill(null);try{await loadSettings(false);if(current)render();msg('')}catch(x){msg(x.message||String(x),false)}}setupPoll()});
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
<div class="toolbar filter"><span id="filterLabel">Filter:</span><button id="filterAll" data-filter="all">Alle</button><button id="filterCsv" data-filter="csv" class="selected">Messdaten</button><button id="filterSerial" data-filter="serial">Seriellog</button><button id="filterEvidence" data-filter="evidence">Nachweise</button></div>
<div id="yearFilter" class="toolbar filter"><span id="yearLabel">Jahr:</span><span id="yearButtons"></span></div>
<div id="monthFilter" class="toolbar filter"><span id="monthLabel">Monat:</span><span id="monthButtons"></span></div>
<div id="notice" class="notice"><b>Messung, Regelung und Safety bleiben aktiv.</b> Während des Dateidownloads werden keine weiteren Webanfragen bearbeitet. SD- und Seriellogging haben weiterhin Vorrang.<br>Bei einer aktiven Datei lädt <b>Snapshot</b> im Modus Aus den bereits auf SD vorhandenen Stand. Im SHA-256- oder zertifizierten Modus wird der aktuelle Puffer geschrieben, der Abschnitt versiegelt und danach unveränderlich heruntergeladen.</div>
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
const applyLanguage=()=>{document.documentElement.lang=lang===1?'en':'de';document.title=tx('TP-3000 Log-Dateien','TP-3000 log files');E('pageTitle').textContent=tx('TP-3000 Log-Dateien','TP-3000 log files');E('homeLink').textContent=tx('Zur Hauptanzeige','Back to main display');E('reload').textContent=tx('Aktualisieren','Refresh');E('filterLabel').textContent=tx('Filter:','Filter:');E('filterAll').textContent=tx('Alle','All');E('filterCsv').textContent=tx('Messdaten','Measurement data');E('filterSerial').textContent=tx('Seriellog','Serial log');E('filterEvidence').textContent=tx('Nachweise','Evidence');E('yearLabel').textContent=tx('Jahr:','Year:');E('monthLabel').textContent=tx('Monat:','Month:');E('notice').innerHTML='<b>'+tx('Messung, Regelung und Safety bleiben aktiv.','Measurement, control and safety remain active.')+'</b> '+tx('Während des Dateidownloads werden keine weiteren Webanfragen bearbeitet. SD- und Seriellogging haben weiterhin Vorrang.','No further web requests are processed during a file download. SD and serial logging retain priority.')+'<br>'+tx('Bei einer aktiven Datei lädt','For an active file,')+' <b>Snapshot</b> '+tx('im Modus Aus den bereits auf SD vorhandenen Stand. Im SHA-256- oder zertifizierten Modus wird der aktuelle Puffer geschrieben, der Abschnitt versiegelt und danach unveränderlich heruntergeladen.','downloads the data already present on SD in Off mode. In SHA-256 or certified mode, the current buffer is written, the segment is sealed, and the immutable file is then downloaded.');E('thFile').textContent=tx('Datei','File');E('thModified').textContent=tx('Geändert','Modified');E('thStatus').textContent=tx('Status','Status');E('thSize').textContent=tx('Größe','Size');E('first').textContent=tx('Erste','First');E('prev').textContent=tx('Zurück','Previous');E('next').textContent=tx('Weiter','Next');E('last').textContent=tx('Letzte','Last');E('downloadFrame').title=tx('Download','Download')};
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


// Vorgeschaltete Auswahl- und Suchseite für aktuelle und historische
// Kalibrierzertifikate. Die eigentliche druckbare Zertifikatsansicht liegt
// bewusst auf /calibration-certificate-view.
static const char ethCalibrationCertificateIndexPage[] PROGMEM = R"TPCALINDEX(
<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TP-3000 Kalibrierzertifikate</title>
<style>
*{box-sizing:border-box}html,body{margin:0;background:#111;color:#eee;font-family:Arial,sans-serif}body{max-width:1120px;margin:24px auto;padding:0 16px}a{color:#8fe7ff}h1{font-size:30px;margin:18px 0 24px}h2{font-size:21px;margin:0 0 12px}.card{background:#1b1b1b;border:1px solid #444;border-radius:7px;padding:16px;margin:0 0 18px}.summary{display:grid;grid-template-columns:220px 1fr;gap:7px 12px;margin-bottom:14px}.label{color:#bbb}.btn,button{display:inline-block;background:#333;color:#fff;border:1px solid #777;border-radius:6px;padding:10px 15px;text-decoration:none;font:inherit;cursor:pointer}.btn:hover,button:hover{background:#444}.btn.pdf{margin-left:7px;background:#26384b}.btn.disabled{pointer-events:none;opacity:.45}.searchGrid{display:grid;grid-template-columns:160px minmax(180px,260px);gap:10px 12px;align-items:center}.searchGrid input{background:#0e0e0e;color:#fff;border:1px solid #777;border-radius:5px;padding:9px;font:inherit}.searchActions{margin-top:14px}.msg{margin:12px 0;color:#ddd}.ok{color:#7dff69}.warn{color:#ffd85a}.bad{color:#ff7777}.muted{color:#aaa}table{width:100%;border-collapse:collapse;background:#1b1b1b}th,td{padding:9px 8px;border-bottom:1px solid #444;text-align:left;vertical-align:middle}th{color:#bbb;font-weight:normal}td.actions{white-space:nowrap}.activeTag{display:inline-block;margin-left:7px;padding:2px 6px;border:1px solid #3c8f33;border-radius:4px;color:#7dff69;font-size:12px}.empty{padding:18px;text-align:center;color:#aaa}@media(max-width:760px){.summary,.searchGrid{grid-template-columns:1fr}table,thead,tbody,tr,th,td{display:block}thead{display:none}tr{border-bottom:1px solid #666;padding:8px}td{border:0;padding:4px 2px}td:before{content:attr(data-label) ': ';color:#aaa}td.actions:before{content:''}}
</style></head><body>
<p><a href="/">← Hauptanzeige</a> · <a href="/validity">Info / Gültigkeit</a></p>
<h1>Kalibrierzertifikate</h1>
<section class="card"><h2>Aktuelles Kalibrierzertifikat</h2><div id="activeSummary" class="summary"><div class="muted">Aktive Daten werden geladen …</div></div><div id="activeActions"></div></section>
<section class="card"><h2>Kalibrierzertifikate suchen</h2><div class="searchGrid"><label for="from">Zeitraum von</label><input id="from" type="date" required><label for="until">Zeitraum bis</label><input id="until" type="date" required></div><div class="searchActions"><button id="search" type="button">Suchen</button></div><div id="searchMsg" class="msg">Die Suche berücksichtigt jedes Zertifikat, dessen Gültigkeitszeitraum den eingegebenen Zeitraum überschneidet.</div></section>
<section><table><thead><tr><th>Kalibrierschein-Nr. / Zeichen</th><th>Typ / verwendeter Kopf</th><th>Gültig von</th><th>Gültig bis</th><th>Status</th><th></th></tr></thead><tbody id="results"><tr><td colspan="6" class="empty">Noch keine Suche ausgeführt.</td></tr></tbody></table></section>
<script>
const E=id=>document.getElementById(id),esc=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fmt=v=>{v=String(v||'');return v.length===8?v.slice(6,8)+'.'+v.slice(4,6)+'.'+v.slice(0,4):'—'};
const fmtUtc=v=>{v=Number(v)||0;if(v<=0)return'—';let d=new Date(v*1000),x=d.toISOString();return x.slice(8,10)+'.'+x.slice(5,7)+'.'+x.slice(0,4)+' '+x.slice(11,16)+' UTC'};
const reasonText=c=>({HEAD_VALUES_CHANGED:'Kopfjustierwerte geändert',DEVICE_VALUES_CHANGED:'Gerätejustierwerte geändert',HEAD_ADJUSTMENT_CHANGED:'Kopfjustierung geändert',DEVICE_ADJUSTMENT_CHANGED:'Gerätejustierung geändert',SYSTEM_CALIBRATION_REPLACED:'durch neue Systemkalibrierung ersetzt',EXTERNAL_CERTIFICATE_CHANGED:'externer Kalibrierschein geändert',CONFIGURATION_CHANGED:'Kalibrierkonfiguration geändert'})[String(c||'')]||String(c||'—');
const ymd=v=>String(v||'').replace(/-/g,'');
const numberOf=r=>r.externalCertificateBound?(r.externalCertificateNumber||r.calibrationId):r.calibrationId;
let selectedHeadType='—',selectedHeadSerial=0;
const pad5=v=>String(Number(v)||0).padStart(5,'0');
const selectedType=()=>selectedHeadType&&selectedHeadType!=='—'?'TP-3000 / '+selectedHeadType+' K'+pad5(selectedHeadSerial):'TP-3000 / kein Kopf ausgewählt';
const typeOf=r=>selectedType();
const applyHeadContext=d=>{selectedHeadType=String(d.selectedHeadType||'—');selectedHeadSerial=Number(d.selectedHeadSerial)||0};
function statusOf(r,current){if(!r.applicabilityStateKnown)return{c:'bad',t:'Statusdatei fehlerhaft'};if(r.applicabilityEnded)return{c:'warn',t:'Valide – bis '+fmtUtc(r.applicableUntilUtc)};if(!current||!r.validFromYmd||!r.validUntilYmd)return{c:'warn',t:r.active?'Aktiv – nicht beurteilbar':'Valide – nicht beurteilbar'};if(current<r.validFromYmd)return{c:'warn',t:(r.active?'Aktiv – ':'Valide – ')+'noch nicht gültig'};if(current>r.validUntilYmd)return{c:'bad',t:(r.active?'Aktiv – ':'Valide – ')+'abgelaufen'};if(!r.activeBindingValid)return{c:r.active?'bad':'warn',t:r.active?'Aktiv – Bindung nicht wirksam':'Valide – andere Justierung'};if(r.active&&r.firmwareApplicability==='FIRMWARE_CHANGED')return{c:'warn',t:'Aktiv – gültig; Firmwareabweichung dokumentiert'};if(r.active&&r.firmwareApplicability==='NOT_COMPARABLE')return{c:'warn',t:'Aktiv – Firmwarebezug nicht beurteilbar'};return{c:r.active?'ok':'warn',t:r.active?'Aktiv – gültig':'Valide – nicht aktiv'}}
function buttons(r,activeView=false){let id=encodeURIComponent(r.systemManifestSha256||''),view=activeView?'/calibration-certificate-view':'/calibration-certificate-view?id='+id,h='<a class="btn" href="'+view+'">Anzeigen</a>';if(r.externalPdfAvailable&&r.externalCertificateDocumentId)h+='<a class="btn pdf" target="_blank" rel="noopener" href="/calibration-external-pdf?id='+encodeURIComponent(r.externalCertificateDocumentId)+'&view=1">PDF</a>';return h}
function activeHtml(r,current){if(!r)return'<div class="bad">Kein aktuell gültiges Kalibrierzertifikat für <b>'+esc(selectedType())+'</b> vorhanden.</div>';let s=statusOf(r,current),a=r.applicabilityEnded?'<div class="label">Anwendbar bis</div><div>'+fmtUtc(r.applicableUntilUtc)+'</div><div class="label">Grund</div><div>'+esc(reasonText(r.applicabilityReason))+'</div>':'',f=r.firmwareApplicability==='FIRMWARE_CHANGED'?'<div class="label">Firmwarebezug</div><div class="warn"><b>Aktueller SHA-256 weicht vom Kalibrierstand ab; Kalibrierschein bleibt gültig</b></div>':(r.firmwareApplicability==='EXACT_MATCH'?'<div class="label">Firmwarebezug</div><div class="ok">Exakter Firmwarestand stimmt überein</div>':'<div class="label">Firmwarebezug</div><div class="warn">Nicht beurteilbar</div>');return'<div class="label">Kalibrierschein-Nr. / Zeichen</div><div><b>'+esc(numberOf(r)||'—')+'</b><span class="activeTag">Aktuell</span></div><div class="label">Typ / verwendeter Kopf</div><div>'+esc(typeOf(r))+'</div><div class="label">Gültig von</div><div>'+fmt(r.validFromYmd)+'</div><div class="label">Gültig bis</div><div>'+fmt(r.validUntilYmd)+'</div>'+a+f+'<div class="label">Status</div><div class="'+s.c+'"><b>'+esc(s.t)+'</b></div>'}
function rows(list,current){if(!list.length)return'<tr><td colspan="6" class="empty">Keine Kalibrierzertifikate im eingegebenen Zeitraum gefunden.</td></tr>';return list.map(r=>{let s=statusOf(r,current),tag=r.active?'<span class="activeTag">Aktuell</span>':'';return'<tr><td data-label="Kalibrierschein-Nr. / Zeichen"><b>'+esc(numberOf(r)||'—')+'</b>'+tag+'</td><td data-label="Typ / verwendeter Kopf">'+esc(typeOf(r))+'</td><td data-label="Gültig von">'+fmt(r.validFromYmd)+'</td><td data-label="Gültig bis">'+fmt(r.validUntilYmd)+'</td><td data-label="Status" class="'+s.c+'">'+esc(s.t)+'</td><td class="actions">'+buttons(r,false)+'</td></tr>'}).join('')}
async function api(url){let r=await fetch(url,{cache:'no-store'}),t=await r.text();if(!r.ok)throw new Error(t||('HTTP '+r.status));return JSON.parse(t)}
async function loadActive(){try{let d=await api('/calibration-certificates.json');applyHeadContext(d);E('activeSummary').innerHTML=activeHtml(d.active,d.currentYmd);E('activeActions').innerHTML=d.active?buttons(d.active,true):'';E('searchMsg').textContent='Die Suche ist auf '+selectedType()+' begrenzt und berücksichtigt zeitliche Überschneidungen.';if(d.active){let f=String(d.active.validFromYmd||''),u=String(d.active.validUntilYmd||'');if(f.length===8)E('from').value=f.slice(0,4)+'-'+f.slice(4,6)+'-'+f.slice(6,8);if(u.length===8)E('until').value=u.slice(0,4)+'-'+u.slice(4,6)+'-'+u.slice(6,8)}}catch(e){E('activeSummary').innerHTML='<div class="bad">'+esc(e.message||e)+'</div>'}}
async function search(){let f=ymd(E('from').value),u=ymd(E('until').value),m=E('searchMsg');if(f.length!==8||u.length!==8){m.className='msg bad';m.textContent='Bitte beide Datumsfelder ausfüllen.';return}if(f>u){m.className='msg bad';m.textContent='„Zeitraum von“ darf nicht nach „Zeitraum bis“ liegen.';return}m.className='msg';m.textContent='Kalibrierarchiv wird durchsucht …';E('search').disabled=true;try{let d=await api('/calibration-certificates.json?from='+f+'&until='+u);applyHeadContext(d);E('results').innerHTML=rows(d.results||[],d.currentYmd);m.className='msg '+(d.truncated?'warn':'ok');m.textContent=(d.total||0)+' Zertifikat(e) für '+selectedType()+' gefunden.'+(d.truncated?' Angezeigt werden die neuesten '+(d.results||[]).length+' Treffer.':'')}catch(e){m.className='msg bad';m.textContent=e.message||String(e)}finally{E('search').disabled=false}}
E('search').onclick=search;loadActive();
</script></body></html>
)TPCALINDEX";

// Druckbarer Kalibrierschein. Die QR-Bibliothek ist lokal in der Firmware
// eingebettet; es werden keinerlei Internet- oder CDN-Ressourcen benötigt.
static const char ethCalibrationCertificatePart0[] PROGMEM = R"TPCALSHEET0(
<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TP-3000 Kalibrierschein</title>
<style id="dynamicPrintStyle"></style>
<style>
*{box-sizing:border-box}html,body{margin:0;background:#d7d7d7;color:#111;font-family:Arial,sans-serif}body{padding:18px}.toolbar{max-width:1400px;margin:0 auto 12px;display:grid;grid-template-columns:minmax(0,1fr) max-content;gap:8px 12px;align-items:start}.toolbarActions{display:flex;gap:8px;align-items:center;flex-wrap:wrap;min-width:0}.toolbar a,.toolbar button{background:#303030;color:#fff;border:1px solid #666;border-radius:6px;padding:9px 14px;text-decoration:none;cursor:pointer;font:inherit;white-space:nowrap}.toolbar button:hover,.toolbar a:hover{background:#444}.toolbar .msg{grid-column:1/-1;color:#333;margin:0 6px}.toolbar .msg:empty{display:none}.langGroup{display:inline-flex;border:1px solid #666;border-radius:6px;overflow:hidden;justify-self:end}.toolbar .langButton{border:0;border-radius:0;border-right:1px solid #666}.toolbar .langButton:last-child{border-right:0}.toolbar .langButton.selected{background:#006c91;color:#fff;font-weight:bold}@media(max-width:900px){.toolbar{grid-template-columns:1fr}.langGroup{grid-column:1;grid-row:1;justify-self:start}.toolbarActions{grid-column:1;grid-row:2}.toolbar .msg{grid-column:1;grid-row:3}}.pdfLink{display:inline-block;background:#26384b;color:#fff;border:1px solid #777;border-radius:5px;padding:6px 9px;text-decoration:none}.sheet{width:210mm;min-height:297mm;margin:0 auto;background:#fff;padding:16mm 16mm 14mm;box-shadow:0 2px 18px #777}.head{display:grid;grid-template-columns:minmax(0,1fr) max-content;align-items:start;gap:20px;border-bottom:2px solid #111;padding-bottom:10px}.head h1{margin:0;font-size:25px}.head .brand{text-align:right;font-weight:bold;white-space:nowrap}.head #docId{display:block;white-space:nowrap}.subtitle{margin:5px 0 0;color:#444}.status{margin:14px 0;padding:8px 10px;border:1px solid #999;background:#f5f5f5}.ok{color:#087400;font-weight:bold}.warn{color:#9a5800;font-weight:bold}.bad{color:#a00000;font-weight:bold}h2{font-size:17px;margin:20px 0 8px;border-bottom:1px solid #777;padding-bottom:4px}h3{font-size:14px;margin:14px 0 6px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}.topic{break-inside:avoid-page;page-break-inside:avoid}.card{break-inside:avoid-page;page-break-inside:avoid}.rows{width:100%;border-collapse:collapse;font-size:12px}.rows td{padding:4px 5px;border-bottom:1px solid #ddd;vertical-align:top}.rows td:first-child{width:43%;color:#555}.mono{font-family:"Courier New",monospace;word-break:break-all}.long{white-space:pre-wrap;line-height:1.35}.professionalGrid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:10px 14px;align-items:start}.professional .field{min-width:0;margin:0}.professional .field.full{grid-column:1/-1}.professional .label{font-weight:bold;font-size:12px;break-after:avoid-page}.professional .value{font-size:12px;white-space:pre-wrap;overflow-wrap:anywhere;word-break:break-word;border-left:3px solid #ddd;padding:4px 7px;margin-top:2px}.qrSection{break-inside:avoid}.qrArea{display:flex;gap:16px;align-items:flex-start;flex-wrap:wrap;break-inside:avoid}.qrCard{flex:1 1 280px;text-align:center;break-inside:avoid}.qrCard svg{width:min(100%,372px);height:auto;border:1px solid #999;background:#fff}.qrCaption{font-size:11px;margin-top:5px;color:#444;white-space:nowrap}.qrArea.qrTwo{display:grid;grid-template-columns:1fr;justify-items:center;gap:12px}.qrArea.qrTwo .qrCard{width:100%;max-width:460px}.foot{margin-top:16px;padding-top:8px;border-top:1px solid #777;font-size:10px;color:#444;line-height:1.35}.missing{padding:18px;border:1px solid #b00;background:#fff1f1}.calibrationResults{break-inside:avoid-page;page-break-inside:avoid}.calResult{margin-bottom:14px;break-inside:avoid-page;page-break-inside:avoid}.calObject{font-weight:bold;margin:8px 0}.calTable{width:100%;border-collapse:collapse;font-size:10px;break-inside:avoid}.calTable th,.calTable td{border:1px solid #aaa;padding:3px 4px;text-align:right;white-space:nowrap}.calTable th:nth-child(2),.calTable td:nth-child(2){text-align:left}.calTable th{background:#eee}.formula{font-size:11px;color:#444}.qrCount{flex:1 0 100%;font-size:12px;margin-bottom:4px}.pageBreak{break-before:page}.hidden{display:none!important}.superseded{text-decoration-line:line-through;text-decoration-thickness:1px;text-decoration-color:#111}.applicabilityNote{color:#9a5800;font-weight:bold}
@media screen and (max-width:900px){body{padding:6px}.sheet{width:100%;min-height:0;padding:18px}.grid,.professionalGrid{grid-template-columns:1fr}.head{grid-template-columns:1fr}.head .brand{text-align:left;margin-top:10px;white-space:normal}.head #docId{white-space:normal;overflow-wrap:anywhere}}
@media print{html,body{width:100%;margin:0;background:#fff}body{padding:0}.toolbar{display:none!important}.sheet{width:auto;min-height:0;margin:0;padding:0;box-shadow:none}.topic,.card,.calibrationResults{break-inside:avoid-page;page-break-inside:avoid}.grid,.professionalGrid{grid-template-columns:minmax(0,1fr) minmax(0,1fr)!important;gap:8mm}.professionalGrid{column-gap:7mm;row-gap:3mm}.professional .field{break-inside:avoid-page;page-break-inside:avoid}.professional .field.full{grid-column:1/-1}.head{grid-template-columns:minmax(0,1fr) max-content;gap:7mm;padding-bottom:2.2mm}.head h1{font-size:22px;line-height:1.05}.head .brand{font-size:10.5px;line-height:1.15;white-space:nowrap}.head #docId{font-size:10.5px;white-space:nowrap}.subtitle{margin-top:1.2mm}.status{margin:2.5mm 0 2mm;padding:1.4mm 2mm;font-size:9.6px;line-height:1.15}h2{margin-top:9px;margin-bottom:5px;padding-bottom:2px}h2,h3{break-after:avoid-page;page-break-after:avoid}.rows{font-size:9.8px}.rows td{padding:2.4px 4px}.issuerSection h2{margin-top:7px}.issuerSection .rows td{padding:2px 4px}.card>.foot{margin-top:2.2mm;padding-top:1.5mm;font-size:8.4px;line-height:1.15}.qrCard svg{width:112mm;max-width:112mm}.qrSection{break-before:page;page-break-before:always;break-inside:avoid-page;page-break-inside:avoid}.qrSection.qrTwoSection{break-before:page;page-break-before:always;break-inside:avoid-page;page-break-inside:avoid}.qrSection.qrTwoSection>h2{font-size:15px;margin:0 0 .7mm;padding-bottom:.7mm}.qrArea.qrTwo{display:grid;grid-template-columns:1fr;justify-items:center;gap:.25mm}.qrArea.qrTwo .qrCard{width:100%;max-width:none}.qrArea.qrTwo .qrCard h3{font-size:10.5px;line-height:1;margin:.25mm 0 .25mm}.qrArea.qrTwo .qrCaption{font-size:7.2px;line-height:1.05;margin-top:.3mm}.qrArea.qrTwo .qrCount{font-size:9.4px;line-height:1.05;margin:0 0 .35mm}.qrSection .qrFoot{margin-top:1mm;padding-top:1mm;font-size:7.6px;line-height:1.08;break-inside:avoid;page-break-inside:avoid}.applicabilityNote{color:#111!important}.noPrint{display:none!important}}
</style></head><body>
<div class="toolbar"><div class="toolbarActions"><a id="certificateSelectionLink" href="/calibration-certificate">← Zertifikatsauswahl</a><a id="calibrationManageLink" href="/calibration">Justierung / Kalibrierung verwalten</a><button id="printCert" type="button" onclick="window.print()" disabled>Drucken / PDF</button><button id="downloadQr" type="button" disabled>Vollständige QR-Daten speichern (TP3Q3 .tpqr)</button></div><span class="langGroup" aria-label="Language"><button id="langDe" class="langButton selected" type="button">Deutsch</button><button id="langEn" class="langButton" type="button">English</button></span><span id="toolbarMsg" class="msg">Kalibrierdaten werden geladen …</span></div>
<main id="sheet" class="sheet"><div class="head"><div><h1 id="certificateTitle">TP-3000 Kalibrierschein</h1><div id="certificateSubtitle" class="subtitle">Taupunktspiegel-Hygrometer</div></div><div class="brand">TP-3000<br><span id="docId">—</span></div></div><div id="content"><p>Kalibrierdaten werden geladen …</p></div></main>
<script>
)TPCALSHEET0";

static const char ethCalibrationQrLibrary[] PROGMEM = R"TPCALQRLIB(
/* QRCode for JavaScript, Copyright (c) 2009 Kazuhiko Arase, MIT License.
   Adapted for TP-3000 browser use; QR Code is a registered trademark of DENSO WAVE. */
(function(){
"use strict";
var QRMode = {
    MODE_NUMBER :       1 << 0,
    MODE_ALPHA_NUM :    1 << 1,
    MODE_8BIT_BYTE :    1 << 2,
    MODE_KANJI :        1 << 3
};
var QRErrorCorrectLevel = {
	L : 1,
	M : 0,
	Q : 3,
	H : 2
};
var QRMaskPattern = {
	PATTERN000 : 0,
	PATTERN001 : 1,
	PATTERN010 : 2,
	PATTERN011 : 3,
	PATTERN100 : 4,
	PATTERN101 : 5,
	PATTERN110 : 6,
	PATTERN111 : 7
};
var QRMath = {
	glog : function(n) {
		if (n < 1) {
			throw new Error("glog(" + n + ")");
		}
		return QRMath.LOG_TABLE[n];
	},
	gexp : function(n) {
		while (n < 0) {
			n += 255;
		}
		while (n >= 256) {
			n -= 255;
		}
		return QRMath.EXP_TABLE[n];
	},
	EXP_TABLE : new Array(256),
	LOG_TABLE : new Array(256)
};
for (var i = 0; i < 8; i++) {
	QRMath.EXP_TABLE[i] = 1 << i;
}
for (var i = 8; i < 256; i++) {
	QRMath.EXP_TABLE[i] = QRMath.EXP_TABLE[i - 4]
		^ QRMath.EXP_TABLE[i - 5]
		^ QRMath.EXP_TABLE[i - 6]
		^ QRMath.EXP_TABLE[i - 8];
}
for (var i = 0; i < 255; i++) {
	QRMath.LOG_TABLE[QRMath.EXP_TABLE[i] ] = i;
}
function QRPolynomial(num, shift) {
	if (num.length === undefined) {
		throw new Error(num.length + "/" + shift);
	}
	var offset = 0;
	while (offset < num.length && num[offset] === 0) {
		offset++;
	}
	this.num = new Array(num.length - offset + shift);
	for (var i = 0; i < num.length - offset; i++) {
		this.num[i] = num[i + offset];
	}
}
QRPolynomial.prototype = {
	get : function(index) {
		return this.num[index];
	},
	getLength : function() {
		return this.num.length;
	},
	multiply : function(e) {
		var num = new Array(this.getLength() + e.getLength() - 1);
		for (var i = 0; i < this.getLength(); i++) {
			for (var j = 0; j < e.getLength(); j++) {
				num[i + j] ^= QRMath.gexp(QRMath.glog(this.get(i) ) + QRMath.glog(e.get(j) ) );
			}
		}
		return new QRPolynomial(num, 0);
	},
	mod : function(e) {
		if (this.getLength() - e.getLength() < 0) {
			return this;
		}
		var ratio = QRMath.glog(this.get(0) ) - QRMath.glog(e.get(0) );
		var num = new Array(this.getLength() );
		for (var i = 0; i < this.getLength(); i++) {
			num[i] = this.get(i);
		}
		for (var x = 0; x < e.getLength(); x++) {
			num[x] ^= QRMath.gexp(QRMath.glog(e.get(x) ) + ratio);
		}
		return new QRPolynomial(num, 0).mod(e);
	}
};
function QRRSBlock(totalCount, dataCount) {
	this.totalCount = totalCount;
	this.dataCount  = dataCount;
}
QRRSBlock.RS_BLOCK_TABLE = [
	[1, 26, 19],
	[1, 26, 16],
	[1, 26, 13],
	[1, 26, 9],
	[1, 44, 34],
	[1, 44, 28],
	[1, 44, 22],
	[1, 44, 16],
	[1, 70, 55],
	[1, 70, 44],
	[2, 35, 17],
	[2, 35, 13],
	[1, 100, 80],
	[2, 50, 32],
	[2, 50, 24],
	[4, 25, 9],
	[1, 134, 108],
	[2, 67, 43],
	[2, 33, 15, 2, 34, 16],
	[2, 33, 11, 2, 34, 12],
	[2, 86, 68],
	[4, 43, 27],
	[4, 43, 19],
	[4, 43, 15],
	[2, 98, 78],
	[4, 49, 31],
	[2, 32, 14, 4, 33, 15],
	[4, 39, 13, 1, 40, 14],
	[2, 121, 97],
	[2, 60, 38, 2, 61, 39],
	[4, 40, 18, 2, 41, 19],
	[4, 40, 14, 2, 41, 15],
	[2, 146, 116],
	[3, 58, 36, 2, 59, 37],
	[4, 36, 16, 4, 37, 17],
	[4, 36, 12, 4, 37, 13],
	[2, 86, 68, 2, 87, 69],
	[4, 69, 43, 1, 70, 44],
	[6, 43, 19, 2, 44, 20],
	[6, 43, 15, 2, 44, 16],
	[4, 101, 81],
	[1, 80, 50, 4, 81, 51],
	[4, 50, 22, 4, 51, 23],
	[3, 36, 12, 8, 37, 13],
	[2, 116, 92, 2, 117, 93],
	[6, 58, 36, 2, 59, 37],
	[4, 46, 20, 6, 47, 21],
	[7, 42, 14, 4, 43, 15],
	[4, 133, 107],
	[8, 59, 37, 1, 60, 38],
	[8, 44, 20, 4, 45, 21],
	[12, 33, 11, 4, 34, 12],
	[3, 145, 115, 1, 146, 116],
	[4, 64, 40, 5, 65, 41],
	[11, 36, 16, 5, 37, 17],
	[11, 36, 12, 5, 37, 13],
	[5, 109, 87, 1, 110, 88],
	[5, 65, 41, 5, 66, 42],
	[5, 54, 24, 7, 55, 25],
	[11, 36, 12],
	[5, 122, 98, 1, 123, 99],
	[7, 73, 45, 3, 74, 46],
	[15, 43, 19, 2, 44, 20],
	[3, 45, 15, 13, 46, 16],
	[1, 135, 107, 5, 136, 108],
	[10, 74, 46, 1, 75, 47],
	[1, 50, 22, 15, 51, 23],
	[2, 42, 14, 17, 43, 15],
	[5, 150, 120, 1, 151, 121],
	[9, 69, 43, 4, 70, 44],
	[17, 50, 22, 1, 51, 23],
	[2, 42, 14, 19, 43, 15],
	[3, 141, 113, 4, 142, 114],
	[3, 70, 44, 11, 71, 45],
	[17, 47, 21, 4, 48, 22],
	[9, 39, 13, 16, 40, 14],
	[3, 135, 107, 5, 136, 108],
	[3, 67, 41, 13, 68, 42],
	[15, 54, 24, 5, 55, 25],
	[15, 43, 15, 10, 44, 16],
	[4, 144, 116, 4, 145, 117],
	[17, 68, 42],
	[17, 50, 22, 6, 51, 23],
	[19, 46, 16, 6, 47, 17],
	[2, 139, 111, 7, 140, 112],
	[17, 74, 46],
	[7, 54, 24, 16, 55, 25],
	[34, 37, 13],
	[4, 151, 121, 5, 152, 122],
	[4, 75, 47, 14, 76, 48],
	[11, 54, 24, 14, 55, 25],
	[16, 45, 15, 14, 46, 16],
	[6, 147, 117, 4, 148, 118],
	[6, 73, 45, 14, 74, 46],
	[11, 54, 24, 16, 55, 25],
	[30, 46, 16, 2, 47, 17],
	[8, 132, 106, 4, 133, 107],
	[8, 75, 47, 13, 76, 48],
	[7, 54, 24, 22, 55, 25],
	[22, 45, 15, 13, 46, 16],
	[10, 142, 114, 2, 143, 115],
	[19, 74, 46, 4, 75, 47],
	[28, 50, 22, 6, 51, 23],
	[33, 46, 16, 4, 47, 17],
	[8, 152, 122, 4, 153, 123],
	[22, 73, 45, 3, 74, 46],
	[8, 53, 23, 26, 54, 24],
	[12, 45, 15, 28, 46, 16],
	[3, 147, 117, 10, 148, 118],
	[3, 73, 45, 23, 74, 46],
	[4, 54, 24, 31, 55, 25],
	[11, 45, 15, 31, 46, 16],
	[7, 146, 116, 7, 147, 117],
	[21, 73, 45, 7, 74, 46],
	[1, 53, 23, 37, 54, 24],
	[19, 45, 15, 26, 46, 16],
	[5, 145, 115, 10, 146, 116],
	[19, 75, 47, 10, 76, 48],
	[15, 54, 24, 25, 55, 25],
	[23, 45, 15, 25, 46, 16],
	[13, 145, 115, 3, 146, 116],
	[2, 74, 46, 29, 75, 47],
	[42, 54, 24, 1, 55, 25],
	[23, 45, 15, 28, 46, 16],
	[17, 145, 115],
	[10, 74, 46, 23, 75, 47],
	[10, 54, 24, 35, 55, 25],
	[19, 45, 15, 35, 46, 16],
	[17, 145, 115, 1, 146, 116],
	[14, 74, 46, 21, 75, 47],
	[29, 54, 24, 19, 55, 25],
	[11, 45, 15, 46, 46, 16],
	[13, 145, 115, 6, 146, 116],
	[14, 74, 46, 23, 75, 47],
	[44, 54, 24, 7, 55, 25],
	[59, 46, 16, 1, 47, 17],
	[12, 151, 121, 7, 152, 122],
	[12, 75, 47, 26, 76, 48],
	[39, 54, 24, 14, 55, 25],
	[22, 45, 15, 41, 46, 16],
	[6, 151, 121, 14, 152, 122],
	[6, 75, 47, 34, 76, 48],
	[46, 54, 24, 10, 55, 25],
	[2, 45, 15, 64, 46, 16],
	[17, 152, 122, 4, 153, 123],
	[29, 74, 46, 14, 75, 47],
	[49, 54, 24, 10, 55, 25],
	[24, 45, 15, 46, 46, 16],
	[4, 152, 122, 18, 153, 123],
	[13, 74, 46, 32, 75, 47],
	[48, 54, 24, 14, 55, 25],
	[42, 45, 15, 32, 46, 16],
	[20, 147, 117, 4, 148, 118],
	[40, 75, 47, 7, 76, 48],
	[43, 54, 24, 22, 55, 25],
	[10, 45, 15, 67, 46, 16],
	[19, 148, 118, 6, 149, 119],
	[18, 75, 47, 31, 76, 48],
	[34, 54, 24, 34, 55, 25],
	[20, 45, 15, 61, 46, 16]
];
QRRSBlock.getRSBlocks = function(typeNumber, errorCorrectLevel) {
	var rsBlock = QRRSBlock.getRsBlockTable(typeNumber, errorCorrectLevel);
	if (rsBlock === undefined) {
		throw new Error("bad rs block @ typeNumber:" + typeNumber + "/errorCorrectLevel:" + errorCorrectLevel);
	}
	var length = rsBlock.length / 3;
	var list = [];
	for (var i = 0; i < length; i++) {
		var count = rsBlock[i * 3 + 0];
		var totalCount = rsBlock[i * 3 + 1];
		var dataCount  = rsBlock[i * 3 + 2];
		for (var j = 0; j < count; j++) {
			list.push(new QRRSBlock(totalCount, dataCount) );	
		}
	}
	return list;
};
QRRSBlock.getRsBlockTable = function(typeNumber, errorCorrectLevel) {
	switch(errorCorrectLevel) {
	case QRErrorCorrectLevel.L :
		return QRRSBlock.RS_BLOCK_TABLE[(typeNumber - 1) * 4 + 0];
	case QRErrorCorrectLevel.M :
		return QRRSBlock.RS_BLOCK_TABLE[(typeNumber - 1) * 4 + 1];
	case QRErrorCorrectLevel.Q :
		return QRRSBlock.RS_BLOCK_TABLE[(typeNumber - 1) * 4 + 2];
	case QRErrorCorrectLevel.H :
		return QRRSBlock.RS_BLOCK_TABLE[(typeNumber - 1) * 4 + 3];
	default :
		return undefined;
	}
};
function QRBitBuffer() {
	this.buffer = [];
	this.length = 0;
}
QRBitBuffer.prototype = {
	get : function(index) {
		var bufIndex = Math.floor(index / 8);
		return ( (this.buffer[bufIndex] >>> (7 - index % 8) ) & 1) == 1;
	},
	put : function(num, length) {
		for (var i = 0; i < length; i++) {
			this.putBit( ( (num >>> (length - i - 1) ) & 1) == 1);
		}
	},
	getLengthInBits : function() {
		return this.length;
	},
	putBit : function(bit) {
		var bufIndex = Math.floor(this.length / 8);
		if (this.buffer.length <= bufIndex) {
			this.buffer.push(0);
		}
		if (bit) {
			this.buffer[bufIndex] |= (0x80 >>> (this.length % 8) );
		}
		this.length++;
	}
};
function QR8bitByte(data) {
	this.mode = QRMode.MODE_8BIT_BYTE;
	this.data = data;
}
QR8bitByte.prototype = {
	getLength : function() {
		return this.data.length;
	},
	write : function(buffer) {
		for (var i = 0; i < this.data.length; i++) {
			buffer.put(this.data.charCodeAt(i), 8);
		}
	}
};
var QRUtil = {
    PATTERN_POSITION_TABLE : [
        [],
        [6, 18],
        [6, 22],
        [6, 26],
        [6, 30],
        [6, 34],
        [6, 22, 38],
        [6, 24, 42],
        [6, 26, 46],
        [6, 28, 50],
        [6, 30, 54],        
        [6, 32, 58],
        [6, 34, 62],
        [6, 26, 46, 66],
        [6, 26, 48, 70],
        [6, 26, 50, 74],
        [6, 30, 54, 78],
        [6, 30, 56, 82],
        [6, 30, 58, 86],
        [6, 34, 62, 90],
        [6, 28, 50, 72, 94],
        [6, 26, 50, 74, 98],
        [6, 30, 54, 78, 102],
        [6, 28, 54, 80, 106],
        [6, 32, 58, 84, 110],
        [6, 30, 58, 86, 114],
        [6, 34, 62, 90, 118],
        [6, 26, 50, 74, 98, 122],
        [6, 30, 54, 78, 102, 126],
        [6, 26, 52, 78, 104, 130],
        [6, 30, 56, 82, 108, 134],
        [6, 34, 60, 86, 112, 138],
        [6, 30, 58, 86, 114, 142],
        [6, 34, 62, 90, 118, 146],
        [6, 30, 54, 78, 102, 126, 150],
        [6, 24, 50, 76, 102, 128, 154],
        [6, 28, 54, 80, 106, 132, 158],
        [6, 32, 58, 84, 110, 136, 162],
        [6, 26, 54, 82, 110, 138, 166],
        [6, 30, 58, 86, 114, 142, 170]
    ],
    G15 : (1 << 10) | (1 << 8) | (1 << 5) | (1 << 4) | (1 << 2) | (1 << 1) | (1 << 0),
    G18 : (1 << 12) | (1 << 11) | (1 << 10) | (1 << 9) | (1 << 8) | (1 << 5) | (1 << 2) | (1 << 0),
    G15_MASK : (1 << 14) | (1 << 12) | (1 << 10)    | (1 << 4) | (1 << 1),
    getBCHTypeInfo : function(data) {
        var d = data << 10;
        while (QRUtil.getBCHDigit(d) - QRUtil.getBCHDigit(QRUtil.G15) >= 0) {
            d ^= (QRUtil.G15 << (QRUtil.getBCHDigit(d) - QRUtil.getBCHDigit(QRUtil.G15) ) );    
        }
        return ( (data << 10) | d) ^ QRUtil.G15_MASK;
    },
    getBCHTypeNumber : function(data) {
        var d = data << 12;
        while (QRUtil.getBCHDigit(d) - QRUtil.getBCHDigit(QRUtil.G18) >= 0) {
            d ^= (QRUtil.G18 << (QRUtil.getBCHDigit(d) - QRUtil.getBCHDigit(QRUtil.G18) ) );    
        }
        return (data << 12) | d;
    },
    getBCHDigit : function(data) {
        var digit = 0;
        while (data !== 0) {
            digit++;
            data >>>= 1;
        }
        return digit;
    },
    getPatternPosition : function(typeNumber) {
        return QRUtil.PATTERN_POSITION_TABLE[typeNumber - 1];
    },
    getMask : function(maskPattern, i, j) {
        switch (maskPattern) {
        case QRMaskPattern.PATTERN000 : return (i + j) % 2 === 0;
        case QRMaskPattern.PATTERN001 : return i % 2 === 0;
        case QRMaskPattern.PATTERN010 : return j % 3 === 0;
        case QRMaskPattern.PATTERN011 : return (i + j) % 3 === 0;
        case QRMaskPattern.PATTERN100 : return (Math.floor(i / 2) + Math.floor(j / 3) ) % 2 === 0;
        case QRMaskPattern.PATTERN101 : return (i * j) % 2 + (i * j) % 3 === 0;
        case QRMaskPattern.PATTERN110 : return ( (i * j) % 2 + (i * j) % 3) % 2 === 0;
        case QRMaskPattern.PATTERN111 : return ( (i * j) % 3 + (i + j) % 2) % 2 === 0;
        default :
            throw new Error("bad maskPattern:" + maskPattern);
        }
    },
    getErrorCorrectPolynomial : function(errorCorrectLength) {
        var a = new QRPolynomial([1], 0);
        for (var i = 0; i < errorCorrectLength; i++) {
            a = a.multiply(new QRPolynomial([1, QRMath.gexp(i)], 0) );
        }
        return a;
    },
    getLengthInBits : function(mode, type) {
        if (1 <= type && type < 10) {
            switch(mode) {
            case QRMode.MODE_NUMBER     : return 10;
            case QRMode.MODE_ALPHA_NUM  : return 9;
            case QRMode.MODE_8BIT_BYTE  : return 8;
            case QRMode.MODE_KANJI      : return 8;
            default :
                throw new Error("mode:" + mode);
            }
        } else if (type < 27) {
            switch(mode) {
            case QRMode.MODE_NUMBER     : return 12;
            case QRMode.MODE_ALPHA_NUM  : return 11;
            case QRMode.MODE_8BIT_BYTE  : return 16;
            case QRMode.MODE_KANJI      : return 10;
            default :
                throw new Error("mode:" + mode);
            }
        } else if (type < 41) {
            switch(mode) {
            case QRMode.MODE_NUMBER     : return 14;
            case QRMode.MODE_ALPHA_NUM  : return 13;
            case QRMode.MODE_8BIT_BYTE  : return 16;
            case QRMode.MODE_KANJI      : return 12;
            default :
                throw new Error("mode:" + mode);
            }
        } else {
            throw new Error("type:" + type);
        }
    },
    getLostPoint : function(qrCode) {
        var moduleCount = qrCode.getModuleCount();
        var lostPoint = 0;
        var row = 0; 
        var col = 0;
        for (row = 0; row < moduleCount; row++) {
            for (col = 0; col < moduleCount; col++) {
                var sameCount = 0;
                var dark = qrCode.isDark(row, col);
                for (var r = -1; r <= 1; r++) {
                    if (row + r < 0 || moduleCount <= row + r) {
                        continue;
                    }
                    for (var c = -1; c <= 1; c++) {
                        if (col + c < 0 || moduleCount <= col + c) {
                            continue;
                        }
                        if (r === 0 && c === 0) {
                            continue;
                        }
                        if (dark === qrCode.isDark(row + r, col + c) ) {
                            sameCount++;
                        }
                    }
                }
                if (sameCount > 5) {
                    lostPoint += (3 + sameCount - 5);
                }
            }
        }
        for (row = 0; row < moduleCount - 1; row++) {
            for (col = 0; col < moduleCount - 1; col++) {
                var count = 0;
                if (qrCode.isDark(row,     col    ) ) count++;
                if (qrCode.isDark(row + 1, col    ) ) count++;
                if (qrCode.isDark(row,     col + 1) ) count++;
                if (qrCode.isDark(row + 1, col + 1) ) count++;
                if (count === 0 || count === 4) {
                    lostPoint += 3;
                }
            }
        }
        for (row = 0; row < moduleCount; row++) {
            for (col = 0; col < moduleCount - 6; col++) {
                if (qrCode.isDark(row, col) && 
                        !qrCode.isDark(row, col + 1) && 
                         qrCode.isDark(row, col + 2) && 
                         qrCode.isDark(row, col + 3) && 
                         qrCode.isDark(row, col + 4) && 
                        !qrCode.isDark(row, col + 5) && 
                         qrCode.isDark(row, col + 6) ) {
                    lostPoint += 40;
                }
            }
        }
        for (col = 0; col < moduleCount; col++) {
            for (row = 0; row < moduleCount - 6; row++) {
                if (qrCode.isDark(row, col) &&
                        !qrCode.isDark(row + 1, col) &&
                         qrCode.isDark(row + 2, col) &&
                         qrCode.isDark(row + 3, col) &&
                         qrCode.isDark(row + 4, col) &&
                        !qrCode.isDark(row + 5, col) &&
                         qrCode.isDark(row + 6, col) ) {
                    lostPoint += 40;
                }
            }
        }
        var darkCount = 0;
        for (col = 0; col < moduleCount; col++) {
            for (row = 0; row < moduleCount; row++) {
                if (qrCode.isDark(row, col) ) {
                    darkCount++;
                }
            }
        }
        var ratio = Math.abs(100 * darkCount / moduleCount / moduleCount - 50) / 5;
        lostPoint += ratio * 10;
        return lostPoint;       
    }
};
function QRAlphaNum(data){this.mode=QRMode.MODE_ALPHA_NUM;this.data=data;}
QRAlphaNum.ALPHABET="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
QRAlphaNum.prototype={
 getLength:function(){return this.data.length;},
 write:function(buffer){var i=0;while(i+1<this.data.length){var a=QRAlphaNum.ALPHABET.indexOf(this.data.charAt(i));var b=QRAlphaNum.ALPHABET.indexOf(this.data.charAt(i+1));if(a<0||b<0)throw new Error("invalid alphanumeric QR data");buffer.put(a*45+b,11);i+=2;}if(i<this.data.length){var c=QRAlphaNum.ALPHABET.indexOf(this.data.charAt(i));if(c<0)throw new Error("invalid alphanumeric QR data");buffer.put(c,6);}}
};
function QRCode(typeNumber, errorCorrectLevel) {
	this.typeNumber = typeNumber;
	this.errorCorrectLevel = errorCorrectLevel;
	this.modules = null;
	this.moduleCount = 0;
	this.dataCache = null;
	this.dataList = [];
}
QRCode.prototype = {
	addData : function(data, mode) {
		var newData;
		if (mode === 'byte') newData = new QR8bitByte(data);
		else if (mode === 'alphanumeric') newData = new QRAlphaNum(data);
		else newData = /^[0-9A-Z $%*+\-./:]*$/.test(data) ? new QRAlphaNum(data) : new QR8bitByte(data);
		this.dataList.push(newData);
		this.dataCache = null;
	},
	isDark : function(row, col) {
		if (row < 0 || this.moduleCount <= row || col < 0 || this.moduleCount <= col) {
			throw new Error(row + "," + col);
		}
		return this.modules[row][col];
	},
	getModuleCount : function() {
		return this.moduleCount;
	},
	make : function() {
		if (this.typeNumber < 1 ){
			var typeNumber = 1;
			for (typeNumber = 1; typeNumber < 40; typeNumber++) {
				var rsBlocks = QRRSBlock.getRSBlocks(typeNumber, this.errorCorrectLevel);
				var buffer = new QRBitBuffer();
				var totalDataCount = 0;
				for (var i = 0; i < rsBlocks.length; i++) {
					totalDataCount += rsBlocks[i].dataCount;
				}
				for (var x = 0; x < this.dataList.length; x++) {
					var data = this.dataList[x];
					buffer.put(data.mode, 4);
					buffer.put(data.getLength(), QRUtil.getLengthInBits(data.mode, typeNumber) );
					data.write(buffer);
				}
				if (buffer.getLengthInBits() <= totalDataCount * 8)
					break;
			}
			this.typeNumber = typeNumber;
		}
		this.makeImpl(false, this.getBestMaskPattern() );
	},
	makeImpl : function(test, maskPattern) {
		this.moduleCount = this.typeNumber * 4 + 17;
		this.modules = new Array(this.moduleCount);
		for (var row = 0; row < this.moduleCount; row++) {
			this.modules[row] = new Array(this.moduleCount);
			for (var col = 0; col < this.moduleCount; col++) {
				this.modules[row][col] = null;
			}
		}
		this.setupPositionProbePattern(0, 0);
		this.setupPositionProbePattern(this.moduleCount - 7, 0);
		this.setupPositionProbePattern(0, this.moduleCount - 7);
		this.setupPositionAdjustPattern();
		this.setupTimingPattern();
		this.setupTypeInfo(test, maskPattern);
		if (this.typeNumber >= 7) {
			this.setupTypeNumber(test);
		}
		if (this.dataCache === null) {
			this.dataCache = QRCode.createData(this.typeNumber, this.errorCorrectLevel, this.dataList);
		}
		this.mapData(this.dataCache, maskPattern);
	},
	setupPositionProbePattern : function(row, col)  {
		for (var r = -1; r <= 7; r++) {
			if (row + r <= -1 || this.moduleCount <= row + r) continue;
			for (var c = -1; c <= 7; c++) {
				if (col + c <= -1 || this.moduleCount <= col + c) continue;
				if ( (0 <= r && r <= 6 && (c === 0 || c === 6) ) || 
                     (0 <= c && c <= 6 && (r === 0 || r === 6) ) || 
                     (2 <= r && r <= 4 && 2 <= c && c <= 4) ) {
					this.modules[row + r][col + c] = true;
				} else {
					this.modules[row + r][col + c] = false;
				}
			}		
		}		
	},
	getBestMaskPattern : function() {
		var minLostPoint = 0;
		var pattern = 0;
		for (var i = 0; i < 8; i++) {
			this.makeImpl(true, i);
			var lostPoint = QRUtil.getLostPoint(this);
			if (i === 0 || minLostPoint >  lostPoint) {
				minLostPoint = lostPoint;
				pattern = i;
			}
		}
		return pattern;
	},
	createMovieClip : function(target_mc, instance_name, depth) {
		var qr_mc = target_mc.createEmptyMovieClip(instance_name, depth);
		var cs = 1;
		this.make();
		for (var row = 0; row < this.modules.length; row++) {
			var y = row * cs;
			for (var col = 0; col < this.modules[row].length; col++) {
				var x = col * cs;
				var dark = this.modules[row][col];
				if (dark) {
					qr_mc.beginFill(0, 100);
					qr_mc.moveTo(x, y);
					qr_mc.lineTo(x + cs, y);
					qr_mc.lineTo(x + cs, y + cs);
					qr_mc.lineTo(x, y + cs);
					qr_mc.endFill();
				}
			}
		}
		return qr_mc;
	},
	setupTimingPattern : function() {
		for (var r = 8; r < this.moduleCount - 8; r++) {
			if (this.modules[r][6] !== null) {
				continue;
			}
			this.modules[r][6] = (r % 2 === 0);
		}
		for (var c = 8; c < this.moduleCount - 8; c++) {
			if (this.modules[6][c] !== null) {
				continue;
			}
			this.modules[6][c] = (c % 2 === 0);
		}
	},
	setupPositionAdjustPattern : function() {
		var pos = QRUtil.getPatternPosition(this.typeNumber);
		for (var i = 0; i < pos.length; i++) {
			for (var j = 0; j < pos.length; j++) {
				var row = pos[i];
				var col = pos[j];
				if (this.modules[row][col] !== null) {
					continue;
				}
				for (var r = -2; r <= 2; r++) {
					for (var c = -2; c <= 2; c++) {
						if (Math.abs(r) === 2 || 
                            Math.abs(c) === 2 ||
                            (r === 0 && c === 0) ) {
							this.modules[row + r][col + c] = true;
						} else {
							this.modules[row + r][col + c] = false;
						}
					}
				}
			}
		}
	},
	setupTypeNumber : function(test) {
		var bits = QRUtil.getBCHTypeNumber(this.typeNumber);
        var mod;
		for (var i = 0; i < 18; i++) {
			mod = (!test && ( (bits >> i) & 1) === 1);
			this.modules[Math.floor(i / 3)][i % 3 + this.moduleCount - 8 - 3] = mod;
		}
		for (var x = 0; x < 18; x++) {
			mod = (!test && ( (bits >> x) & 1) === 1);
			this.modules[x % 3 + this.moduleCount - 8 - 3][Math.floor(x / 3)] = mod;
		}
	},
	setupTypeInfo : function(test, maskPattern) {
		var data = (this.errorCorrectLevel << 3) | maskPattern;
		var bits = QRUtil.getBCHTypeInfo(data);
        var mod;
		for (var v = 0; v < 15; v++) {
			mod = (!test && ( (bits >> v) & 1) === 1);
			if (v < 6) {
				this.modules[v][8] = mod;
			} else if (v < 8) {
				this.modules[v + 1][8] = mod;
			} else {
				this.modules[this.moduleCount - 15 + v][8] = mod;
			}
		}
		for (var h = 0; h < 15; h++) {
			mod = (!test && ( (bits >> h) & 1) === 1);
			if (h < 8) {
				this.modules[8][this.moduleCount - h - 1] = mod;
			} else if (h < 9) {
				this.modules[8][15 - h - 1 + 1] = mod;
			} else {
				this.modules[8][15 - h - 1] = mod;
			}
		}
		this.modules[this.moduleCount - 8][8] = (!test);
	},
	mapData : function(data, maskPattern) {
		var inc = -1;
		var row = this.moduleCount - 1;
		var bitIndex = 7;
		var byteIndex = 0;
		for (var col = this.moduleCount - 1; col > 0; col -= 2) {
			if (col === 6) col--;
			while (true) {
				for (var c = 0; c < 2; c++) {
					if (this.modules[row][col - c] === null) {
						var dark = false;
						if (byteIndex < data.length) {
							dark = ( ( (data[byteIndex] >>> bitIndex) & 1) === 1);
						}
						var mask = QRUtil.getMask(maskPattern, row, col - c);
						if (mask) {
							dark = !dark;
						}
						this.modules[row][col - c] = dark;
						bitIndex--;
						if (bitIndex === -1) {
							byteIndex++;
							bitIndex = 7;
						}
					}
				}
				row += inc;
				if (row < 0 || this.moduleCount <= row) {
					row -= inc;
					inc = -inc;
					break;
				}
			}
		}
	}
};
QRCode.PAD0 = 0xEC;
QRCode.PAD1 = 0x11;
QRCode.createData = function(typeNumber, errorCorrectLevel, dataList) {
	var rsBlocks = QRRSBlock.getRSBlocks(typeNumber, errorCorrectLevel);
	var buffer = new QRBitBuffer();
	for (var i = 0; i < dataList.length; i++) {
		var data = dataList[i];
		buffer.put(data.mode, 4);
		buffer.put(data.getLength(), QRUtil.getLengthInBits(data.mode, typeNumber) );
		data.write(buffer);
	}
	var totalDataCount = 0;
	for (var x = 0; x < rsBlocks.length; x++) {
		totalDataCount += rsBlocks[x].dataCount;
	}
	if (buffer.getLengthInBits() > totalDataCount * 8) {
		throw new Error("code length overflow. (" + 
            buffer.getLengthInBits() + 
            ">" +  
            totalDataCount * 8 + 
            ")");
	}
	if (buffer.getLengthInBits() + 4 <= totalDataCount * 8) {
		buffer.put(0, 4);
	}
	while (buffer.getLengthInBits() % 8 !== 0) {
		buffer.putBit(false);
	}
	while (true) {
		if (buffer.getLengthInBits() >= totalDataCount * 8) {
			break;
		}
		buffer.put(QRCode.PAD0, 8);
		if (buffer.getLengthInBits() >= totalDataCount * 8) {
			break;
		}
		buffer.put(QRCode.PAD1, 8);
	}
	return QRCode.createBytes(buffer, rsBlocks);
};
QRCode.createBytes = function(buffer, rsBlocks) {
	var offset = 0;
	var maxDcCount = 0;
	var maxEcCount = 0;
	var dcdata = new Array(rsBlocks.length);
	var ecdata = new Array(rsBlocks.length);
	for (var r = 0; r < rsBlocks.length; r++) {
		var dcCount = rsBlocks[r].dataCount;
		var ecCount = rsBlocks[r].totalCount - dcCount;
		maxDcCount = Math.max(maxDcCount, dcCount);
		maxEcCount = Math.max(maxEcCount, ecCount);
		dcdata[r] = new Array(dcCount);
		for (var i = 0; i < dcdata[r].length; i++) {
			dcdata[r][i] = 0xff & buffer.buffer[i + offset];
		}
		offset += dcCount;
		var rsPoly = QRUtil.getErrorCorrectPolynomial(ecCount);
		var rawPoly = new QRPolynomial(dcdata[r], rsPoly.getLength() - 1);
		var modPoly = rawPoly.mod(rsPoly);
		ecdata[r] = new Array(rsPoly.getLength() - 1);
		for (var x = 0; x < ecdata[r].length; x++) {
            var modIndex = x + modPoly.getLength() - ecdata[r].length;
			ecdata[r][x] = (modIndex >= 0)? modPoly.get(modIndex) : 0;
		}
	}
	var totalCodeCount = 0;
	for (var y = 0; y < rsBlocks.length; y++) {
		totalCodeCount += rsBlocks[y].totalCount;
	}
	var data = new Array(totalCodeCount);
	var index = 0;
	for (var z = 0; z < maxDcCount; z++) {
		for (var s = 0; s < rsBlocks.length; s++) {
			if (z < dcdata[s].length) {
				data[index++] = dcdata[s][z];
			}
		}
	}
	for (var xx = 0; xx < maxEcCount; xx++) {
		for (var t = 0; t < rsBlocks.length; t++) {
			if (xx < ecdata[t].length) {
				data[index++] = ecdata[t][xx];
			}
		}
	}
	return data;
};
window.QRCode=QRCode;window.QRErrorCorrectLevel=QRErrorCorrectLevel;
})();
)TPCALQRLIB";

static const char ethPakoDeflateLibrary[] PROGMEM = R"TPPAKO(
!function(t){if("object"==typeof exports&&"undefined"!=typeof module)module.exports=t();else if("function"==typeof define&&define.amd)define([],t);else{("undefined"!=typeof window?window:"undefined"!=typeof global?global:"undefined"!=typeof self?self:this).pako=t()}}(function(){return function i(s,h,l){function o(e,t){if(!h[e]){if(!s[e]){var a="function"==typeof require&&require;if(!t&&a)return a(e,!0);if(_)return _(e,!0);var n=new Error("Cannot find module '"+e+"'");throw n.code="MODULE_NOT_FOUND",n}var r=h[e]={exports:{}};s[e][0].call(r.exports,function(t){return o(s[e][1][t]||t)},r,r.exports,i,s,h,l)}return h[e].exports}for(var _="function"==typeof require&&require,t=0;t<l.length;t++)o(l[t]);return o}({1:[function(t,e,a){"use strict";var n="undefined"!=typeof Uint8Array&&"undefined"!=typeof Uint16Array&&"undefined"!=typeof Int32Array;a.assign=function(t){for(var e,a,n=Array.prototype.slice.call(arguments,1);n.length;){var r=n.shift();if(r){if("object"!=typeof r)throw new TypeError(r+"must be non-object");for(var i in r)e=r,a=i,Object.prototype.hasOwnProperty.call(e,a)&&(t[i]=r[i])}}return t},a.shrinkBuf=function(t,e){return t.length===e?t:t.subarray?t.subarray(0,e):(t.length=e,t)};var r={arraySet:function(t,e,a,n,r){if(e.subarray&&t.subarray)t.set(e.subarray(a,a+n),r);else for(var i=0;i<n;i++)t[r+i]=e[a+i]},flattenChunks:function(t){var e,a,n,r,i,s;for(e=n=0,a=t.length;e<a;e++)n+=t[e].length;for(s=new Uint8Array(n),e=r=0,a=t.length;e<a;e++)i=t[e],s.set(i,r),r+=i.length;return s}},i={arraySet:function(t,e,a,n,r){for(var i=0;i<n;i++)t[r+i]=e[a+i]},flattenChunks:function(t){return[].concat.apply([],t)}};a.setTyped=function(t){t?(a.Buf8=Uint8Array,a.Buf16=Uint16Array,a.Buf32=Int32Array,a.assign(a,r)):(a.Buf8=Array,a.Buf16=Array,a.Buf32=Array,a.assign(a,i))},a.setTyped(n)},{}],2:[function(t,e,a){"use strict";var l=t("./common"),r=!0,i=!0;try{String.fromCharCode.apply(null,[0])}catch(t){r=!1}try{String.fromCharCode.apply(null,new Uint8Array(1))}catch(t){i=!1}for(var o=new l.Buf8(256),n=0;n<256;n++)o[n]=252<=n?6:248<=n?5:240<=n?4:224<=n?3:192<=n?2:1;function _(t,e){if(e<65534&&(t.subarray&&i||!t.subarray&&r))return String.fromCharCode.apply(null,l.shrinkBuf(t,e));for(var a="",n=0;n<e;n++)a+=String.fromCharCode(t[n]);return a}o[254]=o[254]=1,a.string2buf=function(t){var e,a,n,r,i,s=t.length,h=0;for(r=0;r<s;r++)55296==(64512&(a=t.charCodeAt(r)))&&r+1<s&&56320==(64512&(n=t.charCodeAt(r+1)))&&(a=65536+(a-55296<<10)+(n-56320),r++),h+=a<128?1:a<2048?2:a<65536?3:4;for(e=new l.Buf8(h),r=i=0;i<h;r++)55296==(64512&(a=t.charCodeAt(r)))&&r+1<s&&56320==(64512&(n=t.charCodeAt(r+1)))&&(a=65536+(a-55296<<10)+(n-56320),r++),a<128?e[i++]=a:(a<2048?e[i++]=192|a>>>6:(a<65536?e[i++]=224|a>>>12:(e[i++]=240|a>>>18,e[i++]=128|a>>>12&63),e[i++]=128|a>>>6&63),e[i++]=128|63&a);return e},a.buf2binstring=function(t){return _(t,t.length)},a.binstring2buf=function(t){for(var e=new l.Buf8(t.length),a=0,n=e.length;a<n;a++)e[a]=t.charCodeAt(a);return e},a.buf2string=function(t,e){var a,n,r,i,s=e||t.length,h=new Array(2*s);for(a=n=0;a<s;)if((r=t[a++])<128)h[n++]=r;else if(4<(i=o[r]))h[n++]=65533,a+=i-1;else{for(r&=2===i?31:3===i?15:7;1<i&&a<s;)r=r<<6|63&t[a++],i--;1<i?h[n++]=65533:r<65536?h[n++]=r:(r-=65536,h[n++]=55296|r>>10&1023,h[n++]=56320|1023&r)}return _(h,n)},a.utf8border=function(t,e){var a;for((e=e||t.length)>t.length&&(e=t.length),a=e-1;0<=a&&128==(192&t[a]);)a--;return a<0?e:0===a?e:a+o[t[a]]>e?a:e}},{"./common":1}],3:[function(t,e,a){"use strict";e.exports=function(t,e,a,n){for(var r=65535&t|0,i=t>>>16&65535|0,s=0;0!==a;){for(a-=s=2e3<a?2e3:a;i=i+(r=r+e[n++]|0)|0,--s;);r%=65521,i%=65521}return r|i<<16|0}},{}],4:[function(t,e,a){"use strict";var h=function(){for(var t,e=[],a=0;a<256;a++){t=a;for(var n=0;n<8;n++)t=1&t?3988292384^t>>>1:t>>>1;e[a]=t}return e}();e.exports=function(t,e,a,n){var r=h,i=n+a;t^=-1;for(var s=n;s<i;s++)t=t>>>8^r[255&(t^e[s])];return-1^t}},{}],5:[function(t,e,a){"use strict";var l,u=t("../utils/common"),o=t("./trees"),f=t("./adler32"),c=t("./crc32"),n=t("./messages"),_=0,d=4,p=0,g=-2,m=-1,b=4,r=2,v=8,w=9,i=286,s=30,h=19,y=2*i+1,k=15,z=3,x=258,B=x+z+1,A=42,C=113,S=1,j=2,E=3,U=4;function D(t,e){return t.msg=n[e],e}function I(t){return(t<<1)-(4<t?9:0)}function O(t){for(var e=t.length;0<=--e;)t[e]=0}function q(t){var e=t.state,a=e.pending;a>t.avail_out&&(a=t.avail_out),0!==a&&(u.arraySet(t.output,e.pending_buf,e.pending_out,a,t.next_out),t.next_out+=a,e.pending_out+=a,t.total_out+=a,t.avail_out-=a,e.pending-=a,0===e.pending&&(e.pending_out=0))}function T(t,e){o._tr_flush_block(t,0<=t.block_start?t.block_start:-1,t.strstart-t.block_start,e),t.block_start=t.strstart,q(t.strm)}function L(t,e){t.pending_buf[t.pending++]=e}function N(t,e){t.pending_buf[t.pending++]=e>>>8&255,t.pending_buf[t.pending++]=255&e}function R(t,e){var a,n,r=t.max_chain_length,i=t.strstart,s=t.prev_length,h=t.nice_match,l=t.strstart>t.w_size-B?t.strstart-(t.w_size-B):0,o=t.window,_=t.w_mask,d=t.prev,u=t.strstart+x,f=o[i+s-1],c=o[i+s];t.prev_length>=t.good_match&&(r>>=2),h>t.lookahead&&(h=t.lookahead);do{if(o[(a=e)+s]===c&&o[a+s-1]===f&&o[a]===o[i]&&o[++a]===o[i+1]){i+=2,a++;do{}while(o[++i]===o[++a]&&o[++i]===o[++a]&&o[++i]===o[++a]&&o[++i]===o[++a]&&o[++i]===o[++a]&&o[++i]===o[++a]&&o[++i]===o[++a]&&o[++i]===o[++a]&&i<u);if(n=x-(u-i),i=u-x,s<n){if(t.match_start=e,h<=(s=n))break;f=o[i+s-1],c=o[i+s]}}}while((e=d[e&_])>l&&0!=--r);return s<=t.lookahead?s:t.lookahead}function H(t){var e,a,n,r,i,s,h,l,o,_,d=t.w_size;do{if(r=t.window_size-t.lookahead-t.strstart,t.strstart>=d+(d-B)){for(u.arraySet(t.window,t.window,d,d,0),t.match_start-=d,t.strstart-=d,t.block_start-=d,e=a=t.hash_size;n=t.head[--e],t.head[e]=d<=n?n-d:0,--a;);for(e=a=d;n=t.prev[--e],t.prev[e]=d<=n?n-d:0,--a;);r+=d}if(0===t.strm.avail_in)break;if(s=t.strm,h=t.window,l=t.strstart+t.lookahead,o=r,_=void 0,_=s.avail_in,o<_&&(_=o),a=0===_?0:(s.avail_in-=_,u.arraySet(h,s.input,s.next_in,_,l),1===s.state.wrap?s.adler=f(s.adler,h,_,l):2===s.state.wrap&&(s.adler=c(s.adler,h,_,l)),s.next_in+=_,s.total_in+=_,_),t.lookahead+=a,t.lookahead+t.insert>=z)for(i=t.strstart-t.insert,t.ins_h=t.window[i],t.ins_h=(t.ins_h<<t.hash_shift^t.window[i+1])&t.hash_mask;t.insert&&(t.ins_h=(t.ins_h<<t.hash_shift^t.window[i+z-1])&t.hash_mask,t.prev[i&t.w_mask]=t.head[t.ins_h],t.head[t.ins_h]=i,i++,t.insert--,!(t.lookahead+t.insert<z)););}while(t.lookahead<B&&0!==t.strm.avail_in)}function F(t,e){for(var a,n;;){if(t.lookahead<B){if(H(t),t.lookahead<B&&e===_)return S;if(0===t.lookahead)break}if(a=0,t.lookahead>=z&&(t.ins_h=(t.ins_h<<t.hash_shift^t.window[t.strstart+z-1])&t.hash_mask,a=t.prev[t.strstart&t.w_mask]=t.head[t.ins_h],t.head[t.ins_h]=t.strstart),0!==a&&t.strstart-a<=t.w_size-B&&(t.match_length=R(t,a)),t.match_length>=z)if(n=o._tr_tally(t,t.strstart-t.match_start,t.match_length-z),t.lookahead-=t.match_length,t.match_length<=t.max_lazy_match&&t.lookahead>=z){for(t.match_length--;t.strstart++,t.ins_h=(t.ins_h<<t.hash_shift^t.window[t.strstart+z-1])&t.hash_mask,a=t.prev[t.strstart&t.w_mask]=t.head[t.ins_h],t.head[t.ins_h]=t.strstart,0!=--t.match_length;);t.strstart++}else t.strstart+=t.match_length,t.match_length=0,t.ins_h=t.window[t.strstart],t.ins_h=(t.ins_h<<t.hash_shift^t.window[t.strstart+1])&t.hash_mask;else n=o._tr_tally(t,0,t.window[t.strstart]),t.lookahead--,t.strstart++;if(n&&(T(t,!1),0===t.strm.avail_out))return S}return t.insert=t.strstart<z-1?t.strstart:z-1,e===d?(T(t,!0),0===t.strm.avail_out?E:U):t.last_lit&&(T(t,!1),0===t.strm.avail_out)?S:j}function K(t,e){for(var a,n,r;;){if(t.lookahead<B){if(H(t),t.lookahead<B&&e===_)return S;if(0===t.lookahead)break}if(a=0,t.lookahead>=z&&(t.ins_h=(t.ins_h<<t.hash_shift^t.window[t.strstart+z-1])&t.hash_mask,a=t.prev[t.strstart&t.w_mask]=t.head[t.ins_h],t.head[t.ins_h]=t.strstart),t.prev_length=t.match_length,t.prev_match=t.match_start,t.match_length=z-1,0!==a&&t.prev_length<t.max_lazy_match&&t.strstart-a<=t.w_size-B&&(t.match_length=R(t,a),t.match_length<=5&&(1===t.strategy||t.match_length===z&&4096<t.strstart-t.match_start)&&(t.match_length=z-1)),t.prev_length>=z&&t.match_length<=t.prev_length){for(r=t.strstart+t.lookahead-z,n=o._tr_tally(t,t.strstart-1-t.prev_match,t.prev_length-z),t.lookahead-=t.prev_length-1,t.prev_length-=2;++t.strstart<=r&&(t.ins_h=(t.ins_h<<t.hash_shift^t.window[t.strstart+z-1])&t.hash_mask,a=t.prev[t.strstart&t.w_mask]=t.head[t.ins_h],t.head[t.ins_h]=t.strstart),0!=--t.prev_length;);if(t.match_available=0,t.match_length=z-1,t.strstart++,n&&(T(t,!1),0===t.strm.avail_out))return S}else if(t.match_available){if((n=o._tr_tally(t,0,t.window[t.strstart-1]))&&T(t,!1),t.strstart++,t.lookahead--,0===t.strm.avail_out)return S}else t.match_available=1,t.strstart++,t.lookahead--}return t.match_available&&(n=o._tr_tally(t,0,t.window[t.strstart-1]),t.match_available=0),t.insert=t.strstart<z-1?t.strstart:z-1,e===d?(T(t,!0),0===t.strm.avail_out?E:U):t.last_lit&&(T(t,!1),0===t.strm.avail_out)?S:j}function M(t,e,a,n,r){this.good_length=t,this.max_lazy=e,this.nice_length=a,this.max_chain=n,this.func=r}function P(){this.strm=null,this.status=0,this.pending_buf=null,this.pending_buf_size=0,this.pending_out=0,this.pending=0,this.wrap=0,this.gzhead=null,this.gzindex=0,this.method=v,this.last_flush=-1,this.w_size=0,this.w_bits=0,this.w_mask=0,this.window=null,this.window_size=0,this.prev=null,this.head=null,this.ins_h=0,this.hash_size=0,this.hash_bits=0,this.hash_mask=0,this.hash_shift=0,this.block_start=0,this.match_length=0,this.prev_match=0,this.match_available=0,this.strstart=0,this.match_start=0,this.lookahead=0,this.prev_length=0,this.max_chain_length=0,this.max_lazy_match=0,this.level=0,this.strategy=0,this.good_match=0,this.nice_match=0,this.dyn_ltree=new u.Buf16(2*y),this.dyn_dtree=new u.Buf16(2*(2*s+1)),this.bl_tree=new u.Buf16(2*(2*h+1)),O(this.dyn_ltree),O(this.dyn_dtree),O(this.bl_tree),this.l_desc=null,this.d_desc=null,this.bl_desc=null,this.bl_count=new u.Buf16(k+1),this.heap=new u.Buf16(2*i+1),O(this.heap),this.heap_len=0,this.heap_max=0,this.depth=new u.Buf16(2*i+1),O(this.depth),this.l_buf=0,this.lit_bufsize=0,this.last_lit=0,this.d_buf=0,this.opt_len=0,this.static_len=0,this.matches=0,this.insert=0,this.bi_buf=0,this.bi_valid=0}function G(t){var e;return t&&t.state?(t.total_in=t.total_out=0,t.data_type=r,(e=t.state).pending=0,e.pending_out=0,e.wrap<0&&(e.wrap=-e.wrap),e.status=e.wrap?A:C,t.adler=2===e.wrap?0:1,e.last_flush=_,o._tr_init(e),p):D(t,g)}function J(t){var e,a=G(t);return a===p&&((e=t.state).window_size=2*e.w_size,O(e.head),e.max_lazy_match=l[e.level].max_lazy,e.good_match=l[e.level].good_length,e.nice_match=l[e.level].nice_length,e.max_chain_length=l[e.level].max_chain,e.strstart=0,e.block_start=0,e.lookahead=0,e.insert=0,e.match_length=e.prev_length=z-1,e.match_available=0,e.ins_h=0),a}function Q(t,e,a,n,r,i){if(!t)return g;var s=1;if(e===m&&(e=6),n<0?(s=0,n=-n):15<n&&(s=2,n-=16),r<1||w<r||a!==v||n<8||15<n||e<0||9<e||i<0||b<i)return D(t,g);8===n&&(n=9);var h=new P;return(t.state=h).strm=t,h.wrap=s,h.gzhead=null,h.w_bits=n,h.w_size=1<<h.w_bits,h.w_mask=h.w_size-1,h.hash_bits=r+7,h.hash_size=1<<h.hash_bits,h.hash_mask=h.hash_size-1,h.hash_shift=~~((h.hash_bits+z-1)/z),h.window=new u.Buf8(2*h.w_size),h.head=new u.Buf16(h.hash_size),h.prev=new u.Buf16(h.w_size),h.lit_bufsize=1<<r+6,h.pending_buf_size=4*h.lit_bufsize,h.pending_buf=new u.Buf8(h.pending_buf_size),h.d_buf=1*h.lit_bufsize,h.l_buf=3*h.lit_bufsize,h.level=e,h.strategy=i,h.method=a,J(t)}l=[new M(0,0,0,0,function(t,e){var a=65535;for(a>t.pending_buf_size-5&&(a=t.pending_buf_size-5);;){if(t.lookahead<=1){if(H(t),0===t.lookahead&&e===_)return S;if(0===t.lookahead)break}t.strstart+=t.lookahead,t.lookahead=0;var n=t.block_start+a;if((0===t.strstart||t.strstart>=n)&&(t.lookahead=t.strstart-n,t.strstart=n,T(t,!1),0===t.strm.avail_out))return S;if(t.strstart-t.block_start>=t.w_size-B&&(T(t,!1),0===t.strm.avail_out))return S}return t.insert=0,e===d?(T(t,!0),0===t.strm.avail_out?E:U):(t.strstart>t.block_start&&(T(t,!1),t.strm.avail_out),S)}),new M(4,4,8,4,F),new M(4,5,16,8,F),new M(4,6,32,32,F),new M(4,4,16,16,K),new M(8,16,32,32,K),new M(8,16,128,128,K),new M(8,32,128,256,K),new M(32,128,258,1024,K),new M(32,258,258,4096,K)],a.deflateInit=function(t,e){return Q(t,e,v,15,8,0)},a.deflateInit2=Q,a.deflateReset=J,a.deflateResetKeep=G,a.deflateSetHeader=function(t,e){return t&&t.state?2!==t.state.wrap?g:(t.state.gzhead=e,p):g},a.deflate=function(t,e){var a,n,r,i;if(!t||!t.state||5<e||e<0)return t?D(t,g):g;if(n=t.state,!t.output||!t.input&&0!==t.avail_in||666===n.status&&e!==d)return D(t,0===t.avail_out?-5:g);if(n.strm=t,a=n.last_flush,n.last_flush=e,n.status===A)if(2===n.wrap)t.adler=0,L(n,31),L(n,139),L(n,8),n.gzhead?(L(n,(n.gzhead.text?1:0)+(n.gzhead.hcrc?2:0)+(n.gzhead.extra?4:0)+(n.gzhead.name?8:0)+(n.gzhead.comment?16:0)),L(n,255&n.gzhead.time),L(n,n.gzhead.time>>8&255),L(n,n.gzhead.time>>16&255),L(n,n.gzhead.time>>24&255),L(n,9===n.level?2:2<=n.strategy||n.level<2?4:0),L(n,255&n.gzhead.os),n.gzhead.extra&&n.gzhead.extra.length&&(L(n,255&n.gzhead.extra.length),L(n,n.gzhead.extra.length>>8&255)),n.gzhead.hcrc&&(t.adler=c(t.adler,n.pending_buf,n.pending,0)),n.gzindex=0,n.status=69):(L(n,0),L(n,0),L(n,0),L(n,0),L(n,0),L(n,9===n.level?2:2<=n.strategy||n.level<2?4:0),L(n,3),n.status=C);else{var s=v+(n.w_bits-8<<4)<<8;s|=(2<=n.strategy||n.level<2?0:n.level<6?1:6===n.level?2:3)<<6,0!==n.strstart&&(s|=32),s+=31-s%31,n.status=C,N(n,s),0!==n.strstart&&(N(n,t.adler>>>16),N(n,65535&t.adler)),t.adler=1}if(69===n.status)if(n.gzhead.extra){for(r=n.pending;n.gzindex<(65535&n.gzhead.extra.length)&&(n.pending!==n.pending_buf_size||(n.gzhead.hcrc&&n.pending>r&&(t.adler=c(t.adler,n.pending_buf,n.pending-r,r)),q(t),r=n.pending,n.pending!==n.pending_buf_size));)L(n,255&n.gzhead.extra[n.gzindex]),n.gzindex++;n.gzhead.hcrc&&n.pending>r&&(t.adler=c(t.adler,n.pending_buf,n.pending-r,r)),n.gzindex===n.gzhead.extra.length&&(n.gzindex=0,n.status=73)}else n.status=73;if(73===n.status)if(n.gzhead.name){r=n.pending;do{if(n.pending===n.pending_buf_size&&(n.gzhead.hcrc&&n.pending>r&&(t.adler=c(t.adler,n.pending_buf,n.pending-r,r)),q(t),r=n.pending,n.pending===n.pending_buf_size)){i=1;break}L(n,i=n.gzindex<n.gzhead.name.length?255&n.gzhead.name.charCodeAt(n.gzindex++):0)}while(0!==i);n.gzhead.hcrc&&n.pending>r&&(t.adler=c(t.adler,n.pending_buf,n.pending-r,r)),0===i&&(n.gzindex=0,n.status=91)}else n.status=91;if(91===n.status)if(n.gzhead.comment){r=n.pending;do{if(n.pending===n.pending_buf_size&&(n.gzhead.hcrc&&n.pending>r&&(t.adler=c(t.adler,n.pending_buf,n.pending-r,r)),q(t),r=n.pending,n.pending===n.pending_buf_size)){i=1;break}L(n,i=n.gzindex<n.gzhead.comment.length?255&n.gzhead.comment.charCodeAt(n.gzindex++):0)}while(0!==i);n.gzhead.hcrc&&n.pending>r&&(t.adler=c(t.adler,n.pending_buf,n.pending-r,r)),0===i&&(n.status=103)}else n.status=103;if(103===n.status&&(n.gzhead.hcrc?(n.pending+2>n.pending_buf_size&&q(t),n.pending+2<=n.pending_buf_size&&(L(n,255&t.adler),L(n,t.adler>>8&255),t.adler=0,n.status=C)):n.status=C),0!==n.pending){if(q(t),0===t.avail_out)return n.last_flush=-1,p}else if(0===t.avail_in&&I(e)<=I(a)&&e!==d)return D(t,-5);if(666===n.status&&0!==t.avail_in)return D(t,-5);if(0!==t.avail_in||0!==n.lookahead||e!==_&&666!==n.status){var h=2===n.strategy?function(t,e){for(var a;;){if(0===t.lookahead&&(H(t),0===t.lookahead)){if(e===_)return S;break}if(t.match_length=0,a=o._tr_tally(t,0,t.window[t.strstart]),t.lookahead--,t.strstart++,a&&(T(t,!1),0===t.strm.avail_out))return S}return t.insert=0,e===d?(T(t,!0),0===t.strm.avail_out?E:U):t.last_lit&&(T(t,!1),0===t.strm.avail_out)?S:j}(n,e):3===n.strategy?function(t,e){for(var a,n,r,i,s=t.window;;){if(t.lookahead<=x){if(H(t),t.lookahead<=x&&e===_)return S;if(0===t.lookahead)break}if(t.match_length=0,t.lookahead>=z&&0<t.strstart&&(n=s[r=t.strstart-1])===s[++r]&&n===s[++r]&&n===s[++r]){i=t.strstart+x;do{}while(n===s[++r]&&n===s[++r]&&n===s[++r]&&n===s[++r]&&n===s[++r]&&n===s[++r]&&n===s[++r]&&n===s[++r]&&r<i);t.match_length=x-(i-r),t.match_length>t.lookahead&&(t.match_length=t.lookahead)}if(t.match_length>=z?(a=o._tr_tally(t,1,t.match_length-z),t.lookahead-=t.match_length,t.strstart+=t.match_length,t.match_length=0):(a=o._tr_tally(t,0,t.window[t.strstart]),t.lookahead--,t.strstart++),a&&(T(t,!1),0===t.strm.avail_out))return S}return t.insert=0,e===d?(T(t,!0),0===t.strm.avail_out?E:U):t.last_lit&&(T(t,!1),0===t.strm.avail_out)?S:j}(n,e):l[n.level].func(n,e);if(h!==E&&h!==U||(n.status=666),h===S||h===E)return 0===t.avail_out&&(n.last_flush=-1),p;if(h===j&&(1===e?o._tr_align(n):5!==e&&(o._tr_stored_block(n,0,0,!1),3===e&&(O(n.head),0===n.lookahead&&(n.strstart=0,n.block_start=0,n.insert=0))),q(t),0===t.avail_out))return n.last_flush=-1,p}return e!==d?p:n.wrap<=0?1:(2===n.wrap?(L(n,255&t.adler),L(n,t.adler>>8&255),L(n,t.adler>>16&255),L(n,t.adler>>24&255),L(n,255&t.total_in),L(n,t.total_in>>8&255),L(n,t.total_in>>16&255),L(n,t.total_in>>24&255)):(N(n,t.adler>>>16),N(n,65535&t.adler)),q(t),0<n.wrap&&(n.wrap=-n.wrap),0!==n.pending?p:1)},a.deflateEnd=function(t){var e;return t&&t.state?(e=t.state.status)!==A&&69!==e&&73!==e&&91!==e&&103!==e&&e!==C&&666!==e?D(t,g):(t.state=null,e===C?D(t,-3):p):g},a.deflateSetDictionary=function(t,e){var a,n,r,i,s,h,l,o,_=e.length;if(!t||!t.state)return g;if(2===(i=(a=t.state).wrap)||1===i&&a.status!==A||a.lookahead)return g;for(1===i&&(t.adler=f(t.adler,e,_,0)),a.wrap=0,_>=a.w_size&&(0===i&&(O(a.head),a.strstart=0,a.block_start=0,a.insert=0),o=new u.Buf8(a.w_size),u.arraySet(o,e,_-a.w_size,a.w_size,0),e=o,_=a.w_size),s=t.avail_in,h=t.next_in,l=t.input,t.avail_in=_,t.next_in=0,t.input=e,H(a);a.lookahead>=z;){for(n=a.strstart,r=a.lookahead-(z-1);a.ins_h=(a.ins_h<<a.hash_shift^a.window[n+z-1])&a.hash_mask,a.prev[n&a.w_mask]=a.head[a.ins_h],a.head[a.ins_h]=n,n++,--r;);a.strstart=n,a.lookahead=z-1,H(a)}return a.strstart+=a.lookahead,a.block_start=a.strstart,a.insert=a.lookahead,a.lookahead=0,a.match_length=a.prev_length=z-1,a.match_available=0,t.next_in=h,t.input=l,t.avail_in=s,a.wrap=i,p},a.deflateInfo="pako deflate (from Nodeca project)"},{"../utils/common":1,"./adler32":3,"./crc32":4,"./messages":6,"./trees":7}],6:[function(t,e,a){"use strict";e.exports={2:"need dictionary",1:"stream end",0:"","-1":"file error","-2":"stream error","-3":"data error","-4":"insufficient memory","-5":"buffer error","-6":"incompatible version"}},{}],7:[function(t,e,a){"use strict";var l=t("../utils/common"),h=0,o=1;function n(t){for(var e=t.length;0<=--e;)t[e]=0}var _=0,s=29,d=256,u=d+1+s,f=30,c=19,g=2*u+1,m=15,r=16,p=7,b=256,v=16,w=17,y=18,k=[0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0],z=[0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13],x=[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7],B=[16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15],A=new Array(2*(u+2));n(A);var C=new Array(2*f);n(C);var S=new Array(512);n(S);var j=new Array(256);n(j);var E=new Array(s);n(E);var U,D,I,O=new Array(f);function q(t,e,a,n,r){this.static_tree=t,this.extra_bits=e,this.extra_base=a,this.elems=n,this.max_length=r,this.has_stree=t&&t.length}function i(t,e){this.dyn_tree=t,this.max_code=0,this.stat_desc=e}function T(t){return t<256?S[t]:S[256+(t>>>7)]}function L(t,e){t.pending_buf[t.pending++]=255&e,t.pending_buf[t.pending++]=e>>>8&255}function N(t,e,a){t.bi_valid>r-a?(t.bi_buf|=e<<t.bi_valid&65535,L(t,t.bi_buf),t.bi_buf=e>>r-t.bi_valid,t.bi_valid+=a-r):(t.bi_buf|=e<<t.bi_valid&65535,t.bi_valid+=a)}function R(t,e,a){N(t,a[2*e],a[2*e+1])}function H(t,e){for(var a=0;a|=1&t,t>>>=1,a<<=1,0<--e;);return a>>>1}function F(t,e,a){var n,r,i=new Array(m+1),s=0;for(n=1;n<=m;n++)i[n]=s=s+a[n-1]<<1;for(r=0;r<=e;r++){var h=t[2*r+1];0!==h&&(t[2*r]=H(i[h]++,h))}}function K(t){var e;for(e=0;e<u;e++)t.dyn_ltree[2*e]=0;for(e=0;e<f;e++)t.dyn_dtree[2*e]=0;for(e=0;e<c;e++)t.bl_tree[2*e]=0;t.dyn_ltree[2*b]=1,t.opt_len=t.static_len=0,t.last_lit=t.matches=0}function M(t){8<t.bi_valid?L(t,t.bi_buf):0<t.bi_valid&&(t.pending_buf[t.pending++]=t.bi_buf),t.bi_buf=0,t.bi_valid=0}function P(t,e,a,n){var r=2*e,i=2*a;return t[r]<t[i]||t[r]===t[i]&&n[e]<=n[a]}function G(t,e,a){for(var n=t.heap[a],r=a<<1;r<=t.heap_len&&(r<t.heap_len&&P(e,t.heap[r+1],t.heap[r],t.depth)&&r++,!P(e,n,t.heap[r],t.depth));)t.heap[a]=t.heap[r],a=r,r<<=1;t.heap[a]=n}function J(t,e,a){var n,r,i,s,h=0;if(0!==t.last_lit)for(;n=t.pending_buf[t.d_buf+2*h]<<8|t.pending_buf[t.d_buf+2*h+1],r=t.pending_buf[t.l_buf+h],h++,0===n?R(t,r,e):(R(t,(i=j[r])+d+1,e),0!==(s=k[i])&&N(t,r-=E[i],s),R(t,i=T(--n),a),0!==(s=z[i])&&N(t,n-=O[i],s)),h<t.last_lit;);R(t,b,e)}function Q(t,e){var a,n,r,i=e.dyn_tree,s=e.stat_desc.static_tree,h=e.stat_desc.has_stree,l=e.stat_desc.elems,o=-1;for(t.heap_len=0,t.heap_max=g,a=0;a<l;a++)0!==i[2*a]?(t.heap[++t.heap_len]=o=a,t.depth[a]=0):i[2*a+1]=0;for(;t.heap_len<2;)i[2*(r=t.heap[++t.heap_len]=o<2?++o:0)]=1,t.depth[r]=0,t.opt_len--,h&&(t.static_len-=s[2*r+1]);for(e.max_code=o,a=t.heap_len>>1;1<=a;a--)G(t,i,a);for(r=l;a=t.heap[1],t.heap[1]=t.heap[t.heap_len--],G(t,i,1),n=t.heap[1],t.heap[--t.heap_max]=a,t.heap[--t.heap_max]=n,i[2*r]=i[2*a]+i[2*n],t.depth[r]=(t.depth[a]>=t.depth[n]?t.depth[a]:t.depth[n])+1,i[2*a+1]=i[2*n+1]=r,t.heap[1]=r++,G(t,i,1),2<=t.heap_len;);t.heap[--t.heap_max]=t.heap[1],function(t,e){var a,n,r,i,s,h,l=e.dyn_tree,o=e.max_code,_=e.stat_desc.static_tree,d=e.stat_desc.has_stree,u=e.stat_desc.extra_bits,f=e.stat_desc.extra_base,c=e.stat_desc.max_length,p=0;for(i=0;i<=m;i++)t.bl_count[i]=0;for(l[2*t.heap[t.heap_max]+1]=0,a=t.heap_max+1;a<g;a++)c<(i=l[2*l[2*(n=t.heap[a])+1]+1]+1)&&(i=c,p++),l[2*n+1]=i,o<n||(t.bl_count[i]++,s=0,f<=n&&(s=u[n-f]),h=l[2*n],t.opt_len+=h*(i+s),d&&(t.static_len+=h*(_[2*n+1]+s)));if(0!==p){do{for(i=c-1;0===t.bl_count[i];)i--;t.bl_count[i]--,t.bl_count[i+1]+=2,t.bl_count[c]--,p-=2}while(0<p);for(i=c;0!==i;i--)for(n=t.bl_count[i];0!==n;)o<(r=t.heap[--a])||(l[2*r+1]!==i&&(t.opt_len+=(i-l[2*r+1])*l[2*r],l[2*r+1]=i),n--)}}(t,e),F(i,o,t.bl_count)}function V(t,e,a){var n,r,i=-1,s=e[1],h=0,l=7,o=4;for(0===s&&(l=138,o=3),e[2*(a+1)+1]=65535,n=0;n<=a;n++)r=s,s=e[2*(n+1)+1],++h<l&&r===s||(h<o?t.bl_tree[2*r]+=h:0!==r?(r!==i&&t.bl_tree[2*r]++,t.bl_tree[2*v]++):h<=10?t.bl_tree[2*w]++:t.bl_tree[2*y]++,i=r,(h=0)===s?(l=138,o=3):r===s?(l=6,o=3):(l=7,o=4))}function W(t,e,a){var n,r,i=-1,s=e[1],h=0,l=7,o=4;for(0===s&&(l=138,o=3),n=0;n<=a;n++)if(r=s,s=e[2*(n+1)+1],!(++h<l&&r===s)){if(h<o)for(;R(t,r,t.bl_tree),0!=--h;);else 0!==r?(r!==i&&(R(t,r,t.bl_tree),h--),R(t,v,t.bl_tree),N(t,h-3,2)):h<=10?(R(t,w,t.bl_tree),N(t,h-3,3)):(R(t,y,t.bl_tree),N(t,h-11,7));i=r,(h=0)===s?(l=138,o=3):r===s?(l=6,o=3):(l=7,o=4)}}n(O);var X=!1;function Y(t,e,a,n){var r,i,s,h;N(t,(_<<1)+(n?1:0),3),i=e,s=a,h=!0,M(r=t),h&&(L(r,s),L(r,~s)),l.arraySet(r.pending_buf,r.window,i,s,r.pending),r.pending+=s}a._tr_init=function(t){X||(function(){var t,e,a,n,r,i=new Array(m+1);for(n=a=0;n<s-1;n++)for(E[n]=a,t=0;t<1<<k[n];t++)j[a++]=n;for(j[a-1]=n,n=r=0;n<16;n++)for(O[n]=r,t=0;t<1<<z[n];t++)S[r++]=n;for(r>>=7;n<f;n++)for(O[n]=r<<7,t=0;t<1<<z[n]-7;t++)S[256+r++]=n;for(e=0;e<=m;e++)i[e]=0;for(t=0;t<=143;)A[2*t+1]=8,t++,i[8]++;for(;t<=255;)A[2*t+1]=9,t++,i[9]++;for(;t<=279;)A[2*t+1]=7,t++,i[7]++;for(;t<=287;)A[2*t+1]=8,t++,i[8]++;for(F(A,u+1,i),t=0;t<f;t++)C[2*t+1]=5,C[2*t]=H(t,5);U=new q(A,k,d+1,u,m),D=new q(C,z,0,f,m),I=new q(new Array(0),x,0,c,p)}(),X=!0),t.l_desc=new i(t.dyn_ltree,U),t.d_desc=new i(t.dyn_dtree,D),t.bl_desc=new i(t.bl_tree,I),t.bi_buf=0,t.bi_valid=0,K(t)},a._tr_stored_block=Y,a._tr_flush_block=function(t,e,a,n){var r,i,s=0;0<t.level?(2===t.strm.data_type&&(t.strm.data_type=function(t){var e,a=4093624447;for(e=0;e<=31;e++,a>>>=1)if(1&a&&0!==t.dyn_ltree[2*e])return h;if(0!==t.dyn_ltree[18]||0!==t.dyn_ltree[20]||0!==t.dyn_ltree[26])return o;for(e=32;e<d;e++)if(0!==t.dyn_ltree[2*e])return o;return h}(t)),Q(t,t.l_desc),Q(t,t.d_desc),s=function(t){var e;for(V(t,t.dyn_ltree,t.l_desc.max_code),V(t,t.dyn_dtree,t.d_desc.max_code),Q(t,t.bl_desc),e=c-1;3<=e&&0===t.bl_tree[2*B[e]+1];e--);return t.opt_len+=3*(e+1)+5+5+4,e}(t),r=t.opt_len+3+7>>>3,(i=t.static_len+3+7>>>3)<=r&&(r=i)):r=i=a+5,a+4<=r&&-1!==e?Y(t,e,a,n):4===t.strategy||i===r?(N(t,2+(n?1:0),3),J(t,A,C)):(N(t,4+(n?1:0),3),function(t,e,a,n){var r;for(N(t,e-257,5),N(t,a-1,5),N(t,n-4,4),r=0;r<n;r++)N(t,t.bl_tree[2*B[r]+1],3);W(t,t.dyn_ltree,e-1),W(t,t.dyn_dtree,a-1)}(t,t.l_desc.max_code+1,t.d_desc.max_code+1,s+1),J(t,t.dyn_ltree,t.dyn_dtree)),K(t),n&&M(t)},a._tr_tally=function(t,e,a){return t.pending_buf[t.d_buf+2*t.last_lit]=e>>>8&255,t.pending_buf[t.d_buf+2*t.last_lit+1]=255&e,t.pending_buf[t.l_buf+t.last_lit]=255&a,t.last_lit++,0===e?t.dyn_ltree[2*a]++:(t.matches++,e--,t.dyn_ltree[2*(j[a]+d+1)]++,t.dyn_dtree[2*T(e)]++),t.last_lit===t.lit_bufsize-1},a._tr_align=function(t){var e;N(t,2,3),R(t,b,A),16===(e=t).bi_valid?(L(e,e.bi_buf),e.bi_buf=0,e.bi_valid=0):8<=e.bi_valid&&(e.pending_buf[e.pending++]=255&e.bi_buf,e.bi_buf>>=8,e.bi_valid-=8)}},{"../utils/common":1}],8:[function(t,e,a){"use strict";e.exports=function(){this.input=null,this.next_in=0,this.avail_in=0,this.total_in=0,this.output=null,this.next_out=0,this.avail_out=0,this.total_out=0,this.msg="",this.state=null,this.data_type=2,this.adler=0}},{}],"/lib/deflate.js":[function(t,e,a){"use strict";var s=t("./zlib/deflate"),h=t("./utils/common"),l=t("./utils/strings"),r=t("./zlib/messages"),i=t("./zlib/zstream"),o=Object.prototype.toString,_=0,d=-1,u=0,f=8;function c(t){if(!(this instanceof c))return new c(t);this.options=h.assign({level:d,method:f,chunkSize:16384,windowBits:15,memLevel:8,strategy:u,to:""},t||{});var e=this.options;e.raw&&0<e.windowBits?e.windowBits=-e.windowBits:e.gzip&&0<e.windowBits&&e.windowBits<16&&(e.windowBits+=16),this.err=0,this.msg="",this.ended=!1,this.chunks=[],this.strm=new i,this.strm.avail_out=0;var a=s.deflateInit2(this.strm,e.level,e.method,e.windowBits,e.memLevel,e.strategy);if(a!==_)throw new Error(r[a]);if(e.header&&s.deflateSetHeader(this.strm,e.header),e.dictionary){var n;if(n="string"==typeof e.dictionary?l.string2buf(e.dictionary):"[object ArrayBuffer]"===o.call(e.dictionary)?new Uint8Array(e.dictionary):e.dictionary,(a=s.deflateSetDictionary(this.strm,n))!==_)throw new Error(r[a]);this._dict_set=!0}}function n(t,e){var a=new c(e);if(a.push(t,!0),a.err)throw a.msg||r[a.err];return a.result}c.prototype.push=function(t,e){var a,n,r=this.strm,i=this.options.chunkSize;if(this.ended)return!1;n=e===~~e?e:!0===e?4:0,"string"==typeof t?r.input=l.string2buf(t):"[object ArrayBuffer]"===o.call(t)?r.input=new Uint8Array(t):r.input=t,r.next_in=0,r.avail_in=r.input.length;do{if(0===r.avail_out&&(r.output=new h.Buf8(i),r.next_out=0,r.avail_out=i),1!==(a=s.deflate(r,n))&&a!==_)return this.onEnd(a),!(this.ended=!0);0!==r.avail_out&&(0!==r.avail_in||4!==n&&2!==n)||("string"===this.options.to?this.onData(l.buf2binstring(h.shrinkBuf(r.output,r.next_out))):this.onData(h.shrinkBuf(r.output,r.next_out)))}while((0<r.avail_in||0===r.avail_out)&&1!==a);return 4===n?(a=s.deflateEnd(this.strm),this.onEnd(a),this.ended=!0,a===_):2!==n||(this.onEnd(_),!(r.avail_out=0))},c.prototype.onData=function(t){this.chunks.push(t)},c.prototype.onEnd=function(t){t===_&&("string"===this.options.to?this.result=this.chunks.join(""):this.result=h.flattenChunks(this.chunks)),this.chunks=[],this.err=t,this.msg=this.strm.msg},a.Deflate=c,a.deflate=n,a.deflateRaw=function(t,e){return(e=e||{}).raw=!0,n(t,e)},a.gzip=function(t,e){return(e=e||{}).gzip=!0,n(t,e)}},{"./utils/common":1,"./utils/strings":2,"./zlib/deflate":5,"./zlib/messages":6,"./zlib/zstream":8}]},{},[])("/lib/deflate.js")});

)TPPAKO";

static const char ethTp3c1EncoderLibrary[] PROGMEM = R"TP3C1JS(
(function(g){
'use strict';
const ALPH='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.';
const PREFIX='tp3000://verify#';
const te=new TextEncoder();
function fail(s){throw new Error('TP3C1: '+s)}
function append(o,a){for(let i=0;i<a.length;i++)o.push(a[i])}
function vu(v){v=Number(v);if(!Number.isSafeInteger(v)||v<0)fail('ungültiger VarUInt');let o=[];while(v>=128){o.push((v%128)|128);v=Math.floor(v/128)}o.push(v);return o}
function zz(v){v=Number(v);if(!Number.isSafeInteger(v))fail('ungültiger ZigZag-Wert');return v>=0?v*2:(-v)*2-1}
function fu(n,v){let o=vu(n*8);append(o,vu(v));return o}
function fb(n,b){b=b instanceof Uint8Array?b:Uint8Array.from(b);let o=vu(n*8+2);append(o,vu(b.length));append(o,b);return o}
function ft(n,s){return fb(n,te.encode(String(s??'')))}
function b64(s){s=String(s||'');let r;try{r=atob(s)}catch(e){fail('ungültiges Base64')}let a=new Uint8Array(r.length);for(let i=0;i<r.length;i++)a[i]=r.charCodeAt(i);return a}
function b64url(s){s=String(s||'').replace(/-/g,'+').replace(/_/g,'/');while(s.length%4)s+='=';return b64(s)}
function hex(s){s=String(s||'');if(s.length%2||!/^[0-9a-fA-F]*$/.test(s))fail('ungültiges Hex');let a=new Uint8Array(s.length/2);for(let i=0;i<a.length;i++)a[i]=parseInt(s.slice(i*2,i*2+2),16);return a}
function isoUnix(s){let ms=Date.parse(String(s||''));if(!Number.isFinite(ms)||ms%1000)fail('ungültige UTC-Zeit');return Math.floor(ms/1000)}
function spkiXY(s){let a=b64(s);if(a.length<65||a[a.length-65]!==4)fail('P-256-SPKI ungültig');return a.slice(a.length-64)}
function semver(s){let p=String(s||'').split('.').map(Number);if(p.length!==3||p.some(x=>!Number.isInteger(x)||x<0||x>1023))fail('Firmwareversion ungültig');return p}
function buildNum(s){let m=String(s||'').match(/_(\d+)$/);if(!m)fail('Build-ID ungültig');return Number(m[1])}
function stdId(kind,x){let y=Number(x.Approval.CalibrationDateYmd),rid=String(x.SourceRequestId||'').slice(0,8).toUpperCase();if(kind==='device')return'DCAL-G'+x.DeviceSerial+'-'+String(y).padStart(8,'0')+'-'+rid;if(kind==='head')return'HCAL-K'+String(x.Values.HeadSerial).padStart(5,'0')+'-'+String(y).padStart(8,'0')+'-'+rid;return'SCAL-G'+x.DeviceSerial+'-K'+String(x.Values.HeadSerial).padStart(5,'0')+'-'+String(y).padStart(8,'0')+'-'+rid}
const CALRES_ENCODING='TP3000-CAL-POINTS-1-BASE64URL';
const CALRES_SCOPE={NONE:0,AS_FOUND:1,AS_LEFT:2,AS_FOUND_AS_LEFT_NO_ADJUSTMENT:4,BEFORE_AFTER_ADJUSTMENT:5};
function calibrationResults(a,kind){let scope=String(a.CalibrationScope||'NONE').trim().toUpperCase();if(!(scope in CALRES_SCOPE))fail(kind+': Kalibrierumfang nicht unterstützt');let code=CALRES_SCOPE[scope],enc=String(a.CalibrationResultsEncoding||''),payload=String(a.CalibrationResultsBase64Url||'');if(code===0){if(enc||payload)fail(kind+': NONE darf keine Kalibriermessdaten enthalten');return{scope:0,raw:new Uint8Array(0)}}if(enc!==CALRES_ENCODING)fail(kind+': Kalibriermessdaten-Kodierung unbekannt');if(!payload)fail(kind+': Kalibriermessdaten fehlen');let raw=b64url(payload);if(raw.length<8||(raw.length-8)%24!==0)fail(kind+': Kalibriermessdatenlänge ungültig');if(raw[0]!==84||raw[1]!==80||raw[2]!==67||raw[3]!==49)fail(kind+': TPC1-Kennung fehlt');if(raw[4]!==code)fail(kind+': Kalibrierumfang und TPC1-Daten stimmen nicht überein');let count=raw[5];if(count<1||count>40||raw.length!==8+count*24)fail(kind+': Punktanzahl ungültig');if(raw[6]!==0||raw[7]!==0)fail(kind+': reservierte TPC1-Bytes sind nicht null');return{scope:code,raw}}
function approvalBlock(x,kind,role){let a=x.Approval||{},o=[],lab=String(a.CalibrationLaboratory||''),op=String(a.CalibrationOperator||''),roleLab=role?String(role.LabName||''):'',lm=(role&&lab===roleLab)?0:(lab==='TP-3000 Manufacturer Root'?1:2),om=(role&&op===roleLab)?0:(op===''?1:2);append(o,fu(1,lm));if(lm===2)append(o,ft(2,lab));append(o,fu(3,om));if(om===2)append(o,ft(4,op));if(a.CertificateReference)append(o,ft(5,a.CertificateReference));let reason=String(a.ReasonForCalibration||''),rc=reason==='Regelkalibrierung'?1:0;append(o,fu(6,rc));if(rc===0&&reason)append(o,ft(7,reason));let fields=['ReferenceStandards','Traceability','MeasurementUncertainty','EnvironmentalConditions','CalibrationProcedure','AccreditationInformation'];for(let i=0;i<fields.length;i++)if(a[fields[i]])append(o,ft(8+i,a[fields[i]]));let cr=calibrationResults(a,kind);append(o,fu(14,cr.scope));if(cr.scope!==0)append(o,fb(15,cr.raw));if(kind==='system'){let source=String(a.CertificateSource||'TP3000');if(source!=='TP3000'&&source!=='EXTERNAL_PDF')fail('Kalibrierscheinquelle ungültig');append(o,fu(16,source==='EXTERNAL_PDF'?1:0))}return Uint8Array.from(o)}
function calCommon(x,kind,values,role){let o=[],a=x.Approval||{};append(o,fu(1,Number(x.Values.CalibrationDateYmd)));append(o,fb(2,hex(x.SourceRequestManifestSha256)));append(o,fb(3,hex(x.SourceRequestId)));append(o,fu(4,isoUnix(x.SourceRequestCreatedUtc)));append(o,fu(5,isoUnix(a.ValidFromUtc)));append(o,fu(6,isoUnix(a.ValidUntilUtc)));append(o,fu(7,isoUnix(x.ApprovedUtc)));append(o,fu(8,Number(a.CalibrationIntervalMonths)));append(o,fu(9,buildNum(x.FirmwareBuildId)));append(o,fb(10,approvalBlock(x,kind,role)));append(o,fb(11,b64(x.CalibrationSignatureBase64)));if(String(a.CalibrationId)!==stdId(kind,x))append(o,ft(12,a.CalibrationId));append(o,fu(13,x.SignerType==='CALIBRATION_LAB'?0:1));append(o,fb(14,values));return Uint8Array.from(o)}
function packedSigned(vals){let o=[];for(const v of vals)append(o,vu(zz(Number(v))));return Uint8Array.from(o)}
function sha256(a){const K=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2],H=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];let n=a.length,pl=Math.ceil((n+9)/64)*64,b=new Uint8Array(pl);b.set(a);b[n]=0x80;let bits=n*8,hi=Math.floor(bits/4294967296),lo=bits>>>0;for(let i=0;i<4;i++){b[pl-8+i]=(hi>>>(24-i*8))&255;b[pl-4+i]=(lo>>>(24-i*8))&255}let w=new Uint32Array(64),r=(x,n)=>(x>>>n)|(x<<(32-n));for(let p=0;p<pl;p+=64){for(let i=0;i<16;i++)w[i]=((b[p+i*4]<<24)|(b[p+i*4+1]<<16)|(b[p+i*4+2]<<8)|b[p+i*4+3])>>>0;for(let i=16;i<64;i++){let x=w[i-15],y=w[i-2],s0=r(x,7)^r(x,18)^(x>>>3),s1=r(y,17)^r(y,19)^(y>>>10);w[i]=(w[i-16]+s0+w[i-7]+s1)>>>0}let A=H[0],B=H[1],C=H[2],D=H[3],E=H[4],F=H[5],G=H[6],I=H[7];for(let i=0;i<64;i++){let S1=r(E,6)^r(E,11)^r(E,25),ch=(E&F)^((~E)&G),t1=(I+S1+ch+K[i]+w[i])>>>0,S0=r(A,2)^r(A,13)^r(A,22),maj=(A&B)^(A&C)^(B&C),t2=(S0+maj)>>>0;I=G;G=F;F=E;E=(D+t1)>>>0;D=C;C=B;B=A;A=(t1+t2)>>>0}H[0]=(H[0]+A)>>>0;H[1]=(H[1]+B)>>>0;H[2]=(H[2]+C)>>>0;H[3]=(H[3]+D)>>>0;H[4]=(H[4]+E)>>>0;H[5]=(H[5]+F)>>>0;H[6]=(H[6]+G)>>>0;H[7]=(H[7]+I)>>>0}let out=new Uint8Array(32);for(let i=0;i<8;i++)for(let j=0;j<4;j++)out[i*4+j]=(H[i]>>>(24-j*8))&255;return out}
let ct=null;function crc32(a){if(!ct){ct=new Uint32Array(256);for(let n=0;n<256;n++){let c=n;for(let k=0;k<8;k++)c=(c&1)?(0xedb88320^(c>>>1)):(c>>>1);ct[n]=c>>>0}}let c=0xffffffff;for(let i=0;i<a.length;i++)c=ct[(c^a[i])&255]^(c>>>8);return(c^0xffffffff)>>>0}
function base38(data){let leading=0;while(leading<data.length&&data[leading]===0)leading++;let a=Array.from(data),start=leading,out='';while(start<a.length){let rem=0;for(let i=start;i<a.length;i++){let x=rem*256+a[i];a[i]=Math.floor(x/38);rem=x%38}out=ALPH[rem]+out;while(start<a.length&&a[start]===0)start++}return'0'.repeat(leading)+(out||'')}
function be16(o,v){o.push((v>>>8)&255,v&255)}function be32(o,v){o.push((v>>>24)&255,(v>>>16)&255,(v>>>8)&255,v&255)}
function sameText(a,b){return String(a??'').toUpperCase()===String(b??'').toUpperCase()}
function signerType(x,label){let t=String(x?.SignerType||'');if(t!=='CALIBRATION_LAB'&&t!=='MANUFACTURER_ROOT')fail(label+': Signierertyp nicht unterstützt');return t}
function selectRole(d,h,s){let role=null;for(const [label,p] of [['Gerätejustierung',d],['Kopfjustierung',h],['Systemkalibrierung',s]]){if(signerType(p,label)!=='CALIBRATION_LAB')continue;let r=p?.SignerRoleCertificate||null;if(!r)fail(label+': Laborzertifikat fehlt');if(r.Format!=='TP3000-SIGNING-ROLE-CERTIFICATE-2')fail(label+': Laborzertifikatformat nicht unterstützt');if(!sameText(r.KeyId,p.SignerKeyId))fail(label+': Laborzertifikat passt nicht zur Signatur');if(role&&!sameText(role.ManifestSha256,r.ManifestSha256))fail('Mehrere unterschiedliche Laborzertifikate werden in TP3C1 V0.4 nicht unterstützt');role=role||r}return role}
function assertInputs(d,h,s,cert,role){if(!d||!h||!s)fail('Geräte-, Kopf- und Systempaket erforderlich');if(d.Format!=='TP3000-DEVICE-CALIBRATION-3')fail('Gerätejustierung V3 erforderlich');if(h.Format!=='TP3000-HEAD-CALIBRATION-3')fail('Kopfjustierung V3 erforderlich');if(s.Format!=='TP3000-SYSTEM-CALIBRATION-2')fail('Systemkalibrierung V2 erforderlich');signerType(d,'Gerätejustierung');signerType(h,'Kopfjustierung');signerType(s,'Systemkalibrierung');if(!cert)fail('Aktives Gerätezertifikat konnte nicht geladen werden');if(cert.Format!=='TP3000-DEVICE-CERTIFICATE-2')fail('Gerätezertifikatformat nicht unterstützt');if(d.FirmwareVersion!==h.FirmwareVersion||d.FirmwareVersion!==s.FirmwareVersion)fail('Firmware-SemVer nicht gemeinsam');let ds=String(d.DeviceSerial||'').padStart(5,'0'),hs=String(h.DeviceSerial||'').padStart(5,'0'),ss=String(s.DeviceSerial||'').padStart(5,'0'),cs=String(cert.DeviceSerial||'').padStart(5,'0');if(ds!==hs||ds!==ss||ds!==cs)fail('Gerätezertifikat passt nicht zur Kalibrierkombination');let serial=String(cert.CertificateSerial||'');if(!sameText(serial,d.DeviceCertificateSerial)||!sameText(serial,h.DeviceCertificateSerial)||!sameText(serial,s.DeviceCertificateSerial))fail('Gerätezertifikat-SN passt nicht zu den Kalibrierpaketen');if(role){for(const [label,p] of [['Gerätejustierung',d],['Kopfjustierung',h],['Systemkalibrierung',s]])if(p.SignerType==='CALIBRATION_LAB'&&!sameText(role.KeyId,p.SignerKeyId))fail(label+': Laborzertifikat passt nicht zur Signatur')}}
function firmwareContextBlock(c,s){if(!c||!c.present||!c.recordValid||!c.requestBindingValid)return null;let requestId=String(s?.SourceRequestId||''),requestHash=String(s?.SourceRequestManifestSha256||'');if(!sameText(requestId,c.sourceRequestId||requestId)||!sameText(requestHash,c.sourceRequestManifestSha256||requestHash))fail('Firmwarekontext passt nicht zur Systemkalibrierungsanfrage');let cm=String(c.contextManifestSha256||''),sig=String(c.deviceSignatureHex||'');if(!/^[0-9A-Fa-f]{64}$/.test(cm)||!/^[0-9A-Fa-f]{128}$/.test(sig))fail('Firmwarekontext-Signaturdaten fehlen');let mode=c.captureMode==='REQUEST'?1:(c.captureMode==='ACTIVATION_FALLBACK'?2:0),ms=({NOT_CERTIFIED:0,VALID:1,MISMATCH:2,INVALID:3})[String(c.manufacturerStatus||'')]??0,o=[];append(o,fu(1,1));append(o,fu(2,mode));append(o,fu(3,Number(c.capturedUtc||0)));append(o,ft(4,c.firmwareVersion||''));append(o,ft(5,c.firmwareBuildId||''));append(o,fu(6,Number(c.imageBase||0)));append(o,fu(7,Number(c.imageSize||0)));append(o,fb(8,hex(c.firmwareSha256||'')));append(o,fu(9,ms));if(c.firmwareCertificateId)append(o,ft(10,c.firmwareCertificateId));if(c.manufacturerRootKeyId)append(o,ft(11,c.manufacturerRootKeyId));if(Number(c.certificateApprovedUtc||0)>0)append(o,fu(12,Number(c.certificateApprovedUtc)));append(o,fb(13,hex(requestId)));append(o,fb(14,hex(requestHash)));append(o,fb(15,hex(cm)));append(o,fb(16,hex(sig)));return Uint8Array.from(o)}
async function build(device,head,system,activeCertificate,firmwareContext){let cert=activeCertificate,role=selectRole(device,head,system);assertInputs(device,head,system,cert,role);let o=[],id=[];append(id,fu(1,Number(cert.DeviceSerial)));append(id,fb(2,spkiXY(cert.DevicePublicKeySpkiBase64)));append(id,fb(3,hex(cert.CertificateSerial)));append(id,fu(4,isoUnix(cert.IssuedUtc)));append(id,fb(5,hex(cert.RequestId)));append(id,fb(6,hex(cert.RequestManifestSha256)));append(id,fb(7,b64(cert.RootSignatureBase64)));let rb=[];if(role){append(rb,fb(1,spkiXY(role.PublicKeySpkiBase64)));append(rb,fb(2,hex(role.CertificateSerial)));append(rb,fu(3,isoUnix(role.IssuedUtc)));append(rb,fu(4,isoUnix(role.NotBeforeUtc)));append(rb,fu(5,isoUnix(role.NotAfterUtc)));append(rb,ft(6,role.LabId));append(rb,ft(7,role.LabName));if(role.LabAddress)append(rb,ft(8,role.LabAddress));append(rb,fb(9,b64(role.RootSignatureBase64)));let dk=String(role.LabId)+' / '+String(role.LabName);if(String(role.KeyName)!==dk)append(rb,ft(10,role.KeyName));if(String(role.Permissions)!=='SIGN_CALIBRATION')append(rb,ft(11,role.Permissions));}let dv=device.Values,dp=packedSigned(['RefLowScaled','RefHighScaled','ChannelALowCorrectionScaled','ChannelAHighCorrectionScaled','ChannelBLowCorrectionScaled','ChannelBHighCorrectionScaled'].map(k=>dv[k])),dvo=[];append(dvo,fu(1,1));append(dvo,fu(2,Number(dv.ReferenceDateYmd)));append(dvo,fb(3,dp));let hv=head.Values,flags=(hv.Mirror2PointActive?1:0)|(hv.Ambient2PointActive?2:0),order=['R0MirrorScaled','R0AmbientScaled','MirrorSet1_mC','MirrorActual1_mC','MirrorSet2_mC','MirrorActual2_mC','AmbientSet1_mC','AmbientActual1_mC','AmbientSet2_mC','AmbientActual2_mC','DewFrostOffset_mC','PidKp','PidKi_x1000','PidKd_x1000','ControlInterval_ms','HBridgeDeadtime_ms','FanPercent','OpticalTarget_x10','PeltierCurrentLimit_mA'],hp=packedSigned(order.map(k=>hv[k])),hvo=[],tc=hv.HeadType==='STP-3001'?1:0;append(hvo,fu(1,1));append(hvo,fu(2,tc));append(hvo,fu(3,Number(hv.HeadSerial)));append(hvo,fu(4,Number(hv.CalibrationTimeHms)));append(hvo,fu(5,flags));append(hvo,fb(6,hp));if(tc===0)append(hvo,ft(7,hv.HeadType));let sv=system.Values||{},svo=[];append(svo,fu(1,2));{let extAvailable=Number(sv.ExternalCertificateAvailable||0)?1:0;append(svo,fu(2,extAvailable));if(extAvailable){append(svo,ft(3,sv.ExternalCertificateDocumentId||''));append(svo,ft(4,sv.ExternalCertificateNumber||''));append(svo,fu(5,Number(sv.ExternalCertificateValidFromYmd||0)));append(svo,fu(6,Number(sv.ExternalCertificateValidUntilYmd||0)));append(svo,ft(7,sv.ExternalCertificateOriginalFileName||''));append(svo,fu(8,Number(sv.ExternalCertificateFileSize||0)));append(svo,fb(9,hex(sv.ExternalCertificateSha256||'')))}}let p=semver(device.FirmwareVersion),sempack=p[0]*1048576+p[1]*1024+p[2],fcb=firmwareContextBlock(firmwareContext,system),features=63|(fcb?192:0);append(o,fu(1,1));append(o,fu(2,2));append(o,fu(3,features));append(o,fu(4,sempack));append(o,fb(5,Uint8Array.from(id)));if(role)append(o,fb(6,Uint8Array.from(rb)));append(o,fb(7,calCommon(device,'device',Uint8Array.from(dvo),role)));append(o,fb(8,calCommon(head,'head',Uint8Array.from(hvo),role)));append(o,fb(9,calCommon(system,'system',Uint8Array.from(svo),role)));if(fcb){append(o,fb(10,fcb));let app=String(firmwareContext?.applicability||'NOT_COMPARABLE'),appCode=app==='EXACT_MATCH'?1:(app==='FIRMWARE_CHANGED'?2:0);append(o,fu(11,appCode));let currentHash=String(firmwareContext?.currentFirmwareSha256||'');if(/^[0-9A-Fa-f]{64}$/.test(currentHash))append(o,fb(12,hex(currentHash)))}let envelope=Uint8Array.from(o);if(!g.pako||typeof g.pako.deflate!=='function')fail('PAKO-Deflate fehlt');let compressed=Uint8Array.from(g.pako.deflate(envelope,{level:9})),transport=compressed.length<envelope.length?compressed:envelope,flagsOut=transport===compressed?1:0,digest=sha256(envelope),half=Math.floor((transport.length+1)/2),frags=[transport.slice(0,half),transport.slice(half)],parts=[];for(let i=0;i<2;i++){let frag=frags[i],hdr=[flagsOut];append(hdr,digest.slice(0,8));hdr.push(0x40+i+1);be16(hdr,transport.length);be16(hdr,frag.length);be32(hdr,crc32(frag));let binary=new Uint8Array(hdr.length+frag.length);binary.set(hdr);binary.set(frag,hdr.length);let raw='TP3C1:'+base38(binary);parts.push({index:i+1,fragment:frag,binary,rawText:raw,deepLink:PREFIX+raw})}return{envelope,compressed,transport,compressedUsed:!!flagsOut,documentId:Array.from(digest.slice(0,8),x=>x.toString(16).padStart(2,'0')).join('').toUpperCase(),parts}}
g.TP3C1={build,base38,crc32,sha256};
})(typeof window!=='undefined'?window:globalThis);
)TP3C1JS";

static const char ethCalibrationCertificatePart1[] PROGMEM = R"TPCALSHEET1(
const E=id=>document.getElementById(id),DE=0,EN=1;
let storedCertificateLanguage='';try{storedCertificateLanguage=localStorage.getItem('tp3000-cal-certificate-language')||''}catch(_){}let lang=storedCertificateLanguage==='en'?EN:DE;
let certificateNumber='—',certificateState=null,renderSequence=0;
const tx=(de,en)=>lang===EN?en:de;
const esc=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fmtYmd=v=>{v=String(v||'');if(v.length!==8)return'—';return lang===EN?v.slice(0,4)+'-'+v.slice(4,6)+'-'+v.slice(6,8):v.slice(6,8)+'.'+v.slice(4,6)+'.'+v.slice(0,4)};
const fmtUtc=v=>{if(!v)return'—';let d=new Date(v);return isNaN(d)?esc(v):d.toLocaleString(lang===EN?'en-GB':'de-DE',{timeZone:'UTC',year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit'})+' UTC'};
const fmtEpochUtc=v=>{v=Number(v)||0;if(v<=0)return'—';let d=new Date(v*1000),x=d.toISOString();return lang===EN?x.slice(0,10)+' '+x.slice(11,19)+' UTC':x.slice(8,10)+'.'+x.slice(5,7)+'.'+x.slice(0,4)+' '+x.slice(11,19)+' UTC'};
const dec=v=>lang===EN?String(v):String(v).replace('.',',');
const num=(v,scale,digits,unit)=>Number.isFinite(Number(v))?dec((Number(v)/scale).toFixed(digits))+(unit?' '+unit:''):'—';
const shown=v=>{if(v===null||v===undefined)return'—';let x=String(v);return x.trim()?x:'—'};
const row=(a,b,mono=false)=>'<tr><td>'+esc(a)+'</td><td'+(mono?' class="mono"':'')+'>'+esc(shown(b))+'</td></tr>';
const rowHtml=(a,b,cls='')=>'<tr><td>'+esc(a)+'</td><td'+(cls?' class="'+esc(cls)+'"':'')+'>'+b+'</td></tr>';
const table=rows=>'<table class="rows">'+rows+'</table>';
const signer=p=>{let r=p?.SignerRoleCertificate,a=p?.Approval||{};return{type:p?.SignerType||'—',key:p?.SignerKeyId||'—',name:r?.LabName||a.CalibrationLaboratory||tx('Hersteller','Manufacturer'),id:r?.LabId||'',address:r?.LabAddress||'',cert:r?.CertificateSerial||'',operator:a.CalibrationOperator||'—'}};
const profFields=[
  ['Verwendete Referenznormale','Reference standards','ReferenceStandards'],
  ['Rückführbarkeit','Traceability','Traceability'],
  ['Messunsicherheit','Measurement uncertainty','MeasurementUncertainty'],
  ['Umgebungsbedingungen','Environmental conditions','EnvironmentalConditions'],
  ['Kalibrierverfahren','Calibration procedure','CalibrationProcedure'],
  ['Akkreditierungsangaben','Accreditation information','AccreditationInformation']
];
function isExternalSystem(system){return String(system?.Approval?.CertificateSource||'TP3000')==='EXTERNAL_PDF'}
function reasonText(c){let de={HEAD_VALUES_CHANGED:'Kopfjustierwerte geändert',DEVICE_VALUES_CHANGED:'Gerätejustierwerte geändert',HEAD_ADJUSTMENT_CHANGED:'Kopfjustierung geändert',DEVICE_ADJUSTMENT_CHANGED:'Gerätejustierung geändert',SYSTEM_CALIBRATION_REPLACED:'durch neue Systemkalibrierung ersetzt',EXTERNAL_CERTIFICATE_CHANGED:'externer Kalibrierschein geändert',CONFIGURATION_CHANGED:'Kalibrierkonfiguration geändert'},en={HEAD_VALUES_CHANGED:'head adjustment values changed',DEVICE_VALUES_CHANGED:'device adjustment values changed',HEAD_ADJUSTMENT_CHANGED:'head adjustment changed',DEVICE_ADJUSTMENT_CHANGED:'device adjustment changed',SYSTEM_CALIBRATION_REPLACED:'replaced by a new system calibration',EXTERNAL_CERTIFICATE_CHANGED:'external calibration certificate changed',CONFIGURATION_CHANGED:'calibration configuration changed'};return(lang===EN?en:de)[String(c||'')]||String(c||'—')}
function professionalTitle(system){return isExternalSystem(system)?tx('Externer Kalibrierschein','External calibration certificate'):tx('Kalibriertechnische Angaben','Calibration details')}
function professional(system){let a=system?.Approval||{},v=system?.Values||{};if(isExternalSystem(system)){let items=[[tx('Kalibrierschein-Nr. / Zeichen','Calibration certificate no. / reference'),v.ExternalCertificateNumber],[tx('Gültig von','Valid from'),fmtYmd(v.ExternalCertificateValidFromYmd)],[tx('Gültig bis','Valid until'),fmtYmd(v.ExternalCertificateValidUntilYmd)],[tx('Originaldatei','Original file'),v.ExternalCertificateOriginalFileName],[tx('PDF-Dokument-ID','PDF document ID'),v.ExternalCertificateDocumentId],[tx('PDF-Größe','PDF size'),String(v.ExternalCertificateFileSize||0)+' Byte'],['PDF SHA-256',v.ExternalCertificateSha256]];return'<div class="professionalGrid">'+items.map((x,i)=>'<div class="field'+(i>=4?' full':'')+'"><div class="label">'+esc(x[0])+'</div><div class="value'+(i>=4?' mono':'')+'">'+esc(shown(x[1]))+'</div></div>').join('')+'</div>'}let items=[];for(let i=0;i<profFields.length;i++){let [de,en,key]=profFields[i];items.push('<div class="field'+(i>=4?' full':'')+'"><div class="label">'+esc(tx(de,en))+'</div><div class="value">'+esc(shown(a[key]))+'</div></div>')}return'<div class="professionalGrid">'+items.join('')+'</div>'}
function issuerSection(packages){let seen=new Set(),blocks=[];for(const p of packages){if(!p)continue;let s=signer(p),a=p.Approval||{},k=[s.name,s.id,s.key,s.operator].join('|');if(seen.has(k))continue;seen.add(k);let r=row(tx('Zertifikatsaussteller','Certificate issuer'),s.name)+row(tx('Bearbeiter','Operator'),s.operator)+row(tx('Labor-/Schlüssel-ID','Laboratory / key ID'),s.id?s.id+' / '+s.key:s.key,true)+row(tx('Anschrift','Address'),s.address)+row(tx('Signierer-Typ','Signer type'),s.type,true)+row(tx('Zertifikatsreferenz','Certificate reference'),a.CertificateReference);blocks.push(table(r))}return'<section class="topic issuerSection"><h2>'+tx('Zertifikatsaussteller und Dokument','Certificate issuer and document')+'</h2>'+blocks.join('')+'</section>'}
function scopeText(scope){let de={AS_FOUND:'Nur As Found',AS_LEFT:'Nur As Left',AS_FOUND_AS_LEFT:'Vor und nach Justierung (Altformat)',AS_FOUND_AS_LEFT_NO_ADJUSTMENT:'As Found / As Left – ohne Justierung',BEFORE_AFTER_ADJUSTMENT:'Vor und nach Justierung'},en={AS_FOUND:'As Found only',AS_LEFT:'As Left only',AS_FOUND_AS_LEFT:'Before and after adjustment (legacy format)',AS_FOUND_AS_LEFT_NO_ADJUSTMENT:'As Found / As Left – without adjustment',BEFORE_AFTER_ADJUSTMENT:'Before and after adjustment'};return(lang===EN?en:de)[String(scope||'')]||'—'}
function externalPdfLink(p,available){let id=String(p?.Values?.ExternalCertificateDocumentId||'');return available&&id?'<a class="btn pdfLink" target="_blank" rel="noopener" href="/calibration-external-pdf?id='+encodeURIComponent(id)+'&view=1">'+tx('Zugehöriges externes PDF-Kalibrierzertifikat öffnen','Open associated external PDF calibration certificate')+'</a>':tx('Externes Original-PDF nicht verfügbar','External original PDF not available')}
function firmwareManufacturerText(c){let x=String(c?.manufacturerStatus||'');if(x==='VALID')return tx('Herstellerfreigabe gültig – Integrität bestätigt','Manufacturer approval valid – integrity confirmed');if(x==='MISMATCH')return tx('Herstellerzertifikat passt nicht zum Firmwareabbild – Bewertung durch Betreiber erforderlich','Manufacturer certificate does not match the firmware image – operator assessment required');if(x==='INVALID')return tx('Herstellerzertifikat ungültig – Bewertung durch Betreiber erforderlich','Manufacturer certificate invalid – operator assessment required');return tx('Kein passendes Herstellerzertifikat – Validierung, Freigabe und Dokumentation durch Betreiber','No matching manufacturer certificate – validation, approval and documentation by the operator')}
function firmwareContextRows(p,c){let x='',version=String(c?.firmwareVersion||p?.FirmwareVersion||''),build=String(c?.firmwareBuildId||p?.FirmwareBuildId||'');x+=row(tx('Firmware bei Systemkalibrierung','Firmware at system calibration'),version+' / '+build);if(c&&c.present&&c.recordValid&&c.requestBindingValid){x+=row(tx('Firmwareabbild','Firmware image'),String(c.imageSize||0)+' Byte')+row('Firmware SHA-256',c.firmwareSha256,true)+row(tx('Firmwarestatus bei Kalibrierung','Firmware status at calibration'),firmwareManufacturerText(c));x+=row(tx('Firmwarezertifikat-ID','Firmware certificate ID'),c.firmwareCertificateId,true)+row(tx('Hersteller-Root','Manufacturer root'),c.manufacturerRootKeyId,true);let mode=c.captureMode==='ACTIVATION_FALLBACK'?tx('bei Aktivierung nachgetragen','added during activation'):tx('bei Systemkalibrierungsanfrage erfasst','captured with the system calibration request');x+=row(tx('Firmwarekontext','Firmware context'),fmtEpochUtc(c.capturedUtc)+' · '+mode)+row(tx('Firmwarekontext-Manifest','Firmware context manifest'),c.contextManifestSha256||'—',true);if(c.currentHashAvailable)x+=rowHtml(tx('Anwendbarkeit auf aktuellen Gerätezustand','Applicability to current device state'),c.currentImageMatches?'<span class="ok">'+tx('Bestätigt – aktuelle Firmware entspricht exakt dem Kalibrierstand','Confirmed – current firmware exactly matches the calibration state')+'</span>':'<span class="warn">'+tx('Kalibrierschein gültig – aktuelle Firmware weicht vom Kalibrierstand ab','Calibration certificate valid – current firmware differs from the calibration state')+'</span>')}else{x+=row('Firmware SHA-256',tx('nicht im historischen Datensatz enthalten','not included in the historical record'))+rowHtml(tx('Anwendbarkeit auf aktuellen Gerätezustand','Applicability to current device state'),'<span class="warn">'+tx('Nicht beurteilbar','Cannot be assessed')+'</span>')}return x}
function systemSection(p,externalPdfAvailable=false,applicability=null,firmwareContext=null){if(!p)return'';let a=p.Approval||{},v=p.Values||{},s=signer(p),r='',m='',external=isExternalSystem(p),ended=!!(applicability&&applicability.stateKnown&&applicability.ended),validUntil=fmtUtc(a.ValidUntilUtc);r+=row(tx('Systemkalibrierungs-ID','System calibration ID'),a.CalibrationId,true)+row(tx('Geräte-SN','Device serial no.'),p.DeviceSerial,true)+row(tx('Kopftyp','Head type'),v.HeadType,true)+row(tx('Kopf-SN','Head serial no.'),String(v.HeadSerial||0).padStart(5,'0'),true)+row(tx('Kalibrierumfang','Calibration scope'),scopeText(a.CalibrationScope))+row(tx('Kalibrierscheinquelle','Certificate source'),external?tx('Externer PDF-Kalibrierschein','External PDF calibration certificate'):tx('TP-3000-eigener Kalibrierschein','TP-3000 calibration certificate'))+row(tx('Kalibriert am','Calibration date'),fmtYmd(a.CalibrationDateYmd||v.CalibrationDateYmd))+row(tx('Gültig ab','Valid from'),fmtUtc(a.ValidFromUtc));if(ended){r+=rowHtml(tx('Gültig bis','Valid until'),'<span class="superseded">'+esc(validUntil)+'</span>')+rowHtml(tx('Anwendbar bis','Applicable until'),esc(fmtEpochUtc(applicability.applicableUntilUtc)),'applicabilityNote')+rowHtml(tx('Status','Status'),tx('Valide – nicht mehr anwendbar','Valid – no longer applicable'),'applicabilityNote')+rowHtml(tx('Grund','Reason'),esc(reasonText(applicability.reasonCode)),'applicabilityNote')}else r+=row(tx('Gültig bis','Valid until'),validUntil);r+=row(tx('Intervall','Interval'),String(a.CalibrationIntervalMonths||0)+' '+tx('Monate','months'));if(external)r+=row(tx('Kalibrierschein-Nr. / Zeichen','Calibration certificate no. / reference'),v.ExternalCertificateNumber,true)+row(tx('Externer Gültigkeitszeitraum','External validity period'),fmtYmd(v.ExternalCertificateValidFromYmd)+' '+tx('bis','to')+' '+fmtYmd(v.ExternalCertificateValidUntilYmd))+'<tr><td>'+tx('Externes Original-PDF','External original PDF')+'</td><td>'+externalPdfLink(p,externalPdfAvailable)+'</td></tr>';m+=row(tx('Gebundene Gerätejustierung','Bound device adjustment'),v.DeviceAdjustmentCalibrationId,true)+row(tx('Gerätejustierungs-Manifest','Device adjustment manifest'),v.DeviceAdjustmentManifestSha256,true)+row(tx('Gebundene Kopfjustierung','Bound head adjustment'),v.HeadAdjustmentCalibrationId,true)+row(tx('Kopfjustierungs-Manifest','Head adjustment manifest'),v.HeadAdjustmentManifestSha256,true);if(external)m+=row(tx('PDF-Dokument-ID','PDF document ID'),v.ExternalCertificateDocumentId,true)+row(tx('PDF-Datei','PDF file'),v.ExternalCertificateOriginalFileName,true)+row('PDF SHA-256',v.ExternalCertificateSha256,true);m+=row('Signer key ID',s.key,true)+firmwareContextRows(p,firmwareContext)+row(tx('System-Manifest SHA-256','System manifest SHA-256'),p.ManifestSha256,true);let note=external?tx('Das externe Original-PDF bleibt unverändert. Der TP-3000 dokumentiert den bei der Systemkalibrierung verwendeten Firmwarestand separat und gerätesigniert. Eine spätere Änderung des Firmware-SHA-256 macht den Kalibrierschein nicht ungültig; die Abweichung zum dokumentierten Kalibrierstand wird nachvollziehbar gekennzeichnet.','The external original PDF remains unchanged. The TP-3000 separately records and device-signs the firmware state used for the system calibration. A later change to the firmware SHA-256 does not invalidate the calibration certificate; the deviation from the documented calibration state is clearly recorded.'):tx('Das Hersteller-Firmwarezertifikat ist optional. Kundenspezifische Firmware ist zulässig, wenn ihr exakter SHA-256 bei der Systemkalibrierung dokumentiert wird. Eine spätere Änderung des Firmware-SHA-256 macht den Kalibrierschein nicht ungültig; sie wird im Firmwarebezug und in den Nachweisen dokumentiert.','The manufacturer firmware certificate is optional. Customer-specific firmware is permitted when its exact SHA-256 is recorded with the system calibration. A later change to the firmware SHA-256 does not invalidate the calibration certificate; it is documented in the firmware reference and in the evidence records.');return'<section class="card topic"><h2>'+tx('Systemkalibrierung','System calibration')+'</h2><div class="grid"><div>'+table(r)+'</div><div>'+table(m)+'</div></div><div class="foot">'+esc(note)+'</div></section>'}
function deviceSection(p){if(!p)return'';let a=p.Approval||{},v=p.Values||{},s=signer(p);let r='';r+=row(tx('Geräte-SN','Device serial no.'),p.DeviceSerial,true)+row(tx('Gerätezertifikat-SN','Device certificate serial no.'),p.DeviceCertificateSerial,true)+row(tx('Justierungs-ID','Adjustment ID'),a.CalibrationId,true)+row(tx('Justiert am','Adjustment date'),fmtYmd(a.CalibrationDateYmd||v.CalibrationDateYmd))+row(tx('Gültig ab','Valid from'),fmtUtc(a.ValidFromUtc))+row(tx('Gültig bis','Valid until'),fmtUtc(a.ValidUntilUtc))+row(tx('Intervall','Interval'),a.CalibrationIntervalMonths+' '+tx('Monate','months'))+row('Signer key ID',s.key,true)+row(tx('Firmware','Firmware'),String(p.FirmwareVersion||'')+' / '+String(p.FirmwareBuildId||''));let m='';m+=row(tx('Referenzdatum','Reference date'),fmtYmd(v.ReferenceDateYmd))+row('Ref Low',num(v.RefLowScaled,100000,5,'Ω'))+row('Ref High',num(v.RefHighScaled,100000,5,'Ω'))+row(tx('Kanal A / Ref Low','Channel A / Ref Low'),num(v.ChannelALowCorrectionScaled,100000,5,'Ω'))+row(tx('Kanal A / Ref High','Channel A / Ref High'),num(v.ChannelAHighCorrectionScaled,100000,5,'Ω'))+row(tx('Kanal B / Ref Low','Channel B / Ref Low'),num(v.ChannelBLowCorrectionScaled,100000,5,'Ω'))+row(tx('Kanal B / Ref High','Channel B / Ref High'),num(v.ChannelBHighCorrectionScaled,100000,5,'Ω'))+row('Manifest SHA-256',p.ManifestSha256,true);return'<section class="card topic"><h2>'+tx('Gerätejustierung','Device adjustment')+'</h2><div class="grid"><div>'+table(r)+'</div><div>'+table(m)+'</div></div></section>'}
function headSection(p){if(!p)return'';let a=p.Approval||{},v=p.Values||{},s=signer(p);let r='';r+=row(tx('Kopftyp','Head type'),v.HeadType,true)+row(tx('Kopf-SN','Head serial no.'),String(v.HeadSerial||0).padStart(5,'0'),true)+row(tx('Geräte-SN','Device serial no.'),p.DeviceSerial,true)+row(tx('Justierungs-ID','Adjustment ID'),a.CalibrationId,true)+row(tx('Justiert am','Adjustment date'),fmtYmd(a.CalibrationDateYmd||v.CalibrationDateYmd))+row(tx('Gültig ab','Valid from'),fmtUtc(a.ValidFromUtc))+row(tx('Gültig bis','Valid until'),fmtUtc(a.ValidUntilUtc))+row('Signer key ID',s.key,true);let m='';m+=row(tx('R0 T-Spiegel','R0 mirror temperature'),num(v.R0MirrorScaled,10000,4,'Ω'))+row(tx('R0 T-Umgebung','R0 ambient temperature'),num(v.R0AmbientScaled,10000,4,'Ω'))+row(tx('Spiegel 2-Punkt','Mirror 2-point'),v.Mirror2PointActive?tx('aktiv','active'):tx('inaktiv','inactive'))+row(tx('Spiegel Punkt 1','Mirror point 1'),num(v.MirrorSet1_mC,1000,3,'°C')+' → '+num(v.MirrorActual1_mC,1000,3,'°C'))+row(tx('Spiegel Punkt 2','Mirror point 2'),num(v.MirrorSet2_mC,1000,3,'°C')+' → '+num(v.MirrorActual2_mC,1000,3,'°C'))+row(tx('Umgebung 2-Punkt','Ambient 2-point'),v.Ambient2PointActive?tx('aktiv','active'):tx('inaktiv','inactive'))+row(tx('Umgebung Punkt 1','Ambient point 1'),num(v.AmbientSet1_mC,1000,3,'°C')+' → '+num(v.AmbientActual1_mC,1000,3,'°C'))+row(tx('Umgebung Punkt 2','Ambient point 2'),num(v.AmbientSet2_mC,1000,3,'°C')+' → '+num(v.AmbientActual2_mC,1000,3,'°C'))+row(tx('Tau-/Frostpunkt-Offset','Dew/frost point offset'),num(v.DewFrostOffset_mC,1000,3,'K'))+row(tx('Optik-Sollwert','Optical target'),num(v.OpticalTarget_x10,10,1,'%'))+row(tx('Peltier-Stromlimit','Peltier current limit'),String(v.PeltierCurrentLimit_mA||0)+' mA')+row('Manifest SHA-256',p.ManifestSha256,true);return'<section class="card topic"><h2>'+tx('Kopfjustierung','Head adjustment')+'</h2><div class="grid"><div>'+table(r)+'</div><div>'+table(m)+'</div></div></section>'}
function b64urlBytes(text){let s=String(text||'').replace(/-/g,'+').replace(/_/g,'/');while(s.length%4)s+='=';let raw=atob(s),a=new Uint8Array(raw.length);for(let i=0;i<raw.length;i++)a[i]=raw.charCodeAt(i);return a}
const qtyNamesDe={1:'Taupunkt',2:'Frostpunkt',3:'Spiegeltemperatur',4:'Umgebungstemperatur',5:'Relative Feuchte'},qtyNamesEn={1:'Dew point',2:'Frost point',3:'Mirror temperature',4:'Ambient temperature',5:'Relative humidity'};const unitNames={1:'°C',2:'% rH'};
function decodeCalResults(a){let scope=String(a?.CalibrationScope||'NONE'),enc=String(a?.CalibrationResultsEncoding||''),payload=String(a?.CalibrationResultsBase64Url||'');if(scope==='NONE'||!payload)return{scope:'NONE',points:[]};if(enc!=='TP3000-CAL-POINTS-1-BASE64URL')throw new Error(tx('Unbekanntes Kalibriermesswertformat','Unknown calibration measurement format'));let b=b64urlBytes(payload);if(b.length<8||String.fromCharCode(...b.slice(0,4))!=='TPC1')throw new Error(tx('Kalibriermesswerte beschädigt','Calibration measurements are corrupted'));let v=new DataView(b.buffer,b.byteOffset,b.byteLength),scopeByte=v.getUint8(4),count=v.getUint8(5);if(b.length!==8+count*24)throw new Error(tx('Kalibriermesswertlänge ungültig','Invalid calibration measurement length'));let points=[];for(let i=0,o=8;i<count;i++,o+=24){let stage=v.getUint8(o),q=v.getUint8(o+1),u=v.getUint8(o+2),flags=v.getUint8(o+3),set=v.getInt32(o+4,true),ref=v.getInt32(o+8,true),ind=v.getInt32(o+12,true),unc=v.getUint32(o+16,true),k=v.getUint16(o+20,true),conf=v.getUint16(o+22,true);points.push({stage,q,u,hasSet:!!(flags&1),set,ref,ind,dev:ind-ref,unc,k,conf})}let expected={1:'AS_FOUND',2:'AS_LEFT',3:'AS_FOUND_AS_LEFT',4:'AS_FOUND_AS_LEFT_NO_ADJUSTMENT',5:'BEFORE_AFTER_ADJUSTMENT'}[scopeByte];if(expected!==scope)throw new Error(tx('Kalibrierumfang stimmt nicht mit Messdaten überein','Calibration scope does not match measurement data'));return{scope,points}}
function mval(v,u){return dec((v/1000).toFixed(3))+' '+(unitNames[u]||'')}
function resultsTable(titleDe,titleEn,points){if(!points.length)return'';let names=lang===EN?qtyNamesEn:qtyNamesDe,rows=points.map((p,i)=>'<tr><td>'+(i+1)+'</td><td>'+esc(names[p.q]||tx('Unbekannt','Unknown'))+'</td><td>'+esc(p.hasSet?mval(p.set,p.u):'—')+'</td><td>'+esc(mval(p.ref,p.u))+'</td><td>'+esc(mval(p.ind,p.u))+'</td><td>'+esc(mval(p.dev,p.u))+'</td><td>'+esc(mval(p.unc,p.u))+'</td><td>'+esc(dec((p.k/1000).toFixed(3)))+'</td><td>'+esc(dec((p.conf/10).toFixed(1))+' %')+'</td></tr>').join('');return'<div class="calResult"><h3>'+esc(tx(titleDe,titleEn))+'</h3><table class="calTable"><thead><tr><th>'+tx('Nr.','No.')+'</th><th>'+tx('Messgröße','Quantity')+'</th><th>'+tx('Sollwert','Set value')+'</th><th>'+tx('Referenzwert','Reference value')+'</th><th>'+tx('Anzeige TP-3000','TP-3000 indication')+'</th><th>'+tx('Abweichung','Deviation')+'</th><th>U</th><th>k</th><th>'+tx('Vertrauensniveau','Confidence level')+'</th></tr></thead><tbody>'+rows+'</tbody></table></div>'}
function calibrationResultsSection(system){if(!system)return'';let a=system.Approval||{},payload=String(a.CalibrationResultsBase64Url||'');if(!payload)return'<section class="calibrationResults topic"><h2>'+tx('Kalibrierergebnisse','Calibration results')+'</h2><div class="missing">'+tx('Die aktive Systemkalibrierung enthält keine Kalibriermesspunkte.','The active system calibration contains no calibration measurement points.')+'</div></section>';let d=decodeCalResults(a),h='';if(d.scope==='AS_FOUND')h=resultsTable('Kalibrierung – As Found','Calibration – As Found',d.points.filter(x=>x.stage===1));else if(d.scope==='AS_LEFT')h=resultsTable('Kalibrierung – As Left','Calibration – As Left',d.points.filter(x=>x.stage===2));else if(d.scope==='AS_FOUND_AS_LEFT_NO_ADJUSTMENT')h=resultsTable('Kalibrierung – As Found / As Left','Calibration – As Found / As Left',d.points.filter(x=>x.stage===3));else h=resultsTable('Kalibrierung vor Justierung (As Found)','Calibration before adjustment (As Found)',d.points.filter(x=>x.stage===1))+resultsTable('Kalibrierung nach Justierung (As Left)','Calibration after adjustment (As Left)',d.points.filter(x=>x.stage===2));return'<section class="calibrationResults topic"><h2>'+tx('Kalibrierergebnisse der Systemkalibrierung','System calibration results')+'</h2>'+h+'<p class="formula">'+tx('Abweichung = Anzeige TP-3000 − Referenzwert. U ist die erweiterte Messunsicherheit.','Deviation = TP-3000 indication − reference value. U is the expanded measurement uncertainty.')+'</p></section>'}
async function fetchOptional(url){let r=await fetch(url,{cache:'no-store'});if(!r.ok)return null;return await r.json()}
async function deflateText(text){if(typeof CompressionStream==='undefined')throw new Error(tx('Browser unterstützt DEFLATE nicht','Browser does not support DEFLATE'));let source=new Blob([new TextEncoder().encode(text)]).stream();let compressed=source.pipeThrough(new CompressionStream('deflate'));return new Uint8Array(await new Response(compressed).arrayBuffer())}
const B45='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:';function base45(a){let o='',i=0;for(;i+1<a.length;i+=2){let n=a[i]*256+a[i+1];o+=B45[n%45]+B45[Math.floor(n/45)%45]+B45[Math.floor(n/2025)]}if(i<a.length){let n=a[i];o+=B45[n%45]+B45[Math.floor(n/45)]}return o}
function sha256(ascii){function rr(v,a){return(v>>>a)|(v<<(32-a))}let m=Math.pow,max=m(2,32),l='length',i,j,result='',words=[],al=ascii[l]*8,h=sha256.h=sha256.h||[],k=sha256.k=sha256.k||[],pc=k[l],isComposite={};for(let c=2;pc<64;c++){if(!isComposite[c]){for(i=0;i<313;i+=c)isComposite[i]=c;h[pc]=(m(c,.5)*max)|0;k[pc++]=(m(c,1/3)*max)|0}}ascii+='\x80';while(ascii[l]%64-56)ascii+='\x00';for(i=0;i<ascii[l];i++){j=ascii.charCodeAt(i);words[i>>2]|=j<<((3-i)%4)*8}words[words[l]]=(al/max)|0;words[words[l]]=al;for(j=0;j<words[l];){let w=words.slice(j,j+=16),old=h;h=h.slice(0,8);for(i=0;i<64;i++){let w15=w[i-15],w2=w[i-2],a=h[0],e=h[4],temp1=h[7]+(rr(e,6)^rr(e,11)^rr(e,25))+((e&h[5])^((~e)&h[6]))+k[i]+(w[i]=i<16?w[i]:(w[i-16]+(rr(w15,7)^rr(w15,18)^(w15>>>3))+w[i-7]+(rr(w2,17)^rr(w2,19)^(w2>>>10)))|0),temp2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&h[1])^(a&h[2])^(h[1]&h[2]));h=[(temp1+temp2)|0,a,h[1],h[2],(h[3]+temp1)|0,e,h[5],h[6]]}for(i=0;i<8;i++)h[i]=(h[i]+old[i])|0}for(i=0;i<8;i++)for(j=3;j+1;j--){let b=(h[i]>>(j*8))&255;result+=(b<16?'0':'')+b.toString(16)}return result.toUpperCase()}
function manifest(p,label){let h=String(p?.ManifestSha256||'').toUpperCase();if(p&&!/^[0-9A-F]{64}$/.test(h))throw new Error(tx('Manifest SHA-256 fehlt: ','Manifest SHA-256 missing: ')+label);return h||'-'}
function documentIdentity(device,head,system,firmwareContext){let dm=manifest(device,tx('Gerätejustierung','device adjustment')),hm=manifest(head,tx('Kopfjustierung','head adjustment')),sm=manifest(system,tx('Systemkalibrierung','system calibration')),fm=firmwareContext&&firmwareContext.recordValid?String(firmwareContext.contextManifestSha256||'').toUpperCase():'';let hash=sha256('TP3000-CAL-DOC-3\nD='+dm+'\nH='+hm+'\nS='+sm+'\nF='+fm);return{id:hash.slice(0,16),hash}}
function firmwareContextForExport(c){if(!c||!c.present||!c.recordValid||!c.requestBindingValid)return null;return{Format:'TP3000-SYSTEM-FIRMWARE-CONTEXT-1',CaptureMode:c.captureMode,CapturedUtc:Number(c.capturedUtc||0),FirmwareVersion:c.firmwareVersion,FirmwareBuildId:c.firmwareBuildId,ImageBase:Number(c.imageBase||0),ImageSize:Number(c.imageSize||0),FirmwareSha256:c.firmwareSha256,ManufacturerStatus:c.manufacturerStatus,FirmwareCertificateId:c.firmwareCertificateId||'',ManufacturerRootKeyId:c.manufacturerRootKeyId||'',CertificateApprovedUtc:Number(c.certificateApprovedUtc||0),SourceRequestId:c.sourceRequestId,SourceRequestManifestSha256:c.sourceRequestManifestSha256,ContextManifestSha256:c.contextManifestSha256,DeviceSignatureHex:c.deviceSignatureHex}}
async function payloadPart(identity,count,index,device,head,system,firmwareContext){let env={Format:'TP3000-CALIBRATION-QR-4',DocumentId:identity.id,DocumentHash:identity.hash,QrPartCount:count,QrPartIndex:index,Device:device||null,Head:head||null,System:system||null,FirmwareContext:system?firmwareContextForExport(firmwareContext):null,FirmwareApplicabilityAtExport:system?String(firmwareContext?.applicability||'NOT_COMPARABLE'):null,CurrentFirmwareSha256AtExport:system?String(firmwareContext?.currentFirmwareSha256||''):null};return'TP3Q3:'+base45(await deflateText(JSON.stringify(env)))}
const QR_DEEP_LINK_PREFIX='tp3000://verify#';
function qrEncodedFragment(payload){return encodeURIComponent(String(payload||''))}
function setQrLayout(count){let area=E('qrArea');area.className='qrArea '+(count===2?'qrTwo':'qrOne');let section=area.parentElement;if(section)section.className='qrSection '+(count===2?'qrTwoSection':'qrOneSection')}
function qrPath(q){let n=q.getModuleCount(),d='';for(let y=0;y<n;y++){let run=-1;for(let x=0;x<=n;x++){let dark=x<n&&q.isDark(y,x);if(dark&&run<0)run=x;if((!dark||x===n)&&run>=0){d+='M'+run+' '+y+'h'+(x-run)+'v1H'+run+'z';run=-1}}}return{n,d}}
function qrSvgTp3c1(rawText){let q=new QRCode(-1,QRErrorCorrectLevel.M);/* Kanonisch identisch zum TFT: Präfix im Byte-Modus, TP3C1/Base38 im QR-Alphanumerikmodus. */q.addData(QR_DEEP_LINK_PREFIX,'byte');q.addData(rawText,'alphanumeric');q.make();let x=qrPath(q);return{modules:x.n,svg:'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -4 '+(x.n+8)+' '+(x.n+8)+'" shape-rendering="crispEdges" role="img" aria-label="TP-3000 Verifier TP3C1 Deep Link"><rect x="-4" y="-4" width="'+(x.n+8)+'" height="'+(x.n+8)+'" fill="#fff"/><path d="'+x.d+'" fill="#000"/></svg>'}}
function qrCardTp3c1(titleDe,titleEn,part){let q=qrSvgTp3c1(part.rawText);return'<div class="qrCard"><h3>'+esc(tx(titleDe,titleEn))+'</h3>'+q.svg+'<div class="qrCaption">TP3C1 V0.4 · '+tx('Teil ','Part ')+part.index+'/2 · ZLIB · Base38 · CRC-32 · EC M · '+q.modules+'×'+q.modules+' Module · ECDSA P-256 / SHA-256</div></div>'}
function tp3q3Probe(payload){let q=new QRCode(-1,QRErrorCorrectLevel.L);q.addData(QR_DEEP_LINK_PREFIX);q.addData(qrEncodedFragment(payload));q.make()}
async function buildTp3q3Export(identity,device,head,system,firmwareContext){let one=await payloadPart(identity,1,1,device,head,system,firmwareContext);try{tp3q3Probe(one);return[one]}catch(oneError){let layouts=[[device,head,null,null,null,system],[device,null,null,null,head,system],[null,head,null,device,null,system]];for(const x of layouts){try{let p1=await payloadPart(identity,2,1,x[0],x[1],x[2],x[2]?firmwareContext:null),p2=await payloadPart(identity,2,2,x[3],x[4],x[5],x[5]?firmwareContext:null);tp3q3Probe(p1);tp3q3Probe(p2);return[p1,p2]}catch(e){}}throw new Error(tx('TP3Q3-Export ist selbst für zwei QR-Teile zu groß.','TP3Q3 export is too large even for two QR parts.'))}}
let qrPayloads=[],qrDocumentId='';
async function buildQr(device,head,system,certificate,firmwareContext,sequence){let area=E('qrArea'),identity=documentIdentity(device,head,system,firmwareContext);qrDocumentId=identity.id;E('toolbarMsg').textContent=tx('TP3C1 wird bytegenau aufgebaut …','TP3C1 is being assembled byte-for-byte …');await new Promise(r=>requestAnimationFrame(()=>r()));let compact=await TP3C1.build(device,head,system,certificate,firmwareContext);if(sequence!==renderSequence)return;if(compact.parts.length!==2)throw new Error(tx('TP3C1 muss aus genau zwei Teilen bestehen.','TP3C1 must consist of exactly two parts.'));let c1=qrCardTp3c1('TP3C1-Offline-Prüfcode – Teil 1/2','TP3C1 offline verification code – part 1/2',compact.parts[0]),c2=qrCardTp3c1('TP3C1-Offline-Prüfcode – Teil 2/2','TP3C1 offline verification code – part 2/2',compact.parts[1]);setQrLayout(2);area.innerHTML='<div class="qrCount"><b>'+tx('Offline-Prüfung: 2 TP3C1-QR-Codes','Offline verification: 2 TP3C1 QR codes')+'</b><br>'+tx('Beide Teile sind erforderlich und dürfen in beliebiger Reihenfolge gescannt werden. Dokumentkennung: ','Both parts are required and may be scanned in any order. Document ID: ')+'<span class="mono">'+esc(compact.documentId)+'</span></div>'+c1+c2;E('toolbarMsg').textContent=tx('Vollständiger TP3Q3-Export wird vorbereitet …','Preparing complete TP3Q3 export …');qrPayloads=await buildTp3q3Export(identity,device,head,system,firmwareContext);if(sequence!==renderSequence)return;E('downloadQr').disabled=false;E('printCert').disabled=false}
function savePayload(text,name){let a=document.createElement('a');a.href=URL.createObjectURL(new Blob([text+'\n'],{type:'text/plain;charset=us-ascii'}));a.download=name;document.body.appendChild(a);a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1500);a.remove()}
function downloadQr(){if(!qrPayloads.length)return;let base=(lang===EN?'TP3000_SystemCalibrationCertificate_':'TP3000_Systemkalibrierschein_')+(qrDocumentId||'').replace(/[^A-Za-z0-9_-]+/g,'_');qrPayloads.forEach((p,i)=>setTimeout(()=>savePayload(p,base+(qrPayloads.length>1?(lang===EN?'_Part':'_Teil')+(i+1)+(lang===EN?'of':'von')+qrPayloads.length:'')+'.tpqr'),i*250))}
E('downloadQr').onclick=downloadQr;
const viewManifest=String(new URLSearchParams(location.search).get('id')||'').toUpperCase();
const historicalView=/^[0-9A-F]{64}$/.test(viewManifest);
const withId=(path,id)=>path+'?id='+encodeURIComponent(id);
function cssQuoted(v){return String(v||'').replace(/\\/g,'\\\\').replace(/"/g,'\\"').replace(/[\r\n]+/g,' ')}
function updatePrintPageStyle(){let cert=cssQuoted(certificateNumber),certLabel=cssQuoted(tx('Kalibrierschein-Nr.: ','Calibration certificate No.: ')),pageLabel=cssQuoted(tx('Seite ','Page ')),ofLabel=cssQuoted(tx(' von ',' of '));E('dynamicPrintStyle').textContent='@page{size:A4;margin:18mm 14mm 16mm;@top-right{content:"'+certLabel+cert+'";font:700 8.5pt Arial,sans-serif;color:#111;vertical-align:bottom;padding-bottom:1.5mm;border-bottom:.2mm solid #777;}@bottom-right{content:"'+pageLabel+'" counter(page) "'+ofLabel+'" counter(pages);font:8.5pt Arial,sans-serif;color:#111;vertical-align:top;padding-top:1.5mm;border-top:.2mm solid #777;}}@page :first{@top-right{content:"";border-bottom:0;}}'}
function applyStaticLanguage(){document.documentElement.lang=lang===EN?'en':'de';document.title=tx('TP-3000 Kalibrierschein','TP-3000 calibration certificate');E('certificateSelectionLink').textContent=tx('← Zertifikatsauswahl','← Certificate selection');E('calibrationManageLink').textContent=tx('Justierung / Kalibrierung verwalten','Manage adjustment / calibration');E('printCert').textContent=tx('Drucken / PDF','Print / PDF');E('downloadQr').textContent=tx('Vollständige QR-Daten speichern (TP3Q3 .tpqr)','Save complete QR data (TP3Q3 .tpqr)');E('certificateTitle').textContent=tx('TP-3000 Kalibrierschein','TP-3000 calibration certificate');E('certificateSubtitle').textContent=tx('Taupunktspiegel-Hygrometer','Chilled-mirror hygrometer');E('langDe').classList.toggle('selected',lang===DE);E('langEn').classList.toggle('selected',lang===EN);updatePrintPageStyle()}
async function renderCertificate(){if(!certificateState)return;let seq=++renderSequence,{device,head,system,certificate,applicability,firmwareContext,externalPdf}=certificateState;applyStaticLanguage();E('downloadQr').disabled=true;E('printCert').disabled=true;qrPayloads=[];let ended=!!(applicability&&applicability.stateKnown&&applicability.ended),firmwareChanged=!!(!historicalView&&firmwareContext&&firmwareContext.recordValid&&firmwareContext.currentHashAvailable&&!firmwareContext.currentImageMatches),issuerPackages=system?[system]:[device,head].filter(Boolean),status=system?(ended?'<div class="status topic"><span class="warn">'+tx('Kalibrierzertifikat kryptografisch valide, aber seit ','Calibration certificate cryptographically valid, but no longer applicable since ')+esc(fmtEpochUtc(applicability.applicableUntilUtc))+tx(' nicht mehr anwendbar.','.')+'</span><br>'+tx('Grund: ','Reason: ')+esc(reasonText(applicability.reasonCode))+'</div>':(firmwareChanged?'<div class="status topic"><span class="warn">'+tx('Systemkalibrierung signiert und gültig.','System calibration signed and valid.')+'</span><br>'+tx('Die aktuelle Firmware weicht vom bei der Kalibrierung dokumentierten Stand ab. Der Kalibrierschein bleibt gültig; die Abweichung und der aktuelle SHA-256 werden im Nachweis dokumentiert.','The current firmware differs from the state documented at calibration. The calibration certificate remains valid; the deviation and current SHA-256 are recorded in the evidence.')+'</div>':(historicalView?'<div class="status topic"><span class="ok">'+tx('Historisches Kalibrierzertifikat aus dem SD-Archiv geladen und kryptografisch erneut geprüft.','Historical calibration certificate loaded from the SD archive and cryptographically reverified.')+'</span><br>'+tx('Gerätejustierung, Kopfjustierung und Systemkalibrierung entsprechen exakt der damaligen signierten Bindung.','Device adjustment, head adjustment and system calibration exactly match the signed binding at that time.')+'</div>':'<div class="status topic"><span class="ok">'+tx('Kryptografisch signierte Systemkalibrierung sowie gebundene Geräte- und Kopfjustierung im Gerät aktiv.','Cryptographically signed system calibration and bound device and head adjustments are active in the instrument.')+'</span><br>'+tx('Die aktuelle Firmware entspricht dem bei der Systemkalibrierung dokumentierten Firmwarestand.','The current firmware matches the firmware state documented with the system calibration.')+'</div>'))):'<div class="missing topic"><b>'+tx('Keine Systemkalibrierung.','No system calibration.')+'</b><br>'+tx('Die vorhandenen Justierungsdaten werden nur als unvollständiger Nachweis angezeigt.','The available adjustment data are shown only as incomplete evidence.')+'</div>';
let html=status+issuerSection(issuerPackages)+systemSection(system,!!externalPdf,applicability,firmwareContext)+calibrationResultsSection(system)+deviceSection(device)+headSection(head)+'<section class="professional topic"><h2>'+professionalTitle(system)+'</h2>'+professional(system)+'</section><section class="qrSection qrOneSection"><h2>'+tx('Offline-Prüfung','Offline verification')+'</h2><div id="qrArea" class="qrArea qrOne">'+(system?'<p>'+tx('QR-Code wird erzeugt …','Generating QR code …')+'</p>':'<div class="missing">'+tx('Ohne Systemkalibrierung wird kein vollständiger Offline-Prüfcode erzeugt.','No complete offline verification code is generated without a system calibration.')+'</div>')+'</div><div class="foot qrFoot">'+tx('Prüfverfahren: TP3C1 V0.4 rekonstruiert Gerätezertifikat, Laborzertifikat, Gerätejustierung, Kopfjustierung und Systemkalibrierung bytegenau, prüft deren fünf ECDSA-P256/SHA-256-Signaturen und zusätzlich den gerätesignierten Firmwarekontext. Beide kompakten QR-Teile sind erforderlich. Der elektronische .tpqr-Download bleibt im vollständigen TP3Q3-Format. Eine Internetverbindung ist nicht erforderlich.','Verification method: TP3C1 V0.4 reconstructs the device certificate, laboratory certificate, device adjustment, head adjustment and system calibration byte-for-byte, verifies their five ECDSA-P256/SHA-256 signatures and additionally verifies the device-signed firmware context. Both compact QR parts are required. The electronic .tpqr download remains in the complete TP3Q3 format. No internet connection is required.')+'</div></section>';
E('content').innerHTML=html;
if(system&&device&&head){try{await buildQr(device,head,system,certificate,firmwareContext,seq);if(seq===renderSequence)E('toolbarMsg').textContent=''}catch(qrError){if(seq!==renderSequence)return;E('qrArea').innerHTML='<div class="missing"><b>'+tx('QR-Code konnte nicht erzeugt werden.','QR code could not be generated.')+'</b><br>'+esc(qrError.message||qrError)+'</div>';E('toolbarMsg').textContent=tx('QR-Fehler','QR error');E('downloadQr').disabled=true;E('printCert').disabled=false}}else{E('toolbarMsg').textContent=tx('Kalibrierschein unvollständig','Calibration certificate incomplete');E('printCert').disabled=false}}
function setLanguage(next){next=next===EN?EN:DE;if(next===lang)return;lang=next;try{localStorage.setItem('tp3000-cal-certificate-language',lang===EN?'en':'de')}catch(_){}applyStaticLanguage();if(certificateState)renderCertificate()}
E('langDe').onclick=()=>setLanguage(DE);E('langEn').onclick=()=>setLanguage(EN);applyStaticLanguage();
(async()=>{try{
  let system=await fetchOptional(historicalView?withId('/calibration-active-system.json',viewManifest):'/calibration-active-system.json');
  let device=null,head=null;
  if(historicalView){
    if(!system)throw new Error(tx('Das ausgewählte historische Systemkalibrierzertifikat ist nicht verfügbar oder wurde verändert.','The selected historical system calibration certificate is unavailable or has been modified.'));
    let v=system.Values||{},dm=String(v.DeviceAdjustmentManifestSha256||'').toUpperCase(),hm=String(v.HeadAdjustmentManifestSha256||'').toUpperCase();
    if(!/^[0-9A-F]{64}$/.test(dm)||!/^[0-9A-F]{64}$/.test(hm))throw new Error(tx('Die historischen Justierungsbindungen sind unvollständig.','The historical adjustment bindings are incomplete.'));
    device=await fetchOptional(withId('/calibration-active-device.json',dm));
    head=await fetchOptional(withId('/calibration-active-head.json',hm));
  }else [device,head]=await Promise.all([fetchOptional('/calibration-active-device.json'),fetchOptional('/calibration-active-head.json')]);
  let applicability=null;if(system&&/^[0-9A-Fa-f]{64}$/.test(String(system.ManifestSha256||'')))applicability=await fetchOptional('/calibration-applicability.json?id='+encodeURIComponent(system.ManifestSha256));
  let firmwareContext=null;if(system){let requestId=String(system.SourceRequestId||''),requestHash=String(system.SourceRequestManifestSha256||'');if(/^[0-9A-Fa-f]{32}$/.test(requestId)&&/^[0-9A-Fa-f]{64}$/.test(requestHash))firmwareContext=await fetchOptional('/calibration-firmware-context.json?request='+encodeURIComponent(requestId)+'&hash='+encodeURIComponent(requestHash))}
  let externalPdf=null;if(system&&isExternalSystem(system)){let externalId=String(system?.Values?.ExternalCertificateDocumentId||'');if(/^[0-9A-Fa-f]{32}$/.test(externalId))externalPdf=await fetchOptional('/calibration-external-active.json?id='+encodeURIComponent(externalId))}
  let certificate=await fetchOptional('/identity-active-certificate.json');
  if(!device&&!head&&!system){E('content').innerHTML='<div class="missing"><b>'+tx('Kein Kalibrierschein verfügbar.','No calibration certificate available.')+'</b><br>'+tx('Es sind keine signierten Justierungs- oder Systemkalibrierdaten vorhanden.','No signed adjustment or system calibration data are available.')+'</div>';E('toolbarMsg').textContent=tx('Keine Kalibrierdaten','No calibration data');return}
  certificateNumber=system?.Approval?.CalibrationId||[device?.Approval?.CalibrationId,head?.Approval?.CalibrationId].filter(Boolean).join(' / ')||'—';E('docId').textContent=certificateNumber;updatePrintPageStyle();certificateState={device,head,system,certificate,applicability,firmwareContext,externalPdf};await renderCertificate();
}catch(e){E('content').innerHTML='<div class="missing"><b>'+tx('Kalibrierschein konnte nicht aufgebaut werden.','Calibration certificate could not be generated.')+'</b><br>'+esc(e.message||e)+'</div>';E('toolbarMsg').textContent=tx('Fehler','Error')}})();
</script></body></html>
)TPCALSHEET1";

static bool FLASHMEM ethPathEquals(const char* requestLine, const char* path);
static bool FLASHMEM ethPathEqualsMethod(const char* requestLine,
                                         const char* method,
                                         const char* path);


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
  void restartBudget() { startedMs = millis(); }

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
// Der Build-ID-Anteil verhindert, dass ein Browser nach einem Firmwareupdate
// eine alte, noch eingebettete JavaScript-Version per HTTP 304 weiterverwendet.
static const char ETH_ETAG_MAIN_DE[] = "\"TP3000-" TP_FIRMWARE_BUILD_ID_STRING "-MAIN-DE\"";
static const char ETH_ETAG_MAIN_EN[] = "\"TP3000-" TP_FIRMWARE_BUILD_ID_STRING "-MAIN-EN\"";
static const char ETH_ETAG_SETUP[]   = "\"TP3000-" TP_FIRMWARE_BUILD_ID_STRING "-SETUP\"";
static const char ETH_ETAG_DOWNLOAD[]= "\"TP3000-" TP_FIRMWARE_BUILD_ID_STRING "-DOWNLOAD\"";
static const char ETH_ETAG_CAL_CERT_INDEX[]= "\"TP3000-" TP_FIRMWARE_BUILD_ID_STRING "-CAL-CERT-INDEX\"";
static const char ETH_ETAG_CAL_CERT_VIEW[]= "\"TP3000-" TP_FIRMWARE_BUILD_ID_STRING "-CAL-CERT-VIEW\"";

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

  // Dynamisch erzeugte Seiten uebergeben einen leeren ETag. Sie duerfen
  // niemals mit 304 beantwortet werden, weil ihr Inhalt vom aktuellen
  // Zertifikats-/Kalibrierzustand abhaengt. Statische Seiten behalten ihre
  // bisherige ETag-Unterstuetzung.
  const bool browserCacheAllowed = requestCode != ETH_REQ_DOWNLOAD_PAGE &&
                                   etag != nullptr && etag[0] != '\0';
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
  int headerLength;
  if (browserCacheAllowed)
  {
    headerLength = snprintf(ethPageHeader, sizeof(ethPageHeader),
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Content-Length: %lu\r\n"
      "Cache-Control: %s\r\n"
      "Pragma: no-cache\r\n"
      "Expires: 0\r\n"
      "ETag: %s\r\n\r\n",
      (unsigned long)contentLength, cacheControl, etag);
  }
  else
  {
    headerLength = snprintf(ethPageHeader, sizeof(ethPageHeader),
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Content-Length: %lu\r\n"
      "Cache-Control: %s\r\n"
      "Pragma: no-cache\r\n"
      "Expires: 0\r\n\r\n",
      (unsigned long)contentLength, cacheControl);
  }
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

// Dynamisch in ethJsonBuf aufgebaute HTML-Seiten werden nicht mehr innerhalb
// des 120-ms-Sofortantwortbudgets gesendet. Stattdessen nutzt auch diese
// Seitengruppe den vorhandenen nicht blockierenden 512-Byte-Seitentransfer.
// Solange ethPageActive gesetzt ist, wird kein weiterer fertiger Request
// beantwortet; ethJsonBuf bleibt daher bis zum Transferende unveraendert.
static bool FLASHMEM ethBeginGeneratedPageTransfer(
    EthernetClient& rawClient,
    EthBoundedWriter& immediateClient,
    const char* requestHeaders,
    uint8_t requestCode,
    const char* body,
    size_t bodyLength,
    bool* keepOpen)
{
  if (body == nullptr || bodyLength == 0U)
  {
    ethSendText(immediateClient, false, "Leere HTML-Seite / empty HTML page");
    return immediateClient.ok();
  }

  const EthPageSegment segment = { body, bodyLength };
  return ethBeginPageTransfer(rawClient, immediateClient, requestHeaders,
                              "", requestCode, &segment, 1U,
                              bodyLength, keepOpen);
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
  ETH_FILE_KIND_SERIAL = 1,
  ETH_FILE_KIND_EVIDENCE = 2
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
  if (ethPathEquals(requestLine, "/identity")) return ETH_REQ_IDENTITY_PAGE;
  if (ethPathEquals(requestLine, "/identity-keygen")) return ETH_REQ_IDENTITY_KEYGEN;
  if (ethPathEquals(requestLine, "/identity-request.tpreq")) return ETH_REQ_IDENTITY_REQUEST;
  if (ethPathEquals(requestLine, "/identity-cert-import")) return ETH_REQ_IDENTITY_CERT_IMPORT;
  if (ethPathEquals(requestLine, ETH_PATH_VALIDITY)) return ETH_REQ_VALIDITY_PAGE;
  if (ethPathEquals(requestLine, ETH_PATH_CALIBRATION)) return ETH_REQ_CALIBRATION_PAGE;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_DEVICE_REQUEST)) return ETH_REQ_CALIBRATION_DEVICE_REQUEST;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_HEAD_REQUEST)) return ETH_REQ_CALIBRATION_HEAD_REQUEST;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_DEVICE_IMPORT)) return ETH_REQ_CALIBRATION_DEVICE_IMPORT;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_HEAD_IMPORT)) return ETH_REQ_CALIBRATION_HEAD_IMPORT;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_SYSTEM_IMPORT)) return ETH_REQ_CALIBRATION_SYSTEM_IMPORT;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_CERTIFICATE)) return ETH_REQ_CALIBRATION_CERTIFICATE;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_CERTIFICATE_VIEW)) return ETH_REQ_CALIBRATION_CERTIFICATE_VIEW;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_CERTIFICATES_JSON)) return ETH_REQ_CALIBRATION_CERTIFICATES_JSON;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_ACTIVE_DEVICE)) return ETH_REQ_CALIBRATION_ACTIVE_DEVICE;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_ACTIVE_HEAD)) return ETH_REQ_CALIBRATION_ACTIVE_HEAD;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_SYSTEM_REQUEST)) return ETH_REQ_CALIBRATION_SYSTEM_REQUEST;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_ACTIVE_SYSTEM)) return ETH_REQ_CALIBRATION_ACTIVE_SYSTEM;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_EXTERNAL_DOWNLOAD)) return ETH_REQ_CALIBRATION_EXTERNAL_DOWNLOAD;
  if (ethPathEquals(requestLine, ETH_PATH_CAL_EXTERNAL_ACTIVE)) return ETH_REQ_CALIBRATION_EXTERNAL_ACTIVE;
  if (ethPathEquals(requestLine, ETH_PATH_IDENTITY_ACTIVE_CERT)) return ETH_REQ_IDENTITY_ACTIVE_CERTIFICATE;
  if (ethPathEquals(requestLine, ETH_PATH_FIRMWARE_REQUEST)) return ETH_REQ_FIRMWARE_REQUEST;
  if (ethPathEquals(requestLine, ETH_PATH_FIRMWARE_CERT_IMPORT)) return ETH_REQ_FIRMWARE_CERT_IMPORT;
  if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_FIRMWARE_CERT_UPLOAD)) return ETH_REQ_FIRMWARE_CERT_UPLOAD;
  if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_BEGIN)) return ETH_REQ_CALIBRATION_EXTERNAL_BEGIN;
  if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_CHUNK)) return ETH_REQ_CALIBRATION_EXTERNAL_CHUNK;
  if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_FINISH)) return ETH_REQ_CALIBRATION_EXTERNAL_FINISH;
  if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_ABORT)) return ETH_REQ_CALIBRATION_EXTERNAL_ABORT;
  if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_SYSTEM_UPLOAD)) return ETH_REQ_CALIBRATION_SYSTEM_UPLOAD;
  if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_DEVICE_UPLOAD)) return ETH_REQ_CALIBRATION_DEVICE_UPLOAD;
  if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_HEAD_UPLOAD)) return ETH_REQ_CALIBRATION_HEAD_UPLOAD;
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

static bool FLASHMEM ethIpMatches(IPAddress ip, const uint8_t expected[4])
{
  return expected != nullptr &&
         ip[0] == expected[0] && ip[1] == expected[1] &&
         ip[2] == expected[2] && ip[3] == expected[3];
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
    // NativeEthernet/FNET hat keine end()-Funktion. Wurde Ethernet lediglich
    // ausgeschaltet und im selben DHCP-Modus wieder eingeschaltet, bleibt die
    // gueltige Lease im Stack erhalten. In diesem Fall darf Ethernet.begin()
    // nicht erneut aufgerufen werden; das vermeidet Reinitialisierungs-Haenger.
    if (ethRuntimeModeKnown && ethRuntimeDhcp &&
        ethIpIsValid(Ethernet.localIP()))
    {
      ethConfigured = true;
      ethDhcpPending = false;
    }
    else
    {
      // Erster DHCP-Start mit begrenztem Zeitbudget. Wenn der PHY waehrend der
      // FNET-Initialisierung erst spaet LinkON meldet, folgt genau ein kurzer
      // zweiter Warteabschnitt. Beide Abschnitte zusammen bleiben deutlich
      // unter dem 8-s-Hardware-Watchdog. Danach laeuft FNET bis zu 30 s im
      // Hintergrund weiter; es gibt keinen zyklischen begin()-Suchlauf.
      int dhcpResult = Ethernet.begin(mac,
                                      ETH_DHCP_FIRST_TIMEOUT_MS,
                                      ETH_DHCP_RESPONSE_TIMEOUT_MS);
      ethRuntimeModeKnown = true;
      ethRuntimeDhcp = true;

      if ((dhcpResult == 0 || !ethIpIsValid(Ethernet.localIP())) &&
          Ethernet.linkStatus() == LinkON)
      {
        dhcpResult = Ethernet.begin(mac,
                                    ETH_DHCP_RETRY_TIMEOUT_MS,
                                    ETH_DHCP_RESPONSE_TIMEOUT_MS);
      }

      if (dhcpResult != 0 && ethIpIsValid(Ethernet.localIP()))
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
    else if (ethRuntimeModeKnown && !ethRuntimeDhcp &&
             ethIpMatches(Ethernet.localIP(), ethLastStaticIp))
    {
      // Ethernet wurde nur aus- und wieder eingeschaltet. Die passende feste
      // IP ist im FNET-Stack noch aktiv; kein zweites begin() noetig.
      ethConfigured = true;
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
      ethRuntimeModeKnown = true;
      ethRuntimeDhcp = false;
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

static bool FLASHMEM ethGetQueryText(const char* requestLine,
                                     const char* key,
                                     char* output,
                                     size_t outputSize)
{
  if (requestLine == nullptr || key == nullptr || output == nullptr || outputSize == 0U) return false;
  output[0] = '\0';

  const char* query = strchr(requestLine, '?');
  const char* requestEnd = strchr(requestLine + 4, ' ');
  if (query == nullptr || requestEnd == nullptr || query >= requestEnd) return false;

  const size_t keyLength = strlen(key);
  const char* position = query + 1;
  while (position < requestEnd)
  {
    const char* itemEnd = strchr(position, '&');
    if (itemEnd == nullptr || itemEnd > requestEnd) itemEnd = requestEnd;

    if ((size_t)(itemEnd - position) > keyLength &&
        strncmp(position, key, keyLength) == 0 &&
        position[keyLength] == '=')
    {
      const char* value = position + keyLength + 1U;
      const size_t valueLength = (size_t)(itemEnd - value);
      if (valueLength + 1U > outputSize) return false;
      memcpy(output, value, valueLength);
      output[valueLength] = '\0';
      return true;
    }

    position = itemEnd < requestEnd ? itemEnd + 1U : requestEnd;
  }
  return false;
}

static int FLASHMEM ethHexDigit(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool FLASHMEM ethGetQueryDecodedText(const char* requestLine,
                                            const char* key,
                                            char* output,
                                            size_t outputSize)
{
  char encoded[256];
  if (!ethGetQueryText(requestLine, key, encoded, sizeof(encoded)) ||
      output == nullptr || outputSize == 0U) return false;
  size_t out = 0U;
  for (size_t i = 0U; encoded[i] != '\0'; ++i)
  {
    uint8_t value = (uint8_t)encoded[i];
    if (value == '+') value = ' ';
    else if (value == '%')
    {
      const int high = ethHexDigit(encoded[i + 1U]);
      const int low = ethHexDigit(encoded[i + 2U]);
      if (high < 0 || low < 0) return false;
      value = (uint8_t)((high << 4) | low);
      i += 2U;
    }
    if (value == 0U || out + 1U >= outputSize) return false;
    output[out++] = (char)value;
  }
  output[out] = '\0';
  return true;
}

// Gemeinsame, kompakte Zeit- und Statusformatierung fuer die Webseiten.
static void FLASHMEM ethFormatValidityDateTime(int64_t unixTime,
                                               char* output,
                                               size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return;
  if (unixTime <= 0)
  {
    strncpy(output, "nicht beurteilbar", outputSize - 1U);
    output[outputSize - 1U] = '\0';
    return;
  }
  const time_t t = (time_t)unixTime;
  snprintf(output, outputSize, "%02d.%02d.%04d %02d:%02d:%02d UTC",
           day(t), month(t), year(t), hour(t), minute(t), second(t));
}

static void FLASHMEM ethFormatValidityYmd(uint32_t ymd,
                                         char* output,
                                         size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return;
  const uint32_t yearValue = ymd / 10000UL;
  const uint32_t monthValue = (ymd / 100UL) % 100UL;
  const uint32_t dayValue = ymd % 100UL;
  if (yearValue < 2020UL || yearValue > 2099UL ||
      monthValue < 1UL || monthValue > 12UL ||
      dayValue < 1UL || dayValue > 31UL)
  {
    strncpy(output, "–", outputSize - 1U);
    output[outputSize - 1U] = '\0';
    return;
  }
  snprintf(output, outputSize, "%02lu.%02lu.%04lu",
           (unsigned long)dayValue,
           (unsigned long)monthValue,
           (unsigned long)yearValue);
}

static const char* FLASHMEM ethSignedCalibrationClass(
    bool ready,
    const TpSignedCalibrationStatus& status,
    TpCalibrationTimeStatus timeStatus)
{
  if (!ready) return status.filePresent ? "bad" : "warn";
  if (timeStatus == TP_CAL_TIME_EXPIRED) return "bad";
  return timeStatus == TP_CAL_TIME_VALID ? "ok" : "warn";
}

static const char* FLASHMEM ethSignedCalibrationStatusText(
    bool ready,
    const TpSignedCalibrationStatus& status,
    TpCalibrationTimeStatus timeStatus,
    bool device)
{
  if (!ready)
  {
    if (status.lastError[0] != '\0') return status.lastError;
    return device ? "Keine aktive signierte Gerätejustierung"
                  : "Keine aktive signierte Kopfjustierung";
  }
  switch (timeStatus)
  {
    case TP_CAL_TIME_VALID:
      return "Signiert und aktiv – innerhalb Kalibrierzeitraum";
    case TP_CAL_TIME_EXPIRED:
      return "Signiert und aktiv – außerhalb Kalibrierzeitraum";
    case TP_CAL_TIME_NOT_YET_VALID:
      return "Signiert und aktiv – Kalibrierzeitraum beginnt später";
    default:
      return "Signiert und aktiv – Kalibrierzeit nicht beurteilbar";
  }
}

#define ETH_IDENTITY_PAGE_TEMPLATE \
  "<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>" \
  "<title>TP-3000 Zertifizierung</title><style>body{font-family:Arial,sans-serif;max-width:980px;margin:24px auto;padding:0 16px;background:#111;color:#eee}" \
  "h1{font-size:28px}h2{margin-top:28px}table{border-collapse:collapse;width:100%%;background:#1b1b1b;margin-bottom:18px}" \
  "td{padding:9px;border-bottom:1px solid #444;vertical-align:top}td:first-child{width:230px;color:#bbb}" \
  "a{color:#8fe7ff}a.btn,button{display:inline-block;margin:8px 8px 8px 0;padding:10px 14px;background:#333;color:#fff;border:1px solid #777;border-radius:5px;text-decoration:none;cursor:pointer}" \
  ".ok{color:#66ff66}.warn{color:#ffd85a}.bad{color:#ff7777}.box{padding:14px;background:#1b1b1b;border:1px solid #444;border-radius:6px;margin:16px 0}code{color:#8fe7ff}" \
  ".progress{table-layout:fixed}.progress td:nth-child(1){width:22%%}.progress td:nth-child(2){width:58%%;white-space:normal;overflow-wrap:anywhere}.progress td:nth-child(3){width:20%%;color:#bbb}</style></head><body>" \
  "<p><a href='/'>← Hauptanzeige</a></p><h1>TP-3000 Zertifizierung</h1>" \
  "<h2>Aktueller Zertifizierungsstand</h2><table class='progress'>" \
  "<tr><td>Geräteidentität</td><td class='%s'><b>%s</b></td><td>SN %s</td></tr>" \
  "<tr><td>Gerätejustierung</td><td class='%s'><b>%s</b></td><td>%s</td></tr>" \
  "<tr><td>Kopfjustierung</td><td class='%s'><b>%s</b></td><td>%s / %05lu</td></tr>" \
  "<tr><td>Firmwarefreigabe</td><td class='%s'><b>%s</b></td><td>%s / %s</td></tr></table>" \
  "<h2>Geräteidentität</h2><div class='box'><b class='%s'>%s</b><br>Diese Testversion vertraut dem eingebauten Wegwerf-Test-Root <code>%s</code>.</div>" \
  "<table><tr><td>Wirksame Geräte-SN</td><td><b>%s</b></td></tr><tr><td>Quelle der Seriennummer</td><td>%s</td></tr>" \
  "<tr><td>Geräte-Key-ID</td><td><code>%s</code></td></tr><tr><td>Zertifikat ausgestellt</td><td>%s</td></tr><tr><td>Firmware</td><td>%s</td></tr></table>" \
  "<h2>1. Geräteschlüssel</h2><p>Der private P-256-Schlüssel wird im Teensy erzeugt und verlässt das Gerät nicht.</p>%s" \
  "<h2>2. Zertifikatsanfrage</h2><p>Die Anfrage enthält nur Public Key, vorläufige SN und eine Selbstsignatur. Beim Download wird zusätzlich eine Kopie in <code>/IDENTITY</code> auf der SD-Karte angelegt.</p>%s" \
  "<h2>3. Zertifikat importieren</h2><p>Das vom Offline-KeyTool erzeugte <code>.tpcert</code> auf der SD-Karte in <code>/IDENTITY</code> ablegen. Das Gerät akzeptiert nur ein Zertifikat, das zu seinem internen Schlüssel und zum Test-Root passt.</p>%s" \
  "<div class='box'><b>Nach erfolgreichem Import:</b> Die Geräte-SN stammt ausschließlich aus dem Zertifikat und ist in den normalen Einstellungen gesperrt.</div>" \
  "<p><a class='btn' href='/calibration'>Justierung / Kalibrierung verwalten</a></p>" \
  "<script>function gen(){if(confirm('Jetzt für Geräte-SN %s einen dauerhaften Test-Geräteschlüssel erzeugen?'))location.href='/identity-keygen?sn=%s&confirm=YES';}</script></body></html>"

// Identity-Webtexte und HTML-Vorlagen verbleiben im QSPI-Flash.
struct EthIdentityFlashTextTable
{
  char t000[sizeof(ETH_IDENTITY_PAGE_TEMPLATE)];
  char t001[sizeof("<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>%s</title><style>body{font-family:Arial,sans-serif;max-width:800px;margin:30px auto;padding:0 16px;background:#111;color:#eee}.ok{color:#66ff66}.bad{color:#ff7777}a{color:#8fe7ff}</style></head><body><h1 class='%s'>%s</h1><p>%s</p><p><a href='/identity'>Zurück zur Zertifizierung</a></p></body></html>")];
  char t002[sizeof("Gerätezertifikat (schreibgeschützt)")];
  char t003[sizeof("Geräteeinstellung (vorläufig)")];
  char t004[sizeof("---")];
  char t005[sizeof("00000")];
  char t006[sizeof("<button onclick='gen()'>Geräteschlüssel erzeugen</button>")];
  char t007[sizeof("<span class='ok'>Geräteschlüssel vorhanden.</span>")];
  char t008[sizeof("<span class='warn'>Zuerst eine SN ungleich 00000 einstellen.</span>")];
  char t009[sizeof("<a class='btn' href='/identity-request.tpreq'>.tpreq herunterladen</a>")];
  char t010[sizeof("<span class='ok'>Gerät ist bereits zertifiziert.</span>")];
  char t011[sizeof("<span class='warn'>Zuerst Geräteschlüssel erzeugen.</span>")];
  char t012[sizeof("<a class='btn' href='/identity-cert-import'>Zertifikat von SD importieren</a>")];
  char t013[sizeof("<span class='ok'>Zertifikat ist aktiv.</span>")];
  char t014[sizeof("ok")];
  char t015[sizeof("bad")];
  char t016[sizeof("warn")];
  char t017[sizeof("Zertifizierungsseite ist zu gross")];
  char t018[sizeof("text/html; charset=utf-8")];
  char t019[sizeof("TP-3000")];
  char t020[sizeof("Erfolgreich")];
  char t021[sizeof("Fehler")];
  char t022[sizeof("")];
  char t023[sizeof("sn")];
  char t024[sizeof("confirm")];
  char t025[sizeof("YES")];
  char t026[sizeof("Schlüsselerzeugung abgebrochen")];
  char t027[sizeof("Bestätigung oder Geräte-SN fehlt.")];
  char t028[sizeof("Geräteschlüssel erzeugt")];
  char t029[sizeof("Geräteschlüssel nicht erzeugt")];
  char t030[sizeof("Der private P-256-Geräteschlüssel wurde intern gespeichert. Jetzt kann die .tpreq-Anfrage heruntergeladen werden.")];
  char t031[sizeof("HTTP/1.1 200 OK\r\nConnection: close\r\n")];
  char t032[sizeof("Content-Type: application/json; charset=utf-8\r\n")];
  char t033[sizeof("Content-Disposition: attachment; filename=\"")];
  char t034[sizeof("\"\r\nCache-Control: no-store\r\n")];
  char t035[sizeof("X-TP3000-SD-Copy: ")];
  char t036[sizeof("not-saved")];
  char t037[sizeof("\r\nContent-Length: ")];
  char t038[sizeof("\r\n\r\n")];
  char t039[sizeof("Zertifikat %s wurde geprüft und aktiviert. Verbindliche Geräte-SN: %s. Ab jetzt ist die normale Seriennummerneingabe gesperrt.")];
  char t040[sizeof("Gerätezertifikat aktiviert")];
  char t041[sizeof("Gerätezertifikat nicht importiert")];
};

static const EthIdentityFlashTextTable ethIdentityText PROGMEM =
{
  ETH_IDENTITY_PAGE_TEMPLATE,
  "<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>%s</title><style>body{font-family:Arial,sans-serif;max-width:800px;margin:30px auto;padding:0 16px;background:#111;color:#eee}.ok{color:#66ff66}.bad{color:#ff7777}a{color:#8fe7ff}</style></head><body><h1 class='%s'>%s</h1><p>%s</p><p><a href='/identity'>Zurück zur Zertifizierung</a></p></body></html>",
  "Gerätezertifikat (schreibgeschützt)",
  "Geräteeinstellung (vorläufig)",
  "---",
  "00000",
  "<button onclick='gen()'>Geräteschlüssel erzeugen</button>",
  "<span class='ok'>Geräteschlüssel vorhanden.</span>",
  "<span class='warn'>Zuerst eine SN ungleich 00000 einstellen.</span>",
  "<a class='btn' href='/identity-request.tpreq'>.tpreq herunterladen</a>",
  "<span class='ok'>Gerät ist bereits zertifiziert.</span>",
  "<span class='warn'>Zuerst Geräteschlüssel erzeugen.</span>",
  "<a class='btn' href='/identity-cert-import'>Zertifikat von SD importieren</a>",
  "<span class='ok'>Zertifikat ist aktiv.</span>",
  "ok",
  "bad",
  "warn",
  "Zertifizierungsseite ist zu gross",
  "text/html; charset=utf-8",
  "TP-3000",
  "Erfolgreich",
  "Fehler",
  "",
  "sn",
  "confirm",
  "YES",
  "Schlüsselerzeugung abgebrochen",
  "Bestätigung oder Geräte-SN fehlt.",
  "Geräteschlüssel erzeugt",
  "Geräteschlüssel nicht erzeugt",
  "Der private P-256-Geräteschlüssel wurde intern gespeichert. Jetzt kann die .tpreq-Anfrage heruntergeladen werden.",
  "HTTP/1.1 200 OK\r\nConnection: close\r\n",
  "Content-Type: application/json; charset=utf-8\r\n",
  "Content-Disposition: attachment; filename=\"",
  "\"\r\nCache-Control: no-store\r\n",
  "X-TP3000-SD-Copy: ",
  "not-saved",
  "\r\nContent-Length: ",
  "\r\n\r\n",
  "Zertifikat %s wurde geprüft und aktiviert. Verbindliche Geräte-SN: %s. Ab jetzt ist die normale Seriennummerneingabe gesperrt.",
  "Gerätezertifikat aktiviert",
  "Gerätezertifikat nicht importiert"
};
#undef ETH_IDENTITY_PAGE_TEMPLATE

static void FLASHMEM ethSendIdentityPage(EthBoundedWriter& client,
                                             EthernetClient& rawClient,
                                             const char* requestHeaders,
                                             bool* keepOpen)
{
  const bool certificateValid = deviceIdentityCertificateValid();
  const bool certificateStoredInvalid = deviceIdentityCertificateStoredInvalid();
  const char* effectiveSerial = deviceSerialGet();
  const char* serialSource = certificateValid ? ethIdentityText.t002 : ethIdentityText.t003;
  const char* keyId = deviceIdentityHasKey() ? deviceIdentityDeviceKeyId() : ethIdentityText.t004;
  const bool canGenerate = !deviceIdentityHasKey() && !certificateValid && strcmp(effectiveSerial, ethIdentityText.t005) != 0;
  const bool canRequest = deviceIdentityHasKey() && !certificateValid;
  const bool canImport = deviceIdentityHasKey() && !certificateValid;

  const char* keyAction = canGenerate ? ethIdentityText.t006 : (deviceIdentityHasKey() ? ethIdentityText.t007 : ethIdentityText.t008);
  const char* requestAction = canRequest ? ethIdentityText.t009 : (certificateValid ? ethIdentityText.t010 : ethIdentityText.t011);
  const char* importAction = canImport ? ethIdentityText.t012 : (certificateValid ? ethIdentityText.t013 : ethIdentityText.t011);

  const bool deviceReady = tpSignedCalibrationDeviceReady();
  const bool headReady = tpSignedCalibrationHeadReady();
  const TpSignedCalibrationStatus& deviceStatus = tpSignedCalibrationDeviceStatus();
  const TpSignedCalibrationStatus& headStatus = tpSignedCalibrationHeadStatus();
  const int64_t currentUtc = tpCurrentUtcUnixTime();
  const TpCalibrationTimeStatus deviceTime = tpSignedCalibrationTimeStatus(deviceStatus, currentUtc);
  const TpCalibrationTimeStatus headTime = tpSignedCalibrationTimeStatus(headStatus, currentUtc);
  const TpFirmwareRuntimeIdentity& firmware = tpSignedDataFirmwareIdentity();
  const TpFirmwareApprovalStatus& firmwareApproval = tpSignedDataFirmwareApprovalStatus();
  const bool firmwareReady = tpSignedDataFirmwareManifestReady();

  char certificateIssued[40];
  char deviceUntil[40];
  ethFormatValidityDateTime(deviceIdentityCertificateIssuedUtc(), certificateIssued, sizeof(certificateIssued));
  ethFormatValidityDateTime(deviceStatus.validUntilUtc, deviceUntil, sizeof(deviceUntil));
  char deviceSummary[72];
  if (deviceReady) snprintf(deviceSummary, sizeof(deviceSummary), "gültig bis %s", deviceUntil);
  else strncpy(deviceSummary, "noch nicht importiert", sizeof(deviceSummary) - 1U);
  deviceSummary[sizeof(deviceSummary) - 1U] = '\0';

  const char* certificateClass = certificateValid ? ethIdentityText.t014 : (certificateStoredInvalid ? ethIdentityText.t015 : ethIdentityText.t016);
  const char* firmwareClass = firmwareReady ? ethIdentityText.t014 : (firmwareApproval.manifestPresent ? ethIdentityText.t015 : ethIdentityText.t016);
  const char* firmwareStatus = firmwareApproval.statusText[0] != '\0' ? firmwareApproval.statusText : firmware.approvalStatus;

  const int length = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
    ethIdentityText.t000,
    certificateClass,
    deviceIdentityStatusText(),
    effectiveSerial,
    ethSignedCalibrationClass(deviceReady, deviceStatus, deviceTime),
    ethSignedCalibrationStatusText(deviceReady, deviceStatus, deviceTime, true),
    deviceSummary,
    ethSignedCalibrationClass(headReady, headStatus, headTime),
    ethSignedCalibrationStatusText(headReady, headStatus, headTime, false),
    headTypeTextGet(),
    (unsigned long)R.head_serial,
    firmwareClass,
    firmwareStatus,
    firmware.version,
    firmware.buildId,
    certificateClass,
    deviceIdentityStatusText(),
    deviceIdentityRootKeyId(),
    effectiveSerial,
    serialSource,
    keyId,
    certificateIssued,
    VERSION,
    keyAction,
    requestAction,
    importAction,
    effectiveSerial,
    effectiveSerial);

  if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, ethIdentityText.t017);
    return;
  }
  ethBeginGeneratedPageTransfer(rawClient, client, requestHeaders,
                                ETH_REQ_IDENTITY_PAGE,
                                ethJsonBuf, (size_t)length, keepOpen);
}

static void FLASHMEM ethSendIdentityResultPage(EthBoundedWriter& client,
                                                bool ok,
                                                const char* title,
                                                const char* message)
{
  const int length = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
    ethIdentityText.t001,
    title != nullptr ? title : ethIdentityText.t019,
    ok ? ethIdentityText.t014 : ethIdentityText.t015,
    title != nullptr ? title : (ok ? ethIdentityText.t020 : ethIdentityText.t021),
    message != nullptr ? message : ethIdentityText.t022);

  if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
  {
    ethSendText(client, ok, message);
    return;
  }
  ethSendBufferedBody(client,
                      ethIdentityText.t018,
                      true,
                      ethJsonBuf,
                      (size_t)length);
}

static void FLASHMEM ethHandleIdentityKeygen(EthBoundedWriter& client,
                                             const char* requestLine)
{
  char serial[8];
  char confirmation[8];
  char error[180];
  if (!ethGetQueryText(requestLine, ethIdentityText.t023, serial, sizeof(serial)) ||
      !ethGetQueryText(requestLine, ethIdentityText.t024, confirmation, sizeof(confirmation)) ||
      strcmp(confirmation, ethIdentityText.t025) != 0)
  {
    ethSendIdentityResultPage(client, false, ethIdentityText.t026,
                              ethIdentityText.t027);
    return;
  }

  const bool ok = deviceIdentityGenerateKey(serial, error, sizeof(error));
  ethSendIdentityResultPage(client,
                            ok,
                            ok ? ethIdentityText.t028 : ethIdentityText.t029,
                            ok ? ethIdentityText.t030
                               : error);
}

static void FLASHMEM ethHandleIdentityRequest(EthBoundedWriter& client)
{
  char downloadName[64];
  char error[180];
  char sdPath[96];
  size_t bodyLength = 0U;

  if (!deviceIdentityBuildRequestJson(ethJsonBuf,
                                      sizeof(ethJsonBuf),
                                      &bodyLength,
                                      downloadName,
                                      sizeof(downloadName),
                                      error,
                                      sizeof(error)))
  {
    ethSendText(client, false, error);
    return;
  }

  char sdError[160];
  const bool sdSaved = deviceIdentitySaveRequestToSd(ethJsonBuf,
                                                      bodyLength,
                                                      downloadName,
                                                      sdPath,
                                                      sizeof(sdPath),
                                                      sdError,
                                                      sizeof(sdError));

  client.print(ethIdentityText.t031);
  client.print(ethIdentityText.t032);
  client.print(ethIdentityText.t033);
  client.print(downloadName);
  client.print(ethIdentityText.t034);
  client.print(ethIdentityText.t035);
  client.print(sdSaved ? sdPath : ethIdentityText.t036);
  client.print(ethIdentityText.t037);
  client.print((unsigned long)bodyLength);
  client.print(ethIdentityText.t038);
  client.write((const uint8_t*)ethJsonBuf, bodyLength);
}

static void FLASHMEM ethHandleIdentityCertificateImport(EthBoundedWriter& client)
{
  char importedPath[96];
  char error[200];
  const bool ok = deviceIdentityImportCertificateFromSd(importedPath,
                                                         sizeof(importedPath),
                                                         error,
                                                         sizeof(error));
  if (ok)
  {
    // Ein live importiertes Geraetezertifikat kann die Bindung bereits auf SD
    // vorhandener Kalibriermanifeste aendern. Den Cache deshalb sofort neu
    // laden; ein Neustart ist fuer die Kalibrierseite nicht erforderlich.
    tpSignedCalibrationInvalidateCache();
    tpSignedCalibrationBegin();
    char message[256];
    snprintf(message, sizeof(message),
             ethIdentityText.t039,
             importedPath,
             deviceSerialGet());
    ethSendIdentityResultPage(client, true, ethIdentityText.t040, message);
  }
  else
  {
    ethSendIdentityResultPage(client, false, ethIdentityText.t041, error);
  }
}



// Kalibrier-Webtexte und HTML-Vorlagen liegen gesammelt im QSPI-Flash.
#define ETH_VALIDITY_PAGE_TEMPLATE \
  "<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>" \
  "<title>TP-3000 Info / Gültigkeit</title><style>body{font-family:Arial,sans-serif;max-width:980px;margin:24px auto;padding:0 16px;background:#111;color:#eee}" \
  "h1{font-size:28px}h2{margin-top:28px}table{border-collapse:collapse;width:100%%;background:#1b1b1b;margin-bottom:18px}" \
  "td{padding:9px;border-bottom:1px solid #444;vertical-align:top}td:first-child{width:235px;color:#bbb}" \
  "a{color:#8fe7ff}.ok{color:#7dff69}.warn{color:#ffd85a}.bad{color:#ff7777}code{color:#8fe7ff}</style></head>" \
  "<body><p><a href='/'>← Hauptanzeige</a></p><h1>Info / Gültigkeit</h1>" \
  "<h2>Gerät</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Geräte-SN</td><td><b>%s</b></td></tr><tr><td>Zertifikat ausgestellt</td><td>%s</td></tr></table>" \
  "<h2>Gerätejustierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Kalibriert am</td><td>%s</td></tr>" \
  "<tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr><tr><td>Calibration-ID</td><td><code>%s</code></td></tr><tr><td>Signer-Key-ID</td><td><code>%s</code></td></tr></table>" \
  "<h2>Kopfjustierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Kopftyp</td><td><b>%s</b></td></tr><tr><td>Kopf-SN</td><td><b>%05lu</b></td></tr>" \
  "<tr><td>Kalibriert am</td><td>%s</td></tr><tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr><tr><td>Calibration-ID</td><td><code>%s</code></td></tr><tr><td>Signer-Key-ID</td><td><code>%s</code></td></tr></table>" \
  "<h2>Firmware</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Version / Build</td><td>%s / %s</td></tr><tr><td>Freigabe- oder Buildtag</td><td>%s</td></tr></table>" \
  "</body></html>"

#define ETH_CALIBRATION_MANAGE_TEMPLATE \
  "<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>" \
  "<title>TP-3000 Justierung / Kalibrierung verwalten</title><style>body{font-family:Arial,sans-serif;max-width:1120px;margin:24px auto;padding:0 16px;background:#111;color:#eee}" \
  "h1{font-size:30px}h2{margin-top:26px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}.card{background:#1b1b1b;border:1px solid #444;border-radius:6px;padding:14px}" \
  "table{border-collapse:collapse;width:100%%}td{padding:7px;border-bottom:1px solid #3d3d3d;vertical-align:top}td:first-child{width:145px;color:#bbb}" \
  "a{color:#8fe7ff}a.btn{display:inline-block;margin:8px 8px 8px 0;padding:12px 16px;background:#333;color:#fff;border:1px solid #777;border-radius:6px;text-decoration:none}" \
  ".ok{color:#7dff69}.warn{color:#ffd85a}.bad{color:#ff7777}.box{padding:14px;background:#1b1b1b;border:1px solid #444;border-radius:6px;margin:18px 0}code{color:#8fe7ff}" \
  "@media(max-width:760px){.grid{grid-template-columns:1fr}}</style></head><body><p><a href='/identity'>← Zurück zur Zertifizierung</a></p><h1>Justierung / Kalibrierung verwalten</h1>" \
  "<div class='grid'><div class='card'><h2>Gerätejustierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Kalibriert am</td><td>%s</td></tr><tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr></table></div>" \
  "<div class='card'><h2>Kopfjustierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Kopf</td><td><b>%s / %05lu</b></td></tr><tr><td>Kalibriert am</td><td>%s</td></tr><tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr></table></div></div>" \
  "<h2>Anfragen exportieren</h2><p>%s</p><p>Beim Export wird zusätzlich eine Kopie in <code>/CALIBRATION/REQUESTS</code> auf der SD-Karte gespeichert.</p>" \
  "<h2>Signierte Freigaben importieren</h2><p>%s</p>" \
  "<div class='box'><b>Nachweise für den zertifizierten Logmodus:</b> %s<br><b>.TPSIG-Ausgabe:</b> <span class='ok'>aktiv – TP3000-LOG-SIGNATURE-2</span>.<br><b>.TPLOG-Ausgabe:</b> <span class='ok'>aktiv – TP3000-LOG-CONTAINER-1</span>.</div>" \
  "</body></html>"

struct EthCalibrationFlashTextTable
{
  char t000[sizeof("nicht beurteilbar")];
  char t001[sizeof("%02d.%02d.%04d %02d:%02d:%02d UTC")];
  char t002[sizeof("bad")];
  char t003[sizeof("warn")];
  char t004[sizeof("ok")];
  char t005[sizeof("Keine aktive signierte Gerätejustierung")];
  char t006[sizeof("Keine aktive signierte Kopfjustierung")];
  char t007[sizeof("Signiert und aktiv – innerhalb Kalibrierzeitraum")];
  char t008[sizeof("Signiert und aktiv – außerhalb Kalibrierzeitraum")];
  char t009[sizeof("Signiert und aktiv – Kalibrierzeitraum beginnt später")];
  char t010[sizeof("Signiert und aktiv – Kalibrierzeit nicht beurteilbar")];
  char t011[sizeof("–")];
  char t012[sizeof("<a class='btn' href='/calibration-device-request.tpdcalreq'>Gerätejustierung exportieren (.tpdcalreq)</a><a class='btn' href='/calibration-head-request.tphcalreq'>Kopfjustierung exportieren (.tphcalreq)</a>")];
  char t013[sizeof("<span class='bad'>Für Kalibrieranfragen ist zuerst ein gültiges Gerätezertifikat erforderlich.</span>")];
  char t014[sizeof("<a class='btn' href='/calibration-device-import'>Signierte Gerätejustierung von SD importieren</a><a class='btn' href='/calibration-head-import'>Signierte Kopfjustierung von SD importieren</a>")];
  char t015[sizeof("")];
  char t016[sizeof(ETH_VALIDITY_PAGE_TEMPLATE)];
  char t017[sizeof("<span class='ok'>Gerätezertifikat sowie Geräte- und Kopfjustierung vollständig</span>")];
  char t018[sizeof("Webseite ist zu groß")];
  char t019[sizeof("text/html; charset=utf-8")];
  char t020[sizeof("<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>%s</title><style>body{font-family:Arial,sans-serif;max-width:850px;margin:30px auto;padding:0 16px;background:#111;color:#eee}.ok{color:#7dff69}.bad{color:#ff7777}a{color:#8fe7ff}</style></head><body><h1 class='%s'>%s</h1><p>%s</p><p><a href='/calibration'>Zurück zu Justierung / Kalibrierung verwalten</a></p></body></html>")];
  char t021[sizeof("Kalibrierung")];
  char t022[sizeof("Erfolgreich")];
  char t023[sizeof("Fehler")];
  char t024[sizeof("Kalibrieranfrage nicht erzeugt")];
  char t025[sizeof("HTTP/1.1 200 OK\r\n")];
  char t026[sizeof("Connection: close\r\n")];
  char t027[sizeof("Content-Type: application/json; charset=utf-8\r\n")];
  char t028[sizeof("Content-Disposition: attachment; filename=\"")];
  char t029[sizeof("\"\r\n")];
  char t030[sizeof("Cache-Control: no-store\r\n")];
  char t031[sizeof("X-TP3000-SD-Copy: ")];
  char t032[sizeof("not-saved")];
  char t033[sizeof("\r\nContent-Length: ")];
  char t034[sizeof("\r\n\r\n")];
  char t035[sizeof("%s <code>%s</code> wurde kryptografisch geprüft und aktiviert. Der Kalibrierzeitraum wird nur informativ bewertet und blockiert die Messung nicht.")];
  char t036[sizeof("Gerätejustierung")];
  char t037[sizeof("Kopfjustierung")];
  char t038[sizeof("Signierte Gerätejustierung aktiviert")];
  char t039[sizeof("Signierte Kopfjustierung aktiviert")];
  char t040[sizeof("Gerätejustierung nicht importiert")];
  char t041[sizeof("Kopfjustierung nicht importiert")];
  char t042[sizeof(ETH_CALIBRATION_MANAGE_TEMPLATE)];
};

static const EthCalibrationFlashTextTable ethCalibrationText PROGMEM =
{
  "nicht beurteilbar",
  "%02d.%02d.%04d %02d:%02d:%02d UTC",
  "bad",
  "warn",
  "ok",
  "Keine aktive signierte Gerätejustierung",
  "Keine aktive signierte Kopfjustierung",
  "Signiert und aktiv – innerhalb Kalibrierzeitraum",
  "Signiert und aktiv – außerhalb Kalibrierzeitraum",
  "Signiert und aktiv – Kalibrierzeitraum beginnt später",
  "Signiert und aktiv – Kalibrierzeit nicht beurteilbar",
  "–",
  "<a class='btn' href='/calibration-device-request.tpdcalreq'>Gerätejustierung exportieren (.tpdcalreq)</a><a class='btn' href='/calibration-head-request.tphcalreq'>Kopfjustierung exportieren (.tphcalreq)</a>",
  "<span class='bad'>Für Kalibrieranfragen ist zuerst ein gültiges Gerätezertifikat erforderlich.</span>",
  "<a class='btn' href='/calibration-device-import'>Signierte Gerätejustierung von SD importieren</a><a class='btn' href='/calibration-head-import'>Signierte Kopfjustierung von SD importieren</a>",
  "",
  ETH_VALIDITY_PAGE_TEMPLATE,
  "<span class='ok'>Gerätezertifikat sowie Geräte- und Kopfjustierung vollständig</span>",
  "Webseite ist zu groß",
  "text/html; charset=utf-8",
  "<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>%s</title><style>body{font-family:Arial,sans-serif;max-width:850px;margin:30px auto;padding:0 16px;background:#111;color:#eee}.ok{color:#7dff69}.bad{color:#ff7777}a{color:#8fe7ff}</style></head><body><h1 class='%s'>%s</h1><p>%s</p><p><a href='/calibration'>Zurück zu Justierung / Kalibrierung verwalten</a></p></body></html>",
  "Kalibrierung",
  "Erfolgreich",
  "Fehler",
  "Kalibrieranfrage nicht erzeugt",
  "HTTP/1.1 200 OK\r\n",
  "Connection: close\r\n",
  "Content-Type: application/json; charset=utf-8\r\n",
  "Content-Disposition: attachment; filename=\"",
  "\"\r\n",
  "Cache-Control: no-store\r\n",
  "X-TP3000-SD-Copy: ",
  "not-saved",
  "\r\nContent-Length: ",
  "\r\n\r\n",
  "%s <code>%s</code> wurde kryptografisch geprüft und aktiviert. Der Kalibrierzeitraum wird nur informativ bewertet und blockiert die Messung nicht.",
  "Gerätejustierung",
  "Kopfjustierung",
  "Signierte Gerätejustierung aktiviert",
  "Signierte Kopfjustierung aktiviert",
  "Gerätejustierung nicht importiert",
  "Kopfjustierung nicht importiert",
  ETH_CALIBRATION_MANAGE_TEMPLATE
};
#undef ETH_VALIDITY_PAGE_TEMPLATE
#undef ETH_CALIBRATION_MANAGE_TEMPLATE


// V0.50.1_26: Die eigentliche Kalibrierung gilt für das komplette Messsystem
// aus Grundgerät und aktuell angeschlossenem Sensorkopf. Gerät und Kopf bleiben
// weiterhin getrennte Justierungen; die Systemkalibrierung wird als dritter,
// eigenständiger und kryptografisch gebundener Nachweis dargestellt.
static const char ETH_VALIDITY_PAGE_TEMPLATE_V25[] PROGMEM =
  "<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>TP-3000 Info / Gültigkeit</title><style>body{font-family:Arial,sans-serif;max-width:980px;margin:24px auto;padding:0 16px;background:#111;color:#eee}"
  "h1{font-size:28px}h2{margin-top:28px}table{border-collapse:collapse;width:100%%;background:#1b1b1b;margin-bottom:18px}"
  "td{padding:9px;border-bottom:1px solid #444;vertical-align:top}td:first-child{width:235px;color:#bbb}"
  "a{color:#8fe7ff}.ok{color:#7dff69}.warn{color:#ffd85a}.bad{color:#ff7777}.muted{color:#bbb}code{color:#8fe7ff;overflow-wrap:anywhere}</style></head>"
  "<body><p><a href='/'>← Hauptanzeige</a></p><h1>Info / Gültigkeit</h1>"
  "<h2>Gerät</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Geräte-SN</td><td><b>%s</b></td></tr><tr><td>Zertifikat ausgestellt</td><td>%s</td></tr></table>"
  "<h2>Gerätejustierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Justiert am</td><td>%s</td></tr>"
  "<tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr><tr><td>Justierungs-ID</td><td><code>%s</code></td></tr><tr><td>Signer-Key-ID</td><td><code>%s</code></td></tr></table>"
  "<h2>Kopfjustierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Kopftyp</td><td><b>%s</b></td></tr><tr><td>Kopf-SN</td><td><b>%05lu</b></td></tr>"
  "<tr><td>Justiert am</td><td>%s</td></tr><tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr><tr><td>Justierungs-ID</td><td><code>%s</code></td></tr><tr><td>Signer-Key-ID</td><td><code>%s</code></td></tr></table>"
  "<h2>Systemkalibrierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Gerät / Kopf</td><td><b>G%s / %s K%05lu</b></td></tr>"
  "<tr><td>Kalibrierumfang</td><td>%s</td></tr><tr><td>Kalibriert am</td><td>%s</td></tr><tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Nächste Kalibrierung</td><td>%s</td></tr>"
  "<tr><td>Kalibrierintervall</td><td>%u Monate</td></tr><tr><td>Zertifikatsaussteller</td><td>%s</td></tr><tr><td>Bearbeiter</td><td>%s</td></tr>"
  "<tr><td>Kalibrierscheinquelle</td><td>%s</td></tr>%s"
  "<tr><td>Calibration-ID</td><td><code>%s</code></td></tr><tr><td>Signer-Key-ID</td><td><code>%s</code></td></tr>"
  "<tr><td>Gebundene Gerätejustierung</td><td><code>%s</code></td></tr><tr><td>Gebundene Kopfjustierung</td><td><code>%s</code></td></tr>"
  "<tr><td>Firmwarebezug</td><td class='%s'><b>%s</b></td></tr><tr><td>Firmware-SHA-256 bei Kalibrierung</td><td><code>%s</code></td></tr></table>"
  "<h2>Firmware</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Version / Build</td><td>%s / %s</td></tr><tr><td>Buildtag</td><td>%s</td></tr>"
  "<tr><td>Firmwareabbild</td><td>%lu Byte</td></tr><tr><td>Soll-SHA-256</td><td><code>%s</code></td></tr><tr><td>Ist-SHA-256</td><td><code>%s</code></td></tr>"
  "<tr><td>Firmwarezertifikat</td><td><code>%s</code></td></tr><tr><td>Hersteller-Root</td><td><code>%s</code></td></tr><tr><td>Zertifiziert am</td><td>%s</td></tr><tr><td>Geprüft</td><td>%s</td></tr></table>"
  "</body></html>";

static const char ETH_CAL_REQUEST_UNAVAILABLE[] PROGMEM =
  "<span class='bad'>Für die Anfrage ist zuerst ein gültiges Gerätezertifikat erforderlich.</span>";
static const char ETH_CAL_DEVICE_REQUEST_CONTROL[] PROGMEM =
  "<a class='btn' href='/calibration-device-request.tpdcalreq'>Anfrage herunterladen (.tpdcalreq)</a>";
static const char ETH_CAL_HEAD_REQUEST_CONTROL[] PROGMEM =
  "<a class='btn' href='/calibration-head-request.tphcalreq'>Anfrage herunterladen (.tphcalreq)</a>";
static const char ETH_CAL_SYSTEM_REQUEST_CONTROL[] PROGMEM =
  "<a class='btn' href='/calibration-system-request.tpscalreq'>Anfrage herunterladen (.tpscalreq)</a>";
static const char ETH_CAL_SYSTEM_REQUEST_BLOCKED[] PROGMEM =
  "<span class='warn'>Die Anfrage wird freigeschaltet, sobald Geräte- und Kopfjustierung gültig aktiv sind.</span>";

static const char ETH_CALIBRATION_MANAGE_TEMPLATE_V25[] PROGMEM =
  "<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>TP-3000 Justierung / Systemkalibrierung verwalten</title><style>body{font-family:Arial,sans-serif;max-width:1120px;margin:24px auto;padding:0 16px;background:#111;color:#eee}"
  "h1{font-size:30px}h2{margin-top:26px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}.card{background:#1b1b1b;border:1px solid #444;border-radius:6px;padding:14px;display:flex;flex-direction:column}.wide{grid-column:1/-1}.card>h2,.box>h2{margin-top:6px}"
  "table{border-collapse:collapse;width:100%%}td{padding:7px;border-bottom:1px solid #3d3d3d;vertical-align:top}td:first-child{width:190px;color:#bbb}"
  "a{color:#8fe7ff}a.btn,button{display:inline-block;margin:8px 8px 8px 0;padding:12px 16px;background:#333;color:#fff;border:1px solid #777;border-radius:6px;text-decoration:none;font-size:15px}button:disabled{opacity:.45}"
  "input[type=file],input[type=text],input[type=date]{display:block;box-sizing:border-box;width:100%%;margin:7px 0 12px;padding:9px;background:#101010;color:#eee;border:1px solid #666;border-radius:4px}.fieldGrid{display:grid;grid-template-columns:1.3fr .8fr .8fr;gap:12px}label{color:#ccc}"
  ".ok{color:#7dff69}.warn{color:#ffd85a}.bad{color:#ff7777}.box{padding:14px;background:#1b1b1b;border:1px solid #444;border-radius:6px;margin:18px 0}.actions{margin-top:14px;padding-top:12px;border-top:1px solid #4a4a4a}.card .actions{margin-top:auto}.btnrow{margin-bottom:4px}.hint{color:#bbb;font-size:13px;line-height:1.35;margin:5px 0 2px}.uploadLabel{display:block;margin-top:8px}code{color:#8fe7ff;overflow-wrap:anywhere}progress{width:100%%;height:20px}"
  "@media(max-width:900px){.fieldGrid{grid-template-columns:1fr}}@media(max-width:760px){.grid{grid-template-columns:1fr}.wide{grid-column:auto}td:first-child{width:145px}}</style></head><body><p><a href='/identity'>← Zurück zur Zertifizierung</a></p><h1>Justierung / Systemkalibrierung verwalten</h1>"
  "<div class='grid'><div class='card'><h2>Gerätejustierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Justiert am</td><td>%s</td></tr><tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr></table>"
  "<div class='actions'><div class='btnrow'>%s<a class='btn' href='/calibration-device-import'>Von SD importieren</a></div><label class='uploadLabel'>Signierte Gerätejustierung vom PC<input id='deviceFile' type='file' accept='.tpdcal,application/json'></label><button id='deviceUpload' disabled>Gerätejustierung prüfen und aktivieren</button><p id='deviceMessage' class='warn'></p><p class='hint'>Die Anfrage wird beim Herunterladen zusätzlich unter <code>/CALIBRATION/REQUESTS</code> auf SD gespeichert.</p></div></div>"
  "<div class='card'><h2>Kopfjustierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Kopf</td><td><b>%s / %05lu</b></td></tr><tr><td>Justiert am</td><td>%s</td></tr><tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr></table>"
  "<div class='actions'><div class='btnrow'>%s<a class='btn' href='/calibration-head-import'>Von SD importieren</a></div><label class='uploadLabel'>Signierte Kopfjustierung vom PC<input id='headFile' type='file' accept='.tphcal,application/json'></label><button id='headUpload' disabled>Kopfjustierung prüfen und aktivieren</button><p id='headMessage' class='warn'></p><p class='hint'>Die Anfrage wird beim Herunterladen zusätzlich unter <code>/CALIBRATION/REQUESTS</code> auf SD gespeichert.</p></div></div>"
  "<div class='card wide'><h2>Systemkalibrierung</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Gerät / Kopf</td><td><b>G%s / %s K%05lu</b></td></tr>"
  "<tr><td>Kalibrierumfang</td><td>%s</td></tr><tr><td>Kalibriert am</td><td>%s</td></tr><tr><td>Gültig ab</td><td>%s</td></tr><tr><td>Nächste Kalibrierung</td><td>%s</td></tr><tr><td>Kalibrierscheinquelle</td><td>%s</td></tr>%s"
  "<tr><td>Firmwarebezug</td><td class='%s'><b>%s</b></td></tr><tr><td>Firmware-SHA-256 bei Kalibrierung</td><td><code>%s</code></td></tr><tr><td>Calibration-ID</td><td><code>%s</code></td></tr></table>"
  "<div class='actions'><div class='btnrow'>%s<a class='btn' href='/calibration-system-import'>Von SD importieren</a></div><label class='uploadLabel'>Signierte Systemkalibrierung vom PC<input id='systemFile' type='file' accept='.tpscal,application/json'></label><button id='systemUpload' disabled>Systemkalibrierung prüfen und aktivieren</button><p id='systemMessage' class='warn'></p></div></div></div>"
  "<div class='box'><h2>Firmwarezertifikat</h2><table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Version / Build</td><td>%s / %s</td></tr><tr><td>Firmwareabbild</td><td>%lu Byte</td></tr><tr><td>Ist-SHA-256</td><td><code>%s</code></td></tr><tr><td>Zertifikats-ID</td><td><code>%s</code></td></tr><tr><td>Hersteller-Root</td><td><code>%s</code></td></tr><tr><td>Zertifiziert am</td><td>%s</td></tr></table>"
  "<div class='actions'><div class='btnrow'><a class='btn' href='/firmware-request.tpfwreq'>Firmwareanfrage herunterladen (.tpfwreq)</a><a class='btn' href='/firmware-certificate-import'>Von SD importieren</a></div><label class='uploadLabel'>Firmwarezertifikat vom PC<input id='firmwareFile' type='file' accept='.tpfwcert,application/json'></label><button id='firmwareUpload' disabled>Firmwarezertifikat prüfen und aktivieren</button><p id='firmwareMessage' class='warn'></p></div></div>"
  "<div class='box'><h2>Externer Kalibrierschein (PDF)</h2><p>Das Original-PDF wird unverändert und ausschließlich auf der SD-Karte archiviert. Maximal 6 MiB.</p>"
  "<table><tr><td>Status</td><td class='%s'><b>%s</b></td></tr><tr><td>Kalibrierschein-Nr. / Zeichen</td><td>%s</td></tr><tr><td>Gültig von</td><td>%s</td></tr><tr><td>Gültig bis</td><td>%s</td></tr><tr><td>Originaldatei</td><td>%s</td></tr><tr><td>Dateigröße</td><td>%lu Byte</td></tr><tr><td>Dokument-ID</td><td><code>%s</code></td></tr><tr><td>SHA-256</td><td><code>%s</code></td></tr></table><p>%s</p>"
  "<div class='fieldGrid'><label>Kalibrierschein-Nr. / Zeichen (max. 32)<input id='externalNumber' type='text' maxlength='32' autocomplete='off'></label><label>Gültig von<input id='externalFrom' type='date'></label><label>Gültig bis<input id='externalUntil' type='date'></label></div>"
  "<label>PDF-Datei<input id='externalFile' type='file' accept='.pdf,application/pdf'></label><button id='externalUpload' disabled>Externen Kalibrierschein prüfen und auf SD speichern</button><progress id='externalProgress' max='100' value='0'></progress><p id='externalMessage' class='warn'></p></div>"
  "<div class='box'><b>Nachweise für den zertifizierten Logmodus:</b> %s<br><b>.TPSIG-Ausgabe:</b> <span class='ok'>aktiv – TP3000-LOG-SIGNATURE-2</span>.<br><b>.TPLOG-Ausgabe:</b> <span class='ok'>aktiv – TP3000-LOG-CONTAINER-1</span>.</div>"
  "<script>(()=>{const E=id=>document.getElementById(id);function bind(fileId,buttonId,messageId,url){const f=E(fileId),b=E(buttonId),m=E(messageId);f.addEventListener('change',()=>{b.disabled=!(f.files&&f.files.length);m.textContent='';m.className='warn';});b.addEventListener('click',async()=>{if(!(f.files&&f.files.length))return;b.disabled=true;m.className='warn';m.textContent='Datei wird vollständig übertragen und geprüft …';try{const text=await f.files[0].text();const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:text,cache:'no-store'});const answer=await r.text();m.className=r.ok?'ok':'bad';m.textContent=answer;if(r.ok)setTimeout(()=>location.reload(),1400);}catch(e){m.className='bad';m.textContent='Upload fehlgeschlagen: '+e;}finally{if(m.className!=='ok')b.disabled=false;}});}bind('deviceFile','deviceUpload','deviceMessage','/calibration-device-upload');bind('headFile','headUpload','headMessage','/calibration-head-upload');bind('systemFile','systemUpload','systemMessage','/calibration-system-upload');bind('firmwareFile','firmwareUpload','firmwareMessage','/firmware-certificate-upload');"
  "const f=E('externalFile'),n=E('externalNumber'),vf=E('externalFrom'),vu=E('externalUntil'),b=E('externalUpload'),m=E('externalMessage'),p=E('externalProgress');function ready(){b.disabled=!(f.files&&f.files.length&&n.value.trim()&&vf.value&&vu.value)}[f,n,vf,vu].forEach(x=>x.addEventListener('input',ready));f.addEventListener('change',ready);b.addEventListener('click',async()=>{const file=f.files&&f.files[0],number=n.value.trim();if(!file)return;if(number.length<1||number.length>32){m.className='bad';m.textContent='Kalibrierschein-Nr. / Zeichen muss 1 bis 32 Zeichen lang sein.';return}if(!vf.value||!vu.value||vu.value<vf.value){m.className='bad';m.textContent='Gültigkeitszeitraum ist ungültig.';return}if(file.size<5||file.size>6291456){m.className='bad';m.textContent='PDF muss zwischen 5 Byte und 6 MiB groß sein.';return}b.disabled=true;p.value=0;m.className='warn';m.textContent='PDF wird blockweise auf SD geschrieben und zweimal per SHA-256 geprüft …';const ymd=x=>x.replaceAll('-','');try{let r=await fetch('/calibration-external-pdf-begin?number='+encodeURIComponent(number)+'&from='+ymd(vf.value)+'&until='+ymd(vu.value)+'&name='+encodeURIComponent(file.name)+'&size='+file.size,{method:'POST'});let answer=await r.text();if(!r.ok)throw new Error(answer);const chunk=16000;let sent=0;while(sent<file.size){const next=Math.min(file.size,sent+chunk);r=await fetch('/calibration-external-pdf-chunk?offset='+sent,{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:file.slice(sent,next)});answer=await r.text();if(!r.ok)throw new Error(answer);sent=next;p.value=Math.round(sent*100/file.size);m.textContent='Übertragen: '+p.value+' %%'}r=await fetch('/calibration-external-pdf-finish',{method:'POST'});answer=await r.text();if(!r.ok)throw new Error(answer);p.value=100;m.className='ok';m.textContent=answer;setTimeout(()=>location.reload(),1600)}catch(e){try{await fetch('/calibration-external-pdf-abort',{method:'POST'})}catch(_){}m.className='bad';m.textContent='PDF-Upload fehlgeschlagen: '+(e.message||e);b.disabled=false}});})();</script>"
  "</body></html>";

static TpCalibrationTimeStatus FLASHMEM ethSystemCalibrationTimeStatus(
    const TpSystemCalibrationStatus& status,
    int64_t unixTimeUtc)
{
  if (!status.signatureValid || unixTimeUtc <= 0 || status.validFromUtc <= 0 ||
      status.validUntilUtc < status.validFromUtc) return TP_CAL_TIME_UNKNOWN;
  if (unixTimeUtc < status.validFromUtc) return TP_CAL_TIME_NOT_YET_VALID;
  if (unixTimeUtc > status.validUntilUtc) return TP_CAL_TIME_EXPIRED;
  return TP_CAL_TIME_VALID;
}

static TpSystemFirmwareApplicability FLASHMEM ethLoadSystemFirmwareApplicability(
    const TpSystemCalibrationStatus& status)
{
  memset(&ethSystemFirmwareContextStatus, 0,
         sizeof(ethSystemFirmwareContextStatus));
  if (!status.signatureValid || status.sourceRequestId[0] == '\0' ||
      status.sourceRequestManifestSha256[0] == '\0')
    return TP_SYSTEM_FW_APPLICABILITY_NOT_COMPARABLE;
  if (!tpSystemFirmwareContextLoad(status.sourceRequestId,
                                   status.sourceRequestManifestSha256,
                                   ethSystemFirmwareContextStatus))
    return TP_SYSTEM_FW_APPLICABILITY_NOT_COMPARABLE;
  return ethSystemFirmwareContextStatus.applicability;
}

static const char* FLASHMEM ethSystemFirmwareApplicabilityClass(
    TpSystemFirmwareApplicability applicability)
{
  switch (applicability)
  {
    case TP_SYSTEM_FW_APPLICABILITY_EXACT_MATCH: return "ok";
    case TP_SYSTEM_FW_APPLICABILITY_CHANGED: return "warn";
    default: return "warn";
  }
}

static const char* FLASHMEM ethSystemFirmwareApplicabilityStatusText(
    TpSystemFirmwareApplicability applicability)
{
  switch (applicability)
  {
    case TP_SYSTEM_FW_APPLICABILITY_EXACT_MATCH:
      return "Aktuelle Firmware entspricht exakt dem Stand der Systemkalibrierung";
    case TP_SYSTEM_FW_APPLICABILITY_CHANGED:
      return "Aktuelle Firmware weicht vom dokumentierten Kalibrierstand ab; Kalibrierschein bleibt gültig";
    default:
      return "Firmwarebezug der Systemkalibrierung nicht beurteilbar";
  }
}

static const char* FLASHMEM ethSystemCalibrationClass(
    bool ready,
    const TpSystemCalibrationStatus& status,
    TpCalibrationTimeStatus timeStatus,
    TpSystemFirmwareApplicability firmwareApplicability)
{
  if (!status.filePresent) return "warn";
  if (!status.signatureValid || !status.bindingMatchesCurrent || !ready) return "bad";
  if (firmwareApplicability == TP_SYSTEM_FW_APPLICABILITY_CHANGED) return "warn";
  if (firmwareApplicability == TP_SYSTEM_FW_APPLICABILITY_NOT_COMPARABLE) return "warn";
  return timeStatus == TP_CAL_TIME_VALID ? "ok" : "warn";
}

static const char* FLASHMEM ethSystemCalibrationStatusText(
    bool ready,
    const TpSystemCalibrationStatus& status,
    TpCalibrationTimeStatus timeStatus,
    TpSystemFirmwareApplicability firmwareApplicability)
{
  if (!status.filePresent) return "Keine aktive signierte Systemkalibrierung";
  if (!status.signatureValid || !status.bindingMatchesCurrent || !ready)
    return status.lastError[0] != '\0' ? status.lastError : "Systemkalibrierung ist ungültig";
  if (firmwareApplicability == TP_SYSTEM_FW_APPLICABILITY_CHANGED)
    return "Signiert und gültig – aktuelle Firmware weicht vom Kalibrierstand ab";
  if (firmwareApplicability == TP_SYSTEM_FW_APPLICABILITY_NOT_COMPARABLE)
    return "Signiert und aktiv – Firmwarebezug nicht beurteilbar";
  switch (timeStatus)
  {
    case TP_CAL_TIME_VALID: return "Signiert und aktiv – innerhalb Kalibrierintervall";
    case TP_CAL_TIME_EXPIRED: return "Signiert und aktiv – Kalibrierintervall überschritten";
    case TP_CAL_TIME_NOT_YET_VALID: return "Signiert – Kalibrierintervall beginnt später";
    default: return "Signiert und aktiv – Kalibrierzeit nicht beurteilbar";
  }
}

static const char* FLASHMEM ethSystemCalibrationScopeText(const char* scope)
{
  if (scope == nullptr) return "–";
  if (strcmp(scope, "AS_FOUND") == 0) return "Nur As Found";
  if (strcmp(scope, "AS_LEFT") == 0) return "Nur As Left";
  if (strcmp(scope, "AS_FOUND_AS_LEFT_NO_ADJUSTMENT") == 0) return "As Found / As Left – ohne Justierung";
  if (strcmp(scope, "BEFORE_AFTER_ADJUSTMENT") == 0) return "Vor und nach Justierung";
  if (strcmp(scope, "AS_FOUND_AS_LEFT") == 0) return "Vor und nach Justierung (Altformat)";
  return scope[0] != '\0' ? scope : "–";
}

static const char* FLASHMEM ethSystemCertificateSourceText(const TpSystemCalibrationStatus& status)
{
  return status.externalCertificateBound
    ? ETH_EXTERNAL_SOURCE_PDF
    : ETH_EXTERNAL_SOURCE_OWN;
}

static uint32_t FLASHMEM ethCurrentUtcYmd(int64_t unixTimeUtc)
{
  if (unixTimeUtc <= 0) return 0U;
  const time_t value = (time_t)unixTimeUtc;
  return (uint32_t)year(value) * 10000UL +
         (uint32_t)month(value) * 100UL +
         (uint32_t)day(value);
}

static TpCalibrationTimeStatus FLASHMEM ethExternalCalibrationTimeStatus(
    const TpExternalCalibrationMetadata& metadata,
    int64_t unixTimeUtc)
{
  if (!metadata.present || metadata.validFromYmd == 0U ||
      metadata.validUntilYmd < metadata.validFromYmd) return TP_CAL_TIME_UNKNOWN;
  const uint32_t nowYmd = ethCurrentUtcYmd(unixTimeUtc);
  if (nowYmd == 0U) return TP_CAL_TIME_UNKNOWN;
  if (nowYmd < metadata.validFromYmd) return TP_CAL_TIME_NOT_YET_VALID;
  if (nowYmd > metadata.validUntilYmd) return TP_CAL_TIME_EXPIRED;
  return TP_CAL_TIME_VALID;
}

static const char* FLASHMEM ethExternalCalibrationClass(
    const TpExternalCalibrationMetadata& metadata,
    TpCalibrationTimeStatus timeStatus)
{
  if (!metadata.present) return "warn";
  return timeStatus == TP_CAL_TIME_VALID ? "ok" : "warn";
}

static const char* FLASHMEM ethExternalCalibrationStatusText(
    const TpExternalCalibrationMetadata& metadata,
    TpCalibrationTimeStatus timeStatus)
{
  if (!metadata.present) return ETH_EXTERNAL_STATUS_NONE;
  switch (timeStatus)
  {
    case TP_CAL_TIME_VALID: return ETH_EXTERNAL_STATUS_VALID;
    case TP_CAL_TIME_EXPIRED: return ETH_EXTERNAL_STATUS_EXPIRED;
    case TP_CAL_TIME_NOT_YET_VALID: return ETH_EXTERNAL_STATUS_FUTURE;
    default: return ETH_EXTERNAL_STATUS_UNKNOWN;
  }
}

static bool FLASHMEM ethHtmlEscape(const char* input, char* output, size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return false;
  if (input == nullptr) input = "";
  size_t out = 0U;
  for (const char* p = input; *p != '\0'; ++p)
  {
    const char* replacement = nullptr;
    switch (*p)
    {
      case '&': replacement = "&amp;"; break;
      case '<': replacement = "&lt;"; break;
      case '>': replacement = "&gt;"; break;
      case '\"': replacement = "&quot;"; break;
      case '\'': replacement = "&#39;"; break;
      default: break;
    }
    if (replacement != nullptr)
    {
      const size_t length = strlen(replacement);
      if (out + length + 1U > outputSize) return false;
      memcpy(output + out, replacement, length);
      out += length;
    }
    else
    {
      if (out + 2U > outputSize) return false;
      output[out++] = *p;
    }
  }
  output[out] = '\0';
  return true;
}

static void FLASHMEM ethBuildSystemExternalRows(
    const TpSystemCalibrationStatus& status,
    char* output,
    size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return;
  output[0] = '\0';
  if (!status.externalCertificateBound) return;
  char from[16], until[16];
  if (!ethHtmlEscape(status.externalCertificateNumber, ethExternalNumberEscaped, sizeof(ethExternalNumberEscaped))) strcpy(ethExternalNumberEscaped, "–");
  if (!ethHtmlEscape(status.externalCertificateOriginalFileName, ethExternalFileEscaped, sizeof(ethExternalFileEscaped))) strcpy(ethExternalFileEscaped, "–");
  if (!ethHtmlEscape(status.externalCertificateSha256, ethExternalHashEscaped, sizeof(ethExternalHashEscaped))) strcpy(ethExternalHashEscaped, "–");
  ethFormatValidityYmd(status.externalCertificateValidFromYmd, from, sizeof(from));
  ethFormatValidityYmd(status.externalCertificateValidUntilYmd, until, sizeof(until));
  snprintf(output, outputSize, ETH_EXTERNAL_ROWS_TEMPLATE,
    ethExternalNumberEscaped, from, until, ethExternalFileEscaped, ethExternalHashEscaped);
}

static void FLASHMEM ethPrepareCalibrationStrings(
    char* certificateIssued, size_t certificateIssuedSize,
    char* deviceDate, size_t deviceDateSize,
    char* deviceFrom, size_t deviceFromSize,
    char* deviceUntil, size_t deviceUntilSize,
    char* headDate, size_t headDateSize,
    char* headFrom, size_t headFromSize,
    char* headUntil, size_t headUntilSize,
    char* firmwareDate, size_t firmwareDateSize)
{
  const TpSignedCalibrationStatus& deviceStatus = tpSignedCalibrationDeviceStatus();
  const TpSignedCalibrationStatus& headStatus = tpSignedCalibrationHeadStatus();
  const TpFirmwareRuntimeIdentity& firmware = tpSignedDataFirmwareIdentity();
  const TpFirmwareApprovalStatus& firmwareApproval = tpSignedDataFirmwareApprovalStatus();
  ethFormatValidityDateTime(deviceIdentityCertificateIssuedUtc(), certificateIssued, certificateIssuedSize);
  ethFormatValidityYmd(deviceStatus.calibrationDateYmd, deviceDate, deviceDateSize);
  ethFormatValidityDateTime(deviceStatus.validFromUtc, deviceFrom, deviceFromSize);
  ethFormatValidityDateTime(deviceStatus.validUntilUtc, deviceUntil, deviceUntilSize);
  ethFormatValidityYmd(headStatus.calibrationDateYmd, headDate, headDateSize);
  ethFormatValidityDateTime(headStatus.validFromUtc, headFrom, headFromSize);
  ethFormatValidityDateTime(headStatus.validUntilUtc, headUntil, headUntilSize);
  ethFormatValidityYmd(tpSignedDataFirmwareManifestReady() ? firmwareApproval.approvalDateYmd : firmware.buildDateYmd,
                       firmwareDate, firmwareDateSize);
}

static void FLASHMEM ethSendValidityPage(EthBoundedWriter& client,
                                             EthernetClient& rawClient,
                                             const char* requestHeaders,
                                             bool* keepOpen)
{
  const bool certificateValid = deviceIdentityCertificateValid();
  const bool certificateStoredInvalid = deviceIdentityCertificateStoredInvalid();
  const bool deviceReady = tpSignedCalibrationDeviceReady();
  const bool headReady = tpSignedCalibrationHeadReady();
  const bool systemReady = tpSignedCalibrationSystemReady();
  const TpSignedCalibrationStatus& deviceStatus = tpSignedCalibrationDeviceStatus();
  const TpSignedCalibrationStatus& headStatus = tpSignedCalibrationHeadStatus();
  const TpSystemCalibrationStatus& systemStatus = tpSignedCalibrationSystemStatus();
  const TpFirmwareRuntimeIdentity& firmware = tpSignedDataFirmwareIdentity();
  const TpFirmwareIntegrityStatus& firmwareIntegrity = tpFirmwareIntegrityStatus();
  const bool firmwareReady = tpSignedDataFirmwareManifestReady();
  const int64_t currentUtc = tpCurrentUtcUnixTime();
  const TpCalibrationTimeStatus deviceTime = tpSignedCalibrationTimeStatus(deviceStatus, currentUtc);
  const TpCalibrationTimeStatus headTime = tpSignedCalibrationTimeStatus(headStatus, currentUtc);
  const TpCalibrationTimeStatus systemTime = ethSystemCalibrationTimeStatus(systemStatus, currentUtc);
  const TpSystemFirmwareApplicability systemFirmwareApplicability =
      ethLoadSystemFirmwareApplicability(systemStatus);
  const char* systemFirmwareHashAtCalibration =
      ethSystemFirmwareContextStatus.recordValid
        ? ethSystemFirmwareContextStatus.firmwareSha256 : "–";

  char certificateIssued[40], deviceDate[16], deviceFrom[40], deviceUntil[40];
  char headDate[16], headFrom[40], headUntil[40], firmwareDate[16];
  char systemDate[16], systemFrom[40], systemUntil[40];
  char firmwareExpectedHash[65], firmwareMeasuredHash[65], firmwareVerified[40], firmwareApproved[40];
  ethPrepareCalibrationStrings(certificateIssued, sizeof(certificateIssued),
                               deviceDate, sizeof(deviceDate), deviceFrom, sizeof(deviceFrom), deviceUntil, sizeof(deviceUntil),
                               headDate, sizeof(headDate), headFrom, sizeof(headFrom), headUntil, sizeof(headUntil),
                               firmwareDate, sizeof(firmwareDate));
  ethFormatValidityYmd(systemStatus.calibrationDateYmd, systemDate, sizeof(systemDate));
  ethFormatValidityDateTime(systemStatus.validFromUtc, systemFrom, sizeof(systemFrom));
  ethFormatValidityDateTime(systemStatus.validUntilUtc, systemUntil, sizeof(systemUntil));
  tpFirmwareIntegrityExpectedHashHex(firmwareExpectedHash, sizeof(firmwareExpectedHash));
  tpFirmwareIntegrityMeasuredHashHex(firmwareMeasuredHash, sizeof(firmwareMeasuredHash));
  ethFormatValidityDateTime(firmwareIntegrity.verifiedUtc, firmwareVerified, sizeof(firmwareVerified));
  ethFormatValidityDateTime(firmwareIntegrity.approvedUtc, firmwareApproved, sizeof(firmwareApproved));

  const char* certificateClass = certificateValid ? ethCalibrationText.t004 : (certificateStoredInvalid ? ethCalibrationText.t002 : ethCalibrationText.t003);
  const char* certificateSerial = certificateValid ? deviceIdentityCertifiedSerial() : deviceSerialGet();
  const char* deviceId = deviceStatus.calibrationId[0] != '\0' ? deviceStatus.calibrationId : ethCalibrationText.t011;
  const char* headId = headStatus.calibrationId[0] != '\0' ? headStatus.calibrationId : ethCalibrationText.t011;
  const char* deviceSigner = deviceStatus.signerKeyId[0] != '\0' ? deviceStatus.signerKeyId : ethCalibrationText.t011;
  const char* headSigner = headStatus.signerKeyId[0] != '\0' ? headStatus.signerKeyId : ethCalibrationText.t011;
  const char* systemId = systemStatus.calibrationId[0] != '\0' ? systemStatus.calibrationId : ethCalibrationText.t011;
  const char* systemSigner = systemStatus.signerKeyId[0] != '\0' ? systemStatus.signerKeyId : ethCalibrationText.t011;
  const char* systemIssuer = systemStatus.certificateIssuer[0] != '\0' ? systemStatus.certificateIssuer : ethCalibrationText.t011;
  const char* systemOperator = systemStatus.operatorName[0] != '\0' ? systemStatus.operatorName : ethCalibrationText.t011;
  const char* systemDeviceAdjustment = systemStatus.deviceAdjustmentCalibrationId[0] != '\0' ? systemStatus.deviceAdjustmentCalibrationId : ethCalibrationText.t011;
  const char* systemHeadAdjustment = systemStatus.headAdjustmentCalibrationId[0] != '\0' ? systemStatus.headAdjustmentCalibrationId : ethCalibrationText.t011;
  const char* systemHeadType = systemStatus.headType[0] != '\0' ? systemStatus.headType : headTypeTextGet();
  const uint32_t systemHeadSerial = systemStatus.headSerial != 0U ? systemStatus.headSerial : R.head_serial;
  const char* firmwareClass = firmwareReady
      ? ethCalibrationText.t004
      : (firmwareIntegrity.state == TP_FW_INTEGRITY_ERROR
           ? ethCalibrationText.t002 : ethCalibrationText.t003);
  const char* firmwareStatus = tpFirmwareIntegrityStatusText();
  ethBuildSystemExternalRows(systemStatus, ethExternalSystemRows, sizeof(ethExternalSystemRows));

  const int length = snprintf(ethJsonBuf, sizeof(ethJsonBuf), ETH_VALIDITY_PAGE_TEMPLATE_V25,
    certificateClass, deviceIdentityStatusText(), certificateSerial, certificateIssued,
    ethSignedCalibrationClass(deviceReady, deviceStatus, deviceTime), ethSignedCalibrationStatusText(deviceReady, deviceStatus, deviceTime, true),
    deviceDate, deviceFrom, deviceUntil, deviceId, deviceSigner,
    ethSignedCalibrationClass(headReady, headStatus, headTime), ethSignedCalibrationStatusText(headReady, headStatus, headTime, false),
    headTypeTextGet(), (unsigned long)R.head_serial, headDate, headFrom, headUntil, headId, headSigner,
    ethSystemCalibrationClass(systemReady, systemStatus, systemTime, systemFirmwareApplicability), ethSystemCalibrationStatusText(systemReady, systemStatus, systemTime, systemFirmwareApplicability),
    certificateSerial, systemHeadType, (unsigned long)systemHeadSerial,
    ethSystemCalibrationScopeText(systemStatus.calibrationScope), systemDate, systemFrom, systemUntil,
    (unsigned)systemStatus.intervalMonths, systemIssuer, systemOperator,
    ethSystemCertificateSourceText(systemStatus), ethExternalSystemRows, systemId, systemSigner,
    systemDeviceAdjustment, systemHeadAdjustment,
    ethSystemFirmwareApplicabilityClass(systemFirmwareApplicability),
    ethSystemFirmwareApplicabilityStatusText(systemFirmwareApplicability),
    systemFirmwareHashAtCalibration,
    firmwareClass, firmwareStatus, firmware.version, firmware.buildId, firmwareDate,
    (unsigned long)firmwareIntegrity.measuredImageSize,
    firmwareExpectedHash, firmwareMeasuredHash,
    firmwareIntegrity.firmwareCertificateId[0] != '\0' ? firmwareIntegrity.firmwareCertificateId : "–",
    firmwareIntegrity.rootKeyId[0] != '\0' ? firmwareIntegrity.rootKeyId : "–",
    firmwareApproved, firmwareVerified);

  if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, ethCalibrationText.t018);
    return;
  }
  ethBeginGeneratedPageTransfer(rawClient, client, requestHeaders,
                                ETH_REQ_VALIDITY_PAGE,
                                ethJsonBuf, (size_t)length, keepOpen);
}

static void FLASHMEM ethSendCalibrationPage(EthBoundedWriter& client,
                                                EthernetClient& rawClient,
                                                const char* requestHeaders,
                                                bool* keepOpen)
{
  const bool certificateValid = deviceIdentityCertificateValid();
  const bool deviceReady = tpSignedCalibrationDeviceReady();
  const bool headReady = tpSignedCalibrationHeadReady();
  const bool systemReady = tpSignedCalibrationSystemReady();
  const TpSignedCalibrationStatus& deviceStatus = tpSignedCalibrationDeviceStatus();
  const TpSignedCalibrationStatus& headStatus = tpSignedCalibrationHeadStatus();
  const TpSystemCalibrationStatus& systemStatus = tpSignedCalibrationSystemStatus();
  const TpFirmwareRuntimeIdentity& firmware = tpSignedDataFirmwareIdentity();
  const TpFirmwareIntegrityStatus& firmwareIntegrity = tpFirmwareIntegrityStatus();
  const bool firmwareReady = tpSignedDataFirmwareManifestReady();
  const int64_t currentUtc = tpCurrentUtcUnixTime();
  const TpCalibrationTimeStatus deviceTime = tpSignedCalibrationTimeStatus(deviceStatus, currentUtc);
  const TpCalibrationTimeStatus headTime = tpSignedCalibrationTimeStatus(headStatus, currentUtc);
  const TpCalibrationTimeStatus systemTime = ethSystemCalibrationTimeStatus(systemStatus, currentUtc);
  const TpSystemFirmwareApplicability systemFirmwareApplicability =
      ethLoadSystemFirmwareApplicability(systemStatus);
  const char* systemFirmwareHashAtCalibration =
      ethSystemFirmwareContextStatus.recordValid
        ? ethSystemFirmwareContextStatus.firmwareSha256 : "–";

  char certificateIssued[40], deviceDate[16], deviceFrom[40], deviceUntil[40];
  char headDate[16], headFrom[40], headUntil[40], firmwareDate[16];
  char systemDate[16], systemFrom[40], systemUntil[40];
  char firmwareMeasuredHash[65], firmwareApproved[40];
  ethPrepareCalibrationStrings(certificateIssued, sizeof(certificateIssued),
                               deviceDate, sizeof(deviceDate), deviceFrom, sizeof(deviceFrom), deviceUntil, sizeof(deviceUntil),
                               headDate, sizeof(headDate), headFrom, sizeof(headFrom), headUntil, sizeof(headUntil),
                               firmwareDate, sizeof(firmwareDate));
  ethFormatValidityYmd(systemStatus.calibrationDateYmd, systemDate, sizeof(systemDate));
  ethFormatValidityDateTime(systemStatus.validFromUtc, systemFrom, sizeof(systemFrom));
  ethFormatValidityDateTime(systemStatus.validUntilUtc, systemUntil, sizeof(systemUntil));
  tpFirmwareIntegrityMeasuredHashHex(firmwareMeasuredHash, sizeof(firmwareMeasuredHash));
  ethFormatValidityDateTime(firmwareIntegrity.approvedUtc, firmwareApproved, sizeof(firmwareApproved));
  const char* firmwareClass = firmwareReady
      ? ethCalibrationText.t004
      : (firmwareIntegrity.state == TP_FW_INTEGRITY_ERROR
           ? ethCalibrationText.t002 : ethCalibrationText.t003);

  const char* deviceRequestControl = certificateValid
      ? ETH_CAL_DEVICE_REQUEST_CONTROL : ETH_CAL_REQUEST_UNAVAILABLE;
  const char* headRequestControl = certificateValid
      ? ETH_CAL_HEAD_REQUEST_CONTROL : ETH_CAL_REQUEST_UNAVAILABLE;
  const char* systemRequestControl = !certificateValid
      ? ETH_CAL_REQUEST_UNAVAILABLE
      : ((deviceReady && headReady)
          ? ETH_CAL_SYSTEM_REQUEST_CONTROL : ETH_CAL_SYSTEM_REQUEST_BLOCKED);
  const char* evidenceText = nullptr;
  if (!certificateValid) evidenceText = "Gültiges Gerätezertifikat fehlt";
  else if (!deviceReady) evidenceText = "Signierte Gerätejustierung fehlt oder stimmt nicht mit den aktiven Werten überein";
  else if (!headReady) evidenceText = "Signierte Kopfjustierung fehlt oder stimmt nicht mit dem aktuellen Kopf überein";
  else if (!systemReady) evidenceText = "Signierte Systemkalibrierung für die aktuelle Geräte-/Kopfkombination fehlt";
  else if (systemFirmwareApplicability == TP_SYSTEM_FW_APPLICABILITY_CHANGED)
    evidenceText = "<span class='warn'>Nachweise vollständig; aktuelle Firmware weicht vom bei der Kalibrierung dokumentierten Stand ab. Der Kalibrierschein bleibt gültig und die Abweichung wird in den Nachweisen gekennzeichnet.</span>";
  else if (systemFirmwareApplicability == TP_SYSTEM_FW_APPLICABILITY_NOT_COMPARABLE)
    evidenceText = "<span class='warn'>Nachweise vollständig; Firmwarebezug der Systemkalibrierung ist nicht beurteilbar.</span>";
  else evidenceText = "<span class='ok'>Gerätezertifikat, Geräte- und Kopfjustierung sowie Systemkalibrierung vollständig; Firmware entspricht dem Kalibrierstand.</span>";
  const char* systemId = systemStatus.calibrationId[0] != '\0' ? systemStatus.calibrationId : ethCalibrationText.t011;
  const char* systemHeadType = systemStatus.headType[0] != '\0' ? systemStatus.headType : headTypeTextGet();
  const uint32_t systemHeadSerial = systemStatus.headSerial != 0U ? systemStatus.headSerial : R.head_serial;
  ethBuildSystemExternalRows(systemStatus, ethExternalSystemRows, sizeof(ethExternalSystemRows));

  const TpExternalCalibrationMetadata& external = tpExternalCalibrationActive();
  const TpCalibrationTimeStatus externalTime = ethExternalCalibrationTimeStatus(external, currentUtc);
  char externalFrom[16], externalUntil[16];
  ethFormatValidityYmd(external.validFromYmd, externalFrom, sizeof(externalFrom));
  ethFormatValidityYmd(external.validUntilYmd, externalUntil, sizeof(externalUntil));
  ethHtmlEscape(external.present ? external.certificateNumber : "–", ethExternalNumberEscaped, sizeof(ethExternalNumberEscaped));
  ethHtmlEscape(external.present ? external.originalFileName : "–", ethExternalFileEscaped, sizeof(ethExternalFileEscaped));
  ethHtmlEscape(external.present ? external.documentId : "–", ethExternalDocumentIdEscaped, sizeof(ethExternalDocumentIdEscaped));
  ethHtmlEscape(external.present ? external.sha256 : "–", ethExternalHashEscaped, sizeof(ethExternalHashEscaped));
  const char* externalDownload = external.present
    ? "<a class='btn' href='/calibration-external-pdf' download>Aktiven externen Kalibrierschein herunterladen</a>"
    : "";

  const int length = snprintf(ethJsonBuf, sizeof(ethJsonBuf), ETH_CALIBRATION_MANAGE_TEMPLATE_V25,
    ethSignedCalibrationClass(deviceReady, deviceStatus, deviceTime), ethSignedCalibrationStatusText(deviceReady, deviceStatus, deviceTime, true),
    deviceDate, deviceFrom, deviceUntil, deviceRequestControl,
    ethSignedCalibrationClass(headReady, headStatus, headTime), ethSignedCalibrationStatusText(headReady, headStatus, headTime, false),
    headTypeTextGet(), (unsigned long)R.head_serial, headDate, headFrom, headUntil, headRequestControl,
    ethSystemCalibrationClass(systemReady, systemStatus, systemTime, systemFirmwareApplicability), ethSystemCalibrationStatusText(systemReady, systemStatus, systemTime, systemFirmwareApplicability),
    deviceIdentityCertificateValid() ? deviceIdentityCertifiedSerial() : deviceSerialGet(), systemHeadType, (unsigned long)systemHeadSerial,
    ethSystemCalibrationScopeText(systemStatus.calibrationScope), systemDate, systemFrom, systemUntil,
    ethSystemCertificateSourceText(systemStatus), ethExternalSystemRows,
    ethSystemFirmwareApplicabilityClass(systemFirmwareApplicability),
    ethSystemFirmwareApplicabilityStatusText(systemFirmwareApplicability),
    systemFirmwareHashAtCalibration,
    systemId, systemRequestControl,
    firmwareClass, tpFirmwareIntegrityStatusText(), firmware.version, firmware.buildId,
    (unsigned long)firmwareIntegrity.measuredImageSize, firmwareMeasuredHash,
    firmwareIntegrity.firmwareCertificateId[0] != '\0' ? firmwareIntegrity.firmwareCertificateId : "–",
    firmwareIntegrity.rootKeyId[0] != '\0' ? firmwareIntegrity.rootKeyId : "–",
    firmwareApproved,
    ethExternalCalibrationClass(external, externalTime), ethExternalCalibrationStatusText(external, externalTime),
    ethExternalNumberEscaped, externalFrom, externalUntil, ethExternalFileEscaped,
    (unsigned long)(external.present ? external.fileSize : 0U), ethExternalDocumentIdEscaped, ethExternalHashEscaped, externalDownload,
    evidenceText);

  if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, ethCalibrationText.t018);
    return;
  }
  ethBeginGeneratedPageTransfer(rawClient, client, requestHeaders,
                                ETH_REQ_CALIBRATION_PAGE,
                                ethJsonBuf, (size_t)length, keepOpen);
}

static void FLASHMEM ethSendCalibrationResultPage(EthBoundedWriter& client,
                                                   bool ok,
                                                   const char* title,
                                                   const char* message)
{
  const int length = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
    ethCalibrationText.t020,
    title != nullptr ? title : ethCalibrationText.t021,
    ok ? ethCalibrationText.t004 : ethCalibrationText.t002,
    title != nullptr ? title : (ok ? ethCalibrationText.t022 : ethCalibrationText.t023),
    message != nullptr ? message : ethCalibrationText.t015);
  if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
  {
    ethSendText(client, ok, message);
    return;
  }
  ethSendBufferedBody(client, ethCalibrationText.t019, true, ethJsonBuf, (size_t)length);
}

static void FLASHMEM ethHandleCalibrationRequest(EthBoundedWriter& client,
                                                  bool device)
{
  char downloadName[96];
  char error[200];
  char sdPath[128];
  size_t bodyLength = 0U;
  const bool built = device
    ? tpSignedCalibrationBuildDeviceRequestJson(
        ethJsonBuf, sizeof(ethJsonBuf), &bodyLength,
        downloadName, sizeof(downloadName), error, sizeof(error))
    : tpSignedCalibrationBuildHeadRequestJson(
        ethJsonBuf, sizeof(ethJsonBuf), &bodyLength,
        downloadName, sizeof(downloadName), error, sizeof(error));

  if (!built)
  {
    // Aufbau, Signaturpruefung und SD-Zugriffe koennen das allgemeine
    // 120-ms-Antwortbudget bereits verbraucht haben. Fuer die eigentliche
    // Browserantwort beginnt deshalb ein frisches reines Schreibbudget.
    client.restartBudget();
    ethSendCalibrationResultPage(client, false,
                                 ethCalibrationText.t024, error);
    return;
  }

  char sdError[180];
  const bool sdSaved = tpSignedCalibrationSaveRequestToSd(
    ethJsonBuf, bodyLength, downloadName,
    sdPath, sizeof(sdPath), sdError, sizeof(sdError));

  // Das Antwortbudget darf nur die Netzwerkuebertragung umfassen, nicht
  // JSON-Aufbau, ECDSA-Signatur oder die zusaetzliche SD-Archivkopie.
  client.restartBudget();
  client.print(ethCalibrationText.t025);
  client.print(ethCalibrationText.t026);
  client.print(ethCalibrationText.t027);
  client.print(ethCalibrationText.t028);
  client.print(downloadName);
  client.print(ethCalibrationText.t029);
  client.print(ethCalibrationText.t030);
  client.print(ethCalibrationText.t031);
  client.print(sdSaved ? sdPath : ethCalibrationText.t032);
  client.print(ethCalibrationText.t033);
  client.print((unsigned long)bodyLength);
  client.print(ethCalibrationText.t034);
  client.write((const uint8_t*)ethJsonBuf, bodyLength);
}

static void FLASHMEM ethHandleSystemCalibrationRequest(EthBoundedWriter& client)
{
  char downloadName[128];
  char error[220];
  char sdPath[128];
  size_t bodyLength = 0U;
  if (!tpSignedCalibrationBuildSystemRequestJson(
        ethJsonBuf, sizeof(ethJsonBuf), &bodyLength,
        downloadName, sizeof(downloadName), error, sizeof(error)))
  {
    // Der gerätesignierte Firmwarekontext kann beim Fehlerweg bereits
    // Kryptografie- und SD-Zeit verbraucht haben. Die Fehlermeldung bekommt
    // deshalb ein neues reines Netzwerk-Schreibbudget.
    client.restartBudget();
    ethSendCalibrationResultPage(client, false,
                                 "Systemkalibrierungsanfrage nicht erzeugt", error);
    return;
  }

  char sdError[180];
  const bool sdSaved = tpSignedCalibrationSaveRequestToSd(
    ethJsonBuf, bodyLength, downloadName,
    sdPath, sizeof(sdPath), sdError, sizeof(sdError));

  // Seit Build 55 wird hier zusaetzlich der gerätesignierte Firmwarekontext
  // geschrieben. Diese Vorarbeit darf nicht vom 120-ms-Budget der
  // anschliessenden HTTP-Dateiuebertragung abgezogen werden.
  client.restartBudget();
  client.print(ethCalibrationText.t025);
  client.print(ethCalibrationText.t026);
  client.print(ethCalibrationText.t027);
  client.print(ethCalibrationText.t028);
  client.print(downloadName);
  client.print(ethCalibrationText.t029);
  client.print(ethCalibrationText.t030);
  client.print(ethCalibrationText.t031);
  client.print(sdSaved ? sdPath : ethCalibrationText.t032);
  client.print(ethCalibrationText.t033);
  client.print((unsigned long)bodyLength);
  client.print(ethCalibrationText.t034);
  client.write((const uint8_t*)ethJsonBuf, bodyLength);
}

static void FLASHMEM ethHandleFirmwareRequest(EthBoundedWriter& client)
{
  char downloadName[96];
  char error[220];
  char sdPath[160];
  size_t bodyLength = 0U;
  if (!tpFirmwareIntegrityBuildRequestJson(
        ethJsonBuf, sizeof(ethJsonBuf), &bodyLength,
        downloadName, sizeof(downloadName), error, sizeof(error)))
  {
    client.restartBudget();
    ethSendCalibrationResultPage(client, false,
                                 "Firmwareanfrage nicht erzeugt", error);
    return;
  }

  char sdError[180];
  const bool sdSaved = tpFirmwareIntegritySaveRequestToSd(
    ethJsonBuf, bodyLength, downloadName,
    sdPath, sizeof(sdPath), sdError, sizeof(sdError));

  client.restartBudget();
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Type: application/json; charset=utf-8\r\n");
  client.print("Content-Disposition: attachment; filename=\"");
  client.print(downloadName);
  client.print("\"\r\nCache-Control: no-store\r\n");
  client.print("X-TP3000-SD-Copy: ");
  client.print(sdSaved ? sdPath : "not-saved");
  client.print("\r\nContent-Length: ");
  client.print((unsigned long)bodyLength);
  client.print("\r\n\r\n");
  client.write((const uint8_t*)ethJsonBuf, bodyLength);
}

static void FLASHMEM ethHandleFirmwareCertificateUpload(
    EthBoundedWriter& client,
    const char* requestBody,
    size_t requestBodyLength)
{
  if (requestBody == nullptr || requestBodyLength == 0U)
  {
    ethSendText(client, false, "Keine .tpfwcert-Datei übertragen");
    return;
  }
  char error[240];
  const bool ok = tpFirmwareIntegrityImportCertificateJson(
      requestBody, requestBodyLength, error, sizeof(error));
  client.restartBudget();
  ethSendText(client, ok, error[0] != '\0' ? error :
              (ok ? "Firmwarezertifikat aktiviert" : "Firmwarezertifikat abgewiesen"));
}

static void FLASHMEM ethHandleFirmwareCertificateImport(EthBoundedWriter& client)
{
  char importedPath[160];
  char error[240];
  const bool ok = tpFirmwareIntegrityImportCertificateFromSd(
      importedPath, sizeof(importedPath), error, sizeof(error));
  client.restartBudget();
  char message[360];
  if (ok)
  {
    snprintf(message, sizeof(message),
             "Firmwarezertifikat %s wurde vom Hersteller-Root geprüft und für das laufende Firmwareabbild aktiviert.",
             importedPath);
    ethSendCalibrationResultPage(client, true,
                                 "Firmwarezertifikat aktiviert", message);
  }
  else
  {
    ethSendCalibrationResultPage(client, false,
                                 "Firmwarezertifikat nicht importiert", error);
  }
}

static void FLASHMEM ethSendActiveSystemCalibrationJson(EthBoundedWriter& client,
                                                        const char* requestLine)
{
  const bool historical = ethGetQueryToken(requestLine, "id",
                                           ethCalibrationManifestQuery,
                                           sizeof(ethCalibrationManifestQuery));
  if (!historical && !tpSignedCalibrationSystemReady())
  {
    const TpSystemCalibrationStatus& status = tpSignedCalibrationSystemStatus();
    ethSendText(client, false,
                status.lastError[0] != '\0' ? status.lastError
                                            : "Keine aktive Systemkalibrierung");
    return;
  }

  size_t length = 0U;
  char error[180];
  const bool readOk = historical
      ? tpSignedCalibrationReadArchivedSystemJson(
          ethCalibrationManifestQuery,
          ethJsonBuf, sizeof(ethJsonBuf), &length,
          error, sizeof(error))
      : tpSignedCalibrationReadActiveSystemJson(
          ethJsonBuf, sizeof(ethJsonBuf), &length,
          error, sizeof(error));
  if (historical) client.restartBudget();
  if (!readOk)
  {
    ethSendText(client, false, error);
    return;
  }
  ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                      ethJsonBuf, length);
}

static void FLASHMEM ethSendCalibrationApplicabilityJson(
    EthBoundedWriter& client,
    const char* requestLine)
{
  if (!ethGetQueryToken(requestLine, "id",
                        ethCalibrationManifestQuery,
                        sizeof(ethCalibrationManifestQuery)))
  {
    ethSendText(client, false, "System-Manifest fehlt");
    return;
  }

  bool stateKnown = false;
  bool ended = false;
  int64_t applicableUntilUtc = 0;
  char reasonCode[40];
  if (!tpSignedCalibrationGetApplicability(
          ethCalibrationManifestQuery,
          &stateKnown, &ended, &applicableUntilUtc,
          reasonCode, sizeof(reasonCode)))
  {
    client.restartBudget();
    ethSendText(client, false, "Anwendbarkeitsstatus ist nicht verfügbar");
    return;
  }
  client.restartBudget();

  const int length = snprintf(
      ethJsonBuf, sizeof(ethJsonBuf),
      "{\"manifestSha256\":\"%s\",\"stateKnown\":%s,"
      "\"ended\":%s,\"applicableUntilUtc\":%lld,"
      "\"reasonCode\":\"%s\"}",
      ethCalibrationManifestQuery,
      stateKnown ? "true" : "false",
      ended ? "true" : "false",
      (long long)applicableUntilUtc,
      reasonCode);
  if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Anwendbarkeitsstatus ist zu groß");
    return;
  }
  ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                      ethJsonBuf, (size_t)length);
}

static const char* FLASHMEM ethFirmwareContextManufacturerText(
    TpSystemFirmwareManufacturerStatus status)
{
  switch (status)
  {
    case TP_SYSTEM_FW_MANUFACTURER_VALID: return "VALID";
    case TP_SYSTEM_FW_MANUFACTURER_MISMATCH: return "MISMATCH";
    case TP_SYSTEM_FW_MANUFACTURER_INVALID: return "INVALID";
    default: return "NOT_CERTIFIED";
  }
}

static const char* FLASHMEM ethFirmwareContextCaptureModeText(
    TpSystemFirmwareCaptureMode mode)
{
  return mode == TP_SYSTEM_FW_CAPTURE_REQUEST
      ? "REQUEST"
      : (mode == TP_SYSTEM_FW_CAPTURE_ACTIVATION_FALLBACK
           ? "ACTIVATION_FALLBACK"
           : "UNKNOWN");
}

static const char* FLASHMEM ethFirmwareContextApplicabilityText(
    TpSystemFirmwareApplicability applicability)
{
  switch (applicability)
  {
    case TP_SYSTEM_FW_APPLICABILITY_EXACT_MATCH: return "EXACT_MATCH";
    case TP_SYSTEM_FW_APPLICABILITY_CHANGED: return "FIRMWARE_CHANGED";
    default: return "NOT_COMPARABLE";
  }
}

static void FLASHMEM ethSendSystemFirmwareContextJson(
    EthBoundedWriter& client,
    const char* requestLine)
{
  const bool haveRequest = ethGetQueryToken(
      requestLine, "request", ethFirmwareContextRequestId,
      sizeof(ethFirmwareContextRequestId));
  const bool haveHash = ethGetQueryToken(
      requestLine, "hash", ethFirmwareContextRequestHash,
      sizeof(ethFirmwareContextRequestHash));
  if (!haveRequest || !haveHash)
  {
    ethSendText(client, false, "Firmwarekontext-Anfragebindung fehlt");
    return;
  }

  const bool loaded = tpSystemFirmwareContextLoad(
      ethFirmwareContextRequestId,
      ethFirmwareContextRequestHash,
      ethSystemFirmwareContextStatus);
  client.restartBudget();
  if (!loaded)
  {
    const char* code = ethSystemFirmwareContextStatus.filePresent
        ? "INVALID"
        : "MISSING";
    const int length = snprintf(
        ethJsonBuf, sizeof(ethJsonBuf),
        "{\"present\":%s,\"recordValid\":false,"
        "\"requestBindingValid\":false,\"state\":\"%s\"}",
        ethSystemFirmwareContextStatus.filePresent ? "true" : "false",
        code);
    if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
    {
      ethSendText(client, false, "Firmwarekontext-Antwort ist zu gross");
      return;
    }
    ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                        ethJsonBuf, (size_t)length);
    return;
  }

  char currentFirmwareSha256[65];
  currentFirmwareSha256[0] = '\0';
  if (ethSystemFirmwareContextStatus.currentHashAvailable)
    tpFirmwareIntegrityMeasuredHashHex(currentFirmwareSha256,
                                       sizeof(currentFirmwareSha256));

  const int length = snprintf(
      ethJsonBuf, sizeof(ethJsonBuf),
      "{\"present\":true,\"recordValid\":true,"
      "\"requestBindingValid\":true,\"state\":\"OK\","
      "\"captureMode\":\"%s\",\"capturedUtc\":%lld,"
      "\"sourceRequestId\":\"%s\","
      "\"sourceRequestManifestSha256\":\"%s\","
      "\"firmwareVersion\":\"%s\",\"firmwareBuildId\":\"%s\","
      "\"imageBase\":%lu,\"imageSize\":%lu,"
      "\"firmwareSha256\":\"%s\","
      "\"manufacturerStatus\":\"%s\","
      "\"firmwareCertificateId\":\"%s\","
      "\"manufacturerRootKeyId\":\"%s\","
      "\"certificateApprovedUtc\":%lld,"
      "\"contextManifestSha256\":\"%s\","
      "\"deviceSignatureHex\":\"%s\","
      "\"currentHashAvailable\":%s,\"currentImageMatches\":%s,"
      "\"currentFirmwareSha256\":\"%s\","
      "\"applicability\":\"%s\"}",
      ethFirmwareContextCaptureModeText(ethSystemFirmwareContextStatus.captureMode),
      (long long)ethSystemFirmwareContextStatus.capturedUtc,
      ethSystemFirmwareContextStatus.sourceRequestId,
      ethSystemFirmwareContextStatus.sourceRequestManifestSha256,
      ethSystemFirmwareContextStatus.firmwareVersion,
      ethSystemFirmwareContextStatus.firmwareBuildId,
      (unsigned long)ethSystemFirmwareContextStatus.imageBase,
      (unsigned long)ethSystemFirmwareContextStatus.imageSize,
      ethSystemFirmwareContextStatus.firmwareSha256,
      ethFirmwareContextManufacturerText(
          ethSystemFirmwareContextStatus.manufacturerStatus),
      ethSystemFirmwareContextStatus.firmwareCertificateId,
      ethSystemFirmwareContextStatus.manufacturerRootKeyId,
      (long long)ethSystemFirmwareContextStatus.certificateApprovedUtc,
      ethSystemFirmwareContextStatus.contextManifestSha256,
      ethSystemFirmwareContextStatus.deviceSignatureHex,
      ethSystemFirmwareContextStatus.currentHashAvailable ? "true" : "false",
      ethSystemFirmwareContextStatus.currentImageMatches ? "true" : "false",
      currentFirmwareSha256,
      ethFirmwareContextApplicabilityText(
          ethSystemFirmwareContextStatus.applicability));
  if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, "Firmwarekontext-Antwort ist zu gross");
    return;
  }
  ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                      ethJsonBuf, (size_t)length);
}

static void FLASHMEM ethHandleAdjustmentCalibrationUpload(
    EthBoundedWriter& client,
    bool device,
    const char* requestBody,
    size_t requestBodyLength)
{
  if (requestBody == nullptr || requestBodyLength == 0U)
  {
    ethSendText(client, false,
                device ? "Keine .tpdcal-Datei übertragen"
                       : "Keine .tphcal-Datei übertragen");
    return;
  }

  char error[240];
  const bool importOk = tpSignedCalibrationImportAdjustmentJson(
        device, requestBody, requestBodyLength, error, sizeof(error));
  // Signaturprüfung, SD-Archivierung und die kopfgebundene Systemauswahl
  // dürfen deutlich länger als das kurze HTTP-Schreibbudget dauern. Das
  // Antwortbudget beginnt deshalb erst nach Abschluss des gesamten Imports.
  client.restartBudget();
  if (!importOk)
  {
    ethSendText(client, false,
                error[0] != '\0' ? error
                                 : (device ? "Gerätejustierung wurde abgewiesen"
                                           : "Kopfjustierung wurde abgewiesen"));
    return;
  }

  const TpSignedCalibrationStatus& status = device
    ? tpSignedCalibrationDeviceStatus()
    : tpSignedCalibrationHeadStatus();
  char message[384];
  const int messageLength = snprintf(
      message, sizeof(message),
      "%s %.96s wurde vollständig übertragen, kryptografisch geprüft und aktiviert. Private Schlüssel wurden nicht übertragen.%s",
      device ? "Gerätejustierung" : "Kopfjustierung",
      status.calibrationId[0] != '\0' ? status.calibrationId : "",
      tpSignedCalibrationSystemReady()
        ? " Die bestehende Systemkalibrierung passt weiterhin zu beiden Justierungen."
        : " Eine vorhandene Systemkalibrierung wird nur aktiv, wenn ihre Justierungsbindung weiterhin übereinstimmt.");
  if (messageLength <= 0 || (size_t)messageLength >= sizeof(message))
  {
    ethSendText(client, true, device ? "Gerätejustierung aktiviert"
                                    : "Kopfjustierung aktiviert");
    return;
  }
  ethSendText(client, true, message);
}

static void FLASHMEM ethHandleSystemCalibrationUpload(
    EthBoundedWriter& client,
    const char* requestBody,
    size_t requestBodyLength)
{
  if (requestBody == nullptr || requestBodyLength == 0U)
  {
    ethSendText(client, false, "Keine .tpscal-Datei übertragen");
    return;
  }

  char error[240];
  const bool importOk = tpSignedCalibrationImportSystemJson(
        requestBody, requestBodyLength, error, sizeof(error));
  // Auch der Systemimport archiviert, prüft und aktiviert transaktional.
  // Erst danach darf das 120-ms-Budget für die HTTP-Antwort laufen.
  client.restartBudget();
  if (!importOk)
  {
    ethSendText(client, false, error[0] != '\0' ? error
                                                : "Systemkalibrierung wurde abgewiesen");
    return;
  }

  const TpSystemCalibrationStatus& status = tpSignedCalibrationSystemStatus();
  char message[320];
  const int messageLength = snprintf(
      message, sizeof(message),
      "Systemkalibrierung %.96s wurde kryptografisch geprüft, an G%.5s / %.8s K%05lu gebunden und aktiviert.",
      status.calibrationId[0] != '\0' ? status.calibrationId : "",
      deviceIdentityCertificateValid() ? deviceIdentityCertifiedSerial() : deviceSerialGet(),
      status.headType[0] != '\0' ? status.headType : headTypeTextGet(),
      (unsigned long)(status.headSerial != 0U ? status.headSerial : R.head_serial));
  if (messageLength <= 0 || (size_t)messageLength >= sizeof(message))
  {
    ethSendText(client, true, "Systemkalibrierung aktiviert");
    return;
  }
  ethSendText(client, true, message);
}

static void FLASHMEM ethHandleCalibrationImport(EthBoundedWriter& client,
                                                 bool device)
{
  char importedPath[128];
  char error[220];
  const bool ok = device
    ? tpSignedCalibrationImportDeviceFromSd(
        importedPath, sizeof(importedPath), error, sizeof(error))
    : tpSignedCalibrationImportHeadFromSd(
        importedPath, sizeof(importedPath), error, sizeof(error));
  // SD-Suche und kryptografische Aktivierung können länger dauern als das
  // normale Schreibbudget der anschließenden Ergebnisseite.
  client.restartBudget();

  if (ok)
  {
    char message[300];
    snprintf(message, sizeof(message),
             ethCalibrationText.t035,
             device ? ethCalibrationText.t036 : ethCalibrationText.t037,
             importedPath);
    ethSendCalibrationResultPage(
      client, true,
      device ? ethCalibrationText.t038
             : ethCalibrationText.t039,
      message);
  }
  else
  {
    ethSendCalibrationResultPage(
      client, false,
      device ? ethCalibrationText.t040
             : ethCalibrationText.t041,
      error);
  }
}


static void FLASHMEM ethHandleSystemCalibrationImport(EthBoundedWriter& client)
{
  char importedPath[128];
  char error[220];
  const bool ok = tpSignedCalibrationImportSystemFromSd(
      importedPath, sizeof(importedPath), error, sizeof(error));
  client.restartBudget();

  if (ok)
  {
    char message[340];
    snprintf(message, sizeof(message),
             "Systemkalibrierung <code>%s</code> wurde kryptografisch geprüft und aktiviert.",
             importedPath);
    ethSendCalibrationResultPage(client, true,
                                 "Signierte Systemkalibrierung aktiviert",
                                 message);
  }
  else
  {
    ethSendCalibrationResultPage(client, false,
                                 "Systemkalibrierung nicht importiert",
                                 error);
  }
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

  // Der Integritätslogger verwendet nummerierte Abschnitte. Deshalb ist die
  // einzige verlässliche Quelle der aktuell vom Logger gemeldete Pfad.
  const char* active = ethBaseName(sdLogGetCurrentFile());
  size_t copyLen = strlen(active);
  if (copyLen >= outSize) copyLen = outSize - 1U;
  if (copyLen > 0U) memcpy(out, active, copyLen);
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
  if (ethNameHasExtension(name, ".TPSIG")) return true;
  if (ethNameHasExtension(name, ".TPLOG")) return true;
  if (ethNameHasExtension(name, ".SHA256")) return true;
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
  if (name == nullptr || !(ethNameHasExtension(name, ".CSV") ||
                           ethNameHasExtension(name, ".TPSIG") ||
                           ethNameHasExtension(name, ".TPLOG") ||
                           ethNameHasExtension(name, ".SHA256"))) return false;

  const size_t len = strlen(name);
  if (len < 10U || len > 31U) return false;

  for (uint8_t i = 0U; i < 6U; i++)
  {
    if (name[i] < '0' || name[i] > '9') return false;
  }

  // Legacy: YYMMDD.CSV / YYMMDDD.CSV
  // Segmentiert: YYMMDD_001.CSV / YYMMDDD_001.CSV
  const char c6 = name[6];
  if (!(c6 == '.' || c6 == '_' || c6 == 'D' || c6 == 'd')) return false;
  if ((c6 == 'D' || c6 == 'd') && !(name[7] == '.' || name[7] == '_')) return false;

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
  // Fuer die Web-Dateiliste wird deshalb bei Tagesdateien und ihren Nachweisen
  // nur die aeussere Anzeige/Sortierung korrigiert: Datum aus YYMMDD[D]...,
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
    item.kind = ethSafeSerialTraceName(base)
                  ? ETH_FILE_KIND_SERIAL
                  : (ethNameHasExtension(base, ".CSV") ? ETH_FILE_KIND_CSV
                                                        : ETH_FILE_KIND_EVIDENCE);
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
  if (filter == 3U) return item.kind == ETH_FILE_KIND_EVIDENCE;
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
    else if (strcmp(filterText, "evidence") == 0) filter = 3U;
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
                     item.kind == ETH_FILE_KIND_SERIAL ? "serial" :
                       (item.kind == ETH_FILE_KIND_EVIDENCE ? "evidence" : "csv"),
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

  bool integritySnapshotSealed = false;
  if (activeCsv && snapshotRequested && interfaceSdLogIntegrityMode() != TP_LOG_INTEGRITY_OFF)
  {
    char sealedName[32] = {0};
    char sealError[128] = {0};
    safetyBeginBlockingOperation();
    const bool sealed = sdLogSealActiveSegment(name, sealedName, sizeof(sealedName),
                                               sealError, sizeof(sealError));
    safetyEndBlockingOperation();
    if (!sealed)
    {
      ethSendText(client, false, sealError[0] ? sealError :
                  "Signierter Snapshot konnte nicht abgeschlossen werden / Signed snapshot could not be sealed");
      return false;
    }
    integritySnapshotSealed = true;
    if (sealedName[0] != '\0')
    {
      strncpy(name, sealedName, sizeof(name) - 1U);
      name[sizeof(name) - 1U] = '\0';
    }
    ethFileIndexRequestRefresh();
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
  if (activeFile && snapshotRequested && !integritySnapshotSealed &&
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

  const char* contentType = ethNameHasExtension(name, ".TPSIG")
                          ? "application/json; charset=utf-8"
                          : (ethNameHasExtension(name, ".TPLOG")
                              ? "application/vnd.tp3000.tplog"
                              : (ethNameHasExtension(name, ".CSV")
                                  ? "text/csv; charset=utf-8"
                                  : "text/plain; charset=utf-8"));

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

static void FLASHMEM ethSendExternalCalibrationJson(EthBoundedWriter& client,
                                                     const char* requestLine)
{
  const TpExternalCalibrationMetadata* metadataPtr = nullptr;
  const bool historical = ethGetQueryToken(requestLine, "id",
                                           ethCalibrationManifestQuery,
                                           sizeof(ethCalibrationManifestQuery));
  if (historical)
  {
    const bool loaded = tpExternalCalibrationLoadArchived(
        ethCalibrationManifestQuery, ethExternalArchiveMetadata,
        ethCalibrationArchiveError, sizeof(ethCalibrationArchiveError));
    client.restartBudget();
    if (!loaded)
    {
      ethSendText(client, false,
                  ethCalibrationArchiveError[0] != '\0'
                    ? ethCalibrationArchiveError
                    : "Externes PDF-Kalibrierzertifikat ist nicht verfügbar");
      return;
    }
    metadataPtr = &ethExternalArchiveMetadata;
  }
  else
  {
    metadataPtr = &tpExternalCalibrationActive();
  }
  const TpExternalCalibrationMetadata& metadata = *metadataPtr;
  if (!metadata.present)
  {
    ethSendText(client, false, ETH_EXTERNAL_ACTIVE_NONE);
    return;
  }
  const int length = snprintf(ethJsonBuf, sizeof(ethJsonBuf), ETH_EXTERNAL_JSON_TEMPLATE,
    metadata.documentId, metadata.certificateNumber,
    (unsigned long)metadata.validFromYmd,
    (unsigned long)metadata.validUntilYmd,
    metadata.originalFileName,
    (unsigned long)metadata.fileSize,
    metadata.sha256);
  if (length <= 0 || (size_t)length >= sizeof(ethJsonBuf))
  {
    ethSendText(client, false, ETH_EXTERNAL_METADATA_TOO_LARGE);
    return;
  }
  ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                      ethJsonBuf, (size_t)length);
}

static void FLASHMEM ethHandleExternalCalibrationBegin(
    EthBoundedWriter& client,
    const char* requestLine)
{
  uint32_t validFromYmd = 0U;
  uint32_t validUntilYmd = 0U;
  uint32_t expectedSize = 0U;
  if (!ethGetQueryDecodedText(requestLine, ETH_EXTERNAL_QUERY_NUMBER, ethExternalUploadCertificateNumber, sizeof(ethExternalUploadCertificateNumber)) ||
      !ethGetQueryUint32(requestLine, ETH_EXTERNAL_QUERY_FROM, &validFromYmd) ||
      !ethGetQueryUint32(requestLine, ETH_EXTERNAL_QUERY_UNTIL, &validUntilYmd) ||
      !ethGetQueryDecodedText(requestLine, ETH_EXTERNAL_QUERY_NAME, ethExternalUploadOriginalFileName, sizeof(ethExternalUploadOriginalFileName)) ||
      !ethGetQueryUint32(requestLine, ETH_EXTERNAL_QUERY_SIZE, &expectedSize))
  {
    ethSendText(client, false, ETH_EXTERNAL_METADATA_INCOMPLETE);
    return;
  }
  if (!tpExternalCalibrationBeginUpload(ethExternalUploadCertificateNumber,
                                        validFromYmd,
                                        validUntilYmd,
                                        ethExternalUploadOriginalFileName,
                                        expectedSize,
                                        ethExternalUploadError,
                                        sizeof(ethExternalUploadError)))
  {
    ethSendText(client, false, ethExternalUploadError[0] != '\0' ? ethExternalUploadError : ETH_EXTERNAL_BEGIN_FAILED);
    return;
  }
  ethSendText(client, true, ETH_EXTERNAL_BEGIN_OK);
}

static void FLASHMEM ethHandleExternalCalibrationChunk(
    EthBoundedWriter& client,
    const char* requestLine,
    const char* requestBody,
    size_t requestBodyLength)
{
  uint32_t offset = 0U;
  if (!ethGetQueryUint32(requestLine, ETH_EXTERNAL_QUERY_OFFSET, &offset) ||
      requestBody == nullptr || requestBodyLength == 0U)
  {
    tpExternalCalibrationAbortUpload();
    ethSendText(client, false, ETH_EXTERNAL_CHUNK_MISSING);
    return;
  }
  if (!tpExternalCalibrationAppendChunk(offset,
                                        (const uint8_t*)requestBody,
                                        requestBodyLength,
                                        ethExternalUploadError,
                                        sizeof(ethExternalUploadError)))
  {
    ethSendText(client, false, ethExternalUploadError[0] != '\0' ? ethExternalUploadError : ETH_EXTERNAL_CHUNK_REJECTED);
    return;
  }
  ethSendText(client, true, ETH_EXTERNAL_CHUNK_OK);
}

static void FLASHMEM ethHandleExternalCalibrationFinish(EthBoundedWriter& client)
{
  const bool finishOk = tpExternalCalibrationFinishUpload(
      ethExternalUploadResult, sizeof(ethExternalUploadResult),
      ethExternalUploadError, sizeof(ethExternalUploadError));
  // Schließen, erneutes Einlesen und SHA-256-Prüfung gehören noch zum Upload.
  // Die Antwortzeit wird erst danach gemessen.
  client.restartBudget();
  if (!finishOk)
  {
    ethSendText(client, false, ethExternalUploadError[0] != '\0' ? ethExternalUploadError : ETH_EXTERNAL_FINISH_FAILED);
    return;
  }
  ethSendText(client, true, ethExternalUploadResult);
}

static void FLASHMEM ethHandleExternalCalibrationAbort(EthBoundedWriter& client)
{
  tpExternalCalibrationAbortUpload();
  ethSendText(client, true, ETH_EXTERNAL_ABORT_OK);
}

static bool FLASHMEM ethStartExternalCalibrationDownload(
    EthBoundedWriter& client,
    EthernetClient& rawClient,
    const char* requestLine)
{
  const TpExternalCalibrationMetadata* metadataPtr = nullptr;
  const bool historical = ethGetQueryToken(requestLine, "id",
                                           ethCalibrationManifestQuery,
                                           sizeof(ethCalibrationManifestQuery));
  if (historical)
  {
    const bool loaded = tpExternalCalibrationLoadArchived(
        ethCalibrationManifestQuery, ethExternalArchiveMetadata,
        ethCalibrationArchiveError, sizeof(ethCalibrationArchiveError));
    client.restartBudget();
    if (!loaded)
    {
      ethSendText(client, false,
                  ethCalibrationArchiveError[0] != '\0'
                    ? ethCalibrationArchiveError
                    : "Externes PDF-Kalibrierzertifikat ist nicht verfügbar");
      return false;
    }
    metadataPtr = &ethExternalArchiveMetadata;
  }
  else
  {
    metadataPtr = &tpExternalCalibrationActive();
  }

  const TpExternalCalibrationMetadata& metadata = *metadataPtr;
  char viewText[8];
  const bool inlineView = ethGetQueryToken(requestLine, "view",
                                           viewText, sizeof(viewText)) &&
                          strcmp(viewText, "1") == 0;
  if (!metadata.present)
  {
    ethSendText(client, false, ETH_EXTERNAL_ACTIVE_NONE);
    return false;
  }
  if (!ethEnsureSdReadyForAccess())
  {
    ethSendText(client, false, ETH_EXTERNAL_SD_NOT_READY);
    return false;
  }

  strncpy(ethDownloadCurrentName, metadata.documentId, sizeof(ethDownloadCurrentName) - 1U);
  ethDownloadCurrentName[sizeof(ethDownloadCurrentName) - 1U] = '\0';
  ethDownloadCurrentOffset = 0U;

  sdStorageDiagBegin(SD_DIAG_DOWNLOAD_OPEN);
  ethDownloadIoBegin(ETH_DOWNLOAD_IO_SD);
  ethDownloadFile = SD.open(metadata.storedPdfPath, FILE_READ);
  uint8_t ioStall = ethDownloadIoEnd();
  sdStorageDiagEnd(SD_DIAG_DOWNLOAD_OPEN);
  if (ethDownloadHandleIoStall(ioStall, SD_DIAG_DOWNLOAD_OPEN, 0U) ||
      !ethDownloadFile || ethDownloadFile.isDirectory())
  {
    ethDownloadClose();
    ethSendText(client, false, ETH_EXTERNAL_PDF_NOT_READABLE);
    return false;
  }

  const uint32_t fileSize = (uint32_t)ethDownloadFile.size();
  if (fileSize != metadata.fileSize || fileSize > TP_EXTERNAL_CAL_MAX_PDF_BYTES)
  {
    ethDownloadClose();
    ethSendText(client, false, ETH_EXTERNAL_SIZE_MISMATCH);
    return false;
  }

  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nETag: \""));
  client.print(metadata.sha256);
  client.print(inlineView
                 ? F("\"\r\nContent-Disposition: inline; filename=\"")
                 : F("\"\r\nContent-Disposition: attachment; filename=\""));
  client.print(metadata.originalFileName);
  client.print(F("\"\r\nContent-Length: "));
  client.print((unsigned long)fileSize);
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
  ethDownloadRemaining = fileSize;
  ethDownloadPendingLength = 0U;
  ethDownloadPendingOffset = 0U;
  ethDownloadLastProgressMs = millis();
  ethDownloadCloseAfterMs = 0U;
  if (fileSize == 0U)
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
  int tzMinutes = 0;

  const bool hasDateTime = ethGetQueryInt(requestLine, "y", &y) &&
                           ethGetQueryInt(requestLine, "mo", &mo) &&
                           ethGetQueryInt(requestLine, "d", &d) &&
                           ethGetQueryInt(requestLine, "h", &h) &&
                           ethGetQueryInt(requestLine, "mi", &mi) &&
                           ethGetQueryInt(requestLine, "s", &sec);
  const bool hasUtcOffset = ethGetQueryInt(requestLine, "tz", &tzMinutes);

  if (!hasUtcOffset)
  {
    ethSendText(client, false, (ui_language == LANG_EN)
                              ? "Outdated web page: reload with Ctrl+F5"
                              : "Veraltete Webseite: mit Strg+F5 neu laden");
    return;
  }

  if (!hasDateTime || !ethDateValid(y, mo, d) || h < 0 || h > 23 ||
      mi < 0 || mi > 59 || sec < 0 || sec > 59 ||
      tzMinutes < -14 * 60 || tzMinutes > 14 * 60)
  {
    ethSendText(client, false, (ui_language == LANG_EN)
                              ? "Invalid time parameters"
                              : "Ungültige Zeitparameter");
    return;
  }

  setTime(h, mi, sec, d, mo, y);
  schreibeEchtzeitUhr();
  if (!tpUtcOffsetSetMinutes((int16_t)tzMinutes))
  {
    ethSendText(client, false, (ui_language == LANG_EN)
                              ? "Invalid UTC offset"
                              : "Ungültiger UTC-Offset");
    return;
  }

  const char sign = tzMinutes < 0 ? '-' : '+';
  const int tzAbs = tzMinutes < 0 ? -tzMinutes : tzMinutes;
  char msg[128];
  snprintf(msg, sizeof(msg), "OK %02d.%02d.%04d %02d:%02d:%02d UTC%c%02d:%02d%s",
           d, mo, y, h, mi, sec, sign, tzAbs / 60, tzAbs % 60,
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
    const bool ledAdaptSd = ledAdaptationSdAvailable();
    const uint8_t ledAdaptMode = ledAdaptationModeGet();
    const uint32_t ledAdaptCount = ledAdaptationAcceptedCount();
    const uint16_t ledAdaptBins = ledAdaptationValidBinCount();

    n = snprintf(ethJsonBuf, sizeof(ethJsonBuf),
      "{"
        "\"part\":0,\"lang\":%u,\"tftBusy\":%s,"
        "\"ui\":{\"mainLayout\":%u},"
        "\"control\":{"
          "\"kp\":%u,\"ki10\":%d,\"kd10\":%d,"
          "\"period\":%u,\"optik10\":%u,\"freeTemp\":%u,"
          "\"autoIdx\":%u,\"ledAdaptMode\":%u,\"ledAdaptSd\":%s,"
          "\"ledAdaptCount\":%lu,\"ledAdaptBins\":%u,"
          "\"adcFilter\":%u,\"sfocal\":%u,\"fan\":%u,\"fanEnabled\":%s,\"fanMissing\":%s,\"peltier\":%u},"
        "\"alarm\":{"
          "\"mode\":%u,\"tauL10\":%d,\"tauH10\":%d,"
          "\"rhL10\":%u,\"rhH10\":%u,\"buzzer\":%s},"
        "\"head\":{\"type\":%u,\"typeText\":\"%s\",\"serial\":%lu,\"device\":\"%s\",\"model\":\"%s\",\"certified\":%s}"
      "}",
      (unsigned)ui_language,
      flag.config_mode ? "true" : "false",
      (unsigned)mainScreenLayoutGet(),
      (unsigned)R.pid_kp, ki10, kd10,
      (unsigned)R.regler_intervall_ms,
      (unsigned)R.optik_sollwert,
      (unsigned)R.freiheiz_ziel_temp[0],
      (unsigned)R.optik_autocal_interval_index,
      (unsigned)ledAdaptMode,
      ledAdaptSd ? "true" : "false",
      (unsigned long)ledAdaptCount,
      (unsigned)ledAdaptBins,
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
      getHeadModelName(R.head_type),
      deviceIdentityCertificateValid() ? "true" : "false");
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
          "\"sdHeader\":%s,\"sdIntegrity\":%u,\"sdCertifiedReady\":%s,\"flow\":%u,"
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
      (unsigned)interfaceSdLogIntegrityMode(),
      interfaceSdCertifiedIntegrityReady() ? "true" : "false",
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
  tpSignedCalibrationInvalidateCache();
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

    if (deviceIdentityCertificateValid())
    {
      // Auch eine alte oder manipulierte Web-Seite darf die zertifizierte
      // Seriennummer nicht ueberschreiben. Fuer das Backup gilt nur die
      // wirksame SN aus dem Zertifikat.
      strncpy(newDeviceSn, deviceSerialGet(), sizeof(newDeviceSn) - 1U);
      newDeviceSn[sizeof(newDeviceSn) - 1U] = '\0';
    }
    else
    {
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
  int ledAdaptMode = (int)ledAdaptationModeGet();
  int resetLearning = 0;
  bool ok = ethGetQueryU32(requestLine, "peltier", &peltierMa) &&
            ethGetQueryInt(requestLine, "kp", &kp) &&
            ethGetQueryInt(requestLine, "ki", &ki) &&
            ethGetQueryInt(requestLine, "kd", &kd) &&
            ethGetQueryInt(requestLine, "period", &period) &&
            ethGetQueryInt(requestLine, "optik", &optik) &&
            ethGetQueryInt(requestLine, "free", &freeTemp) &&
            ethGetQueryInt(requestLine, "auto", &autoIdx) &&
            ethGetQueryInt(requestLine, "filter", &filterMode);
  // Abwaertskompatibel: alte gecachte Web-Setup-Seiten ohne die neuen
  // Parameter behalten SFOCAL und LED-Autoadaption unveraendert.
  ethGetQueryInt(requestLine, "sfocal", &sfocalMode);
  ethGetQueryInt(requestLine, "ledadapt", &ledAdaptMode);
  ethGetQueryInt(requestLine, "resetlearn", &resetLearning);

  if (!ok || peltierMa < (uint32_t)PELTIER_CURRENT_LIMIT_MIN_MA ||
      peltierMa > (uint32_t)PELTIER_CURRENT_LIMIT_MAX_MA ||
      kp < 0 || kp > 1000 || ki < 0 || ki > 500 ||
      kd < 0 || kd > 500 || !ethSetupControlPeriodValid(period) ||
      optik < 800 || optik > 990 || freeTemp < 40 || freeTemp > 76 ||
      autoIdx < 0 || autoIdx > 5 || filterMode < 0 || filterMode > (int)ADC_MEAS_FILTER_MAX ||
      sfocalMode < 0 || sfocalMode > (int)ADC1_SFOCAL_MAX ||
      ledAdaptMode < 0 || ledAdaptMode > (int)LED_AUTOADAPT_MAX ||
      resetLearning < 0 || resetLearning > 1)
  {
    ethSendText(client, false, "Ungültige Regelparameter / Invalid control parameters");
    return;
  }

  if (ledAdaptMode == (int)LED_AUTOADAPT_SELF && !ledAdaptationSdAvailable())
  {
    ethSendText(client, false, "Keine SD-Karte: Selbstlernend nicht verfuegbar / No SD card: self-learning unavailable");
    return;
  }

  if (resetLearning == 1)
  {
    char resetMessage[128] = {0};
    if (!ledAdaptationResetCurrentHead(resetMessage, sizeof(resetMessage)))
    {
      ethSendText(client, false, resetMessage[0] ? resetMessage :
                  "LED-Lerndaten nicht geloescht / LED learning data not deleted");
      return;
    }
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
  if (!ledAdaptationSetMode((uint8_t)ledAdaptMode))
  {
    ethSendText(client, false, "LED-Autoadaption konnte nicht gesetzt werden / LED auto-adaptation could not be set");
    return;
  }
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
  // Rueckwaertskompatibel zu einer eventuell noch gecachten V0.50.1_12-
  // Setupseite: fehlt g=, bleibt der aktuell gespeicherte Modus erhalten.
  int integrity = (int)interfaceSdLogIntegrityMode();
  bool ok = ethGetQueryInt(requestLine, "log", &logging) &&
            ethGetQueryInt(requestLine, "i", &interval) &&
            ethGetQueryInt(requestLine, "f", &filter) &&
            ethGetQueryInt(requestLine, "w", &writeInterval) &&
            ethGetQueryInt(requestLine, "d", &diagnostic) &&
            ethGetQueryInt(requestLine, "h", &header);
  (void)ethGetQueryInt(requestLine, "g", &integrity);

  if (!ok || logging < 0 || logging > 1 || diagnostic < 0 || diagnostic > 1 ||
      header < 0 || header > 1 || integrity < 0 || integrity >= TP_LOG_INTEGRITY_COUNT ||
      !interfaceWebSetSd((uint8_t)interval,
                         (uint8_t)filter,
                         (uint8_t)writeInterval,
                         logging != 0,
                         diagnostic != 0,
                         header != 0,
                         (uint8_t)integrity))
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

static void FLASHMEM ethSendCalibrationCertificatePage(EthBoundedWriter& client,
                                                        EthernetClient& rawClient,
                                                        const char* requestHeaders,
                                                        bool* keepOpen)
{
  const EthPageSegment segments[] = {
    { ethCalibrationCertificateIndexPage, sizeof(ethCalibrationCertificateIndexPage) - 1U }
  };
  ethBeginPageTransfer(rawClient, client, requestHeaders,
                       ETH_ETAG_CAL_CERT_INDEX,
                       ETH_REQ_CALIBRATION_CERTIFICATE, segments,
                       (uint8_t)(sizeof(segments) / sizeof(segments[0])),
                       sizeof(ethCalibrationCertificateIndexPage) - 1U,
                       keepOpen);
}

static void FLASHMEM ethSendCalibrationCertificateViewPage(
                                                        EthBoundedWriter& client,
                                                        EthernetClient& rawClient,
                                                        const char* requestHeaders,
                                                        bool* keepOpen)
{
  const EthPageSegment segments[] = {
    { ethCalibrationCertificatePart0, sizeof(ethCalibrationCertificatePart0) - 1U },
    { ethCalibrationQrLibrary, sizeof(ethCalibrationQrLibrary) - 1U },
    { ethPakoDeflateLibrary, sizeof(ethPakoDeflateLibrary) - 1U },
    { ethTp3c1EncoderLibrary, sizeof(ethTp3c1EncoderLibrary) - 1U },
    { ethCalibrationCertificatePart1, sizeof(ethCalibrationCertificatePart1) - 1U }
  };

  size_t contentLength = 0U;
  for (uint8_t i = 0U; i < (uint8_t)(sizeof(segments) / sizeof(segments[0])); i++)
  {
    contentLength += segments[i].length;
  }

  ethBeginPageTransfer(rawClient, client, requestHeaders, ETH_ETAG_CAL_CERT_VIEW,
                       ETH_REQ_CALIBRATION_CERTIFICATE_VIEW, segments,
                       (uint8_t)(sizeof(segments) / sizeof(segments[0])),
                       contentLength, keepOpen);
}

static bool FLASHMEM ethJsonAppendRaw(char* output,
                                      size_t outputSize,
                                      size_t* used,
                                      const char* text)
{
  if (output == nullptr || used == nullptr || text == nullptr) return false;
  const size_t length = strlen(text);
  if (*used + length + 1U > outputSize) return false;
  memcpy(output + *used, text, length);
  *used += length;
  output[*used] = '\0';
  return true;
}

static bool FLASHMEM ethJsonAppendFormat(char* output,
                                         size_t outputSize,
                                         size_t* used,
                                         const char* format,
                                         ...)
{
  if (output == nullptr || used == nullptr || format == nullptr ||
      *used >= outputSize) return false;
  va_list args;
  va_start(args, format);
  const int written = vsnprintf(output + *used, outputSize - *used,
                                format, args);
  va_end(args);
  if (written < 0 || (size_t)written >= outputSize - *used) return false;
  *used += (size_t)written;
  return true;
}

static bool FLASHMEM ethJsonAppendString(char* output,
                                         size_t outputSize,
                                         size_t* used,
                                         const char* text)
{
  if (!ethJsonAppendRaw(output, outputSize, used, "\"")) return false;
  if (text == nullptr) text = "";
  for (const uint8_t* p = (const uint8_t*)text; *p != 0U; ++p)
  {
    char escaped[8];
    if (*p == '"' || *p == '\\')
    {
      escaped[0] = '\\';
      escaped[1] = (char)*p;
      escaped[2] = '\0';
      if (!ethJsonAppendRaw(output, outputSize, used, escaped)) return false;
    }
    else if (*p < 0x20U)
    {
      snprintf(escaped, sizeof(escaped), "\\u%04X", (unsigned)*p);
      if (!ethJsonAppendRaw(output, outputSize, used, escaped)) return false;
    }
    else
    {
      escaped[0] = (char)*p;
      escaped[1] = '\0';
      if (!ethJsonAppendRaw(output, outputSize, used, escaped)) return false;
    }
  }
  return ethJsonAppendRaw(output, outputSize, used, "\"");
}

static bool FLASHMEM ethJsonAppendCertificateRecord(
    char* output,
    size_t outputSize,
    size_t* used,
    const TpCalibrationCertificateRecord& record)
{
  if (!ethJsonAppendRaw(output, outputSize, used, "{")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, "\"active\":")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, record.active ? "true" : "false")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"activeBindingValid\":")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, record.activeBindingValid ? "true" : "false")) return false;

  const char* firmwareApplicability = "HISTORICAL_NOT_EVALUATED";
  const char* firmwareSha256AtCalibration = "";
  if (record.active)
  {
    const TpSystemCalibrationStatus& activeSystem =
        tpSignedCalibrationSystemStatus();
    if (activeSystem.signatureValid &&
        strcasecmp(activeSystem.manifestSha256,
                   record.systemManifestSha256) == 0)
    {
      const TpSystemFirmwareApplicability firmwareState =
          ethLoadSystemFirmwareApplicability(activeSystem);
      firmwareApplicability = firmwareState == TP_SYSTEM_FW_APPLICABILITY_EXACT_MATCH
          ? "EXACT_MATCH"
          : (firmwareState == TP_SYSTEM_FW_APPLICABILITY_CHANGED
              ? "FIRMWARE_CHANGED" : "NOT_COMPARABLE");
      firmwareSha256AtCalibration =
          ethSystemFirmwareContextStatus.firmwareSha256;
    }
    else
    {
      firmwareApplicability = "NOT_COMPARABLE";
    }
  }
  if (!ethJsonAppendRaw(output, outputSize, used,
                        ",\"firmwareApplicability\":")) return false;
  if (!ethJsonAppendString(output, outputSize, used,
                           firmwareApplicability)) return false;
  if (!ethJsonAppendRaw(output, outputSize, used,
                        ",\"firmwareSha256AtCalibration\":")) return false;
  if (!ethJsonAppendString(output, outputSize, used,
                           firmwareSha256AtCalibration)) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"externalCertificateBound\":")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, record.externalCertificateBound ? "true" : "false")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"applicabilityStateKnown\":")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, record.applicabilityStateKnown ? "true" : "false")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"applicabilityEnded\":")) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, record.applicabilityEnded ? "true" : "false")) return false;
  if (!ethJsonAppendFormat(output, outputSize, used,
      ",\"applicableUntilUtc\":%lld,\"applicabilityReason\":",
      (long long)record.applicableUntilUtc)) return false;
  if (!ethJsonAppendString(output, outputSize, used, record.applicabilityReason)) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"systemManifestSha256\":")) return false;
  if (!ethJsonAppendString(output, outputSize, used, record.systemManifestSha256)) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"calibrationId\":")) return false;
  if (!ethJsonAppendString(output, outputSize, used, record.calibrationId)) return false;
  if (!ethJsonAppendFormat(output, outputSize, used,
      ",\"calibrationDateYmd\":%lu,\"validFromYmd\":%lu,\"validUntilYmd\":%lu,\"certificateSource\":" ,
      (unsigned long)record.calibrationDateYmd,
      (unsigned long)record.validFromYmd,
      (unsigned long)record.validUntilYmd)) return false;
  if (!ethJsonAppendString(output, outputSize, used, record.certificateSource)) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"externalCertificateDocumentId\":")) return false;
  if (!ethJsonAppendString(output, outputSize, used, record.externalCertificateDocumentId)) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"externalCertificateNumber\":")) return false;
  if (!ethJsonAppendString(output, outputSize, used, record.externalCertificateNumber)) return false;
  if (!ethJsonAppendRaw(output, outputSize, used, ",\"externalPdfAvailable\":")) return false;
  const bool externalPdfAvailable = record.externalCertificateBound &&
      record.externalCertificateDocumentId[0] != '\0' &&
      tpExternalCalibrationArchivedPdfPresent(record.externalCertificateDocumentId);
  if (!ethJsonAppendRaw(output, outputSize, used,
                        externalPdfAvailable ? "true" : "false")) return false;
  return ethJsonAppendRaw(output, outputSize, used, "}");
}

static void FLASHMEM ethSendCalibrationCertificatesJson(
    EthBoundedWriter& client,
    const char* requestLine)
{
  uint32_t searchFrom = 0U;
  uint32_t searchUntil = 0U;
  const bool hasFrom = ethGetQueryUint32(requestLine, "from", &searchFrom);
  const bool hasUntil = ethGetQueryUint32(requestLine, "until", &searchUntil);
  if (hasFrom != hasUntil)
  {
    ethSendText(client, false, "Für die Suche sind Zeitraum von und bis erforderlich");
    return;
  }

  size_t returned = 0U;
  size_t total = 0U;
  bool truncated = false;
  if (hasFrom)
  {
    const bool searchOk = tpSignedCalibrationSearchCertificates(
        searchFrom, searchUntil,
        ethCalibrationCertificateRecords, ETH_CAL_CERT_SEARCH_MAX,
        &returned, &total, &truncated,
        ethCalibrationArchiveError, sizeof(ethCalibrationArchiveError));
    // Die SD-Archivsuche und kryptografische Prüfung dürfen länger als das
    // normale 120-ms-Antwortbudget dauern. Das Schreibbudget beginnt deshalb
    // erst nach abgeschlossener Suche, nicht bereits beim Eingang des Requests.
    client.restartBudget();
    if (!searchOk)
    {
      ethSendText(client, false,
                  ethCalibrationArchiveError[0] != '\0'
                    ? ethCalibrationArchiveError : "Kalibrierarchiv konnte nicht durchsucht werden");
      return;
    }
  }

  TpCalibrationCertificateRecord activeRecord;
  const bool haveActive = tpSignedCalibrationGetActiveCertificateRecord(activeRecord);
  const uint32_t currentYmd = ethCurrentUtcYmd(tpCurrentUtcUnixTime());
  size_t used = 0U;
  ethJsonBuf[0] = '\0';
  bool ok = ethJsonAppendFormat(ethJsonBuf, sizeof(ethJsonBuf), &used,
                                "{\"ok\":true,\"currentYmd\":%lu,\"selectedHeadType\":" ,
                                (unsigned long)currentYmd);
  if (ok) ok = ethJsonAppendString(ethJsonBuf, sizeof(ethJsonBuf), &used,
                                   headTypeTextGet());
  if (ok) ok = ethJsonAppendFormat(ethJsonBuf, sizeof(ethJsonBuf), &used,
                                   ",\"selectedHeadSerial\":%lu,\"active\":" ,
                                   (unsigned long)R.head_serial);
  if (ok)
  {
    if (haveActive) ok = ethJsonAppendCertificateRecord(
        ethJsonBuf, sizeof(ethJsonBuf), &used, activeRecord);
    else ok = ethJsonAppendRaw(ethJsonBuf, sizeof(ethJsonBuf), &used, "null");
  }
  if (ok) ok = ethJsonAppendFormat(ethJsonBuf, sizeof(ethJsonBuf), &used,
      ",\"total\":%lu,\"truncated\":%s,\"results\":[",
      (unsigned long)total, truncated ? "true" : "false");
  for (size_t i = 0U; ok && i < returned; ++i)
  {
    if (i > 0U) ok = ethJsonAppendRaw(ethJsonBuf, sizeof(ethJsonBuf), &used, ",");
    if (ok) ok = ethJsonAppendCertificateRecord(
        ethJsonBuf, sizeof(ethJsonBuf), &used,
        ethCalibrationCertificateRecords[i]);
  }
  if (ok) ok = ethJsonAppendRaw(ethJsonBuf, sizeof(ethJsonBuf), &used, "]}");
  if (!ok)
  {
    ethSendText(client, false, "Trefferliste ist zu groß");
    return;
  }
  ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                      ethJsonBuf, used);
}

static void FLASHMEM ethSendActiveCalibrationJson(EthBoundedWriter& client,
                                                   bool device,
                                                   const char* requestLine)
{
  size_t bodyLength = 0U;
  char error[160];
  const bool historical = ethGetQueryToken(requestLine, "id",
                                           ethCalibrationManifestQuery,
                                           sizeof(ethCalibrationManifestQuery));
  const bool readOk = historical
      ? tpSignedCalibrationReadArchivedAdjustmentJson(
          device, ethCalibrationManifestQuery,
          ethJsonBuf, sizeof(ethJsonBuf), &bodyLength,
          error, sizeof(error))
      : tpSignedCalibrationReadActiveJson(
          device, ethJsonBuf, sizeof(ethJsonBuf), &bodyLength,
          error, sizeof(error));
  if (historical) client.restartBudget();
  if (!readOk)
  {
    // Der Kalibrierschein behandelt "null" als nicht vorhandenen Abschnitt.
    // Dadurch bleibt die Seite auch nutzbar, wenn nur Gerät oder nur Kopf
    // bereits signiert/importiert ist.
    static const char noCalibration[] = "null";
    ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                        noCalibration, sizeof(noCalibration) - 1U);
    return;
  }

  ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                      ethJsonBuf, bodyLength);
}

static void FLASHMEM ethSendActiveIdentityCertificateJson(EthBoundedWriter& client)
{
  size_t bodyLength = 0U;
  if (!deviceIdentityBuildCertificateJson(ethJsonBuf, sizeof(ethJsonBuf), &bodyLength))
  {
    static const char noCertificate[] = "null";
    ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                        noCertificate, sizeof(noCertificate) - 1U);
    return;
  }

  ethSendBufferedBody(client, "application/json; charset=utf-8", true,
                      ethJsonBuf, bodyLength);
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

  if (ethUploadOwnerSlot == (int8_t)index) ethUploadOwnerSlot = -1;
  slot.client = EthernetClient();
  slot.active = false;
  slot.sawAnyByte = false;
  slot.firstLineComplete = false;
  slot.headerLineHasData = false;
  slot.headersComplete = false;
  slot.bodyExpected = false;
  slot.responseDraining = false;
  slot.requestLength = 0U;
  slot.headerCacheLength = 0U;
  slot.contentLength = 0U;
  slot.bodyLength = 0U;
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
  slot.bodyExpected = false;
  slot.responseDraining = false;
  slot.requestLength = 0U;
  slot.headerCacheLength = 0U;
  slot.contentLength = 0U;
  slot.bodyLength = 0U;
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

static bool FLASHMEM ethPathEqualsMethod(const char* requestLine,
                                               const char* method,
                                               const char* path);

static bool FLASHMEM ethParseContentLength(const char* headers, size_t* value)
{
  if (headers == nullptr || value == nullptr) return false;
  static const char headerName[] = "Content-Length:";
  const char* line = headers;
  while (*line != '\0')
  {
    const char* end = strchr(line, '\n');
    size_t length = end != nullptr ? (size_t)(end - line) : strlen(line);
    while (length > 0U && (line[length - 1U] == '\r' || line[length - 1U] == '\n')) length--;
    if (ethHeaderNameEquals(line, length, headerName))
    {
      const char* p = line + strlen(headerName);
      while (*p == ' ' || *p == '\t') ++p;
      if (*p < '0' || *p > '9') return false;
      size_t parsed = 0U;
      while (*p >= '0' && *p <= '9')
      {
        const size_t digit = (size_t)(*p - '0');
        if (parsed > (ETH_UPLOAD_MAX_BYTES + 1U) / 10U) return false;
        parsed = parsed * 10U + digit;
        ++p;
      }
      *value = parsed;
      return true;
    }
    if (end == nullptr) break;
    line = end + 1;
  }
  return false;
}

static uint8_t ETH_FLASHMEM_NOINLINE ethReadRequestSlotStep(uint8_t index)
{
  if (index >= ETH_REQUEST_SLOT_COUNT) return ETH_REQUEST_ABORT;

  EthRequestSlot& slot = ethRequestSlots[index];
  char* requestLine = ethRequestLines[index];
  if (!slot.active) return ETH_REQUEST_ABORT;
  if (slot.headersComplete) return ETH_REQUEST_COMPLETE;

  if (slot.bodyExpected)
  {
    uint16_t bodyBytesThisLoop = 0U;
    while (bodyBytesThisLoop < ETH_UPLOAD_READ_BYTES_PER_LOOP &&
           slot.bodyLength < slot.contentLength && slot.client.available() > 0)
    {
      const int raw = slot.client.read();
      if (raw < 0) break;
      ethUploadBody[slot.bodyLength++] = (char)raw;
      bodyBytesThisLoop++;
      slot.lastProgressMs = millis();
    }
    if (slot.bodyLength == slot.contentLength)
    {
      ethUploadBody[slot.bodyLength] = '\0';
      slot.headersComplete = true;
      return ETH_REQUEST_COMPLETE;
    }
    if (!slot.client.connected() && slot.client.available() <= 0) return ETH_REQUEST_ABORT;
    const uint32_t bodyNow = millis();
    if ((uint32_t)(bodyNow - slot.startedMs) >= ETH_UPLOAD_ABSOLUTE_TIMEOUT_MS ||
        (uint32_t)(bodyNow - slot.lastProgressMs) >= ETH_UPLOAD_IDLE_TIMEOUT_MS)
      return ETH_REQUEST_ABORT;
    return ETH_REQUEST_WAITING;
  }

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
        if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_SYSTEM_UPLOAD) ||
            ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_DEVICE_UPLOAD) ||
            ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_HEAD_UPLOAD) ||
            ethPathEqualsMethod(requestLine, "POST", ETH_PATH_FIRMWARE_CERT_UPLOAD) ||
            ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_CHUNK))
        {
          size_t contentLength = 0U;
          if (!ethParseContentLength(ethRequestHeaderCache[index], &contentLength) ||
              contentLength == 0U || contentLength > ETH_UPLOAD_MAX_BYTES ||
              (ethUploadOwnerSlot >= 0 && ethUploadOwnerSlot != (int8_t)index))
            return ETH_REQUEST_ABORT;
          ethUploadOwnerSlot = (int8_t)index;
          slot.contentLength = contentLength;
          slot.bodyLength = 0U;
          slot.bodyExpected = true;
          slot.lastProgressMs = millis();
          return ETH_REQUEST_WAITING;
        }
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

static bool FLASHMEM ethPathEqualsMethod(const char* requestLine,
                                               const char* method,
                                               const char* path)
{
  if (requestLine == nullptr || method == nullptr || path == nullptr) return false;
  const size_t methodLength = strlen(method);
  if (strncmp(requestLine, method, methodLength) != 0 || requestLine[methodLength] != ' ') return false;
  const char* p = requestLine + methodLength + 1U;
  const size_t len = strlen(path);
  if (strncmp(p, path, len) != 0) return false;
  const char next = p[len];
  return next == ' ' || next == '?' || next == '\0';
}

static bool FLASHMEM ethPathEquals(const char* requestLine, const char* path)
{
  return ethPathEqualsMethod(requestLine, "GET", path);
}

static bool ETH_FLASHMEM_NOINLINE ethHandleReadyClient(EthernetClient& rawClient,
                                              const char* requestLine,
                                              const char* requestHeaders,
                                              const char* requestBody,
                                              size_t requestBodyLength,
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
  else if (ethPathEquals(requestLine, "/identity"))
  {
    ethSendIdentityPage(client, rawClient, requestHeaders, keepOpen);
  }
  else if (ethPathEquals(requestLine, "/identity-keygen"))
  {
    ethHandleIdentityKeygen(client, requestLine);
  }
  else if (ethPathEquals(requestLine, "/identity-request.tpreq"))
  {
    ethHandleIdentityRequest(client);
  }
  else if (ethPathEquals(requestLine, "/identity-cert-import"))
  {
    ethHandleIdentityCertificateImport(client);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_IDENTITY_ACTIVE_CERT))
  {
    ethSendActiveIdentityCertificateJson(client);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_VALIDITY))
  {
    ethSendValidityPage(client, rawClient, requestHeaders, keepOpen);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CALIBRATION))
  {
    ethSendCalibrationPage(client, rawClient, requestHeaders, keepOpen);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_CERTIFICATE))
  {
    ethSendCalibrationCertificatePage(client, rawClient, requestHeaders, keepOpen);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_CERTIFICATE_VIEW))
  {
    ethSendCalibrationCertificateViewPage(client, rawClient, requestHeaders, keepOpen);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_CERTIFICATES_JSON))
  {
    ethSendCalibrationCertificatesJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_ACTIVE_DEVICE))
  {
    ethSendActiveCalibrationJson(client, true, requestLine);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_ACTIVE_HEAD))
  {
    ethSendActiveCalibrationJson(client, false, requestLine);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_ACTIVE_SYSTEM))
  {
    ethSendActiveSystemCalibrationJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_APPLICABILITY))
  {
    ethSendCalibrationApplicabilityJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_FIRMWARE_CONTEXT))
  {
    ethSendSystemFirmwareContextJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_EXTERNAL_ACTIVE))
  {
    ethSendExternalCalibrationJson(client, requestLine);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_EXTERNAL_DOWNLOAD))
  {
    bool started = ethStartExternalCalibrationDownload(client, rawClient, requestLine);
    if (started && keepOpen != nullptr) *keepOpen = true;
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_DEVICE_REQUEST))
  {
    ethHandleCalibrationRequest(client, true);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_HEAD_REQUEST))
  {
    ethHandleCalibrationRequest(client, false);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_SYSTEM_REQUEST))
  {
    ethHandleSystemCalibrationRequest(client);
  }
  else if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_DEVICE_UPLOAD))
  {
    ethHandleAdjustmentCalibrationUpload(client, true, requestBody, requestBodyLength);
  }
  else if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_HEAD_UPLOAD))
  {
    ethHandleAdjustmentCalibrationUpload(client, false, requestBody, requestBodyLength);
  }
  else if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_SYSTEM_UPLOAD))
  {
    ethHandleSystemCalibrationUpload(client, requestBody, requestBodyLength);
  }
  else if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_BEGIN))
  {
    ethHandleExternalCalibrationBegin(client, requestLine);
  }
  else if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_CHUNK))
  {
    ethHandleExternalCalibrationChunk(client, requestLine, requestBody, requestBodyLength);
  }
  else if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_FINISH))
  {
    ethHandleExternalCalibrationFinish(client);
  }
  else if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_CAL_EXTERNAL_ABORT))
  {
    ethHandleExternalCalibrationAbort(client);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_DEVICE_IMPORT))
  {
    ethHandleCalibrationImport(client, true);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_HEAD_IMPORT))
  {
    ethHandleCalibrationImport(client, false);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_CAL_SYSTEM_IMPORT))
  {
    ethHandleSystemCalibrationImport(client);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_FIRMWARE_REQUEST))
  {
    ethHandleFirmwareRequest(client);
  }
  else if (ethPathEqualsMethod(requestLine, "POST", ETH_PATH_FIRMWARE_CERT_UPLOAD))
  {
    ethHandleFirmwareCertificateUpload(client, requestBody, requestBodyLength);
  }
  else if (ethPathEquals(requestLine, ETH_PATH_FIRMWARE_CERT_IMPORT))
  {
    ethHandleFirmwareCertificateImport(client);
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
  const char* requestBody = slot.bodyExpected ? ethUploadBody : nullptr;
  const size_t requestBodyLength = slot.bodyExpected ? slot.bodyLength : 0U;
  bool responseComplete = ethHandleReadyClient(slot.client, requestLine,
                                               ethRequestHeaderCache[index],
                                               requestBody, requestBodyLength,
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
