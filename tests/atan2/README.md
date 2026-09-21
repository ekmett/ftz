# Native atan2

This installed-package fixture compares same-policy scalar arrays, native
vectors, vector arrays and generic wide calls with the existing scalar
`ftz::atan2` graph. Inputs use the canonicalizing `from_bits` factory. It checks
`m32` under gradual and flush controls, then `h32` under flush controls. Every
non-NaN result, including signed zero, must match exactly; NaNs compare by class.

The bank includes all combinations of signed axes, normals, infinities and NaNs;
both sides of the tiny-ratio cutoff; every normal power-of-two exponent against
one; minnormal rescaling boundaries; all quadrants and swapped operands; and
seeded arbitrary word pairs. Widths1/2/3/4, x86 width8 and AVX512 width16 plus
empty arrays/wides are checked.

```sh
cmake -S tests/atan2 -B build/atan2 -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release \
  -Dftz_DIR=/prefix/lib/cmake/ftz -Dnative_DIR=/prefix/lib/cmake/native \
  '-DATAN2_PROFILES=AVX2;AVX512'
cmake --build build/atan2 --parallel
ctest --test-dir build/atan2 --output-on-failure
python tests/atan2/verify_bounds.py
build/atan2/atan2_avx2 capture.bin
```

Select only CPU profiles admitted by the host; Apple uses NEON and its supported
Clang driver. The fixture does not perform runtime dispatch. An optional path
writes the same vector4 output bank on AVX2, AVX512 and NEON, allowing exact
non-NaN cross-architecture comparison. The package needs SIMD's generic wide atan2 forwarding.

The native graph preserves the scalar bit-derived reciprocal seed, three Newton
steps, mantissa multiply and integer rescaling before the unchanged eight-term
Horner polynomial. It uses no per-lane fallback, division or reciprocal estimate
instruction. Tiny hardware lanes may evaluate unobserved underflowing stages;
manual lanes mask them. Axis/infinity/NaN and quadrant handling is word-based.
The scalar and HLSL implementations are unchanged.

`verify_bounds.py` checks native coefficient words and the three refinement
steps, bounds the exact seed pieces and rounded Newton residuals, then proves
normality of observed Horner stages and quadrant folds. It establishes the
shared graph's FTZ behavior, not correct rounding of mathematical atan2.
`codegen.cc` exposes strict ordinary optimized single/three-register leaves for
native instruction and call inspection.
