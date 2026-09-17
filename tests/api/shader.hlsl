// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <ftz/ftz32.h>
[[vk::binding(1,0)]] StructuredBuffer<uint4> inputs;
[[vk::binding(2,0)]] RWStructuredBuffer<uint4> outputs;
//! [shader_values]
[numthreads(64,1,1)] void shader(uint3 id : SV_DispatchThreadID) {
  // Shader policy is chosen at compile time with FTZ_FP32_HARDWARE_FTZ.
  ftz::ftz32 value = ftz::ftz32::from_bits(inputs[id.x].x);
  ftz::ftz32_sincos_result pair = ftz::sincos(value); // Named sine/cosine fields in HLSL.
  ftz::ftz32 fused = ftz::fma(value, ftz::ftz32::from_float(2.f),
    ftz::ftz32::from_float(1.f));
  outputs[id.x] = uint4(pair.sine.to_bits(), pair.cosine.to_bits(),
    fused.to_bits(), ftz::floor(value).to_bits());
}
//! [shader_values]
