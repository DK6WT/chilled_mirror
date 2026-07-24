/*
 * TP-3000 firmware context bound to a system-calibration request
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_SYSTEM_FIRMWARE_CONTEXT_H
#define TP3000_SYSTEM_FIRMWARE_CONTEXT_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

enum TpSystemFirmwareCaptureMode : uint8_t
{
  TP_SYSTEM_FW_CAPTURE_NONE = 0U,
  TP_SYSTEM_FW_CAPTURE_REQUEST = 1U,
  TP_SYSTEM_FW_CAPTURE_ACTIVATION_FALLBACK = 2U
};

enum TpSystemFirmwareManufacturerStatus : uint8_t
{
  TP_SYSTEM_FW_MANUFACTURER_NOT_CERTIFIED = 0U,
  TP_SYSTEM_FW_MANUFACTURER_VALID = 1U,
  TP_SYSTEM_FW_MANUFACTURER_MISMATCH = 2U,
  TP_SYSTEM_FW_MANUFACTURER_INVALID = 3U
};

enum TpSystemFirmwareApplicability : uint8_t
{
  TP_SYSTEM_FW_APPLICABILITY_NOT_COMPARABLE = 0U,
  TP_SYSTEM_FW_APPLICABILITY_EXACT_MATCH = 1U,
  TP_SYSTEM_FW_APPLICABILITY_CHANGED = 2U
};

struct TpSystemFirmwareContextStatus
{
  bool filePresent;
  bool recordValid;
  bool requestBindingValid;
  bool currentHashAvailable;
  bool currentImageMatches;
  TpSystemFirmwareApplicability applicability;
  TpSystemFirmwareCaptureMode captureMode;
  TpSystemFirmwareManufacturerStatus manufacturerStatus;
  int64_t capturedUtc;
  int64_t certificateApprovedUtc;
  uint32_t imageBase;
  uint32_t imageSize;
  char sourceRequestId[33];
  char sourceRequestManifestSha256[65];
  char firmwareVersion[24];
  char firmwareBuildId[32];
  char firmwareSha256[65];
  char firmwareCertificateId[64];
  char manufacturerRootKeyId[17];
  char contextManifestSha256[65];
  char deviceSignatureHex[129];
  char sourceFile[128];
  char lastError[160];
};

// Captures the exact running image hash and the optional manufacturer
// certificate state at the moment a system-calibration request is created.
// The record is signed by the individual device key and bound to the request
// manifest which is later included in the signed system calibration.
bool tpSystemFirmwareContextCaptureRequest(
    const char* sourceRequestId,
    const uint8_t sourceRequestManifestSha256[32],
    int64_t capturedUtc,
    char* errorText,
    size_t errorTextSize);

// Compatibility path for an older request that has no context record yet.
// A fallback is written only when the imported package names exactly the
// currently running firmware version/build; otherwise no historical hash is
// invented.
bool tpSystemFirmwareContextEnsureForActivation(
    const char* sourceRequestId,
    const char* sourceRequestManifestSha256,
    const char* packageFirmwareVersion,
    const char* packageFirmwareBuildId,
    int64_t capturedUtc);

// Loads and verifies the device-signed context. A missing historical context is
// reported as not comparable. If the exact running image hash differs from the
// hash captured with the system calibration, the signed calibration document
// remains valid; the deviation to the documented calibration state is reported
// separately and included in the evidence export.
bool tpSystemFirmwareContextLoad(
    const char* sourceRequestId,
    const char* sourceRequestManifestSha256,
    TpSystemFirmwareContextStatus& status);

#endif
