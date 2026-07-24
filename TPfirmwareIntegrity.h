/*
 * TP-3000 firmware image integrity and root-signed firmware certificate
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_FIRMWARE_INTEGRITY_H
#define TP3000_FIRMWARE_INTEGRITY_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

enum TpFirmwareIntegrityState : uint8_t
{
  TP_FW_INTEGRITY_NOT_RUN = 0U,
  TP_FW_INTEGRITY_CERTIFICATE_MISSING = 1U,
  TP_FW_INTEGRITY_VERIFIED = 2U,
  TP_FW_INTEGRITY_MISMATCH = 3U,
  TP_FW_INTEGRITY_ERROR = 4U
};

// Legacy alias retained for existing display code while V0.50.1_26 migrates
// from the post-link embedded manifest to an external root-signed certificate.
#define TP_FW_INTEGRITY_UNSEALED TP_FW_INTEGRITY_CERTIFICATE_MISSING

struct TpFirmwareIntegrityStatus
{
  TpFirmwareIntegrityState state;
  bool hashCalculated;
  bool certificatePresent;
  bool certificateSignatureValid;
  bool certificateMatchesImage;
  // Compatibility fields used by the signed-data foundation.
  bool manifestPresent;
  bool manifestFinalized;
  bool manifestCrcValid;
  bool imageSizeMatches;
  uint32_t imageBase;
  uint32_t expectedImageSize;
  uint32_t measuredImageSize;
  uint8_t expectedSha256[32];
  uint8_t measuredSha256[32];
  int64_t verifiedUtc;
  int64_t approvedUtc;
  char firmwareCertificateId[64];
  char rootKeyId[17];
  char lastCertificateFile[128];
  char statusText[96];
};

// Computes SHA-256 over the exact linked Teensy image. No expected digest is
// embedded in the image and no post-link patching step is required.
void tpFirmwareIntegrityBegin(void);

// Recovery path after a stored Teensy CrashReport. The potentially faulty
// image read is skipped for this one boot so the device can start and the
// report can be recovered from SD. Sensitive certified functions remain locked.
void tpFirmwareIntegritySkipAfterBootCrash(void);

// Linker-provided size of the exact image starting at 0x60000000.
uint32_t tpFirmwareIntegrityLinkedImageSize(void);

// Called after RTC and SD are ready. Loads and verifies the root-signed active
// firmware certificate from /CERTIFICATION/FIRMWARE.TPFCERT.
void tpFirmwareIntegrityFinalizeStartup(int64_t verifiedUtc);

const TpFirmwareIntegrityStatus& tpFirmwareIntegrityStatus(void);
bool tpFirmwareIntegrityVerified(void);
bool tpFirmwareIntegrityManifestFinalized(void);
bool tpFirmwareIntegrityAllowsSensitiveOperations(void);
const char* tpFirmwareIntegrityStatusText(void);

bool tpFirmwareIntegrityGetExpectedHash(uint8_t out[32]);
bool tpFirmwareIntegrityGetMeasuredHash(uint8_t out[32]);
void tpFirmwareIntegrityExpectedHashHex(char* out, size_t outSize);
void tpFirmwareIntegrityMeasuredHashHex(char* out, size_t outSize);

// Firmware certificate workflow.
bool tpFirmwareIntegrityBuildRequestJson(char* output,
                                         size_t outputSize,
                                         size_t* outputLength,
                                         char* downloadName,
                                         size_t downloadNameSize,
                                         char* errorText,
                                         size_t errorTextSize);
bool tpFirmwareIntegritySaveRequestToSd(const char* json,
                                        size_t jsonLength,
                                        const char* downloadName,
                                        char* storedPath,
                                        size_t storedPathSize,
                                        char* errorText,
                                        size_t errorTextSize);
bool tpFirmwareIntegrityImportCertificateJson(const char* json,
                                              size_t jsonLength,
                                              char* errorText,
                                              size_t errorTextSize);
bool tpFirmwareIntegrityImportCertificateFromSd(char* importedPath,
                                                size_t importedPathSize,
                                                char* errorText,
                                                size_t errorTextSize);
bool tpFirmwareIntegrityCreateRequestOnSd(char* storedPath,
                                          size_t storedPathSize,
                                          char* message,
                                          size_t messageSize);

// Writes /CERTIFICATION/FIRMWARE.TPS transactionally when SD is available.
bool tpFirmwareIntegrityWriteStatusToSd(void);

#endif
