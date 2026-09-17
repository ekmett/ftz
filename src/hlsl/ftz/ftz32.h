#pragma once
#include "ftz/config.h"
#include "ftz/ftz32_ops.h"

// HLSL 2021 has no user constructors or conversion operators. Use the named
// imports/exports; a raw HLSL aggregate cast does not normalize FP32 words.
// The field is public because HLSL has no access control. Treat it as storage.
namespace ftz {
  /** \defgroup ftz_shader HLSL values
   * Include <ftz/ftz32.h> in HLSL 2021. FTZ_FP32_HARDWARE_FTZ selects one shader
   * policy at compilation; it does not choose CPU m32/h32 or configure the device.
   * Hardware mode requires device-specific admission, including signed add/sub
   * flushing. CPU admission never establishes that shader contract.
   * \snippet shader.hlsl shader_values
   */
  /// \ingroup ftz_shader
  /// \brief Canonical binary32 word wrapper with named HLSL imports and exports.
  /// Ordinary aggregate casts/default storage do not normalize or initialize a value;
  /// use from_bits(), from_float() or zero(). Treat bits_ as implementation storage.
  struct ftz32 {
    unsigned int bits_;

    /// \ingroup ftz_shader
    /// \brief Imports a binary32 word and replaces a subnormal magnitude with signed zero.
    static ftz32 from_bits(unsigned int bits) {
      ftz32 result;
      result.bits_ = detail::ftz32_canonical(bits);
      return result;
    }
    /// \ingroup ftz_shader
    /// \brief Imports a float through its bits and normalizes subnormals.
    static ftz32 from_float(float value) { return from_bits(asuint(value)); }
    // Caller guarantees a canonical word. This performs no normalization.
    /// \ingroup ftz_shader
    /// \brief Wraps bits unchanged; the caller must supply normal, signed zero, infinity or NaN.
    static ftz32 unsafe_from_float32(float value) {
      ftz32 result;
      result.bits_ = asuint(value);
      return result;
    }
    /// \ingroup ftz_shader
    /// \brief Returns positive zero.
    static ftz32 zero() { return from_bits(0u); }
    /// \ingroup ftz_shader
    /// \brief Exports the exact stored word without arithmetic.
    unsigned int to_bits() { return bits_; }
    /// \ingroup ftz_shader
    /// \brief Exports stored bits as an ordinary HLSL float, leaving this value contract.
    float to_float() { return asfloat(bits_); }

    /// \brief Uses the selected shader FTZ policy for canonical operands.
    ftz32 operator+(ftz32 rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_add(bits_, rhs.bits_);
      return result;
    }

    /// \brief Imports a raw float operand before the operation.
    ftz32 operator+(float rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_add(bits_, detail::ftz32_canonical(asuint(rhs)));
      return result;
    }

    /// \brief Uses the selected shader FTZ policy for canonical operands.
    ftz32 operator-(ftz32 rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_sub(bits_, rhs.bits_);
      return result;
    }

    /// \brief Imports a raw float operand before the operation.
    ftz32 operator-(float rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_sub(bits_, detail::ftz32_canonical(asuint(rhs)));
      return result;
    }

    /// \brief Uses the selected shader FTZ policy for canonical operands.
    ftz32 operator*(ftz32 rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_mul(bits_, rhs.bits_);
      return result;
    }

    /// \brief Imports a raw float operand before the operation.
    ftz32 operator*(float rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_mul(bits_, detail::ftz32_canonical(asuint(rhs)));
      return result;
    }

    /// \brief Uses the selected shader FTZ policy for canonical operands.
    ftz32 operator/(ftz32 rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_div(bits_, rhs.bits_);
      return result;
    }

    /// \brief Imports a raw float operand before the operation.
    ftz32 operator/(float rhs) {
      ftz32 result;
      result.bits_ = detail::ftz32_div(bits_, detail::ftz32_canonical(asuint(rhs)));
      return result;
    }
    /// \brief Compares canonical values; NaN is unordered and the two zero signs compare equal.
    bool operator<(ftz32 rhs) { return detail::ftz32_less(bits_, rhs.bits_); }
    /// \brief Compares canonical values; NaN is unordered and the two zero signs compare equal.
    bool operator>(ftz32 rhs) { return detail::ftz32_less(rhs.bits_, bits_); }
    /// \brief Compares canonical values; NaN is unordered and the two zero signs compare equal.
    bool operator==(ftz32 rhs) { return detail::ftz32_equal(bits_, rhs.bits_); }
    /// \brief Compares canonical values; NaN is unordered and the two zero signs compare equal.
    bool operator!=(ftz32 rhs) { return !detail::ftz32_equal(bits_, rhs.bits_); }
    /// \brief Compares canonical values; NaN is unordered and the two zero signs compare equal.
    bool operator<=(ftz32 rhs) { return detail::ftz32_less(bits_, rhs.bits_) || detail::ftz32_equal(bits_, rhs.bits_); }
    /// \brief Compares canonical values; NaN is unordered and the two zero signs compare equal.
    bool operator>=(ftz32 rhs) { return detail::ftz32_less(rhs.bits_, bits_) || detail::ftz32_equal(bits_, rhs.bits_); }
    /// \brief Imports a raw float operand before the operation.
    bool operator<(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_less(bits_, word);
    }
    /// \brief Imports a raw float operand before the operation.
    bool operator>(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_less(word, bits_);
    }
    /// \brief Imports a raw float operand before the operation.
    bool operator==(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_equal(bits_, word);
    }
    /// \brief Imports a raw float operand before the operation.
    bool operator!=(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return !detail::ftz32_equal(bits_, word);
    }
    /// \brief Imports a raw float operand before the operation.
    bool operator<=(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_less(bits_, word) || detail::ftz32_equal(bits_, word);
    }
    /// \brief Imports a raw float operand before the operation.
    bool operator>=(float rhs) {
      unsigned int word = detail::ftz32_canonical(asuint(rhs));
      return detail::ftz32_less(word, bits_) || detail::ftz32_equal(bits_, word);
    }
  };

  // Current DXC rejects unary and compound operators on structs. These named
  // functions preserve the same word graph without converting to builtin float.
  /// \ingroup ftz_shader
  /// \brief Flips the sign bit without floating-point arithmetic.
  ftz32 neg(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_neg(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Clears the sign bit without floating-point arithmetic.
  ftz32 abs(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_abs(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Returns the reciprocal-refined square-root approximation; signed zero and positive
  /// infinity are preserved, negative nonzero values give NaN.
  ftz32 sqrt(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_sqrt(value.bits_);
    return result;
  }
  // Rounding canonical inputs produces zero or an integer, never a subnormal.
  /// \ingroup ftz_shader
  /// \brief Rounds toward negative infinity, independently of the ambient rounding direction;
  /// retains signed zero and infinities.
  ftz32 floor(ftz32 value) {
    return ftz32::unsafe_from_float32(floor(value.to_float()));
  }
  /// \ingroup ftz_shader
  /// \brief Rounds toward positive infinity, independently of the ambient rounding direction;
  /// retains signed zero and infinities.
  ftz32 ceil(ftz32 value) {
    return ftz32::unsafe_from_float32(ceil(value.to_float()));
  }
  /// \ingroup ftz_shader
  /// \brief Rounds toward zero, independently of the ambient rounding direction; retains signed
  /// zero and infinities.
  ftz32 trunc(ftz32 value) {
    return ftz32::unsafe_from_float32(trunc(value.to_float()));
  }
  /// \ingroup ftz_shader
  /// \brief Returns the reproducible sine approximation in radians; signed zero is preserved
  /// and infinity gives NaN.
  ftz32 sin(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_sin(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Returns the reproducible cosine approximation in radians; either zero gives one and
  /// infinity gives NaN.
  ftz32 cos(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_cos(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Returns the reproducible exponential; negative infinity gives positive zero and
  /// positive infinity is preserved.
  ftz32 exp(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_exp(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Approximates exp(x)-1 without subtracting one for tiny x; signed zero is preserved.
  ftz32 expm1(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_expm1(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Returns the reproducible hyperbolic tangent; signed zero is preserved and
  /// infinities give signed one.
  ftz32 tanh(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_tanh(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Returns the natural-log approximation; either zero gives negative infinity,
  /// negative nonzero values give NaN.
  ftz32 log(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_log(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Approximates log(1+x); -1 gives negative infinity, x below -1 gives NaN, and signed
  /// zero is preserved.
  ftz32 log1p(ftz32 value) {
    ftz32 result;
    result.bits_ = detail::ftz32_log1p(value.bits_);
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Returns the angle in radians for (y,x), retaining signed-axis and infinity
  /// quadrants; NaN inputs give NaN.
  ftz32 atan2(ftz32 y, ftz32 x) {
    ftz32 result;
    result.bits_ = detail::ftz32_atan2(y.bits_, x.bits_);
    return result;
  }
  /// \brief Paired HLSL trig values; sine precedes cosine.
  struct ftz32_sincos_result { ftz32 sine, cosine; };
  /// \ingroup ftz_shader
  /// \brief Returns sine followed by cosine, sharing reduction; C++ uses a pair and HLSL uses
  /// named fields.
  /// \returns ftz32_sincos_result with sine and cosine fields.
  ftz32_sincos_result sincos(ftz32 value) {
    detail::ftz32_sincos_bits words = detail::ftz32_sincos(value.bits_);
    ftz32_sincos_result result;
    result.sine.bits_ = words.sine;
    result.cosine.bits_ = words.cosine;
    return result;
  }
  /// \ingroup ftz_shader
  /// \brief Tests the encoded NaN class without floating-point arithmetic or quieting a signaling NaN.
  bool isnan(ftz32 value) { return detail::ftz32_isnan(value.bits_); }
  /// \ingroup ftz_shader
  /// \brief Tests for either signed infinity by its encoded bits.
  bool isinf(ftz32 value) { return (value.bits_ & 0x7fffffffu) == detail::ftz32_infinity; }
  /// \ingroup ftz_shader
  /// \brief Tests for zero or finite magnitude by its encoded bits.
  bool isfinite(ftz32 value) { return (value.bits_ & 0x7fffffffu) < detail::ftz32_infinity; }
  /// \ingroup ftz_shader
  /// \brief Tests the sign bit, including negative zero and signed NaNs.
  bool signbit(ftz32 value) { return (value.bits_ & detail::ftz32_sign) != 0u; }
  /// \ingroup ftz_shader
  /// \brief Copies the second operand's sign to the first operand's magnitude, preserving the
  /// magnitude's NaN payload.
  ftz32 copysign(ftz32 value, ftz32 sign) {
    return ftz32::from_bits((value.bits_ & 0x7fffffffu) | (sign.bits_ & detail::ftz32_sign));
  }
  /// \ingroup ftz_shader
  /// \brief Computes a*b+c with one fused rounding and the FTZ boundary repair.
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
    /// \brief Uses the selected shader FTZ policy for canonical operands.
 * \brief HLSL 2021 FTZ value type, conversion factories, operators, and math overloads.
 */
