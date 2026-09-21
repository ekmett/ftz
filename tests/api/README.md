# Compiled API examples

I keep the Doxygen examples in installed-package consumers so the snippets
exercise the public interface.
`scalar.cc` checks imports, policy choice and mathematical special values;
`environment.cc` checks admission and complete thread-state restoration;
`vectors.cc` checks typed memory, masks, owning swizzles, register arrays and
wide math for both `m32` and `h32`.

Configure with the same LLVM 23/C++26 toolchain as the installed packages:

```sh
cmake -S tests/api -B build/api -G Ninja \
  -Dftz_DIR="/path/to/ftz/lib/cmake/ftz" \
  -Dnative_DIR="/path/to/simd/lib/cmake/native" \
  -DAPI_PROFILES=AVX2
cmake --build build/api --parallel
ctest --test-dir build/api --output-on-failure
```

Select only architectures admitted on the test host: `AVX2`, `AVX512` or `NEON`.
A semicolon-separated list builds each requested vector example. The installed
CMake targets supply the module providers and numerical compile settings.

`shader.hlsl` is a complete HLSL 2021 compute entry point. It uses one compile-time
shader policy, unlike the two coexisting C++ types. Dispatch only records backed
by the input/output buffers (64 records per workgroup in this small example).
Compile it with the include directories exported by `ftz::hlsl`, entry `shader`,
target `cs_6_6`, and `FTZ_FP32_HARDWARE_FTZ=0` or `1`. Hardware shader arithmetic
requires separate device admission. Compiling an example does not establish it.

Doxygen uses `EXAMPLE_PATH=tests/api` and the named snippet markers in these
files. I use these as small API checks. The [validation record](../../docs/validation.md)
keeps their scope separate from numerical and device qualification.

`api.native_types` checks FTZ's scalar and vector mask traits, default one-lane
architecture and rejection of foreign-family architecture tags through the
installed module graph.
