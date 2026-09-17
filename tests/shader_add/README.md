# Shader signed-FTZ addition

`values.hlsl` compares raw binary32 addition/subtraction with the corresponding
FTZ wrapper operations. Hardware admission requires signed output flushing,
round-to-nearest-even and preserved signed zeros, not just observation that some
subnormal result became zero. Keep the explicit policy on an unqualified device.

Generate input and independently rounded expected packets with:

```sh
python tests/shader_add/bank.py build/add-bank
```

The files contain flat little-endian `uint4` records. Bind inputs at set 0,
binding 1 and equally sized output storage at binding 2. The push constant is the
record count; dispatch 64 threads per group. Output fields are raw add, wrapped
add, raw subtract and wrapped subtract. Both operands enter through normalized
factories. The two unused input words are zero.

The oracle uses integer multiples of 2^-149, exact integer addition and explicit
binary32 ties-to-even rounding. Inputs cover all sign combinations, imported
subnormals, zero signs, cancellation at every finite exponent, normal boundaries,
overflow, infinities, NaNs and a fixed random word bank. Tiny finite sums are
already exact multiples of the minimum subnormal; they cannot round up across
the normal boundary. Their sign still has to survive hardware flushing.

Compile in HLSL 2021 with each `FTZ_FP32_HARDWARE_FTZ` setting, using include
roots supplied by `ftz::hlsl`. `FTZ_SHADER_INT64` does not alter this arithmetic.
Compare output packets with `tests/compare_fp32.py`; only NaN sign/payload may
differ. Use ordinary input/output guards and API validation in the device harness.
On a device without admitted raw behavior, raw fields may fail even when the
explicit wrapper fields match. Such a result does not admit the hardware variant.

This sampled gate qualifies the tested compiler/device profile; it does not
establish the behavior of every GPU or compiler. Multiply/FMA retain their
separate minimum-normal rounding repairs.
