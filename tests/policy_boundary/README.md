# Policy boundaries and math surface

This standalone installed-package fixture checks simultaneous `m32` and `h32`
lookup. It rejects mixed arithmetic, comparisons (including `<=>`), assignments,
FMA argument positions, masked scaling, selection, typed memory and swizzle
writes. It checks scalar, vector and wide combinations without importing the
implementation's scalar backend directly.

`surface.cc` records the current math surface. Both policies provide scalar,
native vector, array and wide log/log1p/tanh/atan2. Vector and array calls use
the packed kernels; wide forwards the whole register array through ADL.
These assertions describe current coverage rather than forbidding additions.

Configure against installed packages, selecting only CPU profiles admitted on
the execution host:

```sh
cmake -S tests/policy_boundary -B build/policy -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release \
  -Dftz_DIR=/prefix/lib/cmake/ftz -Dsimd_DIR=/prefix/lib/cmake/simd \
  '-DREVIEW_PROFILES=AVX2;AVX512'
cmake --build build/policy --parallel 2
ctest --test-dir build/policy --output-on-failure
```

`codegen.cc` supplies ordinary scalar/vector/wide entry points.
`codegen_array.cc` requests inlining around three-register array graphs so that
repair arithmetic is visible. Inspect both; a short outlined entry alone does
not establish its complete cost. `REVIEW_BASELINE=ON` builds only these codegen
objects against an explicitly supplied older single-policy package. Compare
manual entry points only when the older package selected manual normalization.
No baseline package, source revision or evidence archive is implicitly fetched.

The retained implicit float conversion permits an explicit standard-library math
call to leave FTZ arithmetic. It also makes `condition ? m32_value : h32_value`
and `std::common_type_t<m32,h32>` select float. Deleted mixed operators cannot
intercept the conditional operator.
