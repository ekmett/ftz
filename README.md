# ftz

[![docs: doxygen](https://img.shields.io/badge/docs-doxygen-blue?style=flat&logo=doxygen&logoColor=white)](https://ekmett.github.io/ftz/)

`ftz` provides fast, bit-for-bit reproducible floating-point arithmetic on real
CPUs and GPUs. The goal is to produce the exact same answer on every supported
platform while staying reasonably close to IEEE behavior. Maximum precision is
not the goal.

The library works with 32-bit floats. It deliberately flushes denormal values
(the tiny values below the normal floating-point range) to signed zero. Many
high-performance GPU targets already do this. Making it part of the arithmetic
lets the CPU and GPU agree without emulating gradual underflow everywhere.

Polynomial kernels exploit bounds on their intermediate values to move flushing
checks out of the core calculation or eliminate them altogether. Hardware flushing
removes more of the work where it gives the right answer. The performance target
for common operations is within roughly 10% of a straightforward implementation;
the cost depends on the operation and the hardware.

Small approximation errors and deliberate cutoffs are acceptable. For example,
`exp` can reach zero or positive infinity a little early at the ends of its
range. It should still track the real function closely through the useful
range, without spurious reversals at approximation boundaries. Reproducibility
and mathematical accuracy are separate properties: matching bits does not make
an approximation correctly rounded.

The C++26 interface builds on [native](https://github.com/ekmett/native) for SIMD
and packs of independent registers. HLSL 2021 implementations provide the same
arithmetic for cross-compilation to Metal and Vulkan. The available math includes
`exp`, `expm1`, `log`, `log1p`, `sin`, `cos`, `sincos`, `tanh` and `atan2`, alongside
ordinary arithmetic, rounding and classification.

## Software or hardware flushing

| Type | Use |
| --- | --- |
| `ftz::m32` | Flush tiny values explicitly; works with CPU hardware flushing on or off |
| `ftz::h32` | Use hardware flushing where it agrees, repairing the cases where it does not |

Both types are four bytes, trivially copyable, and available in the same module.
Both produce the same results under their required floating-point settings.
Use `m32` when the library needs to do the flushing, or `h32` after checking the
hardware path and enabling flushing on each participating thread. Both require
round-to-nearest-even and fused multiply-add.

## Process a batch

This example uses AVX2, so CPU and OS vector-state support must already be
established before entry. It temporarily sets the thread's floating-point
controls, checks the required behavior, processes 96 values and restores the
caller's state.

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

Check CPU support at startup and set floating-point controls once per numerical
thread. Keep these checks out of the arithmetic loop. Use unqualified math calls
so argument-dependent lookup selects the element type's arithmetic. The
[compiled examples](tests/api/README.md) cover conversions, scopes, memory, masks, swizzles, arrays and shaders.

## Result guarantees

The same operations with the same inputs must produce the same result bits on
supported targets. This includes the sign of zero. NaN sign and payload are
left unspecified. Factories flush denormal inputs to signed zero. Approximate
functions do not promise correctly rounded libm results. Converting to `float`
or explicitly calling standard math leaves the FTZ contract.

The [arithmetic guide](docs/arithmetic.md) spells out conversion escapes, unsafe
bit transport, mixed-policy rejection, admission and thread restoration.
The [validation guide](docs/validation.md) describes the maintained numerical,
package and device checks, including the limits of sampled bit equality.

## WebGPU and WebAssembly

These are more difficult targets. Neither provides the combination of guaranteed
fused multiply-add and controllable hardware flushing that the fast paths need.
Baseline Wasm SIMD has no fused multiply-add instruction; relaxed SIMD alone
does not guarantee the required rounding. More work must be done in software.

The repository includes 32-bit integer helpers for restricted shader targets,
but complete WebGPU and WebAssembly execution paths are not yet qualified for
the FTZ result contract. Native's Wasm support does not by itself establish FTZ
support. See the [platform notes](docs/math-kernels.md).

## Build and documentation

The native build uses Clang 23, CMake 4.4, Ninja and an installed native package.
Start with the [build and module guide](docs/building.md); compiler, runtime,
exception settings and profile selection must agree across the dependency graph.

- [Math functions and implementation](docs/math-kernels.md)
- [HLSL headers and device policy](docs/shaders.md)
- [Compiled API examples](tests/api/README.md)

The [C++ API](https://ekmett.github.io/ftz/) and
[HLSL API](https://ekmett.github.io/ftz/shaders/) references are generated on GitHub
with Doxygen from the public interfaces and
example snippets. [Documentation build commands](docs/building.md)
cover the separate C++ and HLSL references.

FTZ is dual-licensed under BSD-2-Clause and Apache-2.0. See
[LICENSE.md](LICENSE.md) and individual source notices for retained upstream terms.
