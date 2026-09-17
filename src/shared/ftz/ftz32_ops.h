#pragma once
#include "ftz/config.h"
#include "simd/attributes.h"
#include "ftz/math/approx.h"
#include "ftz/math/float.h"
#include "ftz/math/words.h"

// One binary32 value contract, shared by scalar, SIMD repair lanes and HLSL.
// The caller supplies normal, signed zero, infinity or any NaN operands.
// NaN signs/payloads are outside the contract. Finite arithmetic needs RNE/FMA.
// Compiler contraction of ordinary a*b+c is disabled by the build configuration.
namespace ftz { namespace detail {
  static const unsigned int ftz32_nan = 0x7fc00000u;
  static const unsigned int ftz32_infinity = 0x7f800000u;
  static const unsigned int ftz32_sign = 0x80000000u;

  simd_nodiscard simd_constexpr simd_inline simd_const unsigned int ftz32_canonical(unsigned int bits) {
    unsigned int magnitude = bits & 0x7fffffffu;
    // Import normalization is mandatory even on DAZ hardware: integer loads,
    // comparisons, stores and casts observe the original bits without arithmetic.
    return magnitude < 0x00800000u ? bits & ftz32_sign : bits;
  }
  simd_nodiscard simd_constexpr simd_inline simd_const bool ftz32_isnan(unsigned int bits) {
    return (bits & 0x7fffffffu) > ftz32_infinity;
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_finish(unsigned int bits) {
    return ::ftz::detail::math::fp32_encode(::ftz::detail::math::fp32_ftz<Hardware>(::ftz::detail::math::fp32_decode(bits)));
  }
  simd_nodiscard simd_constexpr simd_inline simd_const unsigned int ftz32_neg(unsigned int a) {
    return a ^ ftz32_sign;
  }
  simd_nodiscard simd_constexpr simd_inline simd_const unsigned int ftz32_abs(unsigned int a) { return a & 0x7fffffffu; }

  template <bool HardwareFtz>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_add_policy(unsigned int a, unsigned int b) {
#ifdef __cplusplus
    float value = ::ftz::detail::math::fp32_decode(a) + ::ftz::detail::math::fp32_decode(b);
#else
    precise float value = asfloat(a) + asfloat(b);
#endif
    unsigned int bits = ::ftz::detail::math::fp32_encode(value);
    // Finite operands are integer multiples of 2^-149. A sum below minimum
    // normal is exact, so signed hardware FTZ needs no rounding-boundary repair.
    // No NaN classification: every NaN sign and payload is outside the contract.
    if (HardwareFtz) return bits;
    if ((bits & 0x7fffffffu) < 0x00800000u) {
      unsigned int aa = a & 0x7fffffffu, bb = b & 0x7fffffffu;
      return (aa > bb ? a : aa < bb ? b : a & b) & ftz32_sign;
    }
    return bits;
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_add(unsigned int a, unsigned int b) {
    // Hardware admission includes signed flushing of tiny add/sub results.
    return ftz32_add_policy<Hardware>(a, b);
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_sub(unsigned int a, unsigned int b) {
    if (Hardware) {
#ifdef __cplusplus
      return ::ftz::detail::math::fp32_encode(::ftz::detail::math::fp32_decode(a) - ::ftz::detail::math::fp32_decode(b));
#else
      precise float value = asfloat(a) - asfloat(b);
      return asuint(value);
#endif
    }
    return ftz32_add<Hardware>(a, ftz32_neg(b));
  }
  simd_nodiscard simd_inline simd_const unsigned int ftz32_mul(unsigned int a, unsigned int b) {
    float value = ::ftz::detail::math::fp32_mul<false>(::ftz::detail::math::fp32_decode(a), ::ftz::detail::math::fp32_decode(b));
    unsigned int bits = ::ftz::detail::math::fp32_encode(value);
    if ((bits & 0x7fffffffu) > 0x00800000u) return bits;
    unsigned int sign = (a ^ b) & ftz32_sign;
    unsigned int ea = (a >> 23) & 255u, eb = (b >> 23) & 255u;
    if (ea == 0u || eb == 0u || ea + eb < 127u) return sign;
    if (ea + eb > 127u) return sign | 0x00800000u;
    // At exponent sum 127, test the exact minnormal RNE midpoint. The
    // normalized fused residual is zero at the tie and otherwise at least
    // 2^-46 in magnitude, so neither rounding nor FTZ can change its sign.
    float ma = ::ftz::detail::math::fp32_decode((a & 0x007fffffu) | 0x3f800000u);
    float mb = ::ftz::detail::math::fp32_decode((b & 0x007fffffu) | 0x3f800000u);
    float residual = ::ftz::detail::math::fp32_fma<false>(ma, mb,
      ::ftz::detail::math::fp32_decode(0xbfffffffu));
    return sign | (residual >= 0.0f ? 0x00800000u : 0u);
  }
  simd_nodiscard simd_inline simd_const unsigned int ftz32_fma(unsigned int a, unsigned int b, unsigned int c) {
    float value = ::ftz::detail::math::fp32_fma<false>(::ftz::detail::math::fp32_decode(a),
      ::ftz::detail::math::fp32_decode(b), ::ftz::detail::math::fp32_decode(c));
    unsigned int bits = ::ftz::detail::math::fp32_encode(value);
    // Native FTZ profiles disagree at the minimum-normal rounding boundary.
    // Reconstruct only this rare region; a noop flush cannot repair it.
    if ((bits & 0x7fffffffu) <= 0x00800000u)
      return ::ftz::detail::math::fp32_fma_words(a, b, c).bits;
    return bits;
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_div(unsigned int a, unsigned int b) {
    unsigned int aa = a & 0x7fffffffu, bb = b & 0x7fffffffu;
    unsigned int sign = (a ^ b) & ftz32_sign;
    if (aa > ftz32_infinity || bb > ftz32_infinity ||
        (aa == 0u && bb == 0u) || (aa == ftz32_infinity && bb == ftz32_infinity))
      return ftz32_nan;
    if (aa == ftz32_infinity || bb == 0u) return sign | ftz32_infinity;
    if (bb == ftz32_infinity || aa == 0u) return sign;
    ::ftz::detail::math::approx_result r = ::ftz::detail::math::approx_div_prechecked_words(a, b);
    return r.valid != 0u ? ftz32_finish<Hardware>(r.bits) : sign | ftz32_infinity;
  }
  simd_nodiscard simd_inline simd_const unsigned int ftz32_sqrt(unsigned int a) {
    unsigned int magnitude = a & 0x7fffffffu;
    if (magnitude == 0u) return a;
    if ((a & ftz32_sign) != 0u || magnitude > ftz32_infinity) return ftz32_nan;
    if (magnitude == ftz32_infinity) return a;
    return ::ftz::detail::math::approx_sqrt_prechecked_words(a).bits;
  }
  simd_nodiscard simd_constexpr simd_inline simd_const bool ftz32_equal(unsigned int a, unsigned int b) {
    return !ftz32_isnan(a) && !ftz32_isnan(b) &&
      (a == b || ((a | b) & 0x7fffffffu) == 0u);
  }
  simd_nodiscard simd_constexpr simd_inline simd_const bool ftz32_less(unsigned int a, unsigned int b) {
    if (ftz32_isnan(a) || ftz32_isnan(b) || ftz32_equal(a, b)) return false;
    if (((a ^ b) & ftz32_sign) != 0u) return (a & ftz32_sign) != 0u;
    return (a & ftz32_sign) != 0u ? a > b : a < b;
  }
}}

#include "ftz/math/atan2.h"
#include "ftz/math/tanh.h"
#include "ftz/math/log.h"
#ifdef __cplusplus
#include "ftz/math.h"
#else
#include "ftz/math.h"
#endif
namespace ftz { namespace detail {
  struct ftz32_trig_fraction { unsigned int words[9]; };
  struct ftz32_trig_reduction { unsigned int residual, quadrant; };
  simd_nodiscard simd_inline simd_const unsigned int ftz32_trig_window(ftz32_trig_fraction p, unsigned int shift) {
    unsigned int word=shift/32u, bit=shift%32u;
    if(word>=9u)return 0u;
    unsigned int result=p.words[word]>>bit;
    if(bit!=0u && word+1u<9u)result|=p.words[word+1u]<<(32u-bit);
    return result;
  }
  // Finite positive magnitude >=8192. A 256-bit fixed 2/pi and 24-bit input
  // significand cover every binary32 exponent using uint32 limbs only.
  // Truncation contributes less than 2^-128 turns at the largest finite input.
  simd_nodiscard simd_inline simd_const ftz32_trig_reduction ftz32_trig_reduce(unsigned int magnitude) {
    const unsigned int two_over_pi[8]={0xdebbc561u,0xfe5163abu,0x3c439041u,0xdb629599u,0xf534ddc0u,0xfc2757d1u,0x4e441529u,0xa2f9836eu};
    unsigned int mantissa=(magnitude&0x007fffffu)|0x00800000u;
    ftz32_trig_fraction p;
    unsigned int carry=0u;
    for(unsigned int i=0u;i<8u;++i){
      ::ftz::detail::math::unsigned_word_product product=::ftz::detail::math::unsigned_multiply_words(mantissa,two_over_pi[i]);
      p.words[i]=product.low+carry;
      carry=product.high+(p.words[i]<product.low?1u:0u);
    }
    p.words[8]=carry;
    unsigned int shift=406u-(magnitude>>23);
    unsigned int quadrant=ftz32_trig_window(p,shift)&3u;
    bool negative=(ftz32_trig_window(p,shift-1u)&1u)!=0u;
    if(negative)quadrant=(quadrant+1u)&3u;
    unsigned int limb=shift/32u, bit=shift%32u;
    for(unsigned int j=limb+1u;j<9u;++j)p.words[j]=0u;
    p.words[limb]&=bit==0u?0u:(1u<<bit)-1u;
    if(negative){
      unsigned int add=1u;
      for(unsigned int j=0u;j<=limb;++j){
        unsigned int inverted=~p.words[j];
        p.words[j]=inverted+add;add=(p.words[j]<inverted)?1u:0u;
      }
      p.words[limb]&=bit==0u?0u:(1u<<bit)-1u;
    }
    int top=-1;
    for(int j=8;j>=0;--j)if(p.words[j]!=0u){top=j*32+31-(int)::ftz::detail::math::word_leading_zeros(p.words[j]);break;}
    ftz32_trig_reduction result;result.quadrant=quadrant;result.residual=0u;
    if(top<0)return result;
    ::ftz::detail::math::unsigned_word_product packed;
    if(top>61){
      unsigned int discarded_bits=(unsigned int)top-61u;
      packed=::ftz::detail::math::word_pair(ftz32_trig_window(p,discarded_bits),ftz32_trig_window(p,discarded_bits+32u));
      bool sticky=false;
      for(unsigned int j=0u;j<discarded_bits/32u;++j)sticky=sticky||p.words[j]!=0u;
      if(discarded_bits%32u!=0u)sticky=sticky||(p.words[discarded_bits/32u]&((1u<<(discarded_bits%32u))-1u))!=0u;
      packed.low|=sticky?1u:0u;
    }else packed=::ftz::detail::math::word_pair_left(::ftz::detail::math::word_pair(p.words[0],p.words[1]),61u-(unsigned int)top);
    unsigned int fraction=::ftz::detail::math::fp32_pack(packed,top-(int)shift,negative?0x80000000u:0u).bits;
    result.residual=ftz32_mul(fraction,0x3fc90fdbu);
    return result;
  }
}}

namespace ftz { namespace detail {
  struct ftz32_sincos_bits { unsigned int sine, cosine; };
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const ftz32_sincos_bits ftz32_sincos(unsigned int bits) {
    ftz32_sincos_bits result;
    unsigned int magnitude=bits&0x7fffffffu;
    if(magnitude>=ftz32_infinity){result.sine=result.cosine=ftz32_nan;return result;}
    bool large=magnitude>=0x46000000u;
    ftz32_trig_reduction reduced;reduced.residual=bits;reduced.quadrant=0u;
    if(large)reduced=ftz32_trig_reduce(magnitude);
    float x=::ftz::detail::math::fp32_decode(reduced.residual);
#ifdef __cplusplus
    auto [sine_value, cosine_value]=large?ftz::detail::native::sincos_reduced_ftz<Hardware>(x):ftz::detail::native::sincos_ftz<Hardware>(x);
    unsigned int sine=::ftz::detail::math::fp32_encode(sine_value),cosine=::ftz::detail::math::fp32_encode(cosine_value);
#else
    ::ftz::detail::math::sincos_result pair;
    if(large)pair=::ftz::detail::math::sincos_reduced_ftz(x);else pair=::ftz::detail::math::sincos_ftz(x);
    unsigned int sine=::ftz::detail::math::fp32_encode(pair.sine),cosine=::ftz::detail::math::fp32_encode(pair.cosine);
#endif
    if(large){
      result.sine=(reduced.quadrant&1u)!=0u?cosine:sine;
      result.cosine=(reduced.quadrant&1u)!=0u?sine:cosine;
      result.sine^=((reduced.quadrant&2u)!=0u?ftz32_sign:0u)^(bits&ftz32_sign);
      result.cosine^=(((reduced.quadrant+1u)&2u)!=0u?ftz32_sign:0u);
    }else{result.sine=sine;result.cosine=cosine;}
    return result;
  }
#ifdef __cplusplus
  // A known scalar quadrant selects one reduced polynomial, then its sign.
  template <bool Cosine, bool Hardware>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_trig_single(unsigned int bits) {
    unsigned int magnitude=bits&0x7fffffffu;
    if(magnitude>=ftz32_infinity)return ftz32_nan;
    if(magnitude<0x46000000u) {
      float x=::ftz::detail::math::fp32_decode(bits);
      if constexpr(Cosine) return ::ftz::detail::math::fp32_encode(native::cos_ftz<Hardware>(x));
      else return ::ftz::detail::math::fp32_encode(native::sin_ftz<Hardware>(x));
    }
    auto reduced=ftz32_trig_reduce(magnitude);
    float x=::ftz::detail::math::fp32_decode(reduced.residual);
    bool odd=(reduced.quadrant&1u)!=0u;
    float value;
    if constexpr(Cosine) value=odd?native::sin_reduced_ftz<Hardware>(x):native::cos_reduced_ftz<Hardware>(x);
    else value=odd?native::cos_reduced_ftz<Hardware>(x):native::sin_reduced_ftz<Hardware>(x);
    unsigned int result=::ftz::detail::math::fp32_encode(value);
    if constexpr(Cosine) return result^(((reduced.quadrant+1u)&2u)!=0u?ftz32_sign:0u);
    else return result^(((reduced.quadrant&2u)!=0u?ftz32_sign:0u)^(bits&ftz32_sign));
  }
#endif
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_sin(unsigned int bits){
#ifdef __cplusplus
    return ftz32_trig_single<false,Hardware>(bits);
#else
    return ftz32_sincos<Hardware>(bits).sine;
#endif
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_cos(unsigned int bits){
#ifdef __cplusplus
    return ftz32_trig_single<true,Hardware>(bits);
#else
    return ftz32_sincos<Hardware>(bits).cosine;
#endif
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_tanh(unsigned int bits){return ::ftz::detail::math::tanh_words<Hardware>(bits);}
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_log(unsigned int bits){return ::ftz::detail::math::log_words<Hardware>(bits).bits;}
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_log1p(unsigned int bits){return ::ftz::detail::math::log1p_words<Hardware>(bits).bits;}
  simd_nodiscard simd_inline simd_const unsigned int ftz32_exp(unsigned int bits){
#ifdef __cplusplus
    return ::ftz::detail::math::fp32_encode(simd::exp(
      ftz::detail::native::fp32x1(::ftz::detail::math::fp32_decode(bits)), std::true_type{}).value);
#else
    unsigned int magnitude=bits&0x7fffffffu;
    if(magnitude>ftz32_infinity)return ftz32_nan;
    if(magnitude==ftz32_infinity)return (bits&ftz32_sign)!=0u?0u:ftz32_infinity;
    return ::ftz::detail::math::exp_value(bits).x;
#endif
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_expm1(unsigned int bits){
    unsigned int magnitude=bits&0x7fffffffu;
    if(magnitude>ftz32_infinity)return ftz32_nan;
    if(magnitude==ftz32_infinity)return (bits&ftz32_sign)!=0u?0xbf800000u:ftz32_infinity;
    if((bits&ftz32_sign)==0u && magnitude>0x3f800000u)return ftz32_sub<Hardware>(ftz32_exp(bits),0x3f800000u);
#ifdef __cplusplus
    return ::ftz::detail::math::fp32_encode(ftz::detail::native::expm1_checked<Hardware>(::ftz::detail::math::fp32_decode(bits)).value);
#else
    return ::ftz::detail::math::fp32_encode(::ftz::detail::math::expm1_checked(::ftz::detail::math::fp32_decode(bits)).value);
#endif
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  simd_nodiscard simd_inline simd_const unsigned int ftz32_atan2(unsigned int y,unsigned int x){
    unsigned int ay=y&0x7fffffffu,ax=x&0x7fffffffu,sign=y&ftz32_sign;
    if(ay>ftz32_infinity || ax>ftz32_infinity)return ftz32_nan;
    if(ay==0u)return ((x&ftz32_sign)!=0u?0x40490fdbu:0u)|sign;
    if(ay==ftz32_infinity){
      if(ax==ftz32_infinity)return ((x&ftz32_sign)!=0u?0x4016cbe4u:0x3f490fdbu)|sign;
      return 0x3fc90fdbu|sign;
    }
    if(ax==ftz32_infinity)return ((x&ftz32_sign)!=0u?0x40490fdbu:0u)|sign;
    return ::ftz::detail::math::atan2_words<Hardware>(y,x).bits;
  }
}}

/**
 * \file
 * \license
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
 * \endlicense
 * \author Edward Kmett <ekmett@gmail.com>
 * \brief Shared FTZ scalar operators and math, including special values and full-range trig reduction.
 */
