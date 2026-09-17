# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
cmake_minimum_required(VERSION 4.4)
project(ftz_shader_consumer LANGUAGES NONE)
find_package(ftz CONFIG REQUIRED COMPONENTS hlsl)
if(TARGET simd::simd OR TARGET ftz::ftz)
  message(FATAL_ERROR "A shader consumer must not load the host library targets.")
endif()
find_program(DXC dxc REQUIRED)
find_program(SPIRV_VAL spirv-val REQUIRED)
find_program(SPIRV_CROSS spirv-cross REQUIRED)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include-roots.txt"
  CONTENT "$<JOIN:$<TARGET_GENEX_EVAL:ftz::hlsl,$<TARGET_PROPERTY:ftz::hlsl,INTERFACE_INCLUDE_DIRECTORIES>>,\n>\n")
# A custom DXC command does not inherit header dependencies from linking a
# header target. Use the actual exported file sets, including the attributes.
set(shader_dependencies)
foreach(provider IN ITEMS ftz::hlsl simd::headers)
  get_target_property(header_sets ${provider} INTERFACE_HEADER_SETS)
  foreach(header_set IN LISTS header_sets)
    get_target_property(headers ${provider} HEADER_SET_${header_set})
    list(APPEND shader_dependencies ${headers})
  endforeach()
endforeach()
list(REMOVE_DUPLICATES shader_dependencies)
set(outputs)
foreach(policy RANGE 0 1)
  foreach(int64 RANGE 0 1)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/consumer-${policy}-${int64}")
    add_custom_command(OUTPUT "${output}.spv" "${output}.metal"
      COMMAND "${DXC}" -HV 2021 -T cs_6_6 -E consumer -spirv -fspv-target-env=vulkan1.1
        -D "FTZ_FP32_HARDWARE_FTZ=${policy}" -D "FTZ_SHADER_INT64=${int64}"
        "-I$<JOIN:$<TARGET_GENEX_EVAL:ftz::hlsl,$<TARGET_PROPERTY:ftz::hlsl,INTERFACE_INCLUDE_DIRECTORIES>>,;-I>"
        -Fo "${output}.spv" "${CMAKE_CURRENT_SOURCE_DIR}/consumer.hlsl"
      COMMAND "${SPIRV_VAL}" --target-env vulkan1.1 "${output}.spv"
      COMMAND "${SPIRV_CROSS}" "${output}.spv" --msl --msl-version 23000 --output "${output}.metal"
      DEPENDS consumer.hlsl ${shader_dependencies} VERBATIM COMMAND_EXPAND_LISTS)
    list(APPEND outputs "${output}.spv" "${output}.metal")
  endforeach()
endforeach()
add_custom_target(shaders ALL DEPENDS ${outputs})
