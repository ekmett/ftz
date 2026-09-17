# Native classification and sign transport

This import-only fixture checks both FTZ policies on widths1/2/3/4 plus the
selected x86 widths8/16. Classification returns exactly `V::mask`; scalar,
AVX2/NEON full-vector and AVX512 compact-mask behavior is exercised through
mask expansion and `any`/`all`. Value size, alignment and trivial copy match
the underlying raw vector.

Zeros, normalized/subnormal factory inputs, finite extrema, infinities and
positive/negative quiet/signaling NaN payloads are classified by independent
word comparisons. Every source word is paired with every sign word for
`copysign`; magnitude bits and NaN payloads must be preserved exactly. The
complete FP control/status state must remain unchanged, including with sNaNs.
Both m32 control modes and the admitted h32 mode are tested.

```sh
cmake -S tests/value_utilities -B build/values -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release \
  -Dftz_DIR=/prefix/lib/cmake/ftz -Dsimd_DIR=/prefix/lib/cmake/simd \
  '-DVALUE_PROFILES=AVX2;AVX512'
cmake --build build/values --parallel 2
ctest --test-dir build/values --output-on-failure
```

Choose only admitted CPU profiles; Apple uses NEON and its supported Clang
driver. `codegen.cc` exposes the four classifiers and sign transport for ordinary
strict codegen inspection. No arithmetic graph or scalar policy is changed.

`wide.cc` consumes the generic `simd.wide` classification and homogeneous
`copysign` lifts. It checks scalar bool and actual native mask result types,
empty/single/three-register packs, both FTZ types, exact zero/NaN sign and payload
transport, and unchanged FP status. Short two/three-lane owning swizzles and
in-place overlap preserve unsafe typed words, including subnormals; these words
are used only for transport. These tests require a SIMD package with the generic
wide value utilities.
