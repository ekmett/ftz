// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../core_regression/support/imports.h"
namespace {
  std::size_t checked=0;
  std::FILE * packet=nullptr; bool record_common=true;
  void check(std::uint32_t got,std::uint32_t want) {
    bool nan=(got&0x7fffffffu)>0x7f800000u && (want&0x7fffffffu)>0x7f800000u;
    if(got!=want && !nan) {
      std::fprintf(stderr,"native tanh differs: word=%zu got=%08x want=%08x\n",checked,got,want);
      std::abort();
    }
    if(packet && record_common && std::fwrite(&got,sizeof(got),1,packet)!=1)std::abort();
    ++checked;
  }
  std::vector<std::uint32_t> bank() {
    std::vector<std::uint32_t> out{0,0x80000000u,1,0x007fffffu,0x80000001u,0x807fffffu,
      0x7f800000u,0xff800000u,0x7fc00000u,0x7f800001u,0xffa54321u,0x7f7fffffu};
    for(auto edge:{0x00800000u,0x39800000u,0x3f800000u,0x40000000u,0x40400000u,
        0x40800000u,0x40c00000u,0x41000000u,0x41200000u})
      for(int delta=-3;delta<=3;++delta)for(auto sign:{0u,0x80000000u})
        out.push_back((edge+delta)|sign);
    std::uint32_t state=0x579ad03bu;
    for(unsigned i=0;i<4096;++i) {
      state^=state<<13;state^=state>>17;state^=state<<5;out.push_back(state);
      // Dense coverage of the live polynomial intervals as well as raw words.
      float x=(float(state&0xffffffu)/float(0x1000000u))*24.f-12.f;
      out.push_back(std::bit_cast<std::uint32_t>(x));
    }
    return out;
  }
  template<class F,std::size_t L,std::size_t K>
  void shapes(std::vector<std::uint32_t> const & words) {
    using V=simd::vec<F,L,FTZ_TEST_ARCH>;using W=simd::wide<V,K>;
    static_assert(std::same_as<decltype(ftz::tanh(V{})),V>);
    static_assert(std::same_as<decltype(tanh(W{})),W>);
    static_assert(noexcept(ftz::tanh(V{})) && noexcept(tanh(W{})));
    auto empty=ftz::tanh(std::array<V,0>{});(void)empty;
    auto empty_wide=tanh(simd::wide<V,0>{});(void)empty_wide;
    for(std::size_t i=0;i<words.size();i+=L*K) {
      std::array<V,K> inputs{};std::array<std::array<F,L>,K> lanes{};
      for(std::size_t j=0;j<K;++j) {
        for(std::size_t n=0;n<L;++n)lanes[j][n]=F::from_bits(words[(i+j*L+n)%words.size()]);
        inputs[j]=simd::load_simd<V>(lanes[j]);
      }
      auto batch=ftz::tanh(inputs);auto wide=tanh(W{inputs});
      for(std::size_t j=0;j<K;++j) {
        std::array<F,L> actual{},staged{},individual{};
        simd::store_simd(actual.data(),batch[j]);simd::store_simd(staged.data(),wide.registers[j]);
        simd::store_simd(individual.data(),ftz::tanh(inputs[j]));
        for(std::size_t n=0;n<L;++n) {
          auto expected=ftz::tanh(lanes[j][n]).to_bits();
          check(actual[n].to_bits(),expected);check(staged[n].to_bits(),expected);check(individual[n].to_bits(),expected);
        }
      }
    }
  }
  template<class F> void run(std::vector<std::uint32_t> const & words) {
    shapes<F,1,3>(words);shapes<F,2,3>(words);shapes<F,3,3>(words);shapes<F,4,3>(words);
    record_common=false;
#if FTZ_TEST_PROFILE >= 256
    shapes<F,8,12>(words);
#endif
#if FTZ_TEST_PROFILE == 512
    shapes<F,16,3>(words);
#endif
    record_common=true;
    for(std::size_t i=0;i<words.size();i+=3) {
      std::array<F,3> a{F::from_bits(words[i]),F::from_bits(words[(i+1)%words.size()]),F::from_bits(words[(i+2)%words.size()])};
      auto r=tanh(simd::wide<F,3>{a});
      for(std::size_t j=0;j<3;++j)check(r.registers[j].to_bits(),ftz::tanh(a[j]).to_bits());
    }
  }
}
int main(int argc,char ** argv) {
  if(argc>2)return 1;
  if(argc==2){
#if defined(_MSC_VER)
    if(::fopen_s(&packet,argv[1],"wb")!=0)return 2;
#else
    packet=std::fopen(argv[1],"wb");if(!packet)return 2;
#endif
  }
  auto words=bank();
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);run<ftz::m32>(words);}
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);run<ftz::m32>(words);run<ftz::h32>(words);}
  if(packet && std::fclose(packet)!=0)return 3;
  std::printf("native tanh exact scalar agreement: profile=%d inputs=%zu comparisons=%zu\n",FTZ_TEST_PROFILE,words.size(),checked);
}
