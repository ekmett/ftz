#pragma once
#include "ftz/config.h"
#include "ftz/math/float.h"
namespace ftz { namespace detail { namespace math {
  struct approx_result { unsigned int bits, valid; };
  inline unsigned int approx_flush(unsigned int x) {
    return (x & 0x7f800000u) == 0u ? x & 0x80000000u : x;
  }
  inline bool approx_finite(unsigned int x) { return (x & 0x7fffffffu) < 0x7f800000u; }
  // Scale a positive normal intermediate by a power of two without floating
  // underflow. RNE at the minnormal boundary precedes signed output FTZ.
  inline approx_result approx_scale(unsigned int word, int shift, unsigned int sign) {
    approx_result r; r.bits=0u; r.valid=1u;
    int exponent=(int)(word>>23)+shift;
    if (exponent>=255) { r.valid=0u; return r; }
    if (exponent<=0) {
      r.bits=sign | ((exponent==0 && (word&0x007fffffu)==0x007fffffu) ? 0x00800000u : 0u);
      return r;
    }
    r.bits=sign | ((unsigned int)exponent<<23) | (word&0x007fffffu);
    return r;
  }
  // The graph's normalized operands stay normal under every supported FTZ mode.
  // It intentionally approximates the mathematical operation; version 1 is not
  // an exact-RNE division/square-root contract or a hardware-estimate wrapper.
  inline approx_result approx_apply(unsigned int operation, unsigned int a, unsigned int b) {
    approx_result invalid; invalid.bits=invalid.valid=0u;
    if (operation>3u || !approx_finite(a) ||
        (operation==3u ? !approx_finite(b) : b!=0u)) return invalid;
    a=approx_flush(a); b=approx_flush(b);
    unsigned int aa=a&0x7fffffffu, bb=b&0x7fffffffu;
    unsigned int sign=a&0x80000000u;
    if (operation==0u && aa==0u) return invalid;
    if ((operation==1u || operation==2u) && sign!=0u && aa!=0u) return invalid;
    if (operation==2u && aa==0u) return invalid;
    if (operation==3u && bb==0u) return invalid;
    if ((operation==1u || operation==3u) && aa==0u) {
      approx_result result;
      result.bits=operation==1u ? a : (a^b)&0x80000000u;
      result.valid=1u;
      return result;
    }
    int exponent_a=(int)(aa>>23)-127;
    unsigned int ma=0x3f800000u | (aa&0x007fffffu);
    float r, p, e, h;
    int shift;
    if (operation==0u || operation==3u) {
      unsigned int mb=operation==3u ? 0x3f800000u|(bb&0x007fffffu) : ma;
      float m=fp32_decode(mb);
      r=fp32_decode(0x7ef311c3u-mb);
      e=fp32_fma<false>(-m,r,1.0f); r=fp32_fma<false>(r,e,r);
      e=fp32_fma<false>(-m,r,1.0f); r=fp32_fma<false>(r,e,r);
      e=fp32_fma<false>(-m,r,1.0f); r=fp32_fma<false>(r,e,r);
      if (operation==3u) {
        r=fp32_mul<false>(fp32_decode(ma),r);
        shift=exponent_a-((int)(bb>>23)-127); sign=(a^b)&0x80000000u;
      } else shift=-exponent_a;
    } else {
      unsigned int parity=1u-((aa>>23)&1u);
      unsigned int mword=ma+(parity<<23);
      float m=fp32_decode(mword);
      r=fp32_decode(0x5f375a86u-(mword>>1));
      p=fp32_mul<false>(m,r); e=fp32_fma<false>(-p,r,1.0f); h=fp32_mul<false>(0.5f,r); r=fp32_fma<false>(h,e,r);
      p=fp32_mul<false>(m,r); e=fp32_fma<false>(-p,r,1.0f); h=fp32_mul<false>(0.5f,r); r=fp32_fma<false>(h,e,r);
      p=fp32_mul<false>(m,r); e=fp32_fma<false>(-p,r,1.0f); h=fp32_mul<false>(0.5f,r); r=fp32_fma<false>(h,e,r);
      shift=(exponent_a-(int)parity)/2;
      if (operation==1u) r=fp32_mul<false>(m,r); else shift=-shift;
      sign=0u;
    }
    return approx_scale(fp32_encode(r),shift,sign);
  }
  inline approx_result approx_reciprocal(unsigned int a) { return approx_apply(0u,a,0u); }
  inline approx_result approx_sqrt(unsigned int a) { return approx_apply(1u,a,0u); }
  inline approx_result approx_rsqrt(unsigned int a) { return approx_apply(2u,a,0u); }
  inline approx_result approx_divide(unsigned int a,unsigned int b) { return approx_apply(3u,a,b); }
  // Internal precondition: finite normal or signed-zero operands.
  inline approx_result approx_div_prechecked_words(unsigned int a, unsigned int b) {
    unsigned int aa=a&0x7fffffffu, bb=b&0x7fffffffu;
    approx_result result; result.bits=result.valid=0u;
    if (bb==0u) return result;
    unsigned int sign=(a^b)&0x80000000u;
    if (aa==0u) { result.bits=sign; result.valid=1u; return result; }
    unsigned int ma=0x3f800000u|(aa&0x007fffffu);
    unsigned int mb=0x3f800000u|(bb&0x007fffffu);
    float m=fp32_decode(mb);
    float r=fp32_decode(0x7ef311c3u-mb);
    float e=fp32_fma<false>(-m,r,1.0f); r=fp32_fma<false>(r,e,r);
    e=fp32_fma<false>(-m,r,1.0f); r=fp32_fma<false>(r,e,r);
    e=fp32_fma<false>(-m,r,1.0f); r=fp32_fma<false>(r,e,r);
    r=fp32_mul<false>(fp32_decode(ma),r);
    int shift=((int)(aa>>23)-127)-((int)(bb>>23)-127);
    return approx_scale(fp32_encode(r),shift,sign);
  }
  inline approx_result approx_sqrt_prechecked_words(unsigned int a) {
    unsigned int aa=a&0x7fffffffu;
    approx_result result; result.bits=result.valid=0u;
    if (aa==0u) { result.bits=a; result.valid=1u; return result; }
    if ((a&0x80000000u)!=0u) return result;
    unsigned int ma=0x3f800000u|(aa&0x007fffffu);
    unsigned int parity=1u-((aa>>23)&1u);
    unsigned int mword=ma+(parity<<23);
    float m=fp32_decode(mword);
    float r=fp32_decode(0x5f375a86u-(mword>>1));
    float p=fp32_mul<false>(m,r), e=fp32_fma<false>(-p,r,1.0f), h=fp32_mul<false>(0.5f,r); r=fp32_fma<false>(h,e,r);
    p=fp32_mul<false>(m,r); e=fp32_fma<false>(-p,r,1.0f); h=fp32_mul<false>(0.5f,r); r=fp32_fma<false>(h,e,r);
    p=fp32_mul<false>(m,r); e=fp32_fma<false>(-p,r,1.0f); h=fp32_mul<false>(0.5f,r); r=fp32_fma<false>(h,e,r);
    r=fp32_mul<false>(m,r);
    int shift=(((int)(aa>>23)-127)-(int)parity)/2;
    return approx_scale(fp32_encode(r),shift,0u);
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
 * \brief Reciprocal-refined division, square root and reciprocal square root.
 */
