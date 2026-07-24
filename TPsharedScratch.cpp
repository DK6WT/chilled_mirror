/*
 * TP-3000 shared transient work buffers
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPsharedScratch.h"
#include <Arduino.h>

DMAMEM uint8_t tpSharedSha256IoBuffer[TP_SHARED_SHA256_IO_BUFFER_SIZE];
