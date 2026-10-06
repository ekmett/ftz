# Building and consuming FTZ

Build FTZ against an installed [native](https://github.com/ekmett/native) package.
Use the same compiler, standard library and exception settings throughout the
application and its dependencies so their C++ modules remain compatible.

## Native packages

Use Clang 23, CMake 4.4 and Ninja, with an installed native package. Both packages
must use compatible compiler, standard-library, exception and floating-point
settings. CMake rebuilds consumer BMIs from installed module sources. Keep one
consistent dependency configuration through an application and its libraries.

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH=/path/to/native -DCMAKE_BUILD_TYPE=Release \
  -DFTZ_ENABLE_PCH=ON -DFTZ_ENABLE_IPO=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/ftz
```

Use `clang-cl` on Windows. Tests default to AVX2 on x86 and NEON on ARM64.
On a host admitted for AVX-512, set `-DFTZ_TEST_PROFILES="AVX2;AVX512"`
to exercise both native widths; the test executables assume their selected ISA
is available. Merely compiling an ISA provider does not establish that.
Exceptions default to disabled. An exception-enabled
consumer uses packages built with both `NATIVE_ENABLE_EXCEPTIONS=ON` and
`FTZ_ENABLE_EXCEPTIONS=ON`.

```cmake
find_package(ftz CONFIG REQUIRED COMPONENTS ftz)
add_executable(example example.cc)
target_link_libraries(example PRIVATE ftz::ftz native::native)
native_target_profile(example AVX2)
```

`ftz::ftz` supplies the `ftz` and `ftz.controls` modules and static archive. It
depends on native's module provider and headers. FTZ owns its exponential graph
and uses the provider's register operations without adding optional ISA
requirements. Numerical consumers link
`native::native` and import `native`; import `native.math` when using raw SIMD math.
The FTZ numerical overloads are exported by `ftz`. ISA values select vector
families within that hub. The profile target aliases `native::avx2`, `native::avx512` and `native::neon`
refer to the same hub, not separate profile archives.

FTZ's numerical helpers use the consuming translation unit's native ISA
settings. Keep `native_target_profile` on those consumers and admit the selected
ISA before entry. Importing the hub does not perform admission or configure the
thread's FP controls. Controls-only consumers inherit the configured native
package minimum without an additional numerical profile. That minimum defaults
to the compiler target baseline; the dependency
build can configure it through `NATIVE_MINIMAL_COMPILE_OPTIONS`. Applications must
admit that minimum too. Include both installed package prefixes in
`CMAKE_PREFIX_PATH`.

Native vectors use `native::simd<T,N,Arch>`. Architecture values have type
`native::isa<Family>`, with `native::isa<>` denoting the compiler target family.
An ARM or Wasm value cannot select an x86 register backend, and vice versa.
Omitting `Arch` uses the baseline captured by the native module provider; stronger
vectors need an explicit admitted feature set such as `native::avx2` or
`native::neon`. Keep one compatible module build throughout an application.

`FTZ_FP32_HARDWARE_FTZ` is a package build setting selecting the compatibility
alias `ftz::ftz32`, and defaults to zero. Importing a module does not export
preprocessor macros, and a consumer `#define` does not reselect its compiled
alias. It does not disable either `m32` or `h32`, or configure thread controls.
New code can name its policy directly.

## Compiler caching

CI uses sccache for native and FTZ producer builds. Installed consumer checks
run without a cache launcher. The repository's launcher includes explicit PCH
inputs in the cache key and bypasses caching for module/PCH arguments it cannot
safely interpret, including opaque Windows response files. Ordinary Windows
translation units remain cacheable. See [validation](validation.md) for the
invalidation checks.

For local producer builds, install sccache separately and put it on `PATH`.
Add `-DCMAKE_CXX_COMPILER_LAUNCHER=sccache` for its normal local disk cache, or
use the same platform-aware launcher from the FTZ checkout:

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
launcher. [Validation](validation.md) describes the cache
checks; successful builds alone do not establish hosted
cache reuse or a speedup.

## API reference

The host and shader references are generated from the public interfaces and use
compiled consumers for the examples. Generating the documentation does not
execute those consumers or qualify a numerical environment.

Doxygen 1.18 builds separate host and HLSL references from the public interfaces,
with compiled example snippets. An installed native header package is enough for
a documentation-only build:

```sh
cmake -S . -B build/docs -G Ninja -DFTZ_BUILD_HOST=OFF -DFTZ_BUILD_DOCS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/native
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
after a push to main. It pins Doxygen and the native dependency, treats documentation
warnings as errors, and checks generated links before uploading the site.
Generated HTML stays in the build and deployment artifacts, outside the source tree.

## Hosted native package checks

The [Native packages workflow](https://github.com/ekmett/ftz/blob/main/.github/workflows/native.yml)
runs package, numerical and relocated module-consumer checks on pull requests
and manual dispatch. The default pair is Linux ARM64 and Windows ARM64; routine
Mac and x86 Linux work runs locally. Main pushes do not launch this build matrix.
A manual dispatch with `full_matrix` also includes the other three platforms:

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
with the matching Visual Studio SDK environment. The workflow pins the native
dependency revision.

Intel macOS, AVX-512 and GPU execution are not in this hosted matrix.

## Optional MPFR accuracy tests

`FTZ_BUILD_MPFR_TESTS` defaults to `OFF`. Set it to `ON` with host/tests enabled
to measure `log` and `log1p` against installed MPFR/GMP. A missing dependency
produces a configuration error; nothing is fetched, installed globally, or
added to package exports. See the
[oracle method and sampling scope](https://github.com/ekmett/ftz/blob/main/tests/log/README.md#optional-mathematical-accuracy-oracle).
