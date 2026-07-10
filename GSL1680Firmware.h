/*
 * TP-3000 local GSL1680 firmware include
 *
 * The panel-specific firmware is intentionally not distributed with TP-3000.
 * Obtain the original file directly from EastRising / BuyDisplay and place it at:
 *
 *   external/GSL1680/gslX680_311_5_F.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef TP3000_GSL1680_FIRMWARE_INCLUDE_H
#define TP3000_GSL1680_FIRMWARE_INCLUDE_H

/*
 * The original BuyDisplay header uses the 8051 memory-space keyword "code".
 * On Teensy 4.1 it is mapped to PROGMEM so the one-time boot firmware stays
 * in flash instead of occupying RAM1 for the whole runtime.
 */
#ifndef code
#define TP3000_GSL1680_CODE_DEFINED_LOCALLY
#define code PROGMEM
#endif

#if defined(__has_include)
#  if __has_include("external/GSL1680/gslX680_311_5_F.h")
#    include "external/GSL1680/gslX680_311_5_F.h"
#  else
#    error "TP-3000: missing external/GSL1680/gslX680_311_5_F.h - see external/GSL1680/README.md"
#  endif
#else
#  include "external/GSL1680/gslX680_311_5_F.h"
#endif

#ifdef TP3000_GSL1680_CODE_DEFINED_LOCALLY
#undef code
#undef TP3000_GSL1680_CODE_DEFINED_LOCALLY
#endif

#endif