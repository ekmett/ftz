#pragma once
#include "ftz/config.h"
#include "simd/attributes.h"
#include "ftz/math/float.h"
namespace ftz { namespace detail { namespace math {
  struct log_result { unsigned int bits, valid; };
  // Precondition: finite x in [-.5,1], zero or canonical normal. Tiny input
  // returns its original bits before squaring. Fixed x+x*x*P(x) graph; no divide.
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const float log1p_kernel(float x) {
    unsigned int word = fp32_encode(x);
    if ((word & 0x7fffffffu) <= 0x33000000u) return x;
    float z = fp32_mul<true,Hardware>(x,x);
    float t,h;
    if ((word & 0x80000000u) != 0u) {
      t = fp32_fma<true,Hardware>(x,fp32_decode(0x40800000u),fp32_decode(0x3f800000u));
      h = fp32_decode(0xb44f5480u);
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x352754efu));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xb5bb75dbu));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x369a1c19u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xb7866f43u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x3861235au));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xb93e98dfu));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x3a24a041u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xbb117f6au));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x3c04b7c5u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xbcfda364u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x3e029133u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xbf1a5884u));
    }
    else {
      t = fp32_fma<true,Hardware>(x,fp32_decode(0x40000000u),fp32_decode(0xbf800000u));
      h = fp32_decode(0xb29c7ee2u);
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x3378ea39u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xb3faaccbu));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x34c9e1cdu));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xb5b13b5eu));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x36902a0au));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xb76af011u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x38423d8au));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xb9225d51u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x3a0988b0u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xbaed1a41u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x3bd13ce0u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xbcbeef90u));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0x3db786beu));
      h = fp32_fma<true,Hardware>(h,t,fp32_decode(0xbec19b82u));
    }
    return fp32_fma<true,Hardware>(z,h,x);
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const log_result log_words(unsigned int word) {
    unsigned int magnitude = word & 0x7fffffffu;
    log_result result;result.bits=0x7fc00000u;result.valid=0u;
    if (magnitude > 0x7f800000u) return result;
    if (magnitude < 0x00800000u) {
      result.bits=0xff800000u;result.valid=1u;return result;
    }
    if ((word & 0x80000000u) != 0u) return result;
    result.valid=1u;
    if (magnitude == 0x7f800000u) {result.bits=word;return result;}
    int exponent=(int)(word >> 23)-127;
    unsigned int mantissa=(word & 0x007fffffu) | 0x3f800000u;
    if (mantissa >= 0x3fc00000u) {mantissa-=0x00800000u;exponent+=1;}
    // m in [.75,1.5): m-1 is exact by Sterbenz, including the neighborhood of1.
    float r=fp32_add<true,Hardware>(fp32_decode(mantissa),-1.0f);
    float p=log1p_kernel<Hardware>(r);
    float e=(float)exponent;
    // ln2_hi clears the low8 significand bits; ln2_low is the rounded residue.
    float low=fp32_fma<true,Hardware>(e,fp32_decode(0x35bfbe8eu),p);
    result.bits=fp32_encode(fp32_fma<true,Hardware>(e,fp32_decode(0x3f317200u),low));
    return result;
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const log_result log1p_words(unsigned int word) {
    unsigned int magnitude=word & 0x7fffffffu;
    bool negative=(word & 0x80000000u) != 0u;
    log_result result;result.bits=0x7fc00000u;result.valid=0u;
    if (magnitude > 0x7f800000u || (negative && magnitude > 0x3f800000u)) return result;
    result.valid=1u;
    if (magnitude < 0x00800000u) {result.bits=word & 0x80000000u;return result;}
    if (negative && magnitude == 0x3f800000u) {result.bits=0xff800000u;return result;}
    if (magnitude == 0x7f800000u) {result.bits=word;return result;}
    if (magnitude <= 0x33000000u) {result.bits=word;return result;}
    float x=fp32_decode(word);
    if ((!negative && magnitude <= 0x3f800000u) || (negative && magnitude <= 0x3f000000u)) {
      result.bits=fp32_encode(log1p_kernel<Hardware>(x));return result;
    }
    // Negative outer inputs use an exact Sterbenz sum. For x>1 this rounded
    // sum is part of the declared approximation, not a correctly-rounded log1p.
    return log_words<Hardware>(fp32_encode(fp32_add<true,Hardware>(1.0f,x)));
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const float log(float value) {
    return fp32_decode(log_words<Hardware>(fp32_encode(value)).bits);
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const float log1p(float value) {
    return fp32_decode(log1p_words<Hardware>(fp32_encode(value)).bits);
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
 * \brief Deterministic polynomial log and log1p.
 */
