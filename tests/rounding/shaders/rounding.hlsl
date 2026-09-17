// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <ftz/ftz32.h>
// Inputs are canonical words supplied by the caller. This fixture isolates
// the rounding operations from normalization at a raw-float import boundary.
struct parameters { uint count; };
[[vk::push_constant]] ConstantBuffer<parameters> args;
[[vk::binding(1,0)]] StructuredBuffer<uint4> inputs;
[[vk::binding(2,0)]] RWStructuredBuffer<uint4> outputs;
[numthreads(64,1,1)]
void rounding(uint3 id:SV_DispatchThreadID) {
  if(id.x>=args.count)return;
  uint word=inputs[id.x].x;
  ftz::ftz32 value=ftz::ftz32::unsafe_from_float32(asfloat(word));
  outputs[id.x]=uint4(ftz::floor(value).to_bits(),ftz::ceil(value).to_bits(),ftz::trunc(value).to_bits(),word);
}
