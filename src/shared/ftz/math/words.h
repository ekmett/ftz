#pragma once
#include "ftz/config.h"

// HLSL consumers must enable and admit shaderInt64 explicitly. The WebGPU
// compatible path remains 32-bit; native C++ uses uint64_t unconditionally.
#ifndef FTZ_SHADER_INT64
#define FTZ_SHADER_INT64 0
#endif
#if FTZ_SHADER_INT64 != 0 && FTZ_SHADER_INT64 != 1
#error "FTZ_SHADER_INT64 must be 0 or 1"
#endif
#if !defined(__cplusplus) && !FTZ_SHADER_INT64
#include "ftz/webgpu_words.h"
#else
#ifdef __cplusplus
#include <bit>
#include <cstdint>
#endif

namespace ftz { namespace detail { namespace math {
#ifdef __cplusplus
  using std::uint64_t;
#endif
  // The large-angle reducer consumes 32-bit limbs at this boundary.
  struct unsigned_word_product { unsigned int low, high; };
  inline unsigned_word_product word_pair(unsigned int low, unsigned int high) {
    unsigned_word_product r; r.low = low; r.high = high; return r;
  }
  inline unsigned_word_product word_pair(uint64_t value) {
    return word_pair((unsigned int)value, (unsigned int)(value >> 32));
  }
  inline uint64_t word_value(unsigned_word_product value) {
    return (uint64_t(value.high) << 32) | uint64_t(value.low);
  }
  inline unsigned_word_product unsigned_multiply_words(unsigned int a, unsigned int b) {
    return word_pair(uint64_t(a) * uint64_t(b));
  }
  inline unsigned_word_product word_pair_left(unsigned_word_product value, unsigned int shift) {
    return word_pair(shift < 64u ? word_value(value) << shift : uint64_t(0));
  }
  inline unsigned int word_leading_zeros(unsigned int value) {
#ifdef __cplusplus
    return (unsigned int)std::countl_zero(value);
#else
    return value == 0u ? 32u : 31u - (unsigned int)firstbithigh(value);
#endif
  }
  inline unsigned int word_top(uint64_t value) {
#ifdef __cplusplus
    return 63u - (unsigned int)std::countl_zero(value);
#else
    // DXC SPIR-V supports firstbithigh only for 32-bit components.
    unsigned int high = (unsigned int)(value >> 32);
    return high != 0u ? 63u - word_leading_zeros(high)
      : 31u - word_leading_zeros((unsigned int)value);
#endif
  }
  // Discarded bits contribute one sticky bit. No shift reaches 64.
  inline uint64_t word_right_jam(uint64_t value, unsigned int shift) {
    if (shift == 0u) return value;
    if (shift >= 64u) return uint64_t(value != 0);
    return (value >> shift) | uint64_t((value << (64u - shift)) != 0);
  }
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
  // magnitude * 2^(exponent-61), rounded to nearest-even then signed FTZ.
  // Alignment jams only when exponent differences exclude deep cancellation.
  // The final rounding shift discards the jam bit but preserves its meaning.
  inline fp32_result fp32_pack(uint64_t magnitude, int exponent, unsigned int sign) {
    if (magnitude == 0) return fp32_result_of(sign, 1u);
    int top = (int)word_top(magnitude);
    int biased = exponent - 61 + top + 127;
    if (biased >= 255) return fp32_result_of(0u, 0u);
    int shift = top - 23;
    int denormal_shift = -88 - exponent;
    if (shift < denormal_shift) shift = denormal_shift;
    unsigned int quotient;
    if (shift <= 0) {
      quotient = (unsigned int)(magnitude << (unsigned int)(-shift));
    } else if (shift == 1) {
      quotient = (unsigned int)(magnitude >> 1);
      quotient += ((unsigned int)magnitude & quotient & 1u);
    } else {
      unsigned int window = (unsigned int)word_right_jam(magnitude, (unsigned int)(shift - 2));
      quotient = window >> 2;
      quotient += ((window & 2u) != 0u && ((window & 1u) != 0u || (quotient & 1u) != 0u)) ? 1u : 0u;
    }
    if (biased <= 0)
      return fp32_result_of(sign | (quotient >= 0x00800000u ? 0x00800000u : 0u), 1u);
    if (quotient == 0x01000000u) { quotient >>= 1; ++biased; }
    if (biased >= 255) return fp32_result_of(0u, 0u);
    return fp32_result_of(sign | ((unsigned int)biased << 23) | (quotient & 0x007fffffu), 1u);
  }
  inline fp32_result fp32_pack(unsigned_word_product magnitude, int exponent, unsigned int sign) {
    return fp32_pack(word_value(magnitude), exponent, sign);
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
    uint64_t product = uint64_t(ma) * uint64_t(mb);
    unsigned int top = word_top(product);
    int exponent = (int)ea + (int)eb - 300 + (int)top;
    product <<= 61u - top;
    if (ec == 0u) return fp32_pack(product, exponent, sign);
    uint64_t addend = uint64_t((c & 0x7fffffu) | 0x800000u) << 38;
    int ce = (int)ec - 127;
    if (exponent < ce) {
      product = word_right_jam(product, (unsigned int)(ce - exponent));
      exponent = ce;
    } else {
      addend = word_right_jam(addend, (unsigned int)(exponent - ce));
    }
    uint64_t magnitude;
    if (sign == csign) {
      magnitude = product + addend;
    } else if (product < addend) {
      magnitude = addend - product; sign = csign;
    } else {
      magnitude = product - addend;
      if (magnitude == 0) sign = 0u;
    }
    return fp32_pack(magnitude, exponent, sign);
  }
}}}
#endif

/**
 * \file
 * \license
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
 * \endlicense
 * \author Edward Kmett <ekmett@gmail.com>
 * \brief Native 64-bit exact binary32 packing, multiplication, and FMA repair.
 */
