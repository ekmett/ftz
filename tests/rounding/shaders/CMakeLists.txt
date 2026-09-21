# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
cmake_minimum_required(VERSION 4.4)
project(ftz_rounding_shaders LANGUAGES NONE)
find_package(ftz CONFIG REQUIRED COMPONENTS hlsl)
find_program(DXC dxc REQUIRED)
find_program(SPIRV_VAL spirv-val REQUIRED)
find_program(SPIRV_CROSS spirv-cross REQUIRED)
set(dependencies)
foreach(provider IN ITEMS ftz::hlsl native::headers)
  get_target_property(sets ${provider} INTERFACE_HEADER_SETS)
  foreach(set IN LISTS sets)
    get_target_property(headers ${provider} HEADER_SET_${set})
    list(APPEND dependencies ${headers})
  endforeach()
endforeach()
set(outputs)
foreach(policy RANGE 0 1)
  set(stem "${CMAKE_CURRENT_BINARY_DIR}/rounding-${policy}")
  add_custom_command(OUTPUT "${stem}.spv" "${stem}.metal" "${stem}.dxil"
    COMMAND "${DXC}" -HV 2021 -T cs_6_6 -E rounding -spirv -fspv-target-env=vulkan1.1
      -D "FTZ_FP32_HARDWARE_FTZ=${policy}"
      "-I$<JOIN:$<TARGET_GENEX_EVAL:ftz::hlsl,$<TARGET_PROPERTY:ftz::hlsl,INTERFACE_INCLUDE_DIRECTORIES>>,;-I>"
      -Fo "${stem}.spv" "${CMAKE_CURRENT_SOURCE_DIR}/rounding.hlsl"
    COMMAND "${SPIRV_VAL}" --target-env vulkan1.1 "${stem}.spv"
    COMMAND "${SPIRV_CROSS}" "${stem}.spv" --msl --msl-version 23000 --output "${stem}.metal"
    COMMAND "${DXC}" -HV 2021 -T cs_6_6 -E rounding -D "FTZ_FP32_HARDWARE_FTZ=${policy}"
      "-I$<JOIN:$<TARGET_GENEX_EVAL:ftz::hlsl,$<TARGET_PROPERTY:ftz::hlsl,INTERFACE_INCLUDE_DIRECTORIES>>,;-I>"
      -Fo "${stem}.dxil" "${CMAKE_CURRENT_SOURCE_DIR}/rounding.hlsl"
    DEPENDS rounding.hlsl ${dependencies} VERBATIM COMMAND_EXPAND_LISTS)
  list(APPEND outputs "${stem}.spv" "${stem}.metal" "${stem}.dxil")
endforeach()
add_custom_target(shaders ALL DEPENDS ${outputs})
