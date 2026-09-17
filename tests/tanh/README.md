# Native tanh checks

`tanh.cc` compares the scalar graph with vector, register-array and generic-wide
results. The bank covers signed zero, subnormal inputs through the canonical
factory, nonfinite values, every cutover within three ULPs, dense interval samples,
and deterministic arbitrary words. Non-NaN output words must agree exactly.
Manual FTZ runs in both gradual and flushing environments; hardware FTZ runs in
its required flushing environment. Empty arrays/wide values and short vectors
are included. The tests use the installed module API.

`python -B tests/tanh/verify_bounds.py` derives the affine intervals and cancellation
centers from the scalar constants, checks the native coefficient columns, and
propagates exact rational enclosures with conservative binary32 rounding bounds.
Every live Horner stage is normal and separated from zero; leading zero padding
adds exact steps only. This justifies removing internal FTZ work without changing
the scalar polynomial sequence.

The native helper accepts canonical typed FTZ inputs. It does not repair raw
subnormal words smuggled through the unsafe factory. Tiny-region output selection
is part of the scalar approximation; manual FTZ also sanitizes tiny lanes before the bounded polynomial to avoid
subnormal intermediate work. Hardware FTZ only sanitizes the large/nonfinite
side, letting its unused tiny square/product flush naturally. No independent
input-normalization pass is needed.

One interval index per register selects each coefficient with a register-table
permutation. The algorithm preserves the caller's register count and does not
split a wide value into smaller batches. `codegen.cc` supplies ordinary no-LTO
x86 array-boundary witnesses for the scalar fallback, individual vectors, and
several wide shapes. It is an inspection fixture, not a portable speed guarantee.

Pass an optional output path to capture the scalar and common native widths
1/2/3/4 as little-endian binary32 words. Larger widths still run their checks
but are omitted from the packet so AVX2, AVX512 and NEON packets are comparable.
`tests/compare_fp32.py` requires exact non-NaN bits, including signed zero.
