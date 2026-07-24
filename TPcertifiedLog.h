/*
 * TP-3000 certified SD log support
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_CERTIFIED_LOG_H
#define TP3000_CERTIFIED_LOG_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// Called after SD and the signed calibration cache are available. Recovers and
// seals a journalled segment left open by an unclean shutdown.
bool tpCertifiedLogBegin(char* errorText, size_t errorTextSize);

// Returns true and supplies a segmented CSV path when SHA-256 or certified
// integrity is selected. Returns false in legacy/off mode.
bool tpCertifiedLogGetCsvPath(bool diagnostic,
                              char* output,
                              size_t outputSize,
                              char* errorText,
                              size_t errorTextSize);

// Creates the immutable evidence snapshot and write-ahead journal before the
// first byte of a segmented CSV file is appended.
bool tpCertifiedLogBeforeAppend(const char* csvPath,
                                bool fileWasNew,
                                char* errorText,
                                size_t errorTextSize);

// Advances the CRC-protected journal only after the CSV file has been flushed
// and closed successfully.
bool tpCertifiedLogAfterFlush(const char* csvPath,
                              uint32_t confirmedSize,
                              char* errorText,
                              size_t errorTextSize);

// True when mode, date, diagnostic format, calibration evidence or measurement
// configuration no longer match the active segment.
bool tpCertifiedLogNeedsRotation(bool diagnostic);

// Seals the active segment. CSV-certified mode creates .CSV plus .TPSIG.
// TPLOG-certified mode creates one verified .TPLOG container and removes the
// temporary CSV/TPSIG intermediates after the container is complete. SHA mode
// creates .CSV plus a GNU-compatible .sha256 file. The context snapshot is
// removed only after every required output was verified.
bool tpCertifiedLogFinalize(bool recoveredAfterUncleanShutdown,
                            char* sealedCsvPath,
                            size_t sealedCsvPathSize,
                            char* errorText,
                            size_t errorTextSize);

bool tpCertifiedLogHasActiveSegment(void);
const char* tpCertifiedLogActiveCsvPath(void);
const char* tpCertifiedLogLastError(void);

#endif
