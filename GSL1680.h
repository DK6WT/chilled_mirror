/*
 * TP-3000 integration note
 * ------------------------
 * GSL1680 touchscreen driver based on Skallwar/GSL1680 (ESTBLC), which in
 * turn is based in part on wolfmanjm/GSL1680, and adapted for Teensy Wire1
 * operation in TP-3000.
 *
 * Both upstream repositories are distributed under GNU GPL version 3. In
 * July 2026 the Skallwar maintainer clarified that the project should have
 * been GPLv3 from the beginning and corrected the public repository license.
 * Copyright in upstream and contributed code remains with the respective
 * authors and contributors.
 *
 * The panel-specific firmware table is a separate vendor component and is
 * intentionally not distributed in this public source package.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Full details: LICENSES/GSL1680-NOTICE.txt and THIRD_PARTY_NOTICES.md
 */

/**************************************************************************/
/*!
    @file     GSL1680.h
    @author   ESTBLC
    @section  HISTORY
    
    v1.0 - First release

    @thanks to wolfmanjm for the source code this lib is bassed on : https://github.com/wolfmanjm/GSL1680
*/
#include <Arduino.h>
//#include <i2c_t3.h>
 
#include <Wire.h>
#include "GSL1680Firmware.h"
#ifndef _GSL1680
#define _GSL1680

class GSL1680 {
 public:
  GSL1680 ();
  
  void      begin(uint8_t WAKE, uint8_t INTRPT);
  void      clear_reg();
  void      reset();
  void      loadfw();
  void      startchip();
  void      sleep();
  void      datasend(uint8_t REG, uint8_t DATA[], uint16_t NB);
  uint8_t       dataread();
  uint8_t   readFingerID(int NB);
  uint32_t  readFingerX(int NB);
  uint32_t  readFingerY(int NB);
};

struct Scoords {
    uint8_t fingerID;
    uint32_t X,Y;
};

struct Sts_event {
    uint8_t NBfingers;
    struct Scoords coords[5];
};

#endif
