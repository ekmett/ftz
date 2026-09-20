# FTZ profiles against the retained baseline

`baseline.bin` remains the original 2,208-word (96 values, 23 columns) capture.
It is not regenerated for the downstream migration. The fixture covers scalar
FTZ functions, wide96 exp/expm1/sincos, native arithmetic and masks, raw exp,
integer transforms, masked scaling, guarded-page tails and null empty inputs.
NaNs are normalized only in numerical columns; every integer bit stays exact.

Each backend consumer imports the same `ftz` and `native` modules, with its
native profile selected by the ISA value and compile settings.
The former header/import duplicate is replaced by one actual import
consumer per ISA. The x86 entry uses the configured SIMD minimum and performs
CPUID/OS-state admission before invoking profile objects. Its compile guards
compare actual AVX2/AVX512 capabilities with the minimum advertised by the
package; it selects no additional profile and uses no IPO. A caller or runner
must already support the configured project minimum: this executable is not
a portable pre-AVX launcher when its package minimum is AVX2 or stronger.
Older packages retain the pre-AVX guard. FP qualification is
common and runs once per fixture invocation. No production dispatch is added.
