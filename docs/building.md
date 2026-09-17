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
target_link_libraries(example PRIVATE ftz::ftz simd::avx2)
simd_target_profile(example AVX2)
```

`ftz::ftz` supplies the `ftz` and `ftz.controls` modules and static archive. Its
baseline controls do not inherit a numerical ISA. The application selects the
provider for each numerical translation unit and checks CPU/OS support before
entering it. For a separate consuming project, include both package prefixes in
`CMAKE_PREFIX_PATH`.

`FTZ_FP32_HARDWARE_FTZ` is a package build setting selecting the compatibility
alias `ftz::ftz32`, and defaults to zero. Importing a module does not export
preprocessor macros, and a consumer `#define` does not reselect its compiled
alias. It does not disable either `m32` or `h32`, or configure thread controls.
New code can name its policy directly.

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
