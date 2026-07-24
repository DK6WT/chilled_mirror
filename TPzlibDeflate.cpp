/*
 * Small zlib/DEFLATE encoder for TP-3000 QR transport.
 * Emits one fixed-Huffman DEFLATE block and a standards-compliant zlib wrapper.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPzlibDeflate.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#ifndef FLASHMEM
#define FLASHMEM
#endif
#endif

#include <string.h>

namespace TpZlibDeflate
{
namespace
{
  struct BitWriter
  {
    uint8_t* output;
    size_t capacity;
    size_t byteIndex;
    uint32_t bits;
    uint8_t bitCount;
    bool valid;

    BitWriter(uint8_t* out, size_t cap, size_t start)
      : output(out), capacity(cap), byteIndex(start), bits(0U), bitCount(0U), valid(true) {}

    void write(unsigned int value, uint8_t count)
    {
      if (!valid || count > 24U) { valid = false; return; }
      bits |= static_cast<uint32_t>(value) << bitCount;
      bitCount = static_cast<uint8_t>(bitCount + count);
      while (bitCount >= 8U)
      {
        if (byteIndex >= capacity) { valid = false; return; }
        output[byteIndex++] = static_cast<uint8_t>(bits & 0xFFU);
        bits >>= 8U;
        bitCount = static_cast<uint8_t>(bitCount - 8U);
      }
    }

    void finish()
    {
      if (!valid) return;
      if (bitCount > 0U)
      {
        if (byteIndex >= capacity) { valid = false; return; }
        output[byteIndex++] = static_cast<uint8_t>(bits & 0xFFU);
        bits = 0U;
        bitCount = 0U;
      }
    }
  };

  unsigned int FLASHMEM reverseBits(unsigned int value, uint8_t count)
  {
    unsigned int result = 0U;
    for (uint8_t i = 0U; i < count; ++i)
    {
      result = (result << 1U) | (value & 1U);
      value >>= 1U;
    }
    return result;
  }

  void FLASHMEM writeFixedSymbol(BitWriter& writer, int symbol)
  {
    unsigned int code = 0U;
    uint8_t length = 0U;
    if (symbol <= 143)
    {
      code = static_cast<unsigned int>(0x30 + symbol);
      length = 8U;
    }
    else if (symbol <= 255)
    {
      code = static_cast<unsigned int>(0x190 + symbol - 144);
      length = 9U;
    }
    else if (symbol <= 279)
    {
      code = static_cast<unsigned int>(symbol - 256);
      length = 7U;
    }
    else
    {
      code = static_cast<unsigned int>(0xC0 + symbol - 280);
      length = 8U;
    }
    writer.write(reverseBits(code, length), length);
  }

  static const uint16_t LENGTH_BASE[29] =
  {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
  };
  static const uint8_t LENGTH_EXTRA[29] =
  {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
  };
  static const uint16_t DIST_BASE[30] =
  {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,
    4097,6145,8193,12289,16385,24577
  };
  static const uint8_t DIST_EXTRA[30] =
  {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
  };

  void FLASHMEM writeLengthDistance(BitWriter& writer, int length, int distance)
  {
    int lengthCode = 28;
    for (int i = 0; i < 29; ++i)
    {
      const int maxLength = LENGTH_BASE[i] + ((1 << LENGTH_EXTRA[i]) - 1);
      if (length <= maxLength) { lengthCode = i; break; }
    }
    writeFixedSymbol(writer, 257 + lengthCode);
    const uint8_t lengthExtra = LENGTH_EXTRA[lengthCode];
    if (lengthExtra > 0U)
      writer.write(static_cast<unsigned int>(length - LENGTH_BASE[lengthCode]), lengthExtra);

    int distanceCode = 29;
    for (int i = 0; i < 30; ++i)
    {
      const int maxDistance = DIST_BASE[i] + ((1 << DIST_EXTRA[i]) - 1);
      if (distance <= maxDistance) { distanceCode = i; break; }
    }
    writer.write(reverseBits(static_cast<unsigned int>(distanceCode), 5U), 5U);
    const uint8_t distanceExtra = DIST_EXTRA[distanceCode];
    if (distanceExtra > 0U)
      writer.write(static_cast<unsigned int>(distance - DIST_BASE[distanceCode]), distanceExtra);
  }

  uint32_t FLASHMEM adler32(const uint8_t* data, size_t length)
  {
    uint32_t a = 1U;
    uint32_t b = 0U;
    size_t offset = 0U;
    while (offset < length)
    {
      size_t chunk = length - offset;
      if (chunk > 5552U) chunk = 5552U;
      for (size_t i = 0U; i < chunk; ++i)
      {
        a += data[offset + i];
        b += a;
      }
      a %= 65521U;
      b %= 65521U;
      offset += chunk;
    }
    return (b << 16U) | a;
  }
}

bool FLASHMEM compressFixed(const uint8_t* input,
                   size_t inputLength,
                   uint8_t* output,
                   size_t outputCapacity,
                   size_t* outputLength)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if ((input == nullptr && inputLength > 0U) || output == nullptr || outputCapacity < 8U)
    return false;

  // CMF/FLG: DEFLATE, 32 KiB window, fastest compression marker, FCHECK valid.
  output[0] = 0x78U;
  output[1] = 0x01U;
  BitWriter writer(output, outputCapacity - 4U, 2U);

  // One final block, fixed Huffman codes. Bits are written least-significant first.
  writer.write(1U, 1U);  // BFINAL
  writer.write(1U, 2U);  // BTYPE=01

  size_t pos = 0U;
  while (pos < inputLength && writer.valid)
  {
    int bestLength = 0;
    int bestDistance = 0;
    const size_t windowStart = pos > 32768U ? pos - 32768U : 0U;
    int candidates = 0;

    // Greedy backward search. The TP3C1 envelope is at most a few KiB, so a
    // bounded candidate count keeps runtime deterministic without a hash table.
    for (size_t previous = pos; previous > windowStart && candidates < 384; )
    {
      --previous;
      if (input[previous] != input[pos]) continue;
      ++candidates;
      int length = 1;
      const int maximum = static_cast<int>((inputLength - pos) > 258U ? 258U : (inputLength - pos));
      while (length < maximum && input[previous + static_cast<size_t>(length)] == input[pos + static_cast<size_t>(length)])
        ++length;
      if (length >= 3 && length > bestLength)
      {
        bestLength = length;
        bestDistance = static_cast<int>(pos - previous);
        if (length == maximum) break;
      }
    }

    if (bestLength >= 3)
    {
      writeLengthDistance(writer, bestLength, bestDistance);
      pos += static_cast<size_t>(bestLength);
    }
    else
    {
      writeFixedSymbol(writer, input[pos]);
      ++pos;
    }
  }
  writeFixedSymbol(writer, 256);
  writer.finish();
  if (!writer.valid || writer.byteIndex + 4U > outputCapacity) return false;

  const uint32_t checksum = adler32(input, inputLength);
  output[writer.byteIndex++] = static_cast<uint8_t>((checksum >> 24U) & 0xFFU);
  output[writer.byteIndex++] = static_cast<uint8_t>((checksum >> 16U) & 0xFFU);
  output[writer.byteIndex++] = static_cast<uint8_t>((checksum >> 8U) & 0xFFU);
  output[writer.byteIndex++] = static_cast<uint8_t>(checksum & 0xFFU);
  if (outputLength != nullptr) *outputLength = writer.byteIndex;
  return true;
}
}
