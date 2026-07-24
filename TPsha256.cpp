/*
 * TP-3000 SHA-256 helper
 * Compact FIPS 180-4 implementation for firmware identity records.
 * V0.50.1_10: SHA-256-Code und Rundentabelle liegen im QSPI-Flash.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPsha256.h"
#include <string.h>

namespace
{
static const uint32_t K[64] PROGMEM =
{
  0x428A2F98UL, 0x71374491UL, 0xB5C0FBCFUL, 0xE9B5DBA5UL,
  0x3956C25BUL, 0x59F111F1UL, 0x923F82A4UL, 0xAB1C5ED5UL,
  0xD807AA98UL, 0x12835B01UL, 0x243185BEUL, 0x550C7DC3UL,
  0x72BE5D74UL, 0x80DEB1FEUL, 0x9BDC06A7UL, 0xC19BF174UL,
  0xE49B69C1UL, 0xEFBE4786UL, 0x0FC19DC6UL, 0x240CA1CCUL,
  0x2DE92C6FUL, 0x4A7484AAUL, 0x5CB0A9DCUL, 0x76F988DAUL,
  0x983E5152UL, 0xA831C66DUL, 0xB00327C8UL, 0xBF597FC7UL,
  0xC6E00BF3UL, 0xD5A79147UL, 0x06CA6351UL, 0x14292967UL,
  0x27B70A85UL, 0x2E1B2138UL, 0x4D2C6DFCUL, 0x53380D13UL,
  0x650A7354UL, 0x766A0ABBUL, 0x81C2C92EUL, 0x92722C85UL,
  0xA2BFE8A1UL, 0xA81A664BUL, 0xC24B8B70UL, 0xC76C51A3UL,
  0xD192E819UL, 0xD6990624UL, 0xF40E3585UL, 0x106AA070UL,
  0x19A4C116UL, 0x1E376C08UL, 0x2748774CUL, 0x34B0BCB5UL,
  0x391C0CB3UL, 0x4ED8AA4AUL, 0x5B9CCA4FUL, 0x682E6FF3UL,
  0x748F82EEUL, 0x78A5636FUL, 0x84C87814UL, 0x8CC70208UL,
  0x90BEFFFAUL, 0xA4506CEBUL, 0xBEF9A3F7UL, 0xC67178F2UL
};

static inline uint32_t FLASHMEM rotr(uint32_t value, uint8_t count)
{
  return (value >> count) | (value << (32U - count));
}

static inline uint32_t FLASHMEM loadBe32(const uint8_t* p)
{
  return ((uint32_t)p[0] << 24) |
         ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) |
         (uint32_t)p[3];
}

static inline void FLASHMEM storeBe32(uint8_t* p, uint32_t value)
{
  p[0] = (uint8_t)(value >> 24);
  p[1] = (uint8_t)(value >> 16);
  p[2] = (uint8_t)(value >> 8);
  p[3] = (uint8_t)value;
}
}

FLASHMEM TpSha256::TpSha256()
{
  reset();
}

void FLASHMEM TpSha256::reset()
{
  state_[0] = 0x6A09E667UL;
  state_[1] = 0xBB67AE85UL;
  state_[2] = 0x3C6EF372UL;
  state_[3] = 0xA54FF53AUL;
  state_[4] = 0x510E527FUL;
  state_[5] = 0x9B05688CUL;
  state_[6] = 0x1F83D9ABUL;
  state_[7] = 0x5BE0CD19UL;
  totalBytes_ = 0U;
  bufferLength_ = 0U;
  memset(buffer_, 0, sizeof(buffer_));
}

void FLASHMEM TpSha256::update(const void* data, size_t length)
{
  if (data == nullptr || length == 0U) return;

  const uint8_t* input = static_cast<const uint8_t*>(data);
  totalBytes_ += (uint64_t)length;

  if (bufferLength_ > 0U)
  {
    size_t needed = 64U - bufferLength_;
    size_t take = length < needed ? length : needed;
    memcpy(buffer_ + bufferLength_, input, take);
    bufferLength_ += take;
    input += take;
    length -= take;

    if (bufferLength_ == 64U)
    {
      transform(buffer_);
      bufferLength_ = 0U;
    }
  }

  while (length >= 64U)
  {
    transform(input);
    input += 64U;
    length -= 64U;
  }

  if (length > 0U)
  {
    memcpy(buffer_, input, length);
    bufferLength_ = length;
  }
}

void FLASHMEM TpSha256::final(uint8_t digest[32])
{
  if (digest == nullptr) return;

  const uint64_t totalBits = totalBytes_ * 8ULL;
  buffer_[bufferLength_++] = 0x80U;

  if (bufferLength_ > 56U)
  {
    while (bufferLength_ < 64U) buffer_[bufferLength_++] = 0U;
    transform(buffer_);
    bufferLength_ = 0U;
  }

  while (bufferLength_ < 56U) buffer_[bufferLength_++] = 0U;

  for (uint8_t i = 0U; i < 8U; i++)
  {
    buffer_[63U - i] = (uint8_t)(totalBits >> (8U * i));
  }
  transform(buffer_);

  for (uint8_t i = 0U; i < 8U; i++)
  {
    storeBe32(digest + (size_t)i * 4U, state_[i]);
  }

  reset();
}

void FLASHMEM TpSha256::hash(const void* data, size_t length, uint8_t digest[32])
{
  TpSha256 context;
  context.update(data, length);
  context.final(digest);
}

void FLASHMEM TpSha256::transform(const uint8_t block[64])
{
  uint32_t w[64];
  for (uint8_t i = 0U; i < 16U; i++)
  {
    w[i] = loadBe32(block + (size_t)i * 4U);
  }

  for (uint8_t i = 16U; i < 64U; i++)
  {
    const uint32_t s0 = rotr(w[i - 15U], 7U) ^
                        rotr(w[i - 15U], 18U) ^
                        (w[i - 15U] >> 3U);
    const uint32_t s1 = rotr(w[i - 2U], 17U) ^
                        rotr(w[i - 2U], 19U) ^
                        (w[i - 2U] >> 10U);
    w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
  }

  uint32_t a = state_[0];
  uint32_t b = state_[1];
  uint32_t c = state_[2];
  uint32_t d = state_[3];
  uint32_t e = state_[4];
  uint32_t f = state_[5];
  uint32_t g = state_[6];
  uint32_t h = state_[7];

  for (uint8_t i = 0U; i < 64U; i++)
  {
    const uint32_t sum1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
    const uint32_t choose = (e & f) ^ ((~e) & g);
    const uint32_t temp1 = h + sum1 + choose + K[i] + w[i];
    const uint32_t sum0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
    const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = sum0 + majority;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state_[0] += a;
  state_[1] += b;
  state_[2] += c;
  state_[3] += d;
  state_[4] += e;
  state_[5] += f;
  state_[6] += g;
  state_[7] += h;
}
