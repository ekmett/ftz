// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "../core_regression/support/imports.h"
using R=::native::simd<float,4,FTZ_TEST_ARCH>;
template<class F>using V=::native::simd<F,4,FTZ_TEST_ARCH>;
#define ONE(Name,F) extern "C" [[gnu::noinline,gnu::flatten]] void Name(float *o,float const*y,float const*x){atan2(V<F>::unsafe_from_float32(R::loadu(y)),V<F>::unsafe_from_float32(R::loadu(x))).to_native().storeu(o);}
ONE(manual_atan2,ftz::m32) ONE(hardware_atan2,ftz::h32)
#undef ONE
#define ARRAY(Name,F) extern "C" [[gnu::noinline,gnu::flatten]] void Name(float *o,float const*y,float const*x){std::array a{V<F>::unsafe_from_float32(R::loadu(y)),V<F>::unsafe_from_float32(R::loadu(y+4)),V<F>::unsafe_from_float32(R::loadu(y+8))};std::array b{V<F>::unsafe_from_float32(R::loadu(x)),V<F>::unsafe_from_float32(R::loadu(x+4)),V<F>::unsafe_from_float32(R::loadu(x+8))};auto c=atan2(a,b);c[0].to_native().storeu(o);c[1].to_native().storeu(o+4);c[2].to_native().storeu(o+8);}
ARRAY(manual_array_atan2,ftz::m32) ARRAY(hardware_array_atan2,ftz::h32)
#undef ARRAY
