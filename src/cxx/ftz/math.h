#pragma once
#include <native/attributes.h>
#include <ftz/config.h>
#include <array>
#include <bit>
#include <utility>
#include <cstdint>
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#endif
import native.scalar;
import native.math;

namespace ftz::detail::native {
  using ::native::mask_bits;
  using fp32x1 = ::native::simd<float,1,::native::scalar>;
  template <class V> concept float_register = requires { typename V::bits_type; V::lanes; };
  namespace detail {
    template <class V> struct fp32_bit_bridge {
      using bits_type = typename V::bits_type;
      static native_inline bits_type encode(V value) noexcept { return value.bits(); }
      static native_inline V decode(bits_type value) noexcept { return V::from_bits(value); }
    };
    template <class V> struct trig_conversion {
      using U = typename V::bits_type;
      using I = typename V::template rebind<std::int32_t>;
      static native_inline U integer(V value) noexcept {
        return U::from_native(std::bit_cast<typename U::native_type>(convert<std::int32_t>(value).to_native()));
      }
      static native_inline V floating(U value) noexcept {
        return convert<float>(I::from_native(std::bit_cast<typename I::native_type>(value.to_native())));
      }
    };
    template <class V> native_inline V min(V a,V b) noexcept { return select(a<b,a,b); }
    template <class V> native_inline V max(V a,V b) noexcept { return select(a>b,a,b); }
  }
}

// Altered source: paired polynomial and reducer with signed input FTZ,
// explicit tiny-input selection and bitwise reconstruction. Notices are retained below.
// Require binary32 RNE, native fused fma and no implicit expression contraction.
namespace ftz::detail::native {
  enum class trig_output { sine, cosine, pair };
  namespace detail {
    template <trig_output Output, bool Hardware, bool Bounded, float_register V, std::size_t N>
    native_flatten native_inline auto sincos_ftz_kernel(std::array<V, N> const & input) noexcept {
      using B = fp32_bit_bridge<V>;
      using I = typename B::bits_type;
      using C = trig_conversion<V>;
      if constexpr (N == 0) {
        if constexpr (Output == trig_output::pair) return std::pair{std::array<V,0>{}, std::array<V,0>{}};
        else return std::array<V,0>{};
      } else {
        // Bounded lanes can choose either polynomial according to quadrant.
        constexpr bool need_sine = Bounded || Output != trig_output::cosine;
        constexpr bool need_cosine = Bounded || Output != trig_output::sine;
        auto const constant = [](std::uint32_t word) noexcept { return B::decode(I(word)); };
        auto const & [...original] = input;
        // Integer admission/masking is independent of DAZ and preserves -0.
        auto const [...word] = std::array{B::encode(original)...};
        auto const [...canonical] = std::array{B::decode(word &
            ((mask_bits<std::uint32_t>((word & I(0x7f800000u)) == I(0)) &
              I(0x007fffffu)) ^ I(0xffffffffu)))...};
        auto const [...magnitude] = std::array{B::decode(B::encode(canonical) & I(0x7fffffffu))...};
        auto [...index] = std::array<I, N>{};
        auto [...reduced] = std::array{canonical...};
        if constexpr (Bounded) {
          auto const [...product] = std::array{(magnitude * constant(0x3fa2f983u))...};
          ((index = (C::integer(product) + I(1)) & I(0xfffffffeu)), ...);
          auto const [...multiple] = std::array{C::floating(index)...};
          ((reduced = fma(multiple, constant(0xbf490000u), magnitude)), ...);
          ((reduced = fma(multiple, constant(0xb97da000u), reduced)), ...);
          ((reduced = fma(multiple, constant(0xb3222169u), reduced)), ...);
        }
        auto const [...active] = std::array{
            mask_bits<std::uint32_t>((B::encode(reduced) & I(0x7fffffffu)) > I(0x39800000u))...};
        auto const [...masked] = [&] {
          if constexpr(Hardware) return std::array{reduced...};
          else return std::array{B::decode(B::encode(reduced) & active)...};
        }();
        auto const [...square] = std::array{(masked * masked)...};
        auto [...sine] = [&] {
          if constexpr (need_sine) return std::array{fma(constant(0xb94ca1f9u), square, constant(0x3c08839eu))...};
          else return std::array<V,0>{};
        }();
        auto [...cosine] = [&] {
          if constexpr (need_cosine) return std::array{fma(constant(0x37ccf5ceu), square, constant(0xbab6061au))...};
          else return std::array<V,0>{};
        }();
        if constexpr (need_sine) ((sine = fma(sine, square, constant(0xbe2aaaa3u))), ...);
        if constexpr (need_cosine) ((cosine = fma(cosine, square, constant(0x3d2aaaa5u))), ...);
        if constexpr (need_sine) ((sine = sine * square), ...);
        if constexpr (need_cosine) ((cosine = cosine * square), ...);
        if constexpr (need_cosine) ((cosine = cosine * square), ...);
        if constexpr (need_cosine) {
          auto const [...half_square] = std::array{(square * V(0.5f))...};
          ((cosine = cosine - half_square), ...);
        }
        if constexpr (need_sine) ((sine = fma(sine, masked, reduced)), ...);
        if constexpr (need_cosine) ((cosine = cosine + V(1.0f)), ...);
        // The select is essential for a tiny negative zero: +0 + -0 is +0.
        if constexpr (need_sine) ((sine = B::decode((B::encode(sine) & active) |
            (B::encode(reduced) & (active ^ I(0xffffffffu))))), ...);
        if constexpr (Bounded) {
          auto const [...bounded_sine] = [&] {
            if constexpr (Output != trig_output::cosine) return std::array{B::decode(select((index & I(2)) == I(0),
                B::encode(sine), B::encode(cosine)) ^ (B::encode(canonical) & I(0x80000000u)) ^
                ((index & I(4)).template left<29>()))...};
            else return std::array<V,0>{};
          }();
          if constexpr (Output != trig_output::sine)
            ((cosine = B::decode(select((index & I(2)) == I(0), B::encode(cosine), B::encode(sine)) ^
                (((index - I(2)) ^ I(0xffffffffu)) & I(4)).template left<29>())), ...);
          if constexpr (Output == trig_output::pair) return std::pair{std::array{bounded_sine...}, std::array{cosine...}};
          else if constexpr (Output == trig_output::sine) return std::array{bounded_sine...};
          else return std::array{cosine...};
        } else {
          if constexpr (Output == trig_output::pair) return std::pair{std::array{sine...}, std::array{cosine...}};
          else if constexpr (Output == trig_output::sine) return std::array{sine...};
          else return std::array{cosine...};
        }
      }
    }
  } // namespace detail

  // Reduced entry: finite |x| <= 1. Input subnormals become signed zero.
  // No range reduction. This is a reproducible approximation, not CR sin/cos.
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V, std::size_t N>
  native_inline std::pair<std::array<V, N>, std::array<V, N>> sincos_reduced_ftz(std::array<V, N> const & x) noexcept {
    return detail::sincos_ftz_kernel<trig_output::pair,Hardware,false>(x);
  }
  // Bounded entry: finite |x| < 8192. Shared three-FMA reduction.
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V, std::size_t N>
  native_inline std::pair<std::array<V, N>, std::array<V, N>> sincos_ftz(std::array<V, N> const & x) noexcept {
    return detail::sincos_ftz_kernel<trig_output::pair,Hardware,true>(x);
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V, std::size_t N>
  native_inline std::array<V, N> sin_reduced_ftz(std::array<V, N> const & x) noexcept {
    return detail::sincos_ftz_kernel<trig_output::sine,Hardware,false>(x);
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V, std::size_t N>
  native_inline std::array<V, N> cos_reduced_ftz(std::array<V, N> const & x) noexcept {
    return detail::sincos_ftz_kernel<trig_output::cosine,Hardware,false>(x);
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V, std::size_t N>
  native_inline std::array<V, N> sin_ftz(std::array<V, N> const & x) noexcept { return detail::sincos_ftz_kernel<trig_output::sine,Hardware,true>(x); }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V, std::size_t N>
  native_inline std::array<V, N> cos_ftz(std::array<V, N> const & x) noexcept { return detail::sincos_ftz_kernel<trig_output::cosine,Hardware,true>(x); }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V> native_inline std::pair<V, V> sincos_reduced_ftz(V x) noexcept {
    auto [sine, cosine] = sincos_reduced_ftz<Hardware>(std::array{x});
    return {sine[0], cosine[0]};
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V> native_inline std::pair<V, V> sincos_ftz(V x) noexcept {
    auto [sine, cosine] = sincos_ftz<Hardware>(std::array{x});
    return {sine[0], cosine[0]};
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V> native_inline V sin_reduced_ftz(V x) noexcept { return sin_reduced_ftz<Hardware>(std::array{x})[0]; }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V> native_inline V cos_reduced_ftz(V x) noexcept { return cos_reduced_ftz<Hardware>(std::array{x})[0]; }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V> native_inline V sin_ftz(V x) noexcept { return sin_ftz<Hardware>(std::array{x})[0]; }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V> native_inline V cos_ftz(V x) noexcept { return cos_ftz<Hardware>(std::array{x})[0]; }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_inline std::pair<float, float> sincos_reduced_ftz(float x) noexcept {
    auto [sine, cosine] = sincos_reduced_ftz<Hardware>(fp32x1(x)); return {sine.value, cosine.value};
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_inline std::pair<float, float> sincos_ftz(float x) noexcept {
    auto [sine, cosine] = sincos_ftz<Hardware>(fp32x1(x)); return {sine.value, cosine.value};
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_inline float sin_reduced_ftz(float x) noexcept { return sin_reduced_ftz<Hardware>(fp32x1(x)).value; }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_inline float cos_reduced_ftz(float x) noexcept { return cos_reduced_ftz<Hardware>(fp32x1(x)).value; }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_inline float sin_ftz(float x) noexcept { return sin_ftz<Hardware>(fp32x1(x)).value; }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_inline float cos_ftz(float x) noexcept { return cos_ftz<Hardware>(fp32x1(x)).value; }
} // namespace ftz::detail::native

/*
   AVX implementation of sin, cos, sincos, exp and log

   Based on "sse_mathfun.h", by Julien Pommier
   http://gruntthepeon.free.fr/ssemath/

   Copyright (C) 2012 Giovanni Garberoglio
   Interdisciplinary Laboratory for Computational Science (LISC)
   Fondazione Bruno Kessler and University of Trento
   via Sommarive, 18
   I-38123 Trento (Italy)

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* RTS repository license (retained verbatim):
Software License Agreement (BSD 2-Clause License)
========================================

Copyright 2017 Edward Kmett

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL YAHOO! INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * \file
 * \license
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2012 Giovanni Garberoglio
 * SPDX-FileCopyrightText: 2017 Edward Kmett
 * SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
 * SPDX-License-Identifier: Zlib AND BSD-2-Clause
 * \endlicense
 * \author Edward Kmett <ekmett@gmail.com>
 * \brief Sine and cosine polynomials, range reduction, and raw or FTZ register-pack evaluation.
 */

namespace ftz::detail::native {
  template <class F, class U> struct expm1_result {
    F value;
    U valid;
  };
  namespace detail {
    template <bool Hardware, bool Gain, float_register V, std::size_t N>
    native_flatten native_inline auto expm1_graph(std::array<V, N> const & input) noexcept {
      using B = fp32_bit_bridge<V>;
      using U = typename B::bits_type;
      using result_type = expm1_result<std::array<V, N>, std::array<U, N>>;
      if constexpr (N == 0) return result_type{};
      else {
        auto const constant = [](std::uint32_t word) noexcept { return B::decode(U(word)); };
        auto const & [...original] = input;
        auto [...word] = std::array{B::encode(original)...};
        U const finite_limit(0x7f800000u), positive_limit(0x3f800001u), sign(0x80000000u);
        auto [...valid] = std::array<U, N>{};
        // Raw unsigned intervals admit the domain without evaluating even sNaNs.
        if constexpr (Gain) {
          // Nonnegative finite values, plus negative zero.
          ((valid = mask_bits<std::uint32_t>((finite_limit > word) | (word == sign))), ...);
        } else {
          // Negative finite values, or nonnegative values through one inclusive.
          ((valid = mask_bits<std::uint32_t>(
              (finite_limit > (word ^ sign)) | (positive_limit > word))), ...);
        }
        ((word = word & valid), ...);
        ((word = select((word & U(0x7f800000u)) == U(0), word & U(0x80000000u), word)), ...);
        if constexpr (Gain) ((word = word ^ U(0x80000000u)), ...);
        auto const [...x] = std::array{max(B::decode(word), V(-18))...};
        auto const [...n] = std::array{round_even(x * constant(0x3fb8aa3bu))...};
        auto [...r] = std::array{fma(n, constant(0xbf317200u), x)...};
        ((r = fma(n, constant(0xb5bfbe8eu), r)), ...);
        ((r = select(n == V(0), x, r)), ...);
        // The tiny result is selected from r after speculative polynomial work.
        auto const [...t] = [&] {
          if constexpr(Hardware) return std::array{r...};
          else return std::array{B::decode(select(
            (B::encode(r) & U(0x7fffffffu)) > U(0x33000000u),
            B::encode(r), U(0)))...};
        }();
        auto const [...z] = std::array{(t * t)...};
        auto [...h] = std::array{fma(t, constant(0x3493f27eu), constant(0x3638ef1du))...};
        ((h = fma(t, h, constant(0x37d00d01u))), ...);
        ((h = fma(t, h, constant(0x39500d01u))), ...);
        ((h = fma(t, h, constant(0x3ab60b61u))), ...);
        ((h = fma(t, h, constant(0x3c088889u))), ...);
        ((h = fma(t, h, constant(0x3d2aaaabu))), ...);
        ((h = fma(t, h, constant(0x3e2aaaabu))), ...);
        ((h = fma(t, h, constant(0x3f000000u))), ...);
        auto [...p] = std::array{fma(z, h, r)...};
        if constexpr(Hardware) {
        ((p = B::decode(select(
            (B::encode(r) & U(0x7fffffffu)) > U(0x33000000u),
            B::encode(p), B::encode(r)))), ...);
        } else {
        ((p = select(t == V(0), r, p)), ...);
        }
        // 2^n-1 is exact for -24 <= n <= 1. Clamp speculative lanes before it.
        auto const [...scale] = std::array{normal_pow2(max(n, V(-24)))...};
        auto [...result] = std::array{fma(scale, p, scale - V(1))...};
        ((result = select(n == V(0), p, result)), ...);
        ((result = select(n == V(-25), select(p > V(0), constant(0xbf7fffffu), V(-1)), result)), ...);
        ((result = select(n < V(-25), V(-1), result)), ...);
        // The entry-canonicalized graph returns only normal or signed zero.
        if constexpr(Hardware) {
        ((word = B::encode(result)), ...);
        } else {
        ((word = B::encode(flush_to_zero(result))), ...);
        }
        if constexpr (Gain) ((word = word ^ U(0x80000000u)), ...);
        ((word = word & valid), ...);
        return result_type{{{B::decode(word)...}}, {{(valid & U(1))...}}};
      }
    }
  }
  // Conditional reproducible graph: actual RNE fused FMA, separate RNE multiply.
  // Signed FTZ at entry/exit; no ambient FP-mode changes. Finite x <= 1 only.
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V, std::size_t N>
  native_inline auto expm1_checked(std::array<V, N> const & input) noexcept {
    return detail::expm1_graph<Hardware,false>(input);
  }
  // Damping gain -expm1(-a), finite a >= 0 (either signed zero accepted).
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V, std::size_t N>
  native_inline auto damping_gain_checked(std::array<V, N> const & input) noexcept {
    return detail::expm1_graph<Hardware,true>(input);
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V> native_inline auto expm1_checked(V input) noexcept {
    auto r = expm1_checked<Hardware>(std::array{input});
    return expm1_result<V, typename detail::fp32_bit_bridge<V>::bits_type>{
      r.value[0], r.valid[0]};
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0, float_register V> native_inline auto damping_gain_checked(V input) noexcept {
    auto r = damping_gain_checked<Hardware>(std::array{input});
    return expm1_result<V, typename detail::fp32_bit_bridge<V>::bits_type>{
      r.value[0], r.valid[0]};
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_inline expm1_result<float, std::uint32_t> expm1_checked(float input) noexcept {
    auto r = expm1_checked<Hardware>(fp32x1(input));
    return {r.value.value, r.valid.value};
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_inline expm1_result<float, std::uint32_t> damping_gain_checked(float input) noexcept {
    auto r = damping_gain_checked<Hardware>(fp32x1(input));
    return {r.value.value, r.valid.value};
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
 * \brief Cancellation-safe expm1 and damping gain over scalar and SIMD register packs.
 */

namespace ftz::detail::native {
  // Same live Horner chains as the scalar tanh graph, selected per lane.
  // Input comes from typed FTZ values: normal/signed zero/infinity/any NaN.
  // This is a prechecked graph, not an entry point for raw subnormal inputs.
  // For active outputs, |x| in (2^-12,10) gives normal z=x*x in
  // [2^-24,100]. The seven
  // affine transforms have |t|<=1; cancellation near their exact dyadic
  // centers gives t=0 or |t|>=2^-24. Interval propagation with outward
  // binary32 rounding bounds keeps every live Horner stage at |h|>2^-22
  // and |h|<2. The final product is normal as well. Internal FTZ work is
  // therefore unnecessary for either policy. Hardware tiny lanes may flush
  // their unused square/product; their t stays in [-1,1] and their final
  // output is the original word. Leading zero coefficients
  // add only exact 0*t+0 / 0*t+c steps before each original live chain.
  template <bool Hardware, float_register V, std::size_t N>
  native_flatten native_inline std::array<V,N> tanh_ftz(std::array<V,N> const & input) noexcept {
    using B=detail::fp32_bit_bridge<V>;
    using U=typename B::bits_type;
    if constexpr (N==0) return {};
    else {
      auto const & [...original]=input;
      auto const [...word]=std::array{B::encode(original)...};
      auto const [...magnitude]=std::array{(word & U(0x7fffffffu))...};
      // Both paths bound large/nonfinite inputs before squaring. Manual FTZ
      // also avoids subnormal intermediate work; hardware FTZ handles tiny
      // lanes naturally, and the required tiny-region selection replaces them.
      auto const [...safe]=[&] {
        if constexpr(Hardware) return std::array{
          select(magnitude<U(0x41200000u),magnitude,U(0x3f800000u))...};
        else return std::array{select((magnitude>U(0x39800000u)) &
          (magnitude<U(0x41200000u)),magnitude,U(0x3f800000u))...};
      }();
      auto const [...x]=std::array{B::decode(safe)...};
      auto const [...z]=std::array{(x*x)...};
      // Retain one interval index, not seven masks or eleven coefficient packs.
      auto const [...interval]=std::array{select(safe<=U(0x40800000u),
        select(safe<=U(0x40000000u),select(safe<=U(0x3f800000u),U(0),U(1)),
          select(safe<=U(0x40400000u),U(2),U(3))),
        select(safe<=U(0x41000000u),select(safe<=U(0x40c00000u),U(4),U(5)),U(6)))...};
      auto coefficient=[](U index,unsigned c0,unsigned c1,unsigned c2,unsigned c3,
          unsigned c4,unsigned c5,unsigned c6) noexcept {
        if constexpr(U::lanes==1) {
          std::array table{c0,c1,c2,c3,c4,c5,c6,c6};
          return B::decode(U(table[index.to_native()]));
        }
#if defined(__x86_64__) || defined(_M_X64)
        else if constexpr(U::lanes<=8) {
          auto table=std::bit_cast<__m256i>(std::array{c0,c1,c2,c3,c4,c5,c6,c6});
          if constexpr(U::lanes<=4) {
            auto i=_mm256_zextsi128_si256(std::bit_cast<__m128i>(index.to_native()));
            auto bits=_mm256_castsi256_si128(_mm256_permutevar8x32_epi32(table,i));
            return B::decode(U::from_native(std::bit_cast<typename U::native_type>(bits)));
          } else {
            auto bits=_mm256_permutevar8x32_epi32(table,std::bit_cast<__m256i>(index.to_native()));
            return B::decode(U::from_native(std::bit_cast<typename U::native_type>(bits)));
          }
        } else {
          auto table=std::bit_cast<__m512i>(std::array{c0,c1,c2,c3,c4,c5,c6,c6,c0,c1,c2,c3,c4,c5,c6,c6});
          auto bits=_mm512_permutexvar_epi32(std::bit_cast<__m512i>(index.to_native()),table);
          return B::decode(U::from_native(std::bit_cast<typename U::native_type>(bits)));
        }
#elif defined(__aarch64__) || defined(_M_ARM64)
        else {
          uint8x16x2_t table{{std::bit_cast<uint8x16_t>(std::array{c0,c1,c2,c3}),
            std::bit_cast<uint8x16_t>(std::array{c4,c5,c6,c6})}};
          auto i=std::bit_cast<uint32x4_t>(index.to_native());
          auto offsets=vaddq_u32(vmulq_n_u32(i,0x04040404u),vdupq_n_u32(0x03020100u));
          auto bits=vqtbl2q_u8(table,vreinterpretq_u8_u32(offsets));
          return B::decode(U::from_native(std::bit_cast<typename U::native_type>(bits)));
        }
#endif
      };
      auto const [...t]=std::array{fma(z,
        coefficient(interval,0x40000000u,0x3f000000u,0x3e800000u,0x3e800000u,0x3d800000u,0x3d800000u,0x3d000000u),
        coefficient(interval,0xbf800000u,0xbfa00000u,0xbfd00000u,0xc0480000u,0xbfd00000u,0xc0480000u,0xc0240000u))...};
      auto [...h]=std::array{coefficient(interval,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0xb9405fbfu,0x00000000u,0x00000000u)...};
      ((h=fma(h,t,coefficient(interval,0x00000000u,0x38752140u,0x00000000u,0x00000000u,0x39ab4d5fu,0x00000000u,0x00000000u))),...);
      ((h=fma(h,t,coefficient(interval,0x00000000u,0xb9183513u,0xb947bd66u,0xb58f6d10u,0xb9c08feeu,0xb59171b1u,0x00000000u))),...);
      ((h=fma(h,t,coefficient(interval,0x34facb37u,0x398d9ee0u,0x39dfe5a4u,0x36863471u,0x3a2bf5fau,0x3671d749u,0x00000000u))),...);
      ((h=fma(h,t,coefficient(interval,0xb63a0d2du,0xba2fdf3au,0xba4a3864u,0xb758ec9du,0xbaa65b2au,0xb72757e1u,0xb811c415u))),...);
      ((h=fma(h,t,coefficient(interval,0x37813497u,0x3ae1130du,0x3ae2b84bu,0x384b3fb7u,0x3b1584c1u,0x380d4deeu,0x38c8e73cu))),...);
      ((h=fma(h,t,coefficient(interval,0xb8bfb3f8u,0xbb8bc302u,0xbb813c08u,0xb9404721u,0xbb86bc79u,0xb8f4646bu,0xb980a2b8u))),...);
      ((h=fma(h,t,coefficient(interval,0x3a0e6d24u,0x3c2d6773u,0x3c1129ceu,0x3a356f7bu,0x3bf71905u,0x39d46b54u,0x3a37296bu))),...);
      ((h=fma(h,t,coefficient(interval,0xbb535f6cu,0xbcd7a178u,0xbca3c0b8u,0xbb2d3d49u,0xbc67da1fu,0xbabdbc02u,0xbb06690eu))),...);
      ((h=fma(h,t,coefficient(interval,0x3c9d20e4u,0x3d86d45du,0x3d3bcdf4u,0x3c2a7e14u,0x3ce35ff2u,0x3bb1ed8au,0x3bcea86au))),...);
      ((h=fma(h,t,coefficient(interval,0xbded544du,0xbe2e2df9u,0xbde4f8bcu,0xbd36d397u,0xbd76f5e5u,0xbcb95c19u,0xbcb08499u))),...);
      ((h=fma(h,t,coefficient(interval,0x3f5c6e3eu,0x3f14c222u,0x3ec662fcu,0x3e9091d7u,0x3e48ced7u,0x3e10d0b5u,0x3de229ecu))),...);
      auto [...result]=std::array{B::encode(x*h)...};
      ((result=select(result>U(0x3f800000u),U(0x3f800000u),result) |
        (word & U(0x80000000u))),...);
      // Preserve tiny normal inputs and signed zeros without arithmetic, saturate
      // infinities, and use the scalar contract's canonical NaN result.
      ((result=select(magnitude>=U(0x41200000u),
        (word & U(0x80000000u)) | U(0x3f800000u),result)),...);
      ((result=select(magnitude<=U(0x39800000u),word,result)),...);
      ((result=select(magnitude>U(0x7f800000u),U(0x7fc00000u),result)),...);
      return {{B::decode(result)...}};
    }
  }
}

namespace ftz::detail::native {
  // Word admission and range reduction feed one copy of the scalar log1p
  // polynomial. Direct log1p lanes select their original argument; other lanes
  // select the reduced mantissa. No lane is evaluated by a scalar fallback.
  // Typed inputs are already canonical. Manual tiny lanes bypass squaring;
  // hardware tiny lanes may underflow, but return their original words. All
  // observed intermediates are bounded by tests/log/verify_bounds.py.
  template <bool OnePlus, bool Hardware, float_register V, std::size_t N>
  native_flatten native_inline std::array<V,N> log_ftz(std::array<V,N> const & input) noexcept {
    using B=detail::fp32_bit_bridge<V>;
    using U=typename B::bits_type;
    using I=typename V::template rebind<std::int32_t>;
    if constexpr (N==0) return {};
    else {
      auto const & [...original]=input;
      auto const [...word]=std::array{B::encode(original)...};
      auto const [...magnitude]=std::array{(word & U(0x7fffffffu))...};
      auto const [...valid]=[&] {
        if constexpr (OnePlus) return std::array{((magnitude < U(0x7f800000u)) &
          (((word & U(0x80000000u)) == U(0)) | (magnitude < U(0x3f800000u))))...};
        else return std::array{((word > U(0)) & (word < U(0x7f800000u)))...};
      }();
      auto const [...direct]=[&] {
        if constexpr (OnePlus) return std::array{(valid & (magnitude <=
          select((word & U(0x80000000u)) != U(0),U(0x3f000000u),U(0x3f800000u))))...};
        else return std::array<typename V::mask,0>{};
      }();
      auto const [...positive]=[&] {
        if constexpr (OnePlus) return std::array{B::encode(V(1.0f) +
          B::decode(select(valid & !direct,word,U(0))))...};
        else return std::array{select(valid,word,U(0x3f800000u))...};
      }();
      auto [...exponent]=std::array{(positive.template right<23>() - U(127))...};
      auto [...mantissa]=std::array{((positive & U(0x007fffffu)) | U(0x3f800000u))...};
      auto const [...upper]=std::array{(mantissa >= U(0x3fc00000u))...};
      ((mantissa=mantissa-select(upper,U(0x00800000u),U(0))),...);
      ((exponent=exponent+select(upper,U(1),U(0))),...);
      auto const [...reduced]=std::array{(B::decode(mantissa)-V(1.0f))...};
      auto const [...argument]=[&] {
        if constexpr (OnePlus && Hardware) return std::array{select(direct,B::decode(word),reduced)...};
        else if constexpr (OnePlus) return std::array{select(direct,
          B::decode(select(magnitude>U(0x33000000u),word,U(0))),reduced)...};
        else return std::array{reduced...};
      }();
      auto const [...negative]=std::array{((B::encode(argument) & U(0x80000000u)) != U(0))...};
      auto coefficient=[](auto mask,unsigned negative_word,unsigned positive_word) noexcept {
        return B::decode(select(mask,U(negative_word),U(positive_word)));
      };
      auto const [...square]=std::array{(argument*argument)...};
      auto const [...t]=std::array{fma(argument,
        coefficient(negative,0x40800000u,0x40000000u),
        coefficient(negative,0x3f800000u,0xbf800000u))...};
      auto [...h]=std::array{coefficient(negative,0x00000000u,0xb29c7ee2u)...};
      ((h=fma(h,t,coefficient(negative,0x00000000u,0x3378ea39u))),...);
      ((h=fma(h,t,coefficient(negative,0xb44f5480u,0xb3faaccbu))),...);
      ((h=fma(h,t,coefficient(negative,0x352754efu,0x34c9e1cdu))),...);
      ((h=fma(h,t,coefficient(negative,0xb5bb75dbu,0xb5b13b5eu))),...);
      ((h=fma(h,t,coefficient(negative,0x369a1c19u,0x36902a0au))),...);
      ((h=fma(h,t,coefficient(negative,0xb7866f43u,0xb76af011u))),...);
      ((h=fma(h,t,coefficient(negative,0x3861235au,0x38423d8au))),...);
      ((h=fma(h,t,coefficient(negative,0xb93e98dfu,0xb9225d51u))),...);
      ((h=fma(h,t,coefficient(negative,0x3a24a041u,0x3a0988b0u))),...);
      ((h=fma(h,t,coefficient(negative,0xbb117f6au,0xbaed1a41u))),...);
      ((h=fma(h,t,coefficient(negative,0x3c04b7c5u,0x3bd13ce0u))),...);
      ((h=fma(h,t,coefficient(negative,0xbcfda364u,0xbcbeef90u))),...);
      ((h=fma(h,t,coefficient(negative,0x3e029133u,0x3db786beu))),...);
      ((h=fma(h,t,coefficient(negative,0xbf1a5884u,0xbec19b82u))),...);
      auto const [...polynomial]=std::array{fma(square,h,argument)...};
      auto const [...e]=std::array{convert<float>(I::from_native(
        std::bit_cast<typename I::native_type>(exponent.to_native())))...};
      auto const [...low]=std::array{fma(e,B::decode(U(0x35bfbe8eu)),polynomial)...};
      auto [...result]=std::array{B::encode(fma(e,B::decode(U(0x3f317200u)),low))...};
      if constexpr (OnePlus) {
        ((result=select(direct,B::encode(polynomial),result)),...);
        ((result=select(magnitude<=U(0x33000000u),word,result)),...);
        ((result=select(word==U(0xbf800000u),U(0xff800000u),result)),...);
        ((result=select(word==U(0x7f800000u),word,result)),...);
        ((result=select((magnitude>U(0x7f800000u)) |
          (((word & U(0x80000000u)) != U(0)) & (magnitude>U(0x3f800000u))),U(0x7fc00000u),result)),...);
      } else {
        ((result=select(word==U(0x7f800000u),word,result)),...);
        ((result=select((word & U(0x80000000u)) != U(0),U(0x7fc00000u),result)),...);
        ((result=select(magnitude==U(0),U(0xff800000u),result)),...);
        ((result=select(magnitude>U(0x7f800000u),U(0x7fc00000u),result)),...);
      }
      return {{B::decode(result)...}};
    }
  }
}

namespace ftz::detail::native {
  // The normalized reciprocal and polynomial follow the scalar atan2 graph.
  // Inputs are canonical FTZ values. Bit-built mantissas stay in [1,2) even
  // for axis/nonfinite lanes, whose outputs are reconstructed at the end.
  // Altered coefficient use from SLEEF 3.9.0 atan2kf, under Boost 1.0 below.
  template<bool Hardware,float_register V,std::size_t N>
  native_flatten native_inline std::array<V,N> atan2_ftz(
      std::array<V,N> const & y_input,std::array<V,N> const & x_input) noexcept {
    using B=detail::fp32_bit_bridge<V>;
    using U=typename B::bits_type;
    using I=typename V::template rebind<std::int32_t>;
    if constexpr(N==0) return {};
    else {
      auto constant=[](unsigned word) noexcept {return B::decode(U(word));};
      auto integer=[](U word) noexcept {
        return I::from_native(std::bit_cast<typename I::native_type>(word.to_native()));
      };
      auto const & [...y]=y_input;
      auto const & [...x]=x_input;
      auto const [...yw]=std::array{B::encode(y)...};
      auto const [...xw]=std::array{B::encode(x)...};
      auto const [...ay]=std::array{(yw & U(0x7fffffffu))...};
      auto const [...ax]=std::array{(xw & U(0x7fffffffu))...};
      auto const [...swap]=std::array{(ay>ax)...};
      auto const [...negative_x]=std::array{((xw & U(0x80000000u)) != U(0))...};
      auto const [...a]=std::array{select(swap,ax,ay)...};
      auto const [...b]=std::array{select(swap,ay,ax)...};
      auto const [...ma]=std::array{B::decode((a & U(0x007fffffu)) | U(0x3f800000u))...};
      auto const [...mb]=std::array{((b & U(0x007fffffu)) | U(0x3f800000u))...};
      auto const [...m]=std::array{B::decode(mb)...};
      auto [...r]=std::array{B::decode(U(0x7ef311c3u)-mb)...};
      auto [...e]=std::array{fma(-m,r,V(1.0f))...};
      ((r=fma(r,e,r)),...);
      ((e=fma(-m,r,V(1.0f))),...);
      ((r=fma(r,e,r)),...);
      ((e=fma(-m,r,V(1.0f))),...);
      ((r=fma(r,e,r)),...);
      ((r=ma*r),...);
      auto const [...word]=std::array{B::encode(r)...};
      auto const [...fraction]=std::array{(word & U(0x007fffffu))...};
      auto const [...exponent]=std::array{(word.template right<23>()+
        a.template right<23>()-b.template right<23>())...};
      auto [...ratio_word]=std::array{select(integer(exponent)>I(0),
        exponent.template left<23>() | fraction,
        select((exponent==U(0)) & (fraction==U(0x007fffffu)),U(0x00800000u),U(0)))...};
      ((ratio_word=select(ratio_word>U(0x3f800000u),U(0x3f800000u),ratio_word)),...);
      auto const [...tiny]=std::array{(ratio_word<=U(0x39800000u))...};
      auto const [...ratio]=std::array{B::decode(ratio_word)...};
      auto const [...t]=[&] {
        if constexpr(Hardware) return std::array{ratio...};
        else return std::array{B::decode(select(tiny,U(0),ratio_word))...};
      }();
      auto const [...z]=std::array{(t*t)...};
      auto [...h]=std::array{fma(constant(0x3b390ccdu),z,constant(0xbc82b80du))...};
      ((h=fma(h,z,constant(0x3d2e19b6u))),...);
      ((h=fma(h,z,constant(0xbd995ffau))),...);
      ((h=fma(h,z,constant(0x3dd9ccf2u))),...);
      ((h=fma(h,z,constant(0xbe116f9fu))),...);
      ((h=fma(h,z,constant(0x3e4cb9a7u))),...);
      ((h=fma(h,z,constant(0xbeaaaa5du))),...);
      auto const [...q]=std::array{(z*h)...};
      auto [...angle]=std::array{fma(q,t,ratio)...};
      ((angle=select(tiny,ratio,angle)),...);
      ((angle=select(swap,fma(V(-1.0f),angle,constant(0x3fc90fdbu)),angle)),...);
      ((angle=select(negative_x,fma(V(-1.0f),angle,constant(0x40490fdbu)),angle)),...);
      auto [...result]=std::array{B::encode(angle)...};
      auto const [...axis]=std::array{select(negative_x,U(0x40490fdbu),U(0))...};
      ((result=select(ax==U(0),U(0x3fc90fdbu),result)),...);
      ((result=select(ax==U(0x7f800000u),axis,result)),...);
      ((result=select(ay==U(0x7f800000u),select(ax==U(0x7f800000u),
        select(negative_x,U(0x4016cbe4u),U(0x3f490fdbu)),U(0x3fc90fdbu)),result)),...);
      ((result=select(ay==U(0),axis,result)),...);
      ((result=result | (yw & U(0x80000000u))),...);
      ((result=select((ay>U(0x7f800000u)) | (ax>U(0x7f800000u)),U(0x7fc00000u),result)),...);
      return {{B::decode(result)...}};
    }
  }
}

/*
Copyright Naoki Shibata and contributors 2010 - 2025.
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
