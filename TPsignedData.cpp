/*
 * TP-3000 signed-data foundation
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPsignedData.h"
#include "TP_T.h"
#include "TPsignedCalibration.h"
#include "TPfirmwareIntegrity.h"
#include <string.h>
#include <TimeLib.h>

#if defined(__IMXRT1062__)
#define TP_SIGNED_DATA_RODATA __attribute__((section(".progmem")))
#else
#define TP_SIGNED_DATA_RODATA
#endif

static const char TP_SIGNED_PRODUCT[] TP_SIGNED_DATA_RODATA = "TP-3000";
static const char TP_SIGNED_VERSION[] TP_SIGNED_DATA_RODATA = TP_FIRMWARE_VERSION_STRING;
static const char TP_SIGNED_BUILD_ID[] TP_SIGNED_DATA_RODATA = TP_FIRMWARE_BUILD_ID_STRING;
static const char TP_SIGNED_TARGET[] TP_SIGNED_DATA_RODATA = TP_FIRMWARE_TARGET_HARDWARE;
static const char TP_SIGNED_STATUS[] TP_SIGNED_DATA_RODATA = TP_FIRMWARE_DEVELOPMENT_STATUS;
static const char TP_SIGNED_READY_DEVICE_CERT_EN[] TP_SIGNED_DATA_RODATA = "Device certificate missing";
static const char TP_SIGNED_READY_DEVICE_CERT_DE[] TP_SIGNED_DATA_RODATA = "Geraetezertifikat fehlt";
static const char TP_SIGNED_READY_DEVICE_CAL_EN[] TP_SIGNED_DATA_RODATA = "Signed device calibration missing";
static const char TP_SIGNED_READY_DEVICE_CAL_DE[] TP_SIGNED_DATA_RODATA = "Signierte Geraetejustierung fehlt";
static const char TP_SIGNED_READY_HEAD_CAL_EN[] TP_SIGNED_DATA_RODATA = "Signed head calibration missing";
static const char TP_SIGNED_READY_HEAD_CAL_DE[] TP_SIGNED_DATA_RODATA = "Signierte Kopfjustierung fehlt";
static const char TP_SIGNED_READY_SYSTEM_CAL_EN[] TP_SIGNED_DATA_RODATA = "Signed system calibration missing";
static const char TP_SIGNED_READY_SYSTEM_CAL_DE[] TP_SIGNED_DATA_RODATA = "Signierte Systemkalibrierung fehlt";
static const char TP_SIGNED_READY_FIRMWARE_EN[] TP_SIGNED_DATA_RODATA = "Ready - no manufacturer firmware certificate; operator validation required";
static const char TP_SIGNED_READY_FIRMWARE_DE[] TP_SIGNED_DATA_RODATA = "Bereit - kein Hersteller-Firmwarezertifikat; Validierung durch Betreiber";
static const char TP_SIGNED_READY_FIRMWARE_HASH_EN[] TP_SIGNED_DATA_RODATA = "Firmware SHA-256 not available; restart device cleanly";
static const char TP_SIGNED_READY_FIRMWARE_HASH_DE[] TP_SIGNED_DATA_RODATA = "Firmware-SHA-256 fehlt; Geraet sauber neu starten";
static const char TP_SIGNED_READY_OUTPUT_EN[] TP_SIGNED_DATA_RODATA = ".TPSIG output not active yet";
static const char TP_SIGNED_READY_OUTPUT_DE[] TP_SIGNED_DATA_RODATA = ".TPSIG-Erzeugung noch nicht aktiv";
static const char TP_SIGNED_READY_DEVELOPMENT_EN[] TP_SIGNED_DATA_RODATA = "Ready (firmware development status)";
static const char TP_SIGNED_READY_DEVELOPMENT_DE[] TP_SIGNED_DATA_RODATA = "Bereit (Firmware DEVELOPMENT)";
static const TpFirmwareRuntimeIdentity TP_SIGNED_FIRMWARE_IDENTITY TP_SIGNED_DATA_RODATA =
{
  TP_SIGNED_PRODUCT,
  TP_SIGNED_VERSION,
  TP_SIGNED_BUILD_ID,
  TP_FIRMWARE_BUILD_DATE_YMD,
  TP_SIGNED_TARGET,
  TP_SIGNED_STATUS
};

static DMAMEM TpFirmwareApprovalStatus tpSignedFirmwareApproval;

static void FLASHMEM tpSignedDataRefreshFirmwareApproval(void)
{
  const TpFirmwareIntegrityStatus& integrity = tpFirmwareIntegrityStatus();
  memset(&tpSignedFirmwareApproval, 0, sizeof(tpSignedFirmwareApproval));
  tpSignedFirmwareApproval.manifestPresent = integrity.certificatePresent;
  tpSignedFirmwareApproval.signatureValid = tpFirmwareIntegrityVerified();
  tpSignedFirmwareApproval.approvalDateYmd = TP_FIRMWARE_BUILD_DATE_YMD;
  tpSignedFirmwareApproval.validFromUtc = integrity.approvedUtc;
  tpSignedFirmwareApproval.validUntilUtc = 0;
  if (integrity.approvedUtc > 0)
  {
    const time_t approved = (time_t)integrity.approvedUtc;
    tpSignedFirmwareApproval.approvalDateYmd =
      (uint32_t)year(approved) * 10000UL +
      (uint32_t)month(approved) * 100UL +
      (uint32_t)day(approved);
  }
  strncpy(tpSignedFirmwareApproval.signerKeyId, integrity.rootKeyId,
          sizeof(tpSignedFirmwareApproval.signerKeyId) - 1U);
  tpSignedFirmwareApproval.signerKeyId[sizeof(tpSignedFirmwareApproval.signerKeyId) - 1U] = '\0';
  strncpy(tpSignedFirmwareApproval.statusText,
          tpFirmwareIntegrityStatusText(),
          sizeof(tpSignedFirmwareApproval.statusText) - 1U);
  tpSignedFirmwareApproval.statusText[sizeof(tpSignedFirmwareApproval.statusText) - 1U] = '\0';
}

TpCanonicalWriter::TpCanonicalWriter(uint8_t* target, size_t targetSize)
  : data_(target), capacity_(targetSize), length_(0U), valid_(target != nullptr)
{
}

bool FLASHMEM TpCanonicalWriter::append(const void* source, size_t count)
{
  if (!valid_ || (source == nullptr && count != 0U) || count > capacity_ - length_)
  {
    valid_ = false;
    return false;
  }
  if (count != 0U)
  {
    memcpy(data_ + length_, source, count);
    length_ += count;
  }
  return true;
}

bool FLASHMEM TpCanonicalWriter::writeU8(uint8_t value)
{
  return append(&value, sizeof(value));
}

bool FLASHMEM TpCanonicalWriter::writeU16(uint16_t value)
{
  const uint8_t b[2] =
  {
    (uint8_t)value,
    (uint8_t)(value >> 8U)
  };
  return append(b, sizeof(b));
}

bool FLASHMEM TpCanonicalWriter::writeU32(uint32_t value)
{
  const uint8_t b[4] =
  {
    (uint8_t)value,
    (uint8_t)(value >> 8U),
    (uint8_t)(value >> 16U),
    (uint8_t)(value >> 24U)
  };
  return append(b, sizeof(b));
}

bool FLASHMEM TpCanonicalWriter::writeU64(uint64_t value)
{
  uint8_t b[8];
  for (uint8_t i = 0U; i < 8U; i++) b[i] = (uint8_t)(value >> (8U * i));
  return append(b, sizeof(b));
}

bool FLASHMEM TpCanonicalWriter::writeI32(int32_t value)
{
  return writeU32((uint32_t)value);
}

bool FLASHMEM TpCanonicalWriter::writeI64(int64_t value)
{
  return writeU64((uint64_t)value);
}

bool FLASHMEM TpCanonicalWriter::writeBytes(const void* source, size_t count)
{
  if (count > 0xFFFFFFFFUL) return false;
  return writeU32((uint32_t)count) && append(source, count);
}

bool FLASHMEM TpCanonicalWriter::writeUtf8(const char* text)
{
  if (text == nullptr) return false;
  return writeBytes(text, strlen(text));
}

bool FLASHMEM TpCanonicalWriter::writeHash32(const uint8_t hash[32])
{
  return append(hash, 32U);
}

const TpFirmwareRuntimeIdentity& FLASHMEM tpSignedDataFirmwareIdentity(void)
{
  return TP_SIGNED_FIRMWARE_IDENTITY;
}

bool FLASHMEM tpSignedDataDeviceCalibrationReady(void)
{
  return tpSignedCalibrationDeviceReady();
}

bool FLASHMEM tpSignedDataHeadCalibrationReady(void)
{
  return tpSignedCalibrationHeadReady();
}

bool FLASHMEM tpSignedDataSystemCalibrationReady(void)
{
  return tpSignedCalibrationSystemReady();
}

bool FLASHMEM tpSignedDataFirmwareManifestReady(void)
{
  return tpFirmwareIntegrityVerified();
}

const TpFirmwareApprovalStatus& FLASHMEM tpSignedDataFirmwareApprovalStatus(void)
{
  tpSignedDataRefreshFirmwareApproval();
  return tpSignedFirmwareApproval;
}

uint8_t FLASHMEM tpSignedDataMissingEvidenceMask(void)
{
  uint8_t missing = 0U;
  if (!deviceIdentityCertificateValid()) missing |= TP_SIGNED_EVIDENCE_DEVICE_CERTIFICATE;
  if (!tpSignedDataDeviceCalibrationReady()) missing |= TP_SIGNED_EVIDENCE_DEVICE_CALIBRATION;
  if (!tpSignedDataHeadCalibrationReady()) missing |= TP_SIGNED_EVIDENCE_HEAD_CALIBRATION;
  if (!tpSignedDataSystemCalibrationReady()) missing |= TP_SIGNED_EVIDENCE_SYSTEM_CALIBRATION;
  if (!tpSignedDataFirmwareManifestReady()) missing |= TP_SIGNED_EVIDENCE_FIRMWARE_MANIFEST;
  return missing;
}

bool FLASHMEM tpSignedDataCertifiedEvidenceReady(void)
{
  // Ein Hersteller-Firmwarezertifikat ist optional. Auch kundenspezifische,
  // extern validierte Firmware darf zertifizierte Messnachweise erzeugen. Der
  // exakte Ist-SHA-256 wird dennoch immer in den Nachweis aufgenommen; der
  // fehlende Herstellerstatus bleibt als Information sichtbar.
  const uint8_t blockingMask =
      TP_SIGNED_EVIDENCE_DEVICE_CERTIFICATE |
      TP_SIGNED_EVIDENCE_DEVICE_CALIBRATION |
      TP_SIGNED_EVIDENCE_HEAD_CALIBRATION |
      TP_SIGNED_EVIDENCE_SYSTEM_CALIBRATION;
  return (tpSignedDataMissingEvidenceMask() & blockingMask) == 0U &&
         tpFirmwareIntegrityStatus().hashCalculated;
}

bool FLASHMEM tpSignedDataCertifiedOutputImplemented(void)
{
  // Build 0.50.1_37 erzeugt segmentierte CSV-Dateien, TP3000-LOG-SIGNATURE-2-
  // Nachweise und zusätzlich TP3000-LOG-CONTAINER-1-Dateien.
  return true;
}

bool FLASHMEM tpSignedDataCertifiedModeReady(void)
{
  return tpSignedDataCertifiedEvidenceReady() &&
         tpSignedDataCertifiedOutputImplemented();
}

const char* FLASHMEM tpSignedDataCertifiedReadinessText(bool english)
{
  const uint8_t missing = tpSignedDataMissingEvidenceMask();
  if (missing & TP_SIGNED_EVIDENCE_DEVICE_CERTIFICATE)
    return english ? TP_SIGNED_READY_DEVICE_CERT_EN : TP_SIGNED_READY_DEVICE_CERT_DE;
  if (missing & TP_SIGNED_EVIDENCE_DEVICE_CALIBRATION)
    return english ? TP_SIGNED_READY_DEVICE_CAL_EN : TP_SIGNED_READY_DEVICE_CAL_DE;
  if (missing & TP_SIGNED_EVIDENCE_HEAD_CALIBRATION)
    return english ? TP_SIGNED_READY_HEAD_CAL_EN : TP_SIGNED_READY_HEAD_CAL_DE;
  if (missing & TP_SIGNED_EVIDENCE_SYSTEM_CALIBRATION)
    return english ? TP_SIGNED_READY_SYSTEM_CAL_EN : TP_SIGNED_READY_SYSTEM_CAL_DE;
  if (!tpFirmwareIntegrityStatus().hashCalculated)
    return english ? TP_SIGNED_READY_FIRMWARE_HASH_EN : TP_SIGNED_READY_FIRMWARE_HASH_DE;
  if (missing & TP_SIGNED_EVIDENCE_FIRMWARE_MANIFEST)
    return english ? TP_SIGNED_READY_FIRMWARE_EN : TP_SIGNED_READY_FIRMWARE_DE;
  if (!tpSignedDataCertifiedOutputImplemented())
    return english ? TP_SIGNED_READY_OUTPUT_EN : TP_SIGNED_READY_OUTPUT_DE;
  return english ? TP_SIGNED_READY_DEVELOPMENT_EN : TP_SIGNED_READY_DEVELOPMENT_DE;
}
