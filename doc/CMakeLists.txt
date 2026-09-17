# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0

find_package(Doxygen 1.18 REQUIRED)
set(DOXYGEN_GENERATE_HTML YES)
set(DOXYGEN_GENERATE_LATEX NO)
set(DOXYGEN_GENERATE_XML YES)
set(DOXYGEN_RECURSIVE YES)
set(DOXYGEN_FILE_PATTERNS *.h *.ccm *.md)
set(DOXYGEN_EXTENSION_MAPPING "ccm=C++")
set(DOXYGEN_EXTRACT_ALL NO)
set(DOXYGEN_EXTRACT_PRIVATE NO)
set(DOXYGEN_EXTRACT_STATIC NO)
set(DOXYGEN_HIDE_UNDOC_MEMBERS YES)
set(DOXYGEN_HIDE_UNDOC_CLASSES YES)
set(DOXYGEN_EXCLUDE_SYMBOLS "*::detail::*")
set(DOXYGEN_EXAMPLE_PATH "${PROJECT_SOURCE_DIR}/tests/api")
set(DOXYGEN_EXAMPLE_RECURSIVE YES)
set(DOXYGEN_ENABLE_PREPROCESSING YES)
set(DOXYGEN_MACRO_EXPANSION YES)
set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
set(DOXYGEN_PREDEFINED
  "__cplusplus=202600L" "SIMD_PROFILE=512" "SIMD_BACKEND:=avx512_backend" "SIMD_ARCH:=::simd::avx512"
  "SIMD_BACKEND_NAMESPACE:=simd::detail::avx512_backend"
  "SIMD_HAS_AVX2:=1" "SIMD_HAS_AVX512F:=1" "SIMD_HAS_AVX512DQ:=1"
  "SIMD_HAS_AVX512BW:=1" "SIMD_HAS_AVX512VL:=1" "SIMD_HAS_ARM_NEON:=0"
  "simd_inline:=inline" "simd_flatten:=" "simd_noinline:="
  "simd_const:=" "simd_pure:=" "simd_nodiscard:=" "simd_lifetimebound:="
  "simd_artificial:=" "simd_noescape:=" "simd_reinitializes:="
  "simd_empty_bases:=" "simd_restrict:=" "simd_hidden:="
  "simd_nonnull(x):=" "simd_null_terminated_string_arg(x):=")
set(DOXYGEN_ALIASES "license=@code{.spdx}" "endlicense=@endcode"
  "CPUID=@c CPUID" "PAUSE=@c PAUSE" "MONITOR=@c MONITOR" "MWAIT=@c MWAIT"
  "UMONITOR=@c UMONITOR" "UMWAIT=@c UMWAIT")
# Keep relative Markdown links intact in a build-tree rendering copy. The
# original pages, including the license, remain byte-for-byte untouched.
# Paired backticks in the historical license denote quotation marks; escape
# them for Doxygen without a shell filter (whose quoting differs on Windows).
set(docs_markdown_root "${CMAKE_CURRENT_BINARY_DIR}/markdown")
set(docs_pages README.md LICENSE.md CODE_OF_CONDUCT.md src/README.md tests/api/README.md
  tests/shader_add/README.md tests/rounding/README.md tests/shaders/README.md)
file(GLOB_RECURSE docs_guides CONFIGURE_DEPENDS RELATIVE "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/docs/*.md")
list(APPEND docs_pages ${docs_guides})
set(docs_markdown_inputs)
foreach(page IN LISTS docs_pages)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/${page}")
  file(READ "${PROJECT_SOURCE_DIR}/${page}" docs_markdown)
  if(page STREQUAL "LICENSE.md")
    string(REPLACE "``" "\\`\\`" docs_markdown "${docs_markdown}")
  endif()
  get_filename_component(page_directory "${docs_markdown_root}/${page}" DIRECTORY)
  file(MAKE_DIRECTORY "${page_directory}")
  file(WRITE "${docs_markdown_root}/${page}" "${docs_markdown}")
  list(APPEND docs_markdown_inputs "${docs_markdown_root}/${page}")
endforeach()
# Render nested README files as pages, not implicit directory descriptions.
# Directory descriptions otherwise leak build paths into link tooltips.
set(DOXYGEN_IMPLICIT_DIR_DOCS NO)
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${docs_markdown_root}/README.md")
set(DOXYGEN_STRIP_FROM_PATH "${docs_markdown_root}" "${PROJECT_SOURCE_DIR}")
set(DOXYGEN_HAVE_DOT NO)
set(DOXYGEN_GENERATE_TREEVIEW YES)
set(DOXYGEN_HTML_COLORSTYLE AUTO_LIGHT)
set(DOXYGEN_OBFUSCATE_EMAILS NO)
set(DOXYGEN_SOURCE_BROWSER YES)
set(DOXYGEN_INLINE_SOURCES NO)
set(DOXYGEN_FULL_PATH_NAMES YES)
set(DOXYGEN_QUIET YES)
set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
set(DOXYGEN_WARN_IF_DOC_ERROR YES)
set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
set(DOXYGEN_WARN_IF_UNDOC_ENUM_VAL NO)
set(DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/docs")
set(DOXYGEN_WARN_LOGFILE "${PROJECT_BINARY_DIR}/docs/warnings.log")
set(DOXYGEN_PROJECT_BRIEF "Reproducible binary32 arithmetic for C++26 and HLSL")
doxygen_add_docs(ftz_host_docs
  "${PROJECT_SOURCE_DIR}/src/cxx"
  "${PROJECT_SOURCE_DIR}/src/shared"
  ${docs_markdown_inputs}
  WORKING_DIRECTORY "${docs_markdown_root}"
  COMMENT "Build ftz host API documentation and examples")

# Shader and host types deliberately share names. Give the shader interface its
# own reference so the two languages never collapse into one misleading type.
set(DOXYGEN_PROJECT_NAME "ftz shaders")
set(DOXYGEN_PROJECT_BRIEF "HLSL 2021 interface")
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "")
set(DOXYGEN_PREDEFINED "__HLSL_VERSION=2021" "FTZ_FP32_HARDWARE_FTZ=0"
  "FTZ_SHADER_INT64=1" "simd_inline:=inline" "simd_const:=" "simd_pure:=")
set(DOXYGEN_HTML_OUTPUT "html/shaders")
set(DOXYGEN_XML_OUTPUT "xml-shaders")
set(DOXYGEN_WARN_LOGFILE "${PROJECT_BINARY_DIR}/docs/shader-warnings.log")
doxygen_add_docs(ftz_shader_docs "${PROJECT_SOURCE_DIR}/src/hlsl"
  COMMENT "Build ftz HLSL API documentation")
add_custom_target(ftz_docs DEPENDS ftz_host_docs ftz_shader_docs)
