// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
// Ordinary no-LTO witnesses: only input/output arrays cross the ABI boundary.
#include "../core_regression/support/imports.h"
namespace {
  template<class F> void native8(float * out,float const * in) {
    using V=::native::simd<F,8,FTZ_TEST_ARCH>;
    using R=::native::simd<float,8,FTZ_TEST_ARCH>;
    ftz::tanh(V::unsafe_from_float32(R::load(in))).to_native().store(out);
  }
  template<class F,std::size_t L,std::size_t N> void batch(float * out,float const * in) {
    using V=::native::simd<F,L,FTZ_TEST_ARCH>;
    using R=::native::simd<float,L,FTZ_TEST_ARCH>;
    std::array<V,N> input{};
    for(std::size_t j=0;j<N;++j) input[j]=V::unsafe_from_float32(R::load(in+L*j));
    auto result=tanh(::native::wide<V,N>{input});
    for(std::size_t j=0;j<N;++j) result.registers[j].to_native().store(out+L*j);
  }
  template<class F> void scalar8(float * out,float const * in) {
    for(std::size_t j=0;j<8;++j) out[j]=ftz::tanh(F::unsafe_from_float32(in[j])).to_float();
  }
}
extern "C" void tanh_m32_native8(float * out,float const * in) { native8<ftz::m32>(out,in); }
extern "C" void tanh_h32_native8(float * out,float const * in) { native8<ftz::h32>(out,in); }
extern "C" void tanh_m32_native96(float * out,float const * in) { batch<ftz::m32,8,12>(out,in); }
extern "C" void tanh_h32_native96(float * out,float const * in) { batch<ftz::h32,8,12>(out,in); }
extern "C" void tanh_m32_scalar8(float * out,float const * in) { scalar8<ftz::m32>(out,in); }
extern "C" void tanh_h32_scalar8(float * out,float const * in) { scalar8<ftz::h32>(out,in); }

extern "C" void tanh_m32_native8x1(float * out,float const * in) { batch<ftz::m32,8,1>(out,in); }
extern "C" void tanh_m32_native8x2(float * out,float const * in) { batch<ftz::m32,8,2>(out,in); }
extern "C" void tanh_m32_native8x3(float * out,float const * in) { batch<ftz::m32,8,3>(out,in); }
extern "C" void tanh_m32_native8x6(float * out,float const * in) { batch<ftz::m32,8,6>(out,in); }
#if FTZ_TEST_PROFILE == 512
extern "C" void tanh_m32_native16x1(float * out,float const * in) { batch<ftz::m32,16,1>(out,in); }
extern "C" void tanh_m32_native16x2(float * out,float const * in) { batch<ftz::m32,16,2>(out,in); }
extern "C" void tanh_m32_native16x3(float * out,float const * in) { batch<ftz::m32,16,3>(out,in); }
extern "C" void tanh_m32_native16x6(float * out,float const * in) { batch<ftz::m32,16,6>(out,in); }
#endif
