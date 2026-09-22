# ftz

I want binary32 arithmetic whose operation graph is explicit: where rounding
happens, when subnormals become signed zero, and which operations are fused.
FTZ gives that contract a type, in C++26 and HLSL 2021.

I keep arithmetic policy separate from register layout. [native](https://github.com/ekmett/native)
owns vectors, masks, ISA values and wide register packs. FTZ supplies the
element type and its operations. Changing the number of registers does not
change the arithmetic policy.

## Two policies

| Type | Use |
| --- | --- |
| `ftz::m32` | Explicit signed flushing, under gradual or flush CPU controls |
| `ftz::h32` | Qualified hardware flushing, with the required boundary repairs |

Both types are four bytes, trivially copyable, and available in the same module.
I use `m32` when I need explicit normalization. I use `h32` only after admitting
the compiled hardware path. Both require nearest-even rounding and fused FMA;
`h32` also requires the agreed flush controls on every participating thread.

## A register pack

This example uses AVX2, so CPU and OS vector-state support must already be
established before entry. It borrows the caller's thread, checks the compiled
arithmetic contract, processes 96 values and restores the caller's FP state.

```cpp
import ftz;
import native;

bool example() {
  ftz::native_fp32_scope region(ftz::native_fp32_mode::gradual);
  if (!ftz::probe_ftz32_cpu<ftz::m32>().admitted()) return false;

  using V = native::simd<ftz::m32, 8, native::avx2>;
  V x(ftz::m32(.25f));
  native::wide<V, 12> values(x);
  auto y = expm1(values);
  return all(isfinite(y.registers[0]));
}
```

In an application I run admission at startup and establish controls per thread,
not per element or task. Use unqualified math calls so argument-dependent lookup
selects the element type's arithmetic. The [compiled examples](tests/api/README.md)
cover conversions, scopes, memory, masks, swizzles, arrays and shaders.

## The boundaries matter

Factories normalize subnormal inputs to signed zero. Arithmetic follows the
specified graph; NaN sign and payload are outside that contract. Approximate
functions do not promise correctly rounded libm results. Converting to `float`
or explicitly calling standard math leaves the FTZ contract.

The [arithmetic guide](docs/arithmetic.md) spells out conversion escapes, unsafe
bit transport, mixed-policy rejection, admission and thread restoration.
The [validation record](docs/validation.md) states which compilers, profiles,
input banks and devices were checked, including the limits of packet equality.

## Build and read

The native build uses Clang 23, CMake 4.4, Ninja and an installed native package.
Start with the [build and module guide](docs/building.md); compiler, runtime,
exception settings and profile selection must agree across the dependency graph.

- [Math kernels and platform work](docs/math-kernels.md)
- [HLSL headers and device policy](docs/shaders.md)
- [Source and module boundaries](src/README.md)
- [Compiled API examples](tests/api/README.md)

The [C++ API](https://ekmett.github.io/ftz/) and
[HLSL API](https://ekmett.github.io/ftz/shaders/) references are generated on GitHub
with Doxygen from the public interfaces and
example snippets. [Documentation build commands](docs/building.md)
cover the separate C++ and HLSL references.

FTZ is dual-licensed under BSD-2-Clause and Apache-2.0. See
[LICENSE.md](LICENSE.md) and individual source notices for retained upstream terms.
