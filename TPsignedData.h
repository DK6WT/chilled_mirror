/*
 * TP-3000 signed-data foundation
 * Shared firmware-side constants and canonical byte writer for the
 * TP3000 signed-data format family V1.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_SIGNED_DATA_H
#define TP3000_SIGNED_DATA_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// Public firmware identity. The complete linked Teensy image is hashed at
// every startup. A manufacturer-root-signed firmware certificate supplies
// the expected digest; no post-link patching step is required.
#define TP_FIRMWARE_VERSION_STRING       "0.50.1"
#define TP_FIRMWARE_BUILD_ID_STRING      "0.50.1_87"
#define TP_FIRMWARE_BUILD_DATE_TEXT      "24.07.2026"
#define TP_FIRMWARE_BUILD_DATE_ISO       "2026-07-24"
#define TP_FIRMWARE_BUILD_DATE_YMD       20260724UL
#define TP_FIRMWARE_TARGET_HARDWARE      "Teensy 4.1"
#define TP_FIRMWARE_DEVELOPMENT_STATUS   "DEVELOPMENT"

#define TP_SIGNED_DATA_FORMAT_VERSION    1U

#define TP_FORMAT_ROLE_CERTIFICATE       "TP3000-SIGNING-ROLE-CERTIFICATE-1"
#define TP_FORMAT_DEVICE_CAL_REQUEST     "TP3000-DEVICE-CALIBRATION-REQUEST-1"
#define TP_FORMAT_DEVICE_CALIBRATION     "TP3000-DEVICE-CALIBRATION-3"
#define TP_FORMAT_DEVICE_CALIBRATION_V2  "TP3000-DEVICE-CALIBRATION-2"
#define TP_FORMAT_DEVICE_CALIBRATION_V1  "TP3000-DEVICE-CALIBRATION-1"
#define TP_FORMAT_HEAD_CAL_REQUEST       "TP3000-HEAD-CALIBRATION-REQUEST-1"
#define TP_FORMAT_HEAD_CALIBRATION       "TP3000-HEAD-CALIBRATION-3"
#define TP_FORMAT_HEAD_CALIBRATION_V2    "TP3000-HEAD-CALIBRATION-2"
#define TP_FORMAT_HEAD_CALIBRATION_V1    "TP3000-HEAD-CALIBRATION-1"
#define TP_FORMAT_SYSTEM_CAL_REQUEST_V1  "TP3000-SYSTEM-CALIBRATION-REQUEST-1"
#define TP_FORMAT_SYSTEM_CAL_REQUEST     "TP3000-SYSTEM-CALIBRATION-REQUEST-2"
#define TP_FORMAT_SYSTEM_CALIBRATION_V1  "TP3000-SYSTEM-CALIBRATION-1"
#define TP_FORMAT_SYSTEM_CALIBRATION     "TP3000-SYSTEM-CALIBRATION-2"
#define TP_FORMAT_FIRMWARE_REQUEST       "TP3000-FIRMWARE-CERTIFICATE-REQUEST-1"
#define TP_FORMAT_FIRMWARE_CERTIFICATE   "TP3000-FIRMWARE-CERTIFICATE-1"
#define TP_FORMAT_FIRMWARE_MANIFEST      "TP3000-FIRMWARE-MANIFEST-1" // legacy name used only by existing log evidence flags
#define TP_FORMAT_MEASUREMENT_CONFIG     "TP3000-MEASUREMENT-CONFIG-1"
#define TP_FORMAT_LOG_SIGNATURE_V1       "TP3000-LOG-SIGNATURE-1"
#define TP_FORMAT_LOG_SIGNATURE          "TP3000-LOG-SIGNATURE-2"
#define TP_FORMAT_LOG_CONTAINER          "TP3000-LOG-CONTAINER-1"

// Stored SD logging selection.
enum TpLogIntegrityMode : uint8_t
{
  TP_LOG_INTEGRITY_OFF = 0U,
  TP_LOG_INTEGRITY_SHA256 = 1U,
  TP_LOG_INTEGRITY_CERTIFIED_CSV = 2U,
  TP_LOG_INTEGRITY_CERTIFIED_TPLOG = 3U,
  TP_LOG_INTEGRITY_COUNT = 4U
};

static inline bool tpLogIntegrityIsCertified(uint8_t mode)
{
  return mode == TP_LOG_INTEGRITY_CERTIFIED_CSV ||
         mode == TP_LOG_INTEGRITY_CERTIFIED_TPLOG;
}

static inline bool tpLogIntegrityUsesTpLog(uint8_t mode)
{
  return mode == TP_LOG_INTEGRITY_CERTIFIED_TPLOG;
}

// Evidence-status bits. The firmware-certificate bit is informational;
// customer firmware remains usable when its SHA-256 is available.
enum TpSignedEvidenceMask : uint8_t
{
  TP_SIGNED_EVIDENCE_DEVICE_CERTIFICATE = 0x01U,
  TP_SIGNED_EVIDENCE_DEVICE_CALIBRATION = 0x02U,
  TP_SIGNED_EVIDENCE_HEAD_CALIBRATION   = 0x04U,
  TP_SIGNED_EVIDENCE_SYSTEM_CALIBRATION = 0x08U,
  TP_SIGNED_EVIDENCE_FIRMWARE_MANIFEST  = 0x10U
};

struct TpFirmwareRuntimeIdentity
{
  const char* product;
  const char* version;
  const char* buildId;
  uint32_t buildDateYmd;
  const char* targetHardware;
  const char* approvalStatus;
};

struct TpFirmwareApprovalStatus
{
  bool manifestPresent;
  bool signatureValid;
  uint32_t approvalDateYmd;
  int64_t validFromUtc;
  int64_t validUntilUtc;
  char signerKeyId[17];
  char statusText[96];
};

// Canonical little-endian writer used by calibration, firmware and log
// manifests. Strings are length-prefixed UTF-8 bytes. Callers are responsible
// for supplying already-normalized text (ASCII is used by current V1 fields).
class TpCanonicalWriter
{
public:
  TpCanonicalWriter(uint8_t* target, size_t targetSize);

  bool writeU8(uint8_t value);
  bool writeU16(uint16_t value);
  bool writeU32(uint32_t value);
  bool writeU64(uint64_t value);
  bool writeI32(int32_t value);
  bool writeI64(int64_t value);
  bool writeBytes(const void* source, size_t count);
  bool writeUtf8(const char* text);
  bool writeHash32(const uint8_t hash[32]);

  size_t length() const { return length_; }
  bool valid() const { return valid_; }

private:
  bool append(const void* source, size_t count);

  uint8_t* data_;
  size_t capacity_;
  size_t length_;
  bool valid_;
};

const TpFirmwareRuntimeIdentity& tpSignedDataFirmwareIdentity(void);

// These evidence interfaces are deliberately stable now. Later versions can
// replace their placeholder implementations with imported, verified manifests
// without changing menu, EEPROM or .TPSIG callers.
bool tpSignedDataDeviceCalibrationReady(void);
bool tpSignedDataHeadCalibrationReady(void);
bool tpSignedDataSystemCalibrationReady(void);
bool tpSignedDataFirmwareManifestReady(void);
const TpFirmwareApprovalStatus& tpSignedDataFirmwareApprovalStatus(void);
uint8_t tpSignedDataMissingEvidenceMask(void);
// Evidence readiness is independent from the output writer. Certified output
// requires a startup-calculated firmware SHA-256, but a matching manufacturer
// firmware certificate is optional so that customer firmware remains usable.
bool tpSignedDataCertifiedEvidenceReady(void);
bool tpSignedDataCertifiedOutputImplemented(void);
bool tpSignedDataCertifiedModeReady(void);
const char* tpSignedDataCertifiedReadinessText(bool english);

#endif
