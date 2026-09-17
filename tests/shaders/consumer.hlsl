// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <ftz/ftz32.h>
[[vk::binding(0, 0)]] StructuredBuffer<uint4> inputs;
// The caller supplies eight uint4 output records per input record.
[[vk::binding(1, 0)]] RWStructuredBuffer<uint4> outputs;
[numthreads(64, 1, 1)]
void consumer(uint3 id : SV_DispatchThreadID) {
  uint count, stride;
  inputs.GetDimensions(count, stride);
  if (id.x >= count) return;
  uint4 words = inputs[id.x];
  ftz::ftz32 a = ftz::ftz32::from_bits(words.x);
  ftz::ftz32 b = ftz::ftz32::from_bits(words.y);
  ftz::ftz32 c = ftz::ftz32::from_bits(words.z);
  outputs[8 * id.x] = uint4((a + b).to_bits(), (a - b).to_bits(), (a * b).to_bits(), (a / b).to_bits());
  outputs[8 * id.x + 1] = uint4(ftz::fma(a, b, c).to_bits(), ftz::sin(a).to_bits(), ftz::cos(a).to_bits(), ftz::exp(a).to_bits());
  outputs[8 * id.x + 2] = uint4(ftz::expm1(a).to_bits(), ftz::atan2(a, b).to_bits(), ftz::tanh(a).to_bits(), ftz::sqrt(a).to_bits());
  outputs[8 * id.x + 3] = uint4(ftz::log(a).to_bits(), ftz::log1p(a).to_bits(), a < b, a == b);
  ftz::ftz32_sincos_result pair = ftz::sincos(a);
  outputs[8 * id.x + 4] = uint4(pair.sine.to_bits(), pair.cosine.to_bits(), ftz::neg(a).to_bits(), ftz::abs(a).to_bits());
  outputs[8 * id.x + 5] = uint4(ftz::copysign(a, b).to_bits(), ftz::ftz32::from_float(a.to_float()).to_bits(),
    uint(ftz::isnan(a)) | (uint(ftz::isinf(a)) << 1), uint(ftz::isfinite(a)) | (uint(ftz::signbit(a)) << 1));
  outputs[8 * id.x + 6] = uint4((a + .25f).to_bits(), (a - .25f).to_bits(), (a * .25f).to_bits(), (a / .25f).to_bits());
  outputs[8 * id.x + 7] = uint4(a <= b, a >= b, a != b, a > b);
}
