# FTZ profile baseline

`baseline.bin` contains 2,208 words (96 values, 23 columns). The shared exp
overflow cutoff gives positive infinity for input `0x42b17217` at lane 24 in
scalar and wide exp/expm1 (columns 2, 3, 10 and 11).
The baseline uses degree 6 for scalar/wide exp columns 2/10 and raw-native exp
column 20. Scalar/wide expm1 columns 3/11 use that degree in their positive `x>1`
continuation. These exponential columns and continuations match an independent
fused graph oracle. The baseline SHA-256 is
`1f24ad069d3859eb8db991c4bb0b5bc66fea1edb101e023f134bec87307c99d3`.

The dense exp contract fixture independently checks all seven degrees and both
cutoff signs under m32 gradual/flush and h32 flush controls. The profile fixture covers scalar
FTZ functions, wide96 exp/expm1/sincos, native arithmetic and masks, raw exp,
integer transforms, masked scaling, guarded-page tails and null empty inputs.
NaNs are normalized only in numerical columns; every integer bit stays exact.

Each backend consumer imports the same `ftz` and `simd` modules, with its
native profile selected by the ISA value and compile settings.
There is one import consumer per ISA. The x86 entry uses the configured SIMD minimum and performs
CPUID/OS-state admission before invoking profile objects. Its compile guards
compare actual AVX2/AVX512 capabilities with the minimum advertised by the
package; it selects no additional profile and uses no IPO. A caller or runner
must already support the configured project minimum: this executable is not
a portable pre-AVX launcher when its package minimum is AVX2 or stronger.
FP qualification is common and runs once per fixture invocation. No production dispatch is added.
