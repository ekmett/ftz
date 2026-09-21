// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <array>
#include <bit>
#include <cstdint>
#include <utility>
#include <ftz/math.h>
import native;
import native.math;
#if FTZ_TEST_PROFILE == 512
constexpr auto Arch=native::avx512;
#elif FTZ_TEST_PROFILE == 128
constexpr auto Arch=native::neon;
#else
constexpr auto Arch=native::avx2;
#endif
using V=::native::simd<float,4,Arch>;
namespace ftz_native=ftz::detail::native;
#define ONE(Name,Hardware,Function) extern "C" [[gnu::noinline,gnu::flatten]] void Name(float *o,float const*i){ftz_native::Function<Hardware>(V::loadu(i)).storeu(o);}
ONE(manual_reduced_sin,false,sin_reduced_ftz)
ONE(manual_reduced_cos,false,cos_reduced_ftz)
ONE(hardware_reduced_sin,true,sin_reduced_ftz)
ONE(hardware_reduced_cos,true,cos_reduced_ftz)
ONE(manual_bounded_sin,false,sin_ftz)
ONE(manual_bounded_cos,false,cos_ftz)
ONE(hardware_bounded_sin,true,sin_ftz)
ONE(hardware_bounded_cos,true,cos_ftz)
#undef ONE
#define PAIR(Name,Hardware,Function) extern "C" [[gnu::noinline,gnu::flatten]] void Name(float *s,float*c,float const*i){auto pair=ftz_native::Function<Hardware>(V::loadu(i));pair.first.storeu(s);pair.second.storeu(c);}
PAIR(manual_reduced_pair,false,sincos_reduced_ftz)
PAIR(hardware_reduced_pair,true,sincos_reduced_ftz)
PAIR(manual_bounded_pair,false,sincos_ftz)
PAIR(hardware_bounded_pair,true,sincos_ftz)
#undef PAIR
