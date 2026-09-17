// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <ftz/ftz32.h>
struct dispatch_data { uint count; };
[[vk::binding(1, 0)]] StructuredBuffer<uint4> inputs;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint4> outputs;
[[vk::push_constant]] ConstantBuffer<dispatch_data> dispatch;
[numthreads(64, 1, 1)]
void values(uint3 id : SV_DispatchThreadID) {
  if (id.x >= dispatch.count) return;
  uint4 input = inputs[id.x];
  ftz::ftz32 a = ftz::ftz32::from_bits(input.x);
  ftz::ftz32 b = ftz::ftz32::from_bits(input.y);
  precise float sum = a.to_float() + b.to_float();
  precise float difference = a.to_float() - b.to_float();
  outputs[id.x] = uint4(asuint(sum), (a + b).to_bits(),
    asuint(difference), (a - b).to_bits());
}
