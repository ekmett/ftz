// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <ftz/ftz32.h>
[[vk::binding(0,0)]] StructuredBuffer<uint2> inputs;
[[vk::binding(1,0)]] RWStructuredBuffer<uint> outputs;
[numthreads(64,1,1)]
void exp_check(uint3 id : SV_DispatchThreadID) {
  uint count, stride;
  inputs.GetDimensions(count,stride);
  if(id.x<count) outputs[id.x]=ftz::detail::ftz32_exp(inputs[id.x].x);
}
