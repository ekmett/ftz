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
  -Dftz_DIR=/prefix/lib/cmake/ftz -Dnative_DIR=/prefix/lib/cmake/native \
  '-DTRIG_PROFILES=AVX2;AVX512'
cmake --build build/trig --parallel
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
polynomial using its known quadrant parity.

To compare a prior package, configure the same fixture into a second build with
that package's explicit `ftz_DIR`; compare `pair-*.bin` and disassemble the
`trig_codegen_*` objects. No historical source or archive is fetched implicitly.

For a cross-architecture comparison, run each executable with an output path
and `--common`. The packet then records only the scalar and logical 1/2/3/4-lane
shapes shared by AVX2, AVX-512 and NEON; wider native shapes still execute their
checks. Compare packets with `python tests/compare_fp32.py first.bin second.bin`.
Non-NaN words, including signed zeros, must match exactly.

## Paired exceptional-lane repair

The packed pair repairs sine and cosine together, extracting each flagged input
once and calling the shared scalar `ftz32_sincos` contract once. Values below
8192 retain the bounded pair graph; standalone sine and cosine retain their
single-output repair paths.

The fixture also constructs zero-, one- and all-repair vectors, placing the one
exception at every logical lane. It compares vectors, two-register arrays and
wide packs directly against scalar pairs for both policies, including the two
sides of 8192, large finite words, infinities and NaNs. Logical short vectors
and empty arrays/wides remain covered.

`repair_codegen.cc` exposes non-inlined public paired consumers at the selected
native width. Inspect `trig_repair_codegen_*` objects to compare extraction and
full-range reduction between explicitly selected packages. To build the optional
repair-density benchmark, add `-DTRIG_BUILD_BENCHMARK=ON`. Run
`trig_repair_bench_<profile> [iterations]` on an admitted native host; it reports
policy, lane count, repaired-lane count, nanoseconds per pair and an output
checksum. Its zero/one/all banks use bounded inputs or large finite inputs.
Benchmark matched baseline/candidate packages with the same compiler settings,
repeat in alternating order, and report each density separately; a source-level
sharing change alone does not establish a speedup.
