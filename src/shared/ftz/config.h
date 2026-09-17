#pragma once
// The arithmetic policy is fixed for a complete library/shader build.
#ifndef FTZ_FP32_HARDWARE_FTZ
#define FTZ_FP32_HARDWARE_FTZ 0
#endif
#if FTZ_FP32_HARDWARE_FTZ != 0 && FTZ_FP32_HARDWARE_FTZ != 1
#error FTZ_FP32_HARDWARE_FTZ must be 0 or 1
#endif

// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
