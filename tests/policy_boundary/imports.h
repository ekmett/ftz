#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>
import ftz;
#if REVIEW_AVX512
import simd.avx512;
using arch=simd::avx512;
#else
import simd.avx2;
using arch=simd::avx2;
#endif
import simd.wide;
