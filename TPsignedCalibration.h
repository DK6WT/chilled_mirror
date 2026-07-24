/*
 * TP-3000 signed calibration workflow
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_SIGNED_CALIBRATION_H
#define TP3000_SIGNED_CALIBRATION_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include "TPqrCode.h"

enum TpCalibrationTimeStatus : uint8_t
{
  TP_CAL_TIME_UNKNOWN = 0U,
  TP_CAL_TIME_NOT_YET_VALID = 1U,
  TP_CAL_TIME_VALID = 2U,
  TP_CAL_TIME_EXPIRED = 3U
};

struct TpSignedCalibrationStatus
{
  bool filePresent;
  bool signatureValid;
  bool valuesMatchCurrent;
  char calibrationId[97];
  uint32_t calibrationDateYmd;
  int64_t validFromUtc;
  int64_t validUntilUtc;
  uint16_t intervalMonths;
  char signerKeyId[17];
  char manifestSha256[65];
  char sourceFile[96];
  char lastError[128];
};

struct TpSystemCalibrationStatus
{
  bool filePresent;
  bool signatureValid;
  bool bindingMatchesCurrent;
  char calibrationId[97];
  uint32_t calibrationDateYmd;
  int64_t validFromUtc;
  int64_t validUntilUtc;
  uint16_t intervalMonths;
  char signerKeyId[17];
  char manifestSha256[65];
  char sourceRequestId[33];
  char sourceRequestManifestSha256[65];
  char firmwareVersion[24];
  char firmwareBuildId[32];
  char headType[16];
  uint32_t headSerial;
  char deviceAdjustmentCalibrationId[97];
  char headAdjustmentCalibrationId[97];
  char certificateIssuer[161];
  char operatorName[97];
  char certificateReference[161];
  char calibrationScope[40];
  char certificateSource[24];
  bool externalCertificateBound;
  bool applicabilityStateKnown;
  bool applicabilityEnded;
  int64_t applicableUntilUtc;
  char applicabilityReason[40];
  char externalCertificateDocumentId[33];
  char externalCertificateNumber[33];
  uint32_t externalCertificateValidFromYmd;
  uint32_t externalCertificateValidUntilYmd;
  char externalCertificateOriginalFileName[81];
  uint32_t externalCertificateFileSize;
  char externalCertificateSha256[65];
  char sourceFile[96];
  char lastError[160];
};

// Kompakter Indexeintrag für die Kalibrierzertifikat-Suche im Web. Die
// eigentlichen signierten JSON-Pakete bleiben unverändert im SD-Archiv und
// werden erst beim Öffnen eines Treffers geladen.
struct TpCalibrationCertificateRecord
{
  bool active;
  bool activeBindingValid;
  bool externalCertificateBound;
  bool applicabilityStateKnown;
  bool applicabilityEnded;
  int64_t applicableUntilUtc;
  char applicabilityReason[40];
  char systemManifestSha256[65];
  char calibrationId[97];
  uint32_t calibrationDateYmd;
  uint32_t validFromYmd;
  uint32_t validUntilYmd;
  char certificateSource[24];
  char externalCertificateDocumentId[33];
  char externalCertificateNumber[33];
};

void tpSignedCalibrationBegin(void);
void tpSignedCalibrationInvalidateCache(void);

// Beendet die Anwendbarkeit der aktuell geladenen Systemkalibrierung.
// Der signierte JSON-Bereich bleibt unverändert und valide. Zeitpunkt und
// Ereigniscode werden zentral in /CALIBRATION/APPLICABILITY.TPS gespeichert
// und zusätzlich hinter dem signierten Bereich der betreffenden .tpscal
// transportabel angehängt.
bool tpSignedCalibrationEndCurrentSystemApplicability(
    const char* reasonCode,
    char* errorText,
    size_t errorTextSize);

// Liefert den zentral gespeicherten Anwendbarkeitsstatus zu einem
// Systemkalibrierungs-Manifest. Bei einem beendeten Schein wird der
// transportable Anhang in der aktiven/archivierten .tpscal best-effort
// nachgeführt.
bool tpSignedCalibrationGetApplicability(
    const char* manifestSha256,
    bool* stateKnown,
    bool* ended,
    int64_t* applicableUntilUtc,
    char* reasonCode,
    size_t reasonCodeSize);

bool tpSignedCalibrationDeviceReady(void);
bool tpSignedCalibrationHeadReady(void);
bool tpSignedCalibrationSystemReady(void);
const TpSignedCalibrationStatus& tpSignedCalibrationDeviceStatus(void);
const TpSignedCalibrationStatus& tpSignedCalibrationHeadStatus(void);
const TpSystemCalibrationStatus& tpSignedCalibrationSystemStatus(void);

// Liest den unveränderten signierten JSON-Bereich der aktiven Kalibrierdatei
// von SD. Ein transportabler Anwendbarkeitsanhang hinter dem Endmarker wird
// für Web-Kalibrierschein und Offline-QR bewusst nicht als JSON ausgegeben.
bool tpSignedCalibrationReadActiveJson(bool device,
                                       char* output,
                                       size_t outputSize,
                                       size_t* outputLength,
                                       char* errorText,
                                       size_t errorTextSize);

bool tpSignedCalibrationReadActiveSystemJson(char* output,
                                             size_t outputSize,
                                             size_t* outputLength,
                                             char* errorText,
                                             size_t errorTextSize);

// Liest ein historisches, beim Import kryptografisch geprüftes Paket anhand
// seines 64-stelligen Manifest-SHA-256 aus dem append-only SD-Archiv. Vor der
// Ausgabe wird die Signatur erneut geprüft; eine Änderung auf der SD wird so
// nicht still als gültiger historischer Datensatz ausgeliefert.
bool tpSignedCalibrationReadArchivedAdjustmentJson(bool device,
                                                    const char* manifestSha256,
                                                    char* output,
                                                    size_t outputSize,
                                                    size_t* outputLength,
                                                    char* errorText,
                                                    size_t errorTextSize);

bool tpSignedCalibrationReadArchivedSystemJson(const char* manifestSha256,
                                                char* output,
                                                size_t outputSize,
                                                size_t* outputLength,
                                                char* errorText,
                                                size_t errorTextSize);

// Sucht Systemkalibrierungen, deren Gültigkeitszeitraum den angefragten
// Kalenderzeitraum überlappt:
//   ValidFrom <= SearchUntil && ValidUntil >= SearchFrom
// Die neuesten Treffer werden zuerst geliefert. totalMatches kann größer als
// maxRecords sein; in diesem Fall ist truncated=true.
bool tpSignedCalibrationSearchCertificates(uint32_t searchFromYmd,
                                           uint32_t searchUntilYmd,
                                           TpCalibrationCertificateRecord* records,
                                           size_t maxRecords,
                                           size_t* returnedRecords,
                                           size_t* totalMatches,
                                           bool* truncated,
                                           char* errorText,
                                           size_t errorTextSize);

bool tpSignedCalibrationGetActiveCertificateRecord(
    TpCalibrationCertificateRecord& record);

// Builds the two compact TP3C1 V0.4 QR payloads for the currently active
// device, head and system calibration. The function re-reads and re-verifies
// all signed source packages, reconstructs the same compact envelope used by
// the web certificate, optionally zlib-compresses it and returns raw
// alphanumeric strings beginning with "TP3C1:".
bool tpSignedCalibrationBuildActiveTp3c1Parts(
    char* part1,
    size_t part1Size,
    char* part2,
    size_t part2Size,
    char* documentId,
    size_t documentIdSize,
    uint16_t* envelopeSize,
    uint16_t* transportSize,
    bool* compressedUsed,
    char* errorText,
    size_t errorTextSize);

// Encodes one already built TP3C1 text with the canonical Android deep-link
// prefix "tp3000://verify#". The URI prefix is stored in QR byte mode and the
// TP3C1/Base38 part in QR alphanumeric mode. Shared calibration scratch
// buffers avoid permanent QR work arrays in RAM2.
bool tpSignedCalibrationEncodeTp3c1Qr(
    const char* text,
    TpQrCode::Matrix& output,
    char* errorText,
    size_t errorTextSize);

// Streams the already verified active package to a Print target without an
// additional large caller-side RAM buffer. Used by certified log context files.
bool tpSignedCalibrationCopyActiveJsonTo(Print& output,
                                         bool device,
                                         char* errorText,
                                         size_t errorTextSize);

bool tpSignedCalibrationCopyActiveSystemJsonTo(Print& output,
                                               char* errorText,
                                               size_t errorTextSize);

TpCalibrationTimeStatus tpSignedCalibrationTimeStatus(
    const TpSignedCalibrationStatus& status,
    int64_t unixTimeUtc);
const char* tpSignedCalibrationTimeStatusText(TpCalibrationTimeStatus status,
                                              bool english);

bool tpSignedCalibrationBuildDeviceRequestJson(char* output,
                                               size_t outputSize,
                                               size_t* outputLength,
                                               char* downloadName,
                                               size_t downloadNameSize,
                                               char* errorText,
                                               size_t errorTextSize);

bool tpSignedCalibrationBuildHeadRequestJson(char* output,
                                             size_t outputSize,
                                             size_t* outputLength,
                                             char* downloadName,
                                             size_t downloadNameSize,
                                             char* errorText,
                                             size_t errorTextSize);

bool tpSignedCalibrationBuildSystemRequestJson(char* output,
                                               size_t outputSize,
                                               size_t* outputLength,
                                               char* downloadName,
                                               size_t downloadNameSize,
                                               char* errorText,
                                               size_t errorTextSize);

bool tpSignedCalibrationSaveRequestToSd(const char* json,
                                        size_t jsonLength,
                                        const char* downloadName,
                                        char* storedPath,
                                        size_t storedPathSize,
                                        char* errorText,
                                        size_t errorTextSize);

bool tpSignedCalibrationCreateDeviceRequestOnSd(char* storedPath,
                                                 size_t storedPathSize,
                                                 char* errorText,
                                                 size_t errorTextSize);

bool tpSignedCalibrationCreateHeadRequestOnSd(char* storedPath,
                                               size_t storedPathSize,
                                               char* errorText,
                                               size_t errorTextSize);

bool tpSignedCalibrationCreateSystemRequestOnSd(char* storedPath,
                                                 size_t storedPathSize,
                                                 char* errorText,
                                                 size_t errorTextSize);

bool tpSignedCalibrationImportSystemJson(const char* json,
                                          size_t jsonLength,
                                          char* errorText,
                                          size_t errorTextSize);

// Direkter Web-Upload einer signierten Geräte- oder Kopfjustierung.
// Das komplette JSON-Paket wird vor der Aktivierung geprüft; es werden
// niemals private Schlüssel an das Gerät übertragen.
bool tpSignedCalibrationImportAdjustmentJson(bool device,
                                               const char* json,
                                               size_t jsonLength,
                                               char* errorText,
                                               size_t errorTextSize);

bool tpSignedCalibrationImportDeviceFromSd(char* importedPath,
                                           size_t importedPathSize,
                                           char* errorText,
                                           size_t errorTextSize);

bool tpSignedCalibrationImportHeadFromSd(char* importedPath,
                                         size_t importedPathSize,
                                         char* errorText,
                                         size_t errorTextSize);

bool tpSignedCalibrationImportSystemFromSd(char* importedPath,
                                           size_t importedPathSize,
                                           char* errorText,
                                           size_t errorTextSize);

#endif
