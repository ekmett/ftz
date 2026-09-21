# FTZ controls and admission checks

The controls-only consumers retain the configured SIMD minimum, without an
additional profile. The compile guards compare AVX2/AVX512 capabilities against
the package's public `NATIVE_MINIMAL_HAS_AVX2` and `NATIVE_MINIMAL_HAS_AVX512`
definitions, rejecting accidental stronger or weaker compilation. They test both FP modes,
restoration around borrowed regions and external calls, thread-state isolation,
invalid-mode preservation, and the owned-thread initializer. The initializer
never invokes the witness callback.

The admission fixture exercises the unchanged 55 callback observations and
failure precedence, including NaN equivalence and signed-zero distinction.
It tests common classification, not ISA availability or a per-thread math gate.
The real scalar evaluator is tested separately through `import ftz`.

Ordinary and IPO thread-entry callers remain distinct targets. The CMake option
`FTZ_ENABLE_IPO` enables the latter; an ordinary caller is always retained.

The native API fixture checks family-specific architecture arguments, rejects
foreign-family vector tags, checks default one-lane storage, and verifies Boolean
scalar masks, array masks and vector/wide classification types for both policies.
The same source is compiled by the installed API consumer project.
