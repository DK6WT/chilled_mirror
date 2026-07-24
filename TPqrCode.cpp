/*
 * TP-3000 QR Code generator adapter
 *
 * Core algorithm adapted from Project Nayuki's QR Code generator library:
 * Copyright (c) Project Nayuki, MIT License.
 * https://www.nayuki.io/page/qr-code-generator-library
 *
 * This reduced port supports the exact modes needed by TP-3000 TFT output:
 * QR Model 2, versions 1..40, ECC level M and automatic mask selection,
 * either one alphanumeric segment or the canonical TP3C1 deep-link split
 * into one byte segment plus one alphanumeric segment.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "TPqrCode.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#ifndef FLASHMEM
#define FLASHMEM
#endif
#endif

#include <limits.h>
#include <string.h>
#include <stdio.h>

namespace TpQrCode
{
namespace
{
  static const char ALPHANUMERIC_CHARSET[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

  // Index 0 is intentionally invalid. Rows: L, M, Q, H.
  static const int8_t ECC_CODEWORDS_PER_BLOCK[4][41] = {
    {-1,7,10,15,20,26,18,20,24,30,18,20,24,26,30,22,24,28,30,28,28,28,28,30,30,26,28,30,30,30,30,30,30,30,30,30,30,30,30,30,30},
    {-1,10,16,26,18,24,16,18,22,22,26,30,22,22,24,24,28,28,26,26,26,26,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28},
    {-1,13,22,18,26,18,24,18,22,20,24,28,26,24,20,30,24,28,28,26,30,28,30,30,30,30,28,30,30,30,30,30,30,30,30,30,30,30,30,30,30},
    {-1,17,28,22,16,22,28,26,26,24,28,24,28,22,24,24,30,28,28,26,28,30,24,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30}
  };

  static const int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41] = {
    {-1,1,1,1,1,1,2,2,2,2,4,4,4,4,4,6,6,6,6,7,8,8,9,9,10,12,12,12,13,14,15,16,17,18,19,19,20,21,22,24,25},
    {-1,1,1,1,2,2,4,4,4,5,5,5,8,9,9,10,10,11,13,14,16,17,17,18,20,21,23,25,26,28,29,31,33,35,37,38,40,43,45,47,49},
    {-1,1,1,2,2,4,4,6,6,8,8,8,10,12,16,12,17,16,18,21,20,23,23,25,27,29,34,34,35,38,40,43,45,48,51,53,56,59,62,65,68},
    {-1,1,1,2,4,4,4,5,6,8,8,11,11,16,16,18,16,19,21,25,25,25,34,30,32,35,37,40,42,45,48,51,54,57,60,63,66,70,74,77,81}
  };

  static constexpr int ECC_ORDINAL_MEDIUM = 1;
  static constexpr int ECC_FORMAT_BITS_MEDIUM = 0;
  static constexpr int PENALTY_N1 = 3;
  static constexpr int PENALTY_N2 = 3;
  static constexpr int PENALTY_N3 = 40;
  static constexpr int PENALTY_N4 = 10;

  void FLASHMEM setError(char* target, size_t size, const char* text)
  {
    if (target == nullptr || size == 0U) return;
    if (text == nullptr) text = "QR-Fehler";
    snprintf(target, size, "%s", text);
    target[size - 1U] = '\0';
  }

  inline bool getBitFromByte(uint8_t x, int i)
  {
    return ((x >> i) & 1U) != 0U;
  }

  inline size_t moduleIndex(int size, int x, int y)
  {
    return static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x);
  }

  inline bool bitsetGet(const uint8_t* bits, int size, int x, int y)
  {
    const size_t index = moduleIndex(size, x, y);
    return ((bits[index >> 3U] >> (index & 7U)) & 1U) != 0U;
  }

  inline void bitsetSet(uint8_t* bits, int size, int x, int y, bool dark)
  {
    const size_t index = moduleIndex(size, x, y);
    const uint8_t mask = static_cast<uint8_t>(1U << (index & 7U));
    if (dark) bits[index >> 3U] |= mask;
    else bits[index >> 3U] &= static_cast<uint8_t>(~mask);
  }

  inline void bitsetToggle(uint8_t* bits, int size, int x, int y)
  {
    const size_t index = moduleIndex(size, x, y);
    bits[index >> 3U] ^= static_cast<uint8_t>(1U << (index & 7U));
  }

  void FLASHMEM setFunctionModule(Matrix& qr, uint8_t functionModules[], int x, int y, bool dark)
  {
    bitsetSet(qr.modules, qr.size, x, y, dark);
    bitsetSet(functionModules, qr.size, x, y, true);
  }

  int FLASHMEM getNumRawDataModules(int version)
  {
    int result = (16 * version + 128) * version + 64;
    if (version >= 2)
    {
      const int numAlign = version / 7 + 2;
      result -= (25 * numAlign - 10) * numAlign - 55;
      if (version >= 7) result -= 36;
    }
    return result;
  }

  int FLASHMEM getNumDataCodewords(int version)
  {
    return getNumRawDataModules(version) / 8
        - ECC_CODEWORDS_PER_BLOCK[ECC_ORDINAL_MEDIUM][version]
        * NUM_ERROR_CORRECTION_BLOCKS[ECC_ORDINAL_MEDIUM][version];
  }

  int FLASHMEM alphaValue(char c)
  {
    const char* p = strchr(ALPHANUMERIC_CHARSET, c);
    return p == nullptr ? -1 : static_cast<int>(p - ALPHANUMERIC_CHARSET);
  }

  int FLASHMEM charCountBits(int version)
  {
    return version <= 9 ? 9 : (version <= 26 ? 11 : 13);
  }

  int FLASHMEM byteCharCountBits(int version)
  {
    return version <= 9 ? 8 : 16;
  }

  bool FLASHMEM appendBits(uint8_t buffer[], size_t capacityBytes,
                  unsigned int value, int count, int& bitLength)
  {
    if (count < 0 || count > 31) return false;
    if (count < 32 && (value >> count) != 0U) return false;
    if (static_cast<size_t>(bitLength + count) > capacityBytes * 8U) return false;
    for (int i = count - 1; i >= 0; --i, ++bitLength)
    {
      if (((value >> i) & 1U) != 0U)
        buffer[bitLength >> 3] |= static_cast<uint8_t>(1U << (7 - (bitLength & 7)));
    }
    return true;
  }

  uint8_t FLASHMEM reedSolomonMultiply(uint8_t x, uint8_t y)
  {
    uint16_t z = 0;
    for (int i = 7; i >= 0; --i)
    {
      z = static_cast<uint16_t>((z << 1) ^ (((z >> 7) & 1U) * 0x11DU));
      z ^= static_cast<uint16_t>(((y >> i) & 1U) * x);
    }
    return static_cast<uint8_t>(z);
  }

  void FLASHMEM reedSolomonComputeDivisor(int degree, uint8_t result[30])
  {
    memset(result, 0, 30U);
    result[degree - 1] = 1U;
    uint8_t root = 1U;
    for (int i = 0; i < degree; ++i)
    {
      for (int j = 0; j < degree; ++j)
      {
        result[j] = reedSolomonMultiply(result[j], root);
        if (j + 1 < degree) result[j] ^= result[j + 1];
      }
      root = reedSolomonMultiply(root, 0x02U);
    }
  }

  void FLASHMEM reedSolomonComputeRemainder(const uint8_t* data, int dataLength,
                                   const uint8_t* divisor, int degree,
                                   uint8_t result[30])
  {
    memset(result, 0, 30U);
    for (int i = 0; i < dataLength; ++i)
    {
      const uint8_t factor = static_cast<uint8_t>(data[i] ^ result[0]);
      memmove(result, result + 1, static_cast<size_t>(degree - 1));
      result[degree - 1] = 0U;
      for (int j = 0; j < degree; ++j)
        result[j] ^= reedSolomonMultiply(divisor[j], factor);
    }
  }

  bool FLASHMEM addEccAndInterleave(const uint8_t data[], int dataLength, int version,
                           uint8_t result[], uint8_t blockBuffer[])
  {
    const int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[ECC_ORDINAL_MEDIUM][version];
    const int blockEccLen = ECC_CODEWORDS_PER_BLOCK[ECC_ORDINAL_MEDIUM][version];
    const int rawCodewords = getNumRawDataModules(version) / 8;
    const int numShortBlocks = numBlocks - rawCodewords % numBlocks;
    const int shortBlockLen = rawCodewords / numBlocks;
    const int commonBlockLen = shortBlockLen + 1;
    if (numBlocks <= 0 || blockEccLen <= 0 ||
        static_cast<size_t>(numBlocks * commonBlockLen) > BLOCK_BUFFER_SIZE ||
        rawCodewords > static_cast<int>(CODEWORD_BUFFER_SIZE)) return false;

    uint8_t divisor[30];
    uint8_t ecc[30];
    reedSolomonComputeDivisor(blockEccLen, divisor);

    int sourceOffset = 0;
    for (int block = 0; block < numBlocks; ++block)
    {
      uint8_t* dst = blockBuffer + block * commonBlockLen;
      memset(dst, 0, static_cast<size_t>(commonBlockLen));
      const int blockDataLen = shortBlockLen - blockEccLen
          + (block < numShortBlocks ? 0 : 1);
      if (sourceOffset + blockDataLen > dataLength) return false;
      memcpy(dst, data + sourceOffset, static_cast<size_t>(blockDataLen));
      sourceOffset += blockDataLen;
      reedSolomonComputeRemainder(dst, blockDataLen, divisor, blockEccLen, ecc);
      const int eccOffset = blockDataLen + (block < numShortBlocks ? 1 : 0);
      memcpy(dst + eccOffset, ecc, static_cast<size_t>(blockEccLen));
    }
    if (sourceOffset != dataLength) return false;

    int out = 0;
    for (int i = 0; i < commonBlockLen; ++i)
    {
      for (int block = 0; block < numBlocks; ++block)
      {
        if (i == shortBlockLen - blockEccLen && block < numShortBlocks) continue;
        if (out >= rawCodewords) return false;
        result[out++] = blockBuffer[block * commonBlockLen + i];
      }
    }
    return out == rawCodewords;
  }

  void FLASHMEM drawFinderPattern(Matrix& qr, uint8_t functionModules[], int x, int y)
  {
    for (int dy = -4; dy <= 4; ++dy)
    {
      for (int dx = -4; dx <= 4; ++dx)
      {
        const int xx = x + dx;
        const int yy = y + dy;
        if (xx >= 0 && xx < qr.size && yy >= 0 && yy < qr.size)
        {
          const int ax = dx < 0 ? -dx : dx;
          const int ay = dy < 0 ? -dy : dy;
          const int dist = ax > ay ? ax : ay;
          setFunctionModule(qr, functionModules, xx, yy, dist != 2 && dist != 4);
        }
      }
    }
  }

  void FLASHMEM drawAlignmentPattern(Matrix& qr, uint8_t functionModules[], int x, int y)
  {
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx)
      {
        const int ax = dx < 0 ? -dx : dx;
        const int ay = dy < 0 ? -dy : dy;
        const int dist = ax > ay ? ax : ay;
        setFunctionModule(qr, functionModules, x + dx, y + dy, dist != 1);
      }
  }

  int FLASHMEM getAlignmentPatternPositions(int version, int result[7])
  {
    if (version == 1) return 0;
    const int numAlign = version / 7 + 2;
    const int step = version == 32 ? 26
        : ((version * 4 + numAlign * 2 + 1) / (numAlign * 2 - 2)) * 2;
    result[0] = 6;
    for (int i = 1; i < numAlign; ++i)
      result[i] = (version * 4 + 17) - 7 - (numAlign - 1 - i) * step;
    return numAlign;
  }

  void FLASHMEM drawFormatBits(Matrix& qr, uint8_t functionModules[], int mask)
  {
    const int data = (ECC_FORMAT_BITS_MEDIUM << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; ++i)
      rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    const int bits = ((data << 10) | rem) ^ 0x5412;

    for (int i = 0; i <= 5; ++i) setFunctionModule(qr, functionModules, 8, i, ((bits >> i) & 1) != 0);
    setFunctionModule(qr, functionModules, 8, 7, ((bits >> 6) & 1) != 0);
    setFunctionModule(qr, functionModules, 8, 8, ((bits >> 7) & 1) != 0);
    setFunctionModule(qr, functionModules, 7, 8, ((bits >> 8) & 1) != 0);
    for (int i = 9; i < 15; ++i) setFunctionModule(qr, functionModules, 14 - i, 8, ((bits >> i) & 1) != 0);

    for (int i = 0; i < 8; ++i) setFunctionModule(qr, functionModules, qr.size - 1 - i, 8, ((bits >> i) & 1) != 0);
    for (int i = 8; i < 15; ++i) setFunctionModule(qr, functionModules, 8, qr.size - 15 + i, ((bits >> i) & 1) != 0);
    setFunctionModule(qr, functionModules, 8, qr.size - 8, true);
  }

  void FLASHMEM drawVersion(Matrix& qr, uint8_t functionModules[], int version)
  {
    if (version < 7) return;
    int rem = version;
    for (int i = 0; i < 12; ++i)
      rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    const int bits = (version << 12) | rem;
    for (int i = 0; i < 18; ++i)
    {
      const bool bit = ((bits >> i) & 1) != 0;
      const int a = qr.size - 11 + i % 3;
      const int b = i / 3;
      setFunctionModule(qr, functionModules, a, b, bit);
      setFunctionModule(qr, functionModules, b, a, bit);
    }
  }

  void FLASHMEM drawFunctionPatterns(Matrix& qr, uint8_t functionModules[], int version)
  {
    for (int i = 0; i < qr.size; ++i)
    {
      setFunctionModule(qr, functionModules, 6, i, (i & 1) == 0);
      setFunctionModule(qr, functionModules, i, 6, (i & 1) == 0);
    }
    drawFinderPattern(qr, functionModules, 3, 3);
    drawFinderPattern(qr, functionModules, qr.size - 4, 3);
    drawFinderPattern(qr, functionModules, 3, qr.size - 4);

    int positions[7];
    const int count = getAlignmentPatternPositions(version, positions);
    for (int i = 0; i < count; ++i)
      for (int j = 0; j < count; ++j)
      {
        if ((i == 0 && j == 0) || (i == 0 && j == count - 1) ||
            (i == count - 1 && j == 0)) continue;
        drawAlignmentPattern(qr, functionModules, positions[i], positions[j]);
      }
    drawFormatBits(qr, functionModules, 0);
    drawVersion(qr, functionModules, version);
  }

  void FLASHMEM drawCodewords(Matrix& qr, const uint8_t functionModules[],
                     const uint8_t data[], int dataLength)
  {
    int bitIndex = 0;
    for (int right = qr.size - 1; right >= 1; right -= 2)
    {
      if (right == 6) right = 5;
      for (int vert = 0; vert < qr.size; ++vert)
      {
        for (int j = 0; j < 2; ++j)
        {
          const int x = right - j;
          const bool upward = (((right + 1) & 2) == 0);
          const int y = upward ? qr.size - 1 - vert : vert;
          if (!bitsetGet(functionModules, qr.size, x, y) && bitIndex < dataLength * 8)
          {
            bitsetSet(qr.modules, qr.size, x, y,
                      getBitFromByte(data[bitIndex >> 3], 7 - (bitIndex & 7)));
            ++bitIndex;
          }
        }
      }
    }
  }

  int FLASHMEM maskValue(int mask, int x, int y)
  {
    switch (mask)
    {
      case 0: return (x + y) % 2;
      case 1: return y % 2;
      case 2: return x % 3;
      case 3: return (x + y) % 3;
      case 4: return (x / 3 + y / 2) % 2;
      case 5: return (x * y % 2) + (x * y % 3);
      case 6: return ((x * y % 2) + (x * y % 3)) % 2;
      default: return (((x + y) % 2) + (x * y % 3)) % 2;
    }
  }

  void FLASHMEM applyMask(Matrix& qr, const uint8_t functionModules[], int mask)
  {
    for (int y = 0; y < qr.size; ++y)
      for (int x = 0; x < qr.size; ++x)
        if (!bitsetGet(functionModules, qr.size, x, y) && maskValue(mask, x, y) == 0)
          bitsetToggle(qr.modules, qr.size, x, y);
  }

  void FLASHMEM finderPenaltyAddHistory(int currentRunLength, int history[7], int size)
  {
    if (history[0] == 0) currentRunLength += size;
    for (int i = 6; i >= 1; --i) history[i] = history[i - 1];
    history[0] = currentRunLength;
  }

  int FLASHMEM finderPenaltyCountPatterns(const int history[7], int size)
  {
    (void)size;
    const int n = history[1];
    const bool core = n > 0 && history[2] == n && history[3] == n * 3 &&
                      history[4] == n && history[5] == n;
    return (core && history[0] >= n * 4 && history[6] >= n ? 1 : 0)
         + (core && history[6] >= n * 4 && history[0] >= n ? 1 : 0);
  }

  int FLASHMEM finderPenaltyTerminateAndCount(bool currentRunColor, int currentRunLength,
                                     int history[7], int size)
  {
    if (currentRunColor)
    {
      finderPenaltyAddHistory(currentRunLength, history, size);
      currentRunLength = 0;
    }
    currentRunLength += size;
    finderPenaltyAddHistory(currentRunLength, history, size);
    return finderPenaltyCountPatterns(history, size);
  }

  long FLASHMEM getPenaltyScore(const Matrix& qr)
  {
    long result = 0;
    const int size = qr.size;
    for (int y = 0; y < size; ++y)
    {
      bool runColor = false;
      int runLength = 0;
      int history[7] = {0,0,0,0,0,0,0};
      for (int x = 0; x < size; ++x)
      {
        const bool color = bitsetGet(qr.modules, size, x, y);
        if (color == runColor)
        {
          ++runLength;
          if (runLength == 5) result += PENALTY_N1;
          else if (runLength > 5) ++result;
        }
        else
        {
          finderPenaltyAddHistory(runLength, history, size);
          if (!runColor) result += finderPenaltyCountPatterns(history, size) * PENALTY_N3;
          runColor = color;
          runLength = 1;
        }
      }
      result += finderPenaltyTerminateAndCount(runColor, runLength, history, size) * PENALTY_N3;
    }

    for (int x = 0; x < size; ++x)
    {
      bool runColor = false;
      int runLength = 0;
      int history[7] = {0,0,0,0,0,0,0};
      for (int y = 0; y < size; ++y)
      {
        const bool color = bitsetGet(qr.modules, size, x, y);
        if (color == runColor)
        {
          ++runLength;
          if (runLength == 5) result += PENALTY_N1;
          else if (runLength > 5) ++result;
        }
        else
        {
          finderPenaltyAddHistory(runLength, history, size);
          if (!runColor) result += finderPenaltyCountPatterns(history, size) * PENALTY_N3;
          runColor = color;
          runLength = 1;
        }
      }
      result += finderPenaltyTerminateAndCount(runColor, runLength, history, size) * PENALTY_N3;
    }

    for (int y = 0; y < size - 1; ++y)
      for (int x = 0; x < size - 1; ++x)
      {
        const bool c = bitsetGet(qr.modules, size, x, y);
        if (c == bitsetGet(qr.modules, size, x + 1, y) &&
            c == bitsetGet(qr.modules, size, x, y + 1) &&
            c == bitsetGet(qr.modules, size, x + 1, y + 1))
          result += PENALTY_N2;
      }

    long dark = 0;
    for (int y = 0; y < size; ++y)
      for (int x = 0; x < size; ++x)
        if (bitsetGet(qr.modules, size, x, y)) ++dark;
    const long total = static_cast<long>(size) * size;
    const long diff = dark * 20L - total * 10L;
    const long absDiff = diff < 0 ? -diff : diff;
    const long k = (absDiff + total - 1L) / total - 1L;
    result += k * PENALTY_N4;
    return result;
  }

  bool FLASHMEM finishQr(int version,
                         int bitLength,
                         Matrix& output,
                         uint8_t functionModules[],
                         uint8_t dataCodewords[],
                         uint8_t interleavedCodewords[],
                         uint8_t blockBuffer[],
                         char* errorText,
                         size_t errorTextSize)
  {
    const int dataCapacityBytes = getNumDataCodewords(version);
    if (dataCapacityBytes <= 0 || dataCapacityBytes > static_cast<int>(CODEWORD_BUFFER_SIZE))
    {
      setError(errorText, errorTextSize, "QR-Datenpuffer ist zu klein");
      return false;
    }

    const int capacityBits = dataCapacityBytes * 8;
    const int terminator = (capacityBits - bitLength) < 4 ? (capacityBits - bitLength) : 4;
    if (terminator < 0 ||
        !appendBits(dataCodewords, CODEWORD_BUFFER_SIZE, 0U, terminator, bitLength))
    {
      setError(errorText, errorTextSize, "QR-Abschluss konnte nicht erzeugt werden");
      return false;
    }
    const int bytePadBits = (8 - (bitLength & 7)) & 7;
    if (!appendBits(dataCodewords, CODEWORD_BUFFER_SIZE, 0U, bytePadBits, bitLength))
    {
      setError(errorText, errorTextSize, "QR-Byteausrichtung ist fehlgeschlagen");
      return false;
    }
    int usedBytes = bitLength / 8;
    uint8_t pad = 0xECU;
    while (usedBytes < dataCapacityBytes)
    {
      dataCodewords[usedBytes++] = pad;
      pad = pad == 0xECU ? 0x11U : 0xECU;
    }

    const int rawCodewords = getNumRawDataModules(version) / 8;
    memset(interleavedCodewords, 0, CODEWORD_BUFFER_SIZE);
    memset(blockBuffer, 0, BLOCK_BUFFER_SIZE);
    if (!addEccAndInterleave(dataCodewords, dataCapacityBytes, version,
                             interleavedCodewords, blockBuffer))
    {
      setError(errorText, errorTextSize, "QR-Fehlerkorrektur konnte nicht erzeugt werden");
      return false;
    }

    output.size = static_cast<uint8_t>(version * 4 + 17);
    memset(output.modules, 0, MATRIX_BUFFER_SIZE);
    memset(functionModules, 0, MATRIX_BUFFER_SIZE);
    drawFunctionPatterns(output, functionModules, version);
    drawCodewords(output, functionModules, interleavedCodewords, rawCodewords);

    long bestPenalty = LONG_MAX;
    int bestMask = 0;
    for (int mask = 0; mask < 8; ++mask)
    {
      applyMask(output, functionModules, mask);
      drawFormatBits(output, functionModules, mask);
      const long penalty = getPenaltyScore(output);
      if (penalty < bestPenalty)
      {
        bestPenalty = penalty;
        bestMask = mask;
      }
      applyMask(output, functionModules, mask);
    }
    applyMask(output, functionModules, bestMask);
    drawFormatBits(output, functionModules, bestMask);
    return true;
  }
}

bool FLASHMEM encodeAlphanumericMedium(const char* text,
                              Matrix& output,
                              uint8_t functionModules[MATRIX_BUFFER_SIZE],
                              uint8_t dataCodewords[CODEWORD_BUFFER_SIZE],
                              uint8_t interleavedCodewords[CODEWORD_BUFFER_SIZE],
                              uint8_t blockBuffer[BLOCK_BUFFER_SIZE],
                              char* errorText,
                              size_t errorTextSize)
{
  output.size = 0U;
  if (errorText != nullptr && errorTextSize > 0U) errorText[0] = '\0';
  if (text == nullptr || functionModules == nullptr || dataCodewords == nullptr ||
      interleavedCodewords == nullptr || blockBuffer == nullptr)
  {
    setError(errorText, errorTextSize, "QR-Eingabe fehlt");
    return false;
  }

  const size_t textLength = strlen(text);
  if (textLength == 0U)
  {
    setError(errorText, errorTextSize, "QR-Inhalt ist leer");
    return false;
  }
  for (size_t i = 0; i < textLength; ++i)
  {
    if (alphaValue(text[i]) < 0)
    {
      setError(errorText, errorTextSize, "QR-Inhalt ist nicht alphanumerisch");
      return false;
    }
  }

  int version = 0;
  int dataBits = 0;
  for (int ver = MIN_VERSION; ver <= MAX_VERSION; ++ver)
  {
    const int ccBits = charCountBits(ver);
    if (textLength >= (1UL << ccBits)) continue;
    const size_t payloadBits = (textLength / 2U) * 11U + (textLength & 1U ? 6U : 0U);
    if (payloadBits > static_cast<size_t>(INT_MAX)) continue;
    const int used = 4 + ccBits + static_cast<int>(payloadBits);
    if (used <= getNumDataCodewords(ver) * 8)
    {
      version = ver;
      dataBits = used;
      break;
    }
  }
  if (version == 0)
  {
    setError(errorText, errorTextSize, "QR-Inhalt ist fuer Version 40-M zu gross");
    return false;
  }

  memset(dataCodewords, 0, CODEWORD_BUFFER_SIZE);
  int bitLength = 0;
  if (!appendBits(dataCodewords, CODEWORD_BUFFER_SIZE, 0x2U, 4, bitLength) ||
      !appendBits(dataCodewords, CODEWORD_BUFFER_SIZE,
                  static_cast<unsigned int>(textLength), charCountBits(version), bitLength))
  {
    setError(errorText, errorTextSize, "QR-Kopf konnte nicht erzeugt werden");
    return false;
  }
  for (size_t i = 0; i + 1U < textLength; i += 2U)
  {
    const unsigned int value = static_cast<unsigned int>(alphaValue(text[i]) * 45 + alphaValue(text[i + 1U]));
    if (!appendBits(dataCodewords, CODEWORD_BUFFER_SIZE, value, 11, bitLength))
    {
      setError(errorText, errorTextSize, "QR-Nutzdaten sind zu gross");
      return false;
    }
  }
  if ((textLength & 1U) != 0U &&
      !appendBits(dataCodewords, CODEWORD_BUFFER_SIZE,
                  static_cast<unsigned int>(alphaValue(text[textLength - 1U])), 6, bitLength))
  {
    setError(errorText, errorTextSize, "QR-Nutzdaten sind zu gross");
    return false;
  }
  if (bitLength != dataBits)
  {
    setError(errorText, errorTextSize, "QR-Bitlaenge ist inkonsistent");
    return false;
  }

  return finishQr(version, bitLength, output, functionModules,
                  dataCodewords, interleavedCodewords, blockBuffer,
                  errorText, errorTextSize);
}

bool FLASHMEM encodeBytePrefixAlphanumericMedium(
    const char* bytePrefix,
    const char* alphanumericText,
    Matrix& output,
    uint8_t functionModules[MATRIX_BUFFER_SIZE],
    uint8_t dataCodewords[CODEWORD_BUFFER_SIZE],
    uint8_t interleavedCodewords[CODEWORD_BUFFER_SIZE],
    uint8_t blockBuffer[BLOCK_BUFFER_SIZE],
    char* errorText,
    size_t errorTextSize)
{
  output.size = 0U;
  if (errorText != nullptr && errorTextSize > 0U) errorText[0] = '\0';
  if (bytePrefix == nullptr || alphanumericText == nullptr ||
      functionModules == nullptr || dataCodewords == nullptr ||
      interleavedCodewords == nullptr || blockBuffer == nullptr)
  {
    setError(errorText, errorTextSize, "QR-Eingabe fehlt");
    return false;
  }

  const size_t prefixLength = strlen(bytePrefix);
  const size_t textLength = strlen(alphanumericText);
  if (prefixLength == 0U || textLength == 0U)
  {
    setError(errorText, errorTextSize, "QR-Deep-Link ist unvollstaendig");
    return false;
  }
  for (size_t i = 0; i < textLength; ++i)
  {
    if (alphaValue(alphanumericText[i]) < 0)
    {
      setError(errorText, errorTextSize,
               "TP3C1-Nutzdaten sind nicht alphanumerisch");
      return false;
    }
  }

  int version = 0;
  int dataBits = 0;
  for (int ver = MIN_VERSION; ver <= MAX_VERSION; ++ver)
  {
    const int byteCcBits = byteCharCountBits(ver);
    const int alphaCcBits = charCountBits(ver);
    const uint64_t byteLimit = 1ULL << byteCcBits;
    const uint64_t alphaLimit = 1ULL << alphaCcBits;
    if (prefixLength >= byteLimit || textLength >= alphaLimit) continue;

    const size_t alphaPayloadBits =
        (textLength / 2U) * 11U + (textLength & 1U ? 6U : 0U);
    const size_t used = 4U + static_cast<size_t>(byteCcBits) + prefixLength * 8U +
                        4U + static_cast<size_t>(alphaCcBits) + alphaPayloadBits;
    if (used > static_cast<size_t>(INT_MAX)) continue;
    if (used <= static_cast<size_t>(getNumDataCodewords(ver)) * 8U)
    {
      version = ver;
      dataBits = static_cast<int>(used);
      break;
    }
  }
  if (version == 0)
  {
    setError(errorText, errorTextSize,
             "QR-Deep-Link ist fuer Version 40-M zu gross");
    return false;
  }

  memset(dataCodewords, 0, CODEWORD_BUFFER_SIZE);
  int bitLength = 0;

  // Segment 1: exact URI prefix in QR byte mode.
  if (!appendBits(dataCodewords, CODEWORD_BUFFER_SIZE, 0x4U, 4, bitLength) ||
      !appendBits(dataCodewords, CODEWORD_BUFFER_SIZE,
                  static_cast<unsigned int>(prefixLength),
                  byteCharCountBits(version), bitLength))
  {
    setError(errorText, errorTextSize,
             "QR-Deep-Link-Kopf konnte nicht erzeugt werden");
    return false;
  }
  for (size_t i = 0; i < prefixLength; ++i)
  {
    if (!appendBits(dataCodewords, CODEWORD_BUFFER_SIZE,
                    static_cast<unsigned int>(
                        static_cast<uint8_t>(bytePrefix[i])),
                    8, bitLength))
    {
      setError(errorText, errorTextSize,
               "QR-Deep-Link-Praefix ist zu gross");
      return false;
    }
  }

  // Segment 2: compact TP3C1/Base38 payload in QR alphanumeric mode.
  if (!appendBits(dataCodewords, CODEWORD_BUFFER_SIZE, 0x2U, 4, bitLength) ||
      !appendBits(dataCodewords, CODEWORD_BUFFER_SIZE,
                  static_cast<unsigned int>(textLength),
                  charCountBits(version), bitLength))
  {
    setError(errorText, errorTextSize,
             "TP3C1-Segmentkopf konnte nicht erzeugt werden");
    return false;
  }
  for (size_t i = 0; i + 1U < textLength; i += 2U)
  {
    const unsigned int value = static_cast<unsigned int>(
        alphaValue(alphanumericText[i]) * 45 +
        alphaValue(alphanumericText[i + 1U]));
    if (!appendBits(dataCodewords, CODEWORD_BUFFER_SIZE,
                    value, 11, bitLength))
    {
      setError(errorText, errorTextSize,
               "TP3C1-Nutzdaten sind zu gross");
      return false;
    }
  }
  if ((textLength & 1U) != 0U &&
      !appendBits(dataCodewords, CODEWORD_BUFFER_SIZE,
                  static_cast<unsigned int>(
                      alphaValue(alphanumericText[textLength - 1U])),
                  6, bitLength))
  {
    setError(errorText, errorTextSize,
             "TP3C1-Nutzdaten sind zu gross");
    return false;
  }
  if (bitLength != dataBits)
  {
    setError(errorText, errorTextSize,
             "QR-Deep-Link-Bitlaenge ist inkonsistent");
    return false;
  }

  return finishQr(version, bitLength, output, functionModules,
                  dataCodewords, interleavedCodewords, blockBuffer,
                  errorText, errorTextSize);
}

bool FLASHMEM getModule(const Matrix& matrix, int x, int y)
{
  return matrix.size > 0U && x >= 0 && y >= 0 && x < matrix.size && y < matrix.size
      && bitsetGet(matrix.modules, matrix.size, x, y);
}
}
