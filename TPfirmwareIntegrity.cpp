/*
 * TP-3000 firmware image integrity and root-signed firmware certificate
 *
 * The exact linked Teensy 4.1 image is hashed at every boot. The expected
 * image identity is not embedded in the same image; it is imported as a
 * manufacturer-root-signed TP3000-FIRMWARE-CERTIFICATE-1 file. This removes
 * the post-link HEX patching step and validates the bytes that are actually
 * executing on the device.
 *
 * Without Secure Boot a deliberately modified firmware can still bypass its
 * own check. The mechanism is intended to detect damaged flash, wrong builds
 * and ordinary manipulation in the current TP-3000 security profile.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPfirmwareIntegrity.h"
#include "TPsha256.h"
#include "TPsignedData.h"
#include "TPsharedScratch.h"
#include <Arduino.h>
#include <SD.h>
#include <TimeLib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__IMXRT1062__)
#define TP_FWINT_RODATA __attribute__((section(".progmem")))
#else
#define TP_FWINT_RODATA
#endif

extern bool sdLogEnsureReadyForAccess(void);
extern const char* deviceSerialGet(void);
extern int64_t tpCurrentUtcUnixTime(void);
extern bool deviceIdentityCertificateValid(void);
extern const char* deviceIdentityCertifiedSerial(void);
extern const char* deviceIdentityDeviceKeyId(void);
extern const char* deviceIdentityCertificateSerialText(void);
extern const char* deviceIdentityRootKeyId(void);
extern bool deviceIdentityCertificateManifestHash(uint8_t output[32]);
extern bool deviceIdentityBuildCertificateJson(char* output, size_t outputSize,
                                               size_t* outputLength);
extern bool deviceIdentitySignHash(const uint8_t hash[32], uint8_t signature[64]);
extern bool deviceIdentityRandomBytes(uint8_t* output, size_t outputLength);
extern bool deviceIdentityVerifyRootSignedHash(const uint8_t hash[32],
                                               const uint8_t signature[64]);
extern void tpFirmwareIntegrityBootService(void);

extern "C" unsigned long _flashimagelen;

namespace
{
static const uint32_t TP_FW_IMAGE_BASE = 0x60000000UL;
static const uint32_t TP_FW_IMAGE_MAX = 7936UL * 1024UL;
static const size_t TP_FW_CERT_MAX_JSON = 12288U;
static const size_t TP_FW_CANONICAL_MAX = 2048U;
static const int64_t TP_FW_REQUEST_VALIDITY_SECONDS = 72LL * 60LL * 60LL;

static const char REQUEST_FORMAT[] TP_FWINT_RODATA = TP_FORMAT_FIRMWARE_REQUEST;
static const char CERTIFICATE_FORMAT[] TP_FWINT_RODATA = TP_FORMAT_FIRMWARE_CERTIFICATE;
static const char PRODUCT[] TP_FWINT_RODATA = "TP-3000";
static const char SIGNATURE_ALGORITHM[] TP_FWINT_RODATA = "ECDSA-P256-SHA256";
static const char SIGNATURE_ENCODING[] TP_FWINT_RODATA = "IEEE-P1363";
static const char CERT_DIR[] TP_FWINT_RODATA = "/CERTIFICATION";
static const char REQUEST_DIR[] TP_FWINT_RODATA = "/CERTIFICATION/REQUESTS";
static const char CERT_PATH[] TP_FWINT_RODATA = "/CERTIFICATION/FIRMWARE.TPFCERT";
static const char CERT_TEMP[] TP_FWINT_RODATA = "/CERTIFICATION/FIRMWARE.TMP";
static const char CERT_BACKUP[] TP_FWINT_RODATA = "/CERTIFICATION/FIRMWARE.BAK";
static const char STATUS_PATH[] TP_FWINT_RODATA = "/CERTIFICATION/FIRMWARE.TPS";
static const char STATUS_TEMP[] TP_FWINT_RODATA = "/CERTIFICATION/FIRMWARE_STATUS.TMP";
static const char STATUS_BACKUP[] TP_FWINT_RODATA = "/CERTIFICATION/FIRMWARE_STATUS.BAK";
static const char STATUS_FILE_FORMAT[] TP_FWINT_RODATA = "TP3000-FIRMWARE-STATUS-2";
static const char HEX_LOWER[] TP_FWINT_RODATA = "0123456789abcdef";
static const char BASE64_ALPHABET[] TP_FWINT_RODATA =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char STATUS_NOT_RUN[] TP_FWINT_RODATA = "Noch nicht geprüft";
static const char STATUS_CERT_MISSING[] TP_FWINT_RODATA =
  "Hash berechnet – Firmwarezertifikat fehlt";
static const char STATUS_VERIFIED[] TP_FWINT_RODATA =
  "Firmwarezertifikat gültig – Integrität bestätigt";
static const char STATUS_MISMATCH[] TP_FWINT_RODATA =
  "Firmwarezertifikat gehoert zu einem anderen Firmwareabbild";
static const char STATUS_CERT_INVALID[] TP_FWINT_RODATA =
  "Firmwarezertifikat ungültig";
static const char STATUS_HASH_ERROR[] TP_FWINT_RODATA =
  "Firmwareabbild konnte nicht geprüft werden";
static const char STATUS_BOOT_CRASH_SKIPPED[] TP_FWINT_RODATA =
  "Hash im Diagnose-Wiederanlauf übersprungen – siehe SD";

struct FirmwareCertificateParsed
{
  char firmwareVersion[24];
  char firmwareBuildId[32];
  uint32_t buildDateYmd;
  char targetHardware[32];
  uint32_t imageBase;
  uint32_t imageSize;
  uint8_t imageSha256[32];
  int64_t approvedUtc;
  char certificateId[64];
  char rootKeyId[17];
  uint8_t manifestSha256[32];
  uint8_t rootSignature[64];
};

static DMAMEM TpFirmwareIntegrityStatus fwStatus;
static DMAMEM uint8_t canonicalBuffer[TP_FW_CANONICAL_MAX];
static DMAMEM char jsonScratch[TP_FW_CERT_MAX_JSON];
static DMAMEM char jsonVerifyScratch[TP_FW_CERT_MAX_JSON];
static DMAMEM char statusFileBuffer[2048];
static DMAMEM FirmwareCertificateParsed parsedCertificate;

static void FLASHMEM copyText(char* target, size_t targetSize, const char* source)
{
  if (target == nullptr || targetSize == 0U) return;
  if (source == nullptr) source = "";
  const size_t length = strnlen(source, targetSize - 1U);
  memmove(target, source, length);
  target[length] = '\0';
}

static void FLASHMEM setError(char* target, size_t targetSize, const char* source)
{
  copyText(target, targetSize, source);
}

static bool FLASHMEM bytesEqual(const uint8_t* a, const uint8_t* b, size_t count)
{
  if (a == nullptr || b == nullptr) return false;
  uint8_t difference = 0U;
  for (size_t i = 0U; i < count; ++i) difference |= (uint8_t)(a[i] ^ b[i]);
  return difference == 0U;
}

static char FLASHMEM hexDigit(uint8_t value)
{
  return HEX_LOWER[value & 0x0FU];
}

static void FLASHMEM bytesToHex(const uint8_t* data, size_t length,
                                char* output, size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return;
  if (data == nullptr || outputSize < length * 2U + 1U)
  {
    output[0] = '\0';
    return;
  }
  for (size_t i = 0U; i < length; ++i)
  {
    output[i * 2U] = hexDigit((uint8_t)(data[i] >> 4U));
    output[i * 2U + 1U] = hexDigit(data[i]);
  }
  output[length * 2U] = '\0';
}

static int8_t FLASHMEM hexValue(char c)
{
  if (c >= '0' && c <= '9') return (int8_t)(c - '0');
  if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
  if (c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
  return -1;
}

static bool FLASHMEM hexToBytes(const char* text, uint8_t* output, size_t outputLength)
{
  if (text == nullptr || output == nullptr || strlen(text) != outputLength * 2U)
    return false;
  for (size_t i = 0U; i < outputLength; ++i)
  {
    const int8_t high = hexValue(text[i * 2U]);
    const int8_t low = hexValue(text[i * 2U + 1U]);
    if (high < 0 || low < 0) return false;
    output[i] = (uint8_t)(((uint8_t)high << 4U) | (uint8_t)low);
  }
  return true;
}

static size_t FLASHMEM base64EncodedLength(size_t inputLength)
{
  return ((inputLength + 2U) / 3U) * 4U;
}

static bool FLASHMEM base64Encode(const uint8_t* input, size_t inputLength,
                                  char* output, size_t outputSize)
{
  const size_t required = base64EncodedLength(inputLength);
  if (input == nullptr || output == nullptr || outputSize < required + 1U) return false;
  size_t in = 0U;
  size_t out = 0U;
  while (in < inputLength)
  {
    const uint32_t a = input[in++];
    const bool haveB = in < inputLength;
    const uint32_t b = haveB ? input[in++] : 0U;
    const bool haveC = in < inputLength;
    const uint32_t c = haveC ? input[in++] : 0U;
    const uint32_t triple = (a << 16U) | (b << 8U) | c;
    output[out++] = BASE64_ALPHABET[(triple >> 18U) & 0x3FU];
    output[out++] = BASE64_ALPHABET[(triple >> 12U) & 0x3FU];
    output[out++] = haveB ? BASE64_ALPHABET[(triple >> 6U) & 0x3FU] : '=';
    output[out++] = haveC ? BASE64_ALPHABET[triple & 0x3FU] : '=';
  }
  output[out] = '\0';
  return true;
}

static int8_t FLASHMEM base64Value(char c)
{
  if (c >= 'A' && c <= 'Z') return (int8_t)(c - 'A');
  if (c >= 'a' && c <= 'z') return (int8_t)(c - 'a' + 26);
  if (c >= '0' && c <= '9') return (int8_t)(c - '0' + 52);
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static bool FLASHMEM base64Decode(const char* input, uint8_t* output,
                                  size_t outputSize, size_t* outputLength)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (input == nullptr || output == nullptr) return false;
  const size_t length = strlen(input);
  if (length == 0U || (length & 3U) != 0U) return false;
  size_t out = 0U;
  for (size_t i = 0U; i < length; i += 4U)
  {
    const int8_t a = base64Value(input[i]);
    const int8_t b = base64Value(input[i + 1U]);
    const bool padC = input[i + 2U] == '=';
    const bool padD = input[i + 3U] == '=';
    const int8_t c = padC ? 0 : base64Value(input[i + 2U]);
    const int8_t d = padD ? 0 : base64Value(input[i + 3U]);
    if (a < 0 || b < 0 || c < 0 || d < 0 || (padC && !padD) ||
        ((padC || padD) && i + 4U != length)) return false;
    const uint32_t triple = ((uint32_t)a << 18U) | ((uint32_t)b << 12U) |
                            ((uint32_t)c << 6U) | (uint32_t)d;
    if (out >= outputSize) return false;
    output[out++] = (uint8_t)(triple >> 16U);
    if (!padC)
    {
      if (out >= outputSize) return false;
      output[out++] = (uint8_t)(triple >> 8U);
    }
    if (!padD)
    {
      if (out >= outputSize) return false;
      output[out++] = (uint8_t)triple;
    }
  }
  if (outputLength != nullptr) *outputLength = out;
  return true;
}

static bool FLASHMEM calculateImageHash(uint32_t imageSize, uint8_t digest[32])
{
#if !defined(__IMXRT1062__)
  (void)imageSize;
  memset(digest, 0, 32U);
  return false;
#else
  if (imageSize < 8192U || imageSize > TP_FW_IMAGE_MAX) return false;
  TpSha256 sha;
  uint32_t offset = 0U;
  while (offset < imageSize)
  {
    const uint32_t remaining = imageSize - offset;
    const uint32_t count = remaining < TP_SHARED_SHA256_IO_BUFFER_SIZE
                         ? remaining : (uint32_t)TP_SHARED_SHA256_IO_BUFFER_SIZE;
    const uint8_t* source = reinterpret_cast<const uint8_t*>(TP_FW_IMAGE_BASE + offset);
    memcpy(tpSharedSha256IoBuffer, source, count);
    sha.update(tpSharedSha256IoBuffer, count);
    offset += count;
    // Breadcrumb 2 enthaelt den zuletzt vollstaendig gelesenen Flash-Offset.
    // Bei einem MPU-/Bus-Fault zeigt der naechste CrashReport damit, bis zu
    // welchem 4-KiB-Block die Hashpruefung gekommen ist.
    CrashReport.breadcrumb(2, offset);
    // 4-KiB-Schritte halten den Boot-Watchdog auch dann sicher am Leben,
    // wenn ein vorheriger Watchdog-Reset den WDT bereits vor setup() aktiv liess.
    tpFirmwareIntegrityBootService();
    if ((offset & 0xFFFFUL) == 0U) yield();
  }
  sha.final(digest);
  memset(tpSharedSha256IoBuffer, 0, TP_SHARED_SHA256_IO_BUFFER_SIZE);
  tpFirmwareIntegrityBootService();
  return true;
#endif
}

static void FLASHMEM formatIsoUtc(int64_t unixTime, char* output, size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return;
  const time_t t = unixTime > 0 ? (time_t)unixTime : (time_t)0;
  snprintf(output, outputSize, "%04d-%02d-%02dT%02d:%02d:%02dZ",
           year(t), month(t), day(t), hour(t), minute(t), second(t));
}

static bool FLASHMEM parseIsoUtc(const char* text, int64_t* unixTime)
{
  if (text == nullptr || unixTime == nullptr) return false;
  const size_t textLength = strlen(text);
  const bool utcZulu = textLength == 20U && text[19] == 'Z';
  const bool utcOffset = textLength == 25U && strcmp(text + 19, "+00:00") == 0;
  if (!utcZulu && !utcOffset) return false;
  if (text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
      text[13] != ':' || text[16] != ':') return false;
  const uint8_t positions[] = {0,1,2,3,5,6,8,9,11,12,14,15,17,18};
  for (uint8_t i = 0U; i < sizeof(positions); ++i)
    if (text[positions[i]] < '0' || text[positions[i]] > '9') return false;
  const int y = (text[0]-'0')*1000 + (text[1]-'0')*100 + (text[2]-'0')*10 + (text[3]-'0');
  const int mo = (text[5]-'0')*10 + (text[6]-'0');
  const int d = (text[8]-'0')*10 + (text[9]-'0');
  const int h = (text[11]-'0')*10 + (text[12]-'0');
  const int mi = (text[14]-'0')*10 + (text[15]-'0');
  const int s = (text[17]-'0')*10 + (text[18]-'0');
  if (y < 2020 || y > 2199 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
      h > 23 || mi > 59 || s > 59) return false;
  tmElements_t tm;
  memset(&tm, 0, sizeof(tm));
  tm.Year = CalendarYrToTm(y);
  tm.Month = (uint8_t)mo;
  tm.Day = (uint8_t)d;
  tm.Hour = (uint8_t)h;
  tm.Minute = (uint8_t)mi;
  tm.Second = (uint8_t)s;
  *unixTime = (int64_t)makeTime(tm);
  return true;
}

static int FLASHMEM jsonHexNibble(char c)
{
  return hexValue(c);
}

static const char* FLASHMEM jsonFindRootValue(const char* json, const char* key)
{
  if (json == nullptr || key == nullptr) return nullptr;
  bool inString = false;
  bool escaped = false;
  int depth = 0;
  const size_t keyLength = strlen(key);
  for (const char* p = json; *p != '\0'; ++p)
  {
    const char c = *p;
    if (inString)
    {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"')
    {
      if (depth == 1 && strncmp(p + 1, key, keyLength) == 0 && p[1 + keyLength] == '"')
      {
        const char* q = p + 2 + keyLength;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') ++q;
        if (*q++ != ':') continue;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') ++q;
        return q;
      }
      inString = true;
    }
    else if (c == '{') ++depth;
    else if (c == '}') --depth;
  }
  return nullptr;
}

static bool FLASHMEM jsonGetRootString(const char* json, const char* key,
                                       char* output, size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return false;
  const char* p = jsonFindRootValue(json, key);
  if (p == nullptr || *p++ != '"') return false;
  size_t out = 0U;
  while (*p != '\0' && *p != '"')
  {
    char c = *p++;
    if (c == '\\')
    {
      c = *p++;
      switch (c)
      {
        case '"': c = '"'; break;
        case '\\': c = '\\'; break;
        case '/': c = '/'; break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'u':
        {
          int value = 0;
          for (uint8_t i = 0U; i < 4U; ++i)
          {
            const int nibble = jsonHexNibble(*p++);
            if (nibble < 0) return false;
            value = (value << 4) | nibble;
          }
          if (value < 0 || value > 0x7F) return false;
          c = (char)value;
          break;
        }
        default: return false;
      }
    }
    if (out + 1U >= outputSize) return false;
    output[out++] = c;
  }
  if (*p != '"') return false;
  output[out] = '\0';
  return true;
}

static bool FLASHMEM jsonGetRootUInt32(const char* json, const char* key,
                                       uint32_t* value)
{
  if (value == nullptr) return false;
  const char* p = jsonFindRootValue(json, key);
  if (p == nullptr || *p < '0' || *p > '9') return false;
  char* end = nullptr;
  const unsigned long parsed = strtoul(p, &end, 10);
  if (end == p) return false;
  *value = (uint32_t)parsed;
  return true;
}

static bool FLASHMEM buildRequestCanonical(const char* deviceSerial,
                                           const char* deviceKeyId,
                                           const char* deviceCertificateSerial,
                                           const char* requestId,
                                           int64_t createdUtc,
                                           int64_t expiresUtc,
                                           const uint8_t imageHash[32],
                                           const uint8_t deviceCertManifest[32],
                                           size_t* outputLength)
{
  TpCanonicalWriter writer(canonicalBuffer, sizeof(canonicalBuffer));
  const bool ok = writer.writeUtf8(REQUEST_FORMAT) &&
                  writer.writeUtf8(PRODUCT) &&
                  writer.writeUtf8(deviceSerial) &&
                  writer.writeUtf8(deviceKeyId) &&
                  writer.writeUtf8(deviceCertificateSerial) &&
                  writer.writeUtf8(requestId) &&
                  writer.writeI64(createdUtc) &&
                  writer.writeI64(expiresUtc) &&
                  writer.writeUtf8(TP_FIRMWARE_VERSION_STRING) &&
                  writer.writeUtf8(TP_FIRMWARE_BUILD_ID_STRING) &&
                  writer.writeU32(TP_FIRMWARE_BUILD_DATE_YMD) &&
                  writer.writeUtf8(TP_FIRMWARE_TARGET_HARDWARE) &&
                  writer.writeU32(TP_FW_IMAGE_BASE) &&
                  writer.writeU32(fwStatus.measuredImageSize) &&
                  writer.writeHash32(imageHash) &&
                  writer.writeHash32(deviceCertManifest);
  if (outputLength != nullptr) *outputLength = writer.length();
  return ok && writer.valid();
}

static bool FLASHMEM buildCertificateCanonical(const FirmwareCertificateParsed& certificate,
                                               size_t* outputLength)
{
  TpCanonicalWriter writer(canonicalBuffer, sizeof(canonicalBuffer));
  const bool ok = writer.writeUtf8(CERTIFICATE_FORMAT) &&
                  writer.writeUtf8(PRODUCT) &&
                  writer.writeUtf8(certificate.firmwareVersion) &&
                  writer.writeUtf8(certificate.firmwareBuildId) &&
                  writer.writeU32(certificate.buildDateYmd) &&
                  writer.writeUtf8(certificate.targetHardware) &&
                  writer.writeU32(certificate.imageBase) &&
                  writer.writeU32(certificate.imageSize) &&
                  writer.writeHash32(certificate.imageSha256) &&
                  writer.writeI64(certificate.approvedUtc) &&
                  writer.writeUtf8(certificate.certificateId) &&
                  writer.writeUtf8(certificate.rootKeyId);
  if (outputLength != nullptr) *outputLength = writer.length();
  return ok && writer.valid();
}

static bool FLASHMEM readFileExactly(const char* path, char* output,
                                     size_t outputSize, size_t* lengthOut)
{
  if (lengthOut != nullptr) *lengthOut = 0U;
  if (path == nullptr || output == nullptr || outputSize < 2U) return false;
  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory())
  {
    if (file) file.close();
    return false;
  }
  const uint32_t fileSize = file.size();
  if (fileSize == 0U || fileSize >= outputSize)
  {
    file.close();
    return false;
  }
  const size_t read = file.read((uint8_t*)output, fileSize);
  file.close();
  if (read != fileSize) return false;
  output[read] = '\0';
  if (lengthOut != nullptr) *lengthOut = read;
  return true;
}

static bool FLASHMEM writeFileTransactional(const char* finalPath,
                                            const char* tempPath,
                                            const char* backupPath,
                                            const char* data,
                                            size_t length)
{
  if (finalPath == nullptr || tempPath == nullptr || backupPath == nullptr ||
      data == nullptr || length == 0U || length >= TP_FW_CERT_MAX_JSON) return false;
  SD.remove(tempPath);
  File out = SD.open(tempPath, FILE_WRITE);
  if (!out) return false;
  const size_t written = out.write((const uint8_t*)data, length);
  out.flush();
  out.close();
  if (written != length)
  {
    SD.remove(tempPath);
    return false;
  }
  size_t verifyLength = 0U;
  if (!readFileExactly(tempPath, jsonVerifyScratch, sizeof(jsonVerifyScratch), &verifyLength) ||
      verifyLength != length || memcmp(jsonVerifyScratch, data, length) != 0)
  {
    SD.remove(tempPath);
    return false;
  }
  SD.remove(backupPath);
  const bool hadOld = SD.exists(finalPath);
  if (hadOld && !SD.rename(finalPath, backupPath))
  {
    SD.remove(tempPath);
    return false;
  }
  if (!SD.rename(tempPath, finalPath))
  {
    if (hadOld && SD.exists(backupPath)) SD.rename(backupPath, finalPath);
    return false;
  }
  verifyLength = 0U;
  const bool finalOk = readFileExactly(finalPath, jsonVerifyScratch,
                                       sizeof(jsonVerifyScratch), &verifyLength) &&
                       verifyLength == length &&
                       memcmp(jsonVerifyScratch, data, length) == 0;
  if (!finalOk)
  {
    SD.remove(finalPath);
    if (hadOld && SD.exists(backupPath)) SD.rename(backupPath, finalPath);
    return false;
  }
  if (SD.exists(backupPath)) SD.remove(backupPath);
  return true;
}

static bool FLASHMEM validPrintableText(const char* text, size_t maxLength)
{
  if (text == nullptr) return false;
  const size_t length = strlen(text);
  if (length == 0U || length > maxLength) return false;
  for (size_t i = 0U; i < length; ++i)
  {
    const uint8_t c = (uint8_t)text[i];
    if (c < 0x20U || c == 0x7FU) return false;
  }
  return true;
}

static bool FLASHMEM parseAndVerifyCertificate(const char* json,
                                               FirmwareCertificateParsed& certificate,
                                               bool* matchesCurrent,
                                               char* errorText,
                                               size_t errorTextSize)
{
  if (matchesCurrent != nullptr) *matchesCurrent = false;
  memset(&certificate, 0, sizeof(certificate));
  char format[64], product[24], imageHashText[65], approvedText[32];
  char manifestText[65], algorithm[32], encoding[24], signatureText[128];
  if (!jsonGetRootString(json, "Format", format, sizeof(format)) ||
      !jsonGetRootString(json, "Product", product, sizeof(product)) ||
      !jsonGetRootString(json, "FirmwareVersion", certificate.firmwareVersion, sizeof(certificate.firmwareVersion)) ||
      !jsonGetRootString(json, "FirmwareBuildId", certificate.firmwareBuildId, sizeof(certificate.firmwareBuildId)) ||
      !jsonGetRootUInt32(json, "BuildDateYmd", &certificate.buildDateYmd) ||
      !jsonGetRootString(json, "TargetHardware", certificate.targetHardware, sizeof(certificate.targetHardware)) ||
      !jsonGetRootUInt32(json, "ImageBase", &certificate.imageBase) ||
      !jsonGetRootUInt32(json, "ImageSize", &certificate.imageSize) ||
      !jsonGetRootString(json, "ImageSha256", imageHashText, sizeof(imageHashText)) ||
      !jsonGetRootString(json, "ApprovedUtc", approvedText, sizeof(approvedText)) ||
      !jsonGetRootString(json, "FirmwareCertificateId", certificate.certificateId, sizeof(certificate.certificateId)) ||
      !jsonGetRootString(json, "RootKeyId", certificate.rootKeyId, sizeof(certificate.rootKeyId)) ||
      !jsonGetRootString(json, "ManifestSha256", manifestText, sizeof(manifestText)) ||
      !jsonGetRootString(json, "SignatureAlgorithm", algorithm, sizeof(algorithm)) ||
      !jsonGetRootString(json, "SignatureEncoding", encoding, sizeof(encoding)) ||
      !jsonGetRootString(json, "RootSignatureBase64", signatureText, sizeof(signatureText)))
  {
    setError(errorText, errorTextSize, "Firmwarezertifikat ist unvollständig");
    return false;
  }
  size_t signatureLength = 0U;
  if (strcmp(format, CERTIFICATE_FORMAT) != 0 || strcmp(product, PRODUCT) != 0 ||
      strcmp(algorithm, SIGNATURE_ALGORITHM) != 0 ||
      strcmp(encoding, SIGNATURE_ENCODING) != 0 ||
      !validPrintableText(certificate.firmwareVersion, 23U) ||
      !validPrintableText(certificate.firmwareBuildId, 31U) ||
      !validPrintableText(certificate.targetHardware, 31U) ||
      !validPrintableText(certificate.certificateId, 63U) ||
      strlen(certificate.rootKeyId) != 16U ||
      !hexToBytes(imageHashText, certificate.imageSha256, 32U) ||
      !hexToBytes(manifestText, certificate.manifestSha256, 32U) ||
      !parseIsoUtc(approvedText, &certificate.approvedUtc) ||
      !base64Decode(signatureText, certificate.rootSignature,
                    sizeof(certificate.rootSignature), &signatureLength) ||
      signatureLength != sizeof(certificate.rootSignature))
  {
    setError(errorText, errorTextSize, "Firmwarezertifikat enthält ungültige Felder");
    return false;
  }
  if (!deviceIdentityCertificateValid() ||
      strcasecmp(certificate.rootKeyId, deviceIdentityRootKeyId()) != 0)
  {
    setError(errorText, errorTextSize,
             "Firmwarezertifikat wurde nicht vom eingebauten Hersteller-Root ausgestellt");
    return false;
  }
  size_t canonicalLength = 0U;
  uint8_t calculatedManifest[32];
  if (!buildCertificateCanonical(certificate, &canonicalLength))
  {
    setError(errorText, errorTextSize, "Firmwarezertifikat ist kanonisch zu groß");
    return false;
  }
  TpSha256::hash(canonicalBuffer, canonicalLength, calculatedManifest);
  if (!bytesEqual(calculatedManifest, certificate.manifestSha256, 32U) ||
      !deviceIdentityVerifyRootSignedHash(calculatedManifest, certificate.rootSignature))
  {
    memset(calculatedManifest, 0, sizeof(calculatedManifest));
    setError(errorText, errorTextSize, "Root-Signatur des Firmwarezertifikats ist ungültig");
    return false;
  }
  memset(calculatedManifest, 0, sizeof(calculatedManifest));
  const bool matches = certificate.imageBase == TP_FW_IMAGE_BASE &&
                       certificate.imageSize == fwStatus.measuredImageSize &&
                       strcmp(certificate.firmwareVersion, TP_FIRMWARE_VERSION_STRING) == 0 &&
                       strcmp(certificate.firmwareBuildId, TP_FIRMWARE_BUILD_ID_STRING) == 0 &&
                       certificate.buildDateYmd == TP_FIRMWARE_BUILD_DATE_YMD &&
                       strcmp(certificate.targetHardware, TP_FIRMWARE_TARGET_HARDWARE) == 0 &&
                       bytesEqual(certificate.imageSha256, fwStatus.measuredSha256, 32U);
  if (matchesCurrent != nullptr) *matchesCurrent = matches;
  setError(errorText, errorTextSize, matches ? "OK" : "Firmwarezertifikat gehört zu einem anderen Abbild");
  return true;
}

static void FLASHMEM applyCertificateStatus(const FirmwareCertificateParsed& certificate,
                                            bool matchesCurrent,
                                            const char* sourcePath)
{
  fwStatus.certificatePresent = true;
  fwStatus.certificateSignatureValid = true;
  fwStatus.certificateMatchesImage = matchesCurrent;
  fwStatus.manifestPresent = true;
  fwStatus.manifestFinalized = true;
  fwStatus.manifestCrcValid = true;
  fwStatus.expectedImageSize = certificate.imageSize;
  fwStatus.imageSizeMatches = certificate.imageSize == fwStatus.measuredImageSize;
  memcpy(fwStatus.expectedSha256, certificate.imageSha256, 32U);
  fwStatus.approvedUtc = certificate.approvedUtc;
  copyText(fwStatus.firmwareCertificateId, sizeof(fwStatus.firmwareCertificateId),
           certificate.certificateId);
  copyText(fwStatus.rootKeyId, sizeof(fwStatus.rootKeyId), certificate.rootKeyId);
  copyText(fwStatus.lastCertificateFile, sizeof(fwStatus.lastCertificateFile), sourcePath);
  if (matchesCurrent)
  {
    fwStatus.state = TP_FW_INTEGRITY_VERIFIED;
    copyText(fwStatus.statusText, sizeof(fwStatus.statusText), STATUS_VERIFIED);
  }
  else
  {
    fwStatus.state = TP_FW_INTEGRITY_MISMATCH;
    copyText(fwStatus.statusText, sizeof(fwStatus.statusText), STATUS_MISMATCH);
  }
}

static void FLASHMEM setCertificateInvalid(const char* sourcePath)
{
  fwStatus.certificatePresent = true;
  fwStatus.certificateSignatureValid = false;
  fwStatus.certificateMatchesImage = false;
  fwStatus.manifestPresent = true;
  fwStatus.manifestFinalized = false;
  fwStatus.manifestCrcValid = false;
  copyText(fwStatus.lastCertificateFile, sizeof(fwStatus.lastCertificateFile), sourcePath);
  fwStatus.state = TP_FW_INTEGRITY_ERROR;
  copyText(fwStatus.statusText, sizeof(fwStatus.statusText), STATUS_CERT_INVALID);
}

static bool FLASHMEM hasExtension(const char* name, const char* extension)
{
  if (name == nullptr || extension == nullptr) return false;
  const size_t nameLength = strlen(name);
  const size_t extensionLength = strlen(extension);
  return nameLength >= extensionLength &&
         strcasecmp(name + nameLength - extensionLength, extension) == 0;
}

static bool FLASHMEM buildCertificatePath(const char* directory,
                                          const char* name,
                                          char* output,
                                          size_t outputSize)
{
  if (directory == nullptr || name == nullptr || output == nullptr ||
      outputSize == 0U) return false;

  if (name[0] == '/')
  {
    const size_t nameLength = strnlen(name, outputSize);
    if (nameLength >= outputSize) return false;
    memcpy(output, name, nameLength + 1U);
    return true;
  }

  const size_t directoryLength = strnlen(directory, outputSize);
  const size_t nameLength = strnlen(name, outputSize);
  if (directoryLength >= outputSize || nameLength >= outputSize) return false;
  const bool rootDirectory = directoryLength == 1U && directory[0] == '/';
  const size_t required = directoryLength + (rootDirectory ? 0U : 1U) +
                          nameLength + 1U;
  if (required > outputSize) return false;

  memcpy(output, directory, directoryLength);
  size_t position = directoryLength;
  if (!rootDirectory) output[position++] = '/';
  memcpy(output + position, name, nameLength);
  output[position + nameLength] = '\0';
  return true;
}

static bool FLASHMEM scanDirectoryForCertificate(const char* directory,
                                                 char* bestPath,
                                                 size_t bestPathSize,
                                                 int64_t* bestApproved,
                                                 FirmwareCertificateParsed& bestCertificate)
{
  File dir = SD.open(directory);
  if (!dir || !dir.isDirectory())
  {
    if (dir) dir.close();
    return false;
  }
  bool found = false;
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory())
    {
      const char* name = entry.name();
      if (hasExtension(name, ".tpfwcert"))
      {
        char path[160];
        const bool pathValid = buildCertificatePath(directory, name,
                                                    path, sizeof(path));
        entry.close();
        if (!pathValid) continue;
        size_t length = 0U;
        if (readFileExactly(path, jsonScratch, sizeof(jsonScratch), &length))
        {
          FirmwareCertificateParsed candidate;
          bool matches = false;
          char error[160];
          if (parseAndVerifyCertificate(jsonScratch, candidate, &matches,
                                        error, sizeof(error)) && matches &&
              candidate.approvedUtc >= *bestApproved)
          {
            *bestApproved = candidate.approvedUtc;
            bestCertificate = candidate;
            copyText(bestPath, bestPathSize, path);
            found = true;
          }
        }
        continue;
      }
    }
    entry.close();
  }
  dir.close();
  return found;
}

static const char* FLASHMEM resultCode(void)
{
  switch (fwStatus.state)
  {
    case TP_FW_INTEGRITY_VERIFIED: return "OK";
    case TP_FW_INTEGRITY_MISMATCH: return "MISMATCH";
    case TP_FW_INTEGRITY_CERTIFICATE_MISSING: return "CERTIFICATE_MISSING";
    case TP_FW_INTEGRITY_ERROR: return "ERROR";
    default: return "NOT_RUN";
  }
}
}

void FLASHMEM tpFirmwareIntegrityBegin(void)
{
  tpFirmwareIntegrityBootService();
  memset(&fwStatus, 0, sizeof(fwStatus));
  fwStatus.state = TP_FW_INTEGRITY_NOT_RUN;
  fwStatus.imageBase = TP_FW_IMAGE_BASE;
  fwStatus.measuredImageSize = (uint32_t)(uintptr_t)&_flashimagelen;
  copyText(fwStatus.statusText, sizeof(fwStatus.statusText), STATUS_NOT_RUN);
  if (!calculateImageHash(fwStatus.measuredImageSize, fwStatus.measuredSha256))
  {
    fwStatus.state = TP_FW_INTEGRITY_ERROR;
    copyText(fwStatus.statusText, sizeof(fwStatus.statusText), STATUS_HASH_ERROR);
    return;
  }
  fwStatus.hashCalculated = true;
  fwStatus.state = TP_FW_INTEGRITY_CERTIFICATE_MISSING;
  copyText(fwStatus.statusText, sizeof(fwStatus.statusText), STATUS_CERT_MISSING);
}

uint32_t FLASHMEM tpFirmwareIntegrityLinkedImageSize(void)
{
  return (uint32_t)(uintptr_t)&_flashimagelen;
}

void FLASHMEM tpFirmwareIntegritySkipAfterBootCrash(void)
{
  tpFirmwareIntegrityBootService();
  memset(&fwStatus, 0, sizeof(fwStatus));
  fwStatus.state = TP_FW_INTEGRITY_ERROR;
  fwStatus.imageBase = TP_FW_IMAGE_BASE;
  fwStatus.measuredImageSize = (uint32_t)(uintptr_t)&_flashimagelen;
  copyText(fwStatus.statusText, sizeof(fwStatus.statusText),
           STATUS_BOOT_CRASH_SKIPPED);
  tpFirmwareIntegrityBootService();
}

void FLASHMEM tpFirmwareIntegrityFinalizeStartup(int64_t verifiedUtc)
{
  tpFirmwareIntegrityBootService();
  fwStatus.verifiedUtc = verifiedUtc > 0 ? verifiedUtc : 0;
  if (fwStatus.hashCalculated && sdLogEnsureReadyForAccess())
  {
    if (!SD.exists(CERT_DIR)) SD.mkdir(CERT_DIR);
    if (SD.exists(CERT_PATH))
    {
      size_t length = 0U;
      if (readFileExactly(CERT_PATH, jsonScratch, sizeof(jsonScratch), &length))
      {
        bool matches = false;
        char error[180];
        if (parseAndVerifyCertificate(jsonScratch, parsedCertificate, &matches,
                                      error, sizeof(error)))
          applyCertificateStatus(parsedCertificate, matches, CERT_PATH);
        else
          setCertificateInvalid(CERT_PATH);
      }
      else
      {
        setCertificateInvalid(CERT_PATH);
      }
    }
  }
  tpFirmwareIntegrityBootService();
  (void)tpFirmwareIntegrityWriteStatusToSd();
  tpFirmwareIntegrityBootService();
}

const TpFirmwareIntegrityStatus& FLASHMEM tpFirmwareIntegrityStatus(void)
{
  return fwStatus;
}

bool FLASHMEM tpFirmwareIntegrityVerified(void)
{
  return fwStatus.state == TP_FW_INTEGRITY_VERIFIED;
}

bool FLASHMEM tpFirmwareIntegrityManifestFinalized(void)
{
  return fwStatus.certificateSignatureValid;
}

bool FLASHMEM tpFirmwareIntegrityAllowsSensitiveOperations(void)
{
  return tpFirmwareIntegrityVerified();
}

const char* FLASHMEM tpFirmwareIntegrityStatusText(void)
{
  return fwStatus.statusText;
}

bool FLASHMEM tpFirmwareIntegrityGetExpectedHash(uint8_t out[32])
{
  if (out == nullptr || !fwStatus.certificateSignatureValid) return false;
  memcpy(out, fwStatus.expectedSha256, 32U);
  return true;
}

bool FLASHMEM tpFirmwareIntegrityGetMeasuredHash(uint8_t out[32])
{
  if (out == nullptr || !fwStatus.hashCalculated) return false;
  memcpy(out, fwStatus.measuredSha256, 32U);
  return true;
}

void FLASHMEM tpFirmwareIntegrityExpectedHashHex(char* out, size_t outSize)
{
  if (!fwStatus.certificateSignatureValid)
  {
    copyText(out, outSize, "kein Zertifikat");
    return;
  }
  bytesToHex(fwStatus.expectedSha256, 32U, out, outSize);
}

void FLASHMEM tpFirmwareIntegrityMeasuredHashHex(char* out, size_t outSize)
{
  if (!fwStatus.hashCalculated)
  {
    copyText(out, outSize, "nicht geprüft");
    return;
  }
  bytesToHex(fwStatus.measuredSha256, 32U, out, outSize);
}

bool FLASHMEM tpFirmwareIntegrityBuildRequestJson(char* output,
                                                  size_t outputSize,
                                                  size_t* outputLength,
                                                  char* downloadName,
                                                  size_t downloadNameSize,
                                                  char* errorText,
                                                  size_t errorTextSize)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (output == nullptr || outputSize < 1024U || downloadName == nullptr ||
      downloadNameSize < 32U)
  {
    setError(errorText, errorTextSize, "Ausgabepuffer für Firmwareanfrage ist ungültig");
    return false;
  }
  if (!fwStatus.hashCalculated)
  {
    setError(errorText, errorTextSize, "Firmwarehash wurde noch nicht berechnet");
    return false;
  }
  if (!deviceIdentityCertificateValid())
  {
    setError(errorText, errorTextSize, "Für die Firmwareanfrage ist ein gültiges Gerätezertifikat erforderlich");
    return false;
  }
  const int64_t createdUtc = tpCurrentUtcUnixTime();
  if (createdUtc < 1577836800LL)
  {
    setError(errorText, errorTextSize, "UTC-Zeit ist nicht gültig");
    return false;
  }
  uint8_t requestIdBytes[16];
  uint8_t deviceCertManifest[32];
  uint8_t manifest[32];
  uint8_t signature[64];
  char requestId[33], createdText[32], expiresText[32], imageHashText[65], deviceCertManifestText[65];
  char manifestText[65], signatureText[96];
  size_t deviceCertificateLength = 0U;
  if (!deviceIdentityRandomBytes(requestIdBytes, sizeof(requestIdBytes)) ||
      !deviceIdentityCertificateManifestHash(deviceCertManifest) ||
      !deviceIdentityBuildCertificateJson(jsonVerifyScratch, sizeof(jsonVerifyScratch),
                                          &deviceCertificateLength))
  {
    setError(errorText, errorTextSize, "Geräteidentität konnte nicht in die Firmwareanfrage übernommen werden");
    return false;
  }
  bytesToHex(requestIdBytes, sizeof(requestIdBytes), requestId, sizeof(requestId));
  formatIsoUtc(createdUtc, createdText, sizeof(createdText));
  formatIsoUtc(createdUtc + TP_FW_REQUEST_VALIDITY_SECONDS, expiresText, sizeof(expiresText));
  bytesToHex(fwStatus.measuredSha256, 32U, imageHashText, sizeof(imageHashText));
  bytesToHex(deviceCertManifest, 32U, deviceCertManifestText, sizeof(deviceCertManifestText));
  size_t canonicalLength = 0U;
  if (!buildRequestCanonical(deviceIdentityCertifiedSerial(),
                             deviceIdentityDeviceKeyId(),
                             deviceIdentityCertificateSerialText(),
                             requestId, createdUtc,
                             createdUtc + TP_FW_REQUEST_VALIDITY_SECONDS,
                             fwStatus.measuredSha256,
                             deviceCertManifest, &canonicalLength))
  {
    setError(errorText, errorTextSize, "Firmwareanfrage ist kanonisch zu groß");
    return false;
  }
  TpSha256::hash(canonicalBuffer, canonicalLength, manifest);
  if (!deviceIdentitySignHash(manifest, signature) ||
      !base64Encode(signature, sizeof(signature), signatureText, sizeof(signatureText)))
  {
    setError(errorText, errorTextSize, "Firmwareanfrage konnte nicht mit dem Geräteschlüssel signiert werden");
    return false;
  }
  bytesToHex(manifest, 32U, manifestText, sizeof(manifestText));
  const int length = snprintf(
      output, outputSize,
      "{\n"
      "  \"Format\": \"%s\",\n"
      "  \"Product\": \"%s\",\n"
      "  \"DeviceSerial\": \"%s\",\n"
      "  \"DeviceKeyId\": \"%s\",\n"
      "  \"DeviceCertificateSerial\": \"%s\",\n"
      "  \"RequestId\": \"%s\",\n"
      "  \"CreatedUtc\": \"%s\",\n"
      "  \"ExpiresUtc\": \"%s\",\n"
      "  \"FirmwareVersion\": \"%s\",\n"
      "  \"FirmwareBuildId\": \"%s\",\n"
      "  \"BuildDateYmd\": %lu,\n"
      "  \"TargetHardware\": \"%s\",\n"
      "  \"ImageBase\": %lu,\n"
      "  \"ImageSize\": %lu,\n"
      "  \"ImageSha256\": \"%s\",\n"
      "  \"DeviceCertificateManifestSha256\": \"%s\",\n"
      "  \"DeviceCertificate\": %s,\n"
      "  \"ManifestSha256\": \"%s\",\n"
      "  \"SignatureAlgorithm\": \"%s\",\n"
      "  \"SignatureEncoding\": \"%s\",\n"
      "  \"DeviceSignatureBase64\": \"%s\"\n"
      "}\n",
      REQUEST_FORMAT, PRODUCT, deviceIdentityCertifiedSerial(),
      deviceIdentityDeviceKeyId(), deviceIdentityCertificateSerialText(),
      requestId, createdText, expiresText,
      TP_FIRMWARE_VERSION_STRING, TP_FIRMWARE_BUILD_ID_STRING,
      (unsigned long)TP_FIRMWARE_BUILD_DATE_YMD,
      TP_FIRMWARE_TARGET_HARDWARE,
      (unsigned long)TP_FW_IMAGE_BASE,
      (unsigned long)fwStatus.measuredImageSize,
      imageHashText, deviceCertManifestText, jsonVerifyScratch,
      manifestText, SIGNATURE_ALGORITHM, SIGNATURE_ENCODING, signatureText);
  memset(requestIdBytes, 0, sizeof(requestIdBytes));
  memset(signature, 0, sizeof(signature));
  memset(manifest, 0, sizeof(manifest));
  if (length <= 0 || (size_t)length >= outputSize)
  {
    setError(errorText, errorTextSize, "Firmwareanfrage passt nicht in den Ausgabepuffer");
    return false;
  }
  snprintf(downloadName, downloadNameSize, "FWREQ-G%s-%s.tpfwreq",
           deviceIdentityCertifiedSerial(), TP_FIRMWARE_BUILD_ID_STRING);
  if (outputLength != nullptr) *outputLength = (size_t)length;
  setError(errorText, errorTextSize, "OK");
  return true;
}

bool FLASHMEM tpFirmwareIntegritySaveRequestToSd(const char* json,
                                                 size_t jsonLength,
                                                 const char* downloadName,
                                                 char* storedPath,
                                                 size_t storedPathSize,
                                                 char* errorText,
                                                 size_t errorTextSize)
{
  if (!sdLogEnsureReadyForAccess())
  {
    setError(errorText, errorTextSize, "SD-Karte ist nicht bereit");
    return false;
  }
  if (!SD.exists(CERT_DIR) && !SD.mkdir(CERT_DIR))
  {
    setError(errorText, errorTextSize, "Zertifizierungsverzeichnis konnte nicht angelegt werden");
    return false;
  }
  if (!SD.exists(REQUEST_DIR) && !SD.mkdir(REQUEST_DIR))
  {
    setError(errorText, errorTextSize, "Firmwareanfrage-Verzeichnis konnte nicht angelegt werden");
    return false;
  }
  char path[160];
  snprintf(path, sizeof(path), "%s/%s", REQUEST_DIR, downloadName);
  if (SD.exists(path)) SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file)
  {
    setError(errorText, errorTextSize, "Firmwareanfrage konnte nicht auf SD geöffnet werden");
    return false;
  }
  const size_t written = file.write((const uint8_t*)json, jsonLength);
  file.flush();
  file.close();
  if (written != jsonLength)
  {
    SD.remove(path);
    setError(errorText, errorTextSize, "Firmwareanfrage wurde nicht vollständig auf SD geschrieben");
    return false;
  }
  size_t verifyLength = 0U;
  if (!readFileExactly(path, jsonVerifyScratch, sizeof(jsonVerifyScratch), &verifyLength) ||
      verifyLength != jsonLength || memcmp(json, jsonVerifyScratch, jsonLength) != 0)
  {
    SD.remove(path);
    setError(errorText, errorTextSize, "Firmwareanfrage konnte nicht zurückgelesen werden");
    return false;
  }
  copyText(storedPath, storedPathSize, path);
  setError(errorText, errorTextSize, "OK");
  return true;
}

bool FLASHMEM tpFirmwareIntegrityImportCertificateJson(const char* json,
                                                       size_t jsonLength,
                                                       char* errorText,
                                                       size_t errorTextSize)
{
  if (json == nullptr || jsonLength == 0U || jsonLength >= sizeof(jsonScratch))
  {
    setError(errorText, errorTextSize, "Firmwarezertifikat ist leer oder zu groß");
    return false;
  }
  memcpy(jsonScratch, json, jsonLength);
  jsonScratch[jsonLength] = '\0';
  FirmwareCertificateParsed certificate;
  bool matches = false;
  if (!parseAndVerifyCertificate(jsonScratch, certificate, &matches,
                                 errorText, errorTextSize)) return false;
  if (!matches)
  {
    setError(errorText, errorTextSize,
             "Firmwarezertifikat ist gültig, gehört aber nicht zum laufenden Firmwareabbild");
    return false;
  }
  if (!sdLogEnsureReadyForAccess() ||
      (!SD.exists(CERT_DIR) && !SD.mkdir(CERT_DIR)))
  {
    setError(errorText, errorTextSize, "SD-Karte ist nicht bereit");
    return false;
  }
  if (!writeFileTransactional(CERT_PATH, CERT_TEMP, CERT_BACKUP,
                              jsonScratch, jsonLength))
  {
    setError(errorText, errorTextSize, "Firmwarezertifikat konnte nicht sicher gespeichert werden");
    return false;
  }
  applyCertificateStatus(certificate, true, CERT_PATH);
  fwStatus.verifiedUtc = tpCurrentUtcUnixTime();
  (void)tpFirmwareIntegrityWriteStatusToSd();
  setError(errorText, errorTextSize, "Firmwarezertifikat geprüft und aktiviert");
  return true;
}

bool FLASHMEM tpFirmwareIntegrityImportCertificateFromSd(char* importedPath,
                                                         size_t importedPathSize,
                                                         char* errorText,
                                                         size_t errorTextSize)
{
  if (!sdLogEnsureReadyForAccess())
  {
    setError(errorText, errorTextSize, "SD-Karte ist nicht bereit");
    return false;
  }
  char bestPath[160] = {0};
  int64_t bestApproved = 0;
  FirmwareCertificateParsed best;
  memset(&best, 0, sizeof(best));
  bool found = scanDirectoryForCertificate(CERT_DIR, bestPath, sizeof(bestPath),
                                           &bestApproved, best);
  found = scanDirectoryForCertificate("/", bestPath, sizeof(bestPath),
                                      &bestApproved, best) || found;
  if (!found)
  {
    setError(errorText, errorTextSize,
             "Keine passende .tpfwcert-Datei auf der SD-Karte gefunden");
    return false;
  }
  size_t length = 0U;
  if (!readFileExactly(bestPath, jsonScratch, sizeof(jsonScratch), &length) ||
      !tpFirmwareIntegrityImportCertificateJson(jsonScratch, length,
                                                errorText, errorTextSize))
    return false;
  copyText(importedPath, importedPathSize, bestPath);
  return true;
}

bool FLASHMEM tpFirmwareIntegrityCreateRequestOnSd(char* storedPath,
                                                   size_t storedPathSize,
                                                   char* message,
                                                   size_t messageSize)
{
  size_t length = 0U;
  char downloadName[96];
  char error[192];
  if (!tpFirmwareIntegrityBuildRequestJson(jsonScratch, sizeof(jsonScratch),
                                           &length, downloadName, sizeof(downloadName),
                                           error, sizeof(error)))
  {
    setError(message, messageSize, error);
    return false;
  }
  if (!tpFirmwareIntegritySaveRequestToSd(jsonScratch, length, downloadName,
                                          storedPath, storedPathSize,
                                          error, sizeof(error)))
  {
    setError(message, messageSize, error);
    return false;
  }
  setError(message, messageSize, "Firmwareanfrage auf SD gespeichert");
  return true;
}

bool FLASHMEM tpFirmwareIntegrityWriteStatusToSd(void)
{
  if (!sdLogEnsureReadyForAccess()) return false;
  if (!SD.exists(CERT_DIR) && !SD.mkdir(CERT_DIR)) return false;
  char expectedHex[65], measuredHex[65];
  tpFirmwareIntegrityExpectedHashHex(expectedHex, sizeof(expectedHex));
  tpFirmwareIntegrityMeasuredHashHex(measuredHex, sizeof(measuredHex));
  const int length = snprintf(
      statusFileBuffer, sizeof(statusFileBuffer),
      "%s\n"
      "DeviceSerial=%s\n"
      "Version=%s\n"
      "BuildId=%s\n"
      "BuildDateYmd=%lu\n"
      "ImageBase=0x%08lX\n"
      "ExpectedImageSize=%lu\n"
      "MeasuredImageSize=%lu\n"
      "ExpectedSha256=%s\n"
      "MeasuredSha256=%s\n"
      "FirmwareCertificateId=%s\n"
      "RootKeyId=%s\n"
      "ApprovedUtc=%lld\n"
      "VerifiedUtc=%lld\n"
      "Result=%s\n"
      "Status=%s\n",
      STATUS_FILE_FORMAT, deviceSerialGet(), TP_FIRMWARE_VERSION_STRING,
      TP_FIRMWARE_BUILD_ID_STRING, (unsigned long)TP_FIRMWARE_BUILD_DATE_YMD,
      (unsigned long)fwStatus.imageBase,
      (unsigned long)fwStatus.expectedImageSize,
      (unsigned long)fwStatus.measuredImageSize,
      expectedHex, measuredHex, fwStatus.firmwareCertificateId,
      fwStatus.rootKeyId, (long long)fwStatus.approvedUtc,
      (long long)fwStatus.verifiedUtc, resultCode(), fwStatus.statusText);
  if (length <= 0 || (size_t)length >= sizeof(statusFileBuffer)) return false;
  return writeFileTransactional(STATUS_PATH, STATUS_TEMP, STATUS_BACKUP,
                                statusFileBuffer, (size_t)length);
}
