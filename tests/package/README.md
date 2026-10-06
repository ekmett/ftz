# Installed C++ consumer

This standalone project calls `find_package(ftz)` only. It obtains native's module
providers, headers and archive through FTZ's exported dependency, then builds
another static library with an exported C++26 module. The final executable
imports that library's module and links only its CMake target.

The exported inline wrapper uses `native::simd<ftz::ftz32,4,Arch>` and FMA across the module
boundary; an out-of-line function requires real archive linkage and native FTZ
controls. Configure `TEST_ISA=AVX2`, `AVX512`, or `NEON` for the chosen package.
`TEST_IPO` and `TEST_PCH` exercise LTO and separately compiled PCHs. Run only on
a host admitted for the selected ISA.

Use separate relocated installation prefixes for native and FTZ. No repository
include directory or prebuilt PCM is accepted as an extra input. Inspect the
final link command to verify that both dependency archives were selected.

Use the same compiler, standard library, exception settings and runtime-library
search paths as both installed dependencies. For example, a libc++ producer
requires `-DCMAKE_CXX_FLAGS=-stdlib=libc++` on this consumer too. See
[validation](../../docs/validation.md) for the installed-consumer checks.
