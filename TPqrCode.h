/*
 * TP-3000 QR Code generator adapter
 *
 * The QR construction algorithm is adapted from Project Nayuki's
 * QR Code generator library (MIT License). See THIRD_PARTY_NOTICES.md.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_QR_CODE_H
#define TP3000_QR_CODE_H

#include <stddef.h>
#include <stdint.h>

namespace TpQrCode
{
  static constexpr int MIN_VERSION = 1;
  static constexpr int MAX_VERSION = 40;
  static constexpr int MAX_SIZE = 177;
  static constexpr size_t MATRIX_BUFFER_SIZE = (MAX_SIZE * MAX_SIZE + 7U) / 8U;
  static constexpr size_t CODEWORD_BUFFER_SIZE = 3706U;
  static constexpr size_t BLOCK_BUFFER_SIZE = 3800U;

  struct Matrix
  {
    uint8_t size;
    uint8_t modules[MATRIX_BUFFER_SIZE];
  };

  // Encodes text in QR alphanumeric mode with error correction level M,
  // versions 1..40 and automatic mask selection. The input character set is
  // "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:".
  bool encodeAlphanumericMedium(const char* text,
                                Matrix& output,
                                uint8_t functionModules[MATRIX_BUFFER_SIZE],
                                uint8_t dataCodewords[CODEWORD_BUFFER_SIZE],
                                uint8_t interleavedCodewords[CODEWORD_BUFFER_SIZE],
                                uint8_t blockBuffer[BLOCK_BUFFER_SIZE],
                                char* errorText,
                                size_t errorTextSize);

  // Encodes a canonical TP-3000 deep link as two QR segments with error
  // correction level M and automatic mask selection:
  //   1. byte mode for e.g. "tp3000://verify#"
  //   2. alphanumeric mode for "TP3C1:<Base38>"
  // Splitting the URI this way preserves the exact lowercase deep-link
  // prefix while keeping the large TP3C1 payload compact.
  bool encodeBytePrefixAlphanumericMedium(
      const char* bytePrefix,
      const char* alphanumericText,
      Matrix& output,
      uint8_t functionModules[MATRIX_BUFFER_SIZE],
      uint8_t dataCodewords[CODEWORD_BUFFER_SIZE],
      uint8_t interleavedCodewords[CODEWORD_BUFFER_SIZE],
      uint8_t blockBuffer[BLOCK_BUFFER_SIZE],
      char* errorText,
      size_t errorTextSize);

  bool getModule(const Matrix& matrix, int x, int y);
}

#endif
