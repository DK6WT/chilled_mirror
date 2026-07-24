/*
 * TP-3000 certified SD log support
 *
 * The journal is deliberately small and CRC-protected. It is committed only
 * after a complete CSV flush. On boot, bytes after the last confirmed position
 * are scanned for complete newline-terminated records; an incomplete tail is
 * discarded before the file is sealed.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPcertifiedLog.h"
#include "TPsignedData.h"
#include "TPsignedCalibration.h"
#include "TPsha256.h"
#include "TPfirmwareIntegrity.h"
#include "TPsystemFirmwareContext.h"
#include "TP_T.h"

#include <SD.h>
#include <TimeLib.h>
#include <string.h>
#include <math.h>

// Cross-module interfaces.
extern uint8_t interfaceSdLogIntegrityMode(void);
extern uint32_t interfaceSdOutputIntervalMsValue(void);
extern uint8_t interfaceSdOutputFilterSecondsValue(void);
extern uint32_t interfaceSdLogIntervalMsValue(void);
extern bool interfaceSdDiagnosticDataEnabled(void);
extern bool interfaceSdLogHeaderEnabled(void);
extern bool interfaceAlmemoChannelEnabled(uint8_t index);
extern uint8_t interfaceAlmemoAddressFor(uint8_t index);
extern uint8_t interfaceAlmemoChannelFor(uint8_t index);
extern uint8_t interfaceAlmemoRole(uint8_t index);
extern uint8_t adcFilterModeGet(void);
extern uint8_t adc1SfocalModeGet(void);
extern uint16_t peltierCurrentLimitGetMa(void);
extern var_t R;
extern const char* deviceSerialGet(void);
extern const char* deviceIdentityDeviceKeyId(void);
extern bool deviceIdentityCertificateManifestHash(uint8_t output[32]);
extern bool deviceIdentityBuildCertificateJson(char* output, size_t outputSize, size_t* outputLength);
extern bool deviceIdentitySignHash(const uint8_t hash[32], uint8_t signature[64]);

namespace
{
static const char JOURNAL_PATH[] PROGMEM = "/LOG/TPLOG.HST";
static const char JOURNAL_TEMP_PATH[] PROGMEM = "/LOG/TPLOG.TMP";
static const char JOURNAL_BACKUP_PATH[] PROGMEM = "/LOG/TPLOG.BAK";
static const char CONTEXT_FORMAT[] PROGMEM = "TP3000-LOG-CONTEXT-2";
static const char LOG_FORMAT_V2[] PROGMEM = TP_FORMAT_LOG_SIGNATURE;
static const char SIGNATURE_ALGORITHM[] PROGMEM = "ECDSA-P256-SHA256";
static const char SIGNATURE_ENCODING[] PROGMEM = "IEEE-P1363";
static const uint32_t JOURNAL_MAGIC = 0x544C4F47UL; // TLOG
static const uint16_t JOURNAL_VERSION = 2U;
static const uint8_t TPLOG_HEADER_MAGIC[8] PROGMEM = {'T','P','3','L','O','G','1',0};
static const uint8_t TPLOG_FOOTER_MAGIC[8] PROGMEM = {'T','P','3','E','N','D','1',0};
static const uint16_t TPLOG_VERSION = 1U;
static const uint32_t TPLOG_FLAG_RECOVERED = 0x00000001UL;

#if defined(__IMXRT1062__)
#define TP_CERT_LOG_RODATA __attribute__((section(".progmem")))
#else
#define TP_CERT_LOG_RODATA
#endif

// All non-time-critical certified-log texts live in memory-mapped program
// flash. On Teensy 4.x ordinary char pointers can read this section directly.
static const char CL_EMPTY[] TP_CERT_LOG_RODATA = "";
static const char CL_LOG_PREFIX[] TP_CERT_LOG_RODATA = "/LOG/";
static const char CL_EXT_CSV[] TP_CERT_LOG_RODATA = ".CSV";
static const char CL_EXT_CTX[] TP_CERT_LOG_RODATA = ".CTX";
static const char CL_EXT_CTM[] TP_CERT_LOG_RODATA = ".CTM";
static const char CL_EXT_TPLOG[] TP_CERT_LOG_RODATA = ".TPLOG";
static const char CL_EXT_TPSIG[] TP_CERT_LOG_RODATA = ".TPSIG";
static const char CL_EXT_TSG[] TP_CERT_LOG_RODATA = ".TSG";
static const char CL_EXT_TPL[] TP_CERT_LOG_RODATA = ".TPL";
static const char CL_SUFFIX_SHA256[] TP_CERT_LOG_RODATA = ".sha256";
static const char CL_SUFFIX_TMP[] TP_CERT_LOG_RODATA = ".tmp";
static const char CL_FMT_REPLACE_EXT[] TP_CERT_LOG_RODATA = "%.*s%s";
static const char CL_FMT_STEM_DATE[] TP_CERT_LOG_RODATA = "%02lu%02lu%02lu%s";
static const char CL_FMT_STEM_FALLBACK[] TP_CERT_LOG_RODATA = "LOG000%s";
static const char CL_FMT_SEGMENT_PATH[] TP_CERT_LOG_RODATA = "/LOG/%s_%03u.CSV";
static const char CL_FMT_SHA_PATH[] TP_CERT_LOG_RODATA = "%s.sha256";
static const char CL_FMT_SHA_TEMP[] TP_CERT_LOG_RODATA = "%s.tmp";
static const char CL_FMT_SHA_LINE[] TP_CERT_LOG_RODATA = "%s *%s\n";
static const char CL_DIAG_SUFFIX[] TP_CERT_LOG_RODATA = "D";
static const char CL_PROVENANCE_FORMAT[] TP_CERT_LOG_RODATA = "TP3000-LOG-PROVENANCE-1";
static const char CL_PRODUCT[] TP_CERT_LOG_RODATA = "TP-3000";
static const char CL_TRUE[] TP_CERT_LOG_RODATA = "true";
static const char CL_FALSE[] TP_CERT_LOG_RODATA = "false";
static const char CL_HEX_LOWER[] TP_CERT_LOG_RODATA = "0123456789abcdef";
static const char CL_HEX_UPPER[] TP_CERT_LOG_RODATA = "0123456789ABCDEF";
static const char CL_BASE64[] TP_CERT_LOG_RODATA =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char CL_MEASUREMENT_CONFIG_JSON[] TP_CERT_LOG_RODATA =
    "{\"Format\":\"%s\",\"SdOutputInterval_ms\":%lu,\"SdOutputFilter_s\":%u,"
    "\"SdWriteInterval_ms\":%lu,\"DiagnosticDataEnabled\":%u,\"CsvHeaderEnabled\":%u,"
    "\"AdcMeasurementFilterMode\":%u,\"Adc1SfocalMode\":%u,\"PidKp\":%u,"
    "\"PidKi_x1000\":%ld,\"PidKd_x1000\":%ld,\"ControlInterval_ms\":%u,"
    "\"HBridgeDeadtime_ms\":%u,\"FanPercent\":%u,\"OpticalTarget_x10\":%u,"
    "\"PeltierCurrentLimit_mA\":%u,\"AlmemoChannels\":["
    "{\"Enabled\":%u,\"Address\":%u,\"Channel\":%u,\"Role\":%u},"
    "{\"Enabled\":%u,\"Address\":%u,\"Channel\":%u,\"Role\":%u}]}";

static const char CL_CONTEXT_HEADER_JSON[] TP_CERT_LOG_RODATA =
    "{\n  \"Format\": \"%s\",\n  \"Product\": \"TP-3000\",\n"
    "  \"DeviceSerial\": \"%s\",\n  \"DeviceKeyId\": \"%s\",\n"
    "  \"Firmware\": {\"Version\":\"%s\",\"BuildId\":\"%s\","
    "\"BuildDateYmd\":%lu,\"TargetHardware\":\"%s\","
    "\"ApprovalStatus\":\"%s\",\"ManufacturerCertificatePresent\":%s,"
    "\"ManufacturerCertificateValid\":%s},\n";

static const char CL_CONTEXT_HASHES_JSON[] TP_CERT_LOG_RODATA =
    "  \"DeviceCertificateManifestSha256\": \"%s\",\n"
    "  \"DeviceCalibrationManifestSha256\": \"%s\",\n"
    "  \"HeadCalibrationManifestSha256\": \"%s\",\n"
    "  \"SystemCalibrationManifestSha256\": \"%s\",\n"
    "  \"FirmwareManifestSha256\": \"%s\",\n"
    "  \"MeasurementConfigurationSha256\": \"%s\",\n";

static const char CL_CONTEXT_DEVICE_CERT[] TP_CERT_LOG_RODATA = "  \"DeviceCertificate\": ";
static const char CL_CONTEXT_DEVICE_CAL[] TP_CERT_LOG_RODATA = ",\n  \"DeviceCalibration\": ";
static const char CL_CONTEXT_HEAD_CAL[] TP_CERT_LOG_RODATA = ",\n  \"HeadCalibration\": ";
static const char CL_CONTEXT_SYSTEM_CAL[] TP_CERT_LOG_RODATA = ",\n  \"SystemCalibration\": ";
static const char CL_CONTEXT_SYSTEM_FWCTX[] TP_CERT_LOG_RODATA = ",\n  \"SystemCalibrationFirmwareContext\": ";
static const char CL_CONTEXT_SYSTEM_FWAPP[] TP_CERT_LOG_RODATA = ",\n  \"SystemCalibrationFirmwareApplicability\": \"%s\"";
static const char CL_CONTEXT_SEPARATOR[] TP_CERT_LOG_RODATA = ",\n";
static const char CL_CONTEXT_NULLS[] TP_CERT_LOG_RODATA =
    "  \"DeviceCertificate\": null,\n  \"DeviceCalibration\": null,\n"
    "  \"HeadCalibration\": null,\n  \"SystemCalibration\": null,\n";
static const char CL_CONTEXT_MEAS_CONFIG[] TP_CERT_LOG_RODATA = "  \"MeasurementConfiguration\": ";
static const char CL_CONTEXT_END[] TP_CERT_LOG_RODATA = "\n}\n";

static const char CL_TPSIG_HEADER_JSON[] TP_CERT_LOG_RODATA =
    "{\n  \"Format\": \"%s\",\n  \"Product\": \"TP-3000\",\n"
    "  \"LogFileName\": \"%s\",\n  \"LogFileSize\": %lu,\n"
    "  \"LogFileSha256\": \"%s\",\n  \"SnapshotStartUtc\": %lld,\n"
    "  \"SnapshotEndUtc\": %lld,\n  \"CreatedUtc\": %lld,\n"
    "  \"DeviceSerial\": \"%s\",\n  \"DeviceKeyId\": \"%s\",\n"
    "  \"DeviceCertificateManifestSha256\": \"%s\",\n"
    "  \"DeviceCalibrationManifestSha256\": \"%s\",\n"
    "  \"HeadCalibrationManifestSha256\": \"%s\",\n"
    "  \"SystemCalibrationManifestSha256\": \"%s\",\n"
    "  \"FirmwareManifestSha256\": \"%s\",\n"
    "  \"MeasurementConfigurationSha256\": \"%s\",\n"
    "  \"ContextSha256\": \"%s\",\n"
    "  \"RecoveredAfterUncleanShutdown\": %s,\n  \"Context\": ";

static const char CL_TPSIG_FOOTER_JSON[] TP_CERT_LOG_RODATA =
    ",\n  \"ManifestSha256\": \"%s\",\n"
    "  \"SignatureAlgorithm\": \"%s\",\n"
    "  \"SignatureEncoding\": \"%s\",\n"
    "  \"DeviceSignatureBase64\": \"%s\"\n}\n";

static const char CL_ERR_CONTEXT_PATH[] TP_CERT_LOG_RODATA = "Kontextpfad ist zu lang";
static const char CL_ERR_CONTEXT_INVALID[] TP_CERT_LOG_RODATA = "Eingefrorener Log-Kontext fehlt oder wurde verändert";
static const char CL_ERR_CONTEXT_CREATE[] TP_CERT_LOG_RODATA = "Log-Kontext konnte nicht angelegt werden";
static const char CL_ERR_CONTEXT_WRITE[] TP_CERT_LOG_RODATA = "Log-Kontext konnte nicht vollständig geschrieben werden";
static const char CL_ERR_CONTEXT_VERIFY[] TP_CERT_LOG_RODATA = "Log-Kontext konnte nicht geprüft werden";
static const char CL_ERR_CONTEXT_ACTIVATE[] TP_CERT_LOG_RODATA = "Log-Kontext konnte nicht aktiviert werden";
static const char CL_ERR_JOURNAL_WRITE[] TP_CERT_LOG_RODATA = "Log-Journal konnte nicht geschrieben werden";
static const char CL_ERR_JOURNAL_VERIFY[] TP_CERT_LOG_RODATA = "Log-Journal-Prüfung fehlgeschlagen";
static const char CL_ERR_JOURNAL_BACKUP[] TP_CERT_LOG_RODATA = "Altes Log-Journal konnte nicht gesichert werden";
static const char CL_ERR_JOURNAL_ACTIVATE[] TP_CERT_LOG_RODATA = "Log-Journal konnte nicht aktiviert werden";
static const char CL_ERR_MODE[] TP_CERT_LOG_RODATA = "Ungültiger Integritätsmodus";
static const char CL_ERR_EVIDENCE[] TP_CERT_LOG_RODATA = "Zertifizierte Nachweise sind nicht vollständig";
static const char CL_ERR_CONFIG_BIND[] TP_CERT_LOG_RODATA = "Messkonfiguration konnte nicht gebunden werden";
static const char CL_ERR_RECOVER_OPEN[] TP_CERT_LOG_RODATA = "Offene Logdatei konnte nicht wiederhergestellt werden";
static const char CL_ERR_RECOVER_TRUNCATE[] TP_CERT_LOG_RODATA = "Unvollständiges CSV-Ende konnte nicht entfernt werden";
static const char CL_ERR_SHA_PATH[] TP_CERT_LOG_RODATA = "Prüfsummenpfad ist zu lang";
static const char CL_ERR_SHA_WRITE[] TP_CERT_LOG_RODATA = "SHA-256-Begleitdatei konnte nicht geschrieben werden";
static const char CL_ERR_SHA_VERIFY[] TP_CERT_LOG_RODATA = "SHA-256-Begleitdatei konnte nicht geprüft werden";
static const char CL_ERR_SHA_ACTIVATE[] TP_CERT_LOG_RODATA = "SHA-256-Begleitdatei konnte nicht aktiviert werden";
static const char CL_ERR_CANONICAL[] TP_CERT_LOG_RODATA = "Kanonischer Lognachweis ist zu groß";
static const char CL_ERR_SIGN[] TP_CERT_LOG_RODATA = "Gerätesignatur der Logdatei fehlgeschlagen";
static const char CL_ERR_SIGN_ENCODE[] TP_CERT_LOG_RODATA = "Logsignatur konnte nicht codiert werden";
static const char CL_ERR_TPSIG_PATH[] TP_CERT_LOG_RODATA = "TPSIG-Pfad ist zu lang";
static const char CL_ERR_TPSIG_WRITE[] TP_CERT_LOG_RODATA = "TPSIG konnte nicht vollständig geschrieben werden";
static const char CL_ERR_TPSIG_VERIFY[] TP_CERT_LOG_RODATA = "TPSIG konnte nicht geprüft werden";
static const char CL_ERR_TPSIG_ACTIVATE[] TP_CERT_LOG_RODATA = "TPSIG konnte nicht aktiviert werden";
static const char CL_ERR_TPLOG_PATH[] TP_CERT_LOG_RODATA = "TPLOG-Pfad ist zu lang";
static const char CL_ERR_TPLOG_EVIDENCE[] TP_CERT_LOG_RODATA = "TPSIG für TPLOG fehlt oder ist ungültig";
static const char CL_ERR_TPLOG_WRITE[] TP_CERT_LOG_RODATA = "TPLOG konnte nicht vollständig geschrieben werden";
static const char CL_ERR_TPLOG_VERIFY[] TP_CERT_LOG_RODATA = "TPLOG-Struktur- oder Hashprüfung fehlgeschlagen";
static const char CL_ERR_TPLOG_ACTIVATE[] TP_CERT_LOG_RODATA = "TPLOG konnte nicht aktiviert werden";
static const char CL_ERR_JOURNAL_INVALID[] TP_CERT_LOG_RODATA = "Ungültiges Log-Journal wurde entfernt";
static const char CL_ERR_JOURNAL_ORPHAN[] TP_CERT_LOG_RODATA = "Verwaistes Log-Journal wurde entfernt";
static const char CL_ERR_SEGMENT_EMPTY[] TP_CERT_LOG_RODATA = "Leerer unterbrochener Logabschnitt wurde entfernt";
static const char CL_ERR_ROTATE[] TP_CERT_LOG_RODATA = "Aktiver Logabschnitt muss zuerst rotiert werden";
static const char CL_ERR_NO_FILENAME[] TP_CERT_LOG_RODATA = "Kein freier Log-Dateiname verfügbar";
static const char CL_ERR_CSV_PATH[] TP_CERT_LOG_RODATA = "Ungültiger segmentierter CSV-Pfad";
static const char CL_ERR_CSV_ACTIVE_MISMATCH[] TP_CERT_LOG_RODATA = "Aktiver Logabschnitt stimmt nicht zum CSV-Pfad";
static const char CL_ERR_CSV_NO_JOURNAL[] TP_CERT_LOG_RODATA = "Segmentierte CSV-Datei besitzt kein gültiges Journal";
static const char CL_ERR_JOURNAL_AFTER_FLUSH[] TP_CERT_LOG_RODATA = "Log-Journal fehlt nach CSV-Flush";
static const char CL_ERR_CSV_MISSING[] TP_CERT_LOG_RODATA = "Aktive CSV-Datei fehlt; Journal wurde entfernt";
static const char CL_ERR_CSV_HASH[] TP_CERT_LOG_RODATA = "CSV-SHA-256 konnte nicht gebildet werden";
static const char CL_ERR_TPLOG_OUTPUT[] TP_CERT_LOG_RODATA = "TPLOG-Ausgabepfad ist zu lang";
static const char CL_ERR_TPSIG_INTERMEDIATE[] TP_CERT_LOG_RODATA = "TPSIG-Zwischenpfad ist zu lang";

struct __attribute__((packed)) TpLogContainerHeaderV1
{
  uint8_t magic[8];
  uint16_t version;
  uint16_t headerSize;
  uint32_t flags;
  uint64_t csvOffset;
  uint64_t csvLength;
  uint64_t evidenceOffset;
  uint64_t evidenceLength;
  uint8_t csvSha256[32];
  uint8_t evidenceSha256[32];
  uint32_t crc32;
};

struct __attribute__((packed)) TpLogContainerFooterV1
{
  uint8_t magic[8];
  uint16_t version;
  uint16_t footerSize;
  uint32_t reserved;
  uint64_t totalSize;
  uint64_t headerOffset;
  uint32_t headerCrc32;
  uint32_t crc32;
};

static_assert(sizeof(TpLogContainerHeaderV1) == 116U, "Unexpected TPLOG header size");
static_assert(sizeof(TpLogContainerFooterV1) == 40U, "Unexpected TPLOG footer size");

struct __attribute__((packed)) TpMeasurementConfigV1
{
  uint32_t sdOutputIntervalMs;
  uint16_t sdOutputFilterSeconds;
  uint32_t sdWriteIntervalMs;
  uint8_t diagnosticDataEnabled;
  uint8_t csvHeaderEnabled;
  uint8_t adcMeasurementFilterMode;
  uint8_t adc1SfocalMode;
  uint16_t pidKp;
  int32_t pidKiX1000;
  int32_t pidKdX1000;
  uint16_t controlIntervalMs;
  uint8_t hBridgeDeadtimeMs;
  uint8_t fanPercent;
  uint16_t opticalTargetX10;
  uint16_t peltierCurrentLimitMa;
  uint8_t almemoEnabled[2];
  uint8_t almemoAddress[2];
  uint8_t almemoChannel[2];
  uint8_t almemoRole[2];
};

struct __attribute__((packed)) TpLogJournalV2
{
  uint32_t magic;
  uint16_t version;
  uint8_t mode;
  uint8_t diagnostic;
  uint16_t sequence;
  uint16_t reserved;
  uint32_t dateYmd;
  int64_t snapshotStartUtc;
  int64_t lastConfirmedUtc;
  uint32_t confirmedSize;
  char csvPath[48];
  char deviceSerial[8];
  char deviceKeyId[17];
  uint8_t deviceCertificateManifestSha256[32];
  uint8_t deviceCalibrationManifestSha256[32];
  uint8_t headCalibrationManifestSha256[32];
  uint8_t systemCalibrationManifestSha256[32];
  uint8_t firmwareManifestSha256[32];
  uint8_t measurementConfigurationSha256[32];
  uint8_t provenanceSha256[32];
  uint8_t contextSha256[32];
  TpMeasurementConfigV1 measurementConfiguration;
  uint32_t crc32;
};

static DMAMEM TpLogJournalV2 logState;
static DMAMEM char contextScratch[2304];
static DMAMEM TpSystemFirmwareContextStatus certifiedLogFirmwareContext;
static DMAMEM uint8_t containerIoBuffer[1024];
static bool stateActive = false;
static bool beginDone = false;
static char lastError[128] = {0};

static void FLASHMEM copyText(char* target, size_t targetSize, const char* text)
{
  if (target == nullptr || targetSize == 0U) return;
  if (text == nullptr) text = CL_EMPTY;
  const size_t length = strnlen(text, targetSize - 1U);
  memmove(target, text, length);
  target[length] = '\0';
}

static void FLASHMEM setError(char* output, size_t outputSize, const char* text)
{
  if (text == nullptr) text = CL_EMPTY;
  copyText(lastError, sizeof(lastError), text);
  copyText(output, outputSize, text);
}

static uint32_t FLASHMEM crc32Bytes(const void* data, size_t length)
{
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0U; i < length; i++)
  {
    crc ^= p[i];
    for (uint8_t bit = 0U; bit < 8U; bit++)
      crc = (crc >> 1U) ^ (0xEDB88320UL & (uint32_t)-(int32_t)(crc & 1U));
  }
  return ~crc;
}

static bool FLASHMEM bytesEqual(const uint8_t* a, const uint8_t* b, size_t count)
{
  if (a == nullptr || b == nullptr) return false;
  uint8_t diff = 0U;
  for (size_t i = 0U; i < count; i++) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0U;
}

static void FLASHMEM bytesToHex(const uint8_t* bytes, size_t count, char* out, size_t outSize, bool upper = false)
{
  if (out == nullptr || outSize == 0U) return;
  const char* alphabet = upper ? CL_HEX_UPPER : CL_HEX_LOWER;
  if (bytes == nullptr || outSize < count * 2U + 1U)
  {
    out[0] = '\0';
    return;
  }
  for (size_t i = 0U; i < count; i++)
  {
    out[i * 2U] = alphabet[bytes[i] >> 4U];
    out[i * 2U + 1U] = alphabet[bytes[i] & 0x0FU];
  }
  out[count * 2U] = '\0';
}

static bool FLASHMEM hexToBytes(const char* text, uint8_t* out, size_t count)
{
  if (text == nullptr || out == nullptr || strlen(text) != count * 2U) return false;
  for (size_t i = 0U; i < count; i++)
  {
    uint8_t value = 0U;
    for (uint8_t j = 0U; j < 2U; j++)
    {
      char c = text[i * 2U + j];
      uint8_t nibble;
      if (c >= '0' && c <= '9') nibble = (uint8_t)(c - '0');
      else if (c >= 'a' && c <= 'f') nibble = (uint8_t)(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') nibble = (uint8_t)(c - 'A' + 10);
      else return false;
      value = (uint8_t)((value << 4U) | nibble);
    }
    out[i] = value;
  }
  return true;
}

static bool FLASHMEM base64Encode(const uint8_t* input, size_t inputLength,
                                  char* output, size_t outputSize)
{
  const char* alphabet = CL_BASE64;
  const size_t required = ((inputLength + 2U) / 3U) * 4U + 1U;
  if (input == nullptr || output == nullptr || outputSize < required) return false;
  size_t in = 0U, out = 0U;
  while (in < inputLength)
  {
    const size_t remain = inputLength - in;
    uint32_t v = (uint32_t)input[in++] << 16U;
    if (remain > 1U) v |= (uint32_t)input[in++] << 8U;
    if (remain > 2U) v |= input[in++];
    output[out++] = alphabet[(v >> 18U) & 63U];
    output[out++] = alphabet[(v >> 12U) & 63U];
    output[out++] = remain > 1U ? alphabet[(v >> 6U) & 63U] : '=';
    output[out++] = remain > 2U ? alphabet[v & 63U] : '=';
  }
  output[out] = '\0';
  return true;
}

static uint32_t FLASHMEM currentDateYmd(void)
{
  const int y = year();
  if (y < 2020 || y > 2099) return 0U;
  return (uint32_t)y * 10000UL + (uint32_t)month() * 100UL + (uint32_t)day();
}

static int64_t FLASHMEM currentUtc(void)
{
  // Die Geräte-RTC bleibt in lokaler Bedienzeit. Kryptografische Zeitstempel
  // müssen über den persistent gespeicherten UTC-Offset normalisiert werden.
  return tpCurrentUtcUnixTime();
}

static const char* FLASHMEM baseName(const char* path)
{
  if (path == nullptr) return CL_EMPTY;
  const char* slash = strrchr(path, '/');
  return slash == nullptr ? path : slash + 1;
}

static bool FLASHMEM asciiEqualsIgnoreCase(const char* a, const char* b)
{
  if (a == nullptr || b == nullptr) return false;
  while (*a && *b)
  {
    char ca = *a++;
    char cb = *b++;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return false;
  }
  return *a == '\0' && *b == '\0';
}

static bool FLASHMEM safeCsvPath(const char* path)
{
  if (path == nullptr || strncmp(path, CL_LOG_PREFIX, 5U) != 0) return false;
  const size_t len = strlen(path);
  if (len < 10U || len >= sizeof(logState.csvPath)) return false;
  const char* ext = path + len - 4U;
  if (!asciiEqualsIgnoreCase(ext, CL_EXT_CSV)) return false;
  for (const char* p = path + 5U; *p; p++)
  {
    const char c = *p;
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z') || c == '_' || c == '-' || c == '.')) return false;
  }
  return true;
}

static bool FLASHMEM replaceExtension(const char* csvPath, const char* suffix,
                                      char* output, size_t outputSize)
{
  if (!safeCsvPath(csvPath) || suffix == nullptr || output == nullptr) return false;
  const size_t len = strlen(csvPath);
  const int written = snprintf(output, outputSize, CL_FMT_REPLACE_EXT, (int)(len - 4U), csvPath, suffix);
  return written > 0 && (size_t)written < outputSize;
}

static void FLASHMEM captureMeasurementConfig(TpMeasurementConfigV1& cfg)
{
  memset(&cfg, 0, sizeof(cfg));
  cfg.sdOutputIntervalMs = interfaceSdOutputIntervalMsValue();
  cfg.sdOutputFilterSeconds = interfaceSdOutputFilterSecondsValue();
  cfg.sdWriteIntervalMs = interfaceSdLogIntervalMsValue();
  cfg.diagnosticDataEnabled = interfaceSdDiagnosticDataEnabled() ? 1U : 0U;
  cfg.csvHeaderEnabled = interfaceSdLogHeaderEnabled() ? 1U : 0U;
  cfg.adcMeasurementFilterMode = adcFilterModeGet();
  cfg.adc1SfocalMode = adc1SfocalModeGet();
  cfg.pidKp = R.pid_kp;
  cfg.pidKiX1000 = (int32_t)lroundf(R.pid_ki * 1000.0f);
  cfg.pidKdX1000 = (int32_t)lroundf(R.pid_kd * 1000.0f);
  cfg.controlIntervalMs = R.regler_intervall_ms;
  cfg.hBridgeDeadtimeMs = R.h_bruecke_totzeit;
  cfg.fanPercent = R.fan_percent;
  cfg.opticalTargetX10 = R.optik_sollwert;
  cfg.peltierCurrentLimitMa = peltierCurrentLimitGetMa();
  for (uint8_t i = 0U; i < 2U; i++)
  {
    cfg.almemoEnabled[i] = interfaceAlmemoChannelEnabled(i) ? 1U : 0U;
    cfg.almemoAddress[i] = interfaceAlmemoAddressFor(i);
    cfg.almemoChannel[i] = interfaceAlmemoChannelFor(i);
    cfg.almemoRole[i] = interfaceAlmemoRole(i);
  }
}

static bool FLASHMEM hashMeasurementConfig(const TpMeasurementConfigV1& cfg, uint8_t out[32])
{
  uint8_t canonical[128];
  TpCanonicalWriter w(canonical, sizeof(canonical));
  const bool ok = w.writeUtf8(TP_FORMAT_MEASUREMENT_CONFIG) &&
      w.writeU32(cfg.sdOutputIntervalMs) &&
      w.writeU16(cfg.sdOutputFilterSeconds) &&
      w.writeU32(cfg.sdWriteIntervalMs) &&
      w.writeU8(cfg.diagnosticDataEnabled) &&
      w.writeU8(cfg.csvHeaderEnabled) &&
      w.writeU8(cfg.adcMeasurementFilterMode) &&
      w.writeU8(cfg.adc1SfocalMode) &&
      w.writeU16(cfg.pidKp) &&
      w.writeI32(cfg.pidKiX1000) &&
      w.writeI32(cfg.pidKdX1000) &&
      w.writeU16(cfg.controlIntervalMs) &&
      w.writeU8(cfg.hBridgeDeadtimeMs) &&
      w.writeU8(cfg.fanPercent) &&
      w.writeU16(cfg.opticalTargetX10) &&
      w.writeU16(cfg.peltierCurrentLimitMa);
  if (!ok) return false;
  for (uint8_t i = 0U; i < 2U; i++)
  {
    if (!w.writeU8(cfg.almemoEnabled[i]) || !w.writeU8(cfg.almemoAddress[i]) ||
        !w.writeU8(cfg.almemoChannel[i]) || !w.writeU8(cfg.almemoRole[i])) return false;
  }
  TpSha256::hash(canonical, w.length(), out);
  memset(canonical, 0, sizeof(canonical));
  return true;
}

static bool FLASHMEM loadEvidenceHashes(TpLogJournalV2& state, bool requireCertified)
{
  memset(state.deviceCertificateManifestSha256, 0, 32U);
  memset(state.deviceCalibrationManifestSha256, 0, 32U);
  memset(state.headCalibrationManifestSha256, 0, 32U);
  memset(state.systemCalibrationManifestSha256, 0, 32U);
  memset(state.firmwareManifestSha256, 0, 32U);

  if (!requireCertified) return true;
  if (!tpSignedDataCertifiedEvidenceReady() ||
      !tpFirmwareIntegrityGetMeasuredHash(state.firmwareManifestSha256) ||
      !deviceIdentityCertificateManifestHash(state.deviceCertificateManifestSha256))
    return false;
  const TpSignedCalibrationStatus& device = tpSignedCalibrationDeviceStatus();
  const TpSignedCalibrationStatus& head = tpSignedCalibrationHeadStatus();
  const TpSystemCalibrationStatus& system = tpSignedCalibrationSystemStatus();
  return hexToBytes(device.manifestSha256, state.deviceCalibrationManifestSha256, 32U) &&
         hexToBytes(head.manifestSha256, state.headCalibrationManifestSha256, 32U) &&
         hexToBytes(system.manifestSha256, state.systemCalibrationManifestSha256, 32U);
}

static bool FLASHMEM calculateProvenance(TpLogJournalV2& state, bool requireCertified)
{
  captureMeasurementConfig(state.measurementConfiguration);
  if (!hashMeasurementConfig(state.measurementConfiguration,
                             state.measurementConfigurationSha256)) return false;
  if (!loadEvidenceHashes(state, requireCertified)) return false;

  uint8_t canonical[384];
  TpCanonicalWriter w(canonical, sizeof(canonical));
  const bool ok = w.writeUtf8(CL_PROVENANCE_FORMAT) &&
      w.writeUtf8(TP_FIRMWARE_VERSION_STRING) &&
      w.writeUtf8(TP_FIRMWARE_BUILD_ID_STRING) &&
      w.writeUtf8(TP_FIRMWARE_DEVELOPMENT_STATUS) &&
      w.writeHash32(state.deviceCertificateManifestSha256) &&
      w.writeHash32(state.deviceCalibrationManifestSha256) &&
      w.writeHash32(state.headCalibrationManifestSha256) &&
      w.writeHash32(state.systemCalibrationManifestSha256) &&
      w.writeHash32(state.firmwareManifestSha256) &&
      w.writeHash32(state.measurementConfigurationSha256);
  if (!ok) return false;
  TpSha256::hash(canonical, w.length(), state.provenanceSha256);
  memset(canonical, 0, sizeof(canonical));
  return true;
}

static bool FLASHMEM hashFile(const char* path, uint8_t digest[32], uint32_t* sizeOut)
{
  if (sizeOut) *sizeOut = 0U;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory())
  {
    if (f) f.close();
    return false;
  }
  TpSha256 hash;
  uint8_t buffer[512];
  uint32_t total = 0U;
  while (f.available())
  {
    int got = f.read(buffer, sizeof(buffer));
    if (got <= 0)
    {
      f.close();
      memset(buffer, 0, sizeof(buffer));
      return false;
    }
    hash.update(buffer, (size_t)got);
    total += (uint32_t)got;
  }
  f.close();
  hash.final(digest);
  memset(buffer, 0, sizeof(buffer));
  if (sizeOut) *sizeOut = total;
  return true;
}

static bool FLASHMEM hashFileRange(const char* path, uint64_t offset, uint64_t length,
                                   uint8_t digest[32])
{
  if (path == nullptr || digest == nullptr || offset > 0xFFFFFFFFULL ||
      length > 0xFFFFFFFFULL || offset + length > 0xFFFFFFFFULL) return false;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory() || !f.seek((uint32_t)offset))
  {
    if (f) f.close();
    return false;
  }
  TpSha256 hash;
  uint64_t remaining = length;
  while (remaining > 0U)
  {
    const size_t want = remaining < sizeof(containerIoBuffer)
                      ? (size_t)remaining : sizeof(containerIoBuffer);
    const int got = f.read(containerIoBuffer, want);
    if (got <= 0 || (size_t)got != want)
    {
      f.close();
      memset(containerIoBuffer, 0, sizeof(containerIoBuffer));
      return false;
    }
    hash.update(containerIoBuffer, want);
    remaining -= want;
  }
  f.close();
  hash.final(digest);
  memset(containerIoBuffer, 0, sizeof(containerIoBuffer));
  return true;
}

static bool FLASHMEM copyFileToPrint(const char* path, Print& output)
{
  File source = SD.open(path, FILE_READ);
  if (!source || source.isDirectory())
  {
    if (source) source.close();
    return false;
  }
  while (source.available())
  {
    int got = source.read(containerIoBuffer, sizeof(containerIoBuffer));
    if (got <= 0 || output.write(containerIoBuffer, (size_t)got) != (size_t)got)
    {
      source.close();
      memset(containerIoBuffer, 0, sizeof(containerIoBuffer));
      return false;
    }
  }
  source.close();
  memset(containerIoBuffer, 0, sizeof(containerIoBuffer));
  return true;
}

static bool FLASHMEM verifyFrozenContext(char* errorText, size_t errorTextSize)
{
  char contextPath[64];
  if (!replaceExtension(logState.csvPath, CL_EXT_CTX, contextPath, sizeof(contextPath)))
  {
    setError(errorText, errorTextSize, CL_ERR_CONTEXT_PATH);
    return false;
  }
  uint8_t actualHash[32];
  uint32_t actualSize = 0U;
  const bool ok = hashFile(contextPath, actualHash, &actualSize) && actualSize >= 64U &&
                  bytesEqual(actualHash, logState.contextSha256, 32U);
  memset(actualHash, 0, sizeof(actualHash));
  if (!ok)
  {
    setError(errorText, errorTextSize, CL_ERR_CONTEXT_INVALID);
    return false;
  }
  return true;
}

static bool FLASHMEM printMeasurementConfig(Print& out, const TpMeasurementConfigV1& c)
{
  return out.printf(CL_MEASUREMENT_CONFIG_JSON,
      TP_FORMAT_MEASUREMENT_CONFIG,
      (unsigned long)c.sdOutputIntervalMs, (unsigned)c.sdOutputFilterSeconds,
      (unsigned long)c.sdWriteIntervalMs, (unsigned)c.diagnosticDataEnabled,
      (unsigned)c.csvHeaderEnabled, (unsigned)c.adcMeasurementFilterMode,
      (unsigned)c.adc1SfocalMode, (unsigned)c.pidKp, (long)c.pidKiX1000,
      (long)c.pidKdX1000, (unsigned)c.controlIntervalMs,
      (unsigned)c.hBridgeDeadtimeMs, (unsigned)c.fanPercent,
      (unsigned)c.opticalTargetX10, (unsigned)c.peltierCurrentLimitMa,
      (unsigned)c.almemoEnabled[0], (unsigned)c.almemoAddress[0],
      (unsigned)c.almemoChannel[0], (unsigned)c.almemoRole[0],
      (unsigned)c.almemoEnabled[1], (unsigned)c.almemoAddress[1],
      (unsigned)c.almemoChannel[1], (unsigned)c.almemoRole[1]) > 0;
}

static const char* FLASHMEM firmwareApprovalStatusForLog(
    const TpFirmwareIntegrityStatus& firmware)
{
  if (firmware.state == TP_FW_INTEGRITY_VERIFIED)
    return "MANUFACTURER_APPROVED";
  if (!firmware.certificatePresent)
    return "OPERATOR_VALIDATION_REQUIRED";
  if (firmware.certificateSignatureValid && !firmware.certificateMatchesImage)
    return "MANUFACTURER_CERTIFICATE_MISMATCH";
  return "MANUFACTURER_CERTIFICATE_INVALID";
}

static const char* FLASHMEM certifiedLogFirmwareApplicabilityText(
    TpSystemFirmwareApplicability applicability)
{
  switch (applicability)
  {
    case TP_SYSTEM_FW_APPLICABILITY_EXACT_MATCH: return "EXACT_MATCH";
    case TP_SYSTEM_FW_APPLICABILITY_CHANGED: return "FIRMWARE_CHANGED_POSSIBLY_INVALID";
    default: return "NOT_COMPARABLE";
  }
}

static bool FLASHMEM writeSystemFirmwareContextJson(File& out)
{
  memset(&certifiedLogFirmwareContext, 0,
         sizeof(certifiedLogFirmwareContext));
  const TpSystemCalibrationStatus& system = tpSignedCalibrationSystemStatus();
  const bool loaded = system.signatureValid &&
      tpSystemFirmwareContextLoad(system.sourceRequestId,
                                  system.sourceRequestManifestSha256,
                                  certifiedLogFirmwareContext);
  if (!out.print(CL_CONTEXT_SYSTEM_FWCTX)) return false;
  if (!loaded)
  {
    if (!out.print("null")) return false;
    return out.printf(CL_CONTEXT_SYSTEM_FWAPP, "NOT_COMPARABLE") > 0;
  }

  const char* manufacturer = "NOT_CERTIFIED";
  switch (certifiedLogFirmwareContext.manufacturerStatus)
  {
    case TP_SYSTEM_FW_MANUFACTURER_VALID: manufacturer = "VALID"; break;
    case TP_SYSTEM_FW_MANUFACTURER_MISMATCH: manufacturer = "MISMATCH"; break;
    case TP_SYSTEM_FW_MANUFACTURER_INVALID: manufacturer = "INVALID"; break;
    default: break;
  }
  const char* captureMode = certifiedLogFirmwareContext.captureMode ==
      TP_SYSTEM_FW_CAPTURE_REQUEST ? "REQUEST" : "ACTIVATION_FALLBACK";
  const bool ok = out.printf(
      "{\"Format\":\"TP3000-SYSTEM-FIRMWARE-CONTEXT-1\","
      "\"CaptureMode\":\"%s\",\"CapturedUtc\":%lld,"
      "\"FirmwareVersion\":\"%s\","
      "\"FirmwareBuildId\":\"%s\","
      "\"ImageBase\":%lu,\"ImageSize\":%lu,"
      "\"FirmwareSha256\":\"%s\","
      "\"ManufacturerStatus\":\"%s\","
      "\"FirmwareCertificateId\":\"%s\","
      "\"ManufacturerRootKeyId\":\"%s\","
      "\"CertificateApprovedUtc\":%lld,"
      "\"SourceRequestId\":\"%s\","
      "\"SourceRequestManifestSha256\":\"%s\","
      "\"ContextManifestSha256\":\"%s\","
      "\"DeviceSignatureHex\":\"%s\"}",
      captureMode,
      (long long)certifiedLogFirmwareContext.capturedUtc,
      certifiedLogFirmwareContext.firmwareVersion,
      certifiedLogFirmwareContext.firmwareBuildId,
      (unsigned long)certifiedLogFirmwareContext.imageBase,
      (unsigned long)certifiedLogFirmwareContext.imageSize,
      certifiedLogFirmwareContext.firmwareSha256,
      manufacturer,
      certifiedLogFirmwareContext.firmwareCertificateId,
      certifiedLogFirmwareContext.manufacturerRootKeyId,
      (long long)certifiedLogFirmwareContext.certificateApprovedUtc,
      certifiedLogFirmwareContext.sourceRequestId,
      certifiedLogFirmwareContext.sourceRequestManifestSha256,
      certifiedLogFirmwareContext.contextManifestSha256,
      certifiedLogFirmwareContext.deviceSignatureHex) > 0;
  return ok && out.printf(
      CL_CONTEXT_SYSTEM_FWAPP,
      certifiedLogFirmwareApplicabilityText(
          certifiedLogFirmwareContext.applicability)) > 0;
}

static bool FLASHMEM writeContextFile(TpLogJournalV2& state, char* errorText, size_t errorTextSize)
{
  char contextPath[64];
  char tempPath[64];
  if (!replaceExtension(state.csvPath, CL_EXT_CTX, contextPath, sizeof(contextPath)) ||
      !replaceExtension(state.csvPath, CL_EXT_CTM, tempPath, sizeof(tempPath)))
  {
    setError(errorText, errorTextSize, CL_ERR_CONTEXT_PATH);
    return false;
  }
  SD.remove(tempPath);
  File out = SD.open(tempPath, FILE_WRITE);
  if (!out)
  {
    setError(errorText, errorTextSize, CL_ERR_CONTEXT_CREATE);
    return false;
  }

  const TpFirmwareIntegrityStatus& firmware = tpFirmwareIntegrityStatus();
  bool ok = out.printf(CL_CONTEXT_HEADER_JSON,
                       CONTEXT_FORMAT, state.deviceSerial, state.deviceKeyId,
                       TP_FIRMWARE_VERSION_STRING, TP_FIRMWARE_BUILD_ID_STRING,
                       (unsigned long)TP_FIRMWARE_BUILD_DATE_YMD,
                       TP_FIRMWARE_TARGET_HARDWARE,
                       firmwareApprovalStatusForLog(firmware),
                       firmware.certificatePresent ? CL_TRUE : CL_FALSE,
                       tpFirmwareIntegrityVerified() ? CL_TRUE : CL_FALSE) > 0;

  char certHash[65], deviceHash[65], headHash[65], systemHash[65], firmwareHash[65], configHash[65];
  bytesToHex(state.deviceCertificateManifestSha256, 32U, certHash, sizeof(certHash));
  bytesToHex(state.deviceCalibrationManifestSha256, 32U, deviceHash, sizeof(deviceHash));
  bytesToHex(state.headCalibrationManifestSha256, 32U, headHash, sizeof(headHash));
  bytesToHex(state.systemCalibrationManifestSha256, 32U, systemHash, sizeof(systemHash));
  bytesToHex(state.firmwareManifestSha256, 32U, firmwareHash, sizeof(firmwareHash));
  bytesToHex(state.measurementConfigurationSha256, 32U, configHash, sizeof(configHash));
  if (ok) ok = out.printf(CL_CONTEXT_HASHES_JSON,
                           certHash, deviceHash, headHash, systemHash, firmwareHash, configHash) > 0;

  size_t certLength = 0U;
  if (ok && tpLogIntegrityIsCertified(state.mode))
  {
    ok = deviceIdentityBuildCertificateJson(contextScratch, sizeof(contextScratch), &certLength) &&
         out.print(CL_CONTEXT_DEVICE_CERT) &&
         out.write((const uint8_t*)contextScratch, certLength) == certLength &&
         out.print(CL_CONTEXT_DEVICE_CAL) &&
         tpSignedCalibrationCopyActiveJsonTo(out, true, errorText, errorTextSize) &&
         out.print(CL_CONTEXT_HEAD_CAL) &&
         tpSignedCalibrationCopyActiveJsonTo(out, false, errorText, errorTextSize) &&
         out.print(CL_CONTEXT_SYSTEM_CAL) &&
         tpSignedCalibrationCopyActiveSystemJsonTo(out, errorText, errorTextSize) &&
         writeSystemFirmwareContextJson(out) &&
         out.print(CL_CONTEXT_SEPARATOR);
  }
  else if (ok)
  {
    ok = out.print(CL_CONTEXT_NULLS) &&
         out.print("  \"SystemCalibrationFirmwareContext\": null,\n") &&
         out.print("  \"SystemCalibrationFirmwareApplicability\": \"NOT_APPLICABLE\",\n");
  }

  if (ok) ok = out.print(CL_CONTEXT_MEAS_CONFIG) &&
               printMeasurementConfig(out, state.measurementConfiguration) &&
               out.print(CL_CONTEXT_END);
  out.flush();
  out.close();
  memset(contextScratch, 0, sizeof(contextScratch));

  if (!ok)
  {
    SD.remove(tempPath);
    setError(errorText, errorTextSize, CL_ERR_CONTEXT_WRITE);
    return false;
  }

  uint32_t contextSize = 0U;
  if (!hashFile(tempPath, state.contextSha256, &contextSize) || contextSize < 64U)
  {
    SD.remove(tempPath);
    setError(errorText, errorTextSize, CL_ERR_CONTEXT_VERIFY);
    return false;
  }
  SD.remove(contextPath);
  if (!SD.rename(tempPath, contextPath))
  {
    SD.remove(tempPath);
    setError(errorText, errorTextSize, CL_ERR_CONTEXT_ACTIVATE);
    return false;
  }
  return true;
}

static bool FLASHMEM saveJournal(char* errorText, size_t errorTextSize)
{
  logState.magic = JOURNAL_MAGIC;
  logState.version = JOURNAL_VERSION;
  logState.crc32 = 0U;
  logState.crc32 = crc32Bytes(&logState, offsetof(TpLogJournalV2, crc32));
  SD.remove(JOURNAL_TEMP_PATH);
  File f = SD.open(JOURNAL_TEMP_PATH, FILE_WRITE);
  if (!f || f.write((const uint8_t*)&logState, sizeof(logState)) != sizeof(logState))
  {
    if (f) f.close();
    SD.remove(JOURNAL_TEMP_PATH);
    setError(errorText, errorTextSize, CL_ERR_JOURNAL_WRITE);
    return false;
  }
  f.flush();
  f.close();
  File check = SD.open(JOURNAL_TEMP_PATH, FILE_READ);
  TpLogJournalV2 verify = {};
  const bool readOk = check && check.read((uint8_t*)&verify, sizeof(verify)) == (int)sizeof(verify);
  if (check) check.close();
  if (!readOk || verify.crc32 != logState.crc32 ||
      crc32Bytes(&verify, offsetof(TpLogJournalV2, crc32)) != verify.crc32)
  {
    SD.remove(JOURNAL_TEMP_PATH);
    setError(errorText, errorTextSize, CL_ERR_JOURNAL_VERIFY);
    return false;
  }
  SD.remove(JOURNAL_BACKUP_PATH);
  const bool hadPrevious = SD.exists(JOURNAL_PATH);
  if (hadPrevious && !SD.rename(JOURNAL_PATH, JOURNAL_BACKUP_PATH))
  {
    SD.remove(JOURNAL_TEMP_PATH);
    setError(errorText, errorTextSize, CL_ERR_JOURNAL_BACKUP);
    return false;
  }
  if (!SD.rename(JOURNAL_TEMP_PATH, JOURNAL_PATH))
  {
    if (hadPrevious && SD.exists(JOURNAL_BACKUP_PATH))
      SD.rename(JOURNAL_BACKUP_PATH, JOURNAL_PATH);
    SD.remove(JOURNAL_TEMP_PATH);
    setError(errorText, errorTextSize, CL_ERR_JOURNAL_ACTIVATE);
    return false;
  }
  SD.remove(JOURNAL_BACKUP_PATH);
  return true;
}

static bool FLASHMEM loadJournalFrom(const char* path)
{
  memset(&logState, 0, sizeof(logState));
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  const bool ok = f.read((uint8_t*)&logState, sizeof(logState)) == (int)sizeof(logState) &&
                  !f.available();
  f.close();
  if (!ok || logState.magic != JOURNAL_MAGIC || logState.version != JOURNAL_VERSION ||
      logState.crc32 != crc32Bytes(&logState, offsetof(TpLogJournalV2, crc32)) ||
      (logState.mode != TP_LOG_INTEGRITY_SHA256 &&
       !tpLogIntegrityIsCertified(logState.mode)) || !safeCsvPath(logState.csvPath))
  {
    memset(&logState, 0, sizeof(logState));
    return false;
  }
  return true;
}

static bool FLASHMEM loadJournal(void)
{
  stateActive = false;
  if (loadJournalFrom(JOURNAL_PATH) ||
      loadJournalFrom(JOURNAL_BACKUP_PATH) ||
      loadJournalFrom(JOURNAL_TEMP_PATH))
  {
    stateActive = true;
    return true;
  }
  return false;
}

static bool FLASHMEM currentProvenanceMatches(void)
{
  if (!stateActive) return false;
  TpLogJournalV2 current = {};
  if (!calculateProvenance(current, tpLogIntegrityIsCertified(logState.mode))) return false;
  return bytesEqual(current.provenanceSha256, logState.provenanceSha256, 32U);
}

static bool FLASHMEM allocateCsvPath(bool diagnostic, uint8_t mode,
                                     char* output, size_t outputSize,
                                     uint16_t* sequenceOut)
{
  const uint32_t ymd = currentDateYmd();
  char stem[20];
  if (ymd != 0U)
  {
    snprintf(stem, sizeof(stem), CL_FMT_STEM_DATE,
             (unsigned long)((ymd / 10000UL) % 100UL),
             (unsigned long)((ymd / 100UL) % 100UL),
             (unsigned long)(ymd % 100UL), diagnostic ? CL_DIAG_SUFFIX : CL_EMPTY);
  }
  else
  {
    snprintf(stem, sizeof(stem), CL_FMT_STEM_FALLBACK, diagnostic ? CL_DIAG_SUFFIX : CL_EMPTY);
  }

  for (uint16_t seq = 1U; seq <= 999U; seq++)
  {
    char candidate[48];
    snprintf(candidate, sizeof(candidate), CL_FMT_SEGMENT_PATH, stem, (unsigned)seq);
    char sidecar[64] = {0};
    char container[64] = {0};
    bool pathsOk = replaceExtension(candidate, CL_EXT_TPLOG, container, sizeof(container));
    bool filesFree = pathsOk && !SD.exists(candidate) && !SD.exists(container);

    // A TPLOG-only segment removes its CSV/TPSIG intermediates. Therefore all
    // modes reserve the common stem against an already existing TPLOG so that
    // switching the selected output mode can never reuse the same sequence.
    if (mode == TP_LOG_INTEGRITY_SHA256)
    {
      pathsOk = pathsOk && snprintf(sidecar, sizeof(sidecar), CL_FMT_SHA_PATH, candidate) > 0;
      filesFree = filesFree && pathsOk && !SD.exists(sidecar);
    }
    else if (tpLogIntegrityIsCertified(mode))
    {
      pathsOk = pathsOk && replaceExtension(candidate, CL_EXT_TPSIG, sidecar, sizeof(sidecar));
      filesFree = filesFree && pathsOk && !SD.exists(sidecar);
    }
    else
    {
      pathsOk = false;
    }

    if (pathsOk && filesFree)
    {
      strncpy(output, candidate, outputSize - 1U);
      output[outputSize - 1U] = '\0';
      if (sequenceOut) *sequenceOut = seq;
      return true;
    }
  }
  return false;
}

static bool FLASHMEM createStateForPath(const char* csvPath, bool diagnostic,
                                        char* errorText, size_t errorTextSize)
{
  const uint8_t mode = interfaceSdLogIntegrityMode();
  if (mode != TP_LOG_INTEGRITY_SHA256 && !tpLogIntegrityIsCertified(mode))
  {
    setError(errorText, errorTextSize, CL_ERR_MODE);
    return false;
  }
  TpLogJournalV2 next = {};
  next.mode = mode;
  next.diagnostic = diagnostic ? 1U : 0U;
  next.dateYmd = currentDateYmd();
  next.snapshotStartUtc = currentUtc();
  next.confirmedSize = 0U;
  strncpy(next.csvPath, csvPath, sizeof(next.csvPath) - 1U);
  strncpy(next.deviceSerial, deviceSerialGet(), sizeof(next.deviceSerial) - 1U);
  strncpy(next.deviceKeyId, deviceIdentityDeviceKeyId(), sizeof(next.deviceKeyId) - 1U);
  const char* underscore = strrchr(csvPath, '_');
  if (underscore != nullptr) next.sequence = (uint16_t)atoi(underscore + 1U);
  if (!calculateProvenance(next, tpLogIntegrityIsCertified(mode)))
  {
    setError(errorText, errorTextSize,
             tpLogIntegrityIsCertified(mode)
               ? CL_ERR_EVIDENCE
               : CL_ERR_CONFIG_BIND);
    return false;
  }
  logState = next;
  stateActive = true;
  if (!writeContextFile(logState, errorText, errorTextSize) ||
      !saveJournal(errorText, errorTextSize))
  {
    char contextPath[64];
    if (replaceExtension(logState.csvPath, CL_EXT_CTX, contextPath, sizeof(contextPath))) SD.remove(contextPath);
    stateActive = false;
    memset(&logState, 0, sizeof(logState));
    return false;
  }
  return true;
}

static bool FLASHMEM recoverCompleteSize(uint32_t* recoveredSize, char* errorText, size_t errorTextSize)
{
  if (recoveredSize) *recoveredSize = 0U;
  File input = SD.open(logState.csvPath, FILE_READ);
  if (!input || input.isDirectory())
  {
    if (input) input.close();
    setError(errorText, errorTextSize, CL_ERR_RECOVER_OPEN);
    return false;
  }
  const uint32_t fileSize = (uint32_t)input.size();
  uint32_t keep = logState.confirmedSize <= fileSize ? logState.confirmedSize : 0U;
  if (fileSize > keep && input.seek(keep))
  {
    uint8_t buffer[512];
    uint32_t pos = keep;
    uint32_t lastNewline = keep;
    while (pos < fileSize)
    {
      const uint32_t remaining = fileSize - pos;
      const size_t want = remaining < sizeof(buffer) ? (size_t)remaining : sizeof(buffer);
      const int got = input.read(buffer, want);
      if (got <= 0) break;
      for (int i = 0; i < got; i++)
        if (buffer[i] == '\n') lastNewline = pos + (uint32_t)i + 1U;
      pos += (uint32_t)got;
    }
    keep = lastNewline;
    memset(buffer, 0, sizeof(buffer));
  }
  input.close();

  if (fileSize != keep)
  {
    File update = SD.open(logState.csvPath, FILE_WRITE);
    const bool ok = update && update.truncate(keep);
    if (update)
    {
      update.flush();
      update.close();
    }
    if (!ok)
    {
      setError(errorText, errorTextSize, CL_ERR_RECOVER_TRUNCATE);
      return false;
    }
  }
  logState.confirmedSize = keep;
  if (recoveredSize) *recoveredSize = keep;
  return true;
}

static bool FLASHMEM writeShaSidecar(const uint8_t csvHash[32], const char* csvPath,
                                     char* errorText, size_t errorTextSize)
{
  char path[64], temp[64], hex[65];
  if (snprintf(path, sizeof(path), CL_FMT_SHA_PATH, csvPath) <= 0 ||
      snprintf(temp, sizeof(temp), CL_FMT_SHA_TEMP, path) <= 0)
  {
    setError(errorText, errorTextSize, CL_ERR_SHA_PATH);
    return false;
  }
  bytesToHex(csvHash, 32U, hex, sizeof(hex));
  SD.remove(temp);
  File f = SD.open(temp, FILE_WRITE);
  const bool ok = f && f.printf(CL_FMT_SHA_LINE, hex, baseName(csvPath)) > 0;
  if (f) { f.flush(); f.close(); }
  if (!ok)
  {
    SD.remove(temp);
    setError(errorText, errorTextSize, CL_ERR_SHA_WRITE);
    return false;
  }
  uint8_t verifyHash[32];
  uint32_t verifySize = 0U;
  if (!hashFile(temp, verifyHash, &verifySize) || verifySize < 70U)
  {
    SD.remove(temp);
    setError(errorText, errorTextSize, CL_ERR_SHA_VERIFY);
    return false;
  }
  SD.remove(path);
  if (!SD.rename(temp, path))
  {
    SD.remove(temp);
    setError(errorText, errorTextSize, CL_ERR_SHA_ACTIVATE);
    return false;
  }
  return true;
}

static bool FLASHMEM buildLogCanonical(const uint8_t csvHash[32], uint32_t csvSize,
                                       int64_t endUtc, int64_t createdUtc, bool recovered,
                                       uint8_t manifestHash[32])
{
  uint8_t canonical[768];
  TpCanonicalWriter w(canonical, sizeof(canonical));
  const bool ok = w.writeUtf8(LOG_FORMAT_V2) && w.writeUtf8(CL_PRODUCT) &&
      w.writeUtf8(baseName(logState.csvPath)) && w.writeU64(csvSize) &&
      w.writeHash32(csvHash) && w.writeI64(logState.snapshotStartUtc) &&
      w.writeI64(endUtc) && w.writeI64(createdUtc) &&
      w.writeUtf8(logState.deviceSerial) && w.writeUtf8(logState.deviceKeyId) &&
      w.writeHash32(logState.deviceCertificateManifestSha256) &&
      w.writeHash32(logState.deviceCalibrationManifestSha256) &&
      w.writeHash32(logState.headCalibrationManifestSha256) &&
      w.writeHash32(logState.systemCalibrationManifestSha256) &&
      w.writeHash32(logState.firmwareManifestSha256) &&
      w.writeHash32(logState.measurementConfigurationSha256) &&
      w.writeHash32(logState.contextSha256) && w.writeU8(recovered ? 1U : 0U);
  if (!ok) return false;
  TpSha256::hash(canonical, w.length(), manifestHash);
  memset(canonical, 0, sizeof(canonical));
  return true;
}

static bool FLASHMEM writeTpsig(const uint8_t csvHash[32], uint32_t csvSize,
                                bool recovered, char* errorText, size_t errorTextSize)
{
  const int64_t createdUtc = currentUtc();
  const int64_t endUtc = logState.lastConfirmedUtc > 0 ? logState.lastConfirmedUtc : createdUtc;
  uint8_t manifestHash[32];
  if (!buildLogCanonical(csvHash, csvSize, endUtc, createdUtc, recovered, manifestHash))
  {
    setError(errorText, errorTextSize, CL_ERR_CANONICAL);
    return false;
  }
  uint8_t signature[64];
  if (!deviceIdentitySignHash(manifestHash, signature))
  {
    setError(errorText, errorTextSize, CL_ERR_SIGN);
    return false;
  }
  char signatureBase64[89];
  if (!base64Encode(signature, sizeof(signature), signatureBase64, sizeof(signatureBase64)))
  {
    memset(signature, 0, sizeof(signature));
    setError(errorText, errorTextSize, CL_ERR_SIGN_ENCODE);
    return false;
  }

  char path[64], temp[64], contextPath[64];
  if (!replaceExtension(logState.csvPath, CL_EXT_TPSIG, path, sizeof(path)) ||
      !replaceExtension(logState.csvPath, CL_EXT_TSG, temp, sizeof(temp)) ||
      !replaceExtension(logState.csvPath, CL_EXT_CTX, contextPath, sizeof(contextPath)))
  {
    memset(signature, 0, sizeof(signature));
    setError(errorText, errorTextSize, CL_ERR_TPSIG_PATH);
    return false;
  }

  char csvHashHex[65], certHash[65], deviceHash[65], headHash[65], systemHash[65];
  char firmwareHash[65], configHash[65], contextHash[65], manifestHashHex[65];
  bytesToHex(csvHash, 32U, csvHashHex, sizeof(csvHashHex));
  bytesToHex(logState.deviceCertificateManifestSha256, 32U, certHash, sizeof(certHash));
  bytesToHex(logState.deviceCalibrationManifestSha256, 32U, deviceHash, sizeof(deviceHash));
  bytesToHex(logState.headCalibrationManifestSha256, 32U, headHash, sizeof(headHash));
  bytesToHex(logState.systemCalibrationManifestSha256, 32U, systemHash, sizeof(systemHash));
  bytesToHex(logState.firmwareManifestSha256, 32U, firmwareHash, sizeof(firmwareHash));
  bytesToHex(logState.measurementConfigurationSha256, 32U, configHash, sizeof(configHash));
  bytesToHex(logState.contextSha256, 32U, contextHash, sizeof(contextHash));
  bytesToHex(manifestHash, 32U, manifestHashHex, sizeof(manifestHashHex));

  SD.remove(temp);
  File out = SD.open(temp, FILE_WRITE);
  bool ok = out && out.printf(CL_TPSIG_HEADER_JSON,
      LOG_FORMAT_V2, baseName(logState.csvPath), (unsigned long)csvSize, csvHashHex,
      (long long)logState.snapshotStartUtc, (long long)endUtc, (long long)createdUtc,
      logState.deviceSerial, logState.deviceKeyId, certHash, deviceHash, headHash,
      systemHash, firmwareHash, configHash, contextHash, recovered ? CL_TRUE : CL_FALSE) > 0;
  if (ok) ok = copyFileToPrint(contextPath, out);
  if (ok) ok = out.printf(CL_TPSIG_FOOTER_JSON,
      manifestHashHex, SIGNATURE_ALGORITHM, SIGNATURE_ENCODING, signatureBase64) > 0;
  if (out) { out.flush(); out.close(); }
  memset(signature, 0, sizeof(signature));
  memset(signatureBase64, 0, sizeof(signatureBase64));
  if (!ok)
  {
    SD.remove(temp);
    setError(errorText, errorTextSize, CL_ERR_TPSIG_WRITE);
    return false;
  }

  uint8_t verifyHash[32];
  uint32_t verifySize = 0U;
  if (!hashFile(temp, verifyHash, &verifySize) || verifySize < 512U)
  {
    SD.remove(temp);
    setError(errorText, errorTextSize, CL_ERR_TPSIG_VERIFY);
    return false;
  }
  SD.remove(path);
  if (!SD.rename(temp, path))
  {
    SD.remove(temp);
    setError(errorText, errorTextSize, CL_ERR_TPSIG_ACTIVATE);
    return false;
  }
  return true;
}

static bool FLASHMEM writeTpLog(const uint8_t csvHash[32], uint32_t csvSize,
                                bool recovered, char* errorText, size_t errorTextSize)
{
  char tpsigPath[64], tpLogPath[64], tempPath[64];
  if (!replaceExtension(logState.csvPath, CL_EXT_TPSIG, tpsigPath, sizeof(tpsigPath)) ||
      !replaceExtension(logState.csvPath, CL_EXT_TPLOG, tpLogPath, sizeof(tpLogPath)) ||
      !replaceExtension(logState.csvPath, CL_EXT_TPL, tempPath, sizeof(tempPath)))
  {
    setError(errorText, errorTextSize, CL_ERR_TPLOG_PATH);
    return false;
  }

  uint8_t evidenceHash[32];
  uint32_t evidenceSize = 0U;
  if (!hashFile(tpsigPath, evidenceHash, &evidenceSize) || evidenceSize < 512U)
  {
    setError(errorText, errorTextSize, CL_ERR_TPLOG_EVIDENCE);
    return false;
  }

  TpLogContainerHeaderV1 header = {};
  memcpy(header.magic, TPLOG_HEADER_MAGIC, sizeof(header.magic));
  header.version = TPLOG_VERSION;
  header.headerSize = (uint16_t)sizeof(header);
  header.flags = recovered ? TPLOG_FLAG_RECOVERED : 0U;
  header.csvOffset = sizeof(header);
  header.csvLength = csvSize;
  header.evidenceOffset = header.csvOffset + header.csvLength;
  header.evidenceLength = evidenceSize;
  memcpy(header.csvSha256, csvHash, 32U);
  memcpy(header.evidenceSha256, evidenceHash, 32U);
  header.crc32 = crc32Bytes(&header, offsetof(TpLogContainerHeaderV1, crc32));

  TpLogContainerFooterV1 footer = {};
  memcpy(footer.magic, TPLOG_FOOTER_MAGIC, sizeof(footer.magic));
  footer.version = TPLOG_VERSION;
  footer.footerSize = (uint16_t)sizeof(footer);
  footer.totalSize = header.evidenceOffset + header.evidenceLength + sizeof(footer);
  footer.headerOffset = 0U;
  footer.headerCrc32 = header.crc32;
  footer.crc32 = crc32Bytes(&footer, offsetof(TpLogContainerFooterV1, crc32));

  SD.remove(tempPath);
  File out = SD.open(tempPath, FILE_WRITE);
  bool ok = out && out.write((const uint8_t*)&header, sizeof(header)) == sizeof(header) &&
            copyFileToPrint(logState.csvPath, out) &&
            copyFileToPrint(tpsigPath, out) &&
            out.write((const uint8_t*)&footer, sizeof(footer)) == sizeof(footer);
  if (out)
  {
    out.flush();
    out.close();
  }
  if (!ok)
  {
    SD.remove(tempPath);
    setError(errorText, errorTextSize, CL_ERR_TPLOG_WRITE);
    return false;
  }

  File verify = SD.open(tempPath, FILE_READ);
  TpLogContainerHeaderV1 readHeader = {};
  TpLogContainerFooterV1 readFooter = {};
  const bool structureOk = verify && !verify.isDirectory() &&
      (uint64_t)verify.size() == footer.totalSize &&
      verify.read((uint8_t*)&readHeader, sizeof(readHeader)) == (int)sizeof(readHeader) &&
      verify.seek((uint32_t)(footer.totalSize - sizeof(readFooter))) &&
      verify.read((uint8_t*)&readFooter, sizeof(readFooter)) == (int)sizeof(readFooter);
  if (verify) verify.close();

  const bool headerOk = structureOk &&
      bytesEqual(readHeader.magic, TPLOG_HEADER_MAGIC, sizeof(readHeader.magic)) &&
      readHeader.version == TPLOG_VERSION && readHeader.headerSize == sizeof(readHeader) &&
      readHeader.csvOffset == sizeof(readHeader) && readHeader.csvLength == csvSize &&
      readHeader.evidenceOffset == readHeader.csvOffset + readHeader.csvLength &&
      readHeader.evidenceLength == evidenceSize &&
      readHeader.crc32 == crc32Bytes(&readHeader, offsetof(TpLogContainerHeaderV1, crc32));
  const bool footerOk = structureOk &&
      bytesEqual(readFooter.magic, TPLOG_FOOTER_MAGIC, sizeof(readFooter.magic)) &&
      readFooter.version == TPLOG_VERSION && readFooter.footerSize == sizeof(readFooter) &&
      readFooter.totalSize == footer.totalSize && readFooter.headerOffset == 0U &&
      readFooter.headerCrc32 == readHeader.crc32 &&
      readFooter.crc32 == crc32Bytes(&readFooter, offsetof(TpLogContainerFooterV1, crc32));

  uint8_t verifyCsvHash[32], verifyEvidenceHash[32];
  const bool payloadOk = headerOk && footerOk &&
      hashFileRange(tempPath, readHeader.csvOffset, readHeader.csvLength, verifyCsvHash) &&
      hashFileRange(tempPath, readHeader.evidenceOffset, readHeader.evidenceLength, verifyEvidenceHash) &&
      bytesEqual(verifyCsvHash, csvHash, 32U) &&
      bytesEqual(verifyCsvHash, readHeader.csvSha256, 32U) &&
      bytesEqual(verifyEvidenceHash, evidenceHash, 32U) &&
      bytesEqual(verifyEvidenceHash, readHeader.evidenceSha256, 32U);
  memset(verifyCsvHash, 0, sizeof(verifyCsvHash));
  memset(verifyEvidenceHash, 0, sizeof(verifyEvidenceHash));
  memset(evidenceHash, 0, sizeof(evidenceHash));
  if (!payloadOk)
  {
    SD.remove(tempPath);
    setError(errorText, errorTextSize, CL_ERR_TPLOG_VERIFY);
    return false;
  }

  SD.remove(tpLogPath);
  if (!SD.rename(tempPath, tpLogPath))
  {
    SD.remove(tempPath);
    setError(errorText, errorTextSize, CL_ERR_TPLOG_ACTIVATE);
    return false;
  }
  return true;
}

static void FLASHMEM clearStateFiles(void)
{
  char contextPath[64] = {0};
  if (stateActive) replaceExtension(logState.csvPath, CL_EXT_CTX, contextPath, sizeof(contextPath));

  // Der Nachweis ist zu diesem Zeitpunkt bereits vollständig geschrieben und
  // geprüft. Zuerst das Journal entfernen: Ein Stromausfall darf niemals ein
  // Journal hinterlassen, dessen eingefrorener Kontext bereits gelöscht ist.
  SD.remove(JOURNAL_PATH);
  SD.remove(JOURNAL_TEMP_PATH);
  SD.remove(JOURNAL_BACKUP_PATH);
  if (contextPath[0] != '\0') SD.remove(contextPath);
  stateActive = false;
  memset(&logState, 0, sizeof(logState));
}
}

bool FLASHMEM tpCertifiedLogBegin(char* errorText, size_t errorTextSize)
{
  if (beginDone)
  {
    setError(errorText, errorTextSize, lastError);
    return lastError[0] == '\0';
  }
  beginDone = true;
  setError(nullptr, 0U, CL_EMPTY);
  if (!SD.exists(JOURNAL_PATH) && !SD.exists(JOURNAL_BACKUP_PATH) &&
      !SD.exists(JOURNAL_TEMP_PATH)) return true;
  if (!loadJournal())
  {
    SD.remove(JOURNAL_PATH);
    SD.remove(JOURNAL_BACKUP_PATH);
    SD.remove(JOURNAL_TEMP_PATH);
    setError(errorText, errorTextSize, CL_ERR_JOURNAL_INVALID);
    return true;
  }
  if (!SD.exists(logState.csvPath))
  {
    clearStateFiles();
    setError(errorText, errorTextSize, CL_ERR_JOURNAL_ORPHAN);
    return true;
  }
  uint32_t recoveredSize = 0U;
  if (!recoverCompleteSize(&recoveredSize, errorText, errorTextSize)) return false;
  if (recoveredSize == 0U)
  {
    SD.remove(logState.csvPath);
    clearStateFiles();
    setError(errorText, errorTextSize, CL_ERR_SEGMENT_EMPTY);
    return true;
  }
  char sealed[48];
  return tpCertifiedLogFinalize(true, sealed, sizeof(sealed), errorText, errorTextSize);
}

bool FLASHMEM tpCertifiedLogGetCsvPath(bool diagnostic, char* output, size_t outputSize,
                                        char* errorText, size_t errorTextSize)
{
  if (output == nullptr || outputSize < 16U) return false;
  const uint8_t mode = interfaceSdLogIntegrityMode();
  if (mode == TP_LOG_INTEGRITY_OFF) return false;
  if (mode != TP_LOG_INTEGRITY_SHA256 && !tpLogIntegrityIsCertified(mode))
  {
    setError(errorText, errorTextSize, CL_ERR_MODE);
    return false;
  }
  if (stateActive)
  {
    if (tpCertifiedLogNeedsRotation(diagnostic))
    {
      // Rotation muss der SD-Logger vor dem nächsten Sample ausführen, damit
      // sein RAM-Puffer nicht unbemerkt in einen neuen Kontext gelangt.
      setError(errorText, errorTextSize, CL_ERR_ROTATE);
      return false;
    }
    strncpy(output, logState.csvPath, outputSize - 1U);
    output[outputSize - 1U] = '\0';
    return true;
  }
  uint16_t seq = 0U;
  if (!allocateCsvPath(diagnostic, mode, output, outputSize, &seq))
  {
    setError(errorText, errorTextSize, CL_ERR_NO_FILENAME);
    return false;
  }
  return true;
}

bool FLASHMEM tpCertifiedLogBeforeAppend(const char* csvPath, bool fileWasNew,
                                          char* errorText, size_t errorTextSize)
{
  const uint8_t mode = interfaceSdLogIntegrityMode();
  if (mode == TP_LOG_INTEGRITY_OFF) return true;
  if (!safeCsvPath(csvPath))
  {
    setError(errorText, errorTextSize, CL_ERR_CSV_PATH);
    return false;
  }
  if (stateActive)
  {
    if (strcmp(logState.csvPath, csvPath) != 0)
    {
      setError(errorText, errorTextSize, CL_ERR_CSV_ACTIVE_MISMATCH);
      return false;
    }
    return true;
  }
  if (!fileWasNew || SD.exists(csvPath))
  {
    // A segmented file without its journal must never be appended because its
    // provenance is no longer provable.
    setError(errorText, errorTextSize, CL_ERR_CSV_NO_JOURNAL);
    return false;
  }
  return createStateForPath(csvPath, interfaceSdDiagnosticDataEnabled(), errorText, errorTextSize);
}

bool FLASHMEM tpCertifiedLogAfterFlush(const char* csvPath, uint32_t confirmedSize,
                                        char* errorText, size_t errorTextSize)
{
  if (interfaceSdLogIntegrityMode() == TP_LOG_INTEGRITY_OFF) return true;
  if (!stateActive || csvPath == nullptr || strcmp(logState.csvPath, csvPath) != 0)
  {
    setError(errorText, errorTextSize, CL_ERR_JOURNAL_AFTER_FLUSH);
    return false;
  }
  logState.confirmedSize = confirmedSize;
  logState.lastConfirmedUtc = currentUtc();
  return saveJournal(errorText, errorTextSize);
}

bool FLASHMEM tpCertifiedLogNeedsRotation(bool diagnostic)
{
  if (!stateActive) return false;
  const uint8_t mode = interfaceSdLogIntegrityMode();
  if (mode != logState.mode || (diagnostic ? 1U : 0U) != logState.diagnostic) return true;
  const uint32_t ymd = currentDateYmd();
  if (ymd != logState.dateYmd) return true;
  return !currentProvenanceMatches();
}

bool FLASHMEM tpCertifiedLogFinalize(bool recoveredAfterUncleanShutdown,
                                      char* sealedOutputPath, size_t sealedOutputPathSize,
                                      char* errorText, size_t errorTextSize)
{
  if (sealedOutputPath && sealedOutputPathSize) sealedOutputPath[0] = '\0';
  if (!stateActive) return true;
  if (!SD.exists(logState.csvPath))
  {
    clearStateFiles();
    setError(errorText, errorTextSize, CL_ERR_CSV_MISSING);
    return false;
  }
  const uint8_t mode = logState.mode;
  const bool certified = tpLogIntegrityIsCertified(mode);
  const bool tpLogOnly = tpLogIntegrityUsesTpLog(mode);
  if (certified && !verifyFrozenContext(errorText, errorTextSize)) return false;

  uint8_t csvHash[32];
  uint32_t csvSize = 0U;
  if (!hashFile(logState.csvPath, csvHash, &csvSize))
  {
    setError(errorText, errorTextSize, CL_ERR_CSV_HASH);
    return false;
  }
  if (csvSize == 0U)
  {
    SD.remove(logState.csvPath);
    clearStateFiles();
    return true;
  }

  bool ok = certified
      ? writeTpsig(csvHash, csvSize, recoveredAfterUncleanShutdown, errorText, errorTextSize)
      : writeShaSidecar(csvHash, logState.csvPath, errorText, errorTextSize);
  if (!ok) return false;

  char finalPath[64] = {0};
  strncpy(finalPath, logState.csvPath, sizeof(finalPath) - 1U);
  if (tpLogOnly)
  {
    if (!writeTpLog(csvHash, csvSize, recoveredAfterUncleanShutdown, errorText, errorTextSize))
      return false;
    if (!replaceExtension(logState.csvPath, CL_EXT_TPLOG, finalPath, sizeof(finalPath)))
    {
      setError(errorText, errorTextSize, CL_ERR_TPLOG_OUTPUT);
      return false;
    }
  }

  char csvPath[64] = {0};
  char tpsigPath[64] = {0};
  strncpy(csvPath, logState.csvPath, sizeof(csvPath) - 1U);
  if (tpLogOnly && !replaceExtension(logState.csvPath, CL_EXT_TPSIG, tpsigPath, sizeof(tpsigPath)))
  {
    setError(errorText, errorTextSize, CL_ERR_TPSIG_INTERMEDIATE);
    return false;
  }

  clearStateFiles();

  // TPLOG mode exposes exactly one archival file. CSV and TPSIG are verified
  // construction intermediates and are removed only after TPLOG is complete.
  if (tpLogOnly)
  {
    SD.remove(tpsigPath);
    SD.remove(csvPath);
  }

  if (sealedOutputPath && sealedOutputPathSize)
  {
    strncpy(sealedOutputPath, finalPath, sealedOutputPathSize - 1U);
    sealedOutputPath[sealedOutputPathSize - 1U] = '\0';
  }
  setError(errorText, errorTextSize, CL_EMPTY);
  return true;
}

bool FLASHMEM tpCertifiedLogHasActiveSegment(void)
{
  return stateActive;
}

const char* FLASHMEM tpCertifiedLogActiveCsvPath(void)
{
  return stateActive ? logState.csvPath : CL_EMPTY;
}

const char* FLASHMEM tpCertifiedLogLastError(void)
{
  return lastError;
}
