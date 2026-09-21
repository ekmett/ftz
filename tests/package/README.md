# Installed C++ consumer

This standalone project calls `find_package(ftz)` only. It obtains SIMD's module
providers, headers and archive through FTZ's exported dependency, then builds
another static library with an exported C++26 module. The final executable
imports that library's module and links only its CMake target.

The exported inline wrapper uses `native::simd<ftz::ftz32,4,Arch>` and FMA across the module
boundary; an out-of-line function requires real archive linkage and native FTZ
controls. Configure `TEST_ISA=AVX2`, `AVX512`, or `NEON` for the chosen package.
`TEST_IPO` and `TEST_PCH` exercise LTO and separately compiled PCHs. Run only on
a host admitted for the selected ISA.

Use separate relocated installation prefixes for SIMD and FTZ. No repository
include directory or prebuilt PCM is accepted as an extra input. Inspect the
final link command to verify that both dependency archives were selected.

The Linux AVX2 qualification uses Clang 23.1.1, libc++ 23, CMake 4.4.3 and
exceptions enabled in both installed dependencies. Supply the same
`-DCMAKE_CXX_FLAGS=-stdlib=libc++` and LLVM runtime-library search configuration
when configuring this consumer; mixing standard libraries is not covered.
The recorded PCH/ThinLTO transitive consumer passes after both prefixes move.
See [validation](../../docs/validation.md#linux-installed-cpu-packages) for the
source revisions and execution limits.
