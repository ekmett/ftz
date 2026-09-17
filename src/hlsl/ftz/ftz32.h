#pragma once
#include "ftz/config.h"
#include "ftz/ftz32_ops.h"

// HLSL 2021 has no user constructors or conversion operators. Use the named
// imports/exports; a raw HLSL aggregate cast does not normalize FP32 words.
// The field is public because HLSL has no access control. Treat it as storage.
namespace ftz {
  struct ftz32 {
    unsigned int bits_;

    static ftz32 from_bits(unsigned int bits) {
      ftz32 result;
      result.bits_ = detail::ftz32_canonical(bits);
      return result;
    }
    static ftz32 from_float(float value) { return from_bits(asuint(value)); }
    // Caller guarantees a canonical word. This performs no normalization.
    static ftz32 unsafe_from_float32(float value) {
      ftz32 result;
      result.bits_ = asuint(value);
      return result;
    }
    static ftz32 zero() { return from_bits(0u); }
    unsigned int to_bits() { return bits_; }
    float to_float() { return asfloat(bits_); }

    ftz32 operator+(ftz32 rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_add(bits_, rhs.bits_);
      return result;
    }

    ftz32 operator+(float rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_add(bits_, detail::ftz32_canonical(asuint(rhs)));
      return result;
    }

    ftz32 operator-(ftz32 rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_sub(bits_, rhs.bits_);
      return result;
    }

    ftz32 operator-(float rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_sub(bits_, detail::ftz32_canonical(asuint(rhs)));
      return result;
    }

    ftz32 operator*(ftz32 rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_mul(bits_, rhs.bits_);
      return result;
    }

    ftz32 operator*(float rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_mul(bits_, detail::ftz32_canonical(asuint(rhs)));
      return result;
    }

    ftz32 operator/(ftz32 rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_div(bits_, rhs.bits_);
      return result;
    }

    ftz32 operator/(float rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_div(bits_, detail::ftz32_canonical(asuint(rhs)));
      return result;
    }
    bool operator<(ftz32 rhs) { return detail::ftz32_less(bits_, rhs.bits_); }
    bool operator>(ftz32 rhs) { return detail::ftz32_less(rhs.bits_, bits_); }
    bool operator==(ftz32 rhs) { return detail::ftz32_equal(bits_, rhs.bits_); }
    bool operator!=(ftz32 rhs) { return !detail::ftz32_equal(bits_, rhs.bits_); }
    bool operator<=(ftz32 rhs) { return detail::ftz32_less(bits_, rhs.bits_) || detail::ftz32_equal(bits_, rhs.bits_); }
    bool operator>=(ftz32 rhs) { return detail::ftz32_less(rhs.bits_, bits_) || detail::ftz32_equal(bits_, rhs.bits_); }
    bool operator<(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_less(bits_, word);
    }
    bool operator>(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_less(word, bits_);
    }
    bool operator==(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_equal(bits_, word);
    }
    bool operator!=(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return !detail::ftz32_equal(bits_, word);
    }
    bool operator<=(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_less(bits_, word) || detail::ftz32_equal(bits_, word);
    }
    bool operator>=(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_less(word, bits_) || detail::ftz32_equal(bits_, word);
    }
  };

  // Current DXC rejects unary and compound operators on structs. These named
  // functions preserve the same word graph without converting to builtin float.
  ftz32 neg(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_neg(value.bits_);
    return result;
  }
  ftz32 abs(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_abs(value.bits_);
    return result;
  }
  ftz32 sqrt(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_sqrt(value.bits_);
    return result;
  }
  // Rounding canonical inputs produces zero or an integer, never a subnormal.
  ftz32 floor(ftz32 value) {
    return ftz32::unsafe_from_float32(floor(value.to_float()));
  }
  ftz32 ceil(ftz32 value) {
    return ftz32::unsafe_from_float32(ceil(value.to_float()));
  }
  ftz32 trunc(ftz32 value) {
    return ftz32::unsafe_from_float32(trunc(value.to_float()));
  }
  ftz32 sin(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_sin(value.bits_);
    return result;
  }
  ftz32 cos(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_cos(value.bits_);
    return result;
  }
  ftz32 exp(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_exp(value.bits_);
    return result;
  }
  ftz32 expm1(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_expm1(value.bits_);
    return result;
  }
  ftz32 tanh(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_tanh(value.bits_);
    return result;
  }
  ftz32 log(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_log(value.bits_);
    return result;
  }
  ftz32 log1p(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_log1p(value.bits_);
    return result;
  }
  ftz32 atan2(ftz32 y, ftz32 x) {
    ftz32 result;
    result.bits_ = detail::ftz32_atan2(y.bits_, x.bits_);
    return result;
  }
  struct ftz32_sincos_result { ftz32 sine, cosine; };
  ftz32_sincos_result sincos(ftz32 value) {
    detail::ftz32_sincos_bits words = detail::ftz32_sincos(value.bits_);
    ftz32_sincos_result result;
    result.sine.bits_ = words.sine;
    result.cosine.bits_ = words.cosine;
    return result;
  }
  bool isnan(ftz32 value) { return detail::ftz32_isnan(value.bits_); }
  bool isinf(ftz32 value) { return (value.bits_ & 0x7fffffffu) == detail::ftz32_infinity; }
  bool isfinite(ftz32 value) { return (value.bits_ & 0x7fffffffu) < detail::ftz32_infinity; }
  bool signbit(ftz32 value) { return (value.bits_ & detail::ftz32_sign) != 0u; }
  ftz32 copysign(ftz32 value, ftz32 sign) {
    return ftz32::from_bits((value.bits_ & 0x7fffffffu) | (sign.bits_ & detail::ftz32_sign));
  }
  ftz32 fma(ftz32 a, ftz32 b, ftz32 c) {
    ftz32 result;
    result.bits_ = detail::ftz32_fma(a.bits_, b.bits_, c.bits_);
    return result;
  }
}

/**
 * \file
 * \license
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
 * \endlicense
 * \author Edward Kmett <ekmett@gmail.com>
 * \brief HLSL 2021 FTZ value type, conversion factories, operators, and math overloads.
 */
