/*
 * TP-3000 shared transient work buffers
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

static constexpr size_t TP_SHARED_SHA256_IO_BUFFER_SIZE = 4096U;

// Single-threaded shared SHA-256 I/O buffer. Firmware image hashing is only
// performed during boot; external PDF hashing runs later in the normal task
// context. The two users therefore never overlap.
extern uint8_t tpSharedSha256IoBuffer[TP_SHARED_SHA256_IO_BUFFER_SIZE];
