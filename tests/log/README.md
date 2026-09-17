# Native log and log1p

This installed-package fixture compares vector, array and wide results with the
existing scalar `ftz::log` / `ftz::log1p` graphs. It checks both `m32` and `h32`,
manual arithmetic with gradual and flushing controls, and hardware arithmetic
with flushing controls. Finite results and signed zeros compare bitwise; NaNs
compare by classification. The packet is also compared across modes/policies.

Inputs include raw subnormal/zero and nonfinite words, both sides of every
admission/direct-kernel cutoff, 8193 neighbors around one and negative one,
every normal power-of-two boundary, and a seeded full-word bank. Native widths
1/2/3/4, x86 width8, AVX512 width16, empty packs, scalar arrays/wides and vector
arrays/wides are covered. The public `from_bits` factory normalizes each input
before the native and scalar calls. An unsafe constructor transfers this
normalization obligation to its caller; the kernel does not repeat it.

```sh
cmake -S tests/log -B build/log -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release \
  -Dftz_DIR=/prefix/lib/cmake/ftz -Dsimd_DIR=/prefix/lib/cmake/simd \
  '-DLOG_PROFILES=AVX2;AVX512'
cmake --build build/log --parallel 2
ctest --test-dir build/log --output-on-failure
python tests/log/verify_bounds.py
```

Select only admitted CPU profiles; Apple uses `-DLOG_PROFILES=NEON` and its
supported Clang driver. This fixture does not dispatch at runtime.

The native kernel uses one sign-selected Horner chain per lane. The negative
coefficient list is zero-padded only before its original live chain; no live
coefficient or recurrence is changed. `log1p` selects either its direct argument
or the mantissa reduction of the scalar graph's rounded `1+x` before that chain.
Special values and tiny results are reconstructed with word masks. There is no
per-lane scalar fallback or memory gather.

`verify_bounds.py` checks the coefficient words against the scalar source and
uses exact rational outward intervals for every live Horner stage. It also
bounds the nonzero square, affine cancellation, direct log1p polynomial result,
and split-ln2 reconstruction. Observed FP intermediates are normal or exact zero
in binary32 RNE. Manual tiny lanes are masked before squaring; hardware tiny
lanes may underflow but their result is the original word. Neither path needs
intermediate flush operations. These are scalar-graph equivalence bounds, not a claim of
correct rounding of the transcendental functions.

`codegen.cc` exposes ordinary optimized non-LTO single-register and three-register
wide entry points. Inspect calls and register pressure separately; Windows ABI
saves should not be mistaken for arithmetic spills. The scalar/shared/HLSL
implementations are unchanged by this native addition.

Pass an optional output path to capture the scalar and common native widths
1/2/3/4 as little-endian binary32 words. Larger widths still run their checks
but are omitted from the packet so AVX2, AVX512 and NEON packets are comparable.
`tests/compare_fp32.py` requires exact non-NaN bits, including signed zero.
