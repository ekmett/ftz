# Building and consuming FTZ

I build FTZ against an installed [SIMD](https://github.com/ekmett/simd) package.
I keep the producer and every consuming library on one compiler/runtime
configuration; incompatible BMIs are not an application boundary.

## Native packages

Use Clang 23, CMake 4.4 and Ninja, with an installed SIMD package. Both packages
must use compatible compiler, standard-library, exception and floating-point
settings. CMake rebuilds consumer BMIs from installed module sources. Keep one
consistent dependency configuration through an application and its libraries.

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH=/path/to/simd -DCMAKE_BUILD_TYPE=Release \
  -DFTZ_ENABLE_PCH=ON -DFTZ_ENABLE_IPO=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/ftz
```

Use `clang-cl` on Windows. Tests default to AVX2 on x86 and NEON on ARM64.
On a host admitted for AVX-512, set `-DFTZ_TEST_PROFILES="AVX2;AVX512"`
to exercise both native widths; the test executables assume their selected ISA
is available. Merely compiling an ISA provider does not establish that.
Exceptions default to disabled. An exception-enabled
consumer uses packages built with both `SIMD_ENABLE_EXCEPTIONS=ON` and
`FTZ_ENABLE_EXCEPTIONS=ON`.

```cmake
find_package(ftz CONFIG REQUIRED COMPONENTS ftz)
add_executable(example example.cc)
target_link_libraries(example PRIVATE ftz::ftz simd::simd)
simd_target_profile(example AVX2)
```

`ftz::ftz` supplies the `ftz` and `ftz.controls` modules and static archive. It
depends on SIMD's common/minimal modules and headers. Numerical consumers link
`simd::simd` and import `simd`; architecture tags select vector families within
that hub. The compatibility targets `simd::avx2`, `simd::avx512` and `simd::neon`
refer to the same hub, not separate profile archives.

FTZ's numerical helpers still use the consuming translation unit's native ISA
settings. Keep `simd_target_profile` on those consumers and admit the selected
ISA before entry. Importing the hub does not perform admission or configure the
thread's FP controls. Controls-only consumers inherit the configured SIMD
package minimum without an additional numerical profile. That minimum defaults
to AVX2/FMA/BMI2 on x86 and the platform NEON baseline on ARM64; the dependency
build can configure it through `SIMD_MINIMAL_COMPILE_OPTIONS`. Applications must
admit that minimum too. Include both installed package prefixes in
`CMAKE_PREFIX_PATH`.

The hub's canonical architecture tags change C++ type identities from the older
profile modules. Rebuild FTZ and every consumer together, including libraries
whose interfaces contain SIMD values; do not mix old and new objects or BMIs.

`FTZ_FP32_HARDWARE_FTZ` is a package build setting selecting the compatibility
alias `ftz::ftz32`, and defaults to zero. Importing a module does not export
preprocessor macros, and a consumer `#define` does not reselect its compiled
alias. It does not disable either `m32` or `h32`, or configure thread controls.
New code can name its policy directly.

## Compiler caching

Native CI uses sccache 0.16.0 for the SIMD dependency and FTZ producer builds,
with Mozilla's commit-pinned
[action](https://github.com/Mozilla-Actions/sccache-action/tree/fc920bf0ec8de6ee65d409111f7ec508035751ba)
and the GitHub Actions cache backend. The action checks the release archive's
published checksum and supplies cache service credentials; repository permissions
remain `contents: read` and no additional secrets are needed. Advanced and JSON
statistics are retained with package diagnostics, including failed builds when
cache setup succeeded. They cover both producers; the relocated package/API
consumers deliberately configure without a cache launcher.

On Linux and macOS, a conservative
[launcher](https://github.com/ekmett/ftz/blob/main/.github/scripts/sccache_launcher.py) expands recognized CMake
`.modmap` response arguments before sccache. CMake quotes module paths, which
otherwise trigger the pinned cache's `@` bypass. The launcher accepts only
`-x c++-module`, quoted module output paths and named module input paths with
simple nonempty ASCII values. It expands argv without changing CMake's files.
Unknown syntax, whitespace inside values, malformed/compound quotes, escapes,
nested/other response files or size limits preserve the original compiler
arguments and bypass caching directly. An `E2BIG` retry also executes the
original compiler directly. This is not a general response-file parser.
Only POSIX compiler names `clang` and `clang++`, optionally followed by a
numeric version suffix, enter this cache path. Other names, including
`c++` and target-prefixed Clang aliases, execute the original compiler
arguments directly without caching so they cannot bypass PCH input hashing.

Explicit `-include-pch` binary inputs, including CMake's `-Xclang` spelling,
are appended to `SCCACHE_EXTRAFILES`; existing entries are preserved. The
pinned sccache version does not otherwise hash these PCH binaries, which can
leave a cached module referring to a different PCH. Ambiguous or missing PCH
inputs bypass caching. Windows keeps direct sccache because clang-cl's PCH
and forwarded module flags remain unsupported by the pinned release.

The launcher and tests are shared with SIMD; keep the implementations aligned.
POSIX CI runs `test_sccache_launcher.py` and the real PCH/module warm-cache
fixture `test_sccache_pch.py`. Neither disables PCH validation. See the
[validation boundary](https://github.com/ekmett/ftz/blob/main/docs/validation.md#pch-dependent-module-invalidation)
for the observed regression and checks required of this repair.

For local producer builds, install sccache separately and put it on `PATH`.
Add `-DCMAKE_CXX_COMPILER_LAUNCHER=sccache` for its normal local disk cache, or
use the same POSIX module-map launcher from the FTZ checkout:

```sh
-DCMAKE_CXX_COMPILER_LAUNCHER="$(command -v python3);$PWD/.github/scripts/sccache_launcher.py"
```

Use that argument on each producer's configure command. It is confined to the
build tree and is not exported into installed packages. Keep PCH, modules, IPO
and exception settings unchanged. Omit the argument for a fresh uncached tree,
or set `-DCMAKE_CXX_COMPILER_LAUNCHER=` to clear it in an existing tree.

To measure reuse, build and run CTest, record cache statistics, clean the build
outputs, run `sccache --zero-stats`, then rebuild and test with the same paths
and compiler. A no-op incremental build does not exercise caching. Dependency
scanning, linking and some CMake-generated BMI commands remain outside the
launcher. [Validation](https://github.com/ekmett/ftz/blob/main/docs/validation.md#compiler-cache) records the actual cache
coverage and timing limits; successful builds alone do not establish hosted
cache reuse or a speedup.

## API reference

I generate the host and shader references from the public interfaces and use
compiled consumers for the examples. Generating the documentation does not
execute those consumers or qualify a numerical environment.

Doxygen 1.18 builds separate host and HLSL references from the public interfaces,
with compiled example snippets. An installed SIMD header package is enough for
a documentation-only build:

```sh
cmake -S . -B build/docs -G Ninja -DFTZ_BUILD_HOST=OFF -DFTZ_BUILD_DOCS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/simd
cmake --build build/docs --target ftz_docs
```

Open `build/docs/docs/html/index.html` for C++, and
`build/docs/docs/html/shaders/index.html` for HLSL. Warnings fail the build.
Compile the [example projects](../tests/api/README.md) separately to check the
snippets against an installed package; documentation generation is not a
numerical or device qualification.

See [the compiled examples](../tests/api/README.md) for standalone consumer
commands, [arithmetic and thread controls](arithmetic.md) for runtime obligations,
and [shader headers](shaders.md) for a language-free package.

The [Documentation workflow](https://github.com/ekmett/ftz/blob/main/.github/workflows/docs.yml) builds both references
on pull requests and publishes them to [GitHub Pages](https://ekmett.github.io/ftz/)
after a push to main. It pins Doxygen and the SIMD dependency, treats documentation
warnings as errors, and checks generated links before uploading the site.
Generated HTML stays in the build and deployment artifacts, outside the source tree.

## Hosted native package checks

The [Native packages workflow](https://github.com/ekmett/ftz/blob/main/.github/workflows/native.yml)
runs the same package, numerical and relocated module-consumer checks on five
standard GitHub-hosted runner labels:

| Runner | Native profile |
| --- | --- |
| `ubuntu-24.04` | x86-64 AVX2 |
| `ubuntu-24.04-arm` | ARM64 NEON |
| `macos-15` | ARM64 NEON |
| `windows-2025` | x86-64 AVX2 |
| `windows-11-arm` | ARM64 NEON |

Each lane records the actual CPU, OS, compiler and dependency revision in its
own diagnostics artifact and rejects an incompatible architecture before the
build. Both packages retain exceptions, PCH and IPO; the installed packages move
to a path containing spaces before consumer builds. A passing hosted run qualifies
that runner and revision, not every CPU sharing its architecture. AVX-512 and GPU execution are outside this workflow.

Windows uses native, checksum-pinned LLVM 23.1.1, CMake 4.4.3 and Ninja 1.13.2
with the matching Visual Studio SDK environment. The setup action is copied from
SIMD `8f69034`; the separately pinned numerical dependency is unchanged.

Intel macOS is deferred until a qualified LLVM 23 toolchain artifact is available.
The hosted image supplies older Clang versions, Homebrew has no Intel LLVM 23
bottle, and the inspected official LLVM 23 releases provide macOS ARM64 archives
only. A full LLVM source bootstrap is not part of each package test run.

## Optional MPFR accuracy tests

`FTZ_BUILD_MPFR_TESTS` defaults to `OFF`. Set it to `ON` with host/tests enabled
to measure `log` and `log1p` against installed MPFR/GMP. A missing dependency
produces a configuration error; nothing is fetched, installed globally, or
added to package exports. See the
[oracle method and sampling scope](https://github.com/ekmett/ftz/blob/main/tests/log/README.md#optional-mathematical-accuracy-oracle).
