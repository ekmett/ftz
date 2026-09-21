#pragma once
#include "ftz/config.h"
#include "native/attributes.h"
#include "ftz/math/float.h"

namespace ftz { namespace detail { namespace math {
  // Fixed odd graph x*P(x*x). Every active stage is normal or exact zero:
  // the tiny bypass prevents a square near underflow, and retained interval
  // bounds keep every Horner stage away from zero. Neither FTZ policy relies
  // on hardware's ambiguous minimum-normal rounding strip. RNE and real fused
  // FMA are required; no implicit contraction/reassociation is admitted.
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_nodiscard native_inline native_const unsigned int tanh_words(unsigned int word) {
    unsigned int magnitude = word & 0x7fffffffu;
    unsigned int sign = word & 0x80000000u;
    if (magnitude > 0x7f800000u) {
      return 0x7fc00000u;
    }
    if (magnitude < 0x00800000u) {
      return sign;
    }
    // tanh(x) rounds to x throughout this small interval. Preserve raw -0
    // above and normal bits here without evaluating an underflowing product.
    if (magnitude <= 0x39800000u) {
      return word;
    }
    // tanh(10) differs from one by less than half a binary32 ULP below one.
    // This also handles either infinity before any arithmetic.
    if (magnitude >= 0x41200000u) {
      return sign | 0x3f800000u;
    }
    float x = fp32_decode(magnitude);
    float z = fp32_mul<true,Hardware>(x, x);
    float t, h;
    if (magnitude <= 0x3f800000u) {
      t = fp32_fma<true,Hardware>(z, fp32_decode(0x40000000u), fp32_decode(0xbf800000u));
      h = fp32_decode(0x34facb37u);
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb63a0d2du));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x37813497u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb8bfb3f8u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3a0e6d24u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbb535f6cu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3c9d20e4u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbded544du));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3f5c6e3eu));
    }
    else if (magnitude <= 0x40000000u) {
      t = fp32_fma<true,Hardware>(z, fp32_decode(0x3f000000u), fp32_decode(0xbfa00000u));
      h = fp32_decode(0x38752140u);
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb9183513u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x398d9ee0u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xba2fdf3au));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3ae1130du));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbb8bc302u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3c2d6773u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbcd7a178u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3d86d45du));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbe2e2df9u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3f14c222u));
    }
    else if (magnitude <= 0x40400000u) {
      t = fp32_fma<true,Hardware>(z, fp32_decode(0x3e800000u), fp32_decode(0xbfd00000u));
      h = fp32_decode(0xb947bd66u);
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x39dfe5a4u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xba4a3864u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3ae2b84bu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbb813c08u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3c1129ceu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbca3c0b8u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3d3bcdf4u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbde4f8bcu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3ec662fcu));
    }
    else if (magnitude <= 0x40800000u) {
      t = fp32_fma<true,Hardware>(z, fp32_decode(0x3e800000u), fp32_decode(0xc0480000u));
      h = fp32_decode(0xb58f6d10u);
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x36863471u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb758ec9du));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x384b3fb7u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb9404721u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3a356f7bu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbb2d3d49u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3c2a7e14u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbd36d397u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3e9091d7u));
    }
    else if (magnitude <= 0x40c00000u) {
      t = fp32_fma<true,Hardware>(z, fp32_decode(0x3d800000u), fp32_decode(0xbfd00000u));
      h = fp32_decode(0xb9405fbfu);
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x39ab4d5fu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb9c08feeu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3a2bf5fau));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbaa65b2au));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3b1584c1u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbb86bc79u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3bf71905u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbc67da1fu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3ce35ff2u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbd76f5e5u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3e48ced7u));
    }
    else if (magnitude <= 0x41000000u) {
      t = fp32_fma<true,Hardware>(z, fp32_decode(0x3d800000u), fp32_decode(0xc0480000u));
      h = fp32_decode(0xb59171b1u);
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3671d749u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb72757e1u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x380d4deeu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb8f4646bu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x39d46b54u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbabdbc02u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3bb1ed8au));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbcb95c19u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3e10d0b5u));
    }
    else {
      t = fp32_fma<true,Hardware>(z, fp32_decode(0x3d000000u), fp32_decode(0xc0240000u));
      h = fp32_decode(0xb811c415u);
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x38c8e73cu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xb980a2b8u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3a37296bu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbb06690eu));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3bcea86au));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0xbcb08499u));
      h = fp32_fma<true,Hardware>(h, t, fp32_decode(0x3de229ecu));
    }
    // Keep the mathematical range despite the last multiply's rounding error.
    unsigned int rounded_magnitude = fp32_encode(fp32_mul<true,Hardware>(x, h));
    return (rounded_magnitude > 0x3f800000u ? 0x3f800000u : rounded_magnitude) | sign;
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_nodiscard native_inline native_const float tanh(float value) {
    return fp32_decode(tanh_words<Hardware>(fp32_encode(value)));
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
 * \brief Deterministic piecewise-polynomial tanh.
 */
