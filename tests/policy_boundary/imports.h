#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>
import ftz;
import native;
import native.math;
#if REVIEW_AVX512
constexpr auto arch=::native::avx512;
#else
constexpr auto arch=::native::avx2;
#endif
import native.wide;
