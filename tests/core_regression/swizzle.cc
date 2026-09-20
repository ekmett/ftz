// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "math_contract.h"
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <type_traits>
#include <utility>
#include "support/imports.h"

namespace {
  using F=ftz::ftz32;
  template<std::size_t N> using V=::native::simd<F,N,FTZ_TEST_ARCH>;
  void require(bool condition,char const * message) {
    if(!condition) { std::fprintf(stderr,"%s\n",message);std::abort(); }
  }
  template<std::size_t N> auto words(V<N> const & value) {
    std::array<F,N> lanes{};
    ::native::store_simd(lanes.data(),value);
    std::array<std::uint32_t,N> result{};
    for(std::size_t i=0;i<N;++i)result[i]=lanes[i].to_bits();
    return result;
  }
  template<std::size_t N> auto unsafe(std::array<std::uint32_t,N> const & input) {
    std::array<F,N> lanes{};
    for(std::size_t i=0;i<N;++i)
      lanes[i]=F::unsafe_from_float32(std::bit_cast<float>(input[i]));
    return ::native::load_simd<V<N>>(lanes);
  }
  template<std::size_t N> void exact(V<N> const & value,
      std::array<std::uint32_t,N> const & expected,char const * message) {
    require(words(value)==expected,message);
  }
  template<class T> concept nested_write=requires(T & value) { value.xyz.x=F{}; };
  template<class T> concept repeated_write=requires(T & value) { value.xx=value.xy; };
  template<class T> concept const_write=requires(T const & value) { value.xy=value.yx; };
  template<class T> concept rvalue_write=requires(T value) { std::move(value).xy=value.yx; };
  template<std::size_t N> void shape() {
    using T=V<N>;
    static_assert(sizeof(T)==16 && alignof(T)==16 && std::is_trivially_copyable_v<T>);
    static_assert(std::same_as<typename T::value_type,F>);
    static_assert(std::same_as<typename T::mask,typename ::native::simd<float,N,FTZ_TEST_ARCH>::mask>);
    static_assert(std::same_as<decltype(std::declval<T &>().xy),V<2>>);
    static_assert(std::same_as<decltype((std::declval<T &>().xy)),V<2>>);
    static_assert(std::same_as<decltype(std::declval<T &>().x),F>);
    static_assert(std::same_as<decltype(std::declval<T &>().xy=std::declval<T &>().yx),V<2>>);
    static_assert(!nested_write<T> && !repeated_write<T> && !const_write<T> && !rvalue_write<T>);
    exact(T{},std::array<std::uint32_t,N>{},"value initialization");
    if constexpr(N==3)static_assert(std::same_as<decltype(std::declval<T &>().xyz),V<3>>);
  }
  void properties() {
    // Deliberately unsafe words are used only for data movement, never arithmetic.
    // Typed swizzles must not re-import these through the normalizing float path.
    constexpr std::array<std::uint32_t,10> bank{0,0x80000000u,1,0x807fffffu,
      0x7fa12345u,0xffc45678u,0x00800000u,0x80800000u,0x3f800001u,0xbf800001u};
    for(std::size_t i=0;i<bank.size();++i) {
      std::array a{bank[i],bank[(i+1)%bank.size()],bank[(i+2)%bank.size()]};
      std::array b{bank[(i+3)%bank.size()],bank[(i+4)%bank.size()]};
      auto triple=unsafe(a);auto pair=unsafe(b);
      auto snapshot=triple.xyz;
      auto && detached=triple.xy;
      triple.xyz=triple.zyx;
      exact(triple,std::array{a[2],a[1],a[0]},"overlapping typed scatter");
      exact(snapshot,a,"owning xyz snapshot");
      exact(detached,std::array{a[0],a[1]},"detached xy reference snapshot");
      triple.xy=pair.xy;
      exact(triple,std::array{b[0],b[1],a[0]},"typed scatter keeps untouched lane");
      pair.xy=pair.yx;
      exact(pair,std::array{b[1],b[0]},"two-lane overlap");
      auto result=pair.xy=triple.xy;
      exact(result,b,"assignment returns owning typed value");
      exact(pair,b,"same-name typed assignment");
      triple.x=F::unsafe_from_float32(std::bit_cast<float>(a[1]));
      exact(triple,std::array{a[1],b[1],a[0]},"scalar property keeps unsafe word");
      auto repeated=triple.xxxx;
      exact(repeated,std::array{a[1],a[1],a[1],a[1]},"typed repeated read");
      V<3> const immutable=snapshot;
      exact(immutable.zyx,std::array{a[2],a[1],a[0]},"const owning read");
      exact(unsafe(a).xyz,a,"temporary owning read");
    }
  }
  template<std::size_t N> void active_lanes(V<N> const & short_value,V<4> const & full,char const * operation) {
    auto a=words(short_value);auto b=words(full);
    for(std::size_t lane=0;lane<N;++lane)
      if(!ftz::math_test::equivalent_fp32(a[lane],b[lane])) {
        std::fprintf(stderr,"%s N=%zu lane=%zu got=%08x expected=%08x\n",operation,N,lane,a[lane],b[lane]);
        std::abort();
      }
  }
  template<std::size_t N> void arithmetic() {
    constexpr std::array<std::uint32_t,24> bank{0,0x80000000u,1,0x807fffffu,
      0x00800000u,0x80800000u,0x00800001u,0x3f7fffffu,0x20000001u,0x1ffffffeu,
      0x3f800000u,0x3f800001u,0x3f7ffffeu,0xbf800000u,0x7f7fffffu,0xff7fffffu,
      0x7f800000u,0xff800000u,0x7fc00000u,0x7f800001u,0xffa12345u,
      0x33000000u,0x39800000u,0x3f000000u};
    for(std::size_t i=0;i<bank.size();++i)for(std::size_t j=0;j<bank.size();++j) {
      std::array<F,4> a{},b{},c{};
      for(std::size_t lane=0;lane<4;++lane) {
        a[lane]=F::from_bits(bank[(i+lane)%bank.size()]);
        b[lane]=F::from_bits(bank[(j+lane*3)%bank.size()]);
        c[lane]=F::from_bits(bank[(i+j+lane*7)%bank.size()]);
      }
      auto sa=::native::load_simd<V<N>>(a.data()),sb=::native::load_simd<V<N>>(b.data()),sc=::native::load_simd<V<N>>(c.data());
      auto fa=::native::load_simd<V<4>>(a),fb=::native::load_simd<V<4>>(b),fc=::native::load_simd<V<4>>(c);
      active_lanes(sa+sb,fa+fb,"add");active_lanes(sa-sb,fa-fb,"sub");
      active_lanes(sa*sb,fa*fb,"mul");active_lanes(sa/sb,fa/fb,"div");
      active_lanes(fma(sa,sb,sc),fma(fa,fb,fc),"fma");
      active_lanes(sqrt(sa),sqrt(fa),"sqrt");
    }
  }
}
int main(int argc,char ** argv) {
  require(argc==2,"expected gradual or flush");
  std::string_view mode=argv[1];
  require(mode=="gradual" || mode=="flush","invalid FP mode");
  ftz::native_fp32_scope scope(mode=="flush"?ftz::native_fp32_mode::flush:ftz::native_fp32_mode::gradual);
  require(scope.controls_match(),"FP controls not established");
  shape<2>();shape<3>();properties();arithmetic<2>();arithmetic<3>();
  require(scope.controls_match(),"FP controls changed");
  std::printf("FTZ short vectors passed: profile=%d, mode=%s, typed swizzle words exact, active arithmetic/FMA matches four lanes\n",FTZ_TEST_PROFILE,argv[1]);
}
