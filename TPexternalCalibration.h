/*
 * TP-3000 external calibration certificate PDF storage
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_EXTERNAL_CALIBRATION_H
#define TP3000_EXTERNAL_CALIBRATION_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#define TP_EXTERNAL_CAL_MAX_PDF_BYTES (6UL * 1024UL * 1024UL)

struct TpExternalCalibrationMetadata
{
  bool present;
  char documentId[33];
  char certificateNumber[33];
  uint32_t validFromYmd;
  uint32_t validUntilYmd;
  char originalFileName[81];
  char storedPdfPath[128];
  uint32_t fileSize;
  char sha256[65];
};

void tpExternalCalibrationBegin(void);
bool tpExternalCalibrationActiveReady(void);
const TpExternalCalibrationMetadata& tpExternalCalibrationActive(void);

// Lädt einen beliebigen archivierten externen Kalibrierschein anhand seiner
// 32-stelligen Dokument-ID und prüft Metadaten, PDF-Größe, PDF-Kennung und
// SHA-256 erneut. Damit kann die Zertifikatshistorie genau das zu einer
// Systemkalibrierung gehörende Original-PDF öffnen, nicht nur das aktuell
// aktive Dokument.
bool tpExternalCalibrationLoadArchived(const char* documentId,
                                       TpExternalCalibrationMetadata& metadata,
                                       char* errorText,
                                       size_t errorTextSize);

// Schnelle Existenzprüfung für Trefferlisten. Prüft Dokument-ID,
// Metadaten-/PDF-Datei und eine plausible PDF-Dateigröße. Die vollständige
// SHA-256- und PDF-Kennung-Prüfung erfolgt beim tatsächlichen Öffnen.
bool tpExternalCalibrationArchivedPdfPresent(const char* documentId);

bool tpExternalCalibrationBeginUpload(const char* certificateNumber,
                                      uint32_t validFromYmd,
                                      uint32_t validUntilYmd,
                                      const char* originalFileName,
                                      uint32_t expectedSize,
                                      char* errorText,
                                      size_t errorTextSize);

bool tpExternalCalibrationAppendChunk(uint32_t offset,
                                      const uint8_t* data,
                                      size_t length,
                                      char* errorText,
                                      size_t errorTextSize);

bool tpExternalCalibrationFinishUpload(char* resultText,
                                       size_t resultTextSize,
                                       char* errorText,
                                       size_t errorTextSize);

void tpExternalCalibrationAbortUpload(void);

bool tpExternalCalibrationMatches(const char* documentId,
                                  const char* certificateNumber,
                                  uint32_t validFromYmd,
                                  uint32_t validUntilYmd,
                                  const char* originalFileName,
                                  uint32_t fileSize,
                                  const char* sha256Hex);

// Prüft einen beliebigen archivierten externen Kalibrierschein vollständig
// gegen die in einer Systemkalibrierung signierten Metadaten. Anders als
// tpExternalCalibrationMatches() ist diese Prüfung nicht an den aktuell
// gesetzten PDF-Zeiger gebunden. PDF-Kennung, Dateigröße und SHA-256 werden
// direkt vom archivierten Original erneut geprüft.
bool tpExternalCalibrationArchivedMatches(const char* documentId,
                                          const char* certificateNumber,
                                          uint32_t validFromYmd,
                                          uint32_t validUntilYmd,
                                          const char* originalFileName,
                                          uint32_t fileSize,
                                          const char* sha256Hex);

#endif
