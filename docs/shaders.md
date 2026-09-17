# HLSL and device policy

I share arithmetic graphs between C++ and HLSL. CPU state does not establish
what a shader device does, so I keep shader policy selection and admission
separate from the host types.

`find_package(ftz CONFIG REQUIRED COMPONENTS hlsl)` supplies `ftz::hlsl`, its
public shader include directories, and the `simd::headers` dependency. Configure
`FTZ_BUILD_HOST=OFF` to install these without a host compiler. Consumers own shader
entry points and compiler invocations; `ftz::hlsl` is a header library.

HLSL includes `<ftz/ftz32.h>` and uses `ftz::ftz32`. Compile in HLSL 2021 mode.
`ftz::floor`, `ftz::ceil` and `ftz::trunc` use the corresponding shader intrinsics
and preserve the wrapper type without an additional normalization pass.
`FTZ_FP32_HARDWARE_FTZ=0` retains explicit normalization; `=1` selects the admitted
hardware path. Choose the compiled shader variant after device qualification,
including [signed tiny-result add/subtract checks](../tests/shader_add/README.md).
The hardware path uses native addition/subtraction directly; a failed raw sign
check requires the explicit variant.
Shader policy selection is independent of the CPU type and thread environment.
`FTZ_SHADER_INT64` selects native 64-bit integer support where available, with 32-bit word operations as the
portable alternative.

The [complete shader example](../tests/api/README.md) shows the public factories
and a compute entry point. The [installed shader consumer](../tests/shaders/README.md)
checks exported include roots and header dependency tracking after relocation.

DXC compilation, SPIR-V validation and SPIRV-Cross translation establish a
compiler path. The [device records](validation.md) separately
state the RTX and native Metal execution scope, guards and permitted differences.
They are not throughput measurements or a claim about every GPU.
