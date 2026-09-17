# Validation

The combined math checkpoint was exercised with Clang 23.1.1, CMake 4.4.3 and
Ninja on Windows x86-64 and Apple M3. Both policies are present in one library:
manual arithmetic runs under gradual and flush controls; hardware arithmetic
runs under its admitted flush controls. Hardware-with-gradual cases remain
negative admission tests, never claimed as qualified numerical executions.

| Configuration | Result |
| --- | --- |
| Windows AVX2 and AVX-512, PCH and ThinLTO producer | 32 tests passed |
| Windows installed native math and policy-boundary consumers | 12 tests passed |
| Windows third static library using installed modules, PCH and ThinLTO | One test passed per ISA |
| M3 NEON, PCH and ThinLTO producer | 19 tests passed |
| M3 relocated third-library consumer | One test passed |
| Exact-rational tanh, log/log1p and atan2 graph checks | All three passed |

Native arrays, short widths 1/2/3/4, x86 widths 8/16 and wide register packs are
checked against the scalar graph. The same recorded common-width packets are
byte-identical on AVX2, AVX-512 and NEON:

| Packet | Binary32 words | SHA-256 |
| --- | ---: | --- |
| tanh | 325,044 | `ba5ebdbc1867b91108353b64a85259677e711fcdf1006058916f497aae7ce991` |
| log and log1p | 2,023,656 | `6c24a76198a329ecae0dba9525c1a047840fb0a808bd120a206a7504f75f07d8` |
| separate sin/cos and paired sincos | 318,120 | `edd127a9baa992bf7be564b0a58cd420c46e8287eaad465ef8d3b90bab260209` |
| atan2 | 21,768 | `4f0cae730863e2a2d65546ff371bedb5ca13848acd9479869e0ade68949e5f12` |

The comparator permits NaN representation differences, but these packets have
none. Every non-NaN word, including signed zero, must match. The original
2,208-word arithmetic/function golden also remains unchanged. These are sampled
regressions of a specified operation graph, not exhaustive libm accuracy proofs.
The rational scripts establish the stated intermediate bounds and coefficient
identity; they do not establish correct rounding of the mathematical function.

The packed implementations of log/log1p, tanh and atan2 have no scalar calls in
the inspected ordinary optimized x86 leaf functions. Large wide packs can still
spill registers. General multiply/FMA retain rare tiny-result repairs required
to reconcile supported underflow rounding boundaries. Hardware policy removes
redundant FTZ operations; it does not promise every operation is one instruction.

## Value utilities, directed rounding and installed packages

A subsequent Windows qualification uses matching SIMD and FTZ packages built
with exceptions enabled, PCH and ThinLTO. The core passes 35 tests and one
relocated consumer. Focused FTZ consumers pass on both AVX2 and AVX-512:

| Installed consumer | Result |
| --- | --- |
| Native and wide classification/sign transport | Four tests passed |
| Directed rounding | Two tests passed |
| Policy boundaries | Four tests passed |
| Third static library, PCH and ThinLTO | One test passed per ISA |

Classification preserves each vector's actual mask type, including through
`wide`. Sign transport and short swizzles preserve signed zeros and NaN payloads
without changing FP status. `floor`, `ceil` and `trunc` retain the selected FTZ
type through scalar, vector, array and wide operations. Their independent
integer-word oracle runs under all four standard rounding modes, with manual
policy in gradual/flush modes and hardware policy in flush mode. This qualifies
the rounding operations independently of ambient rounding; other arithmetic
retains its nearest-even contract.

These are later focused checks, not reruns of every historical math packet.
No new NEON execution of these value utilities or directed rounding is recorded;
the M3 math results above retain their original scope.

## Shader evidence

A separate RTX 4090 execution bank passed twenty exact controls: arithmetic,
trigonometric/exponential functions, atan2, tanh and logarithms, each under both
FTZ policies and both 32/64-bit integer implementations. Input and output guards
and Vulkan validation passed. The original combined host-math checkpoint left the executed shader artifacts
unchanged. A subsequent shader optimization removes tiny add/subtract repair
from the admitted hardware variant; the twenty RTX cases were rerun and passed.
Its separate [signed-add admission bank](../tests/shader_add/README.md) passes
96,916 records against an integer-exact RNE-then-FTZ oracle, including raw and
wrapped add/subtract. Input and output guards and validation remain clean.
The same oracle also agrees with all 87,772 original addition records.

The hardware admission shader has one `OpFAdd` and one `OpFSub`; its wrapped
outputs reuse the raw results. The explicit version retains its zero-sign repair.
This is generated-code evidence, not a measured throughput improvement. The new
hardware shader graph has RTX qualification. SPIRV-Cross translation produces
Metal source; it does not establish native Metal compilation or execution.

The [directed-rounding shader fixture](../tests/rounding/README.md) separately
passes 276 records per policy on RTX 4090. All non-NaN floor/ceil/trunc words,
including signed zero, and input echoes match the integer oracle; input/tail
guards and Vulkan validation pass. This is sampled device evidence for that
shader source. DXC HLSL 2021 compilation, SPIR-V validation and translation to
Metal source also pass, without native Metal compilation or execution.

The shader-only installed-package consumer also compiles four policy/integer
variants from a relocated prefix without loading either host archive. Its build
tracks the exported header file sets, so changing an installed header causes
recompilation. See [the shader consumer](../tests/shaders/README.md).

## Repeating the checks

The default host CMake build includes the numerical suites. Select only native
profiles supported by the execution host. Focused installed-consumer projects
live beside their tests. Pass a packet output path to tanh, log or atan2; trig
also takes `--common`. Compare packets with `tests/compare_fp32.py`.
Run `tests/{tanh,log,atan2}/verify_bounds.py` against the current source to check
the live coefficient words and their exact-rational bounds.

The preceding simultaneous-policy checkpoint separately passed exception-enabled
Windows consumers and matched-dependency AddressSanitizer checks of admission,
typed storage, short swizzles and both policy types. The new native math pass
above is ordinary execution; those older sanitizer results are not relabeled
as sanitizer coverage of every new function. No iPhone or browser execution is
established by these Windows and M3 checks.
