# Validation

FTZ validation separates three questions: whether the arithmetic follows its
specified graph, whether a compiled CPU or shader environment admits that graph,
and whether an installed consumer sees the same types and definitions. The
records below keep those scopes separate. Packet equality is evidence for the
recorded inputs; exact-rational bounds establish only their stated theorems.

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

The recorded Windows utility and rounding qualification uses matching SIMD and FTZ packages built
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

These focused checks supplement the earlier math packets. A separate Apple M3
installed-consumer run passes thirteen focused tests and one transitive-library
test with exceptions enabled, PCH and ThinLTO. It covers both policies and all
four rounding modes; the original 2,208-word arithmetic/function golden remains
unchanged. Source and dependency hashes match before and after execution. This
extends utility and rounding coverage without rerunning the larger math packets.

## Linux installed CPU packages

Exact source `3e2da97` passes 23 host tests and one relocated third-static-library
consumer on an Intel Core i9-12900K running Ubuntu 22.04/glibc 2.35. Both use the
relocated SIMD `4255f00` package, LLVM 23.1.1 with its bundled libc++ 23,
CMake 4.4.3, exceptions enabled, PCH and ThinLTO. The consumer imports its own
module and obtains both dependency archives through the exported package graph.

The configured compatibility alias selects manual policy. Both explicit policy
types remain present: `m32` runs under gradual and flush controls, and admitted
`h32` runs under flush controls. Tests include admission, conversion contracts,
short vectors, classification, rounding and native math. Source and installed
file hashes are unchanged after execution, old installation paths are absent,
and consumer compile commands contain no production source-checkout paths.

Only AVX2 kernels execute on this host. The SIMD dependency additionally compiles
its AVX-512 provider, without executing it. This extends Linux CPU/package
coverage; it adds no cross-host packet-equality, shader, sanitizer or throughput
claim to the separately recorded results.

## Shader evidence

The RTX 4090 execution bank passed twenty exact controls: arithmetic,
trigonometric/exponential functions, atan2, tanh and logarithms, each under both
FTZ policies and both 32/64-bit integer implementations. Input and output guards
and Vulkan validation passed. The combined host-math record uses unchanged shader artifacts. The admitted
hardware variant subsequently removed tiny add/subtract repair; its separate
rerun also passed all twenty RTX cases.
Its separate [signed-add admission bank](../tests/shader_add/README.md) passes
96,916 records against an integer-exact RNE-then-FTZ oracle, including raw and
wrapped add/subtract. Input and output guards and validation remain clean.
The same oracle also agrees with all 87,772 original addition records.

The hardware admission shader has one `OpFAdd` and one `OpFSub`; its wrapped
outputs reuse the raw results. The explicit version retains its zero-sign repair.
This establishes the generated operations and the tested RTX behavior; no
throughput measurement accompanies it. SPIRV-Cross translation produces
Metal source; the translation check alone does not establish device behavior.
The separate native Metal qualification below exercises the generated shaders.

The [directed-rounding shader fixture](../tests/rounding/README.md) separately
passes 276 records per policy on RTX 4090. All non-NaN floor/ceil/trunc words,
including signed zero, and input echoes match the integer oracle; input/tail
guards and Vulkan validation pass. This is sampled device evidence for that
shader source. DXC HLSL 2021 compilation, SPIR-V validation and translation to
Metal source also pass. Native Metal execution is recorded separately below.

### Native Metal

On Apple M3 with macOS 15.5, the corresponding native Metal bank passes all
24 cases: twenty arithmetic/function policy and integer-width variants, two
signed-add admission cases, and two directed-rounding cases. The latter four
use the 64-bit integer implementation. Across 737,332 records (2,949,328 fields),
every required word matches the reference bank. The two rounding cases contain
24 permitted NaN representation differences; input echoes remain exact.

Input and output-tail guards pass, with Metal API and GPU validation enabled
and no validation diagnostics. The build uses Clang 23.1.1 for the unchanged
native runner and Metal 3.2 without fast math. Generated 32-bit-word shaders
emit sixty unused-variable warnings; those warnings are retained in the record.
This qualifies the frozen shader banks, not a full application build or a
throughput claim.

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

The simultaneous-policy record separately covers exception-enabled Windows
consumers and matched-dependency AddressSanitizer checks of admission, typed
storage, short swizzles and both policy types. The native math pass above is
ordinary execution; its additional functions do not inherit those earlier
sanitizer results. No iPhone or browser execution is
established by these Windows and M3 checks.
