# Arithmetic and thread controls

`ftz::m32` and `ftz::h32` give the same reproducible arithmetic using different
flushing strategies. Choose a type, establish its floating-point settings once
per thread, and use it as a scalar or as the element of a `native::simd` vector.
Changing the register width does not change the result contract.

## Policies and representation

| Type | Normalization | Required CPU denormal mode |
| --- | --- | --- |
| `ftz::m32` | Explicit signed flushing | Gradual or flush |
| `ftz::h32` | Admitted hardware flushing, with required boundary repairs | Flush |

Both types are four bytes and trivially copyable. Both are available from the
same `ftz` module and archive. `native::simd<F,N,Arch>` and `native::wide<V,M>` retain
the selected element policy; there is no per-operation policy dispatch.

## Values, conversions and math

Public factories replace subnormal inputs with signed zero, and arithmetic
returns normalized results. Normal values, infinities and signed zeros follow
the defined operation graph; NaN sign and payload are outside the arithmetic
contract. Reproducibility describes that graph. Approximate functions do not
promise correctly rounded libm results. Admission and regression evidence apply
to a particular compiled profile, compiler and device.

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
without changing a NaN payload. Generic `native::wide` forwarding retains that
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

See [math functions and implementation](math-kernels.md) for the available
operations, their algorithms and range behavior.

## Emulated vectors

Use `native::simd<ftz::m32,4,native::isa<>(native::polyfill)>` for a vector
with no hardware feature requirement. Its arithmetic and math use the scalar
semantic graph lane by lane. Adding `native::polyfill` to a native ISA retains
the native operations and permits Native to emulate missing operations or
storage. Both forms retain the selected FTZ policy and its thread requirements;
`h32` still requires admitted hardware flushing.

## Constant evaluation

Arithmetic, comparisons, classification and math work in constant expressions
for both policies, including SIMD, array and `wide` forms. They use the same
operation graphs and flushing rules as runtime evaluation; division and square
root retain their defined approximations. NaN sign and payload remain unspecified.

```cpp
constexpr auto x = ftz::h32(2.f) * ftz::h32(3.f);
static_assert(x.to_bits() == 0x40c00000u); // 6.f
```

Constant evaluation needs no floating-point scope or hardware admission. The
compiler computes the result with software arithmetic. Runtime calls retain
their native implementations and still require the environment described below.
Thread controls and hardware probes are runtime operations.

## CPU environment and admission

Set the floating-point environment when a numerical thread starts. Use a scope
to restore the previous settings when borrowing a caller's thread; use the direct
initializer for a thread the application owns. Neither belongs in an element loop.

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

`ftz.controls` owns environment controls independently of SIMD. At an
application-owned numerical thread entry, `set_native_fp32_mode(flush)` sets
controls and clears status without running admission, allocating, or arranging
restoration. Check its boolean result before starting numerical work. Use the fully qualified
enum value `ftz::native_fp32_mode::flush`. Establish the agreed controls on each
participating thread; run the compiled-profile qualification at startup rather
than repeatedly in arithmetic or at each task. CPU instruction and OS vector-state
admission is a separate prerequisite before entering an ISA-specific function.

Use `native_fp32_scope` when borrowing a caller's thread. Keep the scope and its
`external_scope` on that thread. The external scope restores the caller's
environment for third-party code, then reinstates the numerical region on return
or unwind. These APIs manage CPU state; GPU qualification is separate.

See [validation](validation.md) for the arithmetic and
admission checks, and [shader policies](shaders.md) for the independent GPU contract.
