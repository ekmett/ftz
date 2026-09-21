// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "../core_regression/support/imports.h"
using R=::native::simd<float,4,FTZ_TEST_ARCH>;
template<class F>using V=::native::simd<F,4,FTZ_TEST_ARCH>;
#define ONE(Name,F,Op) extern "C" [[gnu::noinline,gnu::flatten]] void Name(float *o,float const*i){Op(V<F>::unsafe_from_float32(R::loadu(i))).to_native().storeu(o);}
ONE(manual_log,ftz::m32,log) ONE(hardware_log,ftz::h32,log)
ONE(manual_log1p,ftz::m32,log1p) ONE(hardware_log1p,ftz::h32,log1p)
#undef ONE
#define WIDE(Name,F,Op) extern "C" [[gnu::noinline,gnu::flatten]] void Name(float *o,float const*i){::native::wide a{V<F>::unsafe_from_float32(R::loadu(i)),V<F>::unsafe_from_float32(R::loadu(i+4)),V<F>::unsafe_from_float32(R::loadu(i+8))};auto b=Op(a);b.registers[0].to_native().storeu(o);b.registers[1].to_native().storeu(o+4);b.registers[2].to_native().storeu(o+8);}
WIDE(manual_wide_log,ftz::m32,log) WIDE(hardware_wide_log,ftz::h32,log)
WIDE(manual_wide_log1p,ftz::m32,log1p) WIDE(hardware_wide_log1p,ftz::h32,log1p)
#undef WIDE
