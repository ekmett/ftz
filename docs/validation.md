# Validation

I keep three questions separate: does the arithmetic follow its specified graph,
does a compiled CPU or shader environment admit that graph, and does an installed
consumer see the same types and definitions? The records below answer those
questions within their measured scope. Packet equality is evidence for the
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

## WebAssembly module consumer

FTZ source `51ba559` with SIMD `c2360bd` passes an Emscripten 6.0.9 installed-module
consumer under Node. The manual `m32` path passes 4,229 checks under verified
fixed nearest-even, gradual binary32 behavior. Its 4,820-word packet is byte-identical
to the matching Windows scalar control, with SHA-256
`70f275a882ca908e721fcc3ff775052f297f52be425775af81fc00c4522c41cb`.

The producer and relocated consumer use the exported module/package graph.
Native CPU-control admission reports `environment_unavailable` on this target;
`h32` is rejected. Unsupported rounding-mode warnings are retained. This check
uses neither PCH nor IPO and establishes no browser, GPU or configurable FP-state
behavior. It does not broaden the native hardware-policy admission contract.

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

## Compiler cache

FTZ `4ba71e2` plus the cache integration was checked on macOS 15.5 ARM64
using Clang 23.1.1, CMake 4.4.3, Ninja 1.12.1 and sccache 0.16.0. The SIMD
dependency remained at the workflow's qualified `3bd4d98` revision. Both
packages used Release, NEON, exceptions, PCH and IPO; builds used two compiler
jobs. SIMD was built and installed once with the normalized launcher before
comparing the FTZ producers. Each comparison used a separate fresh build tree,
local disk cache and server; its warm pass cleaned outputs, retained the cache
and reset statistics.

| FTZ launcher | Build | Hits / requests | Misses | Bypasses | Build wall time |
| --- | --- | --- | --- | --- | --- |
| Plain sccache | Cold | 0 / 28 | 1 | 27 | 23.15 s |
| Plain sccache | Clean warm | 1 / 28 | 0 | 27 | 23.23 s |
| Module-map expansion | Cold | 0 / 28 | 28 | 0 | 24.48 s |
| Module-map expansion | Clean warm | 28 / 28 | 0 | 0 | 4.10 s |

The plain launcher cached only the PCH; all 27 remaining requests bypassed
with reason `@`. Expansion admitted 25 C++ compilations, two named module
producers and the PCH. All four producer passes reported zero cache errors and
passed all 22 CTests. The initial SIMD dependency build admitted its eight
module producers and one PCH with no bypasses or cache errors.

Both installed packages were then moved into paths containing spaces. With no
sccache on `PATH` and an empty compiler launcher, the transitive installed
consumer passed 1/1 test and the API consumers passed 3/3. Installed CMake
metadata contains no compiler-launcher dependency. The seven copied launcher
test methods also pass; the five-platform workflow matrix, numerical sources,
SIMD pin and package settings are unchanged.

The wall times are single local observations of `cmake --build`, excluding
configuration, cleaning, CTest and dependency installation. They are not a
repeated benchmark or a hosted CI speedup claim; the cold normalized build
was slower. Statistics exclude dependency scanning, linking and generated BMI
commands that do not use the launcher. GitHub cache-service reuse and native
Windows/Linux execution remain unverified by this run. The Windows workflow
retains plain sccache and its existing unsupported-request bypasses.

Workflow YAML, Bash syntax and whitespace checks pass. New documentation links
use absolute GitHub destinations so they do not escape the generated site;
the three link-checker unit tests pass. Doxygen 1.18 was unavailable locally,
so this check does not establish a generated-documentation build.

The Windows setup action also incorporates SIMD `127533c`'s correction for an
observed hosted CI prerequisite: preserve the SDK environment's `PATH` order
when exporting it through `GITHUB_PATH`, and qualify Git Bash after tool setup.
The previous ordering selected the WindowsApps WSL stub instead of Git Bash.
Native Windows confirmation of this correction remains pending; it does not
change the compiler, SDK versions or arithmetic settings.


### PCH-dependent module invalidation

The earlier unchanged-input cache checks did not establish PCH binary
invalidation. SIMD CI run `35300325585`, job `105461412565`, subsequently
restored a module recording a 19,198,904-byte PCH alongside a newly generated
19,198,912-byte PCH; Clang correctly rejected the combination. The earlier
two-file module fixture did not exercise this PCH dependency.

sccache 0.16.0 hashes explicit module inputs, but treats `-include-pch` only
as a preprocessing argument. The launcher now appends explicit PCH binary
inputs to `SCCACHE_EXTRAFILES`, retaining existing entries. Unknown response
or PCH syntax runs the original compiler directly. No compiler validation is
disabled; modules, PCH and IPO remain enabled.

The hosted `test_sccache_pch.py` fixture checks cold and warm module builds,
a rebuilt PCH whose bytes change while preprocessing stays equivalent,
unchanged reuse afterward, and identical PCH bytes with a changed timestamp.
Each stage compiles a fresh uncached importer so Clang validates the restored
module against the actual PCH. The fixture uses its own local disk cache,
empty configuration, and a short Unix-domain socket in a temporary directory.
Its child environment excludes inherited sccache settings; the normal producer
server and GitHub cache configuration remain untouched. The fixture retains
its entries and counters across stages, asserts hit/miss deltas and zero cache
read/write errors, and stops only its own server in a finally block. ThinLTO
remains enabled. This is a required
regression check, not a claim that the new hosted runs have already passed.
It rebuilds the PCH directly and tests the PCH-consuming module cache key;
cached PCH producer invalidation is unchanged and is not qualified by this
fixture. The five fixture requests are isolated from the producer job's
aggregate cache statistics; per-stage fixture statistics remain in artifacts.

## Independent log accuracy oracle

The optional MPFR fixture measures mathematical accuracy separately from
operation-graph equality. It leaves every coefficient, recurrence and rounding
point unchanged. Reports include exact worst-case words, absolute error near
zero, sampled monotonicity, cutover neighbors and special/domain checks, with
256/512-bit precision agreement.

[Run 35304442816](https://github.com/ekmett/ftz/actions/runs/35304442816), source
`7326ea6`, measured 36,935 distinct raw inputs with MPFR 4.2.1, Ubuntu Clang
23.1.2, CMake 4.4.3 and AVX2 on an Intel Xeon Platinum 8370C. The producer used
PCH, IPO and exceptions; the oracle translation unit used the existing
non-IPO numerical-fixture convention. All 24 native CTests, the relocated
package test and three API tests passed. The MPFR test took 1.75 seconds in
this run; this is not a repeated performance benchmark.

Each function ran under manual gradual, manual flush and hardware flush
controls: 221,610 input/function/policy observations in total. All three
configurations reported the same maxima below, zero sampled monotonicity
decreases, and no precision, domain, signed-zero or output-normalization
failures. This does not establish exhaustive accuracy or monotonicity.

| Function / measure | Input word | Graph output | Rounded FTZ reference | Observed error |
| --- | --- | --- | --- | --- |
| log / maximum ULP distance | `17b97dc0` | `c25c52bc` | `c25c52bd` | 1 binary32 step |
| log1p / maximum ULP distance | `bf7fffee` | `c15bec2e` | `c15bec2d` | 1 binary32 step |
| log / near-zero absolute | `3f7ff1f2` | `b960e62c` | `b960e62c` | 7.2744520291151217e-12 |
| log1p / near-zero absolute | `b977e239` | `b977e9ba` | `b977e9ba` | 7.2734128687377858e-12 |

Near-zero means mathematical output magnitude at most 2^-12; absolute errors
refer to the unflushed 512-bit result. ULP distance is an integer count from
the rounded output reference, not real error divided by one local ULP. In
particular, zero ULP distance can still have nonzero absolute error. Maximum
ties retain the first input in deterministic numeric-word order.

These four inputs and their MPFR reference words are retained explicitly in
the oracle bank. Per-case ULP-distance and absolute-error ceilings match the
observed values (absolute ceilings use the upward-rounded binary64 report).
The regression checks allow improved accuracy rather than freezing the old
graph's output words. Remaining bank values, cutover neighbors and sampled
monotonicity continue to be reported without requiring perfect accuracy.
The full report is retained as `ftz/tests/log/mpfr-avx2.txt` in the run's
`native-logs-ubuntu-24.04-AVX2` artifact. No MPFR-backed ARM, AVX-512, Windows
or GPU accuracy qualification is claimed by this measurement.


## Configured SIMD minimum and common BMI reuse

The configurable-minimum migration was checked locally against installed SIMD
`230ae465a8c5495d0eabb44d1f30e3b4d4b793b2`, using Windows x64 Clang-cl 23.1.1,
CMake 4.4.3 and AVX2 plus AVX512. The dependency used the default AVX2/FMA/BMI2
minimum and exceptions; FTZ enabled exceptions, PCH and IPO. All 38 native tests
passed, followed by a relocated AVX512 third-library package consumer with
PCH/IPO (1/1) and scalar/environment/AVX2/AVX512 API consumers (4/4).

The controls and dispatcher guards accept the advertised minimum and reject
unadvertised profile flags. Six compiler controls accepted a legacy baseline,
AVX2 minimum and stronger AVX512 minimum, and rejected legacy AVX leakage,
unadvertised AVX512 and compilation weaker than the advertised minimum. This
is compiler-option validation, not execution on an unsupported CPU.

Private clang-cl warning and object-format flags now stay on implementation
compiles, including PCH, instead of splitting imported module compile tuples.
Warning enforcement and floating-point/exception/runtime options are retained.
The generated native build graph has one provider for each of the eight common
SIMD modules, reduced from three groups differing only in diagnostics. The
producer and tests consistently disable language extensions. Fixture headers
precede imports; affected Windows packet file opens use `fopen_s` so warnings
remain enabled with the shared imported declarations.

Native CI now pins SIMD `e8bcda617d9dd67f37fccd1692dbe0727a179da6`, whose change
from the locally tested SIMD revision is confined to invalidating cached
minimum-feature configure probes when their input options change. The runtime
code and package partition are unchanged. Hosted validation of this exact pair
is recorded by the migration PR; the older numerical receipts above keep their
original dependency pins and scope.
