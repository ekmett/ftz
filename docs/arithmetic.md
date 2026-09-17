# Arithmetic and thread controls

I want an arithmetic policy to say what each operation means, including the
awkward cases at zero and the normal/subnormal boundary. I keep that policy in
the element type. I leave register width and instruction selection to
[SIMD](https://github.com/ekmett/simd).

## Policies and representation

| Type | Normalization | Required CPU denormal mode |
| --- | --- | --- |
| `ftz::m32` | Explicit signed flushing | Gradual or flush |
| `ftz::h32` | Admitted hardware flushing, with required boundary repairs | Flush |

Both types are four bytes and trivially copyable. Both are available from the
same `ftz` module and archive. `simd::vec<F,N,Arch>` and `simd::wide<V,M>` retain
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

## CPU environment and admission

I establish the environment at a thread boundary and keep it out of element
operations. A borrowed thread needs restoration; a thread owned by the numerical
application can use the direct initializer.

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

## Unimplemented formats

`ftz::m16` and `ftz::h16` are reserved for future reproducible BF16 arithmetic
without subnormal values. They are not implemented. Rounding, intermediate
precision and fused-operation rules need a separate contract.

See [validation](validation.md) for the measured scope of the arithmetic and
admission checks, and [shader policies](shaders.md) for the independent GPU contract.
