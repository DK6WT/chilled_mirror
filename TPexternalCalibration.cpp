/*
 * TP-3000 external calibration certificate PDF storage
 *
 * PDF files are uploaded in small HTTP chunks and written directly to SD.
 * The original bytes are never rewritten. A streamed SHA-256 is compared
 * with a second hash pass after closing and reopening the temporary file.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPexternalCalibration.h"
#include "TPsha256.h"
#include "TPsharedScratch.h"
#include <Arduino.h>
#include <SD.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#if defined(__IMXRT1062__)
#define TP_EXTERNAL_RODATA __attribute__((section(".progmem")))
#else
#define TP_EXTERNAL_RODATA
#endif

extern bool sdLogEnsureReadyForAccess(void);
extern void tpSignedCalibrationInvalidateCache(void);

namespace
{
static const char EXTERNAL_DIR[] TP_EXTERNAL_RODATA = "/CALIBRATION/EXTERNAL";
static const char TEMP_PDF[] TP_EXTERNAL_RODATA = "/CALIBRATION/EXTERNAL/UPLOAD.tmp";
static const char ACTIVE_JSON[] TP_EXTERNAL_RODATA = "/CALIBRATION/EXTERNAL/ACTIVE.json";
static const char ACTIVE_JSON_TEMP[] TP_EXTERNAL_RODATA = "/CALIBRATION/EXTERNAL/ACTIVE.tmp";
static const char ACTIVE_JSON_BACKUP[] TP_EXTERNAL_RODATA = "/CALIBRATION/EXTERNAL/ACTIVE.bak";
static const char METADATA_FORMAT[] TP_EXTERNAL_RODATA = "TP3000-EXTERNAL-CALIBRATION-PDF-1";
static const char METADATA_JSON_TEMPLATE[] TP_EXTERNAL_RODATA =
  "{\n"
  "  \"Format\": \"%s\",\n"
  "  \"DocumentId\": \"%s\",\n"
  "  \"CertificateNumber\": \"%s\",\n"
  "  \"ValidFromYmd\": %lu,\n"
  "  \"ValidUntilYmd\": %lu,\n"
  "  \"OriginalFileName\": \"%s\",\n"
  "  \"StoredPdfPath\": \"%s\",\n"
  "  \"FileSize\": %lu,\n"
  "  \"Sha256\": \"%s\"\n"
  "}\n";

static const char CALIBRATION_DIR[] TP_EXTERNAL_RODATA = "/CALIBRATION";
static const char HEX_LOWER[] TP_EXTERNAL_RODATA = "0123456789abcdef";
static const char JSON_KEY_TOKEN[] TP_EXTERNAL_RODATA = "\"%s\"";
static const char JSON_FIELD_FORMAT[] TP_EXTERNAL_RODATA = "Format";
static const char JSON_FIELD_DOCUMENT_ID[] TP_EXTERNAL_RODATA = "DocumentId";
static const char JSON_FIELD_CERTIFICATE_NUMBER[] TP_EXTERNAL_RODATA = "CertificateNumber";
static const char JSON_FIELD_VALID_FROM[] TP_EXTERNAL_RODATA = "ValidFromYmd";
static const char JSON_FIELD_VALID_UNTIL[] TP_EXTERNAL_RODATA = "ValidUntilYmd";
static const char JSON_FIELD_ORIGINAL_NAME[] TP_EXTERNAL_RODATA = "OriginalFileName";
static const char JSON_FIELD_STORED_PATH[] TP_EXTERNAL_RODATA = "StoredPdfPath";
static const char JSON_FIELD_FILE_SIZE[] TP_EXTERNAL_RODATA = "FileSize";
static const char JSON_FIELD_SHA256[] TP_EXTERNAL_RODATA = "Sha256";
static const char PDF_MAGIC[] TP_EXTERNAL_RODATA = "%PDF-";
static const char RESULT_FORMAT[] TP_EXTERNAL_RODATA =
  "Externer Kalibrierschein %s gespeichert: %lu Byte, SHA-256 %s";

static const char TEXT_EMPTY[] TP_EXTERNAL_RODATA = "";
static const char ERROR_SD_NOT_READY[] TP_EXTERNAL_RODATA =
  "SD-Karte oder Kalibrierverzeichnis ist nicht bereit";
static const char ERROR_CERTIFICATE_NUMBER[] TP_EXTERNAL_RODATA =
  "Kalibrierschein-Nr. / Zeichen muss 1 bis 32 druckbare Zeichen enthalten";
static const char ERROR_VALIDITY[] TP_EXTERNAL_RODATA =
  "Gültigkeitszeitraum ist ungültig";
static const char ERROR_PDF_SIZE[] TP_EXTERNAL_RODATA =
  "PDF muss zwischen 5 Byte und 6 MiB groß sein";
static const char ERROR_PDF_FILENAME[] TP_EXTERNAL_RODATA =
  "PDF-Dateiname ist ungültig oder endet nicht auf .pdf";
static const char ERROR_TEMP_CREATE[] TP_EXTERNAL_RODATA =
  "Temporäre PDF-Datei konnte nicht auf SD angelegt werden";
static const char ERROR_NO_UPLOAD[] TP_EXTERNAL_RODATA =
  "Kein externer PDF-Upload aktiv";
static const char ERROR_CHUNK_ORDER[] TP_EXTERNAL_RODATA =
  "PDF-Block ist leer, außerhalb der Reihenfolge oder zu groß";
static const char ERROR_PDF_MAGIC[] TP_EXTERNAL_RODATA =
  "Datei beginnt nicht mit einer PDF-Kennung (%PDF-)";
static const char ERROR_CHUNK_WRITE[] TP_EXTERNAL_RODATA =
  "PDF-Block konnte nicht vollständig auf SD geschrieben werden";
static const char ERROR_UPLOAD_INCOMPLETE[] TP_EXTERNAL_RODATA =
  "PDF-Upload ist unvollständig";
static const char ERROR_REHASH[] TP_EXTERNAL_RODATA =
  "PDF konnte nach dem Schließen nicht identisch verifiziert werden";
static const char ERROR_ARCHIVE_CONFLICT[] TP_EXTERNAL_RODATA =
  "Eine gleich benannte Archivdatei ist nicht hash-identisch";
static const char ERROR_ARCHIVE_RENAME[] TP_EXTERNAL_RODATA =
  "Geprüfte PDF-Datei konnte nicht in das SD-Archiv übernommen werden";
static const char ERROR_METADATA_CONFLICT[] TP_EXTERNAL_RODATA =
  "Diese PDF ist bereits mit abweichender Nummer, Gültigkeit oder Dateibezeichnung archiviert";
static const char ERROR_METADATA_WRITE[] TP_EXTERNAL_RODATA =
  "Metadaten des externen Kalibrierscheins konnten nicht gespeichert werden";
static const char ERROR_ACTIVE_BACKUP[] TP_EXTERNAL_RODATA =
  "Bisherige aktive PDF-Metadaten konnten nicht gesichert werden";
static const char ERROR_ACTIVE_SWITCH[] TP_EXTERNAL_RODATA =
  "Aktive PDF-Metadaten konnten nicht atomar übernommen werden";
static const char ERROR_ACTIVE_VERIFY[] TP_EXTERNAL_RODATA =
  "Aktive PDF-Metadaten konnten nach dem Speichern nicht verifiziert werden";

// Reine Datenpuffer liegen in RAM2. DMAMEM wird beim Start nicht als
// initialisiert vorausgesetzt; tpExternalCalibrationBegin() setzt deshalb
// alle persistent verwendeten Felder explizit zurück. C++-Objekte mit
// Konstruktor (File und TpSha256) bleiben bewusst in RAM1.
static DMAMEM TpExternalCalibrationMetadata activeMetadata;
static File uploadFile;
static DMAMEM bool uploadActive;
static DMAMEM uint32_t uploadExpectedSize;
static DMAMEM uint32_t uploadReceivedSize;
static DMAMEM char uploadCertificateNumber[33];
static DMAMEM uint32_t uploadValidFromYmd;
static DMAMEM uint32_t uploadValidUntilYmd;
static DMAMEM char uploadOriginalFileName[81];
static TpSha256 uploadSha256;
static DMAMEM char metadataJson[1536];
static DMAMEM TpExternalCalibrationMetadata metadataWorkA;
static DMAMEM TpExternalCalibrationMetadata metadataWorkB;
static DMAMEM char finalPdfPath[128];
static DMAMEM char finalMetadataPath[128];
static DMAMEM char hashHexWork[65];
// Vollständig geprüfte Archiv-PDFs werden für die Laufzeit gecacht. Dadurch
// muss die bis zu 6 MiB große Datei bei Statusabfragen nicht immer wieder
// gehasht werden. Der Cache wird beim Start explizit gelöscht.
static DMAMEM bool archivedMatchCacheValid;
static DMAMEM char archivedMatchDocumentId[33];
static DMAMEM char archivedMatchCertificateNumber[33];
static DMAMEM uint32_t archivedMatchValidFromYmd;
static DMAMEM uint32_t archivedMatchValidUntilYmd;
static DMAMEM char archivedMatchOriginalFileName[81];
static DMAMEM char archivedMatchSha256[65];
static DMAMEM uint32_t archivedMatchFileSize;

static void FLASHMEM setText(char* target, size_t size, const char* text)
{
  if (target == nullptr || size == 0U) return;
  if (text == nullptr) text = TEXT_EMPTY;
  const size_t length = strnlen(text, size - 1U);
  memmove(target, text, length);
  target[length] = '\0';
}

static void FLASHMEM setError(char* target, size_t size, const char* text)
{
  setText(target, size, text);
}

static bool FLASHMEM validYmd(uint32_t ymd)
{
  const uint32_t year = ymd / 10000U;
  const uint32_t month = (ymd / 100U) % 100U;
  const uint32_t day = ymd % 100U;
  if (year < 2024U || year > 2199U || month < 1U || month > 12U || day < 1U) return false;
  static const uint8_t daysPerMonth[12] TP_EXTERNAL_RODATA = {31,28,31,30,31,30,31,31,30,31,30,31};
  uint8_t maxDay = daysPerMonth[month - 1U];
  const bool leap = ((year % 4U) == 0U && (year % 100U) != 0U) || (year % 400U) == 0U;
  if (month == 2U && leap) maxDay = 29U;
  return day <= maxDay;
}

static bool FLASHMEM validCertificateNumber(const char* value)
{
  if (value == nullptr) return false;
  const size_t length = strlen(value);
  if (length == 0U || length > 32U) return false;
  for (size_t i = 0U; i < length; ++i)
  {
    const uint8_t c = (uint8_t)value[i];
    if (c < 0x20U || c > 0x7EU || c == '"' || c == '\\') return false;
  }
  return true;
}

static bool FLASHMEM sanitizeFileName(const char* input, char* output, size_t outputSize)
{
  if (input == nullptr || output == nullptr || outputSize < 5U) return false;
  const char* base = input;
  for (const char* p = input; *p != '\0'; ++p)
  {
    if (*p == '/' || *p == '\\') base = p + 1;
  }
  size_t out = 0U;
  for (const char* p = base; *p != '\0' && out + 1U < outputSize; ++p)
  {
    const uint8_t c = (uint8_t)*p;
    if (c >= 0x20U && c <= 0x7EU && c != '"' && c != '\\' && c != '/' && c != ':')
      output[out++] = (char)c;
    else
      output[out++] = '_';
  }
  output[out] = '\0';
  if (out < 4U) return false;
  const char* ext = output + out - 4U;
  return (ext[0] == '.') &&
         ((ext[1] == 'p' || ext[1] == 'P') &&
          (ext[2] == 'd' || ext[2] == 'D') &&
          (ext[3] == 'f' || ext[3] == 'F'));
}

static bool FLASHMEM ensureDirectories()
{
  if (!sdLogEnsureReadyForAccess()) return false;
  if (!SD.exists(CALIBRATION_DIR) && !SD.mkdir(CALIBRATION_DIR)) return false;
  if (!SD.exists(EXTERNAL_DIR) && !SD.mkdir(EXTERNAL_DIR)) return false;
  return true;
}

static void FLASHMEM bytesToHex(const uint8_t* source, size_t count, char* output, size_t outputSize)
{
    if (output == nullptr || outputSize < count * 2U + 1U) return;
  for (size_t i = 0U; i < count; ++i)
  {
    output[i * 2U] = HEX_LOWER[source[i] >> 4U];
    output[i * 2U + 1U] = HEX_LOWER[source[i] & 0x0FU];
  }
  output[count * 2U] = '\0';
}

static bool FLASHMEM hashFile(const char* path, uint8_t digest[32], uint32_t* sizeOut)
{
  if (digest == nullptr || path == nullptr) return false;
  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory())
  {
    if (file) file.close();
    return false;
  }
  TpSha256 hash;
  uint32_t total = 0U;
  while (file.available())
  {
    int count = file.read(tpSharedSha256IoBuffer, TP_SHARED_SHA256_IO_BUFFER_SIZE);
    if (count <= 0)
    {
      file.close();
      return false;
    }
    hash.update(tpSharedSha256IoBuffer, (size_t)count);
    total += (uint32_t)count;
    if (total > TP_EXTERNAL_CAL_MAX_PDF_BYTES)
    {
      file.close();
      return false;
    }
  }
  file.close();
  hash.final(digest);
  if (sizeOut != nullptr) *sizeOut = total;
  return true;
}

static bool FLASHMEM jsonGetString(const char* json, const char* key, char* output, size_t outputSize)
{
  if (json == nullptr || key == nullptr || output == nullptr || outputSize == 0U) return false;
  char token[64];
  const int tokenLength = snprintf(token, sizeof(token), JSON_KEY_TOKEN, key);
  if (tokenLength <= 0 || (size_t)tokenLength >= sizeof(token)) return false;
  const char* p = strstr(json, token);
  if (p == nullptr) return false;
  p += tokenLength;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  if (*p++ != ':') return false;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  if (*p++ != '"') return false;
  size_t out = 0U;
  while (*p != '\0' && *p != '"')
  {
    if (*p == '\\') return false;
    if (out + 1U >= outputSize) return false;
    output[out++] = *p++;
  }
  if (*p != '"') return false;
  output[out] = '\0';
  return true;
}

static bool FLASHMEM jsonGetU32(const char* json, const char* key, uint32_t* value)
{
  if (json == nullptr || key == nullptr || value == nullptr) return false;
  char token[64];
  const int tokenLength = snprintf(token, sizeof(token), JSON_KEY_TOKEN, key);
  if (tokenLength <= 0 || (size_t)tokenLength >= sizeof(token)) return false;
  const char* p = strstr(json, token);
  if (p == nullptr) return false;
  p += tokenLength;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  if (*p++ != ':') return false;
  while (*p == ' ' || *p == '\t') ++p;
  if (*p < '0' || *p > '9') return false;
  uint64_t parsed = 0U;
  while (*p >= '0' && *p <= '9')
  {
    parsed = parsed * 10U + (uint64_t)(*p - '0');
    if (parsed > 0xFFFFFFFFULL) return false;
    ++p;
  }
  *value = (uint32_t)parsed;
  return true;
}

static bool FLASHMEM readMetadataFile(const char* path, TpExternalCalibrationMetadata& metadata)
{
  memset(&metadata, 0, sizeof(metadata));
  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory())
  {
    if (file) file.close();
    return false;
  }
  const size_t size = (size_t)file.size();
  if (size == 0U || size >= sizeof(metadataJson))
  {
    file.close();
    return false;
  }
  const int read = file.read((uint8_t*)metadataJson, size);
  file.close();
  if (read != (int)size) return false;
  metadataJson[size] = '\0';

  char format[64];
  if (!jsonGetString(metadataJson, JSON_FIELD_FORMAT, format, sizeof(format)) || strcmp(format, METADATA_FORMAT) != 0 ||
      !jsonGetString(metadataJson, JSON_FIELD_DOCUMENT_ID, metadata.documentId, sizeof(metadata.documentId)) ||
      !jsonGetString(metadataJson, JSON_FIELD_CERTIFICATE_NUMBER, metadata.certificateNumber, sizeof(metadata.certificateNumber)) ||
      !jsonGetU32(metadataJson, JSON_FIELD_VALID_FROM, &metadata.validFromYmd) ||
      !jsonGetU32(metadataJson, JSON_FIELD_VALID_UNTIL, &metadata.validUntilYmd) ||
      !jsonGetString(metadataJson, JSON_FIELD_ORIGINAL_NAME, metadata.originalFileName, sizeof(metadata.originalFileName)) ||
      !jsonGetString(metadataJson, JSON_FIELD_STORED_PATH, metadata.storedPdfPath, sizeof(metadata.storedPdfPath)) ||
      !jsonGetU32(metadataJson, JSON_FIELD_FILE_SIZE, &metadata.fileSize) ||
      !jsonGetString(metadataJson, JSON_FIELD_SHA256, metadata.sha256, sizeof(metadata.sha256)))
  {
    memset(&metadata, 0, sizeof(metadata));
    return false;
  }
  if (!validCertificateNumber(metadata.certificateNumber) || !validYmd(metadata.validFromYmd) ||
      !validYmd(metadata.validUntilYmd) || metadata.validUntilYmd < metadata.validFromYmd ||
      metadata.fileSize == 0U || metadata.fileSize > TP_EXTERNAL_CAL_MAX_PDF_BYTES ||
      strlen(metadata.documentId) != 32U || strlen(metadata.sha256) != 64U ||
      !SD.exists(metadata.storedPdfPath))
  {
    memset(&metadata, 0, sizeof(metadata));
    return false;
  }

  // Nicht nur dem Index vertrauen: Die Originaldatei wird bei jedem Start
  // erneut bytegenau gegen Größe und SHA-256 geprüft. Dadurch verliert eine
  // nachträglich auf der SD veränderte PDF sofort ihren Aktivstatus.
  uint8_t digest[32];
  uint32_t verifiedSize = 0U;
  char verifiedHash[65];
  if (!hashFile(metadata.storedPdfPath, digest, &verifiedSize) ||
      verifiedSize != metadata.fileSize)
  {
    memset(&metadata, 0, sizeof(metadata));
    return false;
  }
  bytesToHex(digest, 32U, verifiedHash, sizeof(verifiedHash));
  if (strcasecmp(verifiedHash, metadata.sha256) != 0 ||
      strncasecmp(verifiedHash, metadata.documentId, 32U) != 0)
  {
    memset(&metadata, 0, sizeof(metadata));
    return false;
  }
  File pdf = SD.open(metadata.storedPdfPath, FILE_READ);
  uint8_t header[5] = {0};
  const int headerRead = pdf ? pdf.read(header, sizeof(header)) : -1;
  if (pdf) pdf.close();
  if (headerRead != 5 || memcmp(header, PDF_MAGIC, 5U) != 0)
  {
    memset(&metadata, 0, sizeof(metadata));
    return false;
  }
  metadata.present = true;
  return true;
}

static bool FLASHMEM writeTextFile(const char* path, const char* text, size_t length)
{
  SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  const size_t written = file.write((const uint8_t*)text, length);
  file.flush();
  file.close();
  return written == length;
}

static bool FLASHMEM buildMetadataJson(const TpExternalCalibrationMetadata& metadata, size_t* lengthOut)
{
  const int length = snprintf(metadataJson, sizeof(metadataJson), METADATA_JSON_TEMPLATE,
    METADATA_FORMAT,
    metadata.documentId,
    metadata.certificateNumber,
    (unsigned long)metadata.validFromYmd,
    (unsigned long)metadata.validUntilYmd,
    metadata.originalFileName,
    metadata.storedPdfPath,
    (unsigned long)metadata.fileSize,
    metadata.sha256);
  if (length <= 0 || (size_t)length >= sizeof(metadataJson)) return false;
  if (lengthOut != nullptr) *lengthOut = (size_t)length;
  return true;
}

static void FLASHMEM clearUploadState(bool removeTemp)
{
  if (uploadFile) uploadFile.close();
  if (removeTemp && SD.exists(TEMP_PDF)) SD.remove(TEMP_PDF);
  uploadActive = false;
  uploadExpectedSize = 0U;
  uploadReceivedSize = 0U;
  uploadCertificateNumber[0] = '\0';
  uploadValidFromYmd = 0U;
  uploadValidUntilYmd = 0U;
  uploadOriginalFileName[0] = '\0';
  uploadSha256.reset();
}
}

void FLASHMEM tpExternalCalibrationBegin(void)
{
  memset(&activeMetadata, 0, sizeof(activeMetadata));
  archivedMatchCacheValid = false;
  archivedMatchDocumentId[0] = '\0';
  archivedMatchCertificateNumber[0] = '\0';
  archivedMatchValidFromYmd = 0U;
  archivedMatchValidUntilYmd = 0U;
  archivedMatchOriginalFileName[0] = '\0';
  archivedMatchSha256[0] = '\0';
  archivedMatchFileSize = 0U;
  clearUploadState(false);
  if (!ensureDirectories()) return;

  // Nach einem Stromausfall während des atomaren Zeigerwechsels die zuletzt
  // sicher gespeicherte aktive Metadatei wiederherstellen. Ein vorhandenes
  // Backup wird erst gelöscht, wenn ACTIVE.json einschließlich PDF-Hash
  // erfolgreich geprüft wurde.
  bool activeOk = false;
  if (SD.exists(ACTIVE_JSON))
    activeOk = readMetadataFile(ACTIVE_JSON, activeMetadata);
  if (!activeOk && SD.exists(ACTIVE_JSON_BACKUP))
  {
    if (SD.exists(ACTIVE_JSON)) SD.remove(ACTIVE_JSON);
    if (SD.rename(ACTIVE_JSON_BACKUP, ACTIVE_JSON))
      activeOk = readMetadataFile(ACTIVE_JSON, activeMetadata);
  }
  if (activeOk && SD.exists(ACTIVE_JSON_BACKUP))
    SD.remove(ACTIVE_JSON_BACKUP);
  if (!activeOk) memset(&activeMetadata, 0, sizeof(activeMetadata));
  if (SD.exists(TEMP_PDF)) SD.remove(TEMP_PDF);
  if (SD.exists(ACTIVE_JSON_TEMP)) SD.remove(ACTIVE_JSON_TEMP);
}

bool FLASHMEM tpExternalCalibrationActiveReady(void)
{
  return activeMetadata.present;
}

const TpExternalCalibrationMetadata& FLASHMEM tpExternalCalibrationActive(void)
{
  return activeMetadata;
}

static bool FLASHMEM validDocumentId(const char* documentId)
{
  if (documentId == nullptr || strlen(documentId) != 32U) return false;
  for (size_t i = 0U; i < 32U; ++i)
  {
    const char c = documentId[i];
    if (!((c >= '0' && c <= '9') ||
          (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F')))
      return false;
  }
  return true;
}

static bool FLASHMEM buildArchivePath(char* output,
                                      size_t outputSize,
                                      const char* documentId,
                                      const char* extension)
{
  if (output == nullptr || outputSize == 0U || extension == nullptr ||
      !validDocumentId(documentId)) return false;
  const int written = snprintf(output, outputSize, "%s/%.32s%s",
                               EXTERNAL_DIR, documentId, extension);
  if (written <= 0 || (size_t)written >= outputSize)
  {
    output[0] = '\0';
    return false;
  }
  return true;
}

bool FLASHMEM tpExternalCalibrationArchivedPdfPresent(const char* documentId)
{
  if (!validDocumentId(documentId) || !ensureDirectories()) return false;
  if (!buildArchivePath(finalPdfPath, sizeof(finalPdfPath), documentId, ".pdf") ||
      !buildArchivePath(finalMetadataPath, sizeof(finalMetadataPath), documentId, ".json") ||
      !SD.exists(finalPdfPath) || !SD.exists(finalMetadataPath))
    return false;

  File pdf = SD.open(finalPdfPath, FILE_READ);
  if (!pdf || pdf.isDirectory())
  {
    if (pdf) pdf.close();
    return false;
  }
  const uint32_t size = (uint32_t)pdf.size();
  pdf.close();
  return size >= 5U && size <= TP_EXTERNAL_CAL_MAX_PDF_BYTES;
}

bool FLASHMEM tpExternalCalibrationLoadArchived(
    const char* documentId,
    TpExternalCalibrationMetadata& metadata,
    char* errorText,
    size_t errorTextSize)
{
  memset(&metadata, 0, sizeof(metadata));
  setError(errorText, errorTextSize, TEXT_EMPTY);
  if (!validDocumentId(documentId))
  {
    setError(errorText, errorTextSize, "PDF-Dokument-ID ist ungültig");
    return false;
  }
  if (!ensureDirectories())
  {
    setError(errorText, errorTextSize, ERROR_SD_NOT_READY);
    return false;
  }
  if (!buildArchivePath(finalMetadataPath, sizeof(finalMetadataPath), documentId, ".json") ||
      !readMetadataFile(finalMetadataPath, metadata) ||
      strcasecmp(metadata.documentId, documentId) != 0)
  {
    memset(&metadata, 0, sizeof(metadata));
    setError(errorText, errorTextSize,
             "Archivierter externer Kalibrierschein ist nicht vorhanden oder wurde verändert");
    return false;
  }
  return true;
}

bool FLASHMEM tpExternalCalibrationBeginUpload(const char* certificateNumber,
                                      uint32_t validFromYmd,
                                      uint32_t validUntilYmd,
                                      const char* originalFileName,
                                      uint32_t expectedSize,
                                      char* errorText,
                                      size_t errorTextSize)
{
  setError(errorText, errorTextSize, TEXT_EMPTY);
  clearUploadState(true);
  if (!ensureDirectories())
  {
    setError(errorText, errorTextSize, ERROR_SD_NOT_READY);
    return false;
  }
  if (!validCertificateNumber(certificateNumber))
  {
    setError(errorText, errorTextSize, ERROR_CERTIFICATE_NUMBER);
    return false;
  }
  if (!validYmd(validFromYmd) || !validYmd(validUntilYmd) || validUntilYmd < validFromYmd)
  {
    setError(errorText, errorTextSize, ERROR_VALIDITY);
    return false;
  }
  if (expectedSize < 5U || expectedSize > TP_EXTERNAL_CAL_MAX_PDF_BYTES)
  {
    setError(errorText, errorTextSize, ERROR_PDF_SIZE);
    return false;
  }
  char safeName[81];
  if (!sanitizeFileName(originalFileName, safeName, sizeof(safeName)))
  {
    setError(errorText, errorTextSize, ERROR_PDF_FILENAME);
    return false;
  }

  SD.remove(TEMP_PDF);
  uploadFile = SD.open(TEMP_PDF, FILE_WRITE);
  if (!uploadFile)
  {
    setError(errorText, errorTextSize, ERROR_TEMP_CREATE);
    return false;
  }

  uploadActive = true;
  uploadExpectedSize = expectedSize;
  uploadReceivedSize = 0U;
  setText(uploadCertificateNumber, sizeof(uploadCertificateNumber), certificateNumber);
  uploadValidFromYmd = validFromYmd;
  uploadValidUntilYmd = validUntilYmd;
  setText(uploadOriginalFileName, sizeof(uploadOriginalFileName), safeName);
  uploadSha256.reset();
  return true;
}

bool FLASHMEM tpExternalCalibrationAppendChunk(uint32_t offset,
                                      const uint8_t* data,
                                      size_t length,
                                      char* errorText,
                                      size_t errorTextSize)
{
  setError(errorText, errorTextSize, TEXT_EMPTY);
  if (!uploadActive || !uploadFile)
  {
    setError(errorText, errorTextSize, ERROR_NO_UPLOAD);
    return false;
  }
  if (data == nullptr || length == 0U || offset != uploadReceivedSize ||
      uploadReceivedSize + length > uploadExpectedSize ||
      uploadReceivedSize + length > TP_EXTERNAL_CAL_MAX_PDF_BYTES)
  {
    setError(errorText, errorTextSize, ERROR_CHUNK_ORDER);
    clearUploadState(true);
    return false;
  }
  if (offset == 0U)
  {
    if (length < 5U || memcmp(data, PDF_MAGIC, 5U) != 0)
    {
      setError(errorText, errorTextSize, ERROR_PDF_MAGIC);
      clearUploadState(true);
      return false;
    }
  }
  const size_t written = uploadFile.write(data, length);
  if (written != length)
  {
    setError(errorText, errorTextSize, ERROR_CHUNK_WRITE);
    clearUploadState(true);
    return false;
  }
  uploadSha256.update(data, length);
  uploadReceivedSize += (uint32_t)length;
  if ((uploadReceivedSize & 0xFFFFU) == 0U) uploadFile.flush();
  return true;
}

bool FLASHMEM tpExternalCalibrationFinishUpload(char* resultText,
                                       size_t resultTextSize,
                                       char* errorText,
                                       size_t errorTextSize)
{
  setText(resultText, resultTextSize, TEXT_EMPTY);
  setError(errorText, errorTextSize, TEXT_EMPTY);
  if (!uploadActive || !uploadFile)
  {
    setError(errorText, errorTextSize, ERROR_NO_UPLOAD);
    return false;
  }
  if (uploadReceivedSize != uploadExpectedSize)
  {
    setError(errorText, errorTextSize, ERROR_UPLOAD_INCOMPLETE);
    clearUploadState(true);
    return false;
  }

  uploadFile.flush();
  uploadFile.close();
  uint8_t streamedDigest[32];
  uploadSha256.final(streamedDigest);

  uint8_t verifiedDigest[32];
  uint32_t verifiedSize = 0U;
  if (!hashFile(TEMP_PDF, verifiedDigest, &verifiedSize) ||
      verifiedSize != uploadExpectedSize ||
      memcmp(streamedDigest, verifiedDigest, 32U) != 0)
  {
    setError(errorText, errorTextSize, ERROR_REHASH);
    clearUploadState(true);
    return false;
  }

  TpExternalCalibrationMetadata& metadata = metadataWorkA;
  memset(&metadata, 0, sizeof(metadata));
  metadata.present = true;
  bytesToHex(verifiedDigest, 32U, hashHexWork, sizeof(hashHexWork));
  for (size_t i = 0U; i < 16U; ++i)
  {
    metadata.documentId[i * 2U] = hashHexWork[i * 2U];
    metadata.documentId[i * 2U + 1U] = hashHexWork[i * 2U + 1U];
  }
  metadata.documentId[32] = '\0';
  setText(metadata.certificateNumber, sizeof(metadata.certificateNumber), uploadCertificateNumber);
  metadata.validFromYmd = uploadValidFromYmd;
  metadata.validUntilYmd = uploadValidUntilYmd;
  setText(metadata.originalFileName, sizeof(metadata.originalFileName), uploadOriginalFileName);
  metadata.fileSize = uploadExpectedSize;
  setText(metadata.sha256, sizeof(metadata.sha256), hashHexWork);

  if (!buildArchivePath(finalPdfPath, sizeof(finalPdfPath), metadata.documentId, ".pdf") ||
      !buildArchivePath(finalMetadataPath, sizeof(finalMetadataPath), metadata.documentId, ".json"))
  {
    setError(errorText, errorTextSize, ERROR_ARCHIVE_RENAME);
    clearUploadState(true);
    return false;
  }
  setText(metadata.storedPdfPath, sizeof(metadata.storedPdfPath), finalPdfPath);

  const bool pdfAlreadyExisted = SD.exists(finalPdfPath);
  if (pdfAlreadyExisted)
  {
    uint8_t existingDigest[32];
    uint32_t existingSize = 0U;
    if (!hashFile(finalPdfPath, existingDigest, &existingSize) ||
        existingSize != metadata.fileSize || memcmp(existingDigest, verifiedDigest, 32U) != 0)
    {
      setError(errorText, errorTextSize, ERROR_ARCHIVE_CONFLICT);
      clearUploadState(true);
      return false;
    }
    SD.remove(TEMP_PDF);
  }
  else if (!SD.rename(TEMP_PDF, finalPdfPath))
  {
    setError(errorText, errorTextSize, ERROR_ARCHIVE_RENAME);
    clearUploadState(true);
    return false;
  }

  // Ein vorhandener Dokumentdatensatz wird niemals mit abweichenden
  // Metadaten überschrieben. So bleibt das SD-Archiv append-only, auch wenn
  // dieselben PDF-Bytes später versehentlich mit anderer Nummer oder
  // Gültigkeit erneut hochgeladen werden.
  bool documentMetadataReady = false;
  if (SD.exists(finalMetadataPath))
  {
    TpExternalCalibrationMetadata& existingMetadata = metadataWorkB;
    memset(&existingMetadata, 0, sizeof(existingMetadata));
    documentMetadataReady = readMetadataFile(finalMetadataPath, existingMetadata) &&
        strcmp(existingMetadata.documentId, metadata.documentId) == 0 &&
        strcmp(existingMetadata.certificateNumber, metadata.certificateNumber) == 0 &&
        existingMetadata.validFromYmd == metadata.validFromYmd &&
        existingMetadata.validUntilYmd == metadata.validUntilYmd &&
        strcmp(existingMetadata.originalFileName, metadata.originalFileName) == 0 &&
        strcmp(existingMetadata.storedPdfPath, metadata.storedPdfPath) == 0 &&
        existingMetadata.fileSize == metadata.fileSize &&
        strcasecmp(existingMetadata.sha256, metadata.sha256) == 0;
    if (!documentMetadataReady)
    {
      setError(errorText, errorTextSize, ERROR_METADATA_CONFLICT);
      clearUploadState(false);
      return false;
    }
  }

  size_t metadataLength = 0U;
  if (!buildMetadataJson(metadata, &metadataLength) ||
      (!documentMetadataReady && !writeTextFile(finalMetadataPath, metadataJson, metadataLength)) ||
      !writeTextFile(ACTIVE_JSON_TEMP, metadataJson, metadataLength))
  {
    if (!documentMetadataReady) SD.remove(finalMetadataPath);
    if (!pdfAlreadyExisted && !documentMetadataReady) SD.remove(finalPdfPath);
    setError(errorText, errorTextSize, ERROR_METADATA_WRITE);
    clearUploadState(false);
    return false;
  }
  if (SD.exists(ACTIVE_JSON_BACKUP)) SD.remove(ACTIVE_JSON_BACKUP);
  const bool hadActiveMetadata = SD.exists(ACTIVE_JSON);
  if (hadActiveMetadata && !SD.rename(ACTIVE_JSON, ACTIVE_JSON_BACKUP))
  {
    SD.remove(ACTIVE_JSON_TEMP);
    setError(errorText, errorTextSize, ERROR_ACTIVE_BACKUP);
    clearUploadState(false);
    return false;
  }
  if (!SD.rename(ACTIVE_JSON_TEMP, ACTIVE_JSON))
  {
    SD.remove(ACTIVE_JSON_TEMP);
    if (hadActiveMetadata && SD.exists(ACTIVE_JSON_BACKUP))
      SD.rename(ACTIVE_JSON_BACKUP, ACTIVE_JSON);
    setError(errorText, errorTextSize, ERROR_ACTIVE_SWITCH);
    clearUploadState(false);
    return false;
  }

  // Den neuen aktiven Zeiger nochmals von SD laden. Erst danach wird das
  // Backup gelöscht. Damit bleibt bei Schreib- oder Dateisystemfehlern der
  // bisherige externe Kalibrierschein aktiv.
  TpExternalCalibrationMetadata& verifiedMetadata = metadataWorkB;
  memset(&verifiedMetadata, 0, sizeof(verifiedMetadata));
  const bool activePointerOk = readMetadataFile(ACTIVE_JSON, verifiedMetadata) &&
      strcmp(verifiedMetadata.documentId, metadata.documentId) == 0 &&
      strcmp(verifiedMetadata.certificateNumber, metadata.certificateNumber) == 0 &&
      verifiedMetadata.validFromYmd == metadata.validFromYmd &&
      verifiedMetadata.validUntilYmd == metadata.validUntilYmd &&
      strcmp(verifiedMetadata.originalFileName, metadata.originalFileName) == 0 &&
      strcmp(verifiedMetadata.storedPdfPath, metadata.storedPdfPath) == 0 &&
      verifiedMetadata.fileSize == metadata.fileSize &&
      strcasecmp(verifiedMetadata.sha256, metadata.sha256) == 0;
  if (!activePointerOk)
  {
    SD.remove(ACTIVE_JSON);
    if (hadActiveMetadata && SD.exists(ACTIVE_JSON_BACKUP))
      SD.rename(ACTIVE_JSON_BACKUP, ACTIVE_JSON);
    memset(&activeMetadata, 0, sizeof(activeMetadata));
    (void)readMetadataFile(ACTIVE_JSON, activeMetadata);
    setError(errorText, errorTextSize, ERROR_ACTIVE_VERIFY);
    clearUploadState(false);
    return false;
  }

  if (SD.exists(ACTIVE_JSON_BACKUP)) SD.remove(ACTIVE_JSON_BACKUP);
  activeMetadata = verifiedMetadata;
  clearUploadState(false);
  tpSignedCalibrationInvalidateCache();
  if (resultText != nullptr && resultTextSize > 0U)
  {
    snprintf(resultText, resultTextSize,
             RESULT_FORMAT,
             activeMetadata.certificateNumber,
             (unsigned long)activeMetadata.fileSize,
             activeMetadata.sha256);
  }
  return true;
}

void FLASHMEM tpExternalCalibrationAbortUpload(void)
{
  clearUploadState(true);
}

bool FLASHMEM tpExternalCalibrationMatches(const char* documentId,
                                  const char* certificateNumber,
                                  uint32_t validFromYmd,
                                  uint32_t validUntilYmd,
                                  const char* originalFileName,
                                  uint32_t fileSize,
                                  const char* sha256Hex)
{
  if (!activeMetadata.present) return false;
  return documentId != nullptr && certificateNumber != nullptr && originalFileName != nullptr && sha256Hex != nullptr &&
         strcmp(activeMetadata.documentId, documentId) == 0 &&
         strcmp(activeMetadata.certificateNumber, certificateNumber) == 0 &&
         activeMetadata.validFromYmd == validFromYmd &&
         activeMetadata.validUntilYmd == validUntilYmd &&
         strcmp(activeMetadata.originalFileName, originalFileName) == 0 &&
         activeMetadata.fileSize == fileSize &&
         strcasecmp(activeMetadata.sha256, sha256Hex) == 0;
}

bool FLASHMEM tpExternalCalibrationArchivedMatches(
    const char* documentId,
    const char* certificateNumber,
    uint32_t validFromYmd,
    uint32_t validUntilYmd,
    const char* originalFileName,
    uint32_t fileSize,
    const char* sha256Hex)
{
  if (documentId == nullptr || certificateNumber == nullptr ||
      originalFileName == nullptr || sha256Hex == nullptr)
    return false;
  if (archivedMatchCacheValid &&
      strcmp(archivedMatchDocumentId, documentId) == 0 &&
      strcmp(archivedMatchCertificateNumber, certificateNumber) == 0 &&
      archivedMatchValidFromYmd == validFromYmd &&
      archivedMatchValidUntilYmd == validUntilYmd &&
      strcmp(archivedMatchOriginalFileName, originalFileName) == 0 &&
      archivedMatchFileSize == fileSize &&
      strcasecmp(archivedMatchSha256, sha256Hex) == 0)
    return true;

  TpExternalCalibrationMetadata& metadata = metadataWorkA;
  char errorText[96];
  if (!tpExternalCalibrationLoadArchived(documentId, metadata,
                                         errorText, sizeof(errorText)))
    return false;
  if (!metadata.present ||
      strcmp(metadata.certificateNumber, certificateNumber) != 0 ||
      metadata.validFromYmd != validFromYmd ||
      metadata.validUntilYmd != validUntilYmd ||
      strcmp(metadata.originalFileName, originalFileName) != 0 ||
      metadata.fileSize != fileSize ||
      strcasecmp(metadata.sha256, sha256Hex) != 0)
    return false;

  File pdf = SD.open(metadata.storedPdfPath, FILE_READ);
  if (!pdf || pdf.isDirectory())
  {
    if (pdf) pdf.close();
    return false;
  }
  uint8_t magic[5];
  const int magicRead = pdf.read(magic, sizeof(magic));
  const uint32_t actualSize = (uint32_t)pdf.size();
  pdf.close();
  if (magicRead != (int)sizeof(magic) || memcmp(magic, PDF_MAGIC, sizeof(magic)) != 0 ||
      actualSize != fileSize)
    return false;

  uint8_t digest[32];
  uint32_t hashedSize = 0U;
  if (!hashFile(metadata.storedPdfPath, digest, &hashedSize) || hashedSize != fileSize)
    return false;
  bytesToHex(digest, sizeof(digest), hashHexWork, sizeof(hashHexWork));
  if (strcasecmp(hashHexWork, sha256Hex) != 0) return false;
  setText(archivedMatchDocumentId, sizeof(archivedMatchDocumentId),
          documentId);
  setText(archivedMatchCertificateNumber,
          sizeof(archivedMatchCertificateNumber), certificateNumber);
  archivedMatchValidFromYmd = validFromYmd;
  archivedMatchValidUntilYmd = validUntilYmd;
  setText(archivedMatchOriginalFileName,
          sizeof(archivedMatchOriginalFileName), originalFileName);
  setText(archivedMatchSha256, sizeof(archivedMatchSha256), sha256Hex);
  archivedMatchFileSize = fileSize;
  archivedMatchCacheValid = true;
  return true;
}
