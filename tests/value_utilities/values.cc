// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstdio>
#include <cstdlib>
#include "../core_regression/support/imports.h"
template<class A,class B> concept can_copy_sign=requires(A a,B b){copysign(a,b);};
using manual=::native::simd<ftz::m32,4,FTZ_TEST_ARCH>;
using hardware=::native::simd<ftz::h32,4,FTZ_TEST_ARCH>;
static_assert(!can_copy_sign<manual,hardware> && !can_copy_sign<hardware,manual>);
namespace {
  unsigned checks=0;
  constexpr std::array words{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x807fffffu,
    0x00800000u,0x80800000u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,
    0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u,
    0x7fffffffu,0xffffffffu,0x7fc12345u,0xffc12345u};
  void check(bool value){++checks;if(!value){std::fprintf(stderr,"value utility check failed at%u\n",checks);std::abort();}}
  template<class V> void mask(typename V::mask value,std::array<bool,V::lanes> const & expected) {
    if constexpr(std::same_as<typename V::mask,bool>)check(value==expected[0]);
    else {
      auto bits=::native::mask_bits<std::uint32_t>(value);
      std::array<std::uint32_t,V::lanes> output;bits.store(output.data());
      bool some=false,every=true;
      for(std::size_t i=0;i<V::lanes;++i){check(output[i]==(expected[i]?0xffffffffu:0u));some|=expected[i];every&=expected[i];}
      check(any(value)==some);check(all(value)==every);
    }
  }
  template<class F,std::size_t N> void vectors() {
    using V=::native::simd<F,N,FTZ_TEST_ARCH>;
    using Raw=::native::simd<float,N,FTZ_TEST_ARCH>;
    static_assert(sizeof(V)==sizeof(Raw) && alignof(V)==alignof(Raw));
    static_assert(std::is_trivially_copyable_v<V>);
    static_assert(std::same_as<decltype(isnan(V{})),typename V::mask>);
    static_assert(std::same_as<decltype(isinf(V{})),typename V::mask>);
    static_assert(std::same_as<decltype(isfinite(V{})),typename V::mask>);
    static_assert(std::same_as<decltype(signbit(V{})),typename V::mask>);
    static_assert(std::same_as<decltype(copysign(V{},V{})),V>);
    static_assert(noexcept(copysign(V{},V{})) && noexcept(isnan(V{})));
    for(std::size_t offset=0;offset<words.size();++offset) {
      std::array<F,N> source;std::array<bool,N> nan,inf,finite,sign;
      for(std::size_t i=0;i<N;++i) {
        source[i]=F::from_bits(words[(offset+i)%words.size()]);
        auto word=source[i].to_bits(),magnitude=word&0x7fffffffu;
        nan[i]=magnitude>0x7f800000u;inf[i]=magnitude==0x7f800000u;
        finite[i]=magnitude<0x7f800000u;sign[i]=(word&0x80000000u)!=0;
      }
      V value=native::load_simd<V>(source.data());
      mask<V>(isnan(value),nan);mask<V>(isinf(value),inf);
      mask<V>(isfinite(value),finite);mask<V>(signbit(value),sign);
      for(auto sign_word:words) {
        auto sign_value=F::from_bits(sign_word);
        V result=copysign(value,V(sign_value));
        std::array<F,N> output;native::store_simd(output.data(),result);
        for(std::size_t i=0;i<N;++i)
          check(output[i].to_bits()==((source[i].to_bits()&0x7fffffffu)|(sign_word&0x80000000u)));
      }
    }
  }
  template<class F> void run() {
    vectors<F,1>();vectors<F,2>();vectors<F,3>();vectors<F,4>();
#if FTZ_TEST_PROFILE != 128
    vectors<F,8>();
#endif
#if FTZ_TEST_PROFILE == 512
    vectors<F,16>();
#endif
  }
}
int main() {
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
   auto before=ftz::read_native_fp_state();run<ftz::m32>();check(ftz::read_native_fp_state()==before);}
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
   auto before=ftz::read_native_fp_state();run<ftz::m32>();run<ftz::h32>();check(ftz::read_native_fp_state()==before);}
  std::printf("native classification/copysign checks: %u\n",checks);
}
