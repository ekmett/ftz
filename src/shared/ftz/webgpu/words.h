#pragma once
#include "ftz/config.h"


namespace ftz { namespace detail { namespace math {
  struct unsigned_word_product {
    unsigned int low, high;
  };
  // Independently derived base-2^16 long multiplication. Each limb product and
  // carry addition fits uint32; no native64 type, shift or intermediate is used.
  inline unsigned_word_product unsigned_multiply_words(unsigned int a, unsigned int b) {
    unsigned int a0 = a & 65535u, a1 = a >> 16;
    unsigned int b0 = b & 65535u, b1 = b >> 16;
    unsigned int low = a0 * b0;
    unsigned int middle0 = a1 * b0 + (low >> 16);
    unsigned int middle1 = a0 * b1 + (middle0 & 65535u);
    unsigned_word_product result;
    result.low = (middle1 << 16) | (low & 65535u);
    result.high = a1 * b1 + (middle0 >> 16) + (middle1 >> 16);
    return result;
  }


}}}

namespace ftz { namespace detail { namespace math {
  // Exact binary32 word operations for rare rounding-boundary repair and
  // large-angle reduction. RNE with signed post-round FTZ retains the minimum
  // normal result. Invalid/nonfinite input or overflow returns {0,0}.
  struct fp32_result { unsigned int bits, valid; };
  inline fp32_result fp32_result_of(unsigned int bits, unsigned int valid) {
    fp32_result r; r.bits = bits; r.valid = valid; return r;
  }
  inline unsigned int fp32_flush_word(unsigned int bits) {
    return (bits & 0x7fffffffu) < 0x00800000u ? bits & 0x80000000u : bits;
  }
  inline bool fp32_finite(unsigned int bits) {
    return (bits & 0x7f800000u) != 0x7f800000u;
  }
  inline unsigned_word_product word_pair(unsigned int low, unsigned int high) {
    unsigned_word_product r; r.low = low; r.high = high; return r;
  }
  inline bool word_pair_nonzero(unsigned_word_product a) { return (a.low | a.high) != 0u; }
  inline unsigned_word_product word_pair_add(unsigned_word_product a, unsigned_word_product b) {
    unsigned int low = a.low + b.low;
    return word_pair(low, a.high + b.high + (low < a.low ? 1u : 0u));
  }
  inline unsigned_word_product word_pair_sub(unsigned_word_product a, unsigned_word_product b) {
    return word_pair(a.low - b.low, a.high - b.high - (a.low < b.low ? 1u : 0u));
  }
  inline bool word_pair_less(unsigned_word_product a, unsigned_word_product b) {
    return a.high < b.high || (a.high == b.high && a.low < b.low);
  }
  inline unsigned_word_product word_pair_left(unsigned_word_product a, unsigned int shift) {
    if (shift == 0u) return a;
    if (shift < 32u) return word_pair(a.low << shift, (a.high << shift) | (a.low >> (32u - shift)));
    if (shift < 64u) return word_pair(0u, a.low << (shift - 32u));
    return word_pair(0u, 0u);
  }
  // Shift with a sticky low bit: discarded information affects rounding, never
  // the retained high value. Every variable shift is guarded against 32/64.
  inline unsigned_word_product word_pair_right_jam(unsigned_word_product a, unsigned int shift) {
    if (shift == 0u) return a;
    if (shift < 32u) {
      unsigned int low = (a.low >> shift) | (a.high << (32u - shift));
      return word_pair(low | ((a.low << (32u - shift)) != 0u ? 1u : 0u), a.high >> shift);
    }
    if (shift == 32u) return word_pair(a.high | (a.low != 0u ? 1u : 0u), 0u);
    if (shift < 64u) {
      unsigned int tail = a.low | (a.high << (64u - shift));
      return word_pair((a.high >> (shift - 32u)) | (tail != 0u ? 1u : 0u), 0u);
    }
    return word_pair(word_pair_nonzero(a) ? 1u : 0u, 0u);
  }
  inline unsigned int word_leading_zeros(unsigned int word) {
    if (word == 0u) return 32u;
    unsigned int count = 0u;
    if ((word & 0xffff0000u) == 0u) { word <<= 16; count += 16u; }
    if ((word & 0xff000000u) == 0u) { word <<= 8; count += 8u; }
    if ((word & 0xf0000000u) == 0u) { word <<= 4; count += 4u; }
    if ((word & 0xc0000000u) == 0u) { word <<= 2; count += 2u; }
    if ((word & 0x80000000u) == 0u) ++count;
    return count;
  }
  inline unsigned int word_pair_top(unsigned_word_product a) {
    return a.high != 0u ? 63u - word_leading_zeros(a.high) : 31u - word_leading_zeros(a.low);
  }
  // Magnitude represents value / 2^(exponent-61), with sufficient low rounding
  // information. Alignment can lose bits only when the operand exponents differ
  // by >14, which excludes deep cancellation; the final rounding shift then
  // discards the jam bit and keeps its sticky meaning.
  inline fp32_result fp32_pack(unsigned_word_product magnitude, int exponent, unsigned int sign) {
    if (!word_pair_nonzero(magnitude)) return fp32_result_of(sign, 1u);
    int top = (int)word_pair_top(magnitude);
    int biased = exponent - 61 + top + 127;
    if (biased >= 255) return fp32_result_of(0u, 0u);
    int shift = top - 23;
    int denormal_shift = -88 - exponent;
    if (shift < denormal_shift) shift = denormal_shift;
    unsigned int quotient;
    if (shift <= 0) {
      quotient = word_pair_left(magnitude, (unsigned int)(-shift)).low;
    } else if (shift == 1) {
      quotient = (magnitude.low >> 1) | (magnitude.high << 31);
      quotient += (magnitude.low & quotient & 1u);
    } else {
      unsigned int window = word_pair_right_jam(magnitude, (unsigned int)(shift - 2)).low;
      quotient = window >> 2;
      quotient += ((window & 2u) != 0u && ((window & 1u) != 0u || (quotient & 1u) != 0u)) ? 1u : 0u;
    }
    if (biased <= 0)
      return fp32_result_of(sign | (quotient >= 0x00800000u ? 0x00800000u : 0u), 1u);
    if (quotient == 0x01000000u) { quotient >>= 1; ++biased; }
    if (biased >= 255) return fp32_result_of(0u, 0u);
    return fp32_result_of(sign | ((unsigned int)biased << 23) | (quotient & 0x007fffffu), 1u);
  }

  inline fp32_result fp32_fma_words(unsigned int a, unsigned int b, unsigned int c) {
    if (!fp32_finite(a) || !fp32_finite(b) || !fp32_finite(c))
      return fp32_result_of(0u, 0u);
    a = fp32_flush_word(a); b = fp32_flush_word(b); c = fp32_flush_word(c);
    unsigned int sign = (a ^ b) & 0x80000000u;
    unsigned int csign = c & 0x80000000u;
    unsigned int ea = (a >> 23) & 255u, eb = (b >> 23) & 255u, ec = (c >> 23) & 255u;
    if (ea == 0u || eb == 0u)
      return fp32_result_of(ec == 0u ? sign & csign : c, 1u);
    unsigned int ma = (a & 0x7fffffu) | 0x800000u, mb = (b & 0x7fffffu) | 0x800000u;
    unsigned_word_product product = unsigned_multiply_words(ma, mb);
    unsigned int top = word_pair_top(product);
    int exponent = (int)ea + (int)eb - 300 + (int)top;
    product = word_pair_left(product, 61u - top);
    if (ec == 0u) return fp32_pack(product, exponent, sign);
    unsigned_word_product addend = word_pair_left(word_pair((c & 0x7fffffu) | 0x800000u, 0u), 38u);
    int ce = (int)ec - 127;
    if (exponent < ce) {
      product = word_pair_right_jam(product, (unsigned int)(ce - exponent));
      exponent = ce;
    } else {
      addend = word_pair_right_jam(addend, (unsigned int)(exponent - ce));
    }
    unsigned_word_product magnitude;
    if (sign == csign) {
      magnitude = word_pair_add(product, addend);
    } else if (word_pair_less(product, addend)) {
      magnitude = word_pair_sub(addend, product); sign = csign;
    } else {
      magnitude = word_pair_sub(product, addend);
      if (!word_pair_nonzero(magnitude)) sign = 0u;
    }
    return fp32_pack(magnitude, exponent, sign);
  }

}}}

/**
 * \file
 * \license
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
 * \endlicense
 * \author Edward Kmett <ekmett@gmail.com>
 * \brief Word-pair arithmetic and exact binary32 packing, multiplication, and FMA repair.
 */
