// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "math_contract.h"
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>
#include "support/imports.h"

namespace {
  using M=ftz::m32;using H=ftz::h32;
  template<class F,std::size_t N> using V=::native::simd<F,N,FTZ_TEST_ARCH>;
  void require(bool x,char const * message) { if(!x){std::fprintf(stderr,"%s\n",message);std::abort();} }
  template<class A,class B> concept addable=requires(A a,B b){a+b;};
  template<class A,class B> concept subtractable=requires(A a,B b){a-b;};
  template<class A,class B> concept multipliable=requires(A a,B b){a*b;};
  template<class A,class B> concept divisible=requires(A a,B b){a/b;};
  template<class A,class B> concept comparable=requires(A a,B b){a==b;a<b;};
  template<class A,class B> concept ordered=requires(A a,B b){a<=>b;};
  template<class A,class B> concept compound=requires(A a,B b){a+=b;};
  template<class A,class B> concept fused=requires(A a,B b){fma(a,b,a);};
  template<class A,class B> concept angle=requires(A a,B b){atan2(a,b);};
  template<class A,class B> concept signcopy=requires(A a,B b){copysign(a,b);};
  template<class A,class B> concept scaled=requires(A a,B b){scaleb(a,b);};
  template<class A,class B> consteval void separate() {
    static_assert(!addable<A,B> && !subtractable<A,B> && !multipliable<A,B> && !divisible<A,B>);
    static_assert(!comparable<A,B> && !ordered<A,B> && !compound<A,B> && !fused<A,B> && !angle<A,B> && !signcopy<A,B> && !scaled<A,B>);
  }
  static_assert(!std::same_as<M,H> && std::is_trivially_copyable_v<M> && std::is_trivially_copyable_v<H>);
  static_assert(std::convertible_to<float,M> && std::convertible_to<float,H>);
  static_assert(std::convertible_to<M,float> && std::convertible_to<H,float>);
  static_assert(!std::convertible_to<M,H> && !std::convertible_to<H,M>);
  static_assert(std::constructible_from<M,H> && std::constructible_from<H,M>);
  static_assert(sizeof(M)==4 && sizeof(H)==4 && !M::hardware && H::hardware);
  constexpr bool independent=[] {
    separate<M,H>();separate<H,M>();
    separate<V<M,4>,V<H,4>>();separate<V<H,4>,V<M,4>>();
    separate<V<M,4>,H>();separate<H,V<M,4>>();
    separate<::native::wide<M,2>,::native::wide<H,2>>();
    separate<::native::wide<V<M,4>,2>,::native::wide<V<H,4>,2>>();
    return true;
  }();
  static_assert(independent);
  constexpr std::array<std::uint32_t,36> bank{0,0x80000000u,1,0x807fffffu,
    0x00800000u,0x80800000u,0x00800001u,0x3f7fffffu,0x20000001u,0x1ffffffeu,
    0x3f800000u,0x3f800001u,0x3f7ffffeu,0xbf800000u,0x7f7fffffu,0xff7fffffu,
    0x7f800000u,0xff800000u,0x7fc00000u,0x7f800001u,0xffa12345u,
    0x33000000u,0x33000001u,0xb3000001u,0x39800000u,0x3f000000u,
    0x45ffffffu,0x46000000u,0x46000001u,0xc5ffffffu,0xc6000000u,0xc6000001u,
    0x42b17217u,0x42b17218u,0xc2aeac4fu,0xc2aeac50u};
  using packet=std::vector<std::uint32_t>;
  template<class F> void append(packet & p,F value) requires ftz::ftz32_type<F> {p.push_back(value.to_bits());}
  template<class F,std::size_t N> void append(packet & p,V<F,N> const & value) {
    std::array<F,N> lanes{};native::store_simd(lanes.data(),value);
    for(auto lane:lanes)append(p,lane);
  }
  template<class R,std::size_t N> void append(packet & p,::native::wide<R,N> const & value) {
    for(auto const & item:value.registers)append(p,item);
  }
  template<class R> void operations(packet & out,R a,R b,R c) {
    append(out,a+b);append(out,a-b);append(out,a*b);append(out,a/b);append(out,fma(a,b,c));
    append(out,sqrt(a));append(out,sin(a));append(out,cos(a));append(out,exp(a));append(out,expm1(a));
    auto [s,co]=sincos(a);append(out,s);append(out,co);
  }
  template<class F,std::size_t N> void vectors(packet & out) {
    using R=V<F,N>;
    static_assert(std::same_as<typename R::value_type,F>);
    static_assert(std::same_as<typename R::mask,typename V<float,N>::mask>);
    for(std::size_t i=0;i<bank.size();++i) {
      std::array<F,N>a{},b{},c{};
      for(std::size_t lane=0;lane<N;++lane){a[lane]=F::from_bits(bank[(i+lane)%bank.size()]);b[lane]=F::from_bits(bank[(i+lane*3+5)%bank.size()]);c[lane]=F::from_bits(bank[(i+lane*7+11)%bank.size()]);}
      auto x=native::load_simd<R>(a),y=native::load_simd<R>(b),z=native::load_simd<R>(c);
      operations(out,x,y,z);
      operations(out,::native::wide{x,y,z},::native::wide{z,x,y},::native::wide{y,z,x});
    }
    auto empty=::native::wide<R,0>{};append(out,exp(empty));append(out,fma(empty,empty,empty));
  }
  template<class F> void swizzles() {
    using R=V<F,3>;
    std::array<std::uint32_t,3> w{0x80000000u,1u,0x7fa12345u};
    std::array<F,3> a{};for(std::size_t i=0;i<3;++i)a[i]=F::unsafe_from_float32(std::bit_cast<float>(w[i]));
    auto value=native::load_simd<R>(a);auto saved=value.xyz;value.xyz=value.zyx;
    std::array<F,3> actual{};native::store_simd(actual.data(),value);
    for(std::size_t i=0;i<3;++i)require(actual[i].to_bits()==w[2-i],"policy typed swizzle normalized a word");
    native::store_simd(actual.data(),saved);for(std::size_t i=0;i<3;++i)require(actual[i].to_bits()==w[i],"policy owning swizzle changed");
    static_assert(std::same_as<decltype(value.xy),V<F,2>>);
  }
  struct conversion_failure {};
  template<class F> struct scalar_conversion {
    int * conversions;
    int * cleanups;
    bool fail;
    operator F() & noexcept(false) {
      ++*conversions;
      struct guard { int * count; ~guard() { ++*count; } } cleanup{cleanups};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
      if (fail) throw conversion_failure{};
#else
      (void)fail;
#endif
      return F(.25f);
    }
  };
  template<class F> void scalar_adapters() {
    using C=scalar_conversion<F>;
    static_assert(std::same_as<decltype(ftz::sin(std::declval<C &>())),F>);
    static_assert(std::same_as<decltype(ftz::sqrt(std::declval<C &>())),F>);
    static_assert(!noexcept(ftz::sin(std::declval<C &>())));
    static_assert(std::same_as<decltype(ftz::fma(std::declval<C &>(),F(1),2.f)),F>);
    static_assert(!noexcept(ftz::fma(std::declval<C &>(),F(1),2.f)));
    static_assert(!noexcept(ftz::atan2(std::declval<C &>(),F(1))));
    static_assert(!noexcept(ftz::copysign(F(1),std::declval<C &>())));
    int conversions=0,cleanups=0;
    C value{&conversions,&cleanups,false};
    (void)ftz::sin(value);(void)ftz::cos(value);(void)ftz::sqrt(value);
    (void)ftz::fma(value,F(1),2.f);(void)ftz::atan2(value,F(1));(void)ftz::copysign(F(1),value);
    require(conversions==6 && cleanups==6,"scalar policy conversion side effects");
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    value.fail=true;bool caught=false;
    try {(void)ftz::sin(value);} catch(conversion_failure const &) {caught=true;}
    require(caught && conversions==7 && cleanups==7,"scalar policy conversion unwind");
#endif
  }
  template<class F> packet evaluate() {
    packet out;
    for(auto a:bank)for(auto b:bank) {
      auto x=F::from_bits(a),y=F::from_bits(b),z=F::from_bits(0x3f800001u);
      operations(out,x,y,z);append(out,ftz::atan2(x,y));append(out,ftz::tanh(x));append(out,ftz::log(x));append(out,ftz::log1p(x));
      append(out,fma(x,2.f,1.f));append(out,fma(2.f,x,1.f));append(out,fma(2.f,1.f,x));
    }
    vectors<F,1>(out);vectors<F,2>(out);vectors<F,3>(out);vectors<F,4>(out);
#if FTZ_TEST_PROFILE >= 256
    vectors<F,8>(out);
#endif
#if FTZ_TEST_PROFILE == 512
    vectors<F,16>(out);
#endif
    operations(out,::native::wide{F(.25f),F(-1.f),F(1.f)},::native::wide{F(1),F(2),F(3)},::native::wide{F(4),F(5),F(6)});
    swizzles<F>();scalar_adapters<F>();return out;
  }
  void equal(packet const & a,packet const & b) {
    require(a.size()==b.size(),"policy packet shape");
    for(std::size_t i=0;i<a.size();++i)if(!ftz::math_test::equivalent_fp32(a[i],b[i])) {
      std::fprintf(stderr,"policy mismatch word=%zu %08x != %08x\n",i,a[i],b[i]);std::abort();
    }
  }
}
// Ordinary object-code witnesses: admitted hardware addition removes explicit
// output FTZ work; callers provide the scalar type's canonical input contract.
extern "C" [[gnu::noinline]] std::uint32_t manual_add(ftz::m32 a,ftz::m32 b) {return (a+b).to_bits();}
extern "C" [[gnu::noinline]] std::uint32_t hardware_add(ftz::h32 a,ftz::h32 b) {return (a+b).to_bits();}
int main() {
  packet manual_gradual,manual_flush,hardware;
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    require(ftz::probe_ftz32_cpu<M>().admitted(),"manual qualification rejected gradual");
    require(!ftz::probe_ftz32_cpu<H>().admitted(),"hardware qualification accepted gradual");manual_gradual=evaluate<M>();}
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    require(ftz::probe_ftz32_cpu<M>().admitted() && ftz::probe_ftz32_cpu<H>().admitted(),"flush qualification");
    manual_flush=evaluate<M>();hardware=evaluate<H>();}
  equal(manual_gradual,manual_flush);equal(manual_gradual,hardware);
  std::printf("dual-policy types passed: profile=%d words=%zu manual gradual/flush and hardware flush agree\n",FTZ_TEST_PROFILE,hardware.size());
}
