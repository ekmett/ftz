# Source layout

Applications import `ftz` for the numerical types and math. The separate
`ftz.controls` module supplies floating-point environment management without
requiring a numerical type or an ISA-specific SIMD module.

| Path | Responsibility |
| --- | --- |
| `cxx/modules/ftz.ccm` | `m32`, `h32`, conversions, operators, admission and exported math |
| `cxx/modules/ftz.controls.ccm` | Thread FP controls, scoped restoration and external-call boundaries |
| `cxx/ftz/math.h` | Scalar and register-pack host math implementation |
| `cxx/ftz/simd.h` | The custom-element specialization for `simd::vec` and packed math |
| `hlsl/ftz/ftz32.h` | HLSL 2021 scalar wrapper, conversions and operators |
| `hlsl/ftz/math.h` | Shader math overloads |
| `shared/ftz` | Arithmetic graphs, coefficients and bit operations consumed by host and shader implementations |

The C++ headers are inputs to the named module, installed so downstream builds
can generate their own compatible BMIs. They do not form a separate header-only
public API. The shader headers are the public `ftz::hlsl` interface; applications
own their entry points and choose the compiled policy with defines.

`simd` owns native registers, masks, architecture tags and generic `wide`
forwarding. FTZ supplies an element customization once for each arithmetic
policy; that customization composes with the supported native architectures.
The dependency remains one way. Array math kernels expose independent register
chains without depending on the `wide` container itself.

Manual and hardware arithmetic must describe the same results for admitted
inputs. Hardware policy removes flushing which its established FP environment
already performs. Boundary repairs remain where hardware differs in underflow
rounding or signed-zero behavior. Shared integer helpers that implement these
production repairs belong here; proof tools, captured outputs, code-generation
probes and regression drivers belong under `tests`.

HLSL remains at language version 2021. Its portable integer implementation uses
32-bit words; `FTZ_SHADER_INT64` selects the alternative where native 64-bit
integer arithmetic is available. Host code uses its native implementation rather
than adopting that shader limitation.
