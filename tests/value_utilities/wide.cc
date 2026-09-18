// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include "../core_regression/support/imports.h"
constexpr auto arch=FTZ_TEST_ARCH;
void check(bool value){if(!value)std::abort();}
constexpr std::array words{0u,0x80000000u,1u,0x80000001u,0x00800000u,0x80800000u,0x3f800000u,0xbf800000u,
  0x7f800000u,0xff800000u,0x7fc12345u,0xffc12345u,0x7f800001u,0xff800001u};
template<class A,class B>concept can_copy=requires(A const & a,B const & b){copysign(a,b);};
static_assert(!can_copy<simd::wide<ftz::m32,3>,simd::wide<ftz::h32,3>>);
template<class F,std::size_t L,std::size_t N>void vectors(){
 using V=simd::vec<F,L,arch>;using W=simd::wide<V,N>;using M=typename V::mask;
 static_assert(std::same_as<decltype(isfinite(W{})),simd::wide<M,N>>);
 static_assert(std::same_as<decltype(isinf(W{})),simd::wide<M,N>>);
 static_assert(std::same_as<decltype(isnan(W{})),simd::wide<M,N>>);
 static_assert(std::same_as<decltype(signbit(W{})),simd::wide<M,N>>);
 static_assert(std::same_as<decltype(copysign(W{},W{})),W>);
 static_assert(noexcept(isfinite(W{})) && noexcept(copysign(W{},W{})));
 for(auto word:words){
  auto value=F::from_bits(word);std::array<F,L> source{};for(auto & lane:source)lane=value;
  auto vector=simd::load_simd<V>(source.data());W x(vector),signs(V(F::from_bits(0x80000000u)));
  auto finite=isfinite(x),inf=isinf(x),nan=isnan(x),sign=signbit(x);auto result=copysign(x,signs);
  auto magnitude=value.to_bits()&0x7fffffffu;
  auto mask=[](M m,bool expected){
   if constexpr(std::same_as<M,bool>)check(m==expected);
   else {std::array<std::uint32_t,L> bits{};simd::mask_bits<std::uint32_t>(m).store(bits.data());for(auto b:bits)check(b==(expected?0xffffffffu:0u));}
  };
  for(std::size_t i=0;i<N;++i){
   mask(finite.registers[i],magnitude<0x7f800000u);mask(inf.registers[i],magnitude==0x7f800000u);
   mask(nan.registers[i],magnitude>0x7f800000u);mask(sign.registers[i],(word&0x80000000u)!=0);
   std::array<F,L> output{};simd::store_simd(output.data(),result.registers[i]);
   for(auto lane:output)check(lane.to_bits()==(magnitude|0x80000000u));
  }
 }
}
template<class F>void transport(){
 using V2=simd::vec<F,2,arch>;using V3=simd::vec<F,3,arch>;
 static_assert(std::same_as<decltype(std::declval<V3>().zyx),V3>);
 static_assert(std::same_as<decltype(std::declval<V3>().xy),V2>);
 for(std::size_t i=0;i<words.size();++i){
  std::array<F,3> input;
  for(std::size_t j=0;j<3;++j)input[j]=F::unsafe_from_float32(std::bit_cast<float>(words[(i+j)%words.size()]));
  V3 value=simd::load_simd<V3>(input.data());
  V3 reversed=value.zyx;std::array<F,3> triple{};simd::store_simd(triple.data(),reversed);
  for(std::size_t j=0;j<3;++j)check(triple[j].to_bits()==input[2-j].to_bits());
  V2 pair=value.xy;pair.xy=pair.yx;std::array<F,2> lanes{};simd::store_simd(lanes.data(),pair);
  check(lanes[0].to_bits()==input[1].to_bits());check(lanes[1].to_bits()==input[0].to_bits());
  value.xy=value.yx;simd::store_simd(triple.data(),value);
  check(triple[0].to_bits()==input[1].to_bits());check(triple[1].to_bits()==input[0].to_bits());check(triple[2].to_bits()==input[2].to_bits());
 }
}
template<class F>void run(){
 transport<F>();
 vectors<F,1,0>();vectors<F,1,1>();vectors<F,1,3>();vectors<F,2,3>();vectors<F,3,3>();vectors<F,4,3>();
#if FTZ_TEST_PROFILE != 128
 vectors<F,8,3>();
#endif
#if FTZ_TEST_PROFILE == 512
 vectors<F,16,3>();
#endif
 for(auto word:words){auto value=F::from_bits(word);simd::wide<F,3> x(value),y(F::from_bits(0x80000000u));
  static_assert(std::same_as<decltype(isfinite(x)),simd::wide<bool,3>>);
  auto result=copysign(x,y);auto finite=isfinite(x);auto nan=isnan(x);auto inf=isinf(x);auto sign=signbit(x);
  auto magnitude=value.to_bits()&0x7fffffffu;
  for(int i=0;i<3;++i){check(result.registers[i].to_bits()==(magnitude|0x80000000u));check(finite.registers[i]==(magnitude<0x7f800000u));check(nan.registers[i]==(magnitude>0x7f800000u));check(inf.registers[i]==(magnitude==0x7f800000u));check(sign.registers[i]==((word&0x80000000u)!=0));}
 }
}
int main(){
 {ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);auto before=ftz::read_native_fp_state();run<ftz::m32>();check(before==ftz::read_native_fp_state());}
 {ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);auto before=ftz::read_native_fp_state();run<ftz::m32>();run<ftz::h32>();check(before==ftz::read_native_fp_state());}
}
