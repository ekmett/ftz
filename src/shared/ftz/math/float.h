#pragma once
#include "ftz/config.h"
#include "native/attributes.h"
#ifdef __cplusplus
#include <bit>
#include <cmath>
#endif

// Selected for an entire compiled helper graph, never per arithmetic operation.
// Explicit normalization is the default.
// Hardware identity is suitable only where the caller has qualified the graph
// and its native environment. It does not configure CPU or GPU FP controls.
#ifndef FTZ_FP32_HARDWARE_FTZ
#define FTZ_FP32_HARDWARE_FTZ 0
#endif
#if FTZ_FP32_HARDWARE_FTZ != 0 && FTZ_FP32_HARDWARE_FTZ != 1
#error FTZ_FP32_HARDWARE_FTZ must be 0 or 1
#endif

namespace ftz { namespace detail { namespace math {
  native_nodiscard native_inline native_const float fp32_decode(unsigned int bits) {
#ifdef __cplusplus
    return std::bit_cast<float>(bits);
#else
    return asfloat(bits);
#endif
  }
  native_nodiscard native_inline native_const unsigned int fp32_encode(float value) {
#ifdef __cplusplus
    return std::bit_cast<unsigned int>(value);
#else
    return asuint(value);
#endif
  }
  template <bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  inline float fp32_ftz(float value) {
    if (Hardware) return value;
    unsigned int bits = fp32_encode(value);
    return fp32_decode((bits & 0x7f800000u) == 0u ? bits & 0x80000000u : bits);
  }
  // Flush=false is for graphs that already bound their intermediates, or
  // perform their own final repair. It must not add per-operation FTZ work.
  template <bool Flush = true, bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_nodiscard native_inline float fp32_add(float a, float b) {
#ifdef __cplusplus
    float result = a + b;
#else
    precise float result = a + b;
#endif
    return Flush ? fp32_ftz<Hardware>(result) : result;
  }
  template <bool Flush = true, bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_nodiscard native_inline float fp32_mul(float a, float b) {
#ifdef __cplusplus
    float result = a * b;
#else
    precise float result = a * b;
#endif
    return Flush ? fp32_ftz<Hardware>(result) : result;
  }
  template <bool Flush = true, bool Hardware = FTZ_FP32_HARDWARE_FTZ != 0>
  native_nodiscard native_inline float fp32_fma(float a, float b, float c) {
#ifdef __cplusplus
    float result = std::fma(a, b, c);
#else
    precise float result = mad(a, b, c);
#endif
    return Flush ? fp32_ftz<Hardware>(result) : result;
  }
#ifndef __cplusplus
  inline float2 fp32_ftz(float2 value) {
#if FTZ_FP32_HARDWARE_FTZ
    return value;
#else
    uint2 bits = asuint(value);
    uint2 nonzero = uint2((bits & 0x7f800000u) != 0u);
    return asfloat(bits & ((0u - nonzero) | 0x80000000u));
#endif
  }
  inline float3 fp32_ftz(float3 value) {
#if FTZ_FP32_HARDWARE_FTZ
    return value;
#else
    uint3 bits = asuint(value);
    uint3 nonzero = uint3((bits & 0x7f800000u) != 0u);
    return asfloat(bits & ((0u - nonzero) | 0x80000000u));
#endif
  }
  inline float4 fp32_ftz(float4 value) {
#if FTZ_FP32_HARDWARE_FTZ
    return value;
#else
    uint4 bits = asuint(value);
    uint4 nonzero = uint4((bits & 0x7f800000u) != 0u);
    return asfloat(bits & ((0u - nonzero) | 0x80000000u));
#endif
  }
#endif
}}}

/**
 * \file
 * \license
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
 * \endlicense
 * \author Edward Kmett <ekmett@gmail.com>
 * \brief FP32 bit casts, precise arithmetic and signed flush-to-zero normalization.
 */
