#pragma once
#include <array>
#include <bit>
#include <concepts>
#include <initializer_list>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
import ftz;
import simd;
#if FTZ_TEST_PROFILE == 512
#define FTZ_TEST_ARCH simd::avx512
#elif FTZ_TEST_PROFILE == 256
#define FTZ_TEST_ARCH simd::avx2
#elif FTZ_TEST_PROFILE == 128
#define FTZ_TEST_ARCH simd::neon
#else
#error Select a test SIMD profile
#endif
import simd.wide;

// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
