# Validation

FTZ checks three separate properties: the specified arithmetic graph, the CPU or
shader environment needed by that graph, and the types and definitions visible
through an installed package. Packet equality covers the recorded inputs;
exact-rational bounds establish only the stated intermediate-value theorems.

## Native numerical checks

The default CMake test build exercises both arithmetic policies. Manual `m32`
runs under gradual and flush controls; hardware `h32` runs under admitted flush
controls. Hardware policy under gradual controls is a negative admission case.
The tests save and restore complete caller FP state, including status bits.

The maintained suites cover:

- Scalar, short-vector and native-register arithmetic, typed memory and tails.
- Independent FTZ policies, mixed-policy rejection, owning swizzles and masks.
- Arrays and wide packs with zero, one and several independent registers.
- Classification, sign transport and directed rounding under all four modes.
- Exponential, logarithmic and trigonometric operation graphs, including their
  domain boundaries, exceptional values and signed zeros.
- Module type identity, family-typed architecture arguments, scalar mask traits
  and the provider's default architecture.

The exponential contract fixture checks 299,247 inputs, including dense binary32
windows at both output cutoffs and every reduction transition. Scalar and SIMD
results under gradual and flush CPU controls match an independent graph oracle.
The recorded Apple M3 and AVX2 packets have the same SHA-256:
`64bc518d4fae9be61ea826d2795a8933423fb6f5dc0e81a43d3d647076c8fd41`.
The [DXC → SPIR-V → Metal fixture](../tests/exp_shader/README.md) under both
shader policies matches that bank on
Apple M3, with NaN payload differences permitted. This is sampled cross-platform
agreement, not an MPFR accuracy bound or an exhaustive proof for every input.
Actual Windows and AVX-512 runtime qualification is separate from cross-compilation.

General FTZ scaling now has its own vector integer exponent graph, including
signed flushing, nonfinite cases and the minimum-normal rounding boundary.
On Apple AArch64, the unchanged 40,600-row independent dyadic oracle passes
3,572,844 word checks per admitted run across widths 1, 2, 3 and 4: m32 under
gradual and flush controls, and h32 under flush controls. The h32 gradual
case rejects admission. Merge/zero masks retain inactive words exactly, and raw
scaling forwarders are unavailable when the target has no scaling instruction.
Conversion noexcept and discarded-result side-effect checks pass for both
policies; this dependency build disables exceptions, so it does not exercise
throw/unwind paths. The complete local suites pass 23/23 on Apple AArch64
and 24/24 on Linux AVX2, including gradual, DAZ-only, FTZ-only and flushing x86
scaling checks. Actual AVX-512 and Windows execution remain separate qualification
gates. See the [kernel plan](math-kernels.md).

The common-width numerical packets have these reference identities:

| Packet | Binary32 words | SHA-256 |
| --- | ---: | --- |
| tanh | 325,044 | `ba5ebdbc1867b91108353b64a85259677e711fcdf1006058916f497aae7ce991` |
| log and log1p | 2,023,656 | `6c24a76198a329ecae0dba9525c1a047840fb0a808bd120a206a7504f75f07d8` |
| separate sin/cos and paired sincos | 318,120 | `edd127a9baa992bf7be564b0a58cd420c46e8287eaad465ef8d3b90bab260209` |
| atan2 | 21,768 | `4f0cae730863e2a2d65546ff371bedb5ca13848acd9479869e0ade68949e5f12` |

`tests/compare_fp32.py` permits NaN representation differences. Every non-NaN
word, including signed zero, must match. The core fixture also checks its
2,208-word arithmetic/function golden. These are sampled regressions of an
operation graph, not exhaustive libm accuracy proofs or a promise of identical
NaN payloads across devices.

The packed code-generation fixtures check ordinary optimized leaves separately
from runtime results. Large packs may spill. Multiply and FMA retain the rare
boundary repairs needed for the FTZ contract; hardware policy does not promise
one instruction per public operation.

## Installed consumers

The [package fixture](https://github.com/ekmett/ftz/blob/main/tests/package/README.md)
builds a third static library that exports an FTZ vector operation through its
own named module. PCH and IPO are enabled. The
[API examples](https://github.com/ekmett/ftz/blob/main/tests/api/README.md)
compile scalar policy, FP-environment and vector/array/wide usage.

Hosted CI moves both dependency prefixes to paths containing spaces before
building these consumers. Consumers regenerate compatible BMIs from installed
sources without using the producer's compiler cache. Header-only shader
consumers must not load either host archive. Compilation and package relocation
do not by themselves establish CPU instruction admission or GPU execution.

## Compiler cache

The launcher unit suite checks argument preservation, conservative bypasses and
compiler exit-status propagation. Both direct compilation and cached compilation
must return the actual child status on Windows.

### PCH-dependent module invalidation

The POSIX `test_sccache_pch.py` fixture changes a PCH binary while retaining
equivalent preprocessed text, then compiles a fresh importer against the actual
PCH and module. Explicit PCH inputs participate in the cache key. Unknown or
ambiguous arguments bypass caching with the original compiler command.

The Windows `test_sccache_modules.py` fixture changes a module implementation
while preserving the importer source. The rebuilt importer must observe the new
value and have a different object hash. A separate ordinary translation unit
must still produce a warm cache hit. Opaque module/PCH response inputs bypass
sccache because their binary dependencies cannot be reliably hashed there.

Each real cache fixture owns its server, configuration and disk cache. They
retain stage counters and require zero cache read/write errors; they do not
clear the producer's cache or disable Clang's PCH/module validation. Cache
statistics exclude scanning, linking and compiler commands outside the launcher.

## Independent accuracy checks

Run `tests/{tanh,log,atan2}/verify_bounds.py` to check live coefficient words and
exact-rational intermediate bounds. These scripts do not establish correct
rounding of the mathematical function.

The optional MPFR log/log1p suite leaves coefficients, recurrences and rounding
points unchanged. It reports exact worst-case input/output words, ULP distance,
absolute error near zero, sampled monotonicity and special/domain cases. Its
256/512-bit precision agreement is checked independently. Four retained
regression witnesses are:

| Function / measure | Input word | Graph output | Rounded FTZ reference |
| --- | --- | --- | --- |
| log / ULP distance | `17b97dc0` | `c25c52bc` | `c25c52bd` |
| log1p / ULP distance | `bf7fffee` | `c15bec2e` | `c15bec2d` |
| log / near-zero absolute error | `3f7ff1f2` | `b960e62c` | `b960e62c` |
| log1p / near-zero absolute error | `b977e239` | `b977e9ba` | `b977e9ba` |

The assertions permit improved accuracy and preserve the measured per-case
ceilings. Zero ULP distance can still have nonzero real error. Sampling does not
establish exhaustive accuracy or monotonicity. See the
[oracle method](https://github.com/ekmett/ftz/blob/main/tests/log/README.md#optional-mathematical-accuracy-oracle).

## Shader checks and execution scope

The shader fixtures compile HLSL 2021 with DXC, validate SPIR-V and translate to
Metal. Separate numerical banks exercise both policies and the 32/64-bit integer
implementations. Signed-add admission uses an integer-exact RNE-then-FTZ oracle;
directed rounding uses an independent integer-word oracle. Input echoes and
output-tail guards remain exact even when NaN output representations differ.

Generated code and successful translation do not establish device behavior.
Native device execution must separately admit the compiled arithmetic and check
the bank, guards and graphics validation diagnostics. A CPU test, a shader
compile or a result from another device does not extend that admission.

## Repeating the checks

Use the [build guide](building.md) and select only profiles admitted on the
execution host. The default host build includes the numerical suites. Focused
installed-consumer projects live beside their tests. Pass an output path to the
tanh, log or atan2 executable to retain packets; trig also accepts `--common`.
Keep producer and consumer compiler, standard library, exceptions and FP options
compatible. Per-thread environment admission remains required for numerical
execution.
