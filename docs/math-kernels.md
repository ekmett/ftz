# Math kernels and platform work

FTZ owns the arithmetic graph and its boundary behavior. Native owns registers,
masks, feature requirements and instruction wrappers. The graph may change when
the same result contract is implemented across every admitted platform. Approximate
output words are not a compatibility requirement.
Native's ordinary floating-point approximations can have a looser contract, but
sharing their graph avoids unnecessary differences where it costs nothing.

## Exponential

`exp<Degree=6>` accepts compile-time degrees 1 through 7 on scalar values, SIMD
registers, register arrays and HLSL values. Ordinary `exp(x)` calls select degree
6, whose constant and linear coefficients are exactly one. All degrees use the
same nearest-even reduction and two-part ln(2) subtraction. The lower cutoff is
`-87.33654022216796875f`; the upper finite input is `88.37625885009765625f`.
Larger inputs return positive infinity.
This permits the same inexpensive reconstruction on ARM and AVX2 as native's
`exp<true>`, without a separate exponent-128 correction.

| Platform | Reconstruction |
| --- | --- |
| AVX-512 with an admitted scaling instruction | Masked `VSCALEFPS`, selected by the vector's requirements and available overload. |
| AArch64 NEON | Biased exponent conversion with `FCVTZU`, integer shift and one scale multiply. |
| AVX2 | `CVTTPS2DQ`, signed maximum with zero, integer shift and one scale multiply. |

For the last two paths, every active finite exponent lies in [-126,127]. The
lower cutoff excludes the minimum-normal rounding strip. These bounds permit
one scale multiply with no lane-by-lane repair. Underflow and overflow flags
are computed independently of the polynomial and select its final result; they
do not clamp the input on the reduction dependency chain.

The integer conversions have defined behavior for NaNs and out-of-range values.
NaN payloads remain outside bit equality; every other result word, including
signed zero, is part of the cross-platform contract. NaNs already reach the
polynomial and propagate through the final product; a separate NaN mask before
conversion adds no value. C++ floating-to-integer casts with an out-of-range
precondition are not a substitute for those instructions.

Independent register chains advance through each reduction or polynomial stage
together. Constants are single register values shared across the pack. A
`simd<float,1>` path checks correctness; throughput work uses full SIMD registers
and varies the register count. Increasing a pack indefinitely is not an
optimization: live coefficients, masks and accumulators eventually spill.

The [Sollya script](https://github.com/ekmett/ftz/blob/main/tests/exp_fit/fit.sollya)
reproduces degrees 1 through 6; the [fit record](../tests/exp_fit/README.md) lists
coefficient words for all seven degrees. Lower degrees exchange
accuracy for fewer fused Horner stages; degree 1 is a piecewise affine
approximation after reduction and scaling. Every polynomial has constant one,
so either input zero returns exactly one. Degrees 1 through 5 have fitted linear
terms. Selection is a template parameter with no runtime degree branch.

For a `native::wide` containing FTZ values, use `exp<false, Degree>(pack)` to
select the degree through native's generic adapter. The `false` argument
delegates to FTZ's own operation; the FTZ type determines its flushing contract. There is no additional FTZ `Flush=true` overload.

The default degree-six polynomial uses six fused Horner stages per register,
plus two reduction FMAs. C++ and HLSL use the same coefficient words and operation
graph. The positive `expm1(x)` continuation for `x>1` evaluates this exponential
and subtracts one. The cancellation-safe `expm1` core and damping gain use separate
polynomials. The [fit record](../tests/exp_fit/README.md) distinguishes sampled
accuracy from the exact cross-platform operation contract.

## Next operations

Work in this order, using an independent result oracle for each chosen contract.
Changing a boundary or approximation requires updating every platform graph
and its packet checks together. Scaling's current implementation is described
first; the remaining entries are planned optimizations.

1. **General scaling qualification.** Typed `scaleb` owns an FTZ integer
   exponent graph for every native shape. Arbitrary exponent words are clamped
   into a finite conversion range before flooring; ordinary scaling changes the
   base's exponent field. Vector masks preserve signed zeros, nonfinite cases
   and the single maximum-significand rounding strip at minimum normal, without
   extracting lanes or calling a scalar repair. Raw-float forwarders require an
   actual native scaling instruction. This general algorithm is separate from
   exp's bounded reconstruction. The independent scaling oracle passes
   both policies on ARM and the complete Linux AVX2 suite. Actual AVX-512
   execution remains a separate qualification gate.
2. **`expm1` and damping gain.** Keep the cancellation-safe residual polynomial,
   exact tiny-input selection and the special rounding cases at exponents -25
   and zero. Reuse the normal power-of-two construction for the admitted graph;
   do not compute `exp(x)-1` near zero. Compare the direct polynomial across the
   positive range with continuation through `exp`. Choose one graph for FTZ
   everywhere; the broader native kernel is a candidate, not an exception to
   cross-platform agreement.
3. **`log` and `log1p`.** Retain the sign-selected coefficient words, mantissa
   interval, signed exponent conversion and two-part ln(2) reconstruction.
   Preserve `log1p`'s direct interval and tiny-result selection. Compare the
   hardware graph with FTZ's existing packets; remove a software flush only
   where a bound proves it redundant or admitted hardware supplies the same
   semantics. Native's raw tiny-input policy remains separate from FTZ factory
   normalization.
4. **`tanh`.** Preserve the piecewise odd polynomials, signed saturation and tiny
   selection. Measure coefficient selection and register pressure before
   increasing pack widths or replacing the graph with an exponential identity.
   That identity is not presumed cheaper or result-equivalent.
5. **`sin`, `cos` and `sincos`.** Keep the bounded reducer and signed-zero rules.
   Share reducer work for the paired call, and prove conversion ranges before
   selecting direct integer-conversion instructions. Keep admission or final
   masks off the arithmetic chain when the chosen shared contract permits it.
6. **`atan2`.** Separate its normalization, reciprocal refinement, polynomial
   and quadrant/special-value reconstruction in the code-generation audit.
   Hardware vector division is an experiment for native, not an automatic
   replacement for FTZ's fixed reciprocal graph. Retain the SLEEF source notice
   and license on the adapted coefficients.
7. **Reciprocal, division, square root and reciprocal square root.** Keep FTZ's
   explicitly named reproducible approximation graphs distinct from native
   instruction wrappers. Optimize their normal-input paths and retain boundary
   repairs where required; do not make a software algorithm masquerade as a
   single-instruction intrinsic.

## Qualification

For each kernel, check scalar and SIMD shapes, empty and multi-register packs,
signed zero, infinities, NaNs and dense windows around reduction and output
boundaries. Compare both FTZ policies with the independent graph under their
admitted FP controls, and verify restoration of the caller's state. Accuracy
against libm or MPFR is a separate measurement from graph equality.

Inspect optimized single-register and wide assembly on Apple AArch64 and actual
AVX2 hardware. Report calls, scalarized comparisons, conversion guards, shuffles
and spills. Benchmark full-register packs at several widths before choosing a
schedule; report time per element, input bank and FP mode. Compile checks alone
do not qualify Windows runtime behavior or AVX-512 hardware.

Baseline Wasm SIMD128 lacks a fused multiply-add instruction. Its native math
graph therefore has separate accuracy tests and cannot inherit FTZ admission.
Relaxed SIMD also needs a separate arithmetic qualification; merely accepting
its instruction set does not establish the fused rounding contract. Keep a
potential Wasm FTZ backend behind that numerical decision rather than weakening
the CPU/HLSL contract.
