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

## Optional mathematical accuracy oracle

`-DFTZ_BUILD_MPFR_TESTS=ON` adds `log.mpfr.<profile>` to CTest. It is disabled
by default, requires installed MPFR and GMP headers/libraries, and never
downloads them. For a nonstandard prefix use `CMAKE_PREFIX_PATH`, or the four
`FTZ_MPFR_INCLUDE_DIR`, `FTZ_GMP_INCLUDE_DIR`, `FTZ_MPFR_LIBRARY` and
`FTZ_GMP_LIBRARY` cache entries. The root build also requires host/tests enabled.
These are private test dependencies: installation and exported targets do not
refer to MPFR or GMP. The standalone log fixture accepts the same option.

The oracle measures the existing scalar operation graph; it does not replace
it with a libm call or require correct rounding. Existing native packet checks
remain the separate evidence that packed implementations follow that graph.
Both policies are measured: `m32` under gradual and flush controls, and `h32`
under admitted flush controls. MPFR work runs separately under gradual controls.

Each raw binary32 input is first flushed to signed zero when subnormal, matching
the public factory. MPFR evaluates mathematical `log(x)` or `log1p(x)` with
nearest-even rounding at 256 and 512 bits. Both references are independently
rounded to binary32 nearest-even and then output-flushed to signed zero; a
disagreement fails the test. This precision cross-check is evidence for this
sample, not an exhaustive hard-to-round proof. NaN payload/sign are ignored;
infinities, domain errors and specified signed zeros are checked explicitly.

The bank includes 16,384 xorshift32 words from seed `0xfa728311`, 8,193 neighbors
of each of +1 and -1, all normal power-of-two boundaries, all normal mantissa
reduction boundaries at 1.5 times a power of two, and small neighborhoods of
special words, the minimum normal, the tiny log1p threshold, and direct-kernel
cutovers. Duplicate raw words are removed. The report gives the actual count.

`mpfr-<profile>.txt` in the fixture build directory records maximum observed
ULP distance with raw/normalized input and actual/reference output words.
ULP distance counts adjacent binary32 representations after reference output
flushing; it is not a uniform real-number error scale across zero or the FTZ
gap. For mathematical results with magnitude at most 2^-12, a separate maximum
absolute error compares the graph output with the unflushed 512-bit result.
Sorted sampled inputs report monotonicity decreases (and the first eight exact
pairs), and immediate neighbors of the named cutovers are printed separately.
Accuracy and monotonicity observations are not forced to zero: known errors
are compatible with this library's reproducible fast-graph contract.

Linux x64 native CI enables this optional test using `libmpfr-dev` on the
ephemeral runner and retains its report. Other native lanes leave the option
disabled. The measured envelope and retained worst-case inputs must be read
with their compiler/profile/sample scope, never as exhaustive bounds.
