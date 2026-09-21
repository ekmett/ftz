# Arithmetic and module boundaries

I keep numerical policy in FTZ and register machinery in SIMD. Applications
import `ftz` for the numerical types and math. The separate
`ftz.controls` module supplies floating-point environment management without
importing numerical types or vector operations.

| Path | Responsibility |
| --- | --- |
| `cxx/modules/ftz.ccm` | `m32`, `h32`, conversions, operators, admission and exported math |
| `cxx/modules/ftz.controls.ccm` | Thread FP controls, scoped restoration and external-call boundaries |
| `cxx/ftz/math.h` | Scalar and register-pack host math implementation |
| `cxx/ftz/simd.h` | The custom-element specialization for `native::simd` and packed math |
| `hlsl/ftz/ftz32.h` | HLSL 2021 scalar wrapper, conversions and operators |
| `hlsl/ftz/math.h` | Shader math overloads |
| `shared/ftz` | Arithmetic graphs, coefficients and bit operations consumed by host and shader implementations |

The C++ headers are inputs to the named module, installed so downstream builds
can generate their own compatible BMIs. They do not form a separate header-only
public API. The shader headers are the public `ftz::hlsl` interface; applications
own their entry points and choose the compiled policy with defines.

`native` owns native registers, masks, ISA values and generic `wide`
forwarding. FTZ supplies an element customization once for each arithmetic
policy; that customization composes with the supported native architectures.
FTZ imports `native.math` for the common exponential kernel and `native.scalar`
for scalar storage. I keep that dependency one way. Array math kernels expose independent register
chains without depending on the `wide` container itself.

Manual and hardware policies describe the same arithmetic results under their
respective admitted environments, with NaN sign and payload left unspecified.
Hardware policy removes flushing which its established FP environment already
performs. Boundary repairs remain where hardware differs in underflow
rounding or signed-zero behavior. Shared integer helpers that implement these
production repairs belong here. Coefficient checks, numerical regressions and
code-generation probes exercise those definitions from `tests`.

HLSL remains at language version 2021. Its portable integer implementation uses
32-bit words; `FTZ_SHADER_INT64` selects the alternative where native 64-bit
integer arithmetic is available. Host code uses its native implementation rather
than adopting that shader limitation.

The [arithmetic guide](../docs/arithmetic.md) defines the public obligations;
[building](../docs/building.md) covers installed module dependencies.
