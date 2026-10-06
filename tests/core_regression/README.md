# FTZ regression fixtures

These imported-module consumers check arithmetic, integer scaling, lane order
and tails.
The only intended numerical equivalence is identical non-NaN binary32 bits;
NaN signs and payloads are outside the contract. Signed zeros remain exact.

`ftz32_simd` covers scalar and native SIMD widths, mixed raw/FTZ arguments,
canonical import, masked tails, empty/one/odd register counts, and 96-value
wide transcendental shapes. `scaleb` retains the independent integer dyadic
reference, special exponents, merge/zero masks and four x86 denormal modes.
It exercises every supported width among 1, 2, 3, 4, 8 and 16. Raw scaling
and raw-base/FTZ-exponent forwarding are present only when the selected target
has a native scaling instruction; typed FTZ scaling remains available.
`noexcept_ftz` retains discarded-result conversion side effects and tests real
exception propagation/cleanup when `FTZ_ENABLE_EXCEPTIONS=ON`.

The consumer capture records mixed raw/FTZ outputs. It is
an artifact for exact comparisons, not an accuracy or performance claim.
Tests compile against the selected `FTZ_TEST_ISA` provider; hardware-policy
builds retain the gradual-mode admission rejection control.
