// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <ftz/ftz32.h>
#ifndef FTZ_TEST_EXP_DEGREE
#define FTZ_TEST_EXP_DEGREE 6
#endif
[[vk::binding(0,0)]] StructuredBuffer<uint2> inputs;
[[vk::binding(1,0)]] RWStructuredBuffer<uint> outputs;
[numthreads(64,1,1)]
void exp_check(uint3 id : SV_DispatchThreadID) {
  uint count, stride;
  inputs.GetDimensions(count,stride);
  if(id.x<count) outputs[id.x]=ftz::exp<FTZ_TEST_EXP_DEGREE>(ftz::ftz32::from_bits(inputs[id.x].x)).to_bits();
}
