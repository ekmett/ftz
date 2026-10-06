# Math functions and implementation

Use unqualified calls such as `exp(x)` and `atan2(y, x)` with FTZ values.
The overloads preserve the input policy, whether the value is a scalar, a SIMD
register or a pack of registers. C++ and HLSL share coefficients, rounding points
and special-value rules. NaN sign and payload are the exception to bit equality.

| Operation | Use |
| --- | --- |
| `exp`, `expm1` | Exponential growth and decay; `expm1` retains small changes near zero. |
| `log`, `log1p` | Natural logarithms; `log1p` retains small changes near zero. |
| `tanh` | A smooth signed saturation to [-1,1]. |
| `sin`, `cos`, `sincos` | Angles in radians; the paired call shares range reduction. |
| `atan2(y, x)` | An angle with the correct quadrant, including signed axes. |
| `/`, `sqrt` | Reproducible division and square-root approximations. |
| `floor`, `ceil`, `trunc` | Directed rounding, preserving signed zero. |
| `scaleb`, `masked_scaleb`, `masked_scaleb_zero` | C++ FTZ vector scaling by powers of two. |

The arithmetic graph defines reproducibility, not a claim of correctly rounded
mathematical results. See
[arithmetic and thread controls](arithmetic.md) for the required FP environment,
conversions and classification functions.

## Exponential

`exp<Degree=6>` accepts compile-time degrees 1 through 7 on scalar values, SIMD
registers, register arrays and HLSL values. Ordinary `exp(x)` calls select degree
6, whose constant and linear coefficients are exactly one. All degrees use the
same nearest-even reduction and two-part ln(2) subtraction. The lower cutoff is
`-87.33654022216796875f`; the last input before forced overflow is `88.72283172607421875f`.
Larger inputs return positive infinity.
Degrees six and seven retain finite results through that endpoint;
lower-degree approximation error can overflow earlier.

| Platform | Reconstruction |
| --- | --- |
| AVX-512 with an admitted scaling instruction | Masked `VSCALEFPS`, selected by the vector's requirements and available overload. |
| AArch64 NEON | Biased exponent conversion with `FCVTZU`, integer shift and one scale multiply, plus a selected doubling for n=128. |
| AVX2 | `CVTTPS2DQ`, signed maximum with zero, integer shift and one scale multiply, plus a selected doubling for n=128. |

For the last two paths, every active finite exponent lies in [-126,128]. The
lower cutoff excludes the minimum-normal rounding strip. These bounds permit
one scale multiply below n=128. At n=128 the first factor is 2^127 and
the second is 2, with an exact normal first product. Lower lanes retain their
first product directly under all admitted FP modes. Underflow and overflow flags
are computed independently of the polynomial and select its final result; they
do not clamp the input on the reduction dependency chain.

The integer conversions have defined behavior for NaNs and out-of-range values.
NaN payloads remain outside bit equality; every other result word, including
signed zero, is part of the cross-platform contract. NaNs already reach the
polynomial and propagate through the final product; a separate NaN mask before
conversion adds no value. C++ floating-to-integer casts with an out-of-range
precondition are not a substitute for those instructions.

Independent register chains advance through each reduction or polynomial stage
together. Constants are single register values shared across the pack. Larger
packs expose more independent work, but live coefficients, masks and accumulators
eventually spill.

The [Sollya script](https://github.com/ekmett/ftz/blob/main/tests/exp_fit/fit.sollya)
reproduces degrees 1 through 6; the [coefficient tables](https://github.com/ekmett/ftz/blob/main/tests/exp_fit/README.md) list
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
polynomials. The [fit notes](https://github.com/ekmett/ftz/blob/main/tests/exp_fit/README.md) distinguish sampled
accuracy from the exact cross-platform operation contract.

## Small changes: expm1 and log1p

Subtracting one from `exp(x)` loses small results near zero. `expm1` instead uses
a residual polynomial, nearest-even reduction and split ln(2). It returns tiny
inputs directly, preserves signed zero, and handles the reconstruction rounding
cases at exponents -25 and zero. For `x > 1`, it uses `exp(x) - 1` with the default
degree-six fit. Negative infinity gives -1; positive infinity gives infinity.

`log1p` evaluates `log(1+x)` directly on [-0.5,1], using a sign-selected
polynomial. Tiny inputs return their original value. Outside that interval it
uses `log` on the rounded sum `1+x`. At -1 it returns negative infinity; below
-1 it returns NaN. Signed zero is preserved.

The implementation also contains a checked damping helper for `1-exp(-a)` with
finite `a >= 0`, sharing the cancellation-safe `expm1` core. It is an internal
helper, not a separate exported function.

## Natural logarithm

`log` separates the input's exponent and mantissa, reduces the mantissa to
[0.75,1.5), and evaluates the same polynomial used by `log1p`. Two FMAs restore
the exponent's contribution using split ln(2). The subtraction of one from the
reduced mantissa is exact, including near one.

Either zero gives negative infinity. Negative nonzero inputs give NaN, positive
infinity stays positive infinity, and `log(1)` is zero. FTZ factories normalize
subnormal inputs before this calculation.

## Hyperbolic tangent

`tanh` uses piecewise odd polynomials, with coefficient selection per lane.
Tiny inputs return directly; large magnitudes saturate to signed one. This
avoids evaluating an exponential and division merely to form the same bounded
result. Signed zero is preserved, and infinities give signed one.

## Sine and cosine

`sin`, `cos` and `sincos` accept the full finite float range. For `|x| < 8192`,
a three-FMA reduction feeds the sine and cosine polynomials. Larger finite
angles use a fixed-point 2/pi reducer covering every float exponent. Packed
calls repair those exceptional lanes through the shared scalar implementation;
large angles therefore cost substantially more than the bounded path.

Use `sincos` when both results are needed. It shares reduction and repairs both
outputs together. C++ returns a pair, sine first; HLSL returns fields named
`sine` and `cosine`. Separate calls omit unused output work, though bounded SIMD
lanes can occupy different quadrants and still require both polynomials.

Sine preserves signed zero; cosine of either zero is one. Infinite and NaN
inputs return NaN.

## Two-argument arctangent

`atan2(y, x)` forms a magnitude ratio no larger than one using normalized
mantissas, a bit-derived reciprocal seed and three Newton refinements. A fixed
Horner polynomial evaluates the reduced angle; quadrant and sign reconstruction
produce the result in radians. Tiny ratios bypass the polynomial result.

Signed axes, infinities and NaNs have explicit handling. This is a fixed
reproducible graph, not a hardware reciprocal estimate or vector division.
The coefficients are adapted from SLEEF; its source and license notices
accompany the implementation.

## Division and square root

`/` and `sqrt` normalize their operands, refine a bit-derived reciprocal or
reciprocal-square-root seed three times, and restore the exponent with integer
operations. Boundary handling supplies signed flushing and special values.
`sqrt` preserves signed zero and positive infinity; negative nonzero inputs
return NaN.

These are approximations with a shared operation sequence, not promises of
correctly rounded division or square root. Reciprocal and reciprocal-square-root
helpers are internal; they are not additional exported math functions.

## Vector scaling

C++ FTZ vectors provide `scaleb(value, exponent)`, scaling by
`2^floor(exponent)`, plus merge-masked and zero-masked forms. The implementation
adjusts the exponent field with integer operations. It handles signed zeros,
infinities, NaNs and the rounding boundary at minimum normal without a per-lane
scalar repair. Arbitrary exponent inputs are bounded before integer conversion;
the special-value result is selected separately.

This general algorithm is separate from `exp`'s bounded reconstruction. FTZ
scaling is a reproducible numerical operation and can take several instructions.
Forwarding a raw native vector with an FTZ exponent is available only when the
native target provides a scaling instruction.

## Coverage and platform limits

The [validation guide](validation.md) describes the maintained graph, accuracy,
package and shader checks. Equality across implementations and accuracy against
the mathematical function are measured separately. Increasing register-pack
width can cause spills; the API does not promise that larger packs run faster.

FTZ currently exposes 32-bit floating-point arithmetic. It does not export
`exp2`, `log2`, or 16-bit numerical types. Native's math functions are a separate
API with their own contracts.

There is no qualified FTZ WebAssembly or WebGPU execution path. Baseline Wasm
SIMD128 lacks fused multiply-add, and relaxed SIMD does not guarantee the fused
rounding required by these graphs. The shared 32-bit integer shader helpers do
not by themselves supply a complete backend.
