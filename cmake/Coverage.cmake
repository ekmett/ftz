# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0

if(NOT FTZ_BUILD_TESTS OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang"
    OR CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "FTZ coverage requires a native Clang test build.")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(FTZ_GRCOV grcov REQUIRED)
get_filename_component(ftz_llvm_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(FTZ_LLVM_PROFDATA llvm-profdata HINTS "${ftz_llvm_bin}" REQUIRED)
find_program(FTZ_LLVM_COV llvm-cov HINTS "${ftz_llvm_bin}" REQUIRED)
get_filename_component(ftz_coverage_llvm_bin "${FTZ_LLVM_COV}" DIRECTORY)
set(ftz_coverage_dir "${CMAKE_CURRENT_BINARY_DIR}/coverage")
file(MAKE_DIRECTORY "${ftz_coverage_dir}/raw")
# Binary signature and process ID keep parallel tests from overwriting profiles.
# Directory options instrument providers and importers without changing the
# installed package's usage requirements.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  add_compile_options(
    "/clang:-fprofile-instr-generate=${ftz_coverage_dir}/raw/%m-%p.profraw"
    /clang:-fcoverage-mapping)
else()
  add_compile_options(
    "-fprofile-instr-generate=${ftz_coverage_dir}/raw/%m-%p.profraw" -fcoverage-mapping)
  add_link_options(-fprofile-instr-generate)
endif()

# Assembly probes follow the *_codegen.cc/codegen_*.cc convention. Disable
# counters in those consumers, reusing the instrumented module dependencies.
# Mixed runtime/assembly targets can explicitly set FTZ_COVERAGE to OFF.
function(ftz_coverage_probes directory)
  get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS targets)
    get_target_property(type "${target}" TYPE)
    if(type STREQUAL "EXECUTABLE")
      set_property(GLOBAL APPEND PROPERTY FTZ_COVERAGE_BINARIES "$<TARGET_FILE:${target}>")
    endif()
    get_target_property(sources "${target}" SOURCES)
    get_property(explicit TARGET "${target}" PROPERTY FTZ_COVERAGE SET)
    get_target_property(coverage "${target}" FTZ_COVERAGE)
    if((explicit AND NOT coverage) OR sources MATCHES "(^|[/;])[^/;]*codegen[^/;]*\\.cc($|;)")
      target_compile_options(${target} PRIVATE
        "$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/clang:>-fno-profile-instr-generate"
        "$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/clang:>-fno-coverage-mapping")
    endif()
  endforeach()
  get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(child IN LISTS children)
    ftz_coverage_probes("${child}")
  endforeach()
endfunction()
function(ftz_coverage_configure)
  add_executable(ftz_coverage_host "${CMAKE_CURRENT_SOURCE_DIR}/cmake/coverage_host.cc")
  target_link_libraries(ftz_coverage_host PRIVATE native::minimal)
  file(MAKE_DIRECTORY "${ftz_coverage_dir}/report")
  add_test(NAME ftz.coverage.host COMMAND ftz_coverage_host "${ftz_coverage_dir}/report/host.json")
  ftz_coverage_probes("${CMAKE_CURRENT_SOURCE_DIR}")
  get_property(binaries GLOBAL PROPERTY FTZ_COVERAGE_BINARIES)
  file(GENERATE OUTPUT "${ftz_coverage_dir}/binaries-$<CONFIG>.txt"
    CONTENT "$<JOIN:${binaries},\n>\n")
endfunction()
cmake_language(DEFER CALL ftz_coverage_configure)

# Run after CTest: reporting never rebuilds or re-executes the test suite.
add_custom_target(ftz_coverage
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/coverage_report.py"
    --source "${CMAKE_CURRENT_SOURCE_DIR}"
    --coverage "${ftz_coverage_dir}" --binaries "${ftz_coverage_dir}/binaries-$<CONFIG>.txt"
    --llvm "${ftz_coverage_llvm_bin}" --grcov "${FTZ_GRCOV}"
  COMMENT "Generating FTZ runtime coverage with grcov"
  VERBATIM)
