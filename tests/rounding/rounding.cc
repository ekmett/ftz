// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <array>
#include <bit>
#include <cfenv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <string>
#include <utility>
#include <vector>
#include "../core_regression/support/imports.h"
constexpr auto arch=FTZ_TEST_ARCH;
enum class direction { down,up,zero };
std::size_t comparisons=0;
void check(bool value) { if(!value)std::abort(); }

// Integer-only IEEE binary32 oracle: remove fractional significand bits, then
// carry one integer unit only when the requested direction moves away from zero.
std::uint32_t expected(std::uint32_t word,direction mode) {
  auto magnitude=word&0x7fffffffu,sign=word&0x80000000u;
  if(magnitude==0 || magnitude>=0x4b000000u)return word;
  bool away=(mode==direction::down && sign!=0) || (mode==direction::up && sign==0);
  if(magnitude<0x3f800000u)return sign|(away?0x3f800000u:0u);
  unsigned fraction_bits=150u-(magnitude>>23);
  std::uint32_t unit=1u<<fraction_bits,mask=unit-1;
  return (word&~mask)+((away && (word&mask)!=0)?unit:0u);
}
std::uint32_t canonical(std::uint32_t word) {
  return (word&0x7fffffffu)<0x00800000u?word&0x80000000u:word;
}
void compare(std::uint32_t actual,std::uint32_t input,direction mode) {
  ++comparisons;
  auto want=expected(input,mode);
  if((want&0x7fffffffu)>0x7f800000u) {check((actual&0x7fffffffu)>0x7f800000u);return;}
  if(actual!=want) {
    std::fprintf(stderr,"direction=%d input=%08x actual=%08x expected=%08x\n",int(mode),input,actual,want);
    std::abort();
  }
}
template<direction Mode,class T>auto apply(T const & input) noexcept {
  if constexpr(Mode==direction::down)return floor(input);
  else if constexpr(Mode==direction::up)return ceil(input);
  else return trunc(input);
}
std::vector<std::uint32_t> bank() {
  std::vector<std::uint32_t> words{0,0x80000000u,1,0x80000001u,0x007fffffu,0x807fffffu,
    0x00800000u,0x80800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,
    0x7fc12345u,0xffc12345u,0x7f800001u,0xff800001u};
  // Adjacent words around fractions, integer boundaries, and the last binade
  // with fractional binary32 values. All larger finite binary32s are integral.
  for(auto middle:std::array{0x00800000u,0x3e800000u,0x3f000000u,0x3f800000u,
      0x40000000u,0x40400000u,0x40800000u,0x43800000u,0x49800000u,
      0x4a000000u,0x4a800000u,0x4b000000u,0x4b800000u,0x4c000000u})
    for(int delta=-3;delta<=3;++delta)
      for(auto sign:std::array{0u,0x80000000u})words.push_back((middle+delta)|sign);
  std::uint32_t state=0xa83b196fu;
  for(int i=0;i<64;++i){state^=state<<13;state^=state>>17;state^=state<<5;words.push_back(state);}
  return words;
}
template<direction Mode,class F>void scalar(std::vector<std::uint32_t> const & words) {
  static_assert(std::same_as<decltype(apply<Mode>(F{})),F>);
  static_assert(noexcept(floor(F{})) && noexcept(ceil(F{})) && noexcept(trunc(F{})));
  for(auto word:words) {
    F x=F::from_bits(word);check(x.to_bits()==canonical(word));
    compare(apply<Mode>(x).to_bits(),canonical(word),Mode);
    std::array<F,3> values{x,x,x};auto a=apply<Mode>(values);
    auto w=apply<Mode>(::native::wide<F,3>{values});
    for(std::size_t i=0;i<3;++i){compare(a[i].to_bits(),x.to_bits(),Mode);compare(w.registers[i].to_bits(),x.to_bits(),Mode);}
  }
  static_assert(std::same_as<decltype(apply<Mode>(std::array<F,0>{})),std::array<F,0>>);
  check(apply<Mode>(std::array<F,0>{}).empty());
  static_assert(std::same_as<decltype(apply<Mode>(::native::wide<F,0>{})),::native::wide<F,0>>);
  (void)apply<Mode>(::native::wide<F,0>{});
}
template<direction Mode,class F,std::size_t N>void vectors(std::vector<std::uint32_t> const & words) {
  using V=::native::simd<F,N,arch>;
  static_assert(std::same_as<decltype(apply<Mode>(V{})),V>);
  static_assert(noexcept(floor(V{})) && noexcept(ceil(V{})) && noexcept(trunc(V{})));
  for(std::size_t offset=0;offset<words.size();offset+=N) {
    std::array<F,N> values{};
    for(std::size_t j=0;j<N;++j)values[j]=F::from_bits(words[(offset+j)%words.size()]);
    auto x=::native::load_simd<V>(values.data());
    auto verify=[&](V result){std::array<F,N> out{};::native::store_simd(out.data(),result);for(std::size_t j=0;j<N;++j)compare(out[j].to_bits(),values[j].to_bits(),Mode);};
    verify(apply<Mode>(x));
    std::array<V,3> a{x,x,x};for(auto value:apply<Mode>(a))verify(value);
    auto w=apply<Mode>(::native::wide<V,3>{a});for(auto value:w.registers)verify(value);
  }
  (void)apply<Mode>(std::array<V,0>{});(void)apply<Mode>(::native::wide<V,0>{});
}
template<direction Mode,class F>void operation(std::vector<std::uint32_t> const & words) {
  scalar<Mode,F>(words);vectors<Mode,F,1>(words);vectors<Mode,F,2>(words);vectors<Mode,F,3>(words);vectors<Mode,F,4>(words);
#if FTZ_TEST_PROFILE != 128
  vectors<Mode,F,8>(words);
#endif
#if FTZ_TEST_PROFILE == 512
  vectors<Mode,F,16>(words);
#endif
}
template<class F>void run(std::vector<std::uint32_t> const & words) {
  operation<direction::down,F>(words);operation<direction::up,F>(words);operation<direction::zero,F>(words);
}
struct conversion {
  int * calls;
  operator ftz::m32() const noexcept {++*calls;return ftz::m32(-.25f);}
};
struct throwing_conversion { operator ftz::h32() const noexcept(false); };
static_assert(!noexcept(ftz::floor(std::declval<throwing_conversion>())));
static_assert(!noexcept(ftz::ceil(std::declval<throwing_conversion>())));
static_assert(!noexcept(ftz::trunc(std::declval<throwing_conversion>())));
static_assert(noexcept(ftz::floor(std::declval<conversion>())));
void capture(std::vector<std::uint32_t> const & words,std::string const & prefix) {
  using record=std::array<std::uint32_t,4>;
  std::vector<record> input,output;
  for(auto word:words) {
    auto x=canonical(word);input.push_back({x,0u,0u,0u});
    output.push_back({expected(x,direction::down),expected(x,direction::up),expected(x,direction::zero),x});
  }
  auto write=[&](char const * suffix,std::vector<record> const & records) {
    auto path=prefix+suffix;auto file=std::fopen(path.c_str(),"wb");check(file!=nullptr);
    check(std::fwrite(records.data(),sizeof(record),records.size(),file)==records.size());check(std::fclose(file)==0);
  };
  write(".inputs.bin",input);write(".expected.bin",output);
  std::printf("%zu uint4 records captured; canonical input.x and floor/ceil/trunc/echo expected\n",words.size());
}
int main(int argc,char ** argv) {
  auto words=bank();check(argc<=2);
  if(argc==2){capture(words,argv[1]);return 0;}
  auto original=ftz::read_native_fp_state();
  std::fenv_t original_environment{};check(std::fegetenv(&original_environment)==0);
  for(auto mode:std::array{ftz::native_fp32_mode::gradual,ftz::native_fp32_mode::flush}) {
    ftz::native_fp32_scope scope(mode);check(scope.controls_match());
    for(int rounding:std::array{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}) {
      check(std::fesetround(rounding)==0);
      run<ftz::m32>(words);if(mode==ftz::native_fp32_mode::flush)run<ftz::h32>(words);
      check(std::fegetround()==rounding);
      int calls=0;auto result=ftz::floor(conversion{&calls});check(calls==1 && result.to_bits()==0xbf800000u);
    }
  }
  check(ftz::read_native_fp_state()==original);
  check(std::fesetenv(&original_environment)==0);
  std::printf("%zu independent word comparisons; named directions retained across four rounding modes\n",comparisons);
}
