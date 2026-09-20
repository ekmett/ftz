# Installed shader consumer

Configure this directory against separately installed `native` and `ftz` prefixes.
It has no C/C++ language enabled and rejects loading either host archive target.
The DXC include paths come entirely from `ftz::hlsl`, including the transitive
`native::headers` dependency that provides the attributes.

The build compiles both FTZ policies and both shader integer-width policies,
validates the SPIR-V and emits Metal source. This checks the package and shader
compiler boundary, not GPU numerical execution.

The entry exercises arithmetic, comparisons, float-right-hand operands, conversion
factories, the scalar math family, the paired sine/cosine result, sign operations,
and classification predicates. It writes eight output records per input when run.
The custom compiler commands depend on the actual exported header file sets, so
changing an installed FTZ or SIMD attributes header rebuilds the shaders.
