# ftz

Binary32 arithmetic with a specified operation graph and signed flush-to-zero
semantics, for C++26 and HLSL 2021. FTZ builds on
[simd](https://github.com/ekmett/simd): the element type chooses arithmetic, while
the vector architecture chooses storage and instructions.

```cpp
import ftz;
import simd.avx2;

using F = ftz::m32;
using V = simd::vec<F, 8, simd::avx2>;

V x(F(.25f));
simd::wide<V, 12> values(x);
auto y = expm1(values);                 // 96 values, twelve native registers
auto [s, c] = sincos(values);
auto z = fma(x, V(F(2.f)), V(F(1.f)));
```

Both policies are available in the same module and archive:

| Type | Normalization | Required CPU denormal mode |
| --- | --- | --- |
| `ftz::m32` | Explicit signed flushing | Gradual or flush |
| `ftz::h32` | Admitted hardware flushing, with required boundary repairs | Flush |

Each type is four bytes and trivially copyable. Policy is a template argument;
there is no per-operation runtime choice. `simd::vec<F,N,Arch>` and
`simd::wide<V,M>` retain that policy. The dependency goes from FTZ to SIMD.

## Contract and conversions

Public factories replace subnormal inputs with signed zero, and arithmetic
returns normalized results. Normal values, infinities and signed zeros follow
the defined operation graph; NaN sign and payload are outside the arithmetic
contract. Reproducibility describes that graph. Approximate functions do not
promise correctly rounded libm results. Admission and regression evidence applies to a particular compiled
profile, compiler and device, rather than every implementation of floating point.

Both types accept and convert to `float` implicitly. Explicit boundaries are
available too:

```cpp
auto x = ftz::m32::from_float(source);
auto y = ftz::m32::from_bits(word);      // normalizes a subnormal to signed zero
float f = x.to_float();
auto bits = x.to_bits();
auto known = ftz::m32::unsafe_from_float32(already_normalized);
```

The unsafe factory preserves the supplied bits without normalization. Its caller
must supply a normal value, signed zero, infinity or NaN. Storage and swizzle
operations transport bits; they do not repair invalid unsafe inputs.

Use unqualified math calls such as `sin(x)` and `fma(x,y,z)` so argument-dependent
lookup selects the numerical type's overload. The scalar overloads are also
available as `ftz::sin`, `ftz::exp`, and so on. An explicit `std::sin(x)` or a
conversion to `float` leaves this contract.

Scalar and vector overloads provide `isnan`, `isinf`, `isfinite`, `signbit` and
`copysign`. Vector classifications return `V::mask`, with the selected backend's
native mask representation; `copysign` transports the magnitude and sign bits
without changing a NaN payload. Generic `simd::wide` forwarding retains that
mask type for each register, or `bool` when the element is scalar.

The common scalar/vector/wide math surface includes `abs`, `sqrt`, `sin`, `cos`,
`sincos`, `exp`, `expm1`, `log`, `log1p`, `tanh`, `atan2`, `floor`, `ceil`,
`trunc` and `fma`. Rounding functions use their named direction independently of
the ambient rounding mode and retain signed zero. Binary and
ternary wide math takes matching wide operands; broadcast a scalar explicitly.
Vector `atan2` and `copysign` likewise take matching vector types. This is a
numerical type with a defined math surface, not a complete replacement for every
standard floating-point facility. `numeric_limits`, increment/decrement and
`min`/`max` are not provided.

Arithmetic and math mixing `m32` and `h32` are rejected. Choose a policy with an
explicit conversion first. C++ conditional expressions and `std::common_type`
can still choose `float` for two different policy types: retaining implicit
conversion to float makes those language-level escapes unavoidable. Keep both
branches of a numerical conditional in the same type.

## CPU environment

Both policies require round-to-nearest-even, genuine fused FMA, and ordinary
operations compiled without reassociation or implicit contraction. `m32` supports
both gradual and flush environments. `h32` additionally requires the qualified
hardware flush behavior. It removes redundant normalization; required underflow
boundary and signed-zero repairs remain.

```cpp
ftz::native_fp32_scope region(ftz::native_fp32_mode::flush);
auto result = ftz::probe_ftz32_cpu<ftz::h32>();
if (!result.admitted()) return 1;
// Hardware-policy work here; the scope restores the caller's state afterward.
```

`ftz.controls` owns environment controls independently of SIMD. At an application-owned
numerical thread entry, `set_native_fp32_mode(flush)` sets controls and clears status
without running admission, allocating, or arranging restoration. Check its
boolean result before starting numerical work. Use the fully qualified
enum value `ftz::native_fp32_mode::flush`. Establish the agreed controls on each
participating thread; run the compiled-profile qualification at startup rather
than repeatedly in arithmetic or at each task. CPU instruction and OS vector-state
admission is a separate prerequisite before entering an ISA-specific function.

Use `native_fp32_scope` when borrowing a caller's thread. Keep the scope and its
`external_scope` on that thread. The external scope restores the caller's
environment for third-party code, then reinstates the numerical region on return
or unwind. These APIs manage CPU state; GPU qualification is separate.

## Build and consume

Use Clang 23, CMake 4.4 and Ninja, with an installed SIMD package. Both packages
must use compatible compiler, standard-library, exception and floating-point
settings. CMake rebuilds consumer BMIs from installed module sources. Keep one
consistent dependency configuration through an application and its libraries.

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH=/path/to/simd -DCMAKE_BUILD_TYPE=Release \
  -DFTZ_ENABLE_PCH=ON -DFTZ_ENABLE_IPO=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/ftz
```

Use `clang-cl` on Windows. Tests default to AVX2 on x86 and NEON on ARM64.
On a host admitted for AVX-512, set `-DFTZ_TEST_PROFILES="AVX2;AVX512"`
to exercise both native widths; the test executables assume their selected ISA
is available. Merely compiling an ISA provider does not establish that.
Exceptions default to disabled. An exception-enabled
consumer uses packages built with both `SIMD_ENABLE_EXCEPTIONS=ON` and
`FTZ_ENABLE_EXCEPTIONS=ON`.

```cmake
find_package(ftz CONFIG REQUIRED COMPONENTS ftz)
add_executable(example example.cc)
target_link_libraries(example PRIVATE ftz::ftz simd::avx2)
simd_target_profile(example AVX2)
```

`ftz::ftz` supplies the `ftz` and `ftz.controls` modules and static archive. Its
baseline controls do not inherit a numerical ISA. The application selects the
provider for each numerical translation unit and checks CPU/OS support before
entering it. For a separate consuming project, include both package prefixes in
`CMAKE_PREFIX_PATH`.

`FTZ_FP32_HARDWARE_FTZ` is a package build setting selecting the compatibility
alias `ftz::ftz32`, and defaults to zero. Importing a module does not export
preprocessor macros, and a consumer `#define` does not reselect its compiled
alias. It does not disable either `m32` or `h32`, or configure thread controls.
New code can name its policy directly.

## Shader headers

`find_package(ftz CONFIG REQUIRED COMPONENTS hlsl)` supplies `ftz::hlsl`, its
public shader include directories, and the `simd::headers` dependency. Configure
`FTZ_BUILD_HOST=OFF` to install these without a host compiler. Consumers own shader
entry points and compiler invocations; `ftz::hlsl` is a header library.

HLSL includes `<ftz/ftz32.h>` and uses `ftz::ftz32`. Compile in HLSL 2021 mode.
`ftz::floor`, `ftz::ceil` and `ftz::trunc` use the corresponding shader intrinsics
and preserve the wrapper type without an additional normalization pass.
`FTZ_FP32_HARDWARE_FTZ=0` retains explicit normalization; `=1` selects the admitted
hardware path. Choose the compiled shader variant after device qualification, including
[signed tiny-result add/subtract checks](tests/shader_add/README.md).
The hardware path uses native addition/subtraction directly; a failed raw sign
check requires the explicit variant.
Shader policy selection is independent of the CPU type and thread environment. `FTZ_SHADER_INT64` selects
native 64-bit integer support where available, with 32-bit word operations as the
portable alternative.

`ftz::m16` and `ftz::h16` are reserved for future reproducible BF16 arithmetic
without subnormal values. They are not implemented; rounding, intermediate
precision and fused-operation rules require a separate contract.

See [LICENSE.md](LICENSE.md) for the dual BSD-2-Clause/Apache-2.0 license and
individual source notices for retained upstream terms.

[Compiled API examples](tests/api/README.md) cover scalar conversions, thread
scopes, vector memory, masks, arrays, wide math and a complete shader entry.
The [source guide](src/README.md) explains the module and shader boundaries.
See [validation](docs/validation.md) for measured cross-architecture agreement,
shader evidence, and the limits of those checks. The Linux AVX2 package check
uses LLVM 23.1.1/libc++ 23 and relocated SIMD/FTZ prefixes; its CPU scope is
separate from the shader and cross-host packet records.

## API documentation

Doxygen 1.18 builds separate host and HLSL references from the public interfaces,
with compiled example snippets. An installed SIMD header package is enough for
a documentation-only build:

```sh
cmake -S . -B build/docs -G Ninja -DFTZ_BUILD_HOST=OFF -DFTZ_BUILD_DOCS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/simd
cmake --build build/docs --target ftz_docs
```

Open `build/docs/docs/html/index.html` for C++, and
`build/docs/docs/html/shaders/index.html` for HLSL. Warnings fail the build.
Compile the [example projects](tests/api/README.md) separately to check the
snippets against an installed package; documentation generation is not a
numerical or device qualification.
