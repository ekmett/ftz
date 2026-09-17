#pragma once
#include "ftz/config.h"
#include "ftz/math/float.h"

namespace ftz { namespace detail { namespace math {

  // The cutoff proof excludes the minimum-normal rounding strip for finite
  // input. Two normal-factor products and signed FTZ therefore agree with the
  // reference scaler. Tiny inputs round to exactly one without an entry mask.
  uint2 exp_value(unsigned int bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u) return uint2(0u, 0u);
    precise float x = min(max(asfloat(bits), asfloat(0xc2d00000u)), asfloat(0x42b17218u));
    precise float product = x * asfloat(0x3fb8aa3bu);
    precise float n = round(product);
    precise float first = fp32_fma<false>(n, asfloat(0xbf317200u), x);
    precise float r = fp32_fma<false>(n, asfloat(0xb5bfbe8eu), first);
    precise float y = fp32_fma<false>(r, asfloat(0x3950eb8au), asfloat(0x3ab6d3abu));
    y = fp32_fma<false>(r, y, asfloat(0x3c08882eu));
    y = fp32_fma<false>(r, y, asfloat(0x3d2aaa32u));
    y = fp32_fma<false>(r, y, asfloat(0x3e2aaaabu));
    y = fp32_fma<false>(r, y, 0.5f);
    y = fp32_fma<false>(r, y, 1.0f);
    y = fp32_fma<false>(r, y, 1.0f);
    if (!(n >= -150.0f && n <= 128.0f)) return uint2(0u, 0u);
    precise float normal_index = min(max(n, -126.0f), 127.0f);
    precise float first_index = n - normal_index;
    float first_factor = asfloat((unsigned int)((int)first_index + 127) << 23);
    float second_factor = asfloat((unsigned int)((int)normal_index + 127) << 23);
    precise float scaled = y * first_factor;
    precise float value = scaled * second_factor;
    return uint2(asuint(fp32_ftz(value)), 1u);
  }

}}}

namespace ftz { namespace detail { namespace math {
  struct expm1_result {
    float value;
    unsigned int valid;
  };
  // Internal finite argument <=1; clamped below. Callers apply signed input FTZ.
  float expm1_core(float input) {
    precise float x = max(input, -18.0f);
    precise float product = x * asfloat(0x3fb8aa3bu);
    precise float n = round(product);
    precise float r0 = fp32_fma<false>(n, asfloat(0xbf317200u), x);
    precise float r1 = fp32_fma<false>(n, asfloat(0xb5bfbe8eu), r0);
    precise float r = n == 0.0f ? x : r1;
    bool tiny = (asuint(r) & 0x7fffffffu) <= 0x33000000u;
#if FTZ_FP32_HARDWARE_FTZ
    precise float t = r;
#else
    precise float t = tiny ? 0.0f : r;
#endif
    precise float z = t * t;
    precise float h0 = asfloat(0x3493f27eu);
    precise float h1 = fp32_fma<false>(t, h0, asfloat(0x3638ef1du));
    precise float h2 = fp32_fma<false>(t, h1, asfloat(0x37d00d01u));
    precise float h3 = fp32_fma<false>(t, h2, asfloat(0x39500d01u));
    precise float h4 = fp32_fma<false>(t, h3, asfloat(0x3ab60b61u));
    precise float h5 = fp32_fma<false>(t, h4, asfloat(0x3c088889u));
    precise float h6 = fp32_fma<false>(t, h5, asfloat(0x3d2aaaabu));
    precise float h7 = fp32_fma<false>(t, h6, asfloat(0x3e2aaaabu));
    precise float h8 = fp32_fma<false>(t, h7, asfloat(0x3f000000u));
    precise float fused = fp32_fma<false>(z, h8, r);
    precise float p = tiny ? r : fused;
    int index = (int)max(n, -24.0f);
    precise float scale = asfloat((unsigned int)(index + 127) << 23u);
    precise float offset = scale - 1.0f;
    precise float scaled = fp32_fma<false>(scale, p, offset);
    precise float result = n == 0.0f ? p : scaled;
    result = n == -25.0f ? (p > 0.0f ? asfloat(0xbf7fffffu) : -1.0f) : result;
    result = n < -25.0f ? -1.0f : result;
    // Entry-canonicalized graph outputs are already normal or signed zero.
    return fp32_ftz(result);
  }
  expm1_result expm1_checked(float input) {
    unsigned int word = asuint(input), magnitude = word & 0x7fffffffu;
    bool valid = magnitude < 0x7f800000u &&
      ((word & 0x80000000u) != 0u || magnitude <= 0x3f800000u);
    word = valid ? word : 0u;
    word = (word & 0x7f800000u) == 0u ? (word & 0x80000000u) : word;
    precise float evaluated = expm1_core(asfloat(word));
    expm1_result result;
    result.value = valid ? evaluated : 0.0f;
    result.valid = valid ? 1u : 0u;
    return result;
  }
  expm1_result damping_gain_checked(float input) {
    unsigned int word = asuint(input), magnitude = word & 0x7fffffffu;
    bool valid = magnitude < 0x7f800000u &&
      ((word & 0x80000000u) == 0u || magnitude == 0u);
    word = valid ? word : 0u;
    word = (word & 0x7f800000u) == 0u ? (word & 0x80000000u) : word;
    precise float evaluated = expm1_core(asfloat(word ^ 0x80000000u));
    expm1_result result;
    result.value = asfloat(valid ? (asuint(evaluated) ^ 0x80000000u) : 0u);
    result.valid = valid ? 1u : 0u;
    return result;
  }

}}}



// Altered source: Pommier coefficients and reducer, signed input FTZ,
// mask-before-square and bitwise quadrant reconstruction. Original notices below.
// Require RNE, fused mad, and precise non-fused stages.
namespace ftz { namespace detail { namespace math {
  struct sincos_result { float sine, cosine; };

  sincos_result trig_ftz_graph(float v, bool bounded) {
    uint bits = asuint(v);
    uint canonical = (bits & 0x7f800000u) == 0 ? bits & 0x80000000u : bits;
    uint magnitude = canonical & 0x7fffffffu;
    uint index = 0u;
    precise float x;
    if (bounded) {
      precise float product = asfloat(magnitude) * asfloat(0x3fa2f983u);
      index = ((uint)product + 1u) & 0xfffffffeu;
      precise float multiple = (float)index;
      precise float r0 = fp32_fma<false>(multiple, asfloat(0xbf490000u), asfloat(magnitude));
      precise float r1 = fp32_fma<false>(multiple, asfloat(0xb97da000u), r0);
      x = fp32_fma<false>(multiple, asfloat(0xb3222169u), r1);
    } else {
      x = asfloat(canonical);
    }
    bool active = (asuint(x) & 0x7fffffffu) > 0x39800000u;
#if FTZ_FP32_HARDWARE_FTZ
    precise float t = x;
#else
    precise float t = asfloat(active ? asuint(x) : 0u);
#endif
    precise float z = t * t;
    precise float s0 = fp32_fma<false>(asfloat(0xb94ca1f9u), z, asfloat(0x3c08839eu));
    precise float c0 = fp32_fma<false>(asfloat(0x37ccf5ceu), z, asfloat(0xbab6061au));
    precise float s1 = fp32_fma<false>(s0, z, asfloat(0xbe2aaaa3u));
    precise float c1 = fp32_fma<false>(c0, z, asfloat(0x3d2aaaa5u));
    precise float s2 = s1 * z;
    precise float c2 = c1 * z;
    precise float c3 = c2 * z;
    precise float half_z = z * 0.5f;
    precise float c4 = c3 - half_z;
    precise float s3 = fp32_fma<false>(s2, t, x);
    precise float c5 = c4 + 1.0f;
    uint sine = active ? asuint(s3) : asuint(x);
    uint cosine = asuint(c5);
    sincos_result result;
    if (bounded) {
      bool swap_pair = (index & 2u) != 0;
      result.sine = asfloat((swap_pair ? cosine : sine) ^
        (canonical & 0x80000000u) ^ ((index & 4u) << 29));
      result.cosine = asfloat((swap_pair ? sine : cosine) ^
        ((~(index - 2u) & 4u) << 29));
    } else {
      result.sine = asfloat(sine);
      result.cosine = asfloat(cosine);
    }
    return result;
  }

  // Finite |x| <= 1; subnormal inputs become signed zero.
  sincos_result sincos_reduced_ftz(float x) { return trig_ftz_graph(x, false); }
  // Finite |x| < 8192. One shared three-FMA reducer.
  sincos_result sincos_ftz(float x) { return trig_ftz_graph(x, true); }
  float sin_reduced_ftz(float x) { return sincos_reduced_ftz(x).sine; }
  float cos_reduced_ftz(float x) { return sincos_reduced_ftz(x).cosine; }
  float sin_ftz(float x) { return sincos_ftz(x).sine; }
  float cos_ftz(float x) { return sincos_ftz(x).cosine; }
}}}

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
 * \brief Shader polynomials and range reduction for exp, expm1, sine, cosine, and damping gain.
 */
