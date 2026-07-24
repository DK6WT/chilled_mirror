/*
 * TP-3000 SHA-256 helper
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_SHA256_H
#define TP3000_SHA256_H

#include <Arduino.h>

class TpSha256
{
public:
  TpSha256();
  void reset();
  void update(const void* data, size_t length);
  void final(uint8_t digest[32]);
  static void hash(const void* data, size_t length, uint8_t digest[32]);

private:
  void transform(const uint8_t block[64]);

  uint32_t state_[8];
  uint64_t totalBytes_;
  uint8_t buffer_[64];
  size_t bufferLength_;
};

#endif
