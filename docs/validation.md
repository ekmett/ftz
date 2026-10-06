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

`ftz.exp.contract` checks degrees 1 through 7 against an independent
frozen-coefficient graph, including dense windows at output cutoffs and reduction
transitions. It exercises scalar, SIMD, array and wide calls under both policies.
The same executable can emit input/expected packets for the
[Metal exponential check](https://github.com/ekmett/ftz/blob/main/tests/exp_shader/README.md).
Packets must be generated from the source being tested: changing the range policy
changes expected outputs even when the polynomial is unchanged.

The [profile check](https://github.com/ekmett/ftz/blob/main/tests/isa_profiles/README.md)
compares 2,208 words against the checked-in `finite-range.bin` golden. At input
`0x42b17217`, scalar and wide `exp`/`expm1` expect the finite word `0x7f7fff84`.
General vector scaling has an independent integer dyadic oracle covering signed
flushing, arbitrary exponents, masked results and the minimum-normal boundary.

The tanh, log/log1p, trig and atan2 tests compare packed results with the scalar
graph and can write common-width packets for comparisons across architectures.
`tests/compare_fp32.py` permits NaN representation differences; every other word,
including signed zero, must match. These checks establish sampled agreement,
not exhaustive accuracy against the mathematical function.

Code-generation fixtures expose optimized leaves for inspection separately from
runtime tests. Large packs may spill. Multiply and FMA retain boundary repairs;
hardware flushing does not promise one instruction per public operation.

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

CI checks argument preservation, conservative cache bypasses and propagation of
the compiler's exit status. The POSIX PCH fixture changes the PCH while retaining
equivalent preprocessed text; the Windows module fixture changes the module
while retaining the importer source. Rebuilt consumers must see the changed
inputs, and an ordinary translation unit must still produce a warm cache hit.
These checks use isolated caches and leave Clang's validation enabled.

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

## Running the checks

Use the [build guide](building.md) and select only profiles admitted on the
execution host. The default host build includes the numerical suites. CI results describe the
revision, compiler and runner of that run; this page does not certify every
architecture or device for the current checkout. Focused
installed-consumer projects live beside their tests. Pass an output path to the
tanh, log or atan2 executable to retain packets; trig also accepts `--common`.
Keep producer and consumer compiler, standard library, exceptions and FP options
compatible. Per-thread environment admission remains required for numerical
execution.
