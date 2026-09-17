# Dedicated sine and cosine

These installed-module tests compare each standalone result to its `sincos`
component. The bank covers signed zeros/subnormals, tiny-input and quadrant
boundaries, the 8192 reduction cutover, infinities/NaNs, and seeded full-range
binary32 words. Finite values and zero signs compare exactly; NaNs compare by
classification. `m32` runs with gradual and flushing controls; `h32` runs with
flushing controls. Scope restoration remains the controls module's responsibility.

The same bank exercises scalar, native vector, two-register array and wide
calls, including empty arrays/wides. Native widths are 1/2/3/4 plus8 on x86 and16
on AVX512. Each executable optionally writes its paired outputs for an external
byte comparison against another explicitly supplied package.

```sh
cmake -S tests/trig_single -B build/trig -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release \
  -Dftz_DIR=/prefix/lib/cmake/ftz -Dsimd_DIR=/prefix/lib/cmake/simd \
  '-DTRIG_PROFILES=AVX2;AVX512'
cmake --build build/trig --parallel 2
ctest --test-dir build/trig --output-on-failure
```

Select only CPU profiles admitted on the execution host; on Apple use
`-DTRIG_PROFILES=NEON` and the supported Clang driver. This fixture performs no
runtime ISA dispatch.

`codegen.cc` is an internal helper probe with ordinary optimized, non-LTO
consumers. It exposes reduced and bounded sine/cosine/pair kernels for both
policies. Reduced single-output calls instantiate just one polynomial. Bounded
vectors still need both polynomials because different lanes can occupy different
quadrants; only the unused output reconstruction/wrapping/repair is absent by
construction. The public full-range scalar repair selects the needed reduced
polynomial using its known quadrant parity. Shader trig remains unchanged.

To compare a prior package, configure the same fixture into a second build with
that package's explicit `ftz_DIR`; compare `pair-*.bin` and disassemble the
`trig_codegen_*` objects. No historical source or archive is fetched implicitly.

For a cross-architecture comparison, run each executable with an output path
and `--common`. The packet then records only the scalar and logical 1/2/3/4-lane
shapes shared by AVX2, AVX-512 and NEON; wider native shapes still execute their
checks. Compare packets with `python tests/compare_fp32.py first.bin second.bin`.
Non-NaN words, including signed zeros, must match exactly.
