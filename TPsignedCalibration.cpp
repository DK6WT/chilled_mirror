/*
 * TP-3000 signed calibration workflow
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPsignedCalibration.h"
#include "TPexternalCalibration.h"
#include "TPsignedData.h"
#include "TP_T.h"
#include "TPsha256.h"
#include "TPfirmwareIntegrity.h"
#include "TPsystemFirmwareContext.h"
#include "TPzlibDeflate.h"
#include "TPqrCode.h"
#include <Arduino.h>
#include <SD.h>
#include <TimeLib.h>

// Shared Ethernet JSON scratch, borrowed only during synchronous TFT QR generation.
extern char ethJsonBuf[16896];
#include <TP3000_uECC.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

extern var_t R;
extern bool sdLogEnsureReadyForAccess(void);
extern void refCalGetAllScaled(uint32_t* date_yyyymmdd,
                               int32_t* ref100,
                               int32_t* ref120,
                               int32_t* chA_corr100,
                               int32_t* chA_corr120,
                               int32_t* chB_corr100,
                               int32_t* chB_corr120);
extern int32_t pt100R0GetScaled(uint8_t sensor);
extern void pt100Cal2GetScaled(uint8_t sensor,
                              int32_t* soll1,
                              int32_t* ist1,
                              int32_t* soll2,
                              int32_t* ist2,
                              bool* aktiv);
extern int32_t taupunktOffsetGetScaled(void);

#if defined(__IMXRT1062__)
#define TP_SIGNED_CAL_RODATA __attribute__((section(".progmem")))
#else
#define TP_SIGNED_CAL_RODATA
#endif

static const char TP_CAL_DIR[] TP_SIGNED_CAL_RODATA = "/CALIBRATION";
static const char TP_CAL_REQUEST_DIR[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/REQUESTS";
static const char TP_CAL_ACTIVE_DEVICE[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_DEVICE.tpdcal";
static const char TP_CAL_ACTIVE_HEAD[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_HEAD.tphcal";
static const char TP_CAL_ACTIVE_DEVICE_TEMP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_DEVICE.tmp";
static const char TP_CAL_ACTIVE_DEVICE_BACKUP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_DEVICE.bak";
static const char TP_CAL_ACTIVE_HEAD_TEMP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_HEAD.tmp";
static const char TP_CAL_ACTIVE_HEAD_BACKUP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_HEAD.bak";
static const char TP_CAL_ACTIVE_SYSTEM[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_SYSTEM.tpscal";
static const char TP_CAL_ACTIVE_SYSTEM_TEMP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_SYSTEM.tmp";
static const char TP_CAL_ACTIVE_SYSTEM_BACKUP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ACTIVE_SYSTEM.bak";
static const char TP_CAL_ARCHIVE_DIR[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE";
static const char TP_CAL_ARCHIVE_DEVICE_DIR[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/DEVICE";
static const char TP_CAL_ARCHIVE_HEAD_DIR[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/HEAD";
static const char TP_CAL_ARCHIVE_SYSTEM_DIR[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/SYSTEM";
static const char TP_CAL_ARCHIVE_DEVICE_TEMP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/DEVICE/ARCHIVE.tmp";
static const char TP_CAL_ARCHIVE_HEAD_TEMP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/HEAD/ARCHIVE.tmp";
static const char TP_CAL_ARCHIVE_SYSTEM_TEMP[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/SYSTEM/ARCHIVE.tmp";
static const char TP_CAL_ARCHIVE_DEVICE_PATH[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/DEVICE/%s.tpdcal";
static const char TP_CAL_ARCHIVE_HEAD_PATH[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/HEAD/%s.tphcal";
static const char TP_CAL_ARCHIVE_SYSTEM_PATH[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/ARCHIVE/SYSTEM/%s.tpscal";
static const char TP_CAL_ARCHIVE_SYSTEM_EXTENSION[] TP_SIGNED_CAL_RODATA = ".tpscal";
static const char TP_CAL_APPLICABILITY_FILE[] TP_SIGNED_CAL_RODATA = "/CALIBRATION/APPLICABILITY.TPS";
static const char TP_CAL_APPLICABILITY_FORMAT[] TP_SIGNED_CAL_RODATA = "TP3000-CAL-APPLICABILITY-1";
static const char TP_CAL_APPLICABILITY_APPENDIX_BEGIN[] TP_SIGNED_CAL_RODATA = "--TP3000-APPLICABILITY-1--";
static const char TP_CAL_APPLICABILITY_APPENDIX_END[] TP_SIGNED_CAL_RODATA = "--TP3000-APPLICABILITY-END--";
static const char SC_REASON_HEAD_VALUES[] TP_SIGNED_CAL_RODATA = "HEAD_VALUES_CHANGED";
static const char SC_REASON_DEVICE_VALUES[] TP_SIGNED_CAL_RODATA = "DEVICE_VALUES_CHANGED";
static const char SC_REASON_HEAD_ADJUSTMENT[] TP_SIGNED_CAL_RODATA = "HEAD_ADJUSTMENT_CHANGED";
static const char SC_REASON_DEVICE_ADJUSTMENT[] TP_SIGNED_CAL_RODATA = "DEVICE_ADJUSTMENT_CHANGED";
static const char SC_REASON_SYSTEM_REPLACED[] TP_SIGNED_CAL_RODATA = "SYSTEM_CALIBRATION_REPLACED";
static const char SC_REASON_EXTERNAL_CHANGED[] TP_SIGNED_CAL_RODATA = "EXTERNAL_CERTIFICATE_CHANGED";
static const char SC_REASON_CONFIGURATION[] TP_SIGNED_CAL_RODATA = "CONFIGURATION_CHANGED";
static const char SC_SYSTEM_IMPORT_CERT_REQUIRED[] TP_SIGNED_CAL_RODATA = "Für den Import ist zuerst ein gültiges Gerätezertifikat erforderlich";
static const char SC_SYSTEM_IMPORT_DIR_UNAVAILABLE[] TP_SIGNED_CAL_RODATA = "Kalibrierverzeichnis auf SD ist nicht verfügbar";
static const char SC_SYSTEM_IMPORT_ACTIVE_NAME[] TP_SIGNED_CAL_RODATA = "ACTIVE_SYSTEM.tpscal";
static const char SC_SYSTEM_IMPORT_NONE[] TP_SIGNED_CAL_RODATA = "Keine passende gültige .tpscal im Verzeichnis /CALIBRATION gefunden";
static const char SC_SYSTEM_IMPORT_REREAD[] TP_SIGNED_CAL_RODATA = "Ausgewählte Systemkalibrierung konnte nicht erneut gelesen werden";
static const char SC_ERR_PREVIOUS_DEVICE_ARCHIVE[] TP_SIGNED_CAL_RODATA = "Bisherige Gerätejustierung konnte nicht archiviert werden";
static const char SC_ERR_PREVIOUS_HEAD_ARCHIVE[] TP_SIGNED_CAL_RODATA = "Bisherige Kopfjustierung konnte nicht archiviert werden";
static const char SC_ERR_PREVIOUS_ADJUSTMENT_SAVE[] TP_SIGNED_CAL_RODATA = "Bisherige Justierung konnte nicht gesichert werden";
static const char SC_ERR_PREVIOUS_DEVICE_SAVE[] TP_SIGNED_CAL_RODATA = "Bisherige Gerätejustierung konnte nicht gesichert werden";
static const char SC_ERR_PREVIOUS_HEAD_SAVE[] TP_SIGNED_CAL_RODATA = "Bisherige Kopfjustierung konnte nicht gesichert werden";
static const char SC_ERR_PREVIOUS_SYSTEM_ARCHIVE[] TP_SIGNED_CAL_RODATA = "Bisherige Systemkalibrierung konnte nicht archiviert werden";
static const char SC_ERR_PREVIOUS_SYSTEM_SAVE[] TP_SIGNED_CAL_RODATA = "Bisherige Systemkalibrierung konnte nicht gesichert werden";
static const char SC_ERR_NEW_DEVICE_ARCHIVE[] TP_SIGNED_CAL_RODATA = "Neue Gerätejustierung konnte nicht im Historienarchiv gesichert werden";
static const char SC_ERR_NEW_HEAD_ARCHIVE[] TP_SIGNED_CAL_RODATA = "Neue Kopfjustierung konnte nicht im Historienarchiv gesichert werden";
static const char SC_ERR_NEW_SYSTEM_ARCHIVE[] TP_SIGNED_CAL_RODATA = "Neue Systemkalibrierung konnte nicht im Historienarchiv gesichert werden";
static const char SC_ERR_NO_SELECTED_SYSTEM[] TP_SIGNED_CAL_RODATA = "Keine gültige Systemkalibrierung für den ausgewählten Kopf";
static const char SC_ERR_NO_CURRENT_SYSTEM[] TP_SIGNED_CAL_RODATA = "Keine aktuell gültige Systemkalibrierung für den ausgewählten Kopf";
static const char SC_ERR_SELECTED_SYSTEM_MISMATCH[] TP_SIGNED_CAL_RODATA = "Systemkalibrierung passt nicht zum ausgewählten Kopf oder zu den aktuellen Justierungen";
static const char SC_ERR_ARCHIVED_ADJUSTMENT_UNAVAILABLE[] TP_SIGNED_CAL_RODATA = "Historische Justierung ist nicht verfügbar";
static const char SC_ERR_ARCHIVED_MANIFEST[] TP_SIGNED_CAL_RODATA = "Ungültige historische Manifest-ID";
static const char SC_ERR_ARCHIVED_ADJUSTMENT_READ[] TP_SIGNED_CAL_RODATA = "Historische Justierungsdatei konnte nicht gelesen werden";
static const char SC_ERR_ARCHIVED_ADJUSTMENT_INVALID[] TP_SIGNED_CAL_RODATA = "Historische Justierung wurde verändert oder ist ungültig";
static const char SC_ERR_ARCHIVED_SYSTEM_UNAVAILABLE[] TP_SIGNED_CAL_RODATA = "Historische Systemkalibrierung ist nicht verfügbar";
static const char SC_ERR_ARCHIVED_SYSTEM_READ[] TP_SIGNED_CAL_RODATA = "Historische Systemkalibrierung konnte nicht gelesen werden";
static const char SC_ERR_ARCHIVED_SYSTEM_INVALID[] TP_SIGNED_CAL_RODATA = "Historische Systemkalibrierung wurde verändert oder ist ungültig";
static const char SC_ERR_SEARCH_RANGE[] TP_SIGNED_CAL_RODATA = "Suchzeitraum ist ungültig";
static const char SC_ERR_ARCHIVE_UNAVAILABLE[] TP_SIGNED_CAL_RODATA = "SD-Karte oder Kalibrierarchiv nicht verfügbar";
static const char SC_ERR_CALIBRATION_DIR_UNAVAILABLE[] TP_SIGNED_CAL_RODATA = "SD-Karte oder /CALIBRATION nicht verfügbar";
static const char SC_ERR_ARCHIVE_READ[] TP_SIGNED_CAL_RODATA = "Systemkalibrierarchiv ist nicht lesbar";
static const char SC_ERR_ACTIVE_ADJUSTMENT_OUTPUT[] TP_SIGNED_CAL_RODATA = "Aktive Kalibrierdatei konnte nicht ausgegeben werden";
static const char SC_ERR_ACTIVE_SYSTEM_OUTPUT[] TP_SIGNED_CAL_RODATA = "Aktive Systemkalibrierung konnte nicht ausgegeben werden";
static const char TP_CAL_ROLE_FORMAT_V1[] TP_SIGNED_CAL_RODATA = "TP3000-SIGNING-ROLE-CERTIFICATE-1";
static const char TP_CAL_ROLE_FORMAT_V2[] TP_SIGNED_CAL_RODATA = "TP3000-SIGNING-ROLE-CERTIFICATE-2";
static const char TP_CAL_ROLE_NAME_V1[] TP_SIGNED_CAL_RODATA = "CALIBRATION";
static const char TP_CAL_ROLE_NAME_V2[] TP_SIGNED_CAL_RODATA = "CALIBRATION_LAB";
static const char TP_CAL_PERMISSION_SIGN[] TP_SIGNED_CAL_RODATA = "SIGN_CALIBRATION";
static const char TP_CAL_SIGNER_ROOT[] TP_SIGNED_CAL_RODATA = "MANUFACTURER_ROOT";
static const char TP_CAL_SIGNER_LAB[] TP_SIGNED_CAL_RODATA = "CALIBRATION_LAB";
static const char TP_CAL_FORMAT_DEVICE_PACKAGE_V1[] TP_SIGNED_CAL_RODATA = TP_FORMAT_DEVICE_CALIBRATION_V1;
static const char TP_CAL_FORMAT_DEVICE_PACKAGE_V2[] TP_SIGNED_CAL_RODATA = TP_FORMAT_DEVICE_CALIBRATION_V2;
static const char TP_CAL_FORMAT_HEAD_PACKAGE_V1[] TP_SIGNED_CAL_RODATA = TP_FORMAT_HEAD_CALIBRATION_V1;
static const char TP_CAL_FORMAT_HEAD_PACKAGE_V2[] TP_SIGNED_CAL_RODATA = TP_FORMAT_HEAD_CALIBRATION_V2;
static const size_t TP_CAL_SIGNED_JSON_MAX = 16384U;
static const int64_t TP_CAL_REQUEST_VALIDITY_SECONDS = 72LL * 60LL * 60LL;
static const int64_t TP_CAL_LAB_MAX_VALIDITY_SECONDS = 366LL * 24LL * 60LL * 60LL;
static const char TP_CAL_PRODUCT[] TP_SIGNED_CAL_RODATA = "TP-3000";
static const char TP_CAL_SIGNATURE_ALGORITHM[] TP_SIGNED_CAL_RODATA = "ECDSA-P256-SHA256";
static const char TP_CAL_SIGNATURE_ENCODING[] TP_SIGNED_CAL_RODATA = "IEEE-P1363";
static const char TP_CAL_FORMAT_DEVICE_REQUEST[] TP_SIGNED_CAL_RODATA = TP_FORMAT_DEVICE_CAL_REQUEST;
static const char TP_CAL_FORMAT_DEVICE_PACKAGE[] TP_SIGNED_CAL_RODATA = TP_FORMAT_DEVICE_CALIBRATION;
static const char TP_CAL_FORMAT_HEAD_REQUEST[] TP_SIGNED_CAL_RODATA = TP_FORMAT_HEAD_CAL_REQUEST;
static const char TP_CAL_FORMAT_HEAD_PACKAGE[] TP_SIGNED_CAL_RODATA = TP_FORMAT_HEAD_CALIBRATION;
static const char TP_CAL_FORMAT_SYSTEM_REQUEST_V1[] TP_SIGNED_CAL_RODATA = TP_FORMAT_SYSTEM_CAL_REQUEST_V1;
static const char TP_CAL_FORMAT_SYSTEM_REQUEST[] TP_SIGNED_CAL_RODATA = TP_FORMAT_SYSTEM_CAL_REQUEST;
static const char TP_CAL_FORMAT_SYSTEM_PACKAGE_V1[] TP_SIGNED_CAL_RODATA = TP_FORMAT_SYSTEM_CALIBRATION_V1;
static const char TP_CAL_FORMAT_SYSTEM_PACKAGE[] TP_SIGNED_CAL_RODATA = TP_FORMAT_SYSTEM_CALIBRATION;
static const char TP_CAL_FIRMWARE_VERSION[] TP_SIGNED_CAL_RODATA = TP_FIRMWARE_VERSION_STRING;
static const char TP_CAL_FIRMWARE_BUILD_ID[] TP_SIGNED_CAL_RODATA = TP_FIRMWARE_BUILD_ID_STRING;
static const char SC_FIELD_LAB_ID[] TP_SIGNED_CAL_RODATA = "LabId";
static const char SC_FIELD_LAB_NAME[] TP_SIGNED_CAL_RODATA = "LabName";
static const char SC_FIELD_LAB_ADDRESS[] TP_SIGNED_CAL_RODATA = "LabAddress";
static const char SC_FIELD_PERMISSIONS[] TP_SIGNED_CAL_RODATA = "Permissions";
static const char SC_FIELD_SOURCE_REQUEST_ID[] TP_SIGNED_CAL_RODATA = "SourceRequestId";
static const char SC_FIELD_SOURCE_REQUEST_CREATED[] TP_SIGNED_CAL_RODATA = "SourceRequestCreatedUtc";
static const char SC_FIELD_SOURCE_REQUEST_EXPIRES[] TP_SIGNED_CAL_RODATA = "SourceRequestExpiresUtc";
static const char SC_FIELD_SIGNER_TYPE[] TP_SIGNED_CAL_RODATA = "SignerType";
static const char SC_FIELD_REFERENCE_STANDARDS[] TP_SIGNED_CAL_RODATA = "ReferenceStandards";
static const char SC_FIELD_TRACEABILITY[] TP_SIGNED_CAL_RODATA = "Traceability";
static const char SC_FIELD_MEASUREMENT_UNCERTAINTY[] TP_SIGNED_CAL_RODATA = "MeasurementUncertainty";
static const char SC_FIELD_ENVIRONMENTAL_CONDITIONS[] TP_SIGNED_CAL_RODATA = "EnvironmentalConditions";
static const char SC_FIELD_CALIBRATION_PROCEDURE[] TP_SIGNED_CAL_RODATA = "CalibrationProcedure";
static const char SC_FIELD_ACCREDITATION_INFORMATION[] TP_SIGNED_CAL_RODATA = "AccreditationInformation";
static const char SC_FIELD_CALIBRATION_SCOPE[] TP_SIGNED_CAL_RODATA = "CalibrationScope";
static const char SC_FIELD_CALIBRATION_RESULTS_ENCODING[] TP_SIGNED_CAL_RODATA = "CalibrationResultsEncoding";
static const char SC_FIELD_CALIBRATION_RESULTS_BASE64URL[] TP_SIGNED_CAL_RODATA = "CalibrationResultsBase64Url";
static const char TP_CAL_RESULTS_ENCODING[] TP_SIGNED_CAL_RODATA = "TP3000-CAL-POINTS-1-BASE64URL";
static const char SC_FIELD_DEVICE_ADJUSTMENT_ID[] TP_SIGNED_CAL_RODATA = "DeviceAdjustmentCalibrationId";
static const char SC_FIELD_DEVICE_ADJUSTMENT_HASH[] TP_SIGNED_CAL_RODATA = "DeviceAdjustmentManifestSha256";
static const char SC_FIELD_HEAD_ADJUSTMENT_ID[] TP_SIGNED_CAL_RODATA = "HeadAdjustmentCalibrationId";
static const char SC_FIELD_HEAD_ADJUSTMENT_HASH[] TP_SIGNED_CAL_RODATA = "HeadAdjustmentManifestSha256";
static const char SC_FIELD_EXTERNAL_CERT_AVAILABLE[] TP_SIGNED_CAL_RODATA = "ExternalCertificateAvailable";
static const char SC_FIELD_EXTERNAL_CERT_DOCUMENT_ID[] TP_SIGNED_CAL_RODATA = "ExternalCertificateDocumentId";
static const char SC_FIELD_EXTERNAL_CERT_NUMBER[] TP_SIGNED_CAL_RODATA = "ExternalCertificateNumber";
static const char SC_FIELD_EXTERNAL_CERT_VALID_FROM[] TP_SIGNED_CAL_RODATA = "ExternalCertificateValidFromYmd";
static const char SC_FIELD_EXTERNAL_CERT_VALID_UNTIL[] TP_SIGNED_CAL_RODATA = "ExternalCertificateValidUntilYmd";
static const char SC_FIELD_EXTERNAL_CERT_ORIGINAL_NAME[] TP_SIGNED_CAL_RODATA = "ExternalCertificateOriginalFileName";
static const char SC_FIELD_EXTERNAL_CERT_FILE_SIZE[] TP_SIGNED_CAL_RODATA = "ExternalCertificateFileSize";
static const char SC_FIELD_EXTERNAL_CERT_SHA256[] TP_SIGNED_CAL_RODATA = "ExternalCertificateSha256";
static const char SC_FIELD_CERTIFICATE_SOURCE[] TP_SIGNED_CAL_RODATA = "CertificateSource";
static const char TP_CAL_CERT_SOURCE_TP3000[] TP_SIGNED_CAL_RODATA = "TP3000";
static const char TP_CAL_CERT_SOURCE_EXTERNAL[] TP_SIGNED_CAL_RODATA = "EXTERNAL_PDF";
static const char SC_SYSTEM_REQUEST_TEMPLATE[] TP_SIGNED_CAL_RODATA =
  "{\n"
  "  \"Format\": \"%s\",\n"
  "  \"Product\": \"TP-3000\",\n"
  "  \"DeviceSerial\": \"%s\",\n"
  "  \"DeviceKeyId\": \"%s\",\n"
  "  \"DeviceCertificateSerial\": \"%s\",\n"
  "  \"RequestId\": \"%s\",\n"
  "  \"CreatedUtc\": \"%s\",\n"
  "  \"Values\": {\n"
  "    \"CalibrationDateYmd\": %lu,\n"
  "    \"HeadType\": \"%s\",\n"
  "    \"HeadSerial\": %lu,\n"
  "    \"DeviceAdjustmentCalibrationId\": \"%s\",\n"
  "    \"DeviceAdjustmentManifestSha256\": \"%s\",\n"
  "    \"HeadAdjustmentCalibrationId\": \"%s\",\n"
  "    \"HeadAdjustmentManifestSha256\": \"%s\",\n"
  "    \"ExternalCertificateAvailable\": %u,\n"
  "    \"ExternalCertificateDocumentId\": \"%s\",\n"
  "    \"ExternalCertificateNumber\": \"%s\",\n"
  "    \"ExternalCertificateValidFromYmd\": %lu,\n"
  "    \"ExternalCertificateValidUntilYmd\": %lu,\n"
  "    \"ExternalCertificateOriginalFileName\": \"%s\",\n"
  "    \"ExternalCertificateFileSize\": %lu,\n"
  "    \"ExternalCertificateSha256\": \"%s\"\n"
  "  },\n"
  "  \"FirmwareVersion\": \"%s\",\n"
  "  \"FirmwareBuildId\": \"%s\",\n"
  "  \"DeviceCertificate\": %s,\n"
  "  \"ManifestSha256\": \"%s\",\n"
  "  \"SignatureAlgorithm\": \"%s\",\n"
  "  \"SignatureEncoding\": \"%s\",\n"
  "  \"DeviceSignatureBase64\": \"%s\"\n"
  "}\n";

// Alle weiteren statischen Texte dieses Moduls liegen gesammelt im QSPI-Flash.
// Dadurch belegen JSON-Feldnamen, Fehlermeldungen und Formatvorlagen kein RAM1.
struct TpSignedCalibrationFlashTextTable
{
  char t000[sizeof("")];
  char t001[sizeof("\"%s\"")];
  char t002[sizeof("%04d-%02d-%02dT%02d:%02d:%02d+00:00")];
  char t003[sizeof("CalibrationDateYmd")];
  char t004[sizeof("ReferenceDateYmd")];
  char t005[sizeof("RefLowScaled")];
  char t006[sizeof("RefHighScaled")];
  char t007[sizeof("ChannelALowCorrectionScaled")];
  char t008[sizeof("ChannelAHighCorrectionScaled")];
  char t009[sizeof("ChannelBLowCorrectionScaled")];
  char t010[sizeof("ChannelBHighCorrectionScaled")];
  char t011[sizeof("HeadType")];
  char t012[sizeof("HeadSerial")];
  char t013[sizeof("CalibrationTimeHms")];
  char t014[sizeof("R0MirrorScaled")];
  char t015[sizeof("R0AmbientScaled")];
  char t016[sizeof("Mirror2PointActive")];
  char t017[sizeof("MirrorSet1_mC")];
  char t018[sizeof("MirrorActual1_mC")];
  char t019[sizeof("MirrorSet2_mC")];
  char t020[sizeof("MirrorActual2_mC")];
  char t021[sizeof("Ambient2PointActive")];
  char t022[sizeof("AmbientSet1_mC")];
  char t023[sizeof("AmbientActual1_mC")];
  char t024[sizeof("AmbientSet2_mC")];
  char t025[sizeof("AmbientActual2_mC")];
  char t026[sizeof("DewFrostOffset_mC")];
  char t027[sizeof("PidKp")];
  char t028[sizeof("PidKi_x1000")];
  char t029[sizeof("PidKd_x1000")];
  char t030[sizeof("ControlInterval_ms")];
  char t031[sizeof("HBridgeDeadtime_ms")];
  char t032[sizeof("FanPercent")];
  char t033[sizeof("OpticalTarget_x10")];
  char t034[sizeof("PeltierCurrentLimit_mA")];
  char t035[sizeof("CalibrationId")];
  char t036[sizeof("ValidFromUtc")];
  char t037[sizeof("ValidUntilUtc")];
  char t038[sizeof("CalibrationIntervalMonths")];
  char t039[sizeof("CalibrationLaboratory")];
  char t040[sizeof("CalibrationOperator")];
  char t041[sizeof("CertificateReference")];
  char t042[sizeof("ReasonForCalibration")];
  char t043[sizeof("Format")];
  char t044[sizeof("Product")];
  char t045[sizeof("Role")];
  char t046[sizeof("KeyName")];
  char t047[sizeof("KeyId")];
  char t048[sizeof("PublicKeySpkiBase64")];
  char t049[sizeof("IssuerKeyId")];
  char t050[sizeof("CertificateSerial")];
  char t051[sizeof("IssuedUtc")];
  char t052[sizeof("NotBeforeUtc")];
  char t053[sizeof("NotAfterUtc")];
  char t054[sizeof("ManifestSha256")];
  char t055[sizeof("SignatureAlgorithm")];
  char t056[sizeof("SignatureEncoding")];
  char t057[sizeof("RootSignatureBase64")];
  char t058[sizeof("Rollenzertifikat unvollstaendig")];
  char t059[sizeof("Rollenzertifikat hat falsche Rolle oder Root-ID")];
  char t060[sizeof("Public Key im Rollenzertifikat ungueltig")];
  char t061[sizeof("Rollenschluessel ist kein P-256-SPKI")];
  char t062[sizeof("Key-ID des Rollenschluessels stimmt nicht")];
  char t063[sizeof("Zeit oder Manifest im Rollenzertifikat ungueltig")];
  char t064[sizeof("Root-Signatur des Rollenzertifikats ungueltig")];
  char t065[sizeof("Rollenzertifikat ist zu gross")];
  char t066[sizeof("Root-Signatur des Rollenzertifikats ist ungueltig")];
  char t067[sizeof("DeviceSerial")];
  char t068[sizeof("DeviceKeyId")];
  char t069[sizeof("DeviceCertificateSerial")];
  char t070[sizeof("FirmwareVersion")];
  char t071[sizeof("FirmwareBuildId")];
  char t072[sizeof("SourceRequestManifestSha256")];
  char t073[sizeof("Approval")];
  char t074[sizeof("ApprovedUtc")];
  char t075[sizeof("SignerRoleCertificate")];
  char t076[sizeof("SignerKeyId")];
  char t077[sizeof("CalibrationSignatureBase64")];
  char t078[sizeof("Kalibrierdatei unvollstaendig")];
  char t079[sizeof("Kalibrierdatei passt nicht zu Geraet, Rolle oder Zeitraum")];
  char t080[sizeof("Values")];
  char t081[sizeof("Geraetejustierung konnte nicht gelesen werden")];
  char t082[sizeof("Kanonischer Geraete-Kalibrierdatensatz ist zu gross")];
  char t083[sizeof("Kalibrierdatum in Freigabe und Geraetewerten stimmt nicht")];
  char t084[sizeof("Signatur der Geraetejustierung ist ungueltig")];
  char t085[sizeof("Aktuelle Geraetewerte weichen von der signierten Justierung ab")];
  char t086[sizeof("Kopfjustierung konnte nicht gelesen werden")];
  char t087[sizeof("Kanonischer Kopf-Kalibrierdatensatz ist zu gross")];
  char t088[sizeof("Kalibrierdatum in Freigabe und Kopfwerten stimmt nicht")];
  char t089[sizeof("Signatur der Kopfjustierung ist ungueltig")];
  char t090[sizeof("Aktuelle Kopfwerte weichen von der signierten Kalibrierung ab")];
  char t091[sizeof("Aktive Kalibrierdatei ist leer oder zu gross")];
  char t092[sizeof("Gueltiges Geraetezertifikat fehlt")];
  char t093[sizeof("SD-Karte oder /CALIBRATION nicht verfuegbar")];
  char t094[sizeof("Verzeichnis /CALIBRATION nicht lesbar")];
  char t095[sizeof("ACTIVE_DEVICE.tpdcal")];
  char t096[sizeof("ACTIVE_HEAD.tphcal")];
  char t097[sizeof(".tpdcal")];
  char t098[sizeof(".tphcal")];
  char t099[sizeof("%s/%s")];
  char t100[sizeof("Keine passende gueltige .tpdcal gefunden")];
  char t101[sizeof("Keine passende gueltige .tphcal gefunden")];
  char t102[sizeof("Ausgewaehlte Kalibrierdatei konnte nicht erneut gelesen werden")];
  char t103[sizeof("Aktive Kalibrierdatei konnte nicht gespeichert werden")];
  char t104[sizeof("OK")];
  char t105[sizeof("not yet valid")];
  char t106[sizeof("noch nicht gueltig")];
  char t107[sizeof("within calibration period")];
  char t108[sizeof("innerhalb Kalibrierzeitraum")];
  char t109[sizeof("outside calibration period")];
  char t110[sizeof("ausserhalb Kalibrierzeitraum")];
  char t111[sizeof("time cannot be assessed")];
  char t112[sizeof("Kalibrierzeit nicht beurteilbar")];
  char t113[sizeof("Zuerst ein gueltiges Geraetezertifikat aktivieren")];
  char t114[sizeof("Ausgabepuffer ist zu klein")];
  char t115[sizeof("Zufaellige Anfrage-ID konnte nicht erzeugt werden")];
  char t116[sizeof("RTC-Zeit oder UTC-Offset ungueltig; PC-Zeit uebertragen")];
  char t117[sizeof("Geraetezertifikat konnte nicht eingebettet werden")];
  char t118[sizeof("Kanonische Geraete-Kalibrieranfrage ist zu gross")];
  char t119[sizeof("Geraetesignatur der Kalibrieranfrage fehlgeschlagen")];
  char t120[sizeof("{\n" "  \"Format\": \"%s\",\n" "  \"Product\": \"TP-3000\",\n" "  \"DeviceSerial\": \"%s\",\n" "  \"DeviceKeyId\": \"%s\",\n" "  \"DeviceCertificateSerial\": \"%s\",\n" "  \"RequestId\": \"%s\",\n" "  \"CreatedUtc\": \"%s\",\n" "  \"Values\": {\n" "    \"CalibrationDateYmd\": %lu,\n" "    \"ReferenceDateYmd\": %lu,\n" "    \"RefLowScaled\": %ld,\n" "    \"RefHighScaled\": %ld,\n" "    \"ChannelALowCorrectionScaled\": %ld,\n" "    \"ChannelAHighCorrectionScaled\": %ld,\n" "    \"ChannelBLowCorrectionScaled\": %ld,\n" "    \"ChannelBHighCorrectionScaled\": %ld\n" "  },\n" "  \"FirmwareVersion\": \"%s\",\n" "  \"FirmwareBuildId\": \"%s\",\n" "  \"DeviceCertificate\": %s,\n" "  \"ManifestSha256\": \"%s\",\n" "  \"SignatureAlgorithm\": \"%s\",\n" "  \"SignatureEncoding\": \"%s\",\n" "  \"DeviceSignatureBase64\": \"%s\"\n" "}\n")];
  char t121[sizeof("TP3000_G%s_%.8s.tpdcalreq")];
  char t122[sizeof("Kopf-SN muss 00001..99999 sein; Kopftyp/Datum pruefen")];
  char t123[sizeof("Kanonische Kopf-Kalibrieranfrage ist zu gross")];
  char t124[sizeof("Geraetesignatur der Kopf-Kalibrieranfrage fehlgeschlagen")];
  char t125[sizeof("{\n" "  \"Format\": \"%s\",\n" "  \"Product\": \"TP-3000\",\n" "  \"DeviceSerial\": \"%s\",\n" "  \"DeviceKeyId\": \"%s\",\n" "  \"DeviceCertificateSerial\": \"%s\",\n" "  \"RequestId\": \"%s\",\n" "  \"CreatedUtc\": \"%s\",\n" "  \"Values\": {\n" "    \"HeadType\": \"%s\",\n" "    \"HeadSerial\": %lu,\n" "    \"CalibrationDateYmd\": %lu,\n" "    \"CalibrationTimeHms\": %lu,\n" "    \"R0MirrorScaled\": %ld,\n" "    \"R0AmbientScaled\": %ld,\n" "    \"Mirror2PointActive\": %u,\n" "    \"MirrorSet1_mC\": %ld,\n" "    \"MirrorActual1_mC\": %ld,\n" "    \"MirrorSet2_mC\": %ld,\n" "    \"MirrorActual2_mC\": %ld,\n" "    \"Ambient2PointActive\": %u,\n" "    \"AmbientSet1_mC\": %ld,\n" "    \"AmbientActual1_mC\": %ld,\n" "    \"AmbientSet2_mC\": %ld,\n" "    \"AmbientActual2_mC\": %ld,\n" "    \"DewFrostOffset_mC\": %ld,\n" "    \"PidKp\": %u,\n" "    \"PidKi_x1000\": %ld,\n" "    \"PidKd_x1000\": %ld,\n" "    \"ControlInterval_ms\": %u,\n" "    \"HBridgeDeadtime_ms\": %u,\n" "    \"FanPercent\": %u,\n" "    \"OpticalTarget_x10\": %u,\n" "    \"PeltierCurrentLimit_mA\": %u\n" "  },\n" "  \"FirmwareVersion\": \"%s\",\n" "  \"FirmwareBuildId\": \"%s\",\n" "  \"DeviceCertificate\": %s,\n" "  \"ManifestSha256\": \"%s\",\n" "  \"SignatureAlgorithm\": \"%s\",\n" "  \"SignatureEncoding\": \"%s\",\n" "  \"DeviceSignatureBase64\": \"%s\"\n" "}\n")];
  char t126[sizeof("%s_K%05lu_%.8s.tphcalreq")];
  char t127[sizeof("Kalibrieranfrage passt nicht in den Ausgabepuffer")];
  char t128[sizeof("SD-Karte oder Kalibrieranfrage ungueltig")];
  char t129[sizeof("Kalibrieranfrage konnte nicht auf SD gespeichert werden")];
  char t130[sizeof("Referenz-/Justierungsdatum fehlt; Datum zuerst in der Referenzjustierung speichern")];
};

static const TpSignedCalibrationFlashTextTable tpSignedCalText TP_SIGNED_CAL_RODATA =
{
  "",
  "\"%s\"",
  "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
  "CalibrationDateYmd",
  "ReferenceDateYmd",
  "RefLowScaled",
  "RefHighScaled",
  "ChannelALowCorrectionScaled",
  "ChannelAHighCorrectionScaled",
  "ChannelBLowCorrectionScaled",
  "ChannelBHighCorrectionScaled",
  "HeadType",
  "HeadSerial",
  "CalibrationTimeHms",
  "R0MirrorScaled",
  "R0AmbientScaled",
  "Mirror2PointActive",
  "MirrorSet1_mC",
  "MirrorActual1_mC",
  "MirrorSet2_mC",
  "MirrorActual2_mC",
  "Ambient2PointActive",
  "AmbientSet1_mC",
  "AmbientActual1_mC",
  "AmbientSet2_mC",
  "AmbientActual2_mC",
  "DewFrostOffset_mC",
  "PidKp",
  "PidKi_x1000",
  "PidKd_x1000",
  "ControlInterval_ms",
  "HBridgeDeadtime_ms",
  "FanPercent",
  "OpticalTarget_x10",
  "PeltierCurrentLimit_mA",
  "CalibrationId",
  "ValidFromUtc",
  "ValidUntilUtc",
  "CalibrationIntervalMonths",
  "CalibrationLaboratory",
  "CalibrationOperator",
  "CertificateReference",
  "ReasonForCalibration",
  "Format",
  "Product",
  "Role",
  "KeyName",
  "KeyId",
  "PublicKeySpkiBase64",
  "IssuerKeyId",
  "CertificateSerial",
  "IssuedUtc",
  "NotBeforeUtc",
  "NotAfterUtc",
  "ManifestSha256",
  "SignatureAlgorithm",
  "SignatureEncoding",
  "RootSignatureBase64",
  "Rollenzertifikat unvollstaendig",
  "Rollenzertifikat hat falsche Rolle oder Root-ID",
  "Public Key im Rollenzertifikat ungueltig",
  "Rollenschluessel ist kein P-256-SPKI",
  "Key-ID des Rollenschluessels stimmt nicht",
  "Zeit oder Manifest im Rollenzertifikat ungueltig",
  "Root-Signatur des Rollenzertifikats ungueltig",
  "Rollenzertifikat ist zu gross",
  "Root-Signatur des Rollenzertifikats ist ungueltig",
  "DeviceSerial",
  "DeviceKeyId",
  "DeviceCertificateSerial",
  "FirmwareVersion",
  "FirmwareBuildId",
  "SourceRequestManifestSha256",
  "Approval",
  "ApprovedUtc",
  "SignerRoleCertificate",
  "SignerKeyId",
  "CalibrationSignatureBase64",
  "Kalibrierdatei unvollstaendig",
  "Kalibrierdatei passt nicht zu Geraet, Rolle oder Zeitraum",
  "Values",
  "Geraetejustierung konnte nicht gelesen werden",
  "Kanonischer Geraete-Kalibrierdatensatz ist zu gross",
  "Kalibrierdatum in Freigabe und Geraetewerten stimmt nicht",
  "Signatur der Geraetejustierung ist ungueltig",
  "Aktuelle Geraetewerte weichen von der signierten Justierung ab",
  "Kopfjustierung konnte nicht gelesen werden",
  "Kanonischer Kopf-Kalibrierdatensatz ist zu gross",
  "Kalibrierdatum in Freigabe und Kopfwerten stimmt nicht",
  "Signatur der Kopfjustierung ist ungueltig",
  "Aktuelle Kopfwerte weichen von der signierten Kalibrierung ab",
  "Aktive Kalibrierdatei ist leer oder zu gross",
  "Gueltiges Geraetezertifikat fehlt",
  "SD-Karte oder /CALIBRATION nicht verfuegbar",
  "Verzeichnis /CALIBRATION nicht lesbar",
  "ACTIVE_DEVICE.tpdcal",
  "ACTIVE_HEAD.tphcal",
  ".tpdcal",
  ".tphcal",
  "%s/%s",
  "Keine passende gueltige .tpdcal gefunden",
  "Keine passende gueltige .tphcal gefunden",
  "Ausgewaehlte Kalibrierdatei konnte nicht erneut gelesen werden",
  "Aktive Kalibrierdatei konnte nicht gespeichert werden",
  "OK",
  "not yet valid",
  "noch nicht gueltig",
  "within calibration period",
  "innerhalb Kalibrierzeitraum",
  "outside calibration period",
  "ausserhalb Kalibrierzeitraum",
  "time cannot be assessed",
  "Kalibrierzeit nicht beurteilbar",
  "Zuerst ein gueltiges Geraetezertifikat aktivieren",
  "Ausgabepuffer ist zu klein",
  "Zufaellige Anfrage-ID konnte nicht erzeugt werden",
  "RTC-Zeit oder UTC-Offset ungueltig; PC-Zeit uebertragen",
  "Geraetezertifikat konnte nicht eingebettet werden",
  "Kanonische Geraete-Kalibrieranfrage ist zu gross",
  "Geraetesignatur der Kalibrieranfrage fehlgeschlagen",
  "{\n" "  \"Format\": \"%s\",\n" "  \"Product\": \"TP-3000\",\n" "  \"DeviceSerial\": \"%s\",\n" "  \"DeviceKeyId\": \"%s\",\n" "  \"DeviceCertificateSerial\": \"%s\",\n" "  \"RequestId\": \"%s\",\n" "  \"CreatedUtc\": \"%s\",\n" "  \"Values\": {\n" "    \"CalibrationDateYmd\": %lu,\n" "    \"ReferenceDateYmd\": %lu,\n" "    \"RefLowScaled\": %ld,\n" "    \"RefHighScaled\": %ld,\n" "    \"ChannelALowCorrectionScaled\": %ld,\n" "    \"ChannelAHighCorrectionScaled\": %ld,\n" "    \"ChannelBLowCorrectionScaled\": %ld,\n" "    \"ChannelBHighCorrectionScaled\": %ld\n" "  },\n" "  \"FirmwareVersion\": \"%s\",\n" "  \"FirmwareBuildId\": \"%s\",\n" "  \"DeviceCertificate\": %s,\n" "  \"ManifestSha256\": \"%s\",\n" "  \"SignatureAlgorithm\": \"%s\",\n" "  \"SignatureEncoding\": \"%s\",\n" "  \"DeviceSignatureBase64\": \"%s\"\n" "}\n",
  "TP3000_G%s_%.8s.tpdcalreq",
  "Kopf-SN muss 00001..99999 sein; Kopftyp/Datum pruefen",
  "Kanonische Kopf-Kalibrieranfrage ist zu gross",
  "Geraetesignatur der Kopf-Kalibrieranfrage fehlgeschlagen",
  "{\n" "  \"Format\": \"%s\",\n" "  \"Product\": \"TP-3000\",\n" "  \"DeviceSerial\": \"%s\",\n" "  \"DeviceKeyId\": \"%s\",\n" "  \"DeviceCertificateSerial\": \"%s\",\n" "  \"RequestId\": \"%s\",\n" "  \"CreatedUtc\": \"%s\",\n" "  \"Values\": {\n" "    \"HeadType\": \"%s\",\n" "    \"HeadSerial\": %lu,\n" "    \"CalibrationDateYmd\": %lu,\n" "    \"CalibrationTimeHms\": %lu,\n" "    \"R0MirrorScaled\": %ld,\n" "    \"R0AmbientScaled\": %ld,\n" "    \"Mirror2PointActive\": %u,\n" "    \"MirrorSet1_mC\": %ld,\n" "    \"MirrorActual1_mC\": %ld,\n" "    \"MirrorSet2_mC\": %ld,\n" "    \"MirrorActual2_mC\": %ld,\n" "    \"Ambient2PointActive\": %u,\n" "    \"AmbientSet1_mC\": %ld,\n" "    \"AmbientActual1_mC\": %ld,\n" "    \"AmbientSet2_mC\": %ld,\n" "    \"AmbientActual2_mC\": %ld,\n" "    \"DewFrostOffset_mC\": %ld,\n" "    \"PidKp\": %u,\n" "    \"PidKi_x1000\": %ld,\n" "    \"PidKd_x1000\": %ld,\n" "    \"ControlInterval_ms\": %u,\n" "    \"HBridgeDeadtime_ms\": %u,\n" "    \"FanPercent\": %u,\n" "    \"OpticalTarget_x10\": %u,\n" "    \"PeltierCurrentLimit_mA\": %u\n" "  },\n" "  \"FirmwareVersion\": \"%s\",\n" "  \"FirmwareBuildId\": \"%s\",\n" "  \"DeviceCertificate\": %s,\n" "  \"ManifestSha256\": \"%s\",\n" "  \"SignatureAlgorithm\": \"%s\",\n" "  \"SignatureEncoding\": \"%s\",\n" "  \"DeviceSignatureBase64\": \"%s\"\n" "}\n",
  "%s_K%05lu_%.8s.tphcalreq",
  "Kalibrieranfrage passt nicht in den Ausgabepuffer",
  "SD-Karte oder Kalibrieranfrage ungueltig",
  "Kalibrieranfrage konnte nicht auf SD gespeichert werden",
  "Referenz-/Justierungsdatum fehlt; Datum zuerst in der Referenzjustierung speichern"
};

struct TpDeviceCalValues
{
  uint32_t calibrationDateYmd;
  uint32_t referenceDateYmd;
  int32_t refLowScaled;
  int32_t refHighScaled;
  int32_t chALowScaled;
  int32_t chAHighScaled;
  int32_t chBLowScaled;
  int32_t chBHighScaled;
};

struct TpHeadCalValues
{
  char headType[HEAD_TYPE_TEXT_LEN + 1U];
  uint32_t headSerial;
  uint32_t calibrationDateYmd;
  uint32_t calibrationTimeHms;
  int32_t r0MirrorScaled;
  int32_t r0AmbientScaled;
  uint8_t mirror2PointActive;
  int32_t mirrorSet1_mC;
  int32_t mirrorActual1_mC;
  int32_t mirrorSet2_mC;
  int32_t mirrorActual2_mC;
  uint8_t ambient2PointActive;
  int32_t ambientSet1_mC;
  int32_t ambientActual1_mC;
  int32_t ambientSet2_mC;
  int32_t ambientActual2_mC;
  int32_t dewFrostOffset_mC;
  uint16_t pidKp;
  int32_t pidKi_x1000;
  int32_t pidKd_x1000;
  uint16_t controlInterval_ms;
  uint8_t hBridgeDeadtime_ms;
  uint8_t fanPercent;
  uint16_t opticalTarget_x10;
  uint16_t peltierCurrentLimit_mA;
};

struct TpSystemCalValues
{
  uint32_t calibrationDateYmd;
  char headType[HEAD_TYPE_TEXT_LEN + 1U];
  uint32_t headSerial;
  char deviceAdjustmentCalibrationId[97];
  uint8_t deviceAdjustmentManifest[32];
  char headAdjustmentCalibrationId[97];
  uint8_t headAdjustmentManifest[32];
  uint8_t externalCertificateAvailable;
  char externalCertificateDocumentId[33];
  char externalCertificateNumber[33];
  uint32_t externalCertificateValidFromYmd;
  uint32_t externalCertificateValidUntilYmd;
  char externalCertificateOriginalFileName[81];
  uint32_t externalCertificateFileSize;
  uint8_t externalCertificateSha256[32];
};

struct TpCalApproval
{
  char calibrationId[97];
  uint32_t calibrationDateYmd;
  int64_t validFromUtc;
  int64_t validUntilUtc;
  uint16_t intervalMonths;
  char laboratory[161];
  char operatorName[97];
  char certificateReference[161];
  char reason[161];
  char referenceStandards[769];
  char traceability[385];
  char measurementUncertainty[257];
  char environmentalConditions[257];
  char calibrationProcedure[513];
  char accreditationInformation[385];
  char calibrationScope[40];
  char calibrationResultsEncoding[49];
  char calibrationResultsBase64Url[2049];
  char certificateSource[24];
};

struct TpRoleCertificateParsed
{
  bool legacy;
  char role[24];
  char keyName[97];
  char keyId[17];
  char issuerKeyId[17];
  char labId[33];
  char labName[161];
  char labAddress[321];
  char permissions[129];
  char certificateSerial[40];
  int64_t issuedUtc;
  int64_t notBeforeUtc;
  int64_t notAfterUtc;
  uint8_t publicSpki[91];
  uint8_t publicRaw[64];
  uint8_t manifestSha256[32];
  uint8_t rootSignature[64];
};

static DMAMEM TpSignedCalibrationStatus tpDeviceCalStatus;
static DMAMEM TpSignedCalibrationStatus tpHeadCalStatus;
static DMAMEM TpSystemCalibrationStatus tpSystemCalStatus;
static DMAMEM TpDeviceCalValues tpDeviceCalSignedValues;
static DMAMEM TpHeadCalValues tpHeadCalSignedValues;
static DMAMEM TpSystemCalValues tpSystemCalSignedValues;
// Arbeitsstrukturen für die automatische, kopfbezogene Zertifikatsauswahl.
// Sie werden nur sequenziell benutzt und liegen bewusst in RAM2, damit der
// knappe RAM1-Stack beim Durchsuchen des SD-Archivs nicht belastet wird.
static DMAMEM TpSystemCalibrationStatus tpSystemSelectionStatus;
static DMAMEM TpSystemCalValues tpSystemSelectionValues;
static DMAMEM TpSignedCalibrationStatus tpHeadSelectionStatus;
static DMAMEM TpHeadCalValues tpHeadSelectionValues;
static DMAMEM TpExternalCalibrationMetadata tpExternalSelectionMetadata;
static DMAMEM int64_t tpSystemSelectionLastAttemptUtc = 0;
static DMAMEM bool tpSystemVerificationSignatureOnly = false;
static DMAMEM TpCalApproval tpSignedCalApproval = {};
static DMAMEM TpRoleCertificateParsed tpSignedCalRole = {};
static bool tpSignedCalibrationLoaded = false;
// Unterdrückt während eines transaktionalen Justierungsimports sowohl das
// dauerhafte Anwendbarkeitsereignis als auch die automatische Systemauswahl.
// Erst nach erfolgreicher Rücklese- und Archivprüfung wird der alte Schein
// beendet und anschließend neu ausgewählt.
static bool tpSignedCalibrationTransactionReload = false;

static DMAMEM char tpSignedCalJson[16896];
static DMAMEM char tpSignedCalObjectA[12288];
static DMAMEM char tpSignedCalObjectB[4096];
static DMAMEM uint8_t tpSignedCalCanonical[8192];
static DMAMEM uint8_t tpSignedCalFileBuffer[512];

static void FLASHMEM scSetText(char* target, size_t size, const char* text)
{
  if (target == nullptr || size == 0U) return;
  if (text == nullptr) text = tpSignedCalText.t000;
  const size_t length = strnlen(text, size - 1U);
  memmove(target, text, length);
  target[length] = '\0';
}

static bool FLASHMEM scRequireFirmwareHashAvailable(char* errorText,
                                                     size_t errorTextSize)
{
  // Ein Hersteller-Firmwarezertifikat ist bewusst keine technische
  // Voraussetzung fuer Justierung oder Systemkalibrierung. Betreiber duerfen
  // eigene, extern validierte Firmware einsetzen. Fuer die nachvollziehbare
  // Dokumentation muss lediglich der SHA-256 des laufenden Abbildes vorliegen.
  if (tpFirmwareIntegrityStatus().hashCalculated) return true;
  if (errorText != nullptr && errorTextSize > 0U)
  {
    snprintf(errorText, errorTextSize,
             "Firmware-SHA-256 ist noch nicht verfuegbar: %s. "
             "Geraet sauber neu starten und erneut versuchen.",
             tpFirmwareIntegrityStatusText());
    errorText[errorTextSize - 1U] = '\0';
  }
  return false;
}

static void FLASHMEM scSetError(TpSignedCalibrationStatus& status, const char* text)
{
  scSetText(status.lastError, sizeof(status.lastError), text);
}

static void FLASHMEM scSetSystemError(TpSystemCalibrationStatus& status, const char* text)
{
  scSetText(status.lastError, sizeof(status.lastError), text);
}

static bool FLASHMEM scBytesEqual(const uint8_t* a, const uint8_t* b, size_t count)
{
  if (a == nullptr || b == nullptr) return false;
  uint8_t diff = 0U;
  for (size_t i = 0U; i < count; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0U;
}

static int8_t FLASHMEM scHexNibble(char c)
{
  if (c >= '0' && c <= '9') return (int8_t)(c - '0');
  if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
  if (c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
  return -1;
}

static bool FLASHMEM scHexToBytes(const char* text, uint8_t* output, size_t outputLength)
{
  if (text == nullptr || output == nullptr || strlen(text) != outputLength * 2U) return false;
  for (size_t i = 0U; i < outputLength; ++i)
  {
    const int8_t h = scHexNibble(text[i * 2U]);
    const int8_t l = scHexNibble(text[i * 2U + 1U]);
    if (h < 0 || l < 0) return false;
    output[i] = (uint8_t)(((uint8_t)h << 4U) | (uint8_t)l);
  }
  return true;
}

static char FLASHMEM scHexDigit(uint8_t value, bool upper)
{
  value &= 0x0FU;
  if (value < 10U) return (char)('0' + value);
  return (char)((upper ? 'A' : 'a') + value - 10U);
}

static bool FLASHMEM scBytesToHex(const uint8_t* input,
                                  size_t inputLength,
                                  char* output,
                                  size_t outputSize,
                                  bool upper)
{
  if (input == nullptr || output == nullptr || outputSize < inputLength * 2U + 1U) return false;
  for (size_t i = 0U; i < inputLength; ++i)
  {
    output[i * 2U] = scHexDigit((uint8_t)(input[i] >> 4U), upper);
    output[i * 2U + 1U] = scHexDigit(input[i], upper);
  }
  output[inputLength * 2U] = '\0';
  return true;
}

static const char scBase64Alphabet[] TP_SIGNED_CAL_RODATA =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static bool FLASHMEM scBase64Encode(const uint8_t* input,
                                    size_t inputLength,
                                    char* output,
                                    size_t outputSize)
{
  const size_t required = ((inputLength + 2U) / 3U) * 4U;
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
    output[out++] = scBase64Alphabet[(triple >> 18U) & 0x3FU];
    output[out++] = scBase64Alphabet[(triple >> 12U) & 0x3FU];
    output[out++] = haveB ? scBase64Alphabet[(triple >> 6U) & 0x3FU] : '=';
    output[out++] = haveC ? scBase64Alphabet[triple & 0x3FU] : '=';
  }
  output[out] = '\0';
  return true;
}

static int8_t FLASHMEM scBase64Value(char c)
{
  if (c >= 'A' && c <= 'Z') return (int8_t)(c - 'A');
  if (c >= 'a' && c <= 'z') return (int8_t)(c - 'a' + 26);
  if (c >= '0' && c <= '9') return (int8_t)(c - '0' + 52);
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static bool FLASHMEM scBase64Decode(const char* input,
                                    uint8_t* output,
                                    size_t outputSize,
                                    size_t* outputLength)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (input == nullptr || output == nullptr) return false;
  const size_t inputLength = strlen(input);
  if (inputLength == 0U || (inputLength % 4U) != 0U) return false;
  size_t in = 0U;
  size_t out = 0U;
  while (in < inputLength)
  {
    const char c0 = input[in++];
    const char c1 = input[in++];
    const char c2 = input[in++];
    const char c3 = input[in++];
    const int8_t v0 = scBase64Value(c0);
    const int8_t v1 = scBase64Value(c1);
    const int8_t v2 = c2 == '=' ? 0 : scBase64Value(c2);
    const int8_t v3 = c3 == '=' ? 0 : scBase64Value(c3);
    if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) return false;
    const uint32_t triple = ((uint32_t)v0 << 18U) | ((uint32_t)v1 << 12U) |
                            ((uint32_t)v2 << 6U) | (uint32_t)v3;
    if (out >= outputSize) return false;
    output[out++] = (uint8_t)(triple >> 16U);
    if (c2 != '=')
    {
      if (out >= outputSize) return false;
      output[out++] = (uint8_t)(triple >> 8U);
    }
    if (c3 != '=')
    {
      if (out >= outputSize) return false;
      output[out++] = (uint8_t)triple;
    }
  }
  if (outputLength != nullptr) *outputLength = out;
  return true;
}

static int FLASHMEM scJsonHexNibble(char c)
{
  return scHexNibble(c);
}

static bool FLASHMEM scJsonGetString(const char* json,
                                     const char* key,
                                     char* output,
                                     size_t outputSize)
{
  if (json == nullptr || key == nullptr || output == nullptr || outputSize == 0U) return false;
  char pattern[96];
  if (snprintf(pattern, sizeof(pattern), tpSignedCalText.t001, key) <= 0) return false;
  const char* p = strstr(json, pattern);
  if (p == nullptr) return false;
  p += strlen(pattern);
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  if (*p++ != ':') return false;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  if (*p++ != '"') return false;

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
            const int nibble = scJsonHexNibble(*p++);
            if (nibble < 0) return false;
            value = (value << 4) | nibble;
          }
          if (value <= 0 || value > 0x7F) return false;
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

// Liefert einen String ausschließlich aus dem äußersten JSON-Objekt. Das ist
// für Felder wie ManifestSha256 wichtig, die zusätzlich in eingebetteten
// Zertifikaten vorkommen können.
static const char* FLASHMEM scJsonFindRootValue(const char* json, const char* key)
{
  if (json == nullptr || key == nullptr) return nullptr;
  char pattern[96];
  const int written = snprintf(pattern, sizeof(pattern), tpSignedCalText.t001, key);
  if (written <= 0 || (size_t)written >= sizeof(pattern)) return nullptr;
  const size_t patternLength = (size_t)written;

  int objectDepth = 0;
  bool inString = false;
  bool escaped = false;
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
      if (objectDepth == 1 && strncmp(p, pattern, patternLength) == 0)
      {
        const char* value = p + patternLength;
        while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') ++value;
        if (*value == ':')
        {
          ++value;
          while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') ++value;
          return value;
        }
      }
      inString = true;
    }
    else if (c == '{') ++objectDepth;
    else if (c == '}') --objectDepth;
  }
  return nullptr;
}

static bool FLASHMEM scJsonGetRootString(const char* json,
                                         const char* key,
                                         char* output,
                                         size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return false;
  const char* p = scJsonFindRootValue(json, key);
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
            const int nibble = scJsonHexNibble(*p++);
            if (nibble < 0) return false;
            value = (value << 4) | nibble;
          }
          if (value <= 0 || value > 0x7F) return false;
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

static bool FLASHMEM scJsonGetInt64(const char* json, const char* key, int64_t* value)
{
  if (json == nullptr || key == nullptr || value == nullptr) return false;
  char pattern[96];
  if (snprintf(pattern, sizeof(pattern), tpSignedCalText.t001, key) <= 0) return false;
  const char* p = strstr(json, pattern);
  if (p == nullptr) return false;
  p += strlen(pattern);
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  if (*p++ != ':') return false;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  char* end = nullptr;
  const long long parsed = strtoll(p, &end, 10);
  if (end == p) return false;
  *value = (int64_t)parsed;
  return true;
}

static bool FLASHMEM scJsonGetUInt32(const char* json, const char* key, uint32_t* value)
{
  int64_t parsed = 0;
  if (!scJsonGetInt64(json, key, &parsed) || parsed < 0 || parsed > 0xFFFFFFFFLL) return false;
  *value = (uint32_t)parsed;
  return true;
}

static bool FLASHMEM scJsonGetInt32(const char* json, const char* key, int32_t* value)
{
  int64_t parsed = 0;
  if (!scJsonGetInt64(json, key, &parsed) || parsed < INT32_MIN || parsed > INT32_MAX) return false;
  *value = (int32_t)parsed;
  return true;
}

static bool FLASHMEM scJsonGetUInt16(const char* json, const char* key, uint16_t* value)
{
  uint32_t parsed = 0U;
  if (!scJsonGetUInt32(json, key, &parsed) || parsed > 65535UL) return false;
  *value = (uint16_t)parsed;
  return true;
}

static bool FLASHMEM scJsonGetUInt8(const char* json, const char* key, uint8_t* value)
{
  uint32_t parsed = 0U;
  if (!scJsonGetUInt32(json, key, &parsed) || parsed > 255UL) return false;
  *value = (uint8_t)parsed;
  return true;
}

static bool FLASHMEM scJsonGetObject(const char* json,
                                     const char* key,
                                     char* output,
                                     size_t outputSize)
{
  if (json == nullptr || key == nullptr || output == nullptr || outputSize < 3U) return false;
  char pattern[96];
  if (snprintf(pattern, sizeof(pattern), tpSignedCalText.t001, key) <= 0) return false;
  const char* p = strstr(json, pattern);
  if (p == nullptr) return false;
  p += strlen(pattern);
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  if (*p++ != ':') return false;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
  if (*p != '{') return false;

  const char* start = p;
  int depth = 0;
  bool inString = false;
  bool escaped = false;
  while (*p != '\0')
  {
    const char c = *p++;
    if (inString)
    {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') inString = true;
    else if (c == '{') ++depth;
    else if (c == '}')
    {
      --depth;
      if (depth == 0)
      {
        const size_t length = (size_t)(p - start);
        if (length + 1U > outputSize) return false;
        memcpy(output, start, length);
        output[length] = '\0';
        return true;
      }
    }
  }
  return false;
}

static bool FLASHMEM scParseIsoUtc(const char* text, int64_t* unixTime)
{
  if (text == nullptr || unixTime == nullptr || strlen(text) < 19U) return false;
  const int y = (text[0]-'0')*1000 + (text[1]-'0')*100 + (text[2]-'0')*10 + (text[3]-'0');
  const int mo = (text[5]-'0')*10 + (text[6]-'0');
  const int d = (text[8]-'0')*10 + (text[9]-'0');
  const int h = (text[11]-'0')*10 + (text[12]-'0');
  const int mi = (text[14]-'0')*10 + (text[15]-'0');
  const int s = (text[17]-'0')*10 + (text[18]-'0');
  if (text[4] != '-' || text[7] != '-' || text[10] != 'T' || text[13] != ':' || text[16] != ':') return false;
  if (y < 2020 || y > 2199 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59) return false;
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

static void FLASHMEM scFormatIsoUtc(int64_t unixTime, char* output, size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return;
  const time_t t = unixTime > 0 ? (time_t)unixTime : (time_t)0;
  snprintf(output, outputSize, tpSignedCalText.t002,
           year(t), month(t), day(t), hour(t), minute(t), second(t));
}

static int64_t FLASHMEM scCurrentUnixTime(void)
{
  return tpCurrentUtcUnixTime();
}

static uint32_t FLASHMEM scCurrentYmd(void)
{
  const time_t t = now();
  if (t < (time_t)1577836800) return 0U;
  return (uint32_t)year(t) * 10000UL + (uint32_t)month(t) * 100UL + (uint32_t)day(t);
}

static uint32_t FLASHMEM scCurrentHms(void)
{
  const time_t t = now();
  if (t < (time_t)1577836800) return 0U;
  return (uint32_t)hour(t) * 10000UL + (uint32_t)minute(t) * 100UL + (uint32_t)second(t);
}

static bool FLASHMEM scReadFile(const char* path, char* buffer, size_t bufferSize, size_t* length)
{
  if (length != nullptr) *length = 0U;
  File file = SD.open(path, FILE_READ);
  if (!file || buffer == nullptr || bufferSize < 2U) return false;
  const uint32_t size = file.size();
  if (size == 0U || size >= bufferSize)
  {
    file.close();
    return false;
  }
  size_t read = 0U;
  while (file.available() && read < bufferSize - 1U)
  {
    const int c = file.read();
    if (c < 0) break;
    buffer[read++] = (char)c;
  }
  file.close();
  buffer[read] = '\0';
  if (length != nullptr) *length = read;
  return read == size;
}

// Systemkalibrierungsdateien dürfen hinter dem unveränderten signierten
// JSON einen transportablen Anwendbarkeitsanhang enthalten. Für sämtliche
// kryptografischen Prüfungen und JSON-Webantworten wird ausschließlich der
// Bereich vor der Markerzeile verwendet.
static bool FLASHMEM scReadSystemFile(const char* path,
                                      char* buffer,
                                      size_t bufferSize,
                                      size_t* signedLength)
{
  size_t fullLength = 0U;
  if (!scReadFile(path, buffer, bufferSize, &fullLength)) return false;

  const size_t markerLength = strlen(TP_CAL_APPLICABILITY_APPENDIX_BEGIN);
  char* scan = buffer;
  char* marker = nullptr;
  while ((scan = strstr(scan, TP_CAL_APPLICABILITY_APPENDIX_BEGIN)) != nullptr)
  {
    const bool lineStart = scan == buffer || scan[-1] == '\n';
    const char after = scan[markerLength];
    const bool lineEnd = after == '\n' ||
                         (after == '\r' && scan[markerLength + 1U] == '\n');
    if (lineStart && lineEnd)
    {
      marker = scan;
      break;
    }
    ++scan;
  }

  size_t length = fullLength;
  if (marker != nullptr)
  {
    length = (size_t)(marker - buffer);
    while (length > 0U &&
           (buffer[length - 1U] == '\r' || buffer[length - 1U] == '\n'))
      --length;
    buffer[length] = '\0';
  }
  if (signedLength != nullptr) *signedLength = length;
  return length > 0U;
}

static bool FLASHMEM scWriteFile(const char* path, const char* data, size_t length)
{
  if (SD.exists(path)) SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  const size_t written = file.write((const uint8_t*)data, length);
  file.flush();
  file.close();
  if (written != length)
  {
    SD.remove(path);
    return false;
  }
  return true;
}

static bool FLASHMEM scEnsureDirectories(void)
{
  if (!sdLogEnsureReadyForAccess()) return false;
  if (!SD.exists(TP_CAL_DIR) && !SD.mkdir(TP_CAL_DIR)) return false;
  if (!SD.exists(TP_CAL_REQUEST_DIR) && !SD.mkdir(TP_CAL_REQUEST_DIR)) return false;
  if (!SD.exists(TP_CAL_ARCHIVE_DIR) && !SD.mkdir(TP_CAL_ARCHIVE_DIR)) return false;
  if (!SD.exists(TP_CAL_ARCHIVE_DEVICE_DIR) && !SD.mkdir(TP_CAL_ARCHIVE_DEVICE_DIR)) return false;
  if (!SD.exists(TP_CAL_ARCHIVE_HEAD_DIR) && !SD.mkdir(TP_CAL_ARCHIVE_HEAD_DIR)) return false;
  if (!SD.exists(TP_CAL_ARCHIVE_SYSTEM_DIR) && !SD.mkdir(TP_CAL_ARCHIVE_SYSTEM_DIR)) return false;
  return true;
}

static bool FLASHMEM scValidDateYmd(uint32_t value)
{
  const uint16_t y = (uint16_t)(value / 10000UL);
  const uint8_t m = (uint8_t)((value / 100UL) % 100UL);
  const uint8_t d = (uint8_t)(value % 100UL);
  return y >= 2024U && y <= 2199U && m >= 1U && m <= 12U && d >= 1U && d <= 31U;
}

static void FLASHMEM scCurrentDeviceValues(TpDeviceCalValues& values)
{
  memset(&values, 0, sizeof(values));
  refCalGetAllScaled(&values.referenceDateYmd,
                     &values.refLowScaled,
                     &values.refHighScaled,
                     &values.chALowScaled,
                     &values.chAHighScaled,
                     &values.chBLowScaled,
                     &values.chBHighScaled);
  // Das Referenz-/Justierungsdatum muss aus dem persistenten Metrologieblock
  // stammen. Ein Ersatz durch das aktuelle Tagesdatum waere nicht stabil:
  // Beim Datumswechsel wuerde eine unveraenderte signierte Justierung sonst
  // faelschlich als abweichend bewertet.
  values.calibrationDateYmd = values.referenceDateYmd;
}

static void FLASHMEM scCurrentHeadValues(TpHeadCalValues& values)
{
  memset(&values, 0, sizeof(values));
  scSetText(values.headType, sizeof(values.headType), headTypeTextGet());
  values.headSerial = R.head_serial;
  values.calibrationDateYmd = scCurrentYmd();
  values.calibrationTimeHms = scCurrentHms();
  values.r0MirrorScaled = pt100R0GetScaled(0U);
  values.r0AmbientScaled = pt100R0GetScaled(1U);
  bool active = false;
  pt100Cal2GetScaled(0U,
                     &values.mirrorSet1_mC,
                     &values.mirrorActual1_mC,
                     &values.mirrorSet2_mC,
                     &values.mirrorActual2_mC,
                     &active);
  values.mirror2PointActive = active ? 1U : 0U;
  pt100Cal2GetScaled(1U,
                     &values.ambientSet1_mC,
                     &values.ambientActual1_mC,
                     &values.ambientSet2_mC,
                     &values.ambientActual2_mC,
                     &active);
  values.ambient2PointActive = active ? 1U : 0U;
  values.dewFrostOffset_mC = taupunktOffsetGetScaled();
  values.pidKp = R.pid_kp;
  values.pidKi_x1000 = (int32_t)lround((double)R.pid_ki * 1000.0);
  values.pidKd_x1000 = (int32_t)lround((double)R.pid_kd * 1000.0);
  values.controlInterval_ms = R.regler_intervall_ms;
  values.hBridgeDeadtime_ms = R.h_bruecke_totzeit;
  values.fanPercent = R.fan_percent;
  values.opticalTarget_x10 = R.optik_sollwert;
  values.peltierCurrentLimit_mA = peltierCurrentLimitGetMa();
}

static bool FLASHMEM scDeviceValuesEqual(const TpDeviceCalValues& a, const TpDeviceCalValues& b)
{
  // Datumsfelder sind Dokument-/Metadaten und keine wirksamen
  // Justierungsparameter. Der Aktivstatus darf deshalb nur von den aktuell
  // angewendeten Referenz- und Kanalkorrekturwerten abhaengen.
  return a.refLowScaled == b.refLowScaled &&
         a.refHighScaled == b.refHighScaled &&
         a.chALowScaled == b.chALowScaled &&
         a.chAHighScaled == b.chAHighScaled &&
         a.chBLowScaled == b.chBLowScaled &&
         a.chBHighScaled == b.chBHighScaled;
}

static bool FLASHMEM scHeadValuesEqual(const TpHeadCalValues& a, const TpHeadCalValues& b)
{
  return strcmp(a.headType, b.headType) == 0 &&
         a.headSerial == b.headSerial &&
         a.calibrationDateYmd == b.calibrationDateYmd &&
         a.r0MirrorScaled == b.r0MirrorScaled &&
         a.r0AmbientScaled == b.r0AmbientScaled &&
         a.mirror2PointActive == b.mirror2PointActive &&
         a.mirrorSet1_mC == b.mirrorSet1_mC &&
         a.mirrorActual1_mC == b.mirrorActual1_mC &&
         a.mirrorSet2_mC == b.mirrorSet2_mC &&
         a.mirrorActual2_mC == b.mirrorActual2_mC &&
         a.ambient2PointActive == b.ambient2PointActive &&
         a.ambientSet1_mC == b.ambientSet1_mC &&
         a.ambientActual1_mC == b.ambientActual1_mC &&
         a.ambientSet2_mC == b.ambientSet2_mC &&
         a.ambientActual2_mC == b.ambientActual2_mC &&
         a.dewFrostOffset_mC == b.dewFrostOffset_mC &&
         a.pidKp == b.pidKp &&
         a.pidKi_x1000 == b.pidKi_x1000 &&
         a.pidKd_x1000 == b.pidKd_x1000 &&
         a.controlInterval_ms == b.controlInterval_ms &&
         a.hBridgeDeadtime_ms == b.hBridgeDeadtime_ms &&
         a.fanPercent == b.fanPercent &&
         a.opticalTarget_x10 == b.opticalTarget_x10 &&
         a.peltierCurrentLimit_mA == b.peltierCurrentLimit_mA;
}

static bool FLASHMEM scWriteRequestPrefix(TpCanonicalWriter& w,
                                          const char* format,
                                          const char* serial,
                                          const char* keyId,
                                          const char* certificateSerial,
                                          const char* requestId,
                                          int64_t createdUtc)
{
  return w.writeUtf8(format) &&
         w.writeUtf8(TP_CAL_PRODUCT) &&
         w.writeUtf8(serial) &&
         w.writeUtf8(keyId) &&
         w.writeUtf8(certificateSerial) &&
         w.writeUtf8(requestId) &&
         w.writeI64(createdUtc);
}

static bool FLASHMEM scWriteDeviceValues(TpCanonicalWriter& w, const TpDeviceCalValues& v)
{
  return w.writeU32(v.calibrationDateYmd) &&
         w.writeU32(v.referenceDateYmd) &&
         w.writeI32(v.refLowScaled) &&
         w.writeI32(v.refHighScaled) &&
         w.writeI32(v.chALowScaled) &&
         w.writeI32(v.chAHighScaled) &&
         w.writeI32(v.chBLowScaled) &&
         w.writeI32(v.chBHighScaled);
}

static bool FLASHMEM scWriteHeadValues(TpCanonicalWriter& w, const TpHeadCalValues& v)
{
  return w.writeUtf8(v.headType) &&
         w.writeU32(v.headSerial) &&
         w.writeU32(v.calibrationDateYmd) &&
         w.writeU32(v.calibrationTimeHms) &&
         w.writeI32(v.r0MirrorScaled) &&
         w.writeI32(v.r0AmbientScaled) &&
         w.writeU8(v.mirror2PointActive) &&
         w.writeI32(v.mirrorSet1_mC) &&
         w.writeI32(v.mirrorActual1_mC) &&
         w.writeI32(v.mirrorSet2_mC) &&
         w.writeI32(v.mirrorActual2_mC) &&
         w.writeU8(v.ambient2PointActive) &&
         w.writeI32(v.ambientSet1_mC) &&
         w.writeI32(v.ambientActual1_mC) &&
         w.writeI32(v.ambientSet2_mC) &&
         w.writeI32(v.ambientActual2_mC) &&
         w.writeI32(v.dewFrostOffset_mC) &&
         w.writeU16(v.pidKp) &&
         w.writeI32(v.pidKi_x1000) &&
         w.writeI32(v.pidKd_x1000) &&
         w.writeU16(v.controlInterval_ms) &&
         w.writeU8(v.hBridgeDeadtime_ms) &&
         w.writeU8(v.fanPercent) &&
         w.writeU16(v.opticalTarget_x10) &&
         w.writeU16(v.peltierCurrentLimit_mA);
}

static bool FLASHMEM scWriteSystemValuesV1(TpCanonicalWriter& w, const TpSystemCalValues& v)
{
  return w.writeU32(v.calibrationDateYmd) &&
         w.writeUtf8(v.headType) &&
         w.writeU32(v.headSerial) &&
         w.writeUtf8(v.deviceAdjustmentCalibrationId) &&
         w.writeHash32(v.deviceAdjustmentManifest) &&
         w.writeUtf8(v.headAdjustmentCalibrationId) &&
         w.writeHash32(v.headAdjustmentManifest);
}

static bool FLASHMEM scWriteSystemValuesV2(TpCanonicalWriter& w, const TpSystemCalValues& v)
{
  return scWriteSystemValuesV1(w, v) &&
         w.writeU8(v.externalCertificateAvailable) &&
         w.writeUtf8(v.externalCertificateDocumentId) &&
         w.writeUtf8(v.externalCertificateNumber) &&
         w.writeU32(v.externalCertificateValidFromYmd) &&
         w.writeU32(v.externalCertificateValidUntilYmd) &&
         w.writeUtf8(v.externalCertificateOriginalFileName) &&
         w.writeU32(v.externalCertificateFileSize) &&
         w.writeHash32(v.externalCertificateSha256);
}

static bool FLASHMEM scWriteApprovalV1(TpCanonicalWriter& w,
                                       const TpCalApproval& a,
                                       int64_t approvedUtc,
                                       const char* signerKeyId,
                                       const uint8_t roleCertManifest[32])
{
  return w.writeUtf8(a.calibrationId) &&
         w.writeU32(a.calibrationDateYmd) &&
         w.writeI64(a.validFromUtc) &&
         w.writeI64(a.validUntilUtc) &&
         w.writeU16(a.intervalMonths) &&
         w.writeUtf8(a.laboratory) &&
         w.writeUtf8(a.operatorName) &&
         w.writeUtf8(a.certificateReference) &&
         w.writeUtf8(a.reason) &&
         w.writeI64(approvedUtc) &&
         w.writeUtf8(signerKeyId) &&
         w.writeHash32(roleCertManifest);
}

static bool FLASHMEM scWriteApprovalV2(TpCanonicalWriter& w,
                                       const TpCalApproval& a,
                                       int64_t approvedUtc,
                                       const char* signerType,
                                       const char* signerKeyId,
                                       const uint8_t signerCertificateManifest[32])
{
  return w.writeUtf8(a.calibrationId) &&
         w.writeU32(a.calibrationDateYmd) &&
         w.writeI64(a.validFromUtc) &&
         w.writeI64(a.validUntilUtc) &&
         w.writeU16(a.intervalMonths) &&
         w.writeUtf8(a.laboratory) &&
         w.writeUtf8(a.operatorName) &&
         w.writeUtf8(a.certificateReference) &&
         w.writeUtf8(a.reason) &&
         w.writeUtf8(a.referenceStandards) &&
         w.writeUtf8(a.traceability) &&
         w.writeUtf8(a.measurementUncertainty) &&
         w.writeUtf8(a.environmentalConditions) &&
         w.writeUtf8(a.calibrationProcedure) &&
         w.writeUtf8(a.accreditationInformation) &&
         w.writeI64(approvedUtc) &&
         w.writeUtf8(signerType) &&
         w.writeUtf8(signerKeyId) &&
         w.writeHash32(signerCertificateManifest);
}

static bool FLASHMEM scWriteApprovalV3(TpCanonicalWriter& w,
                                       const TpCalApproval& a,
                                       int64_t approvedUtc,
                                       const char* signerType,
                                       const char* signerKeyId,
                                       const uint8_t signerCertificateManifest[32])
{
  return w.writeUtf8(a.calibrationId) &&
         w.writeU32(a.calibrationDateYmd) &&
         w.writeI64(a.validFromUtc) &&
         w.writeI64(a.validUntilUtc) &&
         w.writeU16(a.intervalMonths) &&
         w.writeUtf8(a.laboratory) &&
         w.writeUtf8(a.operatorName) &&
         w.writeUtf8(a.certificateReference) &&
         w.writeUtf8(a.reason) &&
         w.writeUtf8(a.referenceStandards) &&
         w.writeUtf8(a.traceability) &&
         w.writeUtf8(a.measurementUncertainty) &&
         w.writeUtf8(a.environmentalConditions) &&
         w.writeUtf8(a.calibrationProcedure) &&
         w.writeUtf8(a.accreditationInformation) &&
         w.writeUtf8(a.calibrationScope) &&
         w.writeUtf8(a.calibrationResultsEncoding) &&
         w.writeUtf8(a.calibrationResultsBase64Url) &&
         w.writeI64(approvedUtc) &&
         w.writeUtf8(signerType) &&
         w.writeUtf8(signerKeyId) &&
         w.writeHash32(signerCertificateManifest);
}

static bool FLASHMEM scWriteApprovalV4(TpCanonicalWriter& w,
                                       const TpCalApproval& a,
                                       int64_t approvedUtc,
                                       const char* signerType,
                                       const char* signerKeyId,
                                       const uint8_t signerCertificateManifest[32])
{
  return w.writeUtf8(a.calibrationId) &&
         w.writeU32(a.calibrationDateYmd) &&
         w.writeI64(a.validFromUtc) &&
         w.writeI64(a.validUntilUtc) &&
         w.writeU16(a.intervalMonths) &&
         w.writeUtf8(a.laboratory) &&
         w.writeUtf8(a.operatorName) &&
         w.writeUtf8(a.certificateReference) &&
         w.writeUtf8(a.reason) &&
         w.writeUtf8(a.referenceStandards) &&
         w.writeUtf8(a.traceability) &&
         w.writeUtf8(a.measurementUncertainty) &&
         w.writeUtf8(a.environmentalConditions) &&
         w.writeUtf8(a.calibrationProcedure) &&
         w.writeUtf8(a.accreditationInformation) &&
         w.writeUtf8(a.calibrationScope) &&
         w.writeUtf8(a.calibrationResultsEncoding) &&
         w.writeUtf8(a.calibrationResultsBase64Url) &&
         w.writeUtf8(a.certificateSource) &&
         w.writeI64(approvedUtc) &&
         w.writeUtf8(signerType) &&
         w.writeUtf8(signerKeyId) &&
         w.writeHash32(signerCertificateManifest);
}

static bool FLASHMEM scCalibrationResultsFieldsValid(const TpCalApproval& a)
{
  if (strcmp(a.calibrationScope, "NONE") == 0)
  {
    return a.calibrationResultsEncoding[0] == '\0' && a.calibrationResultsBase64Url[0] == '\0';
  }
  const bool knownScope = strcmp(a.calibrationScope, "AS_FOUND") == 0 ||
                          strcmp(a.calibrationScope, "AS_LEFT") == 0 ||
                          strcmp(a.calibrationScope, "AS_FOUND_AS_LEFT") == 0 ||
                          strcmp(a.calibrationScope, "AS_FOUND_AS_LEFT_NO_ADJUSTMENT") == 0 ||
                          strcmp(a.calibrationScope, "BEFORE_AFTER_ADJUSTMENT") == 0;
  if (!knownScope || strcmp(a.calibrationResultsEncoding, TP_CAL_RESULTS_ENCODING) != 0 ||
      a.calibrationResultsBase64Url[0] == '\0') return false;
  // Base64URL ohne Padding; die vollständige Strukturprüfung erfolgt im PC-Tool/Web.
  for (const char* q = a.calibrationResultsBase64Url; *q != '\0'; ++q)
  {
    if (!(isalnum((unsigned char)*q) || *q == '-' || *q == '_')) return false;
  }
  return true;
}

static bool FLASHMEM scParseDeviceValues(const char* json, TpDeviceCalValues& v)
{
  return scJsonGetUInt32(json, tpSignedCalText.t003, &v.calibrationDateYmd) &&
         scJsonGetUInt32(json, tpSignedCalText.t004, &v.referenceDateYmd) &&
         scJsonGetInt32(json, tpSignedCalText.t005, &v.refLowScaled) &&
         scJsonGetInt32(json, tpSignedCalText.t006, &v.refHighScaled) &&
         scJsonGetInt32(json, tpSignedCalText.t007, &v.chALowScaled) &&
         scJsonGetInt32(json, tpSignedCalText.t008, &v.chAHighScaled) &&
         scJsonGetInt32(json, tpSignedCalText.t009, &v.chBLowScaled) &&
         scJsonGetInt32(json, tpSignedCalText.t010, &v.chBHighScaled);
}

static bool FLASHMEM scParseHeadValues(const char* json, TpHeadCalValues& v)
{
  return scJsonGetString(json, tpSignedCalText.t011, v.headType, sizeof(v.headType)) &&
         scJsonGetUInt32(json, tpSignedCalText.t012, &v.headSerial) &&
         scJsonGetUInt32(json, tpSignedCalText.t003, &v.calibrationDateYmd) &&
         scJsonGetUInt32(json, tpSignedCalText.t013, &v.calibrationTimeHms) &&
         scJsonGetInt32(json, tpSignedCalText.t014, &v.r0MirrorScaled) &&
         scJsonGetInt32(json, tpSignedCalText.t015, &v.r0AmbientScaled) &&
         scJsonGetUInt8(json, tpSignedCalText.t016, &v.mirror2PointActive) &&
         scJsonGetInt32(json, tpSignedCalText.t017, &v.mirrorSet1_mC) &&
         scJsonGetInt32(json, tpSignedCalText.t018, &v.mirrorActual1_mC) &&
         scJsonGetInt32(json, tpSignedCalText.t019, &v.mirrorSet2_mC) &&
         scJsonGetInt32(json, tpSignedCalText.t020, &v.mirrorActual2_mC) &&
         scJsonGetUInt8(json, tpSignedCalText.t021, &v.ambient2PointActive) &&
         scJsonGetInt32(json, tpSignedCalText.t022, &v.ambientSet1_mC) &&
         scJsonGetInt32(json, tpSignedCalText.t023, &v.ambientActual1_mC) &&
         scJsonGetInt32(json, tpSignedCalText.t024, &v.ambientSet2_mC) &&
         scJsonGetInt32(json, tpSignedCalText.t025, &v.ambientActual2_mC) &&
         scJsonGetInt32(json, tpSignedCalText.t026, &v.dewFrostOffset_mC) &&
         scJsonGetUInt16(json, tpSignedCalText.t027, &v.pidKp) &&
         scJsonGetInt32(json, tpSignedCalText.t028, &v.pidKi_x1000) &&
         scJsonGetInt32(json, tpSignedCalText.t029, &v.pidKd_x1000) &&
         scJsonGetUInt16(json, tpSignedCalText.t030, &v.controlInterval_ms) &&
         scJsonGetUInt8(json, tpSignedCalText.t031, &v.hBridgeDeadtime_ms) &&
         scJsonGetUInt8(json, tpSignedCalText.t032, &v.fanPercent) &&
         scJsonGetUInt16(json, tpSignedCalText.t033, &v.opticalTarget_x10) &&
         scJsonGetUInt16(json, tpSignedCalText.t034, &v.peltierCurrentLimit_mA);
}

static bool FLASHMEM scParseSystemValues(const char* json, TpSystemCalValues& v, bool v2)
{
  char deviceHash[72];
  char headHash[72];
  memset(&v, 0, sizeof(v));
  if (!scJsonGetUInt32(json, tpSignedCalText.t003, &v.calibrationDateYmd) ||
      !scJsonGetString(json, tpSignedCalText.t011, v.headType, sizeof(v.headType)) ||
      !scJsonGetUInt32(json, tpSignedCalText.t012, &v.headSerial) ||
      !scJsonGetString(json, SC_FIELD_DEVICE_ADJUSTMENT_ID, v.deviceAdjustmentCalibrationId, sizeof(v.deviceAdjustmentCalibrationId)) ||
      !scJsonGetString(json, SC_FIELD_DEVICE_ADJUSTMENT_HASH, deviceHash, sizeof(deviceHash)) ||
      !scJsonGetString(json, SC_FIELD_HEAD_ADJUSTMENT_ID, v.headAdjustmentCalibrationId, sizeof(v.headAdjustmentCalibrationId)) ||
      !scJsonGetString(json, SC_FIELD_HEAD_ADJUSTMENT_HASH, headHash, sizeof(headHash)) ||
      !scHexToBytes(deviceHash, v.deviceAdjustmentManifest, 32U) ||
      !scHexToBytes(headHash, v.headAdjustmentManifest, 32U)) return false;
  if (!v2) return true;
  char externalHash[72];
  return scJsonGetUInt8(json, SC_FIELD_EXTERNAL_CERT_AVAILABLE, &v.externalCertificateAvailable) &&
         scJsonGetString(json, SC_FIELD_EXTERNAL_CERT_DOCUMENT_ID, v.externalCertificateDocumentId, sizeof(v.externalCertificateDocumentId)) &&
         scJsonGetString(json, SC_FIELD_EXTERNAL_CERT_NUMBER, v.externalCertificateNumber, sizeof(v.externalCertificateNumber)) &&
         scJsonGetUInt32(json, SC_FIELD_EXTERNAL_CERT_VALID_FROM, &v.externalCertificateValidFromYmd) &&
         scJsonGetUInt32(json, SC_FIELD_EXTERNAL_CERT_VALID_UNTIL, &v.externalCertificateValidUntilYmd) &&
         scJsonGetString(json, SC_FIELD_EXTERNAL_CERT_ORIGINAL_NAME, v.externalCertificateOriginalFileName, sizeof(v.externalCertificateOriginalFileName)) &&
         scJsonGetUInt32(json, SC_FIELD_EXTERNAL_CERT_FILE_SIZE, &v.externalCertificateFileSize) &&
         scJsonGetString(json, SC_FIELD_EXTERNAL_CERT_SHA256, externalHash, sizeof(externalHash)) &&
         scHexToBytes(externalHash, v.externalCertificateSha256, 32U);
}

static bool FLASHMEM scSystemExternalValuesValid(const TpSystemCalValues& v, bool v2)
{
  if (!v2) return true;
  if (v.externalCertificateAvailable > 1U) return false;
  if (v.externalCertificateAvailable == 0U)
  {
    if (v.externalCertificateDocumentId[0] != '\0' ||
        v.externalCertificateNumber[0] != '\0' ||
        v.externalCertificateValidFromYmd != 0U ||
        v.externalCertificateValidUntilYmd != 0U ||
        v.externalCertificateOriginalFileName[0] != '\0' ||
        v.externalCertificateFileSize != 0U) return false;
    for (size_t i = 0U; i < sizeof(v.externalCertificateSha256); ++i)
      if (v.externalCertificateSha256[i] != 0U) return false;
    return true;
  }

  const size_t numberLength = strlen(v.externalCertificateNumber);
  const size_t nameLength = strlen(v.externalCertificateOriginalFileName);
  uint8_t documentIdBytes[16];
  bool hashNonZero = false;
  for (size_t i = 0U; i < sizeof(v.externalCertificateSha256); ++i)
    hashNonZero = hashNonZero || v.externalCertificateSha256[i] != 0U;
  return strlen(v.externalCertificateDocumentId) == 32U &&
         scHexToBytes(v.externalCertificateDocumentId, documentIdBytes, sizeof(documentIdBytes)) &&
         numberLength >= 1U && numberLength <= 32U &&
         scValidDateYmd(v.externalCertificateValidFromYmd) &&
         scValidDateYmd(v.externalCertificateValidUntilYmd) &&
         v.externalCertificateValidUntilYmd >= v.externalCertificateValidFromYmd &&
         nameLength >= 5U && nameLength <= 80U &&
         strcasecmp(v.externalCertificateOriginalFileName + nameLength - 4U, ".pdf") == 0 &&
         v.externalCertificateFileSize >= 5U &&
         v.externalCertificateFileSize <= TP_EXTERNAL_CAL_MAX_PDF_BYTES &&
         hashNonZero;
}

static bool FLASHMEM scParseApproval(const char* json, TpCalApproval& a, uint8_t version)
{
  char validFrom[48];
  char validUntil[48];
  memset(&a, 0, sizeof(a));
  if (!scJsonGetString(json, tpSignedCalText.t035, a.calibrationId, sizeof(a.calibrationId)) ||
      !scJsonGetUInt32(json, tpSignedCalText.t003, &a.calibrationDateYmd) ||
      !scJsonGetString(json, tpSignedCalText.t036, validFrom, sizeof(validFrom)) ||
      !scJsonGetString(json, tpSignedCalText.t037, validUntil, sizeof(validUntil)) ||
      !scParseIsoUtc(validFrom, &a.validFromUtc) ||
      !scParseIsoUtc(validUntil, &a.validUntilUtc) ||
      !scJsonGetUInt16(json, tpSignedCalText.t038, &a.intervalMonths) ||
      !scJsonGetString(json, tpSignedCalText.t039, a.laboratory, sizeof(a.laboratory)) ||
      !scJsonGetString(json, tpSignedCalText.t040, a.operatorName, sizeof(a.operatorName)) ||
      !scJsonGetString(json, tpSignedCalText.t041, a.certificateReference, sizeof(a.certificateReference)) ||
      !scJsonGetString(json, tpSignedCalText.t042, a.reason, sizeof(a.reason)))
  {
    return false;
  }
  if (version < 2U) return true;
  if (!scJsonGetString(json, SC_FIELD_REFERENCE_STANDARDS, a.referenceStandards, sizeof(a.referenceStandards)) ||
      !scJsonGetString(json, SC_FIELD_TRACEABILITY, a.traceability, sizeof(a.traceability)) ||
      !scJsonGetString(json, SC_FIELD_MEASUREMENT_UNCERTAINTY, a.measurementUncertainty, sizeof(a.measurementUncertainty)) ||
      !scJsonGetString(json, SC_FIELD_ENVIRONMENTAL_CONDITIONS, a.environmentalConditions, sizeof(a.environmentalConditions)) ||
      !scJsonGetString(json, SC_FIELD_CALIBRATION_PROCEDURE, a.calibrationProcedure, sizeof(a.calibrationProcedure)) ||
      !scJsonGetString(json, SC_FIELD_ACCREDITATION_INFORMATION, a.accreditationInformation, sizeof(a.accreditationInformation)))
  {
    return false;
  }
  if (version < 3U)
  {
    scSetText(a.calibrationScope, sizeof(a.calibrationScope), "NONE");
    return true;
  }
  if (!scJsonGetString(json, SC_FIELD_CALIBRATION_SCOPE, a.calibrationScope, sizeof(a.calibrationScope)) ||
      !scJsonGetString(json, SC_FIELD_CALIBRATION_RESULTS_ENCODING, a.calibrationResultsEncoding, sizeof(a.calibrationResultsEncoding)) ||
      !scJsonGetString(json, SC_FIELD_CALIBRATION_RESULTS_BASE64URL, a.calibrationResultsBase64Url, sizeof(a.calibrationResultsBase64Url)) ||
      !scCalibrationResultsFieldsValid(a)) return false;
  if (version < 4U)
  {
    scSetText(a.certificateSource, sizeof(a.certificateSource), TP_CAL_CERT_SOURCE_TP3000);
    return true;
  }
  return scJsonGetString(json, SC_FIELD_CERTIFICATE_SOURCE, a.certificateSource, sizeof(a.certificateSource));
}

static bool FLASHMEM scHasPermission(const char* permissions, const char* permission)
{
  if (permissions == nullptr || permission == nullptr || *permission == '\0') return false;
  const size_t wanted = strlen(permission);
  const char* p = permissions;
  while (*p != '\0')
  {
    while (*p == ',' || *p == ' ' || *p == '\t') ++p;
    const char* start = p;
    while (*p != '\0' && *p != ',') ++p;
    const char* finish = p;
    while (finish > start && (finish[-1] == ' ' || finish[-1] == '\t')) --finish;
    if ((size_t)(finish - start) == wanted && strncmp(start, permission, wanted) == 0) return true;
    if (*p == ',') ++p;
  }
  return false;
}

static bool FLASHMEM scParseRoleCertificate(const char* json,
                                            TpRoleCertificateParsed& role,
                                            char* errorText,
                                            size_t errorTextSize)
{
  char format[48];
  char product[16];
  char spkiBase64[160];
  char certificateSerial[40];
  char issuedText[48];
  char notBeforeText[48];
  char notAfterText[48];
  char manifestText[72];
  char algorithm[32];
  char encoding[24];
  char signatureBase64[120];
  memset(&role, 0, sizeof(role));
  if (!scJsonGetString(json, tpSignedCalText.t043, format, sizeof(format)) ||
      !scJsonGetString(json, tpSignedCalText.t044, product, sizeof(product)) ||
      !scJsonGetString(json, tpSignedCalText.t045, role.role, sizeof(role.role)) ||
      !scJsonGetString(json, tpSignedCalText.t046, role.keyName, sizeof(role.keyName)) ||
      !scJsonGetString(json, tpSignedCalText.t047, role.keyId, sizeof(role.keyId)) ||
      !scJsonGetString(json, tpSignedCalText.t048, spkiBase64, sizeof(spkiBase64)) ||
      !scJsonGetString(json, tpSignedCalText.t049, role.issuerKeyId, sizeof(role.issuerKeyId)) ||
      !scJsonGetString(json, tpSignedCalText.t050, certificateSerial, sizeof(certificateSerial)) ||
      !scJsonGetString(json, tpSignedCalText.t051, issuedText, sizeof(issuedText)) ||
      !scJsonGetString(json, tpSignedCalText.t052, notBeforeText, sizeof(notBeforeText)) ||
      !scJsonGetString(json, tpSignedCalText.t053, notAfterText, sizeof(notAfterText)) ||
      !scJsonGetRootString(json, tpSignedCalText.t054, manifestText, sizeof(manifestText)) ||
      !scJsonGetRootString(json, tpSignedCalText.t055, algorithm, sizeof(algorithm)) ||
      !scJsonGetRootString(json, tpSignedCalText.t056, encoding, sizeof(encoding)) ||
      !scJsonGetString(json, tpSignedCalText.t057, signatureBase64, sizeof(signatureBase64)))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t058);
    return false;
  }

  role.legacy = strcmp(format, TP_CAL_ROLE_FORMAT_V1) == 0;
  const bool v2 = strcmp(format, TP_CAL_ROLE_FORMAT_V2) == 0;
  if ((!role.legacy && !v2) ||
      strcmp(product, TP_CAL_PRODUCT) != 0 ||
      strcmp(role.issuerKeyId, deviceIdentityRootKeyId()) != 0 ||
      strcmp(algorithm, TP_CAL_SIGNATURE_ALGORITHM) != 0 ||
      strcmp(encoding, TP_CAL_SIGNATURE_ENCODING) != 0 ||
      (role.legacy && strcmp(role.role, TP_CAL_ROLE_NAME_V1) != 0) ||
      (v2 && strcmp(role.role, TP_CAL_ROLE_NAME_V2) != 0))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t059);
    return false;
  }

  if (v2)
  {
    if (!scJsonGetString(json, SC_FIELD_LAB_ID, role.labId, sizeof(role.labId)) ||
        !scJsonGetString(json, SC_FIELD_LAB_NAME, role.labName, sizeof(role.labName)) ||
        !scJsonGetString(json, SC_FIELD_LAB_ADDRESS, role.labAddress, sizeof(role.labAddress)) ||
        !scJsonGetString(json, SC_FIELD_PERMISSIONS, role.permissions, sizeof(role.permissions)) ||
        role.labId[0] == '\0' || role.labName[0] == '\0' ||
        !scHasPermission(role.permissions, TP_CAL_PERMISSION_SIGN))
    {
      scSetText(errorText, errorTextSize, tpSignedCalText.t059);
      return false;
    }
  }

  size_t spkiLength = 0U;
  if (!scBase64Decode(spkiBase64, role.publicSpki, sizeof(role.publicSpki), &spkiLength) || spkiLength != 91U)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t060);
    return false;
  }
  static const uint8_t spkiPrefix[27] TP_SIGNED_CAL_RODATA =
  {
    0x30,0x59,0x30,0x13,0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01,
    0x06,0x08,0x2A,0x86,0x48,0xCE,0x3D,0x03,0x01,0x07,0x03,0x42,0x00,0x04
  };
  if (!scBytesEqual(role.publicSpki, spkiPrefix, sizeof(spkiPrefix)))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t061);
    return false;
  }
  memcpy(role.publicRaw, role.publicSpki + 27U, 64U);
  uint8_t spkiHash[32];
  TpSha256::hash(role.publicSpki, sizeof(role.publicSpki), spkiHash);
  char calculatedKeyId[17];
  scBytesToHex(spkiHash, 8U, calculatedKeyId, sizeof(calculatedKeyId), true);
  if (strcmp(calculatedKeyId, role.keyId) != 0)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t062);
    return false;
  }
  if (!scParseIsoUtc(issuedText, &role.issuedUtc) ||
      !scParseIsoUtc(notBeforeText, &role.notBeforeUtc) ||
      !scParseIsoUtc(notAfterText, &role.notAfterUtc) ||
      role.notAfterUtc <= role.notBeforeUtc || role.issuedUtc > role.notAfterUtc ||
      (v2 && role.notAfterUtc - role.notBeforeUtc > TP_CAL_LAB_MAX_VALIDITY_SECONDS) ||
      !scHexToBytes(manifestText, role.manifestSha256, sizeof(role.manifestSha256)))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t063);
    return false;
  }
  uint8_t rootSignature[64];
  size_t signatureLength = 0U;
  if (!scBase64Decode(signatureBase64, rootSignature, sizeof(rootSignature), &signatureLength) || signatureLength != 64U)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t064);
    return false;
  }
  scSetText(role.certificateSerial, sizeof(role.certificateSerial), certificateSerial);
  memcpy(role.rootSignature, rootSignature, sizeof(role.rootSignature));

  TpCanonicalWriter w(tpSignedCalCanonical, sizeof(tpSignedCalCanonical));
  bool canonicalOk =
      w.writeUtf8(format) && w.writeUtf8(product) && w.writeUtf8(role.role) &&
      w.writeUtf8(role.keyName) && w.writeUtf8(role.keyId) &&
      w.writeBytes(role.publicSpki, sizeof(role.publicSpki)) &&
      w.writeUtf8(role.issuerKeyId) && w.writeUtf8(certificateSerial) &&
      w.writeI64(role.issuedUtc) && w.writeI64(role.notBeforeUtc) && w.writeI64(role.notAfterUtc);
  if (canonicalOk && v2)
  {
    canonicalOk = w.writeUtf8(role.labId) && w.writeUtf8(role.labName) &&
                  w.writeUtf8(role.labAddress) && w.writeUtf8(role.permissions);
  }
  if (!canonicalOk || !w.valid())
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t065);
    return false;
  }
  uint8_t calculatedManifest[32];
  TpSha256::hash(tpSignedCalCanonical, w.length(), calculatedManifest);
  if (!scBytesEqual(calculatedManifest, role.manifestSha256, 32U) ||
      !deviceIdentityVerifyRootSignedHash(calculatedManifest, rootSignature))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t066);
    return false;
  }
  return true;
}

static bool FLASHMEM scParseCommonPackage(const char* json,
                                          const char* expectedFormat,
                                          char* deviceSerial,
                                          size_t deviceSerialSize,
                                          char* deviceKeyId,
                                          size_t deviceKeyIdSize,
                                          char* deviceCertificateSerial,
                                          size_t deviceCertificateSerialSize,
                                          char* firmwareVersion,
                                          size_t firmwareVersionSize,
                                          char* firmwareBuildId,
                                          size_t firmwareBuildIdSize,
                                          uint8_t sourceRequestHash[32],
                                          TpCalApproval& approval,
                                          int64_t* approvedUtc,
                                          char* signerKeyId,
                                          size_t signerKeyIdSize,
                                          uint8_t manifestHash[32],
                                          uint8_t signature[64],
                                          TpRoleCertificateParsed& role,
                                          char* errorText,
                                          size_t errorTextSize)
{
  char format[48];
  char product[16];
  char sourceHashText[72];
  char approvedText[48];
  char manifestText[72];
  char algorithm[32];
  char encoding[24];
  char signatureBase64[120];
  if (!scJsonGetString(json, tpSignedCalText.t043, format, sizeof(format)) ||
      !scJsonGetString(json, tpSignedCalText.t044, product, sizeof(product)) ||
      !scJsonGetString(json, tpSignedCalText.t067, deviceSerial, deviceSerialSize) ||
      !scJsonGetString(json, tpSignedCalText.t068, deviceKeyId, deviceKeyIdSize) ||
      !scJsonGetString(json, tpSignedCalText.t069, deviceCertificateSerial, deviceCertificateSerialSize) ||
      !scJsonGetString(json, tpSignedCalText.t070, firmwareVersion, firmwareVersionSize) ||
      !scJsonGetString(json, tpSignedCalText.t071, firmwareBuildId, firmwareBuildIdSize) ||
      !scJsonGetString(json, tpSignedCalText.t072, sourceHashText, sizeof(sourceHashText)) ||
      !scJsonGetObject(json, tpSignedCalText.t073, tpSignedCalObjectA, sizeof(tpSignedCalObjectA)) ||
      !scParseApproval(tpSignedCalObjectA, approval, 1U) ||
      !scJsonGetString(json, tpSignedCalText.t074, approvedText, sizeof(approvedText)) ||
      !scParseIsoUtc(approvedText, approvedUtc) ||
      !scJsonGetObject(json, tpSignedCalText.t075, tpSignedCalObjectB, sizeof(tpSignedCalObjectB)) ||
      !scParseRoleCertificate(tpSignedCalObjectB, role, errorText, errorTextSize) ||
      !scJsonGetString(json, tpSignedCalText.t076, signerKeyId, signerKeyIdSize) ||
      !scJsonGetRootString(json, tpSignedCalText.t054, manifestText, sizeof(manifestText)) ||
      !scJsonGetRootString(json, tpSignedCalText.t055, algorithm, sizeof(algorithm)) ||
      !scJsonGetRootString(json, tpSignedCalText.t056, encoding, sizeof(encoding)) ||
      !scJsonGetString(json, tpSignedCalText.t077, signatureBase64, sizeof(signatureBase64)))
  {
    if (errorText != nullptr && errorText[0] == '\0') scSetText(errorText, errorTextSize, tpSignedCalText.t078);
    return false;
  }
  size_t signatureLength = 0U;
  if (strcmp(format, expectedFormat) != 0 || strcmp(product, TP_CAL_PRODUCT) != 0 ||
      strcmp(algorithm, TP_CAL_SIGNATURE_ALGORITHM) != 0 ||
      strcmp(encoding, TP_CAL_SIGNATURE_ENCODING) != 0 ||
      strcmp(deviceSerial, deviceIdentityCertifiedSerial()) != 0 ||
      strcmp(deviceKeyId, deviceIdentityDeviceKeyId()) != 0 ||
      strcmp(deviceCertificateSerial, deviceIdentityCertificateSerialText()) != 0 ||
      strcmp(signerKeyId, role.keyId) != 0 ||
      !scHexToBytes(sourceHashText, sourceRequestHash, 32U) ||
      !scHexToBytes(manifestText, manifestHash, 32U) ||
      !scBase64Decode(signatureBase64, signature, 64U, &signatureLength) || signatureLength != 64U ||
      approval.validUntilUtc <= approval.validFromUtc || approval.intervalMonths == 0U ||
      *approvedUtc < role.notBeforeUtc || *approvedUtc > role.notAfterUtc)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t079);
    return false;
  }
  return true;
}


static bool FLASHMEM scParseCommonPackageV2(const char* json,
                                            const char* expectedFormat,
                                            uint8_t packageVersion,
                                            char* deviceSerial,
                                            size_t deviceSerialSize,
                                            char* deviceKeyId,
                                            size_t deviceKeyIdSize,
                                            char* deviceCertificateSerial,
                                            size_t deviceCertificateSerialSize,
                                            char* firmwareVersion,
                                            size_t firmwareVersionSize,
                                            char* firmwareBuildId,
                                            size_t firmwareBuildIdSize,
                                            uint8_t sourceRequestHash[32],
                                            char sourceRequestId[33],
                                            int64_t* sourceRequestCreatedUtc,
                                            int64_t* sourceRequestExpiresUtc,
                                            TpCalApproval& approval,
                                            int64_t* approvedUtc,
                                            char* signerType,
                                            size_t signerTypeSize,
                                            char* signerKeyId,
                                            size_t signerKeyIdSize,
                                            uint8_t signerCertificateManifest[32],
                                            uint8_t manifestHash[32],
                                            uint8_t signature[64],
                                            TpRoleCertificateParsed& role,
                                            bool* rootSigner,
                                            char* errorText,
                                            size_t errorTextSize)
{
  char format[48];
  char product[16];
  char sourceHashText[72];
  char sourceCreatedText[48];
  char sourceExpiresText[48];
  char approvedText[48];
  char manifestText[72];
  char algorithm[32];
  char encoding[24];
  char signatureBase64[120];
  memset(signerCertificateManifest, 0, 32U);
  memset(&role, 0, sizeof(role));
  *rootSigner = false;

  if (!scJsonGetString(json, tpSignedCalText.t043, format, sizeof(format)) ||
      !scJsonGetString(json, tpSignedCalText.t044, product, sizeof(product)) ||
      !scJsonGetString(json, tpSignedCalText.t067, deviceSerial, deviceSerialSize) ||
      !scJsonGetString(json, tpSignedCalText.t068, deviceKeyId, deviceKeyIdSize) ||
      !scJsonGetString(json, tpSignedCalText.t069, deviceCertificateSerial, deviceCertificateSerialSize) ||
      !scJsonGetString(json, tpSignedCalText.t070, firmwareVersion, firmwareVersionSize) ||
      !scJsonGetString(json, tpSignedCalText.t071, firmwareBuildId, firmwareBuildIdSize) ||
      !scJsonGetString(json, tpSignedCalText.t072, sourceHashText, sizeof(sourceHashText)) ||
      !scJsonGetString(json, SC_FIELD_SOURCE_REQUEST_ID, sourceRequestId, 33U) ||
      !scJsonGetString(json, SC_FIELD_SOURCE_REQUEST_CREATED, sourceCreatedText, sizeof(sourceCreatedText)) ||
      !scJsonGetString(json, SC_FIELD_SOURCE_REQUEST_EXPIRES, sourceExpiresText, sizeof(sourceExpiresText)) ||
      !scJsonGetObject(json, tpSignedCalText.t073, tpSignedCalObjectA, sizeof(tpSignedCalObjectA)) ||
      !scParseApproval(tpSignedCalObjectA, approval, packageVersion) ||
      !scJsonGetString(json, tpSignedCalText.t074, approvedText, sizeof(approvedText)) ||
      !scParseIsoUtc(approvedText, approvedUtc) ||
      !scJsonGetString(json, SC_FIELD_SIGNER_TYPE, signerType, signerTypeSize) ||
      !scJsonGetString(json, tpSignedCalText.t076, signerKeyId, signerKeyIdSize) ||
      !scJsonGetRootString(json, tpSignedCalText.t054, manifestText, sizeof(manifestText)) ||
      !scJsonGetRootString(json, tpSignedCalText.t055, algorithm, sizeof(algorithm)) ||
      !scJsonGetRootString(json, tpSignedCalText.t056, encoding, sizeof(encoding)) ||
      !scJsonGetString(json, tpSignedCalText.t077, signatureBase64, sizeof(signatureBase64)))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t078);
    return false;
  }

  if (!scParseIsoUtc(sourceCreatedText, sourceRequestCreatedUtc) ||
      !scParseIsoUtc(sourceExpiresText, sourceRequestExpiresUtc))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t079);
    return false;
  }

  if (strcmp(signerType, TP_CAL_SIGNER_LAB) == 0)
  {
    if (!scJsonGetObject(json, tpSignedCalText.t075, tpSignedCalObjectB, sizeof(tpSignedCalObjectB)) ||
        !scParseRoleCertificate(tpSignedCalObjectB, role, errorText, errorTextSize) ||
        role.legacy || strcmp(role.role, TP_CAL_ROLE_NAME_V2) != 0 ||
        strcmp(signerKeyId, role.keyId) != 0 ||
        strcmp(approval.laboratory, role.labName) != 0 ||
        *approvedUtc < role.notBeforeUtc || *approvedUtc > role.notAfterUtc)
    {
      if (errorText != nullptr && errorText[0] == '\0') scSetText(errorText, errorTextSize, tpSignedCalText.t079);
      return false;
    }
    memcpy(signerCertificateManifest, role.manifestSha256, 32U);
  }
  else if (strcmp(signerType, TP_CAL_SIGNER_ROOT) == 0)
  {
    if (strcmp(signerKeyId, deviceIdentityRootKeyId()) != 0)
    {
      scSetText(errorText, errorTextSize, tpSignedCalText.t079);
      return false;
    }
    *rootSigner = true;
  }
  else
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t079);
    return false;
  }

  uint8_t sourceRequestIdBytes[16];
  size_t signatureLength = 0U;
  if (strcmp(format, expectedFormat) != 0 || strcmp(product, TP_CAL_PRODUCT) != 0 ||
      strcmp(algorithm, TP_CAL_SIGNATURE_ALGORITHM) != 0 ||
      strcmp(encoding, TP_CAL_SIGNATURE_ENCODING) != 0 ||
      strcmp(deviceSerial, deviceIdentityCertifiedSerial()) != 0 ||
      strcmp(deviceKeyId, deviceIdentityDeviceKeyId()) != 0 ||
      strcmp(deviceCertificateSerial, deviceIdentityCertificateSerialText()) != 0 ||
      !scHexToBytes(sourceHashText, sourceRequestHash, 32U) ||
      !scHexToBytes(sourceRequestId, sourceRequestIdBytes, sizeof(sourceRequestIdBytes)) ||
      !scHexToBytes(manifestText, manifestHash, 32U) ||
      !scBase64Decode(signatureBase64, signature, 64U, &signatureLength) || signatureLength != 64U ||
      approval.validUntilUtc <= approval.validFromUtc || approval.intervalMonths == 0U ||
      *sourceRequestExpiresUtc != *sourceRequestCreatedUtc + TP_CAL_REQUEST_VALIDITY_SECONDS ||
      *approvedUtc < *sourceRequestCreatedUtc || *approvedUtc > *sourceRequestExpiresUtc)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t079);
    return false;
  }
  return true;
}

static bool FLASHMEM scVerifyDevicePackage(const char* json,
                                           TpSignedCalibrationStatus& status,
                                           TpDeviceCalValues& signedValues)
{
  memset(&status, 0, sizeof(status));
  status.filePresent = true;
  char format[48];
  if (!scJsonGetString(json, tpSignedCalText.t043, format, sizeof(format)))
  {
    scSetError(status, tpSignedCalText.t081);
    return false;
  }
  const bool v3 = strcmp(format, TP_CAL_FORMAT_DEVICE_PACKAGE) == 0;
  const bool v2 = strcmp(format, TP_CAL_FORMAT_DEVICE_PACKAGE_V2) == 0;
  const bool v1 = strcmp(format, TP_CAL_FORMAT_DEVICE_PACKAGE_V1) == 0;
  if (!v1 && !v2 && !v3)
  {
    scSetError(status, tpSignedCalText.t081);
    return false;
  }

  char deviceSerial[8];
  char deviceKeyId[20];
  char deviceCertificateSerial[40];
  char firmwareVersion[40];
  char firmwareBuildId[48];
  uint8_t sourceRequestHash[32];
  TpCalApproval& approval = tpSignedCalApproval;
  memset(&approval, 0, sizeof(approval));
  int64_t approvedUtc = 0;
  char signerKeyId[20];
  uint8_t storedManifest[32];
  uint8_t signature[64];
  TpRoleCertificateParsed& role = tpSignedCalRole;
  memset(&role, 0, sizeof(role));
  char error[128] = {0};
  char sourceRequestId[33] = {0};
  int64_t sourceRequestCreatedUtc = 0;
  int64_t sourceRequestExpiresUtc = 0;
  char signerType[32] = {0};
  uint8_t signerCertificateManifest[32] = {0};
  bool rootSigner = false;

  if (!scJsonGetObject(json, tpSignedCalText.t080, tpSignedCalObjectA, sizeof(tpSignedCalObjectA)) ||
      !scParseDeviceValues(tpSignedCalObjectA, signedValues))
  {
    scSetError(status, tpSignedCalText.t081);
    return false;
  }

  bool commonOk = false;
  if (v2 || v3)
  {
    commonOk = scParseCommonPackageV2(json,
                            v3 ? TP_CAL_FORMAT_DEVICE_PACKAGE : TP_CAL_FORMAT_DEVICE_PACKAGE_V2,
                            v3 ? 3U : 2U,
                            deviceSerial, sizeof(deviceSerial),
                            deviceKeyId, sizeof(deviceKeyId),
                            deviceCertificateSerial, sizeof(deviceCertificateSerial),
                            firmwareVersion, sizeof(firmwareVersion),
                            firmwareBuildId, sizeof(firmwareBuildId),
                            sourceRequestHash,
                            sourceRequestId,
                            &sourceRequestCreatedUtc,
                            &sourceRequestExpiresUtc,
                            approval,
                            &approvedUtc,
                            signerType, sizeof(signerType),
                            signerKeyId, sizeof(signerKeyId),
                            signerCertificateManifest,
                            storedManifest,
                            signature,
                            role,
                            &rootSigner,
                            error, sizeof(error));
  }
  else
  {
    commonOk = scParseCommonPackage(json,
                            TP_CAL_FORMAT_DEVICE_PACKAGE_V1,
                            deviceSerial, sizeof(deviceSerial),
                            deviceKeyId, sizeof(deviceKeyId),
                            deviceCertificateSerial, sizeof(deviceCertificateSerial),
                            firmwareVersion, sizeof(firmwareVersion),
                            firmwareBuildId, sizeof(firmwareBuildId),
                            sourceRequestHash,
                            approval,
                            &approvedUtc,
                            signerKeyId, sizeof(signerKeyId),
                            storedManifest,
                            signature,
                            role,
                            error, sizeof(error));
  }
  if (!commonOk)
  {
    scSetError(status, error[0] ? error : tpSignedCalText.t081);
    return false;
  }

  TpCanonicalWriter w(tpSignedCalCanonical, sizeof(tpSignedCalCanonical));
  bool canonicalOk = w.writeUtf8(format) && w.writeUtf8(TP_CAL_PRODUCT) &&
      w.writeUtf8(deviceSerial) && w.writeUtf8(deviceKeyId) &&
      w.writeUtf8(deviceCertificateSerial) && scWriteDeviceValues(w, signedValues) &&
      w.writeUtf8(firmwareVersion) && w.writeUtf8(firmwareBuildId) &&
      w.writeHash32(sourceRequestHash);
  if (canonicalOk && (v2 || v3))
  {
    canonicalOk = w.writeUtf8(sourceRequestId) &&
                  w.writeI64(sourceRequestCreatedUtc) &&
                  w.writeI64(sourceRequestExpiresUtc) &&
                  (v3 ? scWriteApprovalV3(w, approval, approvedUtc, signerType, signerKeyId, signerCertificateManifest)
                      : scWriteApprovalV2(w, approval, approvedUtc, signerType, signerKeyId, signerCertificateManifest));
  }
  else if (canonicalOk)
  {
    canonicalOk = scWriteApprovalV1(w, approval, approvedUtc, signerKeyId, role.manifestSha256);
  }
  if (!canonicalOk || !w.valid())
  {
    scSetError(status, tpSignedCalText.t082);
    return false;
  }
  if (approval.calibrationDateYmd != signedValues.calibrationDateYmd)
  {
    scSetError(status, tpSignedCalText.t083);
    return false;
  }
  uint8_t calculatedManifest[32];
  TpSha256::hash(tpSignedCalCanonical, w.length(), calculatedManifest);
  bool signatureOk = scBytesEqual(calculatedManifest, storedManifest, 32U);
  if (signatureOk)
  {
    signatureOk = rootSigner
      ? deviceIdentityVerifyRootSignedHash(calculatedManifest, signature)
      : (uECC_verify(role.publicRaw, calculatedManifest, 32U, signature, uECC_secp256r1()) == 1);
  }
  if (!signatureOk)
  {
    scSetError(status, tpSignedCalText.t084);
    return false;
  }
  TpDeviceCalValues current;
  scCurrentDeviceValues(current);
  status.signatureValid = true;
  status.valuesMatchCurrent = scDeviceValuesEqual(current, signedValues);
  scSetText(status.calibrationId, sizeof(status.calibrationId), approval.calibrationId);
  status.calibrationDateYmd = approval.calibrationDateYmd;
  status.validFromUtc = approval.validFromUtc;
  status.validUntilUtc = approval.validUntilUtc;
  status.intervalMonths = approval.intervalMonths;
  scSetText(status.signerKeyId, sizeof(status.signerKeyId), signerKeyId);
  scBytesToHex(storedManifest, 32U, status.manifestSha256, sizeof(status.manifestSha256), false);
  scSetError(status, status.valuesMatchCurrent ? tpSignedCalText.t000 : tpSignedCalText.t085);
  return status.valuesMatchCurrent;
}

static bool FLASHMEM scVerifyHeadPackage(const char* json,
                                         TpSignedCalibrationStatus& status,
                                         TpHeadCalValues& signedValues)
{
  memset(&status, 0, sizeof(status));
  status.filePresent = true;
  char format[48];
  if (!scJsonGetString(json, tpSignedCalText.t043, format, sizeof(format)))
  {
    scSetError(status, tpSignedCalText.t086);
    return false;
  }
  const bool v3 = strcmp(format, TP_CAL_FORMAT_HEAD_PACKAGE) == 0;
  const bool v2 = strcmp(format, TP_CAL_FORMAT_HEAD_PACKAGE_V2) == 0;
  const bool v1 = strcmp(format, TP_CAL_FORMAT_HEAD_PACKAGE_V1) == 0;
  if (!v1 && !v2 && !v3)
  {
    scSetError(status, tpSignedCalText.t086);
    return false;
  }

  char deviceSerial[8];
  char deviceKeyId[20];
  char deviceCertificateSerial[40];
  char firmwareVersion[40];
  char firmwareBuildId[48];
  uint8_t sourceRequestHash[32];
  TpCalApproval& approval = tpSignedCalApproval;
  memset(&approval, 0, sizeof(approval));
  int64_t approvedUtc = 0;
  char signerKeyId[20];
  uint8_t storedManifest[32];
  uint8_t signature[64];
  TpRoleCertificateParsed& role = tpSignedCalRole;
  memset(&role, 0, sizeof(role));
  char error[128] = {0};
  char sourceRequestId[33] = {0};
  int64_t sourceRequestCreatedUtc = 0;
  int64_t sourceRequestExpiresUtc = 0;
  char signerType[32] = {0};
  uint8_t signerCertificateManifest[32] = {0};
  bool rootSigner = false;

  if (!scJsonGetObject(json, tpSignedCalText.t080, tpSignedCalObjectA, sizeof(tpSignedCalObjectA)) ||
      !scParseHeadValues(tpSignedCalObjectA, signedValues))
  {
    scSetError(status, tpSignedCalText.t086);
    return false;
  }

  bool commonOk = false;
  if (v2 || v3)
  {
    commonOk = scParseCommonPackageV2(json,
                            v3 ? TP_CAL_FORMAT_HEAD_PACKAGE : TP_CAL_FORMAT_HEAD_PACKAGE_V2,
                            v3 ? 3U : 2U,
                            deviceSerial, sizeof(deviceSerial),
                            deviceKeyId, sizeof(deviceKeyId),
                            deviceCertificateSerial, sizeof(deviceCertificateSerial),
                            firmwareVersion, sizeof(firmwareVersion),
                            firmwareBuildId, sizeof(firmwareBuildId),
                            sourceRequestHash,
                            sourceRequestId,
                            &sourceRequestCreatedUtc,
                            &sourceRequestExpiresUtc,
                            approval,
                            &approvedUtc,
                            signerType, sizeof(signerType),
                            signerKeyId, sizeof(signerKeyId),
                            signerCertificateManifest,
                            storedManifest,
                            signature,
                            role,
                            &rootSigner,
                            error, sizeof(error));
  }
  else
  {
    commonOk = scParseCommonPackage(json,
                            TP_CAL_FORMAT_HEAD_PACKAGE_V1,
                            deviceSerial, sizeof(deviceSerial),
                            deviceKeyId, sizeof(deviceKeyId),
                            deviceCertificateSerial, sizeof(deviceCertificateSerial),
                            firmwareVersion, sizeof(firmwareVersion),
                            firmwareBuildId, sizeof(firmwareBuildId),
                            sourceRequestHash,
                            approval,
                            &approvedUtc,
                            signerKeyId, sizeof(signerKeyId),
                            storedManifest,
                            signature,
                            role,
                            error, sizeof(error));
  }
  if (!commonOk)
  {
    scSetError(status, error[0] ? error : tpSignedCalText.t086);
    return false;
  }

  TpCanonicalWriter w(tpSignedCalCanonical, sizeof(tpSignedCalCanonical));
  bool canonicalOk = w.writeUtf8(format) && w.writeUtf8(TP_CAL_PRODUCT) &&
      w.writeUtf8(deviceSerial) && w.writeUtf8(deviceKeyId) &&
      w.writeUtf8(deviceCertificateSerial) && scWriteHeadValues(w, signedValues) &&
      w.writeUtf8(firmwareVersion) && w.writeUtf8(firmwareBuildId) &&
      w.writeHash32(sourceRequestHash);
  if (canonicalOk && (v2 || v3))
  {
    canonicalOk = w.writeUtf8(sourceRequestId) &&
                  w.writeI64(sourceRequestCreatedUtc) &&
                  w.writeI64(sourceRequestExpiresUtc) &&
                  (v3 ? scWriteApprovalV3(w, approval, approvedUtc, signerType, signerKeyId, signerCertificateManifest)
                      : scWriteApprovalV2(w, approval, approvedUtc, signerType, signerKeyId, signerCertificateManifest));
  }
  else if (canonicalOk)
  {
    canonicalOk = scWriteApprovalV1(w, approval, approvedUtc, signerKeyId, role.manifestSha256);
  }
  if (!canonicalOk || !w.valid())
  {
    scSetError(status, tpSignedCalText.t087);
    return false;
  }
  if (approval.calibrationDateYmd != signedValues.calibrationDateYmd)
  {
    scSetError(status, tpSignedCalText.t088);
    return false;
  }
  uint8_t calculatedManifest[32];
  TpSha256::hash(tpSignedCalCanonical, w.length(), calculatedManifest);
  bool signatureOk = scBytesEqual(calculatedManifest, storedManifest, 32U);
  if (signatureOk)
  {
    signatureOk = rootSigner
      ? deviceIdentityVerifyRootSignedHash(calculatedManifest, signature)
      : (uECC_verify(role.publicRaw, calculatedManifest, 32U, signature, uECC_secp256r1()) == 1);
  }
  if (!signatureOk)
  {
    scSetError(status, tpSignedCalText.t089);
    return false;
  }
  TpHeadCalValues current;
  scCurrentHeadValues(current);
  current.calibrationDateYmd = signedValues.calibrationDateYmd;
  current.calibrationTimeHms = signedValues.calibrationTimeHms;
  status.signatureValid = true;
  status.valuesMatchCurrent = scHeadValuesEqual(current, signedValues);
  scSetText(status.calibrationId, sizeof(status.calibrationId), approval.calibrationId);
  status.calibrationDateYmd = approval.calibrationDateYmd;
  status.validFromUtc = approval.validFromUtc;
  status.validUntilUtc = approval.validUntilUtc;
  status.intervalMonths = approval.intervalMonths;
  scSetText(status.signerKeyId, sizeof(status.signerKeyId), signerKeyId);
  scBytesToHex(storedManifest, 32U, status.manifestSha256, sizeof(status.manifestSha256), false);
  scSetError(status, status.valuesMatchCurrent ? tpSignedCalText.t000 : tpSignedCalText.t090);
  return status.valuesMatchCurrent;
}

static bool FLASHMEM scSystemBindingMatchesCurrent(const TpSystemCalValues& v, bool externalSource)
{
  if (!tpSignedCalibrationDeviceReady() || !tpSignedCalibrationHeadReady()) return false;
  if (strcmp(v.headType, headTypeTextGet()) != 0 || v.headSerial != R.head_serial) return false;
  if (strcmp(v.deviceAdjustmentCalibrationId, tpDeviceCalStatus.calibrationId) != 0 ||
      strcmp(v.headAdjustmentCalibrationId, tpHeadCalStatus.calibrationId) != 0) return false;
  uint8_t deviceManifest[32];
  uint8_t headManifest[32];
  if (!scHexToBytes(tpDeviceCalStatus.manifestSha256, deviceManifest, 32U) ||
      !scHexToBytes(tpHeadCalStatus.manifestSha256, headManifest, 32U)) return false;
  if (!scBytesEqual(v.deviceAdjustmentManifest, deviceManifest, 32U) ||
      !scBytesEqual(v.headAdjustmentManifest, headManifest, 32U)) return false;
  if (!externalSource) return true;
  if (!v.externalCertificateAvailable) return false;
  char externalHash[65];
  scBytesToHex(v.externalCertificateSha256, 32U, externalHash, sizeof(externalHash), false);
  return tpExternalCalibrationArchivedMatches(
      v.externalCertificateDocumentId,
      v.externalCertificateNumber,
      v.externalCertificateValidFromYmd,
      v.externalCertificateValidUntilYmd,
      v.externalCertificateOriginalFileName,
      v.externalCertificateFileSize,
      externalHash);
}

static bool FLASHMEM scVerifySystemPackage(const char* json,
                                           TpSystemCalibrationStatus& status,
                                           TpSystemCalValues& signedValues)
{
  memset(&status, 0, sizeof(status));
  status.filePresent = true;
  char format[48];
  if (!scJsonGetString(json, tpSignedCalText.t043, format, sizeof(format)))
  {
    scSetSystemError(status, "Systemkalibrierungsformat ist ungültig");
    return false;
  }
  const bool v2 = strcmp(format, TP_CAL_FORMAT_SYSTEM_PACKAGE) == 0;
  const bool v1 = strcmp(format, TP_CAL_FORMAT_SYSTEM_PACKAGE_V1) == 0;
  if (!v1 && !v2)
  {
    scSetSystemError(status, "Systemkalibrierungsformat ist ungültig");
    return false;
  }
  if (!scJsonGetObject(json, tpSignedCalText.t080, tpSignedCalObjectA, sizeof(tpSignedCalObjectA)) ||
      !scParseSystemValues(tpSignedCalObjectA, signedValues, v2) ||
      !scSystemExternalValuesValid(signedValues, v2))
  {
    scSetSystemError(status, "Systemkalibrierungswerte oder externe PDF-Metadaten sind unvollständig");
    return false;
  }

  char deviceSerial[8];
  char deviceKeyId[20];
  char deviceCertificateSerial[40];
  char firmwareVersion[40];
  char firmwareBuildId[48];
  uint8_t sourceRequestHash[32];
  char sourceRequestId[33] = {0};
  int64_t sourceRequestCreatedUtc = 0;
  int64_t sourceRequestExpiresUtc = 0;
  TpCalApproval& approval = tpSignedCalApproval;
  memset(&approval, 0, sizeof(approval));
  int64_t approvedUtc = 0;
  char signerType[32] = {0};
  char signerKeyId[20] = {0};
  uint8_t signerCertificateManifest[32] = {0};
  uint8_t storedManifest[32];
  uint8_t signature[64];
  TpRoleCertificateParsed& role = tpSignedCalRole;
  memset(&role, 0, sizeof(role));
  bool rootSigner = false;
  char error[160] = {0};
  if (!scParseCommonPackageV2(json,
                              format,
                              v2 ? 4U : 3U,
                              deviceSerial, sizeof(deviceSerial),
                              deviceKeyId, sizeof(deviceKeyId),
                              deviceCertificateSerial, sizeof(deviceCertificateSerial),
                              firmwareVersion, sizeof(firmwareVersion),
                              firmwareBuildId, sizeof(firmwareBuildId),
                              sourceRequestHash,
                              sourceRequestId,
                              &sourceRequestCreatedUtc,
                              &sourceRequestExpiresUtc,
                              approval,
                              &approvedUtc,
                              signerType, sizeof(signerType),
                              signerKeyId, sizeof(signerKeyId),
                              signerCertificateManifest,
                              storedManifest,
                              signature,
                              role,
                              &rootSigner,
                              error, sizeof(error)))
  {
    scSetSystemError(status, error[0] ? error : "Systemkalibrierung ist unvollständig");
    return false;
  }
  if (!scValidDateYmd(signedValues.calibrationDateYmd) || signedValues.headSerial == 0U ||
      signedValues.headSerial > HEAD_SERIAL_MAX || strlen(signedValues.deviceAdjustmentCalibrationId) < 8U ||
      strlen(signedValues.headAdjustmentCalibrationId) < 8U ||
      approval.calibrationDateYmd != signedValues.calibrationDateYmd ||
      (v2 && approval.operatorName[0] == '\0') ||
      strcmp(approval.calibrationScope, "NONE") == 0)
  {
    scSetSystemError(status, "Systemkalibrierung enthält keine vollständigen Kalibrierdaten");
    return false;
  }

  const bool externalSource = strcmp(approval.certificateSource, TP_CAL_CERT_SOURCE_EXTERNAL) == 0;
  const bool ownSource = strcmp(approval.certificateSource, TP_CAL_CERT_SOURCE_TP3000) == 0;
  if (!ownSource && !externalSource)
  {
    scSetSystemError(status, "Kalibrierscheinquelle ist ungültig");
    return false;
  }
  if (externalSource)
  {
    const time_t validFromTime = (time_t)approval.validFromUtc;
    const time_t validUntilTime = (time_t)approval.validUntilUtc;
    const uint32_t validFromYmd = (uint32_t)year(validFromTime) * 10000UL + (uint32_t)month(validFromTime) * 100UL + (uint32_t)day(validFromTime);
    const uint32_t validUntilYmd = (uint32_t)year(validUntilTime) * 10000UL + (uint32_t)month(validUntilTime) * 100UL + (uint32_t)day(validUntilTime);
    if (!v2 || !signedValues.externalCertificateAvailable ||
        signedValues.externalCertificateDocumentId[0] == '\0' ||
        signedValues.externalCertificateNumber[0] == '\0' ||
        signedValues.externalCertificateFileSize == 0U ||
        strcmp(approval.certificateReference, signedValues.externalCertificateNumber) != 0 ||
        validFromYmd != signedValues.externalCertificateValidFromYmd ||
        validUntilYmd != signedValues.externalCertificateValidUntilYmd ||
        approval.referenceStandards[0] != '\0' || approval.traceability[0] != '\0' ||
        approval.measurementUncertainty[0] != '\0' || approval.environmentalConditions[0] != '\0' ||
        approval.calibrationProcedure[0] != '\0' || approval.accreditationInformation[0] != '\0')
    {
      scSetSystemError(status, "Externer Kalibrierschein ist unvollständig oder die sechs Fachangaben sind nicht leer");
      return false;
    }
  }

  TpCanonicalWriter w(tpSignedCalCanonical, sizeof(tpSignedCalCanonical));
  const bool canonicalOk = w.writeUtf8(format) && w.writeUtf8(TP_CAL_PRODUCT) &&
      w.writeUtf8(deviceSerial) && w.writeUtf8(deviceKeyId) &&
      w.writeUtf8(deviceCertificateSerial) &&
      (v2 ? scWriteSystemValuesV2(w, signedValues) : scWriteSystemValuesV1(w, signedValues)) &&
      w.writeUtf8(firmwareVersion) && w.writeUtf8(firmwareBuildId) &&
      w.writeHash32(sourceRequestHash) && w.writeUtf8(sourceRequestId) &&
      w.writeI64(sourceRequestCreatedUtc) && w.writeI64(sourceRequestExpiresUtc) &&
      (v2 ? scWriteApprovalV4(w, approval, approvedUtc, signerType, signerKeyId, signerCertificateManifest)
          : scWriteApprovalV3(w, approval, approvedUtc, signerType, signerKeyId, signerCertificateManifest));
  if (!canonicalOk || !w.valid())
  {
    scSetSystemError(status, "Kanonische Systemkalibrierung ist zu groß");
    return false;
  }
  uint8_t calculatedManifest[32];
  TpSha256::hash(tpSignedCalCanonical, w.length(), calculatedManifest);
  bool signatureOk = scBytesEqual(calculatedManifest, storedManifest, 32U);
  if (signatureOk)
  {
    signatureOk = rootSigner
      ? deviceIdentityVerifyRootSignedHash(calculatedManifest, signature)
      : (uECC_verify(role.publicRaw, calculatedManifest, 32U, signature, uECC_secp256r1()) == 1);
  }
  if (!signatureOk)
  {
    scSetSystemError(status, "Signatur der Systemkalibrierung ist ungültig");
    return false;
  }

  status.signatureValid = true;
  status.bindingMatchesCurrent = !tpSystemVerificationSignatureOnly &&
      scSystemBindingMatchesCurrent(signedValues, externalSource);
  scSetText(status.calibrationId, sizeof(status.calibrationId), approval.calibrationId);
  status.calibrationDateYmd = approval.calibrationDateYmd;
  status.validFromUtc = approval.validFromUtc;
  status.validUntilUtc = approval.validUntilUtc;
  status.intervalMonths = approval.intervalMonths;
  scSetText(status.signerKeyId, sizeof(status.signerKeyId), signerKeyId);
  scBytesToHex(storedManifest, 32U, status.manifestSha256, sizeof(status.manifestSha256), false);
  scSetText(status.sourceRequestId, sizeof(status.sourceRequestId), sourceRequestId);
  scBytesToHex(sourceRequestHash, 32U,
               status.sourceRequestManifestSha256,
               sizeof(status.sourceRequestManifestSha256), false);
  scSetText(status.firmwareVersion, sizeof(status.firmwareVersion), firmwareVersion);
  scSetText(status.firmwareBuildId, sizeof(status.firmwareBuildId), firmwareBuildId);
  scSetText(status.headType, sizeof(status.headType), signedValues.headType);
  status.headSerial = signedValues.headSerial;
  scSetText(status.deviceAdjustmentCalibrationId, sizeof(status.deviceAdjustmentCalibrationId), signedValues.deviceAdjustmentCalibrationId);
  scSetText(status.headAdjustmentCalibrationId, sizeof(status.headAdjustmentCalibrationId), signedValues.headAdjustmentCalibrationId);
  scSetText(status.certificateIssuer, sizeof(status.certificateIssuer), approval.laboratory);
  scSetText(status.operatorName, sizeof(status.operatorName), approval.operatorName);
  scSetText(status.certificateReference, sizeof(status.certificateReference), approval.certificateReference);
  scSetText(status.calibrationScope, sizeof(status.calibrationScope), approval.calibrationScope);
  scSetText(status.certificateSource, sizeof(status.certificateSource), approval.certificateSource);
  status.externalCertificateBound = externalSource;
  scSetText(status.externalCertificateDocumentId, sizeof(status.externalCertificateDocumentId), signedValues.externalCertificateDocumentId);
  scSetText(status.externalCertificateNumber, sizeof(status.externalCertificateNumber), signedValues.externalCertificateNumber);
  status.externalCertificateValidFromYmd = signedValues.externalCertificateValidFromYmd;
  status.externalCertificateValidUntilYmd = signedValues.externalCertificateValidUntilYmd;
  scSetText(status.externalCertificateOriginalFileName, sizeof(status.externalCertificateOriginalFileName), signedValues.externalCertificateOriginalFileName);
  status.externalCertificateFileSize = signedValues.externalCertificateFileSize;
  scBytesToHex(signedValues.externalCertificateSha256, 32U, status.externalCertificateSha256, sizeof(status.externalCertificateSha256), false);
  if (tpSystemVerificationSignatureOnly)
    scSetSystemError(status, "");
  else
    scSetSystemError(status, status.bindingMatchesCurrent ? "" :
                     (externalSource
                       ? "Systemkalibrierung passt nicht zum archivierten PDF-Kalibrierschein oder zu den Justierungen"
                       : "Systemkalibrierung passt nicht zur aktuellen Geräte-/Kopfjustierung"));
  return tpSystemVerificationSignatureOnly ? status.signatureValid
                                           : status.bindingMatchesCurrent;
}

enum ScCalibrationArchiveKind : uint8_t
{
  SC_ARCHIVE_DEVICE = 0U,
  SC_ARCHIVE_HEAD = 1U,
  SC_ARCHIVE_SYSTEM = 2U
};

static bool FLASHMEM scManifestTextValid(const char* text)
{
  if (text == nullptr || strlen(text) != 64U) return false;
  for (size_t i = 0U; i < 64U; ++i)
  {
    if (scHexNibble(text[i]) < 0) return false;
  }
  return true;
}

struct ScApplicabilityState
{
  bool known;
  bool ended;
  int64_t applicableUntilUtc;
  char reasonCode[40];
};

static bool FLASHMEM scApplicabilityReasonValid(const char* reasonCode)
{
  if (reasonCode == nullptr) return false;
  const size_t length = strlen(reasonCode);
  if (length == 0U || length >= 40U)
    return false;
  for (size_t i = 0U; i < length; ++i)
  {
    const char c = reasonCode[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
      return false;
  }
  return true;
}

static bool FLASHMEM scApplicabilityReadLine(File& file,
                                              char* output,
                                              size_t outputSize,
                                              bool* complete)
{
  if (complete != nullptr) *complete = false;
  if (!file || output == nullptr || outputSize < 2U) return false;
  size_t used = 0U;
  bool haveData = false;
  bool overflow = false;
  bool newline = false;
  while (file.available())
  {
    const int value = file.read();
    if (value < 0) break;
    haveData = true;
    if (value == '\n')
    {
      newline = true;
      break;
    }
    if (used + 1U < outputSize)
      output[used++] = (char)value;
    else
      overflow = true;
  }
  if (!haveData) return false;
  if (used > 0U && output[used - 1U] == '\r') --used;
  output[used] = '\0';
  if (complete != nullptr) *complete = newline && !overflow;
  return true;
}

static bool FLASHMEM scApplicabilityRecordHash(const char* manifestSha256,
                                                int64_t applicableUntilUtc,
                                                const char* reasonCode,
                                                char* output,
                                                size_t outputSize)
{
  if (!scManifestTextValid(manifestSha256) || applicableUntilUtc <= 0 ||
      !scApplicabilityReasonValid(reasonCode) || output == nullptr ||
      outputSize < 65U)
    return false;
  char canonical[192];
  const int length = snprintf(canonical, sizeof(canonical),
                              "%s\n%s\n%lld\n%s\n",
                              TP_CAL_APPLICABILITY_FORMAT,
                              manifestSha256,
                              (long long)applicableUntilUtc,
                              reasonCode);
  if (length <= 0 || (size_t)length >= sizeof(canonical)) return false;
  uint8_t digest[32];
  TpSha256::hash(canonical, (size_t)length, digest);
  return scBytesToHex(digest, sizeof(digest), output, outputSize, false);
}

static bool FLASHMEM scCopyFileExact(const char* sourcePath,
                                     const char* destinationPath);

static bool FLASHMEM scApplicabilityAppendixRead(
    const char* path,
    const char* expectedManifestSha256,
    bool* present,
    ScApplicabilityState& state)
{
  if (present != nullptr) *present = false;
  memset(&state, 0, sizeof(state));
  if (path == nullptr || !scManifestTextValid(expectedManifestSha256))
    return false;
  if (!SD.exists(path)) return false;

  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory())
  {
    if (file) file.close();
    return false;
  }

  char line[256];
  bool complete = false;
  bool found = false;
  while (scApplicabilityReadLine(file, line, sizeof(line), &complete))
  {
    if (complete && strcmp(line, TP_CAL_APPLICABILITY_APPENDIX_BEGIN) == 0)
    {
      found = true;
      break;
    }
  }
  if (!found)
  {
    file.close();
    state.known = true;
    return true;
  }
  if (present != nullptr) *present = true;

  char manifestLine[96];
  char utcLine[64];
  char reasonLine[80];
  char hashLine[96];
  char endLine[64];
  bool c1 = false, c2 = false, c3 = false, c4 = false, c5 = false;
  const bool linesOk =
      scApplicabilityReadLine(file, manifestLine, sizeof(manifestLine), &c1) && c1 &&
      scApplicabilityReadLine(file, utcLine, sizeof(utcLine), &c2) && c2 &&
      scApplicabilityReadLine(file, reasonLine, sizeof(reasonLine), &c3) && c3 &&
      scApplicabilityReadLine(file, hashLine, sizeof(hashLine), &c4) && c4 &&
      scApplicabilityReadLine(file, endLine, sizeof(endLine), &c5) && c5;
  if (!linesOk || strcmp(endLine, TP_CAL_APPLICABILITY_APPENDIX_END) != 0)
  {
    file.close();
    return false;
  }

  static const char manifestPrefix[] = "ManifestSha256=";
  static const char utcPrefix[] = "ApplicableUntilUtc=";
  static const char reasonPrefix[] = "ReasonCode=";
  static const char hashPrefix[] = "RecordSha256=";
  if (strncmp(manifestLine, manifestPrefix, sizeof(manifestPrefix) - 1U) != 0 ||
      strncmp(utcLine, utcPrefix, sizeof(utcPrefix) - 1U) != 0 ||
      strncmp(reasonLine, reasonPrefix, sizeof(reasonPrefix) - 1U) != 0 ||
      strncmp(hashLine, hashPrefix, sizeof(hashPrefix) - 1U) != 0)
  {
    file.close();
    return false;
  }

  const char* manifest = manifestLine + sizeof(manifestPrefix) - 1U;
  const char* utcText = utcLine + sizeof(utcPrefix) - 1U;
  const char* reasonCode = reasonLine + sizeof(reasonPrefix) - 1U;
  const char* storedHash = hashLine + sizeof(hashPrefix) - 1U;
  char* parseEnd = nullptr;
  const long long parsedUtc = strtoll(utcText, &parseEnd, 10);
  char calculatedHash[65];
  const bool valid =
      scManifestTextValid(manifest) &&
      strcasecmp(manifest, expectedManifestSha256) == 0 &&
      parseEnd != utcText && *parseEnd == '\0' && parsedUtc > 0 &&
      scApplicabilityReasonValid(reasonCode) &&
      scManifestTextValid(storedHash) &&
      scApplicabilityRecordHash(manifest, (int64_t)parsedUtc, reasonCode,
                                calculatedHash, sizeof(calculatedHash)) &&
      strcasecmp(calculatedHash, storedHash) == 0;
  if (!valid)
  {
    file.close();
    return false;
  }

  while (scApplicabilityReadLine(file, line, sizeof(line), &complete))
  {
    if (!complete || line[0] != '\0')
    {
      file.close();
      return false;
    }
  }
  file.close();
  state.known = true;
  state.ended = true;
  state.applicableUntilUtc = (int64_t)parsedUtc;
  scSetText(state.reasonCode, sizeof(state.reasonCode), reasonCode);
  return true;
}

static bool FLASHMEM scApplicabilityAppendixWrite(
    const char* path,
    const char* manifestSha256,
    int64_t applicableUntilUtc,
    const char* reasonCode)
{
  if (path == nullptr || !scManifestTextValid(manifestSha256) ||
      applicableUntilUtc <= 0 || !scApplicabilityReasonValid(reasonCode))
    return false;

  char tempPath[176];
  char backupPath[176];
  const int tempLength = snprintf(tempPath, sizeof(tempPath), "%s.apptmp", path);
  const int backupLength = snprintf(backupPath, sizeof(backupPath), "%s.appbak", path);
  if (tempLength <= 0 || backupLength <= 0 ||
      (size_t)tempLength >= sizeof(tempPath) ||
      (size_t)backupLength >= sizeof(backupPath))
    return false;

  // Wiederaufnahme nach einem Stromausfall in der Rename-Phase. Eine bereits
  // vollständig verifizierte Temp-Datei gewinnt; andernfalls wird das
  // unveränderte Backup zurückgestellt und der Anhang neu aufgebaut.
  if (!SD.exists(path))
  {
    bool tempUsable = false;
    if (SD.exists(tempPath))
    {
      bool tempPresent = false;
      ScApplicabilityState tempState;
      tempUsable = scApplicabilityAppendixRead(
          tempPath, manifestSha256, &tempPresent, tempState) &&
          tempPresent && tempState.ended &&
          tempState.applicableUntilUtc == applicableUntilUtc &&
          strcmp(tempState.reasonCode, reasonCode) == 0;
      if (tempUsable)
      {
        if (!SD.rename(tempPath, path)) return false;
        if (SD.exists(backupPath)) SD.remove(backupPath);
      }
      else
      {
        SD.remove(tempPath);
      }
    }
    if (!tempUsable && SD.exists(backupPath))
    {
      if (!SD.rename(backupPath, path)) return false;
    }
  }
  if (!SD.exists(path)) return false;

  if (SD.exists(backupPath))
  {
    bool currentPresent = false;
    ScApplicabilityState currentState;
    const bool currentOk = scApplicabilityAppendixRead(
        path, manifestSha256, &currentPresent, currentState) &&
        currentPresent && currentState.ended &&
        currentState.applicableUntilUtc == applicableUntilUtc &&
        strcmp(currentState.reasonCode, reasonCode) == 0;
    if (currentOk)
    {
      SD.remove(backupPath);
    }
    else
    {
      SD.remove(path);
      if (!SD.rename(backupPath, path)) return false;
    }
  }
  if (SD.exists(tempPath)) SD.remove(tempPath);

  bool present = false;
  ScApplicabilityState existing;
  if (!scApplicabilityAppendixRead(path, manifestSha256, &present, existing))
    return false;
  if (present)
  {
    return existing.ended &&
           existing.applicableUntilUtc == applicableUntilUtc &&
           strcmp(existing.reasonCode, reasonCode) == 0;
  }

  char recordHash[65];
  if (!scApplicabilityRecordHash(manifestSha256, applicableUntilUtc,
                                  reasonCode, recordHash,
                                  sizeof(recordHash)))
    return false;

  char block[420];
  const int blockLength = snprintf(
      block, sizeof(block),
      "\n%s\nManifestSha256=%s\nApplicableUntilUtc=%lld\nReasonCode=%s\nRecordSha256=%s\n%s\n",
      TP_CAL_APPLICABILITY_APPENDIX_BEGIN,
      manifestSha256,
      (long long)applicableUntilUtc,
      reasonCode,
      recordHash,
      TP_CAL_APPLICABILITY_APPENDIX_END);
  if (blockLength <= 0 || (size_t)blockLength >= sizeof(block)) return false;

  if (!scCopyFileExact(path, tempPath)) return false;

  File temp = SD.open(tempPath, FILE_WRITE);
  if (!temp)
  {
    SD.remove(tempPath);
    return false;
  }
  const bool writeOk = temp.write((const uint8_t*)block,
                                  (size_t)blockLength) == (size_t)blockLength;
  temp.flush();
  temp.close();
  if (!writeOk)
  {
    SD.remove(tempPath);
    return false;
  }

  bool verifiedPresent = false;
  ScApplicabilityState verified;
  if (!scApplicabilityAppendixRead(tempPath, manifestSha256,
                                    &verifiedPresent, verified) ||
      !verifiedPresent || !verified.ended ||
      verified.applicableUntilUtc != applicableUntilUtc ||
      strcmp(verified.reasonCode, reasonCode) != 0)
  {
    SD.remove(tempPath);
    return false;
  }

  if (!SD.rename(path, backupPath))
  {
    SD.remove(tempPath);
    return false;
  }
  if (!SD.rename(tempPath, path))
  {
    SD.rename(backupPath, path);
    SD.remove(tempPath);
    return false;
  }

  verifiedPresent = false;
  memset(&verified, 0, sizeof(verified));
  const bool finalOk = scApplicabilityAppendixRead(
      path, manifestSha256, &verifiedPresent, verified) &&
      verifiedPresent && verified.ended &&
      verified.applicableUntilUtc == applicableUntilUtc &&
      strcmp(verified.reasonCode, reasonCode) == 0;
  if (!finalOk)
  {
    SD.remove(path);
    SD.rename(backupPath, path);
    return false;
  }
  SD.remove(backupPath);
  return true;
}

static bool FLASHMEM scApplicabilityEnsureAppendixCopies(
    const char* manifestSha256,
    const ScApplicabilityState& state)
{
  if (!state.ended || !scManifestTextValid(manifestSha256)) return true;
  bool ok = true;
  char archivePath[160];
  const int archiveLength = snprintf(archivePath, sizeof(archivePath),
                                     TP_CAL_ARCHIVE_SYSTEM_PATH,
                                     manifestSha256);
  if (archiveLength <= 0 || (size_t)archiveLength >= sizeof(archivePath) ||
      !SD.exists(archivePath) ||
      !scApplicabilityAppendixWrite(archivePath, manifestSha256,
                                    state.applicableUntilUtc,
                                    state.reasonCode))
    ok = false;

  if (SD.exists(TP_CAL_ACTIVE_SYSTEM) &&
      tpSystemCalStatus.signatureValid &&
      strcasecmp(tpSystemCalStatus.manifestSha256, manifestSha256) == 0)
  {
    if (!scApplicabilityAppendixWrite(TP_CAL_ACTIVE_SYSTEM,
                                      manifestSha256,
                                      state.applicableUntilUtc,
                                      state.reasonCode))
      ok = false;
  }
  return ok;
}

static bool FLASHMEM scApplicabilityRead(const char* manifestSha256,
                                          ScApplicabilityState& state)
{
  memset(&state, 0, sizeof(state));
  if (!scManifestTextValid(manifestSha256)) return false;
  if (!SD.exists(TP_CAL_APPLICABILITY_FILE))
  {
    state.known = true;
    return true;
  }

  File file = SD.open(TP_CAL_APPLICABILITY_FILE, FILE_READ);
  if (!file || file.isDirectory())
  {
    if (file) file.close();
    return false;
  }

  char line[256];
  bool complete = false;
  if (!scApplicabilityReadLine(file, line, sizeof(line), &complete) ||
      !complete || strcmp(line, TP_CAL_APPLICABILITY_FORMAT) != 0)
  {
    file.close();
    return false;
  }

  while (scApplicabilityReadLine(file, line, sizeof(line), &complete))
  {
    // Ein Stromausfall kann nur die letzte Zeile unvollständig hinterlassen.
    // Sie wird ignoriert; jede vollständig abgeschlossene Zeile muss dagegen
    // syntaktisch und per SHA-256 korrekt sein.
    if (!complete) break;
    if (line[0] == '\0') continue;

    char* p1 = strchr(line, '|');
    char* p2 = p1 != nullptr ? strchr(p1 + 1, '|') : nullptr;
    char* p3 = p2 != nullptr ? strchr(p2 + 1, '|') : nullptr;
    if (p1 == nullptr || p2 == nullptr || p3 == nullptr ||
        strchr(p3 + 1, '|') != nullptr)
    {
      file.close();
      return false;
    }
    *p1 = '\0';
    *p2 = '\0';
    *p3 = '\0';
    const char* recordManifest = line;
    const char* utcText = p1 + 1;
    const char* reasonCode = p2 + 1;
    const char* storedHash = p3 + 1;

    char* end = nullptr;
    const long long parsedUtc = strtoll(utcText, &end, 10);
    if (!scManifestTextValid(recordManifest) || end == utcText || *end != '\0' ||
        parsedUtc <= 0 || !scApplicabilityReasonValid(reasonCode) ||
        !scManifestTextValid(storedHash))
    {
      file.close();
      return false;
    }
    char calculatedHash[65];
    if (!scApplicabilityRecordHash(recordManifest, (int64_t)parsedUtc,
                                    reasonCode, calculatedHash,
                                    sizeof(calculatedHash)) ||
        strcasecmp(calculatedHash, storedHash) != 0)
    {
      file.close();
      return false;
    }

    if (strcasecmp(recordManifest, manifestSha256) == 0 &&
        (!state.ended || (int64_t)parsedUtc < state.applicableUntilUtc))
    {
      state.ended = true;
      state.applicableUntilUtc = (int64_t)parsedUtc;
      scSetText(state.reasonCode, sizeof(state.reasonCode), reasonCode);
    }
  }
  file.close();
  state.known = true;
  return true;
}

static bool FLASHMEM scApplicabilityEnsureFile(void)
{
  if (SD.exists(TP_CAL_APPLICABILITY_FILE))
  {
    // Der Header und die vorhandenen vollständigen Datensätze werden beim
    // ersten Lookup geprüft. Hier reicht die Existenzprüfung.
    return true;
  }
  File file = SD.open(TP_CAL_APPLICABILITY_FILE, FILE_WRITE);
  if (!file) return false;
  const bool ok = file.print(TP_CAL_APPLICABILITY_FORMAT) > 0 &&
                  file.print('\n') > 0;
  file.flush();
  file.close();
  return ok;
}

static bool FLASHMEM scApplicabilityAppend(const char* manifestSha256,
                                            const char* reasonCode,
                                            int64_t applicableUntilUtc,
                                            char* errorText,
                                            size_t errorTextSize)
{
  if (!scManifestTextValid(manifestSha256) ||
      !scApplicabilityReasonValid(reasonCode))
  {
    scSetText(errorText, errorTextSize, "Anwendbarkeitsdatensatz ist ungültig");
    return false;
  }
  if (applicableUntilUtc <= 0)
  {
    scSetText(errorText, errorTextSize, "UTC-Zeit für Anwendbarkeitsende ist ungültig");
    return false;
  }
  if (!scEnsureDirectories() || !scApplicabilityEnsureFile())
  {
    scSetText(errorText, errorTextSize, "Anwendbarkeitsdatei auf SD ist nicht verfügbar");
    return false;
  }

  ScApplicabilityState existing;
  if (!scApplicabilityRead(manifestSha256, existing))
  {
    scSetText(errorText, errorTextSize, "Anwendbarkeitsdatei ist beschädigt");
    return false;
  }
  if (existing.ended)
  {
    // Altstände aus V0.50.1_44 werden beim ersten Zugriff nachträglich
    // mit dem transportablen Anwendbarkeitsanhang ergänzt. Die zentrale
    // APPLICABILITY.TPS bleibt die maßgebliche Quelle.
    (void)scApplicabilityEnsureAppendixCopies(manifestSha256, existing);
    scSetText(errorText, errorTextSize, "OK");
    return true;
  }

  char recordHash[65];
  if (!scApplicabilityRecordHash(manifestSha256, applicableUntilUtc,
                                  reasonCode, recordHash,
                                  sizeof(recordHash)))
  {
    scSetText(errorText, errorTextSize, "Anwendbarkeits-Hash konnte nicht gebildet werden");
    return false;
  }
  char line[256];
  const int length = snprintf(line, sizeof(line), "%s|%lld|%s|%s\n",
                              manifestSha256,
                              (long long)applicableUntilUtc,
                              reasonCode,
                              recordHash);
  if (length <= 0 || (size_t)length >= sizeof(line))
  {
    scSetText(errorText, errorTextSize, "Anwendbarkeitsdatensatz ist zu lang");
    return false;
  }

  File file = SD.open(TP_CAL_APPLICABILITY_FILE, FILE_WRITE);
  if (!file)
  {
    scSetText(errorText, errorTextSize, "Anwendbarkeitsdatei ist nicht schreibbar");
    return false;
  }
  const bool writeOk = file.write((const uint8_t*)line, (size_t)length) ==
                       (size_t)length;
  file.flush();
  file.close();
  if (!writeOk)
  {
    scSetText(errorText, errorTextSize, "Anwendbarkeitsdatensatz konnte nicht gespeichert werden");
    return false;
  }

  ScApplicabilityState verified;
  if (!scApplicabilityRead(manifestSha256, verified) || !verified.ended ||
      verified.applicableUntilUtc != applicableUntilUtc ||
      strcmp(verified.reasonCode, reasonCode) != 0)
  {
    scSetText(errorText, errorTextSize, "Anwendbarkeitsdatensatz konnte nicht bestätigt werden");
    return false;
  }

  // Der Anhang ist eine transportable Kopie. Ein Fehler hier darf den bereits
  // sicher in APPLICABILITY.TPS gespeicherten Endzeitpunkt nicht zurücknehmen;
  // beim nächsten Anzeigen/Exportieren wird die Kopie erneut ergänzt.
  (void)scApplicabilityEnsureAppendixCopies(manifestSha256, verified);
  scSetText(errorText, errorTextSize, "OK");
  return true;
}

static bool FLASHMEM scLoadApplicabilityIntoStatus(
    TpSystemCalibrationStatus& status)
{
  status.applicabilityStateKnown = false;
  status.applicabilityEnded = false;
  status.applicableUntilUtc = 0;
  status.applicabilityReason[0] = '\0';
  if (!status.signatureValid || !scManifestTextValid(status.manifestSha256))
    return false;
  ScApplicabilityState state;
  if (!scApplicabilityRead(status.manifestSha256, state)) return false;
  status.applicabilityStateKnown = state.known;
  status.applicabilityEnded = state.ended;
  status.applicableUntilUtc = state.applicableUntilUtc;
  scSetText(status.applicabilityReason,
            sizeof(status.applicabilityReason), state.reasonCode);
  if (state.ended)
    (void)scApplicabilityEnsureAppendixCopies(status.manifestSha256, state);
  return true;
}

static bool FLASHMEM scBuildArchivePath(ScCalibrationArchiveKind kind,
                                        const char* manifestSha256,
                                        char* path,
                                        size_t pathSize)
{
  if (!scManifestTextValid(manifestSha256) || path == nullptr || pathSize == 0U)
    return false;
  const char* format = kind == SC_ARCHIVE_DEVICE ? TP_CAL_ARCHIVE_DEVICE_PATH
                      : (kind == SC_ARCHIVE_HEAD ? TP_CAL_ARCHIVE_HEAD_PATH
                                                 : TP_CAL_ARCHIVE_SYSTEM_PATH);
  const int written = snprintf(path, pathSize, format, manifestSha256);
  return written > 0 && (size_t)written < pathSize;
}

static const char* FLASHMEM scArchiveTempPath(ScCalibrationArchiveKind kind)
{
  return kind == SC_ARCHIVE_DEVICE ? TP_CAL_ARCHIVE_DEVICE_TEMP
       : (kind == SC_ARCHIVE_HEAD ? TP_CAL_ARCHIVE_HEAD_TEMP
                                  : TP_CAL_ARCHIVE_SYSTEM_TEMP);
}

static bool FLASHMEM scCopyFileExact(const char* sourcePath,
                                     const char* destinationPath)
{
  if (sourcePath == nullptr || destinationPath == nullptr) return false;
  File source = SD.open(sourcePath, FILE_READ);
  if (!source || source.isDirectory())
  {
    if (source) source.close();
    return false;
  }
  if (SD.exists(destinationPath)) SD.remove(destinationPath);
  File destination = SD.open(destinationPath, FILE_WRITE);
  if (!destination)
  {
    source.close();
    return false;
  }

  bool ok = true;
  uint32_t copied = 0U;
  const uint32_t expected = (uint32_t)source.size();
  while (source.available())
  {
    const int read = source.read(tpSignedCalFileBuffer, sizeof(tpSignedCalFileBuffer));
    if (read <= 0)
    {
      ok = false;
      break;
    }
    const size_t written = destination.write(tpSignedCalFileBuffer, (size_t)read);
    if (written != (size_t)read)
    {
      ok = false;
      break;
    }
    copied += (uint32_t)written;
  }
  destination.flush();
  destination.close();
  source.close();
  if (!ok || copied != expected)
  {
    SD.remove(destinationPath);
    return false;
  }
  return true;
}

static bool FLASHMEM scVerifyArchivedPackageFile(ScCalibrationArchiveKind kind,
                                                 const char* path,
                                                 const char* expectedManifest)
{
  size_t length = 0U;
  const bool readOk = kind == SC_ARCHIVE_SYSTEM
      ? scReadSystemFile(path, tpSignedCalJson, sizeof(tpSignedCalJson), &length)
      : scReadFile(path, tpSignedCalJson, sizeof(tpSignedCalJson), &length);
  if (!readOk) return false;

  if (kind == SC_ARCHIVE_SYSTEM)
  {
    const bool previousMode = tpSystemVerificationSignatureOnly;
    tpSystemVerificationSignatureOnly = true;
    (void)scVerifySystemPackage(tpSignedCalJson, tpSystemSelectionStatus,
                                tpSystemSelectionValues);
    tpSystemVerificationSignatureOnly = previousMode;
    return tpSystemSelectionStatus.signatureValid &&
           strcasecmp(tpSystemSelectionStatus.manifestSha256,
                      expectedManifest) == 0;
  }

  TpSignedCalibrationStatus status;
  TpDeviceCalValues deviceValues;
  TpHeadCalValues headValues;
  if (kind == SC_ARCHIVE_DEVICE)
    (void)scVerifyDevicePackage(tpSignedCalJson, status, deviceValues);
  else
    (void)scVerifyHeadPackage(tpSignedCalJson, status, headValues);
  return status.signatureValid &&
         strcasecmp(status.manifestSha256, expectedManifest) == 0;
}

// Übernimmt ein bereits aktives und geprüftes Paket append-only in das
// Archiv. Eine beschädigte Datei mit demselben Manifestnamen wird nicht
// akzeptiert, sondern nach erneuter Prüfung durch die aktive Originaldatei
// ersetzt. Erst ein vollständig zurückgelesenes Archivpaket gilt als fertig.
static bool FLASHMEM scArchiveActivePackage(ScCalibrationArchiveKind kind,
                                            const char* activePath,
                                            const char* manifestSha256)
{
  if (!scEnsureDirectories() || activePath == nullptr ||
      !SD.exists(activePath) || !scManifestTextValid(manifestSha256))
    return false;

  char archivePath[160];
  if (!scBuildArchivePath(kind, manifestSha256, archivePath, sizeof(archivePath)))
    return false;

  if (SD.exists(archivePath))
  {
    if (scVerifyArchivedPackageFile(kind, archivePath, manifestSha256)) return true;
    SD.remove(archivePath);
  }

  const char* tempPath = scArchiveTempPath(kind);
  if (SD.exists(tempPath)) SD.remove(tempPath);
  if (!scCopyFileExact(activePath, tempPath) ||
      !scVerifyArchivedPackageFile(kind, tempPath, manifestSha256))
  {
    SD.remove(tempPath);
    return false;
  }
  if (!SD.rename(tempPath, archivePath))
  {
    SD.remove(tempPath);
    return false;
  }
  if (!scVerifyArchivedPackageFile(kind, archivePath, manifestSha256))
  {
    SD.remove(archivePath);
    return false;
  }
  return true;
}

static bool FLASHMEM scLoadActiveSystemFile(void)
{
  memset(&tpSystemCalStatus, 0, sizeof(tpSystemCalStatus));
  if (!SD.exists(TP_CAL_ACTIVE_SYSTEM)) return false;
  size_t length = 0U;
  if (!scReadSystemFile(TP_CAL_ACTIVE_SYSTEM, tpSignedCalJson,
                        sizeof(tpSignedCalJson), &length))
  {
    tpSystemCalStatus.filePresent = true;
    scSetSystemError(tpSystemCalStatus, "Aktive Systemkalibrierung ist leer oder zu groß");
    return false;
  }
  const bool ok = scVerifySystemPackage(tpSignedCalJson, tpSystemCalStatus, tpSystemCalSignedValues);
  if (tpSystemCalStatus.signatureValid)
    (void)scLoadApplicabilityIntoStatus(tpSystemCalStatus);
  scSetText(tpSystemCalStatus.sourceFile, sizeof(tpSystemCalStatus.sourceFile), TP_CAL_ACTIVE_SYSTEM);
  return ok && tpSystemCalStatus.applicabilityStateKnown &&
         !tpSystemCalStatus.applicabilityEnded;
}

static bool FLASHMEM scLoadActiveFile(const char* path,
                                      bool device,
                                      TpSignedCalibrationStatus& status)
{
  memset(&status, 0, sizeof(status));
  if (!SD.exists(path)) return false;
  size_t length = 0U;
  if (!scReadFile(path, tpSignedCalJson, sizeof(tpSignedCalJson), &length))
  {
    status.filePresent = true;
    scSetError(status, tpSignedCalText.t091);
    return false;
  }
  const bool ok = device
    ? scVerifyDevicePackage(tpSignedCalJson, status, tpDeviceCalSignedValues)
    : scVerifyHeadPackage(tpSignedCalJson, status, tpHeadCalSignedValues);
  scSetText(status.sourceFile, sizeof(status.sourceFile), path);
  return ok;
}

static bool FLASHMEM scHasExtension(const char* name, const char* extension)
{
  if (name == nullptr || extension == nullptr) return false;
  const size_t n = strlen(name);
  const size_t e = strlen(extension);
  return n >= e && strcasecmp(name + n - e, extension) == 0;
}

static const char* FLASHMEM scBaseName(const char* path)
{
  if (path == nullptr) return tpSignedCalText.t000;
  const char* base = path;
  for (const char* p = path; *p != '\0'; ++p)
  {
    if (*p == '/' || *p == '\\') base = p + 1;
  }
  return base;
}

static bool FLASHMEM scUtcInside(int64_t nowUtc,
                                  int64_t validFromUtc,
                                  int64_t validUntilUtc)
{
  return nowUtc > 0 && validUntilUtc >= validFromUtc &&
         nowUtc >= validFromUtc && nowUtc <= validUntilUtc;
}

static bool FLASHMEM scCurrentDeviceSelectionReady(int64_t nowUtc,
                                                    uint8_t manifest[32])
{
  if (!tpDeviceCalStatus.signatureValid ||
      !scUtcInside(nowUtc, tpDeviceCalStatus.validFromUtc,
                  tpDeviceCalStatus.validUntilUtc))
    return false;
  TpDeviceCalValues current;
  scCurrentDeviceValues(current);
  tpDeviceCalStatus.valuesMatchCurrent =
      scDeviceValuesEqual(current, tpDeviceCalSignedValues);
  if (!tpDeviceCalStatus.valuesMatchCurrent ||
      !scHexToBytes(tpDeviceCalStatus.manifestSha256, manifest, 32U))
    return false;
  return true;
}

static bool FLASHMEM scExternalSelectionMetadataMatches(
    const TpSystemCalibrationStatus& status,
    const TpSystemCalValues& values)
{
  if (!status.externalCertificateBound) return true;
  memset(&tpExternalSelectionMetadata, 0, sizeof(tpExternalSelectionMetadata));
  char errorText[96];
  if (!tpExternalCalibrationLoadArchived(
          values.externalCertificateDocumentId,
          tpExternalSelectionMetadata, errorText, sizeof(errorText)))
    return false;
  char signedHash[65];
  scBytesToHex(values.externalCertificateSha256, 32U,
               signedHash, sizeof(signedHash), false);
  return tpExternalSelectionMetadata.present &&
      strcmp(tpExternalSelectionMetadata.documentId,
             values.externalCertificateDocumentId) == 0 &&
      strcmp(tpExternalSelectionMetadata.certificateNumber,
             values.externalCertificateNumber) == 0 &&
      tpExternalSelectionMetadata.validFromYmd ==
          values.externalCertificateValidFromYmd &&
      tpExternalSelectionMetadata.validUntilYmd ==
          values.externalCertificateValidUntilYmd &&
      strcmp(tpExternalSelectionMetadata.originalFileName,
             values.externalCertificateOriginalFileName) == 0 &&
      tpExternalSelectionMetadata.fileSize ==
          values.externalCertificateFileSize &&
      strcasecmp(tpExternalSelectionMetadata.sha256, signedHash) == 0 &&
      tpExternalCalibrationArchivedPdfPresent(
          values.externalCertificateDocumentId);
}

static bool FLASHMEM scHeadSelectionMatchesCurrent(
    const TpSystemCalValues& systemValues,
    int64_t nowUtc,
    char* headManifestHex,
    size_t headManifestHexSize)
{
  if (strcmp(systemValues.headType, headTypeTextGet()) != 0 ||
      systemValues.headSerial != R.head_serial)
    return false;

  scBytesToHex(systemValues.headAdjustmentManifest, 32U,
               headManifestHex, headManifestHexSize, false);
  char path[160];
  if (!scBuildArchivePath(SC_ARCHIVE_HEAD, headManifestHex,
                          path, sizeof(path)))
    return false;
  size_t jsonLength = 0U;
  if (!scReadFile(path, tpSignedCalJson, sizeof(tpSignedCalJson),
                  &jsonLength))
    return false;
  (void)scVerifyHeadPackage(tpSignedCalJson, tpHeadSelectionStatus,
                            tpHeadSelectionValues);
  return tpHeadSelectionStatus.signatureValid &&
      tpHeadSelectionStatus.valuesMatchCurrent &&
      strcasecmp(tpHeadSelectionStatus.manifestSha256,
                 headManifestHex) == 0 &&
      strcmp(tpHeadSelectionStatus.calibrationId,
             systemValues.headAdjustmentCalibrationId) == 0 &&
      scUtcInside(nowUtc, tpHeadSelectionStatus.validFromUtc,
                  tpHeadSelectionStatus.validUntilUtc);
}

static void FLASHMEM scRestoreSelectionBackups(bool hadHead,
                                                bool hadSystem)
{
  if (SD.exists(TP_CAL_ACTIVE_HEAD)) SD.remove(TP_CAL_ACTIVE_HEAD);
  if (SD.exists(TP_CAL_ACTIVE_SYSTEM)) SD.remove(TP_CAL_ACTIVE_SYSTEM);
  if (hadHead && SD.exists(TP_CAL_ACTIVE_HEAD_BACKUP))
    SD.rename(TP_CAL_ACTIVE_HEAD_BACKUP, TP_CAL_ACTIVE_HEAD);
  if (hadSystem && SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP))
    SD.rename(TP_CAL_ACTIVE_SYSTEM_BACKUP, TP_CAL_ACTIVE_SYSTEM);
  if (SD.exists(TP_CAL_ACTIVE_HEAD_TEMP)) SD.remove(TP_CAL_ACTIVE_HEAD_TEMP);
  if (SD.exists(TP_CAL_ACTIVE_SYSTEM_TEMP)) SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
}

static bool FLASHMEM scActivateCurrentHeadSelection(
    const char* systemManifest,
    const char* headManifest,
    int64_t nowUtc)
{
  if (!scManifestTextValid(systemManifest) ||
      !scManifestTextValid(headManifest))
    return false;

  // Bereits korrekt ausgewählt: keine unnötigen SD-Schreibzyklen.
  if (tpHeadCalStatus.signatureValid && tpSystemCalStatus.signatureValid &&
      strcasecmp(tpHeadCalStatus.manifestSha256, headManifest) == 0 &&
      strcasecmp(tpSystemCalStatus.manifestSha256, systemManifest) == 0 &&
      tpHeadCalStatus.valuesMatchCurrent &&
      tpSystemCalStatus.bindingMatchesCurrent &&
      tpSystemCalStatus.applicabilityStateKnown &&
      !tpSystemCalStatus.applicabilityEnded &&
      scUtcInside(nowUtc, tpHeadCalStatus.validFromUtc,
                  tpHeadCalStatus.validUntilUtc) &&
      scUtcInside(nowUtc, tpSystemCalStatus.validFromUtc,
                  tpSystemCalStatus.validUntilUtc))
    return true;

  char headSource[160];
  char systemSource[160];
  if (!scBuildArchivePath(SC_ARCHIVE_HEAD, headManifest,
                          headSource, sizeof(headSource)) ||
      !scBuildArchivePath(SC_ARCHIVE_SYSTEM, systemManifest,
                          systemSource, sizeof(systemSource)) ||
      !scVerifyArchivedPackageFile(SC_ARCHIVE_HEAD, headSource,
                                   headManifest) ||
      !scVerifyArchivedPackageFile(SC_ARCHIVE_SYSTEM, systemSource,
                                   systemManifest))
    return false;

  if (SD.exists(TP_CAL_ACTIVE_HEAD_TEMP)) SD.remove(TP_CAL_ACTIVE_HEAD_TEMP);
  if (SD.exists(TP_CAL_ACTIVE_SYSTEM_TEMP)) SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
  if (!scCopyFileExact(headSource, TP_CAL_ACTIVE_HEAD_TEMP) ||
      !scCopyFileExact(systemSource, TP_CAL_ACTIVE_SYSTEM_TEMP) ||
      !scVerifyArchivedPackageFile(SC_ARCHIVE_HEAD,
                                   TP_CAL_ACTIVE_HEAD_TEMP, headManifest) ||
      !scVerifyArchivedPackageFile(SC_ARCHIVE_SYSTEM,
                                   TP_CAL_ACTIVE_SYSTEM_TEMP,
                                   systemManifest))
  {
    SD.remove(TP_CAL_ACTIVE_HEAD_TEMP);
    SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
    return false;
  }

  // Der bisherige wirksame Zustand bleibt bis zur erneuten Prüfung als
  // Backup erhalten. Dadurch kann ein SD-Fehler niemals Kopf- und
  // Systemkalibrierung auseinanderreißen.
  if (tpHeadCalStatus.signatureValid && SD.exists(TP_CAL_ACTIVE_HEAD) &&
      !scArchiveActivePackage(SC_ARCHIVE_HEAD, TP_CAL_ACTIVE_HEAD,
                              tpHeadCalStatus.manifestSha256))
  {
    SD.remove(TP_CAL_ACTIVE_HEAD_TEMP);
    SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
    return false;
  }
  if (tpSystemCalStatus.signatureValid && SD.exists(TP_CAL_ACTIVE_SYSTEM) &&
      !scArchiveActivePackage(SC_ARCHIVE_SYSTEM, TP_CAL_ACTIVE_SYSTEM,
                              tpSystemCalStatus.manifestSha256))
  {
    SD.remove(TP_CAL_ACTIVE_HEAD_TEMP);
    SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
    return false;
  }

  if (SD.exists(TP_CAL_ACTIVE_HEAD_BACKUP))
    SD.remove(TP_CAL_ACTIVE_HEAD_BACKUP);
  if (SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP))
    SD.remove(TP_CAL_ACTIVE_SYSTEM_BACKUP);
  const bool hadHead = SD.exists(TP_CAL_ACTIVE_HEAD);
  const bool hadSystem = SD.exists(TP_CAL_ACTIVE_SYSTEM);
  bool headBackedUp = false;
  if (hadHead)
  {
    headBackedUp = SD.rename(TP_CAL_ACTIVE_HEAD,
                             TP_CAL_ACTIVE_HEAD_BACKUP);
    if (!headBackedUp)
    {
      SD.remove(TP_CAL_ACTIVE_HEAD_TEMP);
      SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
      return false;
    }
  }
  if (hadSystem && !SD.rename(TP_CAL_ACTIVE_SYSTEM,
                               TP_CAL_ACTIVE_SYSTEM_BACKUP))
  {
    if (headBackedUp)
      SD.rename(TP_CAL_ACTIVE_HEAD_BACKUP, TP_CAL_ACTIVE_HEAD);
    SD.remove(TP_CAL_ACTIVE_HEAD_TEMP);
    SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
    return false;
  }
  if (!SD.rename(TP_CAL_ACTIVE_HEAD_TEMP, TP_CAL_ACTIVE_HEAD) ||
      !SD.rename(TP_CAL_ACTIVE_SYSTEM_TEMP, TP_CAL_ACTIVE_SYSTEM))
  {
    scRestoreSelectionBackups(hadHead, hadSystem);
    return false;
  }

  const bool headLoaded = scLoadActiveFile(TP_CAL_ACTIVE_HEAD, false,
                                            tpHeadCalStatus);
  const bool systemLoaded = headLoaded && scLoadActiveSystemFile();
  const bool activated = headLoaded && systemLoaded &&
      strcasecmp(tpHeadCalStatus.manifestSha256, headManifest) == 0 &&
      strcasecmp(tpSystemCalStatus.manifestSha256, systemManifest) == 0 &&
      tpHeadCalStatus.valuesMatchCurrent &&
      tpSystemCalStatus.bindingMatchesCurrent &&
      tpSystemCalStatus.applicabilityStateKnown &&
      !tpSystemCalStatus.applicabilityEnded &&
      scUtcInside(nowUtc, tpHeadCalStatus.validFromUtc,
                  tpHeadCalStatus.validUntilUtc) &&
      scUtcInside(nowUtc, tpSystemCalStatus.validFromUtc,
                  tpSystemCalStatus.validUntilUtc);
  if (!activated)
  {
    scRestoreSelectionBackups(hadHead, hadSystem);
    (void)scLoadActiveFile(TP_CAL_ACTIVE_HEAD, false,
                           tpHeadCalStatus);
    (void)scLoadActiveSystemFile();
    return false;
  }

  if (SD.exists(TP_CAL_ACTIVE_HEAD_BACKUP))
    SD.remove(TP_CAL_ACTIVE_HEAD_BACKUP);
  if (SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP))
    SD.remove(TP_CAL_ACTIVE_SYSTEM_BACKUP);
  return true;
}

static bool FLASHMEM scSelectBestSystemForCurrentHead(void)
{
  if (!scEnsureDirectories() || R.head_serial == 0U) return false;
  const int64_t nowUtc = scCurrentUnixTime();
  tpSystemSelectionLastAttemptUtc = nowUtc;
  uint8_t currentDeviceManifest[32];
  if (!scCurrentDeviceSelectionReady(nowUtc, currentDeviceManifest))
    return false;

  File dir = SD.open(TP_CAL_ARCHIVE_SYSTEM_DIR);
  if (!dir || !dir.isDirectory())
  {
    if (dir) dir.close();
    return false;
  }

  bool found = false;
  char bestSystemManifest[65] = {0};
  char bestHeadManifest[65] = {0};
  int64_t bestValidFromUtc = INT64_MIN;
  uint32_t bestCalibrationDateYmd = 0U;
  File entry;
  while ((entry = dir.openNextFile()))
  {
    if (entry.isDirectory())
    {
      entry.close();
      continue;
    }
    const char* base = scBaseName(entry.name());
    if (!scHasExtension(base, TP_CAL_ARCHIVE_SYSTEM_EXTENSION))
    {
      entry.close();
      continue;
    }
    char path[160];
    const int pathLength = snprintf(path, sizeof(path), tpSignedCalText.t099,
                                    TP_CAL_ARCHIVE_SYSTEM_DIR, base);
    entry.close();
    if (pathLength <= 0 || (size_t)pathLength >= sizeof(path)) continue;

    size_t jsonLength = 0U;
    if (!scReadSystemFile(path, tpSignedCalJson, sizeof(tpSignedCalJson),
                          &jsonLength))
      continue;
    const bool previousMode = tpSystemVerificationSignatureOnly;
    tpSystemVerificationSignatureOnly = true;
    (void)scVerifySystemPackage(tpSignedCalJson, tpSystemSelectionStatus,
                                tpSystemSelectionValues);
    tpSystemVerificationSignatureOnly = previousMode;
    if (tpSystemSelectionStatus.signatureValid)
      (void)scLoadApplicabilityIntoStatus(tpSystemSelectionStatus);
    if (!tpSystemSelectionStatus.signatureValid ||
        !tpSystemSelectionStatus.applicabilityStateKnown ||
        tpSystemSelectionStatus.applicabilityEnded ||
        !scUtcInside(nowUtc, tpSystemSelectionStatus.validFromUtc,
                     tpSystemSelectionStatus.validUntilUtc) ||
        strcmp(tpSystemSelectionValues.headType, headTypeTextGet()) != 0 ||
        tpSystemSelectionValues.headSerial != R.head_serial ||
        strcmp(tpSystemSelectionValues.deviceAdjustmentCalibrationId,
               tpDeviceCalStatus.calibrationId) != 0 ||
        !scBytesEqual(tpSystemSelectionValues.deviceAdjustmentManifest,
                      currentDeviceManifest, 32U))
      continue;

    char headManifest[65];
    if (!scHeadSelectionMatchesCurrent(tpSystemSelectionValues, nowUtc,
                                       headManifest,
                                       sizeof(headManifest)) ||
        !scExternalSelectionMetadataMatches(tpSystemSelectionStatus,
                                            tpSystemSelectionValues))
      continue;

    const bool newer = !found ||
        tpSystemSelectionStatus.validFromUtc > bestValidFromUtc ||
        (tpSystemSelectionStatus.validFromUtc == bestValidFromUtc &&
         tpSystemSelectionStatus.calibrationDateYmd >
             bestCalibrationDateYmd) ||
        (tpSystemSelectionStatus.validFromUtc == bestValidFromUtc &&
         tpSystemSelectionStatus.calibrationDateYmd ==
             bestCalibrationDateYmd &&
         strcasecmp(tpSystemSelectionStatus.manifestSha256,
                    bestSystemManifest) > 0);
    if (newer)
    {
      // Nur ein Kandidat, der die bisherige Auswahl verdrängen würde, muss
      // sein eventuell gebundenes Original-PDF vollständig nachweisen. So
      // bleibt die Auswahl korrekt, ohne jedes ältere Archiv-PDF zu hashen.
      if (tpSystemSelectionStatus.externalCertificateBound)
      {
        char externalHash[65];
        scBytesToHex(tpSystemSelectionValues.externalCertificateSha256, 32U,
                     externalHash, sizeof(externalHash), false);
        if (!tpExternalCalibrationArchivedMatches(
                tpSystemSelectionValues.externalCertificateDocumentId,
                tpSystemSelectionValues.externalCertificateNumber,
                tpSystemSelectionValues.externalCertificateValidFromYmd,
                tpSystemSelectionValues.externalCertificateValidUntilYmd,
                tpSystemSelectionValues.externalCertificateOriginalFileName,
                tpSystemSelectionValues.externalCertificateFileSize,
                externalHash))
          continue;
      }
      found = true;
      bestValidFromUtc = tpSystemSelectionStatus.validFromUtc;
      bestCalibrationDateYmd =
          tpSystemSelectionStatus.calibrationDateYmd;
      scSetText(bestSystemManifest, sizeof(bestSystemManifest),
                tpSystemSelectionStatus.manifestSha256);
      scSetText(bestHeadManifest, sizeof(bestHeadManifest), headManifest);
    }
  }
  dir.close();
  if (!found) return false;

  return scActivateCurrentHeadSelection(bestSystemManifest,
                                        bestHeadManifest, nowUtc);
}

static bool FLASHMEM scRecordCurrentSystemMismatchIfNeeded(void);

static bool FLASHMEM scImportFromSd(bool device,
                                    char* importedPath,
                                    size_t importedPathSize,
                                    char* errorText,
                                    size_t errorTextSize)
{
  if (importedPath != nullptr && importedPathSize > 0U) importedPath[0] = '\0';
  if (!deviceIdentityCertificateValid())
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t092);
    return false;
  }
  if (!scEnsureDirectories())
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t093);
    return false;
  }
  File dir = SD.open(TP_CAL_DIR);
  if (!dir)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t094);
    return false;
  }
  bool found = false;
  char bestPath[96] = {0};
  int64_t bestApproved = INT64_MIN;
  File entry;
  while ((entry = dir.openNextFile()))
  {
    if (!entry.isDirectory())
    {
      const char* name = scBaseName(entry.name());
      const bool activeFile = strcasecmp(name, tpSignedCalText.t095) == 0 ||
                              strcasecmp(name, tpSignedCalText.t096) == 0;
      const bool extensionMatch = !activeFile &&
                                  scHasExtension(name, device ? tpSignedCalText.t097 : tpSignedCalText.t098);
      if (extensionMatch)
      {
        char path[96];
        snprintf(path, sizeof(path), tpSignedCalText.t099, TP_CAL_DIR, name);
        entry.close();
        size_t length = 0U;
        if (scReadFile(path, tpSignedCalJson, sizeof(tpSignedCalJson), &length))
        {
          TpSignedCalibrationStatus candidate;
          TpDeviceCalValues candidateDeviceValues;
          TpHeadCalValues candidateHeadValues;
          bool valid = device
            ? scVerifyDevicePackage(tpSignedCalJson, candidate, candidateDeviceValues)
            : scVerifyHeadPackage(tpSignedCalJson, candidate, candidateHeadValues);
          char approvedText[48];
          int64_t approvedUtc = 0;
          if (valid && scJsonGetString(tpSignedCalJson, tpSignedCalText.t074, approvedText, sizeof(approvedText)) &&
              scParseIsoUtc(approvedText, &approvedUtc) && approvedUtc > bestApproved)
          {
            bestApproved = approvedUtc;
            scSetText(bestPath, sizeof(bestPath), path);
            found = true;
          }
        }
        continue;
      }
    }
    entry.close();
  }
  dir.close();
  if (!found)
  {
    scSetText(errorText, errorTextSize,
              device ? tpSignedCalText.t100 : tpSignedCalText.t101);
    return false;
  }
  const char* activePath = device ? TP_CAL_ACTIVE_DEVICE : TP_CAL_ACTIVE_HEAD;
  const char* tempPath = device ? TP_CAL_ACTIVE_DEVICE_TEMP : TP_CAL_ACTIVE_HEAD_TEMP;
  const char* backupPath = device ? TP_CAL_ACTIVE_DEVICE_BACKUP : TP_CAL_ACTIVE_HEAD_BACKUP;

  // Auch der TFT-/SD-Import muss die Historie vollständig erhalten. Der
  // bisherige aktive Datensatz wird daher vor dem Lesen des neuen Pakets
  // append-only archiviert.
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  const TpSignedCalibrationStatus& previousStatus = device ? tpDeviceCalStatus : tpHeadCalStatus;
  if (SD.exists(activePath) && previousStatus.signatureValid &&
      !scArchiveActivePackage(device ? SC_ARCHIVE_DEVICE : SC_ARCHIVE_HEAD,
                              activePath, previousStatus.manifestSha256))
  {
    scSetText(errorText, errorTextSize,
              device ? SC_ERR_PREVIOUS_DEVICE_ARCHIVE
                     : SC_ERR_PREVIOUS_HEAD_ARCHIVE);
    return false;
  }

  size_t length = 0U;
  if (!scReadFile(bestPath, tpSignedCalJson, sizeof(tpSignedCalJson), &length))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t102);
    return false;
  }
  if (SD.exists(tempPath)) SD.remove(tempPath);
  if (!scWriteFile(tempPath, tpSignedCalJson, length))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t103);
    return false;
  }

  if (SD.exists(backupPath)) SD.remove(backupPath);
  const bool hadActive = SD.exists(activePath);
  if (hadActive && !SD.rename(activePath, backupPath))
  {
    SD.remove(tempPath);
    scSetText(errorText, errorTextSize, SC_ERR_PREVIOUS_ADJUSTMENT_SAVE);
    return false;
  }
  if (!SD.rename(tempPath, activePath))
  {
    SD.remove(tempPath);
    if (hadActive && SD.exists(backupPath)) SD.rename(backupPath, activePath);
    scSetText(errorText, errorTextSize, tpSignedCalText.t103);
    return false;
  }

  tpSignedCalibrationLoaded = false;
  tpSignedCalibrationTransactionReload = true;
  tpSignedCalibrationBegin();
  tpSignedCalibrationTransactionReload = false;
  const bool activeOk = device ? tpSignedCalibrationDeviceReady()
                               : tpSignedCalibrationHeadReady();
  if (!activeOk)
  {
    const TpSignedCalibrationStatus& failedStatus = device ? tpDeviceCalStatus : tpHeadCalStatus;
    char failedReason[160];
    scSetText(failedReason, sizeof(failedReason), failedStatus.lastError);
    SD.remove(activePath);
    if (hadActive && SD.exists(backupPath)) SD.rename(backupPath, activePath);
    tpSignedCalibrationLoaded = false;
    tpSignedCalibrationBegin();
    scSetText(errorText, errorTextSize,
              failedReason[0] != '\0' ? failedReason : tpSignedCalText.t103);
    return false;
  }

  const TpSignedCalibrationStatus& activatedStatus = device ? tpDeviceCalStatus : tpHeadCalStatus;
  if (!scArchiveActivePackage(device ? SC_ARCHIVE_DEVICE : SC_ARCHIVE_HEAD,
                              activePath, activatedStatus.manifestSha256))
  {
    SD.remove(activePath);
    if (hadActive && SD.exists(backupPath)) SD.rename(backupPath, activePath);
    tpSignedCalibrationLoaded = false;
    tpSignedCalibrationBegin();
    scSetText(errorText, errorTextSize,
              device ? SC_ERR_NEW_DEVICE_ARCHIVE
                     : SC_ERR_NEW_HEAD_ARCHIVE);
    return false;
  }

  // Erst jetzt ist die neue Justierung vollständig geprüft und archiviert.
  // Eine dadurch gelöste Bindung beendet den bisherigen Systemschein genau
  // an diesem Zeitpunkt; bei einem vorherigen Fehler bleibt er unangetastet.
  if (!scRecordCurrentSystemMismatchIfNeeded())
  {
    char applicabilityError[160];
    scSetText(applicabilityError, sizeof(applicabilityError),
              tpSystemCalStatus.lastError);
    SD.remove(activePath);
    if (hadActive && SD.exists(backupPath)) SD.rename(backupPath, activePath);
    tpSignedCalibrationLoaded = false;
    tpSignedCalibrationBegin();
    scSetText(errorText, errorTextSize,
              applicabilityError[0] != '\0'
                  ? applicabilityError
                  : "Anwendbarkeitsende konnte nicht gespeichert werden");
    return false;
  }

  if (SD.exists(backupPath)) SD.remove(backupPath);
  if (importedPath != nullptr && importedPathSize > 0U)
    scSetText(importedPath, importedPathSize, bestPath);
  scSetText(errorText, errorTextSize, tpSignedCalText.t104);
  return true;
}

static const char* FLASHMEM scCurrentSystemMismatchReason(void)
{
  if (!tpSignedCalibrationDeviceReady()) return SC_REASON_DEVICE_VALUES;
  if (!tpSignedCalibrationHeadReady()) return SC_REASON_HEAD_VALUES;

  if (strcmp(tpSystemCalSignedValues.deviceAdjustmentCalibrationId,
             tpDeviceCalStatus.calibrationId) != 0)
    return SC_REASON_DEVICE_ADJUSTMENT;
  if (strcmp(tpSystemCalSignedValues.headAdjustmentCalibrationId,
             tpHeadCalStatus.calibrationId) != 0)
    return SC_REASON_HEAD_ADJUSTMENT;

  uint8_t deviceManifest[32];
  uint8_t headManifest[32];
  if (!scHexToBytes(tpDeviceCalStatus.manifestSha256,
                    deviceManifest, sizeof(deviceManifest)) ||
      !scBytesEqual(tpSystemCalSignedValues.deviceAdjustmentManifest,
                    deviceManifest, sizeof(deviceManifest)))
    return SC_REASON_DEVICE_ADJUSTMENT;
  if (!scHexToBytes(tpHeadCalStatus.manifestSha256,
                    headManifest, sizeof(headManifest)) ||
      !scBytesEqual(tpSystemCalSignedValues.headAdjustmentManifest,
                    headManifest, sizeof(headManifest)))
    return SC_REASON_HEAD_ADJUSTMENT;
  // Ein fehlendes oder vorübergehend nicht lesbares externes PDF beendet
  // die metrologische Anwendbarkeit nicht dauerhaft. Es sperrt lediglich
  // die aktuelle Verifikation, bis das gebundene Original wieder verfügbar
  // ist. Für alle übrigen nicht klassifizierbaren Abweichungen wird daher
  // ebenfalls kein irreversibler SD-Eintrag erzeugt.
  return nullptr;
}

static bool FLASHMEM scRecordCurrentSystemMismatchIfNeeded(void)
{
  if (!tpSystemCalStatus.signatureValid) return true;
  if (!tpSystemCalStatus.applicabilityStateKnown &&
      !scLoadApplicabilityIntoStatus(tpSystemCalStatus))
  {
    scSetSystemError(tpSystemCalStatus,
                     "Anwendbarkeitsdatei ist nicht lesbar");
    return false;
  }
  if (tpSystemCalStatus.applicabilityEnded) return true;

  // Ein reiner Kopfwechsel beendet die Kalibrierung des vorherigen Kopfes
  // nicht. Sie darf bei erneutem Anschluss desselben unveränderten Kopfes
  // wieder ausgewählt werden. Nur Abweichungen am aktuell zugeordneten Kopf
  // beziehungsweise an der Gerätejustierung werden dauerhaft protokolliert.
  if (strcmp(tpSystemCalStatus.headType, headTypeTextGet()) != 0 ||
      tpSystemCalStatus.headSerial != R.head_serial)
    return true;

  if (scSystemBindingMatchesCurrent(tpSystemCalSignedValues,
                                    tpSystemCalStatus.externalCertificateBound))
    return true;

  const char* reasonCode = scCurrentSystemMismatchReason();
  if (reasonCode == nullptr) return true;

  char errorText[128];
  const bool stored = scApplicabilityAppend(
      tpSystemCalStatus.manifestSha256,
      reasonCode,
      scCurrentUnixTime(),
      errorText, sizeof(errorText));
  if (!stored)
  {
    scSetSystemError(tpSystemCalStatus,
                     errorText[0] != '\0' ? errorText
                                           : "Anwendbarkeitsende konnte nicht gespeichert werden");
    return false;
  }
  return scLoadApplicabilityIntoStatus(tpSystemCalStatus);
}

bool FLASHMEM tpSignedCalibrationEndCurrentSystemApplicability(
    const char* reasonCode,
    char* errorText,
    size_t errorTextSize)
{
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  if (!tpSystemCalStatus.signatureValid)
  {
    scSetText(errorText, errorTextSize, "OK");
    return true;
  }
  if (!tpSystemCalStatus.applicabilityStateKnown &&
      !scLoadApplicabilityIntoStatus(tpSystemCalStatus))
  {
    scSetText(errorText, errorTextSize,
              "Anwendbarkeitsdatei ist nicht lesbar");
    return false;
  }
  if (tpSystemCalStatus.applicabilityEnded)
  {
    scSetText(errorText, errorTextSize, "OK");
    return true;
  }
  const bool ok = scApplicabilityAppend(tpSystemCalStatus.manifestSha256,
                                         reasonCode,
                                         scCurrentUnixTime(),
                                         errorText,
                                         errorTextSize);
  if (ok) (void)scLoadApplicabilityIntoStatus(tpSystemCalStatus);
  return ok;
}

bool FLASHMEM tpSignedCalibrationGetApplicability(
    const char* manifestSha256,
    bool* stateKnown,
    bool* ended,
    int64_t* applicableUntilUtc,
    char* reasonCode,
    size_t reasonCodeSize)
{
  if (stateKnown != nullptr) *stateKnown = false;
  if (ended != nullptr) *ended = false;
  if (applicableUntilUtc != nullptr) *applicableUntilUtc = 0;
  if (reasonCode != nullptr && reasonCodeSize > 0U) reasonCode[0] = '\0';
  if (!scManifestTextValid(manifestSha256) || !scEnsureDirectories())
    return false;

  ScApplicabilityState state;
  if (!scApplicabilityRead(manifestSha256, state)) return false;
  if (state.ended)
    (void)scApplicabilityEnsureAppendixCopies(manifestSha256, state);
  if (stateKnown != nullptr) *stateKnown = state.known;
  if (ended != nullptr) *ended = state.ended;
  if (applicableUntilUtc != nullptr)
    *applicableUntilUtc = state.applicableUntilUtc;
  scSetText(reasonCode, reasonCodeSize, state.reasonCode);
  return true;
}

void FLASHMEM tpSignedCalibrationBegin(void)
{
  tpSignedCalibrationLoaded = true;
  memset(&tpDeviceCalStatus, 0, sizeof(tpDeviceCalStatus));
  memset(&tpHeadCalStatus, 0, sizeof(tpHeadCalStatus));
  memset(&tpSystemCalStatus, 0, sizeof(tpSystemCalStatus));
  memset(&tpDeviceCalSignedValues, 0, sizeof(tpDeviceCalSignedValues));
  memset(&tpHeadCalSignedValues, 0, sizeof(tpHeadCalSignedValues));
  memset(&tpSystemCalSignedValues, 0, sizeof(tpSystemCalSignedValues));
  if (!deviceIdentityCertificateValid() || !scEnsureDirectories()) return;
  scLoadActiveFile(TP_CAL_ACTIVE_DEVICE, true, tpDeviceCalStatus);
  scLoadActiveFile(TP_CAL_ACTIVE_HEAD, false, tpHeadCalStatus);
  scLoadActiveSystemFile();
  if (!tpSignedCalibrationTransactionReload)
    (void)scRecordCurrentSystemMismatchIfNeeded();

  // Einmalige Migration vorhandener aktiver Pakete in das neue append-only
  // Archiv. Fehler hier verändern den aktiven Zustand nicht; beim nächsten
  // Start beziehungsweise Import wird der Versuch wiederholt.
  if (tpDeviceCalStatus.signatureValid)
    (void)scArchiveActivePackage(SC_ARCHIVE_DEVICE, TP_CAL_ACTIVE_DEVICE,
                                 tpDeviceCalStatus.manifestSha256);
  if (tpHeadCalStatus.signatureValid)
    (void)scArchiveActivePackage(SC_ARCHIVE_HEAD, TP_CAL_ACTIVE_HEAD,
                                 tpHeadCalStatus.manifestSha256);
  if (tpSystemCalStatus.signatureValid)
    (void)scArchiveActivePackage(SC_ARCHIVE_SYSTEM, TP_CAL_ACTIVE_SYSTEM,
                                 tpSystemCalStatus.manifestSha256);

  // Auswahl ist immer kopfbezogen. Nach einem Kopfwechsel wird aus dem
  // append-only Archiv deterministisch das neueste aktuell gültige Paket
  // gewählt, das exakt zu Kopftyp, Kopf-SN sowie den wirksamen Geräte- und
  // Kopfjustierungsmanifesten passt.
  if (!tpSignedCalibrationTransactionReload)
    (void)scSelectBestSystemForCurrentHead();
}

void FLASHMEM tpSignedCalibrationInvalidateCache(void)
{
  // Die meisten Konfigurationspfade rufen diese Funktion unmittelbar nach
  // dem Speichern auf. Vor dem Verwerfen des Caches wird deshalb geprüft, ob
  // die bisher aktive Systemkalibrierung durch die Änderung ihre Bindung
  // verloren hat. Ein reiner Kopfwechsel wird dabei ausdrücklich nicht als
  // dauerhaftes Ende gewertet.
  if (tpSignedCalibrationLoaded)
    (void)scRecordCurrentSystemMismatchIfNeeded();
  tpSignedCalibrationLoaded = false;
}

bool FLASHMEM tpSignedCalibrationDeviceReady(void)
{
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  if (!tpDeviceCalStatus.signatureValid) return false;
  TpDeviceCalValues current;
  scCurrentDeviceValues(current);
  tpDeviceCalStatus.valuesMatchCurrent = scDeviceValuesEqual(current, tpDeviceCalSignedValues);
  if (!tpDeviceCalStatus.valuesMatchCurrent)
    scSetError(tpDeviceCalStatus, tpSignedCalText.t085);
  else
    scSetError(tpDeviceCalStatus, tpSignedCalText.t000);
  return tpDeviceCalStatus.valuesMatchCurrent;
}

bool FLASHMEM tpSignedCalibrationHeadReady(void)
{
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  if (!tpHeadCalStatus.signatureValid) return false;
  TpHeadCalValues current;
  scCurrentHeadValues(current);
  current.calibrationDateYmd = tpHeadCalSignedValues.calibrationDateYmd;
  current.calibrationTimeHms = tpHeadCalSignedValues.calibrationTimeHms;
  tpHeadCalStatus.valuesMatchCurrent = scHeadValuesEqual(current, tpHeadCalSignedValues);
  if (!tpHeadCalStatus.valuesMatchCurrent)
    scSetError(tpHeadCalStatus, tpSignedCalText.t090);
  else
    scSetError(tpHeadCalStatus, tpSignedCalText.t000);
  return tpHeadCalStatus.valuesMatchCurrent;
}

const TpSignedCalibrationStatus& FLASHMEM tpSignedCalibrationDeviceStatus(void)
{
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  return tpDeviceCalStatus;
}

const TpSignedCalibrationStatus& FLASHMEM tpSignedCalibrationHeadStatus(void)
{
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  return tpHeadCalStatus;
}

bool FLASHMEM tpSignedCalibrationSystemReady(void)
{
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  const int64_t nowUtc = scCurrentUnixTime();

  // Erkennt Änderungen, die nicht über einen speziellen Importpfad laufen,
  // spätestens bei der nächsten Statusprüfung und hält den ersten Zeitpunkt
  // append-only auf SD fest.
  (void)scRecordCurrentSystemMismatchIfNeeded();

  bool ready = tpSystemCalStatus.signatureValid &&
      tpSystemCalStatus.applicabilityStateKnown &&
      !tpSystemCalStatus.applicabilityEnded &&
      scSystemBindingMatchesCurrent(tpSystemCalSignedValues,
                                    tpSystemCalStatus.externalCertificateBound) &&
      scUtcInside(nowUtc, tpSystemCalStatus.validFromUtc,
                  tpSystemCalStatus.validUntilUtc);

  // Nach Kopfwechsel sowie beim Beginn/Ablauf eines Gültigkeitsintervalls
  // höchstens einmal pro Minute nach einem besseren passenden Paket suchen.
  // Bereits beendete Systemkalibrierungen werden von der Auswahl dauerhaft
  // ausgeschlossen.
  if (!ready && nowUtc > 0 &&
      (tpSystemSelectionLastAttemptUtc <= 0 ||
       nowUtc - tpSystemSelectionLastAttemptUtc >= 60))
  {
    (void)scSelectBestSystemForCurrentHead();
    ready = tpSystemCalStatus.signatureValid &&
        tpSystemCalStatus.applicabilityStateKnown &&
        !tpSystemCalStatus.applicabilityEnded &&
        scSystemBindingMatchesCurrent(tpSystemCalSignedValues,
                                      tpSystemCalStatus.externalCertificateBound) &&
        scUtcInside(nowUtc, tpSystemCalStatus.validFromUtc,
                    tpSystemCalStatus.validUntilUtc);
  }

  tpSystemCalStatus.bindingMatchesCurrent = ready;
  if (ready)
    scSetSystemError(tpSystemCalStatus, "");
  else if (!tpSystemCalStatus.signatureValid)
    scSetSystemError(tpSystemCalStatus, SC_ERR_NO_SELECTED_SYSTEM);
  else if (!tpSystemCalStatus.applicabilityStateKnown)
    scSetSystemError(tpSystemCalStatus, "Anwendbarkeitsdatei ist nicht lesbar");
  else if (tpSystemCalStatus.applicabilityEnded)
  {
    char endedText[40];
    scFormatIsoUtc(tpSystemCalStatus.applicableUntilUtc,
                   endedText, sizeof(endedText));
    char message[160];
    snprintf(message, sizeof(message),
             "Systemkalibrierung seit %s nicht mehr anwendbar", endedText);
    scSetSystemError(tpSystemCalStatus, message);
  }
  else if (!scUtcInside(nowUtc, tpSystemCalStatus.validFromUtc,
                        tpSystemCalStatus.validUntilUtc))
    scSetSystemError(tpSystemCalStatus, SC_ERR_NO_CURRENT_SYSTEM);
  else
    scSetSystemError(tpSystemCalStatus, SC_ERR_SELECTED_SYSTEM_MISMATCH);
  return ready;
}

const TpSystemCalibrationStatus& FLASHMEM tpSignedCalibrationSystemStatus(void)
{
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  return tpSystemCalStatus;
}

bool FLASHMEM tpSignedCalibrationReadActiveJson(bool device,
                                                 char* output,
                                                 size_t outputSize,
                                                 size_t* outputLength,
                                                 char* errorText,
                                                 size_t errorTextSize)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (output == nullptr || outputSize < 2U)
  {
    scSetText(errorText, errorTextSize, "Ausgabepuffer zu klein");
    return false;
  }
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  const TpSignedCalibrationStatus& status = device ? tpDeviceCalStatus : tpHeadCalStatus;
  if (!status.filePresent || !status.signatureValid)
  {
    scSetText(errorText, errorTextSize,
              device ? "Keine gültige aktive Gerätejustierung"
                     : "Keine gültige aktive Kopfjustierung");
    output[0] = '\0';
    return false;
  }
  const char* path = device ? TP_CAL_ACTIVE_DEVICE : TP_CAL_ACTIVE_HEAD;
  size_t length = 0U;
  if (!scReadFile(path, output, outputSize, &length))
  {
    scSetText(errorText, errorTextSize, "Aktive Kalibrierdatei konnte nicht gelesen werden");
    output[0] = '\0';
    return false;
  }
  if (outputLength != nullptr) *outputLength = length;
  scSetText(errorText, errorTextSize, tpSignedCalText.t000);
  return true;
}

bool FLASHMEM tpSignedCalibrationReadActiveSystemJson(char* output,
                                                       size_t outputSize,
                                                       size_t* outputLength,
                                                       char* errorText,
                                                       size_t errorTextSize)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (output == nullptr || outputSize < 2U)
  {
    scSetText(errorText, errorTextSize, "Ausgabepuffer zu klein");
    return false;
  }
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  if (!tpSystemCalStatus.signatureValid)
  {
    scSetText(errorText, errorTextSize, "Keine gültige aktive Systemkalibrierung");
    output[0] = '\0';
    return false;
  }
  size_t length = 0U;
  if (!scReadSystemFile(TP_CAL_ACTIVE_SYSTEM, output, outputSize, &length))
  {
    scSetText(errorText, errorTextSize, "Aktive Systemkalibrierung konnte nicht gelesen werden");
    output[0] = '\0';
    return false;
  }
  if (outputLength != nullptr) *outputLength = length;
  scSetText(errorText, errorTextSize, tpSignedCalText.t000);
  return true;
}

static uint32_t FLASHMEM scYmdFromUnix(int64_t unixTime)
{
  if (unixTime <= 0) return 0U;
  const time_t t = (time_t)unixTime;
  return (uint32_t)year(t) * 10000UL +
         (uint32_t)month(t) * 100UL +
         (uint32_t)day(t);
}

static void FLASHMEM scFillCertificateRecord(
    const TpSystemCalibrationStatus& status,
    bool active,
    bool activeBindingValid,
    TpCalibrationCertificateRecord& record)
{
  memset(&record, 0, sizeof(record));
  record.active = active;
  record.activeBindingValid = activeBindingValid;
  record.externalCertificateBound = status.externalCertificateBound;
  ScApplicabilityState applicability;
  if (scApplicabilityRead(status.manifestSha256, applicability))
  {
    record.applicabilityStateKnown = applicability.known;
    record.applicabilityEnded = applicability.ended;
    record.applicableUntilUtc = applicability.applicableUntilUtc;
    scSetText(record.applicabilityReason,
              sizeof(record.applicabilityReason),
              applicability.reasonCode);
  }
  scSetText(record.systemManifestSha256, sizeof(record.systemManifestSha256),
            status.manifestSha256);
  scSetText(record.calibrationId, sizeof(record.calibrationId),
            status.calibrationId);
  record.calibrationDateYmd = status.calibrationDateYmd;
  record.validFromYmd = scYmdFromUnix(status.validFromUtc);
  record.validUntilYmd = scYmdFromUnix(status.validUntilUtc);
  scSetText(record.certificateSource, sizeof(record.certificateSource),
            status.certificateSource);
  scSetText(record.externalCertificateDocumentId,
            sizeof(record.externalCertificateDocumentId),
            status.externalCertificateDocumentId);
  scSetText(record.externalCertificateNumber,
            sizeof(record.externalCertificateNumber),
            status.externalCertificateNumber);
}

bool FLASHMEM tpSignedCalibrationGetActiveCertificateRecord(
    TpCalibrationCertificateRecord& record)
{
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  if (!tpSystemCalStatus.signatureValid)
  {
    memset(&record, 0, sizeof(record));
    return false;
  }
  const bool bindingValid = tpSignedCalibrationSystemReady();
  if (!bindingValid || strcmp(tpSystemCalStatus.headType, headTypeTextGet()) != 0 ||
      tpSystemCalStatus.headSerial != R.head_serial)
  {
    memset(&record, 0, sizeof(record));
    return false;
  }
  scFillCertificateRecord(tpSystemCalStatus, true, true, record);
  return true;
}

bool FLASHMEM tpSignedCalibrationReadArchivedAdjustmentJson(
    bool device,
    const char* manifestSha256,
    char* output,
    size_t outputSize,
    size_t* outputLength,
    char* errorText,
    size_t errorTextSize)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (output == nullptr || outputSize < 2U ||
      !scManifestTextValid(manifestSha256) || !scEnsureDirectories())
  {
    if (output != nullptr && outputSize > 0U) output[0] = '\0';
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVED_ADJUSTMENT_UNAVAILABLE);
    return false;
  }

  char path[160];
  if (!scBuildArchivePath(device ? SC_ARCHIVE_DEVICE : SC_ARCHIVE_HEAD,
                          manifestSha256, path, sizeof(path)))
  {
    output[0] = '\0';
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVED_MANIFEST);
    return false;
  }
  size_t length = 0U;
  if (!scReadFile(path, output, outputSize, &length))
  {
    output[0] = '\0';
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVED_ADJUSTMENT_READ);
    return false;
  }

  TpSignedCalibrationStatus status;
  TpDeviceCalValues deviceValues;
  TpHeadCalValues headValues;
  if (device)
    (void)scVerifyDevicePackage(output, status, deviceValues);
  else
    (void)scVerifyHeadPackage(output, status, headValues);
  if (!status.signatureValid ||
      strcasecmp(status.manifestSha256, manifestSha256) != 0)
  {
    output[0] = '\0';
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVED_ADJUSTMENT_INVALID);
    return false;
  }
  if (outputLength != nullptr) *outputLength = length;
  scSetText(errorText, errorTextSize, tpSignedCalText.t000);
  return true;
}

bool FLASHMEM tpSignedCalibrationReadArchivedSystemJson(
    const char* manifestSha256,
    char* output,
    size_t outputSize,
    size_t* outputLength,
    char* errorText,
    size_t errorTextSize)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (output == nullptr || outputSize < 2U ||
      !scManifestTextValid(manifestSha256) || !scEnsureDirectories())
  {
    if (output != nullptr && outputSize > 0U) output[0] = '\0';
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVED_SYSTEM_UNAVAILABLE);
    return false;
  }

  char path[160];
  if (!scBuildArchivePath(SC_ARCHIVE_SYSTEM, manifestSha256,
                          path, sizeof(path)))
  {
    output[0] = '\0';
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVED_MANIFEST);
    return false;
  }
  size_t length = 0U;
  if (!scReadSystemFile(path, output, outputSize, &length))
  {
    output[0] = '\0';
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVED_SYSTEM_READ);
    return false;
  }

  const bool previousMode = tpSystemVerificationSignatureOnly;
  tpSystemVerificationSignatureOnly = true;
  (void)scVerifySystemPackage(output, tpSystemSelectionStatus,
                              tpSystemSelectionValues);
  tpSystemVerificationSignatureOnly = previousMode;
  if (!tpSystemSelectionStatus.signatureValid ||
      strcasecmp(tpSystemSelectionStatus.manifestSha256,
                 manifestSha256) != 0)
  {
    output[0] = '\0';
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVED_SYSTEM_INVALID);
    return false;
  }
  if (outputLength != nullptr) *outputLength = length;
  scSetText(errorText, errorTextSize, tpSignedCalText.t000);
  return true;
}

static bool FLASHMEM scCertificateRecordNewer(
    const TpCalibrationCertificateRecord& a,
    const TpCalibrationCertificateRecord& b)
{
  if (a.validFromYmd != b.validFromYmd)
    return a.validFromYmd > b.validFromYmd;
  if (a.calibrationDateYmd != b.calibrationDateYmd)
    return a.calibrationDateYmd > b.calibrationDateYmd;
  return strcasecmp(a.systemManifestSha256, b.systemManifestSha256) > 0;
}

bool FLASHMEM tpSignedCalibrationSearchCertificates(
    uint32_t searchFromYmd,
    uint32_t searchUntilYmd,
    TpCalibrationCertificateRecord* records,
    size_t maxRecords,
    size_t* returnedRecords,
    size_t* totalMatches,
    bool* truncated,
    char* errorText,
    size_t errorTextSize)
{
  if (returnedRecords != nullptr) *returnedRecords = 0U;
  if (totalMatches != nullptr) *totalMatches = 0U;
  if (truncated != nullptr) *truncated = false;
  if (!scValidDateYmd(searchFromYmd) || !scValidDateYmd(searchUntilYmd) ||
      searchFromYmd > searchUntilYmd ||
      (maxRecords > 0U && records == nullptr))
  {
    scSetText(errorText, errorTextSize, SC_ERR_SEARCH_RANGE);
    return false;
  }
  if (!scEnsureDirectories())
  {
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVE_UNAVAILABLE);
    return false;
  }

  File dir = SD.open(TP_CAL_ARCHIVE_SYSTEM_DIR);
  if (!dir || !dir.isDirectory())
  {
    if (dir) dir.close();
    scSetText(errorText, errorTextSize, SC_ERR_ARCHIVE_READ);
    return false;
  }

  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  const bool activeBindingValid = tpSystemCalStatus.signatureValid
      ? tpSignedCalibrationSystemReady() : false;
  const char* activeManifest = activeBindingValid
      ? tpSystemCalStatus.manifestSha256 : tpSignedCalText.t000;

  size_t kept = 0U;
  size_t matches = 0U;
  File entry;
  while ((entry = dir.openNextFile()))
  {
    if (entry.isDirectory())
    {
      entry.close();
      continue;
    }
    const char* base = scBaseName(entry.name());
    if (!scHasExtension(base, TP_CAL_ARCHIVE_SYSTEM_EXTENSION))
    {
      entry.close();
      continue;
    }
    char path[160];
    const int pathLength = snprintf(path, sizeof(path), tpSignedCalText.t099,
                                    TP_CAL_ARCHIVE_SYSTEM_DIR, base);
    entry.close();
    if (pathLength <= 0 || (size_t)pathLength >= sizeof(path)) continue;

    size_t jsonLength = 0U;
    if (!scReadSystemFile(path, tpSignedCalJson, sizeof(tpSignedCalJson),
                          &jsonLength))
      continue;
    const bool previousMode = tpSystemVerificationSignatureOnly;
    tpSystemVerificationSignatureOnly = true;
    (void)scVerifySystemPackage(tpSignedCalJson, tpSystemSelectionStatus,
                                tpSystemSelectionValues);
    tpSystemVerificationSignatureOnly = previousMode;
    if (!tpSystemSelectionStatus.signatureValid ||
        strcmp(tpSystemSelectionValues.headType, headTypeTextGet()) != 0 ||
        tpSystemSelectionValues.headSerial != R.head_serial)
      continue;
    const uint32_t validFromYmd =
        scYmdFromUnix(tpSystemSelectionStatus.validFromUtc);
    const uint32_t validUntilYmd =
        scYmdFromUnix(tpSystemSelectionStatus.validUntilUtc);
    if (!scValidDateYmd(validFromYmd) || !scValidDateYmd(validUntilYmd) ||
        validFromYmd > searchUntilYmd || validUntilYmd < searchFromYmd)
      continue;

    matches++;
    if (maxRecords == 0U) continue;
    TpCalibrationCertificateRecord candidate;
    const bool active = activeManifest[0] != '\0' &&
                        strcasecmp(activeManifest,
                                   tpSystemSelectionStatus.manifestSha256) == 0;
    const bool currentConfigurationMatches = active
        ? activeBindingValid
        : scSystemBindingMatchesCurrent(tpSystemSelectionValues, false);
    scFillCertificateRecord(tpSystemSelectionStatus, active,
                            currentConfigurationMatches,
                            candidate);

    size_t insertAt = 0U;
    while (insertAt < kept &&
           !scCertificateRecordNewer(candidate, records[insertAt]))
      insertAt++;
    if (insertAt >= maxRecords) continue;
    const size_t last = kept < maxRecords ? kept : maxRecords - 1U;
    for (size_t i = last; i > insertAt; --i)
      records[i] = records[i - 1U];
    records[insertAt] = candidate;
    if (kept < maxRecords) kept++;
  }
  dir.close();

  if (returnedRecords != nullptr) *returnedRecords = kept;
  if (totalMatches != nullptr) *totalMatches = matches;
  if (truncated != nullptr) *truncated = matches > kept;
  scSetText(errorText, errorTextSize, tpSignedCalText.t000);
  return true;
}


bool FLASHMEM tpSignedCalibrationCopyActiveJsonTo(Print& output,
                                                   bool device,
                                                   char* errorText,
                                                   size_t errorTextSize)
{
  size_t length = 0U;
  if (!tpSignedCalibrationReadActiveJson(device,
                                         tpSignedCalJson,
                                         sizeof(tpSignedCalJson),
                                         &length,
                                         errorText,
                                         errorTextSize))
  {
    return false;
  }
  if (output.write((const uint8_t*)tpSignedCalJson, length) != length)
  {
    scSetText(errorText, errorTextSize, SC_ERR_ACTIVE_ADJUSTMENT_OUTPUT);
    return false;
  }
  return true;
}

bool FLASHMEM tpSignedCalibrationCopyActiveSystemJsonTo(Print& output,
                                                         char* errorText,
                                                         size_t errorTextSize)
{
  size_t length = 0U;
  if (!tpSignedCalibrationReadActiveSystemJson(tpSignedCalJson,
                                               sizeof(tpSignedCalJson),
                                               &length,
                                               errorText,
                                               errorTextSize))
  {
    return false;
  }
  if (output.write((const uint8_t*)tpSignedCalJson, length) != length)
  {
    scSetText(errorText, errorTextSize, SC_ERR_ACTIVE_SYSTEM_OUTPUT);
    return false;
  }
  return true;
}

TpCalibrationTimeStatus FLASHMEM tpSignedCalibrationTimeStatus(
    const TpSignedCalibrationStatus& status,
    int64_t unixTimeUtc)
{
  if (!status.signatureValid || unixTimeUtc <= 0 || status.validUntilUtc <= status.validFromUtc)
    return TP_CAL_TIME_UNKNOWN;
  if (unixTimeUtc < status.validFromUtc) return TP_CAL_TIME_NOT_YET_VALID;
  if (unixTimeUtc > status.validUntilUtc) return TP_CAL_TIME_EXPIRED;
  return TP_CAL_TIME_VALID;
}

const char* FLASHMEM tpSignedCalibrationTimeStatusText(TpCalibrationTimeStatus status,
                                                        bool english)
{
  switch (status)
  {
    case TP_CAL_TIME_NOT_YET_VALID: return english ? tpSignedCalText.t105 : tpSignedCalText.t106;
    case TP_CAL_TIME_VALID: return english ? tpSignedCalText.t107 : tpSignedCalText.t108;
    case TP_CAL_TIME_EXPIRED: return english ? tpSignedCalText.t109 : tpSignedCalText.t110;
    default: return english ? tpSignedCalText.t111 : tpSignedCalText.t112;
  }
}

static bool FLASHMEM scBuildRequestJson(bool device,
                                        char* output,
                                        size_t outputSize,
                                        size_t* outputLength,
                                        char* downloadName,
                                        size_t downloadNameSize,
                                        char* errorText,
                                        size_t errorTextSize)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (!deviceIdentityCertificateValid())
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t113);
    return false;
  }
  if (output == nullptr || outputSize < 4096U)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t114);
    return false;
  }
  uint8_t requestIdBytes[16];
  if (!deviceIdentityRandomBytes(requestIdBytes, sizeof(requestIdBytes)))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t115);
    return false;
  }
  char requestId[33];
  scBytesToHex(requestIdBytes, sizeof(requestIdBytes), requestId, sizeof(requestId), true);
  const int64_t createdUtc = scCurrentUnixTime();
  if (createdUtc <= 0)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t116);
    return false;
  }
  char createdText[48];
  scFormatIsoUtc(createdUtc, createdText, sizeof(createdText));
  char certificateJson[2048];
  size_t certificateLength = 0U;
  if (!deviceIdentityBuildCertificateJson(certificateJson, sizeof(certificateJson), &certificateLength))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t117);
    return false;
  }
  const char* serial = deviceIdentityCertifiedSerial();
  const char* keyId = deviceIdentityDeviceKeyId();
  const char* certificateSerial = deviceIdentityCertificateSerialText();
  TpCanonicalWriter w(tpSignedCalCanonical, sizeof(tpSignedCalCanonical));
  int length = -1;
  if (device)
  {
    TpDeviceCalValues v;
    scCurrentDeviceValues(v);
    if (!scValidDateYmd(v.calibrationDateYmd) || !scValidDateYmd(v.referenceDateYmd))
    {
      scSetText(errorText, errorTextSize, tpSignedCalText.t130);
      return false;
    }
    if (!scWriteRequestPrefix(w, TP_CAL_FORMAT_DEVICE_REQUEST, serial, keyId, certificateSerial, requestId, createdUtc) ||
        !scWriteDeviceValues(w, v) ||
        !w.writeUtf8(TP_CAL_FIRMWARE_VERSION) || !w.writeUtf8(TP_CAL_FIRMWARE_BUILD_ID) || !w.valid())
    {
      scSetText(errorText, errorTextSize, tpSignedCalText.t118);
      return false;
    }
    uint8_t hash[32];
    uint8_t signature[64];
    TpSha256::hash(tpSignedCalCanonical, w.length(), hash);
    if (!deviceIdentitySignHash(hash, signature))
    {
      scSetText(errorText, errorTextSize, tpSignedCalText.t119);
      return false;
    }
    char hashHex[65];
    char signatureBase64[89];
    scBytesToHex(hash, 32U, hashHex, sizeof(hashHex), false);
    scBase64Encode(signature, 64U, signatureBase64, sizeof(signatureBase64));
    length = snprintf(output, outputSize,
      tpSignedCalText.t120,
      TP_CAL_FORMAT_DEVICE_REQUEST,
      serial, keyId, certificateSerial, requestId, createdText,
      (unsigned long)v.calibrationDateYmd,
      (unsigned long)v.referenceDateYmd,
      (long)v.refLowScaled, (long)v.refHighScaled,
      (long)v.chALowScaled, (long)v.chAHighScaled,
      (long)v.chBLowScaled, (long)v.chBHighScaled,
      TP_CAL_FIRMWARE_VERSION, TP_CAL_FIRMWARE_BUILD_ID,
      certificateJson, hashHex,
      TP_CAL_SIGNATURE_ALGORITHM, TP_CAL_SIGNATURE_ENCODING,
      signatureBase64);
    if (downloadName != nullptr && downloadNameSize > 0U)
      snprintf(downloadName, downloadNameSize, tpSignedCalText.t121, serial, requestId);
  }
  else
  {
    TpHeadCalValues v;
    scCurrentHeadValues(v);
    if (!scValidDateYmd(v.calibrationDateYmd) || v.headSerial == 0U || v.headSerial > HEAD_SERIAL_MAX)
    {
      scSetText(errorText, errorTextSize, tpSignedCalText.t122);
      return false;
    }
    if (!scWriteRequestPrefix(w, TP_CAL_FORMAT_HEAD_REQUEST, serial, keyId, certificateSerial, requestId, createdUtc) ||
        !scWriteHeadValues(w, v) ||
        !w.writeUtf8(TP_CAL_FIRMWARE_VERSION) || !w.writeUtf8(TP_CAL_FIRMWARE_BUILD_ID) || !w.valid())
    {
      scSetText(errorText, errorTextSize, tpSignedCalText.t123);
      return false;
    }
    uint8_t hash[32];
    uint8_t signature[64];
    TpSha256::hash(tpSignedCalCanonical, w.length(), hash);
    if (!deviceIdentitySignHash(hash, signature))
    {
      scSetText(errorText, errorTextSize, tpSignedCalText.t124);
      return false;
    }
    char hashHex[65];
    char signatureBase64[89];
    scBytesToHex(hash, 32U, hashHex, sizeof(hashHex), false);
    scBase64Encode(signature, 64U, signatureBase64, sizeof(signatureBase64));
    length = snprintf(output, outputSize,
      tpSignedCalText.t125,
      TP_CAL_FORMAT_HEAD_REQUEST,
      serial, keyId, certificateSerial, requestId, createdText,
      v.headType, (unsigned long)v.headSerial,
      (unsigned long)v.calibrationDateYmd, (unsigned long)v.calibrationTimeHms,
      (long)v.r0MirrorScaled, (long)v.r0AmbientScaled,
      (unsigned)v.mirror2PointActive,
      (long)v.mirrorSet1_mC, (long)v.mirrorActual1_mC,
      (long)v.mirrorSet2_mC, (long)v.mirrorActual2_mC,
      (unsigned)v.ambient2PointActive,
      (long)v.ambientSet1_mC, (long)v.ambientActual1_mC,
      (long)v.ambientSet2_mC, (long)v.ambientActual2_mC,
      (long)v.dewFrostOffset_mC,
      (unsigned)v.pidKp, (long)v.pidKi_x1000, (long)v.pidKd_x1000,
      (unsigned)v.controlInterval_ms, (unsigned)v.hBridgeDeadtime_ms,
      (unsigned)v.fanPercent, (unsigned)v.opticalTarget_x10,
      (unsigned)v.peltierCurrentLimit_mA,
      TP_CAL_FIRMWARE_VERSION, TP_CAL_FIRMWARE_BUILD_ID,
      certificateJson, hashHex,
      TP_CAL_SIGNATURE_ALGORITHM, TP_CAL_SIGNATURE_ENCODING,
      signatureBase64);
    if (downloadName != nullptr && downloadNameSize > 0U)
      snprintf(downloadName, downloadNameSize, tpSignedCalText.t126,
               v.headType, (unsigned long)v.headSerial, requestId);
  }
  if (length <= 0 || (size_t)length >= outputSize)
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t127);
    return false;
  }
  if (outputLength != nullptr) *outputLength = (size_t)length;
  scSetText(errorText, errorTextSize, tpSignedCalText.t104);
  return true;
}

bool FLASHMEM tpSignedCalibrationBuildDeviceRequestJson(char* output,
                                                         size_t outputSize,
                                                         size_t* outputLength,
                                                         char* downloadName,
                                                         size_t downloadNameSize,
                                                         char* errorText,
                                                         size_t errorTextSize)
{
  if (!scRequireFirmwareHashAvailable(errorText, errorTextSize)) return false;
  return scBuildRequestJson(true, output, outputSize, outputLength,
                            downloadName, downloadNameSize, errorText, errorTextSize);
}

bool FLASHMEM tpSignedCalibrationBuildHeadRequestJson(char* output,
                                                       size_t outputSize,
                                                       size_t* outputLength,
                                                       char* downloadName,
                                                       size_t downloadNameSize,
                                                       char* errorText,
                                                       size_t errorTextSize)
{
  if (!scRequireFirmwareHashAvailable(errorText, errorTextSize)) return false;
  return scBuildRequestJson(false, output, outputSize, outputLength,
                            downloadName, downloadNameSize, errorText, errorTextSize);
}

bool FLASHMEM tpSignedCalibrationBuildSystemRequestJson(char* output,
                                                         size_t outputSize,
                                                         size_t* outputLength,
                                                         char* downloadName,
                                                         size_t downloadNameSize,
                                                         char* errorText,
                                                         size_t errorTextSize)
{
  if (!scRequireFirmwareHashAvailable(errorText, errorTextSize)) return false;
  if (outputLength != nullptr) *outputLength = 0U;
  if (!deviceIdentityCertificateValid())
  {
    scSetText(errorText, errorTextSize, "Zuerst ein gültiges Gerätezertifikat aktivieren");
    return false;
  }
  if (!tpSignedCalibrationDeviceReady() || !tpSignedCalibrationHeadReady())
  {
    scSetText(errorText, errorTextSize, "Systemkalibrierung erfordert aktive Geräte- und Kopfjustierung");
    return false;
  }
  const uint32_t calibrationDateYmd = scCurrentYmd();
  if (!scValidDateYmd(calibrationDateYmd) || R.head_serial == 0U || R.head_serial > HEAD_SERIAL_MAX)
  {
    scSetText(errorText, errorTextSize, "Kopf-SN oder Gerätezeit ist ungültig");
    return false;
  }
  if (output == nullptr || outputSize < 4096U)
  {
    scSetText(errorText, errorTextSize, "Ausgabepuffer ist zu klein");
    return false;
  }

  TpSystemCalValues v;
  memset(&v, 0, sizeof(v));
  v.calibrationDateYmd = calibrationDateYmd;
  scSetText(v.headType, sizeof(v.headType), headTypeTextGet());
  v.headSerial = R.head_serial;
  scSetText(v.deviceAdjustmentCalibrationId, sizeof(v.deviceAdjustmentCalibrationId), tpDeviceCalStatus.calibrationId);
  scSetText(v.headAdjustmentCalibrationId, sizeof(v.headAdjustmentCalibrationId), tpHeadCalStatus.calibrationId);
  if (!scHexToBytes(tpDeviceCalStatus.manifestSha256, v.deviceAdjustmentManifest, 32U) ||
      !scHexToBytes(tpHeadCalStatus.manifestSha256, v.headAdjustmentManifest, 32U))
  {
    scSetText(errorText, errorTextSize, "Justierungsbindung enthält keinen gültigen SHA-256-Hash");
    return false;
  }

  if (tpExternalCalibrationActiveReady())
  {
    const TpExternalCalibrationMetadata& external = tpExternalCalibrationActive();
    v.externalCertificateAvailable = 1U;
    scSetText(v.externalCertificateDocumentId, sizeof(v.externalCertificateDocumentId), external.documentId);
    scSetText(v.externalCertificateNumber, sizeof(v.externalCertificateNumber), external.certificateNumber);
    v.externalCertificateValidFromYmd = external.validFromYmd;
    v.externalCertificateValidUntilYmd = external.validUntilYmd;
    scSetText(v.externalCertificateOriginalFileName, sizeof(v.externalCertificateOriginalFileName), external.originalFileName);
    v.externalCertificateFileSize = external.fileSize;
    if (!scHexToBytes(external.sha256, v.externalCertificateSha256, 32U))
    {
      scSetText(errorText, errorTextSize, "SHA-256 des externen Kalibrierscheins ist ungültig");
      return false;
    }
  }

  uint8_t requestIdBytes[16];
  if (!deviceIdentityRandomBytes(requestIdBytes, sizeof(requestIdBytes)))
  {
    scSetText(errorText, errorTextSize, "Zufällige Anfrage-ID konnte nicht erzeugt werden");
    return false;
  }
  char requestId[33];
  scBytesToHex(requestIdBytes, sizeof(requestIdBytes), requestId, sizeof(requestId), true);
  const int64_t createdUtc = scCurrentUnixTime();
  if (createdUtc <= 0)
  {
    scSetText(errorText, errorTextSize, "RTC-Zeit oder UTC-Offset ungültig; PC-Zeit übertragen");
    return false;
  }
  char createdText[48];
  scFormatIsoUtc(createdUtc, createdText, sizeof(createdText));
  char certificateJson[2048];
  size_t certificateLength = 0U;
  if (!deviceIdentityBuildCertificateJson(certificateJson, sizeof(certificateJson), &certificateLength))
  {
    scSetText(errorText, errorTextSize, "Gerätezertifikat konnte nicht eingebettet werden");
    return false;
  }
  const char* serial = deviceIdentityCertifiedSerial();
  const char* keyId = deviceIdentityDeviceKeyId();
  const char* certificateSerial = deviceIdentityCertificateSerialText();
  TpCanonicalWriter w(tpSignedCalCanonical, sizeof(tpSignedCalCanonical));
  if (!scWriteRequestPrefix(w, TP_CAL_FORMAT_SYSTEM_REQUEST, serial, keyId, certificateSerial, requestId, createdUtc) ||
      !scWriteSystemValuesV2(w, v) || !w.writeUtf8(TP_CAL_FIRMWARE_VERSION) ||
      !w.writeUtf8(TP_CAL_FIRMWARE_BUILD_ID) || !w.valid())
  {
    scSetText(errorText, errorTextSize, "Kanonische Systemkalibrierungsanfrage ist zu groß");
    return false;
  }
  uint8_t hash[32];
  uint8_t signature[64];
  TpSha256::hash(tpSignedCalCanonical, w.length(), hash);
  if (!deviceIdentitySignHash(hash, signature))
  {
    scSetText(errorText, errorTextSize, "Gerätesignatur der Systemkalibrierungsanfrage fehlgeschlagen");
    return false;
  }
  char hashHex[65];
  char deviceAdjustmentHash[65];
  char headAdjustmentHash[65];
  char signatureBase64[89];
  scBytesToHex(hash, 32U, hashHex, sizeof(hashHex), false);
  scBytesToHex(v.deviceAdjustmentManifest, 32U, deviceAdjustmentHash, sizeof(deviceAdjustmentHash), false);
  scBytesToHex(v.headAdjustmentManifest, 32U, headAdjustmentHash, sizeof(headAdjustmentHash), false);
  scBase64Encode(signature, 64U, signatureBase64, sizeof(signatureBase64));
  const int length = snprintf(output, outputSize, SC_SYSTEM_REQUEST_TEMPLATE,
      TP_CAL_FORMAT_SYSTEM_REQUEST, serial, keyId, certificateSerial, requestId, createdText,
      (unsigned long)v.calibrationDateYmd, v.headType, (unsigned long)v.headSerial,
      v.deviceAdjustmentCalibrationId, deviceAdjustmentHash,
      v.headAdjustmentCalibrationId, headAdjustmentHash,
      (unsigned)v.externalCertificateAvailable, v.externalCertificateDocumentId,
      v.externalCertificateNumber, (unsigned long)v.externalCertificateValidFromYmd,
      (unsigned long)v.externalCertificateValidUntilYmd, v.externalCertificateOriginalFileName,
      (unsigned long)v.externalCertificateFileSize,
      (v.externalCertificateAvailable ? tpExternalCalibrationActive().sha256 : "0000000000000000000000000000000000000000000000000000000000000000"),
      TP_CAL_FIRMWARE_VERSION, TP_CAL_FIRMWARE_BUILD_ID, certificateJson, hashHex,
      TP_CAL_SIGNATURE_ALGORITHM, TP_CAL_SIGNATURE_ENCODING, signatureBase64);
  if (length <= 0 || (size_t)length >= outputSize)
  {
    scSetText(errorText, errorTextSize, "Systemkalibrierungsanfrage passt nicht in den Ausgabepuffer");
    return false;
  }

  // Der exakte SHA-256 des laufenden Firmwareabbilds und der optionale
  // Herstellerzertifikatsstatus werden als eigener, geraetesignierter
  // Kontext an genau dieses Anfrage-Manifest gebunden. Das gilt identisch
  // fuer TP-3000-eigene und externe PDF-Kalibrierscheine.
  char firmwareContextError[160];
  if (!tpSystemFirmwareContextCaptureRequest(
          requestId, hash, createdUtc,
          firmwareContextError, sizeof(firmwareContextError)))
  {
    scSetText(errorText, errorTextSize,
              firmwareContextError[0] != '\0'
                  ? firmwareContextError
                  : "Firmwarekontext der Systemkalibrierung konnte nicht gespeichert werden");
    return false;
  }
  if (downloadName != nullptr && downloadNameSize > 0U)
    snprintf(downloadName, downloadNameSize, "TP3000_G%s_%s_K%05lu_%.8s.tpscalreq",
             serial, v.headType, (unsigned long)v.headSerial, requestId);
  if (outputLength != nullptr) *outputLength = (size_t)length;
  scSetText(errorText, errorTextSize, "OK");
  return true;
}

bool FLASHMEM tpSignedCalibrationSaveRequestToSd(const char* json,
                                                  size_t jsonLength,
                                                  const char* downloadName,
                                                  char* storedPath,
                                                  size_t storedPathSize,
                                                  char* errorText,
                                                  size_t errorTextSize)
{
  if (json == nullptr || jsonLength == 0U || downloadName == nullptr || !scEnsureDirectories())
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t128);
    return false;
  }
  char path[128];
  snprintf(path, sizeof(path), tpSignedCalText.t099, TP_CAL_REQUEST_DIR, downloadName);
  if (!scWriteFile(path, json, jsonLength))
  {
    scSetText(errorText, errorTextSize, tpSignedCalText.t129);
    return false;
  }
  if (storedPath != nullptr && storedPathSize > 0U) scSetText(storedPath, storedPathSize, path);
  scSetText(errorText, errorTextSize, tpSignedCalText.t104);
  return true;
}

bool FLASHMEM tpSignedCalibrationCreateDeviceRequestOnSd(char* storedPath,
                                                           size_t storedPathSize,
                                                           char* errorText,
                                                           size_t errorTextSize)
{
  char downloadName[96];
  size_t length = 0U;
  if (!tpSignedCalibrationBuildDeviceRequestJson(tpSignedCalJson,
                                                  sizeof(tpSignedCalJson),
                                                  &length,
                                                  downloadName,
                                                  sizeof(downloadName),
                                                  errorText,
                                                  errorTextSize))
  {
    return false;
  }
  return tpSignedCalibrationSaveRequestToSd(tpSignedCalJson,
                                             length,
                                             downloadName,
                                             storedPath,
                                             storedPathSize,
                                             errorText,
                                             errorTextSize);
}

bool FLASHMEM tpSignedCalibrationCreateHeadRequestOnSd(char* storedPath,
                                                         size_t storedPathSize,
                                                         char* errorText,
                                                         size_t errorTextSize)
{
  char downloadName[96];
  size_t length = 0U;
  if (!tpSignedCalibrationBuildHeadRequestJson(tpSignedCalJson,
                                                sizeof(tpSignedCalJson),
                                                &length,
                                                downloadName,
                                                sizeof(downloadName),
                                                errorText,
                                                errorTextSize))
  {
    return false;
  }
  return tpSignedCalibrationSaveRequestToSd(tpSignedCalJson,
                                             length,
                                             downloadName,
                                             storedPath,
                                             storedPathSize,
                                             errorText,
                                             errorTextSize);
}

bool FLASHMEM tpSignedCalibrationCreateSystemRequestOnSd(char* storedPath,
                                                           size_t storedPathSize,
                                                           char* errorText,
                                                           size_t errorTextSize)
{
  char downloadName[128];
  size_t length = 0U;
  if (!tpSignedCalibrationBuildSystemRequestJson(tpSignedCalJson, sizeof(tpSignedCalJson), &length,
                                                 downloadName, sizeof(downloadName),
                                                 errorText, errorTextSize)) return false;
  return tpSignedCalibrationSaveRequestToSd(tpSignedCalJson, length, downloadName,
                                             storedPath, storedPathSize, errorText, errorTextSize);
}

static bool FLASHMEM scImportAdjustmentJson(bool device,
                                             const char* json,
                                             size_t jsonLength,
                                             char* errorText,
                                             size_t errorTextSize)
{
  if (json == nullptr || jsonLength == 0U || jsonLength >= TP_CAL_SIGNED_JSON_MAX)
  {
    scSetText(errorText, errorTextSize,
              device ? "Gerätejustierungsdatei ist leer oder zu groß"
                     : "Kopfjustierungsdatei ist leer oder zu groß");
    return false;
  }

  if (!scEnsureDirectories())
  {
    scSetText(errorText, errorTextSize, SC_ERR_CALIBRATION_DIR_UNAVAILABLE);
    return false;
  }

  const char* activePath = device ? TP_CAL_ACTIVE_DEVICE : TP_CAL_ACTIVE_HEAD;
  const char* tempPath = device ? TP_CAL_ACTIVE_DEVICE_TEMP : TP_CAL_ACTIVE_HEAD_TEMP;
  const char* backupPath = device ? TP_CAL_ACTIVE_DEVICE_BACKUP : TP_CAL_ACTIVE_HEAD_BACKUP;

  // Der aktive Altstand wird vor dem Belegen des gemeinsamen JSON-Puffers
  // eingelesen und archiviert. tpSignedCalibrationBegin() verwendet denselben
  // Puffer und darf deshalb nicht erst nach dem Kopieren des Uploads laufen.
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  const TpSignedCalibrationStatus& previousStatus = device ? tpDeviceCalStatus : tpHeadCalStatus;
  if (SD.exists(activePath) && previousStatus.signatureValid &&
      !scArchiveActivePackage(device ? SC_ARCHIVE_DEVICE : SC_ARCHIVE_HEAD,
                              activePath, previousStatus.manifestSha256))
  {
    scSetText(errorText, errorTextSize,
              device ? SC_ERR_PREVIOUS_DEVICE_ARCHIVE
                     : SC_ERR_PREVIOUS_HEAD_ARCHIVE);
    return false;
  }

  memcpy(tpSignedCalJson, json, jsonLength);
  tpSignedCalJson[jsonLength] = '\0';

  TpSignedCalibrationStatus candidate;
  TpDeviceCalValues candidateDeviceValues;
  TpHeadCalValues candidateHeadValues;
  const bool verified = device
    ? scVerifyDevicePackage(tpSignedCalJson, candidate, candidateDeviceValues)
    : scVerifyHeadPackage(tpSignedCalJson, candidate, candidateHeadValues);
  if (!verified)
  {
    scSetText(errorText, errorTextSize,
              candidate.lastError[0] != '\0' ? candidate.lastError
                                              : (device ? "Gerätejustierung wurde abgewiesen"
                                                        : "Kopfjustierung wurde abgewiesen"));
    return false;
  }

  if (SD.exists(tempPath)) SD.remove(tempPath);
  if (!scWriteFile(tempPath, tpSignedCalJson, jsonLength))
  {
    scSetText(errorText, errorTextSize,
              device ? "Temporäre Gerätejustierung konnte nicht gespeichert werden"
                     : "Temporäre Kopfjustierung konnte nicht gespeichert werden");
    return false;
  }

  if (SD.exists(backupPath)) SD.remove(backupPath);
  const bool hadActive = SD.exists(activePath);
  if (hadActive && !SD.rename(activePath, backupPath))
  {
    SD.remove(tempPath);
    scSetText(errorText, errorTextSize,
              device ? SC_ERR_PREVIOUS_DEVICE_SAVE
                     : SC_ERR_PREVIOUS_HEAD_SAVE);
    return false;
  }

  if (!SD.rename(tempPath, activePath))
  {
    SD.remove(tempPath);
    if (hadActive && SD.exists(backupPath)) SD.rename(backupPath, activePath);
    scSetText(errorText, errorTextSize,
              device ? "Gerätejustierung konnte nicht aktiviert werden"
                     : "Kopfjustierung konnte nicht aktiviert werden");
    return false;
  }

  // Alle drei Nachweise neu einlesen. Eine vorhandene Systemkalibrierung
  // bleibt als Datei erhalten, wird aber bei geänderter Justierungsbindung
  // automatisch als nicht aktiv bewertet.
  tpSignedCalibrationLoaded = false;
  tpSignedCalibrationTransactionReload = true;
  tpSignedCalibrationBegin();
  tpSignedCalibrationTransactionReload = false;
  const bool activeOk = device ? tpSignedCalibrationDeviceReady()
                               : tpSignedCalibrationHeadReady();
  if (!activeOk)
  {
    const TpSignedCalibrationStatus& failedStatus = device ? tpDeviceCalStatus : tpHeadCalStatus;
    char failedReason[160];
    scSetText(failedReason, sizeof(failedReason), failedStatus.lastError);
    SD.remove(activePath);
    if (hadActive && SD.exists(backupPath)) SD.rename(backupPath, activePath);
    tpSignedCalibrationLoaded = false;
    tpSignedCalibrationBegin();
    scSetText(errorText, errorTextSize,
              failedReason[0] != '\0' ? failedReason
                                      : (device ? "Aktivierte Gerätejustierung konnte nicht erneut geprüft werden"
                                                : "Aktivierte Kopfjustierung konnte nicht erneut geprüft werden"));
    return false;
  }

  const TpSignedCalibrationStatus& activatedStatus = device ? tpDeviceCalStatus : tpHeadCalStatus;
  if (!scArchiveActivePackage(device ? SC_ARCHIVE_DEVICE : SC_ARCHIVE_HEAD,
                              activePath, activatedStatus.manifestSha256))
  {
    SD.remove(activePath);
    if (hadActive && SD.exists(backupPath)) SD.rename(backupPath, activePath);
    tpSignedCalibrationLoaded = false;
    tpSignedCalibrationBegin();
    scSetText(errorText, errorTextSize,
              device ? SC_ERR_NEW_DEVICE_ARCHIVE
                     : SC_ERR_NEW_HEAD_ARCHIVE);
    return false;
  }

  // Erst nach vollständiger Prüfung und Archivierung darf eine gebrochene
  // Bindung den bisherigen Systemschein dauerhaft beenden.
  if (!scRecordCurrentSystemMismatchIfNeeded())
  {
    char applicabilityError[160];
    scSetText(applicabilityError, sizeof(applicabilityError),
              tpSystemCalStatus.lastError);
    SD.remove(activePath);
    if (hadActive && SD.exists(backupPath)) SD.rename(backupPath, activePath);
    tpSignedCalibrationLoaded = false;
    tpSignedCalibrationBegin();
    scSetText(errorText, errorTextSize,
              applicabilityError[0] != '\0'
                  ? applicabilityError
                  : "Anwendbarkeitsende konnte nicht gespeichert werden");
    return false;
  }

  if (SD.exists(backupPath)) SD.remove(backupPath);
  scSetText(errorText, errorTextSize, "OK");
  return true;
}

bool FLASHMEM tpSignedCalibrationImportAdjustmentJson(bool device,
                                                       const char* json,
                                                       size_t jsonLength,
                                                       char* errorText,
                                                       size_t errorTextSize)
{
  if (!scRequireFirmwareHashAvailable(errorText, errorTextSize)) return false;
  const bool ok = scImportAdjustmentJson(device, json, jsonLength,
                                         errorText, errorTextSize);
  if (ok) (void)scSelectBestSystemForCurrentHead();
  return ok;
}

bool FLASHMEM tpSignedCalibrationImportSystemJson(const char* json,
                                                  size_t jsonLength,
                                                  char* errorText,
                                                  size_t errorTextSize)
{
  if (!scRequireFirmwareHashAvailable(errorText, errorTextSize)) return false;
  if (json == nullptr || jsonLength == 0U || jsonLength >= TP_CAL_SIGNED_JSON_MAX)
  {
    scSetText(errorText, errorTextSize, "Systemkalibrierungsdatei ist leer oder zu groß");
    return false;
  }
  if (!scEnsureDirectories())
  {
    scSetText(errorText, errorTextSize, SC_ERR_CALIBRATION_DIR_UNAVAILABLE);
    return false;
  }

  // Wie bei Geräte-/Kopfjustierungen muss der Altstand vor dem Kopieren des
  // Uploads in den gemeinsamen JSON-Puffer eingelesen und archiviert werden.
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();
  const bool previousSystemApplicable = tpSignedCalibrationSystemReady();
  char previousSystemManifest[65] = {0};
  if (previousSystemApplicable)
    scSetText(previousSystemManifest, sizeof(previousSystemManifest),
              tpSystemCalStatus.manifestSha256);
  if (SD.exists(TP_CAL_ACTIVE_SYSTEM) && tpSystemCalStatus.signatureValid &&
      !scArchiveActivePackage(SC_ARCHIVE_SYSTEM, TP_CAL_ACTIVE_SYSTEM,
                              tpSystemCalStatus.manifestSha256))
  {
    scSetText(errorText, errorTextSize,
              SC_ERR_PREVIOUS_SYSTEM_ARCHIVE);
    return false;
  }

  if (json != tpSignedCalJson)
    memcpy(tpSignedCalJson, json, jsonLength);
  tpSignedCalJson[jsonLength] = '\0';
  TpSystemCalibrationStatus candidate;
  TpSystemCalValues candidateValues;
  if (!scVerifySystemPackage(tpSignedCalJson, candidate, candidateValues))
  {
    scSetText(errorText, errorTextSize, candidate.lastError);
    return false;
  }

  if (!scWriteFile(TP_CAL_ACTIVE_SYSTEM_TEMP, tpSignedCalJson, jsonLength))
  {
    scSetText(errorText, errorTextSize, "Temporäre Systemkalibrierung konnte nicht gespeichert werden");
    return false;
  }
  // Transaktionaler Austausch: Die bisher aktive Systemkalibrierung bleibt
  // bis zum erfolgreichen Rename als Backup erhalten.
  if (SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP)) SD.remove(TP_CAL_ACTIVE_SYSTEM_BACKUP);
  const bool hadActive = SD.exists(TP_CAL_ACTIVE_SYSTEM);
  if (hadActive && !SD.rename(TP_CAL_ACTIVE_SYSTEM, TP_CAL_ACTIVE_SYSTEM_BACKUP))
  {
    SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
    scSetText(errorText, errorTextSize, SC_ERR_PREVIOUS_SYSTEM_SAVE);
    return false;
  }
  if (!SD.rename(TP_CAL_ACTIVE_SYSTEM_TEMP, TP_CAL_ACTIVE_SYSTEM))
  {
    SD.remove(TP_CAL_ACTIVE_SYSTEM_TEMP);
    if (hadActive && SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP))
      SD.rename(TP_CAL_ACTIVE_SYSTEM_BACKUP, TP_CAL_ACTIVE_SYSTEM);
    scSetText(errorText, errorTextSize, "Systemkalibrierung konnte nicht aktiviert werden");
    return false;
  }

  // Den gerade aktivierten Datensatz nochmals von der SD einlesen. Erst wenn
  // diese zweite Prüfung erfolgreich war, darf das Backup verworfen werden.
  // Damit bleibt auch bei einem Schreib-/Dateisystemfehler die vorherige
  // Systemkalibrierung wiederherstellbar.
  tpSignedCalibrationLoaded = true;
  if (!scLoadActiveSystemFile())
  {
    char failedReason[192];
    scSetText(failedReason, sizeof(failedReason), tpSystemCalStatus.lastError);
    SD.remove(TP_CAL_ACTIVE_SYSTEM);
    if (hadActive && SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP))
      SD.rename(TP_CAL_ACTIVE_SYSTEM_BACKUP, TP_CAL_ACTIVE_SYSTEM);
    tpSignedCalibrationLoaded = false;
    tpSignedCalibrationBegin();
    scSetText(errorText, errorTextSize,
              failedReason[0] != '\0' ? failedReason
                                       : "Aktivierte Systemkalibrierung konnte nicht erneut geprüft werden");
    return false;
  }

  if (!scArchiveActivePackage(SC_ARCHIVE_SYSTEM, TP_CAL_ACTIVE_SYSTEM,
                              tpSystemCalStatus.manifestSha256))
  {
    SD.remove(TP_CAL_ACTIVE_SYSTEM);
    if (hadActive && SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP))
      SD.rename(TP_CAL_ACTIVE_SYSTEM_BACKUP, TP_CAL_ACTIVE_SYSTEM);
    tpSignedCalibrationLoaded = false;
    tpSignedCalibrationBegin();
    scSetText(errorText, errorTextSize,
              SC_ERR_NEW_SYSTEM_ARCHIVE);
    return false;
  }

  // Altanfragen vor Build 55 besitzen noch keinen separaten Firmwarekontext.
  // Nur wenn Version und Build des signierten Pakets exakt der aktuell
  // laufenden Firmware entsprechen, darf beim Aktivieren ein klar als
  // Fallback gekennzeichneter Kontext nachgetragen werden. Andernfalls wird
  // bewusst kein historischer SHA-256 erfunden.
  (void)tpSystemFirmwareContextEnsureForActivation(
      tpSystemCalStatus.sourceRequestId,
      tpSystemCalStatus.sourceRequestManifestSha256,
      tpSystemCalStatus.firmwareVersion,
      tpSystemCalStatus.firmwareBuildId,
      scCurrentUnixTime());

  // Eine neue Systemkalibrierung beendet die bisher wirksame Kalibrierung
  // derselben Geräte-/Kopfkombination. Der alte signierte Schein bleibt
  // valide und im Archiv sichtbar, wird aber nicht mehr automatisch aktiv.
  if (previousSystemManifest[0] != '\0' &&
      strcasecmp(previousSystemManifest,
                 tpSystemCalStatus.manifestSha256) != 0)
  {
    char applicabilityError[160];
    if (!scApplicabilityAppend(previousSystemManifest,
                                SC_REASON_SYSTEM_REPLACED,
                                scCurrentUnixTime(),
                                applicabilityError,
                                sizeof(applicabilityError)))
    {
      SD.remove(TP_CAL_ACTIVE_SYSTEM);
      if (hadActive && SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP))
        SD.rename(TP_CAL_ACTIVE_SYSTEM_BACKUP, TP_CAL_ACTIVE_SYSTEM);
      tpSignedCalibrationLoaded = false;
      tpSignedCalibrationBegin();
      scSetText(errorText, errorTextSize,
                applicabilityError[0] != '\0'
                    ? applicabilityError
                    : "Anwendbarkeitsende des vorherigen Scheins konnte nicht gespeichert werden");
      return false;
    }
  }

  if (SD.exists(TP_CAL_ACTIVE_SYSTEM_BACKUP)) SD.remove(TP_CAL_ACTIVE_SYSTEM_BACKUP);
  (void)scSelectBestSystemForCurrentHead();
  scSetText(errorText, errorTextSize, "OK");
  return true;
}

bool FLASHMEM tpSignedCalibrationImportSystemFromSd(char* importedPath,
                                                     size_t importedPathSize,
                                                     char* errorText,
                                                     size_t errorTextSize)
{
  if (importedPath != nullptr && importedPathSize > 0U) importedPath[0] = '\0';
  if (!scRequireFirmwareHashAvailable(errorText, errorTextSize)) return false;
  if (!deviceIdentityCertificateValid())
  {
    scSetText(errorText, errorTextSize, SC_SYSTEM_IMPORT_CERT_REQUIRED);
    return false;
  }
  if (!scEnsureDirectories())
  {
    scSetText(errorText, errorTextSize, SC_ERR_CALIBRATION_DIR_UNAVAILABLE);
    return false;
  }

  File dir = SD.open(TP_CAL_DIR);
  if (!dir || !dir.isDirectory())
  {
    if (dir) dir.close();
    scSetText(errorText, errorTextSize, SC_SYSTEM_IMPORT_DIR_UNAVAILABLE);
    return false;
  }

  bool found = false;
  char bestPath[128] = {0};
  int64_t bestValidFromUtc = INT64_MIN;
  uint32_t bestCalibrationDateYmd = 0U;
  File entry;
  while ((entry = dir.openNextFile()))
  {
    if (entry.isDirectory())
    {
      entry.close();
      continue;
    }

    const char* name = scBaseName(entry.name());
    const bool activeFile = strcasecmp(name, SC_SYSTEM_IMPORT_ACTIVE_NAME) == 0;
    const bool extensionMatch = !activeFile &&
                                scHasExtension(name, TP_CAL_ARCHIVE_SYSTEM_EXTENSION);
    if (!extensionMatch)
    {
      entry.close();
      continue;
    }

    char path[128];
    const int pathLength = snprintf(path, sizeof(path), tpSignedCalText.t099,
                                    TP_CAL_DIR, name);
    entry.close();
    if (pathLength <= 0 || (size_t)pathLength >= sizeof(path)) continue;

    size_t length = 0U;
    if (!scReadSystemFile(path, tpSignedCalJson, sizeof(tpSignedCalJson), &length))
      continue;

    // Nur ein vollständig gültiger und zur aktuellen Geräte-/Kopfkombination
    // passender Schein darf durch den komfortablen SD-Import ausgewählt werden.
    if (!scVerifySystemPackage(tpSignedCalJson, tpSystemSelectionStatus,
                               tpSystemSelectionValues))
      continue;

    const bool newer = !found ||
        tpSystemSelectionStatus.validFromUtc > bestValidFromUtc ||
        (tpSystemSelectionStatus.validFromUtc == bestValidFromUtc &&
         tpSystemSelectionStatus.calibrationDateYmd > bestCalibrationDateYmd);
    if (!newer) continue;

    found = true;
    bestValidFromUtc = tpSystemSelectionStatus.validFromUtc;
    bestCalibrationDateYmd = tpSystemSelectionStatus.calibrationDateYmd;
    scSetText(bestPath, sizeof(bestPath), path);
  }
  dir.close();

  if (!found)
  {
    scSetText(errorText, errorTextSize, SC_SYSTEM_IMPORT_NONE);
    return false;
  }

  size_t length = 0U;
  if (!scReadSystemFile(bestPath, tpSignedCalJson, sizeof(tpSignedCalJson), &length))
  {
    scSetText(errorText, errorTextSize, SC_SYSTEM_IMPORT_REREAD);
    return false;
  }

  const bool ok = tpSignedCalibrationImportSystemJson(
      tpSignedCalJson, length, errorText, errorTextSize);
  if (ok && importedPath != nullptr && importedPathSize > 0U)
    scSetText(importedPath, importedPathSize, bestPath);
  return ok;
}

bool FLASHMEM tpSignedCalibrationImportDeviceFromSd(char* importedPath,
                                                     size_t importedPathSize,
                                                     char* errorText,
                                                     size_t errorTextSize)
{
  if (!scRequireFirmwareHashAvailable(errorText, errorTextSize)) return false;
  const bool ok = scImportFromSd(true, importedPath, importedPathSize,
                                 errorText, errorTextSize);
  if (ok) (void)scSelectBestSystemForCurrentHead();
  return ok;
}

bool FLASHMEM tpSignedCalibrationImportHeadFromSd(char* importedPath,
                                                   size_t importedPathSize,
                                                   char* errorText,
                                                   size_t errorTextSize)
{
  if (!scRequireFirmwareHashAvailable(errorText, errorTextSize)) return false;
  const bool ok = scImportFromSd(false, importedPath, importedPathSize,
                                 errorText, errorTextSize);
  if (ok) (void)scSelectBestSystemForCurrentHead();
  return ok;
}

// ---------------------------------------------------------------------------
// Native TP3C1 V0.4 export for the local TFT QR display
// ---------------------------------------------------------------------------

namespace
{
static constexpr size_t TP_CAL_QR_ENVELOPE_CAPACITY = 8192U;
static constexpr size_t TP_CAL_QR_COMPRESSED_CAPACITY = 8320U;
static constexpr size_t TP_CAL_QR_VALUES_CAPACITY = 768U;
static constexpr size_t TP_CAL_QR_APPROVAL_CAPACITY = 4096U;
static constexpr size_t TP_CAL_QR_COMMON_CAPACITY = 6144U;
static constexpr size_t TP_CAL_QR_BASE_WORK_CAPACITY = 4096U;
static const char TP_CAL_QR_BASE38_ALPHABET[] TP_SIGNED_CAL_RODATA =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.";

struct ScQrPackageMeta
{
  char deviceSerial[8];
  char deviceKeyId[20];
  char deviceCertificateSerial[40];
  char firmwareVersion[40];
  char firmwareBuildId[48];
  uint8_t sourceRequestHash[32];
  char sourceRequestId[33];
  int64_t sourceRequestCreatedUtc;
  int64_t sourceRequestExpiresUtc;
  int64_t approvedUtc;
  char signerType[32];
  char signerKeyId[20];
  uint8_t signerCertificateManifest[32];
  uint8_t manifestHash[32];
  uint8_t signature[64];
  bool rootSigner;
};

// The TP3C1 builder runs synchronously in the main loop. It therefore borrows
// buffers that already exist for calibration and web JSON processing instead
// of permanently consuming another ~50 KiB of RAM2. No Ethernet request can be
// processed concurrently while this function is active.
static uint8_t* FLASHMEM scQrEnvelopeBuffer()
{
  return reinterpret_cast<uint8_t*>(ethJsonBuf);
}

static uint8_t* FLASHMEM scQrCompressedBuffer()
{
  return reinterpret_cast<uint8_t*>(tpSignedCalObjectA);
}

static uint8_t* FLASHMEM scQrValuesBuffer()
{
  return tpSignedCalCanonical;
}

static uint8_t* FLASHMEM scQrApprovalBuffer()
{
  return reinterpret_cast<uint8_t*>(tpSignedCalObjectB);
}

static uint8_t* FLASHMEM scQrCommonBuffer()
{
  return reinterpret_cast<uint8_t*>(tpSignedCalObjectA);
}

static uint8_t* FLASHMEM scQrResultsWorkBuffer()
{
  // Common output occupies at most the first 6144 bytes of ObjectA.
  return reinterpret_cast<uint8_t*>(tpSignedCalObjectA) + 7168U;
}

static uint8_t* FLASHMEM scQrTransportWorkBuffer()
{
  // After the envelope is complete, ObjectB is no longer needed for approval
  // fields and can safely hold the 18-byte transport header plus one fragment.
  return reinterpret_cast<uint8_t*>(tpSignedCalObjectB);
}

static ScQrPackageMeta tpCalQrMeta;
static char tpCalQrSystemRequestId[33];
static uint8_t tpCalQrSystemRequestHash[32];
static TpSystemFirmwareContextStatus tpCalQrFirmwareContextStatus;
static TpRoleCertificateParsed tpCalQrSelectedRole;

class ScQrWriter
{
public:
  ScQrWriter(uint8_t* data, size_t capacity)
    : data_(data), capacity_(capacity), length_(0U), valid_(data != nullptr)
  {
    if (data_ != nullptr && capacity_ > 0U) memset(data_, 0, capacity_);
  }

  bool valid() const { return valid_; }
  size_t length() const { return length_; }
  const uint8_t* data() const { return data_; }

  bool raw(const uint8_t* value, size_t count)
  {
    if (!valid_ || (value == nullptr && count > 0U) || count > capacity_ - length_)
    {
      valid_ = false;
      return false;
    }
    if (count > 0U) memcpy(data_ + length_, value, count);
    length_ += count;
    return true;
  }

  bool varUInt(uint64_t value)
  {
    uint8_t bytes[10];
    size_t count = 0U;
    do
    {
      uint8_t current = static_cast<uint8_t>(value & 0x7FU);
      value >>= 7U;
      if (value != 0U) current |= 0x80U;
      bytes[count++] = current;
    } while (value != 0U && count < sizeof(bytes));
    return raw(bytes, count);
  }

  bool fieldUInt(uint8_t field, uint64_t value)
  {
    return varUInt(static_cast<uint64_t>(field) * 8ULL) && varUInt(value);
  }

  bool fieldBytes(uint8_t field, const uint8_t* value, size_t count)
  {
    return varUInt(static_cast<uint64_t>(field) * 8ULL + 2ULL) &&
           varUInt(count) && raw(value, count);
  }

  bool fieldText(uint8_t field, const char* text)
  {
    if (text == nullptr) text = "";
    return fieldBytes(field, reinterpret_cast<const uint8_t*>(text), strlen(text));
  }

  bool packedSigned(const int64_t* values, size_t count)
  {
    if (values == nullptr && count > 0U) { valid_ = false; return false; }
    for (size_t i = 0U; i < count; ++i)
    {
      const int64_t value = values[i];
      const uint64_t zigzag = value >= 0
          ? static_cast<uint64_t>(value) * 2ULL
          : static_cast<uint64_t>(-(value + 1LL)) * 2ULL + 1ULL;
      if (!varUInt(zigzag)) return false;
    }
    return true;
  }

private:
  uint8_t* data_;
  size_t capacity_;
  size_t length_;
  bool valid_;
};

enum ScQrPackageKind : uint8_t
{
  SC_QR_DEVICE = 0U,
  SC_QR_HEAD = 1U,
  SC_QR_SYSTEM = 2U
};

static bool FLASHMEM scQrReadAndParsePackage(ScQrPackageKind kind,
                                             ScQrPackageMeta& meta,
                                             char* errorText,
                                             size_t errorTextSize)
{
  memset(&meta, 0, sizeof(meta));
  size_t jsonLength = 0U;
  bool readOk = false;
  if (kind == SC_QR_SYSTEM)
    readOk = scReadSystemFile(TP_CAL_ACTIVE_SYSTEM, tpSignedCalJson,
                              sizeof(tpSignedCalJson), &jsonLength);
  else
    readOk = scReadFile(kind == SC_QR_DEVICE ? TP_CAL_ACTIVE_DEVICE : TP_CAL_ACTIVE_HEAD,
                        tpSignedCalJson, sizeof(tpSignedCalJson), &jsonLength);
  if (!readOk || jsonLength == 0U)
  {
    scSetText(errorText, errorTextSize, "Aktive Kalibrierdatei konnte nicht gelesen werden");
    return false;
  }

  char format[48] = {0};
  if (!scJsonGetString(tpSignedCalJson, tpSignedCalText.t043, format, sizeof(format)))
  {
    scSetText(errorText, errorTextSize, "Kalibrierformat fehlt");
    return false;
  }

  const char* expectedFormat = nullptr;
  // scParseCommonPackageV2() erwartet hier nicht die Dateiformat-Version,
  // sondern die Version des eingebetteten Approval-Schemas. Die aktuellen
  // Geräte-/Kopfpakete verwenden Approval V3, das Systempaket V2 dagegen
  // Approval V4 (mit CertificateSource).
  uint8_t approvalVersion = 0U;
  bool signatureOk = false;
  if (kind == SC_QR_DEVICE)
  {
    expectedFormat = TP_CAL_FORMAT_DEVICE_PACKAGE;
    approvalVersion = 3U;
    (void)scVerifyDevicePackage(tpSignedCalJson,
                                tpDeviceCalStatus,
                                tpDeviceCalSignedValues);
    signatureOk = tpDeviceCalStatus.signatureValid;
  }
  else if (kind == SC_QR_HEAD)
  {
    expectedFormat = TP_CAL_FORMAT_HEAD_PACKAGE;
    approvalVersion = 3U;
    (void)scVerifyHeadPackage(tpSignedCalJson,
                              tpHeadCalStatus,
                              tpHeadCalSignedValues);
    signatureOk = tpHeadCalStatus.signatureValid;
  }
  else
  {
    expectedFormat = TP_CAL_FORMAT_SYSTEM_PACKAGE;
    approvalVersion = 4U;

    // Der TFT-QR ist ein historischer Nachweisexport. Bei dieser erneuten
    // Prüfung darf deshalb ausschließlich die kryptografische Signatur des
    // Systempakets bewertet werden. Die normale Systemprüfung kontrolliert
    // zusätzlich die aktuelle Geräte-/Kopfbindung und kann dadurch einen
    // weiterhin gültig signierten Kalibrierschein als momentan nicht
    // anwendbar zurückmelden. Dieser Laufzeitzustand gehört in den
    // Firmware-/Anwendbarkeitshinweis, darf den QR-Export aber nicht sperren.
    const bool previousMode = tpSystemVerificationSignatureOnly;
    tpSystemVerificationSignatureOnly = true;
    (void)scVerifySystemPackage(tpSignedCalJson,
                                tpSystemSelectionStatus,
                                tpSystemSelectionValues);
    tpSystemVerificationSignatureOnly = previousMode;
    signatureOk = tpSystemSelectionStatus.signatureValid;
    if (signatureOk) tpSystemCalSignedValues = tpSystemSelectionValues;
  }
  if (strcmp(format, expectedFormat) != 0)
  {
    scSetText(errorText, errorTextSize,
              "TP3C1 V0.4 benötigt aktuelle V3/V2-Kalibrierpakete");
    return false;
  }
  if (!signatureOk)
  {
    const char* detail = kind == SC_QR_SYSTEM
        ? tpSystemSelectionStatus.lastError
        : (kind == SC_QR_DEVICE ? tpDeviceCalStatus.lastError
                                : tpHeadCalStatus.lastError);
    scSetText(errorText, errorTextSize,
              detail != nullptr && detail[0] != '\0' ? detail
                                                     : "Kalibriersignatur ist ungültig");
    return false;
  }

  char parseError[160] = {0};
  if (!scParseCommonPackageV2(tpSignedCalJson,
                              expectedFormat,
                              approvalVersion,
                              meta.deviceSerial, sizeof(meta.deviceSerial),
                              meta.deviceKeyId, sizeof(meta.deviceKeyId),
                              meta.deviceCertificateSerial, sizeof(meta.deviceCertificateSerial),
                              meta.firmwareVersion, sizeof(meta.firmwareVersion),
                              meta.firmwareBuildId, sizeof(meta.firmwareBuildId),
                              meta.sourceRequestHash,
                              meta.sourceRequestId,
                              &meta.sourceRequestCreatedUtc,
                              &meta.sourceRequestExpiresUtc,
                              tpSignedCalApproval,
                              &meta.approvedUtc,
                              meta.signerType, sizeof(meta.signerType),
                              meta.signerKeyId, sizeof(meta.signerKeyId),
                              meta.signerCertificateManifest,
                              meta.manifestHash,
                              meta.signature,
                              tpSignedCalRole,
                              &meta.rootSigner,
                              parseError, sizeof(parseError)))
  {
    scSetText(errorText, errorTextSize,
              parseError[0] != '\0' ? parseError : "Kalibrierpaket konnte nicht gelesen werden");
    return false;
  }
  return true;
}

static bool FLASHMEM scQrSelectRole(char* firmwareVersion,
                                    size_t firmwareVersionSize,
                                    char* errorText,
                                    size_t errorTextSize)
{
  bool rolePresent = false;
  char commonVersion[40] = {0};
  memset(&tpCalQrSelectedRole, 0, sizeof(tpCalQrSelectedRole));
  memset(tpCalQrSystemRequestId, 0, sizeof(tpCalQrSystemRequestId));
  memset(tpCalQrSystemRequestHash, 0, sizeof(tpCalQrSystemRequestHash));
  for (uint8_t i = 0U; i < 3U; ++i)
  {
    if (!scQrReadAndParsePackage(static_cast<ScQrPackageKind>(i),
                                 tpCalQrMeta, errorText, errorTextSize)) return false;
    if (i == 0U) scSetText(commonVersion, sizeof(commonVersion), tpCalQrMeta.firmwareVersion);
    else if (strcmp(commonVersion, tpCalQrMeta.firmwareVersion) != 0)
    {
      scSetText(errorText, errorTextSize,
                "Firmware-SemVer der Kalibrierpakete ist nicht gemeinsam");
      return false;
    }

    if (i == 2U)
    {
      scSetText(tpCalQrSystemRequestId, sizeof(tpCalQrSystemRequestId),
                tpCalQrMeta.sourceRequestId);
      memcpy(tpCalQrSystemRequestHash, tpCalQrMeta.sourceRequestHash,
             sizeof(tpCalQrSystemRequestHash));
    }

    if (strcmp(tpCalQrMeta.signerType, TP_CAL_SIGNER_LAB) == 0)
    {
      if (!rolePresent)
      {
        memcpy(&tpCalQrSelectedRole, &tpSignedCalRole, sizeof(tpCalQrSelectedRole));
        rolePresent = true;
      }
      else if (!scBytesEqual(tpCalQrSelectedRole.manifestSha256,
                             tpSignedCalRole.manifestSha256, 32U))
      {
        scSetText(errorText, errorTextSize,
                  "Mehrere Laborzertifikate können nicht gemeinsam angezeigt werden");
        return false;
      }
    }
  }
  if (!rolePresent) memset(&tpCalQrSelectedRole, 0, sizeof(tpCalQrSelectedRole));
  scSetText(firmwareVersion, firmwareVersionSize, commonVersion);
  return true;
}

static bool FLASHMEM scQrDecodeBase64Url(const char* text,
                                         uint8_t* output,
                                         size_t outputCapacity,
                                         size_t* outputLength)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (text == nullptr || output == nullptr) return false;
  uint32_t accumulator = 0U;
  uint8_t bits = 0U;
  size_t written = 0U;
  bool paddingSeen = false;
  for (const char* p = text; *p != '\0'; ++p)
  {
    const char c = *p;
    if (c == '=') { paddingSeen = true; continue; }
    if (paddingSeen) return false;
    int value = -1;
    if (c >= 'A' && c <= 'Z') value = c - 'A';
    else if (c >= 'a' && c <= 'z') value = c - 'a' + 26;
    else if (c >= '0' && c <= '9') value = c - '0' + 52;
    else if (c == '+' || c == '-') value = 62;
    else if (c == '/' || c == '_') value = 63;
    else return false;
    accumulator = (accumulator << 6U) | static_cast<uint32_t>(value);
    bits = static_cast<uint8_t>(bits + 6U);
    if (bits >= 8U)
    {
      bits = static_cast<uint8_t>(bits - 8U);
      if (written >= outputCapacity) return false;
      output[written++] = static_cast<uint8_t>((accumulator >> bits) & 0xFFU);
    }
  }
  if (bits != 0U && (accumulator & ((1U << bits) - 1U)) != 0U) return false;
  if (outputLength != nullptr) *outputLength = written;
  return true;
}

static uint8_t FLASHMEM scQrScopeCode(const TpCalApproval& approval)
{
  if (strcmp(approval.calibrationScope, "NONE") == 0) return 0U;
  if (strcmp(approval.calibrationScope, "AS_FOUND") == 0) return 1U;
  if (strcmp(approval.calibrationScope, "AS_LEFT") == 0) return 2U;
  if (strcmp(approval.calibrationScope, "AS_FOUND_AS_LEFT_NO_ADJUSTMENT") == 0) return 4U;
  if (strcmp(approval.calibrationScope, "BEFORE_AFTER_ADJUSTMENT") == 0) return 5U;
  return 0xFFU;
}

static bool FLASHMEM scQrBuildApproval(ScQrPackageKind kind,
                                       ScQrWriter& writer,
                                       char* errorText,
                                       size_t errorTextSize)
{
  const TpCalApproval& a = tpSignedCalApproval;
  const bool rolePresent = tpCalQrSelectedRole.keyId[0] != '\0';
  uint8_t laboratoryMode = 2U;
  if (rolePresent && strcmp(a.laboratory, tpCalQrSelectedRole.labName) == 0)
    laboratoryMode = 0U;
  else if (strcmp(a.laboratory, "TP-3000 Manufacturer Root") == 0)
    laboratoryMode = 1U;
  if (!writer.fieldUInt(1U, laboratoryMode)) return false;
  if (laboratoryMode == 2U && !writer.fieldText(2U, a.laboratory)) return false;

  uint8_t operatorMode = 2U;
  if (rolePresent && strcmp(a.operatorName, tpCalQrSelectedRole.labName) == 0)
    operatorMode = 0U;
  else if (a.operatorName[0] == '\0')
    operatorMode = 1U;
  if (!writer.fieldUInt(3U, operatorMode)) return false;
  if (operatorMode == 2U && !writer.fieldText(4U, a.operatorName)) return false;
  if (a.certificateReference[0] != '\0' && !writer.fieldText(5U, a.certificateReference)) return false;

  const uint8_t reasonCode = strcmp(a.reason, "Regelkalibrierung") == 0 ? 1U : 0U;
  if (!writer.fieldUInt(6U, reasonCode)) return false;
  if (reasonCode == 0U && a.reason[0] != '\0' && !writer.fieldText(7U, a.reason)) return false;

  const char* fields[6] =
  {
    a.referenceStandards,
    a.traceability,
    a.measurementUncertainty,
    a.environmentalConditions,
    a.calibrationProcedure,
    a.accreditationInformation
  };
  for (uint8_t i = 0U; i < 6U; ++i)
    if (fields[i][0] != '\0' && !writer.fieldText(static_cast<uint8_t>(8U + i), fields[i])) return false;

  const uint8_t scope = scQrScopeCode(a);
  if (scope == 0xFFU)
  {
    scSetText(errorText, errorTextSize, "Kalibrierumfang wird von TP3C1 nicht unterstützt");
    return false;
  }
  if (!writer.fieldUInt(14U, scope)) return false;
  if (scope != 0U)
  {
    if (strcmp(a.calibrationResultsEncoding, TP_CAL_RESULTS_ENCODING) != 0)
    {
      scSetText(errorText, errorTextSize, "Kalibriermesswertformat ist ungültig");
      return false;
    }
    size_t rawLength = 0U;
    if (!scQrDecodeBase64Url(a.calibrationResultsBase64Url,
                             scQrResultsWorkBuffer(), TP_CAL_QR_BASE_WORK_CAPACITY, &rawLength) ||
        rawLength < 8U || (rawLength - 8U) % 24U != 0U ||
        scQrResultsWorkBuffer()[0] != 'T' || scQrResultsWorkBuffer()[1] != 'P' ||
        scQrResultsWorkBuffer()[2] != 'C' || scQrResultsWorkBuffer()[3] != '1' ||
        scQrResultsWorkBuffer()[4] != scope || scQrResultsWorkBuffer()[5] < 1U ||
        scQrResultsWorkBuffer()[5] > 40U || rawLength != 8U + static_cast<size_t>(scQrResultsWorkBuffer()[5]) * 24U ||
        scQrResultsWorkBuffer()[6] != 0U || scQrResultsWorkBuffer()[7] != 0U)
    {
      scSetText(errorText, errorTextSize, "Kalibriermesswerte sind ungültig");
      return false;
    }
    if (!writer.fieldBytes(15U, scQrResultsWorkBuffer(), rawLength)) return false;
  }

  if (kind == SC_QR_SYSTEM)
  {
    uint8_t source = 0U;
    if (strcmp(a.certificateSource, TP_CAL_CERT_SOURCE_EXTERNAL) == 0) source = 1U;
    else if (strcmp(a.certificateSource, TP_CAL_CERT_SOURCE_TP3000) != 0)
    {
      scSetText(errorText, errorTextSize, "Kalibrierscheinquelle ist ungültig");
      return false;
    }
    if (!writer.fieldUInt(16U, source)) return false;
  }
  return writer.valid();
}

static bool FLASHMEM scQrStandardCalibrationId(const ScQrPackageMeta& meta,
                                               ScQrPackageKind kind,
                                               char* output,
                                               size_t outputSize)
{
  char requestPrefix[9] = {0};
  memcpy(requestPrefix, meta.sourceRequestId, 8U);
  for (uint8_t i = 0U; i < 8U; ++i)
    requestPrefix[i] = static_cast<char>(toupper(static_cast<unsigned char>(requestPrefix[i])));
  int length = 0;
  if (kind == SC_QR_DEVICE)
    length = snprintf(output, outputSize, "DCAL-G%s-%08lu-%s",
                      meta.deviceSerial,
                      static_cast<unsigned long>(tpSignedCalApproval.calibrationDateYmd),
                      requestPrefix);
  else if (kind == SC_QR_HEAD)
    length = snprintf(output, outputSize, "HCAL-K%05lu-%08lu-%s",
                      static_cast<unsigned long>(tpHeadCalSignedValues.headSerial),
                      static_cast<unsigned long>(tpSignedCalApproval.calibrationDateYmd),
                      requestPrefix);
  else
    length = snprintf(output, outputSize, "SCAL-G%s-K%05lu-%08lu-%s",
                      meta.deviceSerial,
                      static_cast<unsigned long>(tpSystemCalSignedValues.headSerial),
                      static_cast<unsigned long>(tpSignedCalApproval.calibrationDateYmd),
                      requestPrefix);
  return length > 0 && static_cast<size_t>(length) < outputSize;
}

static bool FLASHMEM scQrBuildNumber(const char* buildId, uint32_t* number)
{
  if (number != nullptr) *number = 0U;
  if (buildId == nullptr) return false;
  const char* underscore = strrchr(buildId, '_');
  if (underscore == nullptr || underscore[1] == '\0') return false;
  char* end = nullptr;
  const unsigned long value = strtoul(underscore + 1, &end, 10);
  if (end == underscore + 1 || *end != '\0' || value > UINT32_MAX) return false;
  if (number != nullptr) *number = static_cast<uint32_t>(value);
  return true;
}

static bool FLASHMEM scQrBuildValues(ScQrPackageKind kind,
                                     ScQrWriter& writer,
                                     char* errorText,
                                     size_t errorTextSize)
{
  if (kind == SC_QR_DEVICE)
  {
    const int64_t packed[6] =
    {
      tpDeviceCalSignedValues.refLowScaled,
      tpDeviceCalSignedValues.refHighScaled,
      tpDeviceCalSignedValues.chALowScaled,
      tpDeviceCalSignedValues.chAHighScaled,
      tpDeviceCalSignedValues.chBLowScaled,
      tpDeviceCalSignedValues.chBHighScaled
    };
    ScQrWriter packedWriter(scQrApprovalBuffer(), TP_CAL_QR_APPROVAL_CAPACITY);
    if (!packedWriter.packedSigned(packed, 6U) ||
        !writer.fieldUInt(1U, 1U) ||
        !writer.fieldUInt(2U, tpDeviceCalSignedValues.referenceDateYmd) ||
        !writer.fieldBytes(3U, packedWriter.data(), packedWriter.length())) return false;
    return true;
  }

  if (kind == SC_QR_HEAD)
  {
    const int64_t packed[19] =
    {
      tpHeadCalSignedValues.r0MirrorScaled,
      tpHeadCalSignedValues.r0AmbientScaled,
      tpHeadCalSignedValues.mirrorSet1_mC,
      tpHeadCalSignedValues.mirrorActual1_mC,
      tpHeadCalSignedValues.mirrorSet2_mC,
      tpHeadCalSignedValues.mirrorActual2_mC,
      tpHeadCalSignedValues.ambientSet1_mC,
      tpHeadCalSignedValues.ambientActual1_mC,
      tpHeadCalSignedValues.ambientSet2_mC,
      tpHeadCalSignedValues.ambientActual2_mC,
      tpHeadCalSignedValues.dewFrostOffset_mC,
      tpHeadCalSignedValues.pidKp,
      tpHeadCalSignedValues.pidKi_x1000,
      tpHeadCalSignedValues.pidKd_x1000,
      tpHeadCalSignedValues.controlInterval_ms,
      tpHeadCalSignedValues.hBridgeDeadtime_ms,
      tpHeadCalSignedValues.fanPercent,
      tpHeadCalSignedValues.opticalTarget_x10,
      tpHeadCalSignedValues.peltierCurrentLimit_mA
    };
    ScQrWriter packedWriter(scQrApprovalBuffer(), TP_CAL_QR_APPROVAL_CAPACITY);
    const uint8_t flags = (tpHeadCalSignedValues.mirror2PointActive ? 1U : 0U) |
                          (tpHeadCalSignedValues.ambient2PointActive ? 2U : 0U);
    const uint8_t typeCode = strcmp(tpHeadCalSignedValues.headType, "STP-3001") == 0 ? 1U : 0U;
    if (!packedWriter.packedSigned(packed, 19U) ||
        !writer.fieldUInt(1U, 1U) ||
        !writer.fieldUInt(2U, typeCode) ||
        !writer.fieldUInt(3U, tpHeadCalSignedValues.headSerial) ||
        !writer.fieldUInt(4U, tpHeadCalSignedValues.calibrationTimeHms) ||
        !writer.fieldUInt(5U, flags) ||
        !writer.fieldBytes(6U, packedWriter.data(), packedWriter.length())) return false;
    if (typeCode == 0U && !writer.fieldText(7U, tpHeadCalSignedValues.headType)) return false;
    return true;
  }

  if (!writer.fieldUInt(1U, 2U) ||
      !writer.fieldUInt(2U, tpSystemCalSignedValues.externalCertificateAvailable ? 1U : 0U)) return false;
  if (tpSystemCalSignedValues.externalCertificateAvailable)
  {
    if (!writer.fieldText(3U, tpSystemCalSignedValues.externalCertificateDocumentId) ||
        !writer.fieldText(4U, tpSystemCalSignedValues.externalCertificateNumber) ||
        !writer.fieldUInt(5U, tpSystemCalSignedValues.externalCertificateValidFromYmd) ||
        !writer.fieldUInt(6U, tpSystemCalSignedValues.externalCertificateValidUntilYmd) ||
        !writer.fieldText(7U, tpSystemCalSignedValues.externalCertificateOriginalFileName) ||
        !writer.fieldUInt(8U, tpSystemCalSignedValues.externalCertificateFileSize) ||
        !writer.fieldBytes(9U, tpSystemCalSignedValues.externalCertificateSha256, 32U)) return false;
  }
  (void)errorText;
  (void)errorTextSize;
  return true;
}

static bool FLASHMEM scQrBuildCommon(ScQrPackageKind kind,
                                     ScQrWriter& common,
                                     char* errorText,
                                     size_t errorTextSize)
{
  ScQrWriter values(scQrValuesBuffer(), TP_CAL_QR_VALUES_CAPACITY);
  if (!scQrBuildValues(kind, values, errorText, errorTextSize) || !values.valid())
  {
    if (errorText != nullptr && errorText[0] == '\0')
      scSetText(errorText, errorTextSize, "Kalibrierwerte sind zu groß");
    return false;
  }

  ScQrWriter approval(scQrApprovalBuffer(), TP_CAL_QR_APPROVAL_CAPACITY);
  if (!scQrBuildApproval(kind, approval, errorText, errorTextSize) || !approval.valid())
  {
    if (errorText != nullptr && errorText[0] == '\0')
      scSetText(errorText, errorTextSize, "Kalibrierscheinangaben sind zu groß");
    return false;
  }

  uint32_t buildNumber = 0U;
  if (!scQrBuildNumber(tpCalQrMeta.firmwareBuildId, &buildNumber))
  {
    scSetText(errorText, errorTextSize, "Firmware-Build-ID ist ungültig");
    return false;
  }

  char standardId[128] = {0};
  if (!scQrStandardCalibrationId(tpCalQrMeta, kind, standardId, sizeof(standardId)))
  {
    scSetText(errorText, errorTextSize, "Kalibrierungs-ID konnte nicht gebildet werden");
    return false;
  }

  if (!common.fieldUInt(1U, kind == SC_QR_DEVICE
                               ? tpDeviceCalSignedValues.calibrationDateYmd
                               : (kind == SC_QR_HEAD ? tpHeadCalSignedValues.calibrationDateYmd
                                                     : tpSystemCalSignedValues.calibrationDateYmd)) ||
      !common.fieldBytes(2U, tpCalQrMeta.sourceRequestHash, 32U)) return false;
  uint8_t requestId[16];
  if (!scHexToBytes(tpCalQrMeta.sourceRequestId, requestId, sizeof(requestId)))
  {
    scSetText(errorText, errorTextSize, "Kalibrieranfrage-ID ist ungültig");
    return false;
  }
  if (!common.fieldBytes(3U, requestId, sizeof(requestId)) ||
      !common.fieldUInt(4U, static_cast<uint64_t>(tpCalQrMeta.sourceRequestCreatedUtc)) ||
      !common.fieldUInt(5U, static_cast<uint64_t>(tpSignedCalApproval.validFromUtc)) ||
      !common.fieldUInt(6U, static_cast<uint64_t>(tpSignedCalApproval.validUntilUtc)) ||
      !common.fieldUInt(7U, static_cast<uint64_t>(tpCalQrMeta.approvedUtc)) ||
      !common.fieldUInt(8U, tpSignedCalApproval.intervalMonths) ||
      !common.fieldUInt(9U, buildNumber) ||
      !common.fieldBytes(10U, approval.data(), approval.length()) ||
      !common.fieldBytes(11U, tpCalQrMeta.signature, 64U)) return false;
  if (strcmp(tpSignedCalApproval.calibrationId, standardId) != 0 &&
      !common.fieldText(12U, tpSignedCalApproval.calibrationId)) return false;
  if (!common.fieldUInt(13U,
                        strcmp(tpCalQrMeta.signerType, TP_CAL_SIGNER_LAB) == 0 ? 0U : 1U) ||
      !common.fieldBytes(14U, values.data(), values.length())) return false;
  return common.valid();
}

static bool FLASHMEM scQrBuildIdentity(ScQrWriter& writer,
                                       char* errorText,
                                       size_t errorTextSize)
{
  size_t certificateLength = 0U;
  if (!deviceIdentityBuildCertificateJson(tpSignedCalObjectA,
                                          sizeof(tpSignedCalObjectA),
                                          &certificateLength) || certificateLength == 0U)
  {
    scSetText(errorText, errorTextSize, "Aktives Gerätezertifikat konnte nicht gelesen werden");
    return false;
  }
  char serialText[8] = {0};
  char spkiBase64[160] = {0};
  char certificateSerial[40] = {0};
  char issuedText[48] = {0};
  char requestIdText[40] = {0};
  char requestHashText[72] = {0};
  char signatureBase64[120] = {0};
  if (!scJsonGetString(tpSignedCalObjectA, "DeviceSerial", serialText, sizeof(serialText)) ||
      !scJsonGetString(tpSignedCalObjectA, "DevicePublicKeySpkiBase64", spkiBase64, sizeof(spkiBase64)) ||
      !scJsonGetString(tpSignedCalObjectA, "CertificateSerial", certificateSerial, sizeof(certificateSerial)) ||
      !scJsonGetString(tpSignedCalObjectA, "IssuedUtc", issuedText, sizeof(issuedText)) ||
      !scJsonGetString(tpSignedCalObjectA, "RequestId", requestIdText, sizeof(requestIdText)) ||
      !scJsonGetString(tpSignedCalObjectA, "RequestManifestSha256", requestHashText, sizeof(requestHashText)) ||
      !scJsonGetString(tpSignedCalObjectA, "RootSignatureBase64", signatureBase64, sizeof(signatureBase64)))
  {
    scSetText(errorText, errorTextSize, "Gerätezertifikat ist unvollständig");
    return false;
  }
  char* serialEnd = nullptr;
  const unsigned long serial = strtoul(serialText, &serialEnd, 10);
  int64_t issuedUtc = 0;
  uint8_t spki[91];
  size_t spkiLength = 0U;
  uint8_t certSerialBytes[16];
  uint8_t requestId[16];
  uint8_t requestHash[32];
  uint8_t rootSignature[64];
  size_t signatureLength = 0U;
  if (serialEnd == serialText || *serialEnd != '\0' ||
      !scParseIsoUtc(issuedText, &issuedUtc) ||
      !scBase64Decode(spkiBase64, spki, sizeof(spki), &spkiLength) || spkiLength != 91U ||
      spki[26] != 0x04U ||
      !scHexToBytes(certificateSerial, certSerialBytes, sizeof(certSerialBytes)) ||
      !scHexToBytes(requestIdText, requestId, sizeof(requestId)) ||
      !scHexToBytes(requestHashText, requestHash, sizeof(requestHash)) ||
      !scBase64Decode(signatureBase64, rootSignature, sizeof(rootSignature), &signatureLength) ||
      signatureLength != 64U)
  {
    scSetText(errorText, errorTextSize, "Gerätezertifikat ist ungültig");
    return false;
  }
  return writer.fieldUInt(1U, serial) &&
         writer.fieldBytes(2U, spki + 27U, 64U) &&
         writer.fieldBytes(3U, certSerialBytes, sizeof(certSerialBytes)) &&
         writer.fieldUInt(4U, static_cast<uint64_t>(issuedUtc)) &&
         writer.fieldBytes(5U, requestId, sizeof(requestId)) &&
         writer.fieldBytes(6U, requestHash, sizeof(requestHash)) &&
         writer.fieldBytes(7U, rootSignature, sizeof(rootSignature));
}

static bool FLASHMEM scQrBuildRole(ScQrWriter& writer)
{
  if (tpCalQrSelectedRole.keyId[0] == '\0') return true;
  uint8_t certificateSerial[16];
  if (!scHexToBytes(tpCalQrSelectedRole.certificateSerial,
                    certificateSerial, sizeof(certificateSerial))) return false;
  if (!writer.fieldBytes(1U, tpCalQrSelectedRole.publicRaw, 64U) ||
      !writer.fieldBytes(2U, certificateSerial, sizeof(certificateSerial)) ||
      !writer.fieldUInt(3U, static_cast<uint64_t>(tpCalQrSelectedRole.issuedUtc)) ||
      !writer.fieldUInt(4U, static_cast<uint64_t>(tpCalQrSelectedRole.notBeforeUtc)) ||
      !writer.fieldUInt(5U, static_cast<uint64_t>(tpCalQrSelectedRole.notAfterUtc)) ||
      !writer.fieldText(6U, tpCalQrSelectedRole.labId) ||
      !writer.fieldText(7U, tpCalQrSelectedRole.labName)) return false;
  if (tpCalQrSelectedRole.labAddress[0] != '\0' &&
      !writer.fieldText(8U, tpCalQrSelectedRole.labAddress)) return false;
  if (!writer.fieldBytes(9U, tpCalQrSelectedRole.rootSignature, 64U)) return false;
  char defaultKeyName[260];
  snprintf(defaultKeyName, sizeof(defaultKeyName), "%s / %s",
           tpCalQrSelectedRole.labId, tpCalQrSelectedRole.labName);
  if (strcmp(tpCalQrSelectedRole.keyName, defaultKeyName) != 0 &&
      !writer.fieldText(10U, tpCalQrSelectedRole.keyName)) return false;
  if (strcmp(tpCalQrSelectedRole.permissions, TP_CAL_PERMISSION_SIGN) != 0 &&
      !writer.fieldText(11U, tpCalQrSelectedRole.permissions)) return false;
  return writer.valid();
}

static bool FLASHMEM scQrParseSemver(const char* text, uint32_t* packed)
{
  if (packed != nullptr) *packed = 0U;
  if (text == nullptr) return false;
  char* end = nullptr;
  const unsigned long major = strtoul(text, &end, 10);
  if (end == text || *end != '.') return false;
  const char* minorStart = end + 1;
  const unsigned long minor = strtoul(minorStart, &end, 10);
  if (end == minorStart || *end != '.') return false;
  const char* patchStart = end + 1;
  const unsigned long patch = strtoul(patchStart, &end, 10);
  if (end == patchStart || *end != '\0' || major > 1023UL || minor > 1023UL || patch > 1023UL)
    return false;
  if (packed != nullptr)
    *packed = static_cast<uint32_t>(major * 1048576UL + minor * 1024UL + patch);
  return true;
}

static bool FLASHMEM scQrBuildFirmwareContext(const char* sourceRequestId,
                                              const uint8_t sourceRequestHash[32],
                                              ScQrWriter& writer,
                                              TpSystemFirmwareContextStatus& context,
                                              char* errorText,
                                              size_t errorTextSize)
{
  char requestHash[65];
  scBytesToHex(sourceRequestHash, 32U, requestHash, sizeof(requestHash), false);
  memset(&context, 0, sizeof(context));
  if (!tpSystemFirmwareContextLoad(sourceRequestId, requestHash, context) ||
      !context.filePresent || !context.recordValid || !context.requestBindingValid)
    return true;  // Historical context is optional in TP3C1 V0.4.

  uint8_t firmwareHash[32];
  uint8_t requestId[16];
  uint8_t sourceHash[32];
  uint8_t contextManifest[32];
  uint8_t deviceSignature[64];
  if (!scHexToBytes(context.firmwareSha256, firmwareHash, sizeof(firmwareHash)) ||
      !scHexToBytes(context.sourceRequestId, requestId, sizeof(requestId)) ||
      !scHexToBytes(context.sourceRequestManifestSha256, sourceHash, sizeof(sourceHash)) ||
      !scHexToBytes(context.contextManifestSha256, contextManifest, sizeof(contextManifest)) ||
      !scHexToBytes(context.deviceSignatureHex, deviceSignature, sizeof(deviceSignature)))
  {
    scSetText(errorText, errorTextSize, "Firmwarekontext ist unvollständig");
    return false;
  }

  if (!writer.fieldUInt(1U, 1U) ||
      !writer.fieldUInt(2U, context.captureMode) ||
      !writer.fieldUInt(3U, static_cast<uint64_t>(context.capturedUtc)) ||
      !writer.fieldText(4U, context.firmwareVersion) ||
      !writer.fieldText(5U, context.firmwareBuildId) ||
      !writer.fieldUInt(6U, context.imageBase) ||
      !writer.fieldUInt(7U, context.imageSize) ||
      !writer.fieldBytes(8U, firmwareHash, sizeof(firmwareHash)) ||
      !writer.fieldUInt(9U, context.manufacturerStatus)) return false;
  if (context.firmwareCertificateId[0] != '\0' &&
      !writer.fieldText(10U, context.firmwareCertificateId)) return false;
  if (context.manufacturerRootKeyId[0] != '\0' &&
      !writer.fieldText(11U, context.manufacturerRootKeyId)) return false;
  if (context.certificateApprovedUtc > 0 &&
      !writer.fieldUInt(12U, static_cast<uint64_t>(context.certificateApprovedUtc))) return false;
  return writer.fieldBytes(13U, requestId, sizeof(requestId)) &&
         writer.fieldBytes(14U, sourceHash, sizeof(sourceHash)) &&
         writer.fieldBytes(15U, contextManifest, sizeof(contextManifest)) &&
         writer.fieldBytes(16U, deviceSignature, sizeof(deviceSignature));
}

static uint32_t FLASHMEM scQrCrc32(const uint8_t* data, size_t length)
{
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0U; i < length; ++i)
  {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit)
      crc = (crc >> 1U) ^ (0xEDB88320UL & static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1U))));
  }
  return crc ^ 0xFFFFFFFFUL;
}

static bool FLASHMEM scQrBase38(const uint8_t* binary,
                                size_t binaryLength,
                                char* output,
                                size_t outputSize)
{
  static const char prefix[] = "TP3C1:";
  if (binary == nullptr || output == nullptr || outputSize <= sizeof(prefix) ||
      binaryLength > TP_CAL_QR_BASE_WORK_CAPACITY) return false;
  memcpy(scQrTransportWorkBuffer(), binary, binaryLength);
  size_t leadingZeroes = 0U;
  while (leadingZeroes < binaryLength && scQrTransportWorkBuffer()[leadingZeroes] == 0U) ++leadingZeroes;
  memcpy(output, prefix, sizeof(prefix) - 1U);
  size_t outLength = sizeof(prefix) - 1U;
  for (size_t i = 0U; i < leadingZeroes; ++i)
  {
    if (outLength + 1U >= outputSize) return false;
    output[outLength++] = '0';
  }
  const size_t digitStart = outLength;
  size_t start = leadingZeroes;
  while (start < binaryLength)
  {
    uint16_t remainder = 0U;
    for (size_t i = start; i < binaryLength; ++i)
    {
      const uint16_t value = static_cast<uint16_t>(remainder * 256U + scQrTransportWorkBuffer()[i]);
      scQrTransportWorkBuffer()[i] = static_cast<uint8_t>(value / 38U);
      remainder = static_cast<uint16_t>(value % 38U);
    }
    if (outLength + 1U >= outputSize) return false;
    output[outLength++] = TP_CAL_QR_BASE38_ALPHABET[remainder];
    while (start < binaryLength && scQrTransportWorkBuffer()[start] == 0U) ++start;
  }
  if (digitStart == outLength)
  {
    if (outLength + 1U >= outputSize) return false;
    output[outLength++] = '0';
  }
  for (size_t a = digitStart, b = outLength - 1U; a < b; ++a, --b)
  {
    const char temp = output[a];
    output[a] = output[b];
    output[b] = temp;
  }
  output[outLength] = '\0';
  return true;
}

static bool FLASHMEM scQrBuildTransportPart(const uint8_t* transport,
                                            size_t transportLength,
                                            const uint8_t digest[32],
                                            uint8_t partIndex,
                                            size_t fragmentOffset,
                                            size_t fragmentLength,
                                            bool compressed,
                                            char* output,
                                            size_t outputSize)
{
  if (transport == nullptr || digest == nullptr || output == nullptr ||
      transportLength > 65535U || fragmentLength > 65535U ||
      fragmentOffset + fragmentLength > transportLength ||
      18U + fragmentLength > TP_CAL_QR_BASE_WORK_CAPACITY) return false;
  uint8_t* binary = scQrTransportWorkBuffer();
  binary[0] = compressed ? 1U : 0U;
  memcpy(binary + 1U, digest, 8U);
  binary[9] = static_cast<uint8_t>(0x40U + partIndex);
  binary[10] = static_cast<uint8_t>((transportLength >> 8U) & 0xFFU);
  binary[11] = static_cast<uint8_t>(transportLength & 0xFFU);
  binary[12] = static_cast<uint8_t>((fragmentLength >> 8U) & 0xFFU);
  binary[13] = static_cast<uint8_t>(fragmentLength & 0xFFU);
  const uint32_t crc = scQrCrc32(transport + fragmentOffset, fragmentLength);
  binary[14] = static_cast<uint8_t>((crc >> 24U) & 0xFFU);
  binary[15] = static_cast<uint8_t>((crc >> 16U) & 0xFFU);
  binary[16] = static_cast<uint8_t>((crc >> 8U) & 0xFFU);
  binary[17] = static_cast<uint8_t>(crc & 0xFFU);
  memcpy(binary + 18U, transport + fragmentOffset, fragmentLength);

  const size_t binaryLength = 18U + fragmentLength;
  return scQrBase38(binary, binaryLength, output, outputSize);
}
}

bool FLASHMEM tpSignedCalibrationBuildActiveTp3c1Parts(
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
    size_t errorTextSize)
{
  if (part1 != nullptr && part1Size > 0U) part1[0] = '\0';
  if (part2 != nullptr && part2Size > 0U) part2[0] = '\0';
  if (documentId != nullptr && documentIdSize > 0U) documentId[0] = '\0';
  if (envelopeSize != nullptr) *envelopeSize = 0U;
  if (transportSize != nullptr) *transportSize = 0U;
  if (compressedUsed != nullptr) *compressedUsed = false;
  if (errorText != nullptr && errorTextSize > 0U) errorText[0] = '\0';
  if (part1 == nullptr || part2 == nullptr || part1Size < 32U || part2Size < 32U ||
      documentId == nullptr || documentIdSize < 17U)
  {
    scSetText(errorText, errorTextSize, "QR-Ausgabepuffer ist ungültig");
    return false;
  }
  if (!deviceIdentityCertificateValid())
  {
    scSetText(errorText, errorTextSize, "Kein gültiges Gerätezertifikat aktiv");
    return false;
  }
  if (!tpSignedCalibrationLoaded) tpSignedCalibrationBegin();

  // Die TFT-QR-Anzeige ist ein Nachweisexport. Ihre Verfuegbarkeit richtet
  // sich deshalb nach den kryptografischen Signaturen der gespeicherten
  // Pakete, nicht nach der aktuellen Firmwareanwendbarkeit oder einem
  // abweichenden Laufzeitzustand. Die drei aktiven Dateien werden unten
  // ohnehin erneut vollstaendig gelesen und signaturgeprueft.
  char commonFirmwareVersion[40] = {0};
  if (!scQrSelectRole(commonFirmwareVersion, sizeof(commonFirmwareVersion),
                      errorText, errorTextSize)) return false;
  uint32_t packedSemver = 0U;
  if (!scQrParseSemver(commonFirmwareVersion, &packedSemver))
  {
    scSetText(errorText, errorTextSize, "Firmware-SemVer ist ungültig");
    return false;
  }

  char systemRequestHash[65];
  scBytesToHex(tpCalQrSystemRequestHash, 32U,
               systemRequestHash, sizeof(systemRequestHash), false);
  memset(&tpCalQrFirmwareContextStatus, 0, sizeof(tpCalQrFirmwareContextStatus));
  const bool firmwareContextLoaded = tpSystemFirmwareContextLoad(
      tpCalQrSystemRequestId, systemRequestHash, tpCalQrFirmwareContextStatus);
  const bool hasFirmwareContext = firmwareContextLoaded &&
      tpCalQrFirmwareContextStatus.filePresent &&
      tpCalQrFirmwareContextStatus.recordValid &&
      tpCalQrFirmwareContextStatus.requestBindingValid;

  ScQrWriter envelope(scQrEnvelopeBuffer(), TP_CAL_QR_ENVELOPE_CAPACITY);
  ScQrWriter identity(scQrValuesBuffer(), TP_CAL_QR_VALUES_CAPACITY);
  if (!scQrBuildIdentity(identity, errorText, errorTextSize) || !identity.valid()) return false;
  ScQrWriter role(scQrApprovalBuffer(), TP_CAL_QR_APPROVAL_CAPACITY);
  if (!scQrBuildRole(role) || !role.valid())
  {
    scSetText(errorText, errorTextSize, "Laborzertifikat ist für TP3C1 zu groß oder ungültig");
    return false;
  }

  if (!envelope.fieldUInt(1U, 1U) ||
      !envelope.fieldUInt(2U, 2U) ||
      !envelope.fieldUInt(3U, hasFirmwareContext ? 255U : 63U) ||
      !envelope.fieldUInt(4U, packedSemver) ||
      !envelope.fieldBytes(5U, identity.data(), identity.length())) return false;
  if (tpCalQrSelectedRole.keyId[0] != '\0' &&
      !envelope.fieldBytes(6U, role.data(), role.length())) return false;

  for (uint8_t i = 0U; i < 3U; ++i)
  {
    const ScQrPackageKind kind = static_cast<ScQrPackageKind>(i);
    if (!scQrReadAndParsePackage(kind, tpCalQrMeta, errorText, errorTextSize)) return false;
    ScQrWriter common(scQrCommonBuffer(), TP_CAL_QR_COMMON_CAPACITY);
    if (!scQrBuildCommon(kind, common, errorText, errorTextSize) || !common.valid())
    {
      if (errorText != nullptr && errorText[0] == '\0')
        scSetText(errorText, errorTextSize, "Kalibrierschein ist für TP3C1 zu groß");
      return false;
    }
    if (!envelope.fieldBytes(static_cast<uint8_t>(7U + i), common.data(), common.length()))
    {
      scSetText(errorText, errorTextSize, "TP3C1-Kalibrierumschlag ist zu groß");
      return false;
    }
  }

  if (hasFirmwareContext)
  {
    ScQrWriter firmwareContext(scQrCommonBuffer(), TP_CAL_QR_COMMON_CAPACITY);
    TpSystemFirmwareContextStatus context;
    if (!scQrBuildFirmwareContext(tpCalQrSystemRequestId, tpCalQrSystemRequestHash,
                                  firmwareContext, context,
                                  errorText, errorTextSize) ||
        !firmwareContext.valid() || firmwareContext.length() == 0U) return false;
    if (!envelope.fieldBytes(10U, firmwareContext.data(), firmwareContext.length()) ||
        !envelope.fieldUInt(11U, context.applicability)) return false;
    if (context.currentHashAvailable)
    {
      const TpFirmwareIntegrityStatus& current = tpFirmwareIntegrityStatus();
      if (!current.hashCalculated ||
          !envelope.fieldBytes(12U, current.measuredSha256, sizeof(current.measuredSha256))) return false;
    }
  }
  if (!envelope.valid() || envelope.length() == 0U || envelope.length() > 65535U)
  {
    scSetText(errorText, errorTextSize, "TP3C1-Umschlag ist zu groß");
    return false;
  }

  uint8_t digest[32];
  TpSha256::hash(envelope.data(), envelope.length(), digest);
  size_t compressedLength = 0U;
  const bool compressionOk = TpZlibDeflate::compressFixed(envelope.data(), envelope.length(),
                                                           scQrCompressedBuffer(),
                                                           TP_CAL_QR_COMPRESSED_CAPACITY,
                                                           &compressedLength);
  const bool useCompressed = compressionOk && compressedLength < envelope.length();
  const uint8_t* transport = useCompressed ? scQrCompressedBuffer() : envelope.data();
  const size_t totalTransport = useCompressed ? compressedLength : envelope.length();
  if (totalTransport == 0U || totalTransport > 65535U)
  {
    scSetText(errorText, errorTextSize, "TP3C1-Transport ist zu groß");
    return false;
  }

  const size_t firstLength = (totalTransport + 1U) / 2U;
  const size_t secondLength = totalTransport - firstLength;
  if (!scQrBuildTransportPart(transport, totalTransport, digest, 1U,
                              0U, firstLength, useCompressed,
                              part1, part1Size) ||
      !scQrBuildTransportPart(transport, totalTransport, digest, 2U,
                              firstLength, secondLength, useCompressed,
                              part2, part2Size))
  {
    scSetText(errorText, errorTextSize,
              "TP3C1-QR-Text passt nicht in den Ausgabepuffer");
    part1[0] = '\0';
    part2[0] = '\0';
    return false;
  }

  scBytesToHex(digest, 8U, documentId, documentIdSize, true);
  if (envelopeSize != nullptr) *envelopeSize = static_cast<uint16_t>(envelope.length());
  if (transportSize != nullptr) *transportSize = static_cast<uint16_t>(totalTransport);
  if (compressedUsed != nullptr) *compressedUsed = useCompressed;
  scSetText(errorText, errorTextSize, "");
  return true;
}

bool FLASHMEM tpSignedCalibrationEncodeTp3c1Qr(
    const char* text,
    TpQrCode::Matrix& output,
    char* errorText,
    size_t errorTextSize)
{
  if (errorText != nullptr && errorTextSize > 0U) errorText[0] = '\0';
  if (text == nullptr || text[0] == '\0')
  {
    scSetText(errorText, errorTextSize, "QR-Teil ist leer");
    return false;
  }

  // Reuse the calibration scratch buffers only after the TP3C1 text has been
  // completed. The regions are non-overlapping for the full QR version-40
  // encoder and are no longer needed by the envelope builder at this point.
  static_assert(sizeof(tpSignedCalCanonical) >= TpQrCode::MATRIX_BUFFER_SIZE,
                "Calibration canonical scratch is too small for QR functions");
  static_assert(sizeof(tpSignedCalObjectB) >= TpQrCode::CODEWORD_BUFFER_SIZE,
                "Calibration object B is too small for QR codewords");
  static_assert(sizeof(tpSignedCalObjectA) >=
                    TpQrCode::CODEWORD_BUFFER_SIZE + TpQrCode::BLOCK_BUFFER_SIZE + 512U,
                "Calibration object A is too small for QR interleave/block scratch");
  uint8_t* functionModules = tpSignedCalCanonical;
  uint8_t* dataCodewords = reinterpret_cast<uint8_t*>(tpSignedCalObjectB);
  uint8_t* interleavedCodewords = reinterpret_cast<uint8_t*>(tpSignedCalObjectA);
  uint8_t* blockBuffer = reinterpret_cast<uint8_t*>(tpSignedCalObjectA) + 4096U;

  static const char deepLinkPrefix[] = "tp3000://verify#";
  return TpQrCode::encodeBytePrefixAlphanumericMedium(
      deepLinkPrefix,
      text,
      output,
      functionModules,
      dataCodewords,
      interleavedCodewords,
      blockBuffer,
      errorText,
      errorTextSize);
}
