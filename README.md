# ftz

<!-- badges:start -->
[![build](https://img.shields.io/github/actions/workflow/status/ekmett/ftz/native.yml?branch=main&style=flat&label=build&logo=githubactions&logoColor=white)](https://github.com/ekmett/ftz/actions/workflows/native.yml?query=branch%3Amain)
[![docs](https://img.shields.io/github/actions/workflow/status/ekmett/ftz/docs.yml?branch=main&style=flat&label=docs&logo=githubactions&logoColor=white)](https://github.com/ekmett/ftz/actions/workflows/docs.yml?query=branch%3Amain)
[![coverage](https://img.shields.io/github/actions/workflow/status/ekmett/ftz/coverage.yml?branch=main&style=flat&label=coverage&logo=githubactions&logoColor=white)](https://github.com/ekmett/ftz/actions/workflows/coverage.yml?query=branch%3Amain)
[![docker](https://img.shields.io/github/actions/workflow/status/ekmett/ftz/docker.yml?branch=main&style=flat&label=docker&logo=githubactions&logoColor=white)](https://github.com/ekmett/ftz/actions/workflows/docker.yml?query=branch%3Amain)
[![nix](https://img.shields.io/github/actions/workflow/status/ekmett/ftz/nix.yml?branch=main&style=flat&label=nix&logo=githubactions&logoColor=white)](https://github.com/ekmett/ftz/actions/workflows/nix.yml?query=branch%3Amain)
[![issues](https://img.shields.io/github/issues/ekmett/ftz?style=flat&label=issues&color=007ec6&logo=github&logoColor=white)](https://github.com/ekmett/ftz/issues)
[![commits](https://img.shields.io/github/commit-activity/w/ekmett/ftz?style=flat&label=commits&color=007ec6&logo=github&logoColor=white)](https://github.com/ekmett/ftz/activity)

[![CMake: 4.4+](https://img.shields.io/static/v1?label=CMake&message=4.4%2B&color=064F8C&style=flat&logo=cmake&logoColor=white)](https://github.com/ekmett/ftz/blob/main/CMakeLists.txt)
[![C++: 26](https://img.shields.io/static/v1?label=C%2B%2B&message=26&color=00599C&style=flat&logo=cplusplus&logoColor=white)](README.md)
[![Clang: 23](https://img.shields.io/static/v1?label=Clang&message=23&color=6f42c1&style=flat&logo=llvm&logoColor=white)](README.md)
[![HLSL: 2021](assets/badges/hlsl-version.svg)](README.md)

[![OS: Linux · macOS · Windows](https://img.shields.io/static/v1?label=OS&message=Linux+%C2%B7+macOS+%C2%B7+Windows&color=64748b&style=flat)](https://github.com/ekmett/ftz/blob/main/.github/workflows/native.yml)
[![CPU: x86-64 · ARM64](https://img.shields.io/static/v1?label=CPU&message=x86-64+%C2%B7+ARM64&color=64748b&style=flat)](README.md)
[![GPU: Metal · Vulkan · WebGPU](https://img.shields.io/static/v1?label=GPU&message=Metal+%C2%B7+Vulkan+%C2%B7+WebGPU&color=64748b&style=flat)](README.md)

[![license: BSD-2-Clause OR Apache-2.0](assets/badges/license.svg)](LICENSE.md)
[![Contributor Covenant: 2.0](https://img.shields.io/static/v1?label=Contributor+Covenant&message=2.0&color=007ec6&style=flat&logo=contributorcovenant&logoColor=white)](CODE_OF_CONDUCT.md)

[![docs: read](https://img.shields.io/static/v1?label=docs&message=read&color=007ec6&style=flat&logo=pandoc&logoColor=white)](https://ekmett.github.io/ftz/)
[![coverage report](https://img.shields.io/badge/coverage-report-F01F7A?logo=codecov&logoColor=white)](https://app.codecov.io/github/ekmett/ftz)
[![Docker: ghcr.io](https://img.shields.io/static/v1?label=Docker&message=ghcr.io&color=2496ED&style=flat&logo=docker&logoColor=white)](https://github.com/ekmett/ftz/pkgs/container/ftz)
[![Nix: flake](https://img.shields.io/static/v1?label=Nix&message=flake&color=5277C3&style=flat&logo=nixos&logoColor=white)](https://github.com/ekmett/ftz/blob/main/flake.nix)
<!-- badges:end -->

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

Runtime coverage is available with `-DFTZ_ENABLE_COVERAGE=ON` in a Clang test
build, with matching `llvm-cov`, `llvm-profdata`, and `grcov` on the path.
Run the tests normally, then build the `ftz_coverage` target. It writes LCOV
and HTML under `coverage/report/` without rerunning the tests. These reports
measure executed C++ code; compile-time proofs and shader execution are not
counted as runtime coverage.

The coverage workflow reports each Linux, macOS and Windows CPU target on every
commit to `main`. Codecov receives the LCOV report and CTest's JUnit results using
GitHub OIDC. The retained artifact includes the detected CPU features and a
browsable report. Ordinary builds remain uninstrumented.
