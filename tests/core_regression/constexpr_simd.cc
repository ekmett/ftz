// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "math_contract.h"
#include <cstdio>
#include <cstdlib>
#include "support/imports.h"

namespace {
  template<class F,std::size_t N> using vector=::native::simd<F,N,FTZ_TEST_ARCH>;
  template<class R> constexpr auto words(R value) {
    std::array<std::uint32_t,R::lanes> result{};
    value.store_bits(result.data());
    return result;
  }
  template<class F,std::size_t N> constexpr bool memory() {
    using R=vector<F,N>;
    std::array<F,N> input{},output{};
    for(std::size_t i=0;i<N;++i)
      input[i]=F::unsafe_from_float32(std::bit_cast<float>(i==0?0x80000000u:0x7fc12345u));
    auto const value=R::load_memory(input.data());
    value.store_memory(output.data());
    if(words(value)!=words(R(input))) return false;
    for(std::size_t i=0;i<N;++i)
      if(input[i].to_bits()!=output[i].to_bits()) return false;
    auto const normalized=R::from_bits(typename R::bits_type(0x80000001u));
    auto const negative_zero=R(F::from_bits(0x80000000u));
    return words(normalized)==words(negative_zero) &&
      words(copysign(abs(value),negative_zero))==words(-abs(value));
  }
  template<class F,std::size_t N> constexpr bool corners() {
    using R=vector<F,N>;
    auto const infinity=R(F::from_bits(0x7f800000u));
    auto const zero=R(F(0.f));
    return words(sqrt(infinity))==words(infinity) &&
      words(log(zero))==words(R(F::from_bits(0xff800000u))) &&
      words(R(F::from_bits(0x7f7fffffu))+R(F::from_bits(0x7f7fffffu)))==words(infinity) &&
      all(ftz::isnan(sin(infinity)));
  }
  template<class F,std::size_t N> constexpr auto evaluate(vector<F,N> value) {
    using R=vector<F,N>;
    auto const twice=R(F(2.f));
    auto const shifted=value+F(1.f);
    auto const [sine,cosine]=sincos(value);
    return std::array{
      words(value+twice),words(value-twice),words(value*twice),words(value/twice),
      words(fma(value,twice,shifted)),words(sqrt(shifted)),
      words(sin(value)),words(cos(value)),words(sine),words(cosine),
      words(exp(value)),words(expm1(value)),words(tanh(value)),
      words(log(shifted)),words(log1p(value)),words(atan2(value,twice)),
      words(floor(value)),words(ceil(value)),words(trunc(value)),
      words(scaleb(value,R(F(1.5f)))),words(select(value<R(F(0.f)),twice,value))};
  }
  template<class F> constexpr bool batches() {
    using R=vector<F,3>;
    std::array<R,2> input{R(F(0.5f)),R(F(1.f))};
    auto const sum=ftz::add(input,input);
    if(words(sum[0])!=words(R(F(1.f)))) return false;
    auto const fused=ftz::fma(input,input,input);
    if(words(fused[1])!=words(R(F(2.f)))) return false;
    auto const empty=ftz::exp(std::array<R,0>{});
    if(!empty.empty()) return false;
    ::native::wide<R,2> wide(input);
    auto const result=exp(wide);
    return words(result.registers[0])==words(exp(input)[0]);
  }
  static_assert(memory<ftz::m32,1>() && memory<ftz::h32,1>());
  static_assert(memory<ftz::m32,3>() && memory<ftz::h32,3>());
  static_assert(memory<ftz::m32,4>() && memory<ftz::h32,4>());
  static_assert(batches<ftz::m32>() && batches<ftz::h32>());
  static_assert(corners<ftz::m32,1>() && corners<ftz::h32,1>());
  static_assert(corners<ftz::m32,3>() && corners<ftz::h32,3>());
  static_assert(corners<ftz::m32,4>() && corners<ftz::h32,4>());

  template<class F,std::size_t N> void compare() {
    using R=vector<F,N>;
    constexpr std::array values{0.5f,-0.5f,1.25f,-0.0f};
    constexpr R input=R::load(values.data());
    constexpr auto expected=evaluate<F,N>(input);
    volatile float runtime_values[]{0.5f,-0.5f,1.25f,-0.0f};
    std::array<float,N> runtime_input{};
    for(std::size_t lane=0;lane<N;++lane) runtime_input[lane]=runtime_values[lane];
    auto actual=evaluate<F,N>(R::load(runtime_input.data()));
    for(std::size_t operation=0;operation<actual.size();++operation)
      for(std::size_t lane=0;lane<N;++lane)
        if(!ftz::math_test::equivalent_fp32(actual[operation][lane],expected[operation][lane])) {
          std::fprintf(stderr,"constexpr SIMD mismatch: h=%d, lanes=%zu, operation=%zu, lane=%zu\n",
            F::hardware,N,operation,lane);
          std::abort();
        }
    constexpr auto minimum=F::from_bits(0x00800000u);
    constexpr auto tiny=R(minimum)*R(F(0.5f));
    static_assert(words(tiny)==std::array<std::uint32_t,N>{});
    constexpr auto boundary=R(minimum)+R(F::from_bits(0x80800000u));
    static_assert(words(boundary)==std::array<std::uint32_t,N>{});
  }
}
int main() {
  { ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    if(!scope.controls_match()) return 1;
    compare<ftz::m32,1>();compare<ftz::m32,3>();compare<ftz::m32,4>();
  }
  { ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    if(!scope.controls_match()) return 1;
    compare<ftz::h32,1>();compare<ftz::h32,3>();compare<ftz::h32,4>();
  }
  std::puts("FTZ constexpr SIMD matches runtime graphs for both policies");
}
