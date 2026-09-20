// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "math_contract.h"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "support/imports.h"

namespace {
  using M=ftz::m32;
  using H=ftz::h32;
  using packet=std::vector<std::uint32_t>;
  template<class F,std::size_t N> using V=simd::vec<F,N,FTZ_TEST_ARCH>;
  template<class T> concept adl_exp=requires(T const & value) { exp(value); };
  template<class T> concept ftz_exp=requires(T const & value) { ftz::exp(value); };

  static_assert(!adl_exp<::wide::tuple<M>> && !adl_exp<::wide::tuple<M,M>>);
  static_assert(!adl_exp<::wide::tuple<float,M>> && !adl_exp<::wide::tuple<M,H>>);
  static_assert(!adl_exp<std::tuple<M>> && !adl_exp<std::tuple<float,M>>);
  static_assert(!ftz_exp<::wide::array<float,3>>);

  void require(bool condition,char const * message) {
    if (!condition) { std::fprintf(stderr,"%s\n",message); std::abort(); }
  }
  constexpr std::uint32_t canonical(std::uint32_t word) {
    return (word & 0x7fffffffu) < 0x00800000u ? word & 0x80000000u : word;
  }
  packet bank() {
    packet words{
      0,0x80000000u,1,0x80000001u,0x007fffffu,0x807fffffu,
      0x00800000u,0x80800000u,0x00800001u,0x80800001u,
      0x3f800000u,0xbf800000u,0x3f000000u,0xbf000000u,
      0x33000000u,0xb3000000u,0x7f7fffffu,0xff7fffffu,
      0x7f800000u,0xff800000u,0x7fc00000u,0xffc12345u,
      0x7f800001u,0xff800001u
    };
    // Both sides of the FTZ cutoff, upper clamp and ordinary underflow cutoff.
    for (auto center : {0xc2aeac4fu,0x42b17218u,0xc2d00000u})
      for (std::uint32_t delta=0;delta<=12;++delta) {
        words.push_back(center-delta); words.push_back(center+delta);
      }
    std::uint32_t state=0x697fad25u;
    for (std::size_t i=0;i<256;++i) {
      state=1664525u*state+1013904223u; words.push_back(state);
    }
    return words;
  }
  template<class R> inline constexpr std::size_t lanes=[] {
    if constexpr (ftz::ftz32_type<R>) return std::size_t(1);
    else return R::lanes;
  }();
  template<class R> auto words(R const & value) {
    std::array<std::uint32_t,lanes<R>> result{};
    if constexpr (ftz::ftz32_type<R>) result[0]=value.to_bits();
    else value.store_bits(result.data());
    return result;
  }
  template<class R> R input(packet const & samples,std::size_t offset) {
    if constexpr (ftz::ftz32_type<R>) return R::from_bits(samples[offset%samples.size()]);
    else {
      std::array<std::uint32_t,lanes<R>> raw{};
      for (std::size_t lane=0;lane<lanes<R>;++lane)
        raw[lane]=samples[(offset+lane*7)%samples.size()];
      return R::load_bits(raw.data());
    }
  }
  template<class F,class R,std::size_t N>
  void arrays(packet & output,packet const & samples) {
    using A=::wide::array<R,N>;
    static_assert(std::same_as<decltype(exp(std::declval<A const &>())),A>);
    static_assert(std::same_as<decltype(ftz::exp(std::declval<A const &>())),A>);
    static_assert(noexcept(exp(std::declval<A const &>())));
    if constexpr (N==0) {
      auto result=exp(A{});
      require(result.values.empty(),"empty FTZ wide exp changed extent");
    } else for (std::size_t offset=0;offset<samples.size();++offset) {
      A value{};
      for (std::size_t reg=0;reg<N;++reg)
        value.values[reg]=input<R>(samples,offset+reg*11);
      auto result=exp(value);
      auto standard=ftz::exp(value.values);
      auto legacy=exp(simd::wide<R,N>{value.values});
      for (std::size_t reg=0;reg<N;++reg) {
        auto original=words(value.values[reg]);
        auto actual=words(result.values[reg]);
        auto prior=words(standard[reg]);
        auto old_wide=words(legacy.registers[reg]);
        for (std::size_t lane=0;lane<lanes<R>;++lane) {
          auto raw=samples[(offset+reg*11+lane*7)%samples.size()];
          require(original[lane]==canonical(raw),"FTZ factory failed to canonicalize input");
          auto expected=ftz::exp(F::from_bits(raw)).to_bits();
          if (!ftz::math_test::equivalent_fp32(actual[lane],expected)) {
            std::fprintf(stderr,"FTZ wide exp mismatch N=%zu lanes=%zu reg=%zu lane=%zu input=%08x actual=%08x expected=%08x\n",
              N,lanes<R>,reg,lane,raw,actual[lane],expected);
            std::abort();
          }
          require(ftz::math_test::equivalent_fp32(actual[lane],prior[lane]),"std::array exp differs");
          require(ftz::math_test::equivalent_fp32(actual[lane],old_wide[lane]),"legacy wide exp differs");
          require(canonical(actual[lane])==actual[lane],"FTZ exp returned a subnormal");
          output.push_back(actual[lane]);
        }
      }
    }
  }
  template<class F,class R> void shapes(packet & output,packet const & samples) {
    arrays<F,R,0>(output,samples); arrays<F,R,1>(output,samples); arrays<F,R,3>(output,samples);
  }
  template<class F> packet evaluate(packet const & samples) {
    packet output;
    shapes<F,F>(output,samples);
    shapes<F,V<F,1>>(output,samples); shapes<F,V<F,2>>(output,samples);
    shapes<F,V<F,3>>(output,samples); shapes<F,V<F,4>>(output,samples);
#if FTZ_TEST_PROFILE >= 256
    shapes<F,V<F,8>>(output,samples);
#endif
#if FTZ_TEST_PROFILE == 512
    shapes<F,V<F,16>>(output,samples);
#endif
    return output;
  }
  void equal(packet const & a,packet const & b) {
    require(a.size()==b.size(),"FTZ wide exp policy packet size differs");
    for (std::size_t i=0;i<a.size();++i)
      require(ftz::math_test::equivalent_fp32(a[i],b[i]),"FTZ wide exp policy packet differs");
  }
}

int main() {
  auto samples=bank();
  packet manual_gradual,manual_flush,hardware_flush;
  {
    ftz::native_fp32_scope region(ftz::native_fp32_mode::gradual);
    require(ftz::probe_ftz32_cpu<M>().admitted(),"manual FTZ admission failed in gradual mode");
    require(!ftz::probe_ftz32_cpu<H>().admitted(),"hardware FTZ admitted gradual mode");
    manual_gradual=evaluate<M>(samples);
  }
  {
    ftz::native_fp32_scope region(ftz::native_fp32_mode::flush);
    require(ftz::probe_ftz32_cpu<M>().admitted() && ftz::probe_ftz32_cpu<H>().admitted(),"FTZ flush admission failed");
    manual_flush=evaluate<M>(samples); hardware_flush=evaluate<H>(samples);
  }
  equal(manual_gradual,manual_flush); equal(manual_gradual,hardware_flush);
  std::printf("FTZ wide exp passed: profile=%d inputs=%zu packet=%zu words; manual gradual/flush and hardware flush agree\n",
    FTZ_TEST_PROFILE,samples.size(),hardware_flush.size());
}
