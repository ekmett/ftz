// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <type_traits>
#include <utility>
import ftz.controls;
import ftz;
import native.wide;
import native;
import native.math;

using A = ::native::simd<ftz::ftz32,8,native::avx2>;
using B = ::native::simd<ftz::ftz32,16,native::avx512>;
// Redundant compiler-implied features must not create another FTZ vector type.
constexpr auto canonical_avx2 = native::avx2 & native::x86_feature::avx;
constexpr auto canonical_avx512 = native::avx512 & native::x86_feature::avx512f;
static_assert(std::same_as<A,::native::simd<ftz::ftz32,8,canonical_avx2>>);
static_assert(std::same_as<B,::native::simd<ftz::ftz32,16,canonical_avx512>>);
static_assert(std::same_as<typename A::value_type,ftz::ftz32>);
static_assert(std::same_as<typename B::value_type,ftz::ftz32>);
static_assert(!std::same_as<::native::simd<ftz::ftz32,4,native::avx2>,::native::simd<ftz::ftz32,4,native::avx512>>);
static_assert(std::same_as<decltype(ftz::probe_ftz32_cpu()),ftz::ftz32_cpu_admission>);
static_assert(std::same_as<decltype(ftz::sincos(ftz::ftz32{})),std::pair<ftz::ftz32,ftz::ftz32>>);
static_assert(std::same_as<decltype(exp(std::declval<::native::wide<A,12>>())),::native::wide<A,12>>);
static_assert(std::same_as<decltype(exp(std::declval<::native::wide<B,6>>())),::native::wide<B,6>>);

static_assert(std::same_as<typename A::mask,decltype(std::declval<A>() < std::declval<A>())>);
static_assert(std::same_as<typename B::mask,decltype(std::declval<B>() < std::declval<B>())>);
static_assert(std::same_as<typename A::mask,typename ::native::simd<float,8,native::avx2>::mask>);
static_assert(std::same_as<typename B::mask,typename ::native::simd<float,16,native::avx512>::mask>);
static_assert(std::same_as<decltype(::native::simd<ftz::ftz32,1,native::avx2>{ftz::ftz32{}}),::native::simd<ftz::ftz32,1,native::avx2>>);
static_assert(std::same_as<decltype(::native::simd<ftz::ftz32,1,native::avx512>{ftz::ftz32{}}),::native::simd<ftz::ftz32,1,native::avx512>>);
