/*
 * Small zlib/DEFLATE encoder for TP-3000 QR transport.
 * Emits one fixed-Huffman DEFLATE block and a standards-compliant zlib wrapper.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef TP3000_ZLIB_DEFLATE_H
#define TP3000_ZLIB_DEFLATE_H

#include <stddef.h>
#include <stdint.h>

namespace TpZlibDeflate
{
  bool compressFixed(const uint8_t* input,
                     size_t inputLength,
                     uint8_t* output,
                     size_t outputCapacity,
                     size_t* outputLength);
}

#endif
