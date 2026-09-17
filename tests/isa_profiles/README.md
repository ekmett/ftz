# FTZ profiles against the retained baseline

`baseline.bin` remains the original 2,208-word (96 values, 23 columns) capture.
It is not regenerated for the downstream migration. The fixture covers scalar
FTZ functions, wide96 exp/expm1/sincos, native arithmetic and masks, raw exp,
integer transforms, masked scaling, guarded-page tails and null empty inputs.
NaNs are normalized only in numerical columns; every integer bit stays exact.

Each backend consumer now imports the same `ftz` module and its chosen `simd`
ISA module. The former header/import duplicate is replaced by one actual import
consumer per ISA. The baseline x86 entry performs CPUID/OS-state admission
before invoking those objects; it has no ISA flags or IPO. FP qualification is
common and runs once per fixture invocation. No production dispatch is added.
