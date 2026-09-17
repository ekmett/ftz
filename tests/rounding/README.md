# Directed rounding

Import-only tests cover `floor`, `ceil` and `trunc` for both FTZ policies,
scalar values, native and padded-short vectors, arrays, and generic wide values.
An integer-only IEEE binary32 oracle clears fractional significand bits and
carries one integer unit when the selected direction requires it. Tests compare
all non-NaN words exactly, including signed zero and infinity; NaNs must remain
NaNs. The bank surrounds fractional/integer boundaries through the last binade
with fractional values, includes minimum normal and maximum finite inputs, and
adds deterministic word samples. Inputs are normalized at the public factory.

The same checks run under all four standard rounding modes. Manual policy runs
with gradual and flush controls; hardware policy runs with flush controls.
This specifically qualifies these directed-rounding operations independently of
the ambient mode, not the rest of FTZ arithmetic outside its nearest-even contract.
Empty arrays/wides and one-conversion forwarding are included. Typed codegen
entry points isolate the rounding operation from float-import normalization.

Configure this directory against installed matching FTZ and SIMD packages;
set `ROUNDING_PROFILES=AVX2;AVX512` or `NEON` for admitted architectures, build,
and run CTest. No GPU execution is involved.

`shaders/` is a separate `LANGUAGES NONE` package consumer. It compiles both
shader policies to DXIL and validated SPIR-V, then translates to Metal. Its
one-record-per-input kernel uses uint4 inputs at binding 1 and uint4 outputs
at binding 2, a uint count push constant, and 64-thread groups. Input.x supplies
a canonical word; outputs are floor/ceil/trunc/input echo. Compilation alone does not qualify shader
signed-zero behavior on a GPU. The fixture is suitable for a separate small
rounding bank without running the full numerical matrix.

Pass an output prefix to a rounding test executable to capture the same
integer-oracle bank without executing floating-point work. It writes
`<prefix>.inputs.bin` and `<prefix>.expected.bin`, both flat uint4 records.
The first three output columns compare non-NaN words exactly and require a NaN
for a NaN expectation; NaN payload/sign need not match. The echo column must
match exactly. This shares the CPU oracle rather than maintaining a second graph.

On 2026-09-17, the shared 276-record bank passed both shader policies on an RTX
4090 using the HLSL 2021/DXC Vulkan path at source `2c0da07`. All non-NaN results,
signed zeros and input echoes matched the integer oracle; input/tail guards and
Vulkan validation passed. This is sampled device execution evidence. SPIR-V was
translated to Metal source, but no Metal compilation or execution is claimed.
