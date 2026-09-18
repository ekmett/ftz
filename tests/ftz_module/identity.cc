// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <type_traits>
#include <utility>
import ftz.controls;
import ftz;
import simd.wide;
import simd;

using A = simd::vec<ftz::ftz32,8,simd::avx2>;
using B = simd::vec<ftz::ftz32,16,simd::avx512>;
// Redundant compiler-implied features must not create another FTZ vector type.
constexpr auto canonical_avx2 = simd::avx2 & simd::feature::avx;
constexpr auto canonical_avx512 = simd::avx512 & simd::feature::avx512f;
static_assert(std::same_as<A,simd::vec<ftz::ftz32,8,canonical_avx2>>);
static_assert(std::same_as<B,simd::vec<ftz::ftz32,16,canonical_avx512>>);
static_assert(std::same_as<typename A::value_type,ftz::ftz32>);
static_assert(std::same_as<typename B::value_type,ftz::ftz32>);
static_assert(!std::same_as<simd::vec<ftz::ftz32,4,simd::avx2>,simd::vec<ftz::ftz32,4,simd::avx512>>);
static_assert(std::same_as<decltype(ftz::probe_ftz32_cpu()),ftz::ftz32_cpu_admission>);
static_assert(std::same_as<decltype(ftz::sincos(ftz::ftz32{})),std::pair<ftz::ftz32,ftz::ftz32>>);
static_assert(std::same_as<decltype(exp(std::declval<simd::wide<A,12>>())),simd::wide<A,12>>);
static_assert(std::same_as<decltype(exp(std::declval<simd::wide<B,6>>())),simd::wide<B,6>>);

static_assert(std::same_as<typename A::mask,decltype(std::declval<A>() < std::declval<A>())>);
static_assert(std::same_as<typename B::mask,decltype(std::declval<B>() < std::declval<B>())>);
static_assert(std::same_as<typename A::mask,typename simd::vec<float,8,simd::avx2>::mask>);
static_assert(std::same_as<typename B::mask,typename simd::vec<float,16,simd::avx512>::mask>);
static_assert(std::same_as<decltype(simd::vec<ftz::ftz32,1,simd::avx2>{ftz::ftz32{}}),simd::vec<ftz::ftz32,1,simd::avx2>>);
static_assert(std::same_as<decltype(simd::vec<ftz::ftz32,1,simd::avx512>{ftz::ftz32{}}),simd::vec<ftz::ftz32,1,simd::avx512>>);
