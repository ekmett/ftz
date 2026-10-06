# FTZ profile baseline

`finite-range.bin` contains the 2,208 expected words (96 values, 23 columns)
used by CTest. Its SHA-256 is
`5f914a3894cef40480f47c220d5c7aa6497287bcccbb322dee40d8ceb56beed1`.
At lane 24, input `0x42b17217` gives `0x7f7fff84` in scalar and wide exp/expm1
(columns 2, 3, 10 and 11).

Scalar/wide exp columns 2/10 and raw-native exp column 20 use degree 6.
Scalar/wide expm1 columns 3/11 use that degree in their positive `x>1`
continuation. The FTZ exponential columns and continuations match an independent
fused graph oracle.

The dense exp contract fixture independently checks all seven degrees and both
cutoff signs under m32 gradual/flush and h32 flush controls. The profile fixture covers scalar
FTZ functions, wide96 exp/expm1/sincos, native arithmetic and masks, raw exp,
integer transforms, masked scaling, guarded-page tails and null empty inputs.
NaNs are normalized only in numerical columns; every integer bit stays exact.

Each backend consumer imports the same `ftz` and `native` modules, with its
native profile selected by the ISA value and compile settings.
There is one import consumer per ISA. The x86 entry uses the configured native minimum and performs
CPUID/OS-state admission before invoking profile objects. Its compile guards
compare actual AVX2/AVX512 capabilities with the minimum advertised by the
package; it selects no additional profile and uses no IPO. A caller or runner
must already support the configured project minimum: this executable is not
a portable pre-AVX launcher when its package minimum is AVX2 or stronger.
FP qualification is common and runs once per fixture invocation. No production dispatch is added.
