# FTZ regression fixtures

These imported-module consumers retain the arithmetic banks, integer scaling
reference, lane order and tail checks from the original integrated fixtures.
The only intended numerical equivalence is identical non-NaN binary32 bits;
NaN signs and payloads are outside the contract. Signed zeros remain exact.

`ftz32_simd` covers scalar and native SIMD widths, mixed raw/FTZ arguments,
canonical import, masked tails, empty/one/odd register counts, and 96-value
wide transcendental shapes. `scaleb` retains the independent integer dyadic
reference, special exponents, merge/zero masks and four x86 denormal modes.
`noexcept_ftz` retains discarded-result conversion side effects and tests real
exception propagation/cleanup when `FTZ_ENABLE_EXCEPTIONS=ON`.

The consumer capture preserves the original mixed raw/FTZ output order. It is
an artifact for exact comparisons, not an accuracy or performance claim.
Tests compile against the selected `FTZ_TEST_ISA` provider; hardware-policy
builds retain the gradual-mode admission rejection control.

`wide_exp` imports the modules and checks `exp(wide::array<R,N>)` for both FTZ
policies, scalar elements, and SIMD widths 1/2/3/4/8 (16 for AVX512). It retains
empty, singleton and three-register shapes, varies values across registers and
lanes, and compares the existing scalar, standard-array and legacy-wide graphs.
Cutoff neighbors, canonicalized subnormal inputs, nonfinites and seeded raw words
form value packets compared across manual gradual/manual flush/hardware flush.
Tuple and mixed float/FTZ tuple calls are rejected at compile time. This is graph
and shape regression evidence, without an FP-status equality or accuracy claim.

Targets are `ftz_test_wide_exp_<profile>` and CTests `ftz.wide_exp.<profile>` for
each configured AVX2, AVX512 or NEON profile. They require the
[updated SIMD package](https://github.com/ekmett/simd/commit/d73f654d1857fca0c56b2610174ec34d5dd99832)
that exports `wide::array` and the shared exp graph; the previous released module
interface does not contain these types.
