/*
 * TP-3000 firmware context bound to a system-calibration request
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPsystemFirmwareContext.h"
#include "TPsignedData.h"
#include "TPfirmwareIntegrity.h"
#include "TP_T.h"
#include "TPsha256.h"
#include <Arduino.h>
#include <SD.h>
#include <string.h>
#include <strings.h>

extern bool sdLogEnsureReadyForAccess(void);

#if defined(__IMXRT1062__)
#define TP_SYSTEM_FWCTX_RODATA __attribute__((section(".progmem")))
#else
#define TP_SYSTEM_FWCTX_RODATA
#endif

static const char TP_SYSTEM_FWCTX_FORMAT[] TP_SYSTEM_FWCTX_RODATA =
    "TP3000-SYSTEM-FIRMWARE-CONTEXT-1";
static const char TP_SYSTEM_FWCTX_PRODUCT[] TP_SYSTEM_FWCTX_RODATA = "TP-3000";
static const char TP_SYSTEM_FWCTX_DIR[] TP_SYSTEM_FWCTX_RODATA =
    "/CALIBRATION/FIRMWARE_CONTEXT";
static const char TP_SYSTEM_FWCTX_PATH[] TP_SYSTEM_FWCTX_RODATA =
    "/CALIBRATION/FIRMWARE_CONTEXT/%s.tpfctx";
static const char TP_SYSTEM_FWCTX_TEMP[] TP_SYSTEM_FWCTX_RODATA =
    "/CALIBRATION/FIRMWARE_CONTEXT/CONTEXT.tmp";
static const char TP_SYSTEM_FWCTX_HEX[] TP_SYSTEM_FWCTX_RODATA =
    "0123456789abcdef";
static const uint32_t TP_SYSTEM_FWCTX_MAGIC = 0x58434654UL; // "TFCX"
static const uint16_t TP_SYSTEM_FWCTX_VERSION = 1U;

#pragma pack(push, 1)
struct TpSystemFirmwareContextRecord
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint8_t captureMode;
  uint8_t manufacturerStatus;
  uint8_t reserved[2];
  int64_t capturedUtc;
  uint32_t imageBase;
  uint32_t imageSize;
  int64_t certificateApprovedUtc;
  char deviceSerial[6];
  char deviceKeyId[17];
  char sourceRequestId[33];
  uint8_t sourceRequestManifestSha256[32];
  char firmwareVersion[24];
  char firmwareBuildId[32];
  uint8_t firmwareSha256[32];
  char firmwareCertificateId[64];
  char manufacturerRootKeyId[17];
  uint8_t manifestSha256[32];
  uint8_t deviceSignature[64];
  uint32_t crc32;
};
#pragma pack(pop)

static_assert(sizeof(TpSystemFirmwareContextRecord) == 393U,
              "Firmware context record size changed");

static DMAMEM uint8_t tpSystemFwContextCanonical[512];
static DMAMEM TpSystemFirmwareContextRecord tpSystemFwContextRecord;
static DMAMEM TpSystemFirmwareContextRecord tpSystemFwContextReadBack;

static void FLASHMEM fwctxSetText(char* target, size_t size, const char* text)
{
  if (target == nullptr || size == 0U) return;
  if (text == nullptr) text = "";
  const size_t length = strnlen(text, size - 1U);
  if (length > 0U) memcpy(target, text, length);
  target[length] = '\0';
}

static uint32_t FLASHMEM fwctxCrc32(const void* data, size_t length)
{
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0U; i < length; ++i)
  {
    crc ^= bytes[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit)
      crc = (crc >> 1U) ^ (0xEDB88320UL & (uint32_t)-(int32_t)(crc & 1U));
  }
  return ~crc;
}

static bool FLASHMEM fwctxHexTextValid(const char* text, size_t bytes)
{
  if (text == nullptr || strlen(text) != bytes * 2U) return false;
  for (size_t i = 0U; i < bytes * 2U; ++i)
  {
    const char c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
          (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

static int8_t FLASHMEM fwctxNibble(char c)
{
  if (c >= '0' && c <= '9') return (int8_t)(c - '0');
  if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
  if (c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
  return -1;
}

static bool FLASHMEM fwctxHexToBytes(const char* text, uint8_t* output,
                                     size_t outputLength)
{
  if (!fwctxHexTextValid(text, outputLength) || output == nullptr) return false;
  for (size_t i = 0U; i < outputLength; ++i)
  {
    const int8_t hi = fwctxNibble(text[i * 2U]);
    const int8_t lo = fwctxNibble(text[i * 2U + 1U]);
    if (hi < 0 || lo < 0) return false;
    output[i] = (uint8_t)(((uint8_t)hi << 4U) | (uint8_t)lo);
  }
  return true;
}

static void FLASHMEM fwctxBytesToHex(const uint8_t* bytes, size_t length,
                                     char* output, size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return;
  output[0] = '\0';
  if (bytes == nullptr || outputSize < length * 2U + 1U) return;
  for (size_t i = 0U; i < length; ++i)
  {
    output[i * 2U] = TP_SYSTEM_FWCTX_HEX[(bytes[i] >> 4U) & 0x0FU];
    output[i * 2U + 1U] = TP_SYSTEM_FWCTX_HEX[bytes[i] & 0x0FU];
  }
  output[length * 2U] = '\0';
}

static bool FLASHMEM fwctxRequestIdValid(const char* text)
{
  return fwctxHexTextValid(text, 16U);
}

static bool FLASHMEM fwctxBuildCanonical(
    const TpSystemFirmwareContextRecord& record,
    uint8_t hash[32])
{
  TpCanonicalWriter writer(tpSystemFwContextCanonical,
                           sizeof(tpSystemFwContextCanonical));
  const bool ok = writer.writeUtf8(TP_SYSTEM_FWCTX_FORMAT) &&
      writer.writeUtf8(TP_SYSTEM_FWCTX_PRODUCT) &&
      writer.writeUtf8(record.deviceSerial) &&
      writer.writeUtf8(record.deviceKeyId) &&
      writer.writeUtf8(record.sourceRequestId) &&
      writer.writeHash32(record.sourceRequestManifestSha256) &&
      writer.writeU8(record.captureMode) &&
      writer.writeI64(record.capturedUtc) &&
      writer.writeUtf8(record.firmwareVersion) &&
      writer.writeUtf8(record.firmwareBuildId) &&
      writer.writeU32(record.imageBase) &&
      writer.writeU32(record.imageSize) &&
      writer.writeHash32(record.firmwareSha256) &&
      writer.writeU8(record.manufacturerStatus) &&
      writer.writeUtf8(record.firmwareCertificateId) &&
      writer.writeUtf8(record.manufacturerRootKeyId) &&
      writer.writeI64(record.certificateApprovedUtc) &&
      writer.valid();
  if (!ok) return false;
  TpSha256::hash(tpSystemFwContextCanonical, writer.length(), hash);
  return true;
}

static bool FLASHMEM fwctxBytesEqual(const uint8_t* a, const uint8_t* b,
                                     size_t count)
{
  if (a == nullptr || b == nullptr) return false;
  uint8_t diff = 0U;
  for (size_t i = 0U; i < count; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0U;
}

static bool FLASHMEM fwctxEnsureDirectory(void)
{
  if (!sdLogEnsureReadyForAccess()) return false;
  if (!SD.exists("/CALIBRATION") && !SD.mkdir("/CALIBRATION")) return false;
  if (!SD.exists(TP_SYSTEM_FWCTX_DIR) && !SD.mkdir(TP_SYSTEM_FWCTX_DIR)) return false;
  return true;
}

static bool FLASHMEM fwctxPathForRequest(const char* requestId,
                                         char* path, size_t pathSize)
{
  if (!fwctxRequestIdValid(requestId) || path == nullptr || pathSize < 64U)
    return false;
  const int n = snprintf(path, pathSize, TP_SYSTEM_FWCTX_PATH, requestId);
  return n > 0 && (size_t)n < pathSize;
}

static bool FLASHMEM fwctxReadExact(const char* path,
                                    TpSystemFirmwareContextRecord& record)
{
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  const bool sizeOk = (uint32_t)file.size() == sizeof(record);
  const size_t read = sizeOk
      ? file.read(reinterpret_cast<uint8_t*>(&record), sizeof(record))
      : 0U;
  file.close();
  return sizeOk && read == sizeof(record);
}

static bool FLASHMEM fwctxValidateRecord(
    const TpSystemFirmwareContextRecord& record,
    const char* expectedRequestId,
    const uint8_t expectedRequestManifest[32])
{
  if (record.magic != TP_SYSTEM_FWCTX_MAGIC ||
      record.version != TP_SYSTEM_FWCTX_VERSION ||
      record.size != sizeof(record) ||
      record.captureMode < TP_SYSTEM_FW_CAPTURE_REQUEST ||
      record.captureMode > TP_SYSTEM_FW_CAPTURE_ACTIVATION_FALLBACK ||
      record.manufacturerStatus > TP_SYSTEM_FW_MANUFACTURER_INVALID ||
      record.capturedUtc <= 0 ||
      record.imageSize == 0U ||
      record.deviceSerial[sizeof(record.deviceSerial) - 1U] != '\0' ||
      record.deviceKeyId[sizeof(record.deviceKeyId) - 1U] != '\0' ||
      record.sourceRequestId[sizeof(record.sourceRequestId) - 1U] != '\0' ||
      record.firmwareVersion[sizeof(record.firmwareVersion) - 1U] != '\0' ||
      record.firmwareBuildId[sizeof(record.firmwareBuildId) - 1U] != '\0' ||
      record.firmwareCertificateId[sizeof(record.firmwareCertificateId) - 1U] != '\0' ||
      record.manufacturerRootKeyId[sizeof(record.manufacturerRootKeyId) - 1U] != '\0')
    return false;

  if (record.crc32 != fwctxCrc32(&record, offsetof(TpSystemFirmwareContextRecord, crc32)))
    return false;
  if (expectedRequestId != nullptr &&
      strcasecmp(record.sourceRequestId, expectedRequestId) != 0)
    return false;
  if (expectedRequestManifest != nullptr &&
      !fwctxBytesEqual(record.sourceRequestManifestSha256,
                       expectedRequestManifest, 32U))
    return false;
  if (!deviceIdentityCertificateValid() ||
      strcmp(record.deviceSerial, deviceIdentityCertifiedSerial()) != 0 ||
      strcasecmp(record.deviceKeyId, deviceIdentityDeviceKeyId()) != 0)
    return false;

  uint8_t calculated[32];
  if (!fwctxBuildCanonical(record, calculated) ||
      !fwctxBytesEqual(calculated, record.manifestSha256, 32U) ||
      !deviceIdentityVerifyDeviceSignedHash(calculated, record.deviceSignature))
    return false;
  return true;
}

static bool FLASHMEM fwctxWriteRecord(
    const TpSystemFirmwareContextRecord& record,
    char* errorText, size_t errorTextSize)
{
  if (!fwctxEnsureDirectory())
  {
    fwctxSetText(errorText, errorTextSize,
                 "SD-Verzeichnis fuer Firmwarekontext nicht verfuegbar");
    return false;
  }

  char targetPath[128];
  if (!fwctxPathForRequest(record.sourceRequestId,
                           targetPath, sizeof(targetPath)))
  {
    fwctxSetText(errorText, errorTextSize,
                 "Anfrage-ID fuer Firmwarekontext ist ungueltig");
    return false;
  }

  if (SD.exists(TP_SYSTEM_FWCTX_TEMP)) SD.remove(TP_SYSTEM_FWCTX_TEMP);
  File file = SD.open(TP_SYSTEM_FWCTX_TEMP, FILE_WRITE);
  if (!file)
  {
    fwctxSetText(errorText, errorTextSize,
                 "Firmwarekontext konnte nicht temporaer angelegt werden");
    return false;
  }
  const size_t written = file.write(
      reinterpret_cast<const uint8_t*>(&record), sizeof(record));
  file.flush();
  file.close();
  if (written != sizeof(record) ||
      !fwctxReadExact(TP_SYSTEM_FWCTX_TEMP, tpSystemFwContextReadBack) ||
      memcmp(&record, &tpSystemFwContextReadBack, sizeof(record)) != 0)
  {
    SD.remove(TP_SYSTEM_FWCTX_TEMP);
    fwctxSetText(errorText, errorTextSize,
                 "Firmwarekontext wurde nicht vollstaendig auf SD geschrieben");
    return false;
  }

  if (SD.exists(targetPath) && !SD.remove(targetPath))
  {
    SD.remove(TP_SYSTEM_FWCTX_TEMP);
    fwctxSetText(errorText, errorTextSize,
                 "Alter Firmwarekontext konnte nicht ersetzt werden");
    return false;
  }
  if (!SD.rename(TP_SYSTEM_FWCTX_TEMP, targetPath) ||
      !fwctxReadExact(targetPath, tpSystemFwContextReadBack) ||
      memcmp(&record, &tpSystemFwContextReadBack, sizeof(record)) != 0)
  {
    SD.remove(TP_SYSTEM_FWCTX_TEMP);
    fwctxSetText(errorText, errorTextSize,
                 "Firmwarekontext konnte nicht sicher aktiviert werden");
    return false;
  }
  fwctxSetText(errorText, errorTextSize, "OK");
  return true;
}

static TpSystemFirmwareManufacturerStatus FLASHMEM fwctxManufacturerStatus(
    const TpFirmwareIntegrityStatus& fw)
{
  if (fw.state == TP_FW_INTEGRITY_VERIFIED &&
      fw.certificateSignatureValid && fw.certificateMatchesImage)
    return TP_SYSTEM_FW_MANUFACTURER_VALID;
  if (!fw.certificatePresent)
    return TP_SYSTEM_FW_MANUFACTURER_NOT_CERTIFIED;
  if (fw.certificateSignatureValid && !fw.certificateMatchesImage)
    return TP_SYSTEM_FW_MANUFACTURER_MISMATCH;
  return TP_SYSTEM_FW_MANUFACTURER_INVALID;
}

static bool FLASHMEM fwctxCapture(
    const char* sourceRequestId,
    const uint8_t sourceRequestManifestSha256[32],
    TpSystemFirmwareCaptureMode captureMode,
    int64_t capturedUtc,
    char* errorText,
    size_t errorTextSize)
{
  if (!fwctxRequestIdValid(sourceRequestId) ||
      sourceRequestManifestSha256 == nullptr || capturedUtc <= 0)
  {
    fwctxSetText(errorText, errorTextSize,
                 "Firmwarekontext hat keine gueltige Anfragebindung");
    return false;
  }
  if (!deviceIdentityCertificateValid())
  {
    fwctxSetText(errorText, errorTextSize,
                 "Firmwarekontext erfordert ein gueltiges Geraetezertifikat");
    return false;
  }

  const TpFirmwareIntegrityStatus& fw = tpFirmwareIntegrityStatus();
  if (!fw.hashCalculated)
  {
    fwctxSetText(errorText, errorTextSize,
                 "Firmware-SHA-256 ist noch nicht berechnet; sauber neu starten");
    return false;
  }

  memset(&tpSystemFwContextRecord, 0, sizeof(tpSystemFwContextRecord));
  tpSystemFwContextRecord.magic = TP_SYSTEM_FWCTX_MAGIC;
  tpSystemFwContextRecord.version = TP_SYSTEM_FWCTX_VERSION;
  tpSystemFwContextRecord.size = sizeof(tpSystemFwContextRecord);
  tpSystemFwContextRecord.captureMode = (uint8_t)captureMode;
  tpSystemFwContextRecord.manufacturerStatus =
      (uint8_t)fwctxManufacturerStatus(fw);
  tpSystemFwContextRecord.capturedUtc = capturedUtc;
  tpSystemFwContextRecord.imageBase = fw.imageBase;
  tpSystemFwContextRecord.imageSize = fw.measuredImageSize;
  tpSystemFwContextRecord.certificateApprovedUtc = fw.approvedUtc;
  fwctxSetText(tpSystemFwContextRecord.deviceSerial,
               sizeof(tpSystemFwContextRecord.deviceSerial),
               deviceIdentityCertifiedSerial());
  fwctxSetText(tpSystemFwContextRecord.deviceKeyId,
               sizeof(tpSystemFwContextRecord.deviceKeyId),
               deviceIdentityDeviceKeyId());
  fwctxSetText(tpSystemFwContextRecord.sourceRequestId,
               sizeof(tpSystemFwContextRecord.sourceRequestId),
               sourceRequestId);
  memcpy(tpSystemFwContextRecord.sourceRequestManifestSha256,
         sourceRequestManifestSha256, 32U);
  fwctxSetText(tpSystemFwContextRecord.firmwareVersion,
               sizeof(tpSystemFwContextRecord.firmwareVersion),
               TP_FIRMWARE_VERSION_STRING);
  fwctxSetText(tpSystemFwContextRecord.firmwareBuildId,
               sizeof(tpSystemFwContextRecord.firmwareBuildId),
               TP_FIRMWARE_BUILD_ID_STRING);
  memcpy(tpSystemFwContextRecord.firmwareSha256, fw.measuredSha256, 32U);
  fwctxSetText(tpSystemFwContextRecord.firmwareCertificateId,
               sizeof(tpSystemFwContextRecord.firmwareCertificateId),
               fw.firmwareCertificateId);
  fwctxSetText(tpSystemFwContextRecord.manufacturerRootKeyId,
               sizeof(tpSystemFwContextRecord.manufacturerRootKeyId),
               fw.rootKeyId);

  if (!fwctxBuildCanonical(tpSystemFwContextRecord,
                           tpSystemFwContextRecord.manifestSha256) ||
      !deviceIdentitySignHash(tpSystemFwContextRecord.manifestSha256,
                              tpSystemFwContextRecord.deviceSignature))
  {
    fwctxSetText(errorText, errorTextSize,
                 "Geraetesignatur des Firmwarekontexts fehlgeschlagen");
    return false;
  }
  tpSystemFwContextRecord.crc32 = fwctxCrc32(
      &tpSystemFwContextRecord,
      offsetof(TpSystemFirmwareContextRecord, crc32));
  return fwctxWriteRecord(tpSystemFwContextRecord,
                          errorText, errorTextSize);
}

bool FLASHMEM tpSystemFirmwareContextCaptureRequest(
    const char* sourceRequestId,
    const uint8_t sourceRequestManifestSha256[32],
    int64_t capturedUtc,
    char* errorText,
    size_t errorTextSize)
{
  return fwctxCapture(sourceRequestId, sourceRequestManifestSha256,
                      TP_SYSTEM_FW_CAPTURE_REQUEST, capturedUtc,
                      errorText, errorTextSize);
}

bool FLASHMEM tpSystemFirmwareContextLoad(
    const char* sourceRequestId,
    const char* sourceRequestManifestSha256,
    TpSystemFirmwareContextStatus& status)
{
  memset(&status, 0, sizeof(status));
  if (!fwctxRequestIdValid(sourceRequestId) ||
      !fwctxHexTextValid(sourceRequestManifestSha256, 32U))
  {
    fwctxSetText(status.lastError, sizeof(status.lastError),
                 "Anfragebindung des Firmwarekontexts ist ungueltig");
    return false;
  }

  uint8_t expectedManifest[32];
  if (!fwctxHexToBytes(sourceRequestManifestSha256,
                       expectedManifest, sizeof(expectedManifest)))
  {
    fwctxSetText(status.lastError, sizeof(status.lastError),
                 "Anfrage-Manifest des Firmwarekontexts ist ungueltig");
    return false;
  }

  char path[128];
  if (!fwctxPathForRequest(sourceRequestId, path, sizeof(path)) ||
      !sdLogEnsureReadyForAccess())
  {
    fwctxSetText(status.lastError, sizeof(status.lastError),
                 "Firmwarekontext-Verzeichnis ist nicht verfuegbar");
    return false;
  }
  fwctxSetText(status.sourceFile, sizeof(status.sourceFile), path);
  status.filePresent = SD.exists(path);
  if (!status.filePresent)
  {
    fwctxSetText(status.lastError, sizeof(status.lastError),
                 "Kein historischer Firmwarekontext gespeichert");
    return false;
  }

  if (!fwctxReadExact(path, tpSystemFwContextReadBack) ||
      !fwctxValidateRecord(tpSystemFwContextReadBack,
                           sourceRequestId, expectedManifest))
  {
    fwctxSetText(status.lastError, sizeof(status.lastError),
                 "Firmwarekontext wurde veraendert oder ist ungueltig");
    return false;
  }

  const TpSystemFirmwareContextRecord& record = tpSystemFwContextReadBack;
  status.recordValid = true;
  status.requestBindingValid = true;
  status.captureMode = (TpSystemFirmwareCaptureMode)record.captureMode;
  status.manufacturerStatus =
      (TpSystemFirmwareManufacturerStatus)record.manufacturerStatus;
  status.capturedUtc = record.capturedUtc;
  status.certificateApprovedUtc = record.certificateApprovedUtc;
  status.imageBase = record.imageBase;
  status.imageSize = record.imageSize;
  fwctxSetText(status.sourceRequestId, sizeof(status.sourceRequestId),
               record.sourceRequestId);
  fwctxBytesToHex(record.sourceRequestManifestSha256, 32U,
                  status.sourceRequestManifestSha256,
                  sizeof(status.sourceRequestManifestSha256));
  fwctxSetText(status.firmwareVersion, sizeof(status.firmwareVersion),
               record.firmwareVersion);
  fwctxSetText(status.firmwareBuildId, sizeof(status.firmwareBuildId),
               record.firmwareBuildId);
  fwctxBytesToHex(record.firmwareSha256, 32U,
                  status.firmwareSha256, sizeof(status.firmwareSha256));
  fwctxSetText(status.firmwareCertificateId,
               sizeof(status.firmwareCertificateId),
               record.firmwareCertificateId);
  fwctxSetText(status.manufacturerRootKeyId,
               sizeof(status.manufacturerRootKeyId),
               record.manufacturerRootKeyId);
  fwctxBytesToHex(record.manifestSha256, 32U,
                  status.contextManifestSha256,
                  sizeof(status.contextManifestSha256));
  fwctxBytesToHex(record.deviceSignature, 64U,
                  status.deviceSignatureHex,
                  sizeof(status.deviceSignatureHex));

  uint8_t currentHash[32];
  status.currentHashAvailable = tpFirmwareIntegrityGetMeasuredHash(currentHash);
  status.currentImageMatches = status.currentHashAvailable &&
      strcmp(record.firmwareVersion, TP_FIRMWARE_VERSION_STRING) == 0 &&
      strcmp(record.firmwareBuildId, TP_FIRMWARE_BUILD_ID_STRING) == 0 &&
      fwctxBytesEqual(record.firmwareSha256, currentHash, 32U);
  status.applicability = !status.currentHashAvailable
      ? TP_SYSTEM_FW_APPLICABILITY_NOT_COMPARABLE
      : (status.currentImageMatches
          ? TP_SYSTEM_FW_APPLICABILITY_EXACT_MATCH
          : TP_SYSTEM_FW_APPLICABILITY_CHANGED);
  fwctxSetText(status.lastError, sizeof(status.lastError), "");
  return true;
}

bool FLASHMEM tpSystemFirmwareContextEnsureForActivation(
    const char* sourceRequestId,
    const char* sourceRequestManifestSha256,
    const char* packageFirmwareVersion,
    const char* packageFirmwareBuildId,
    int64_t capturedUtc)
{
  TpSystemFirmwareContextStatus status;
  if (tpSystemFirmwareContextLoad(sourceRequestId,
                                  sourceRequestManifestSha256, status))
    return true;
  if (packageFirmwareVersion == nullptr || packageFirmwareBuildId == nullptr ||
      strcmp(packageFirmwareVersion, TP_FIRMWARE_VERSION_STRING) != 0 ||
      strcmp(packageFirmwareBuildId, TP_FIRMWARE_BUILD_ID_STRING) != 0)
    return false;
  uint8_t requestManifest[32];
  if (!fwctxHexToBytes(sourceRequestManifestSha256,
                       requestManifest, sizeof(requestManifest)))
    return false;
  char ignored[160];
  return fwctxCapture(sourceRequestId, requestManifest,
                      TP_SYSTEM_FW_CAPTURE_ACTIVATION_FALLBACK,
                      capturedUtc, ignored, sizeof(ignored));
}
