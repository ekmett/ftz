// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "../core_regression/support/imports.h"
namespace {
  constexpr std::size_t lanes=FTZ_TEST_PROFILE/32;
  template<class F> void pair(float *sine,float *cosine,float const *input) {
    using V=::native::simd<F,lanes,FTZ_TEST_ARCH>;
    using R=::native::simd<float,lanes,FTZ_TEST_ARCH>;
    auto result=sincos(V::unsafe_from_float32(R::loadu(input)));
    result.first.to_native().storeu(sine);result.second.to_native().storeu(cosine);
  }
}
extern "C" [[gnu::noinline]] void ftz_sincos_manual(float *s,float *c,float const *input){pair<ftz::m32>(s,c,input);}
extern "C" [[gnu::noinline]] void ftz_sincos_hardware(float *s,float *c,float const *input){pair<ftz::h32>(s,c,input);}
