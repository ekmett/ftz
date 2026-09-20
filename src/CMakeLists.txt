# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
add_library(ftz STATIC)
add_library(ftz::ftz ALIAS ftz)
ftz_host_settings(ftz)
set_target_properties(ftz PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_SCAN_FOR_MODULES ON)
target_sources(ftz PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES
  BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/cxx/modules"
  FILES cxx/modules/ftz.controls.ccm cxx/modules/ftz.ccm)
target_include_directories(ftz PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cxx>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shared>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cxx>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/shared>")
target_link_libraries(ftz PUBLIC native::native native::headers)
if(FTZ_ENABLE_PCH)
  target_precompile_headers(ftz PRIVATE <array> <bit> <cmath> <concepts> <cstdint> <type_traits> <utility> <native/attributes.h>)
endif()
file(GLOB_RECURSE ftz_cxx_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cxx/ftz/*.h")
target_sources(ftz PUBLIC
  FILE_SET cxx_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/cxx" FILES ${ftz_cxx_headers}
  FILE_SET shared_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/shared" FILES ${ftz_shared_headers})
install(TARGETS ftz EXPORT ftzHostTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  FILE_SET cxx_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/cxx"
  FILE_SET shared_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/shared"
  FILE_SET cxx_modules DESTINATION "${CMAKE_INSTALL_LIBDIR}/ftz/modules")
install(EXPORT ftzHostTargets NAMESPACE ftz:: CXX_MODULES_DIRECTORY cxx-modules
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ftz")
export(EXPORT ftzHostTargets NAMESPACE ftz:: CXX_MODULES_DIRECTORY cxx-modules
  FILE "${PROJECT_BINARY_DIR}/ftzHostTargets.cmake")
