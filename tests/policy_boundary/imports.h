#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>
import ftz;
import simd;
#if REVIEW_AVX512
using arch=simd::avx512;
#else
using arch=simd::avx2;
#endif
import simd.wide;
