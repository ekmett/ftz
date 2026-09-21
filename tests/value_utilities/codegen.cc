// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "../core_regression/support/imports.h"
using V=::native::simd<ftz::m32,4,FTZ_TEST_ARCH>;
using R=::native::simd<float,4,FTZ_TEST_ARCH>;
#define CLASSIFY(Name) extern "C" [[gnu::noinline,gnu::flatten]] void check_##Name(std::uint32_t *o,float const *p){::native::mask_bits<std::uint32_t>(Name(V::unsafe_from_float32(R::loadu(p)))).store(o);}
CLASSIFY(isnan) CLASSIFY(isinf) CLASSIFY(isfinite) CLASSIFY(signbit)
#undef CLASSIFY
extern "C" [[gnu::noinline,gnu::flatten]] void copy_sign(float *o,float const *m,float const *s){copysign(V::unsafe_from_float32(R::loadu(m)),V::unsafe_from_float32(R::loadu(s))).to_native().storeu(o);}
