// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../core_regression/support/imports.h"
namespace {
  std::vector<std::pair<std::uint32_t,std::uint32_t>> inputs;
  std::vector<std::uint32_t> reference,packet;
  std::size_t checks=0,index=0;bool capture=true;
  bool same(std::uint32_t a,std::uint32_t b) {
    return a==b || ((a&0x7fffffffu)>0x7f800000u && (b&0x7fffffffu)>0x7f800000u);
  }
  template<class F> F value(std::uint32_t word){return F::from_bits(word);}
  template<class F> void check(F actual,std::size_t i) {
    auto [y,x]=inputs[i];auto expected=ftz::atan2(value<F>(y),value<F>(x));
    auto bits=actual.to_bits();
    if(!same(bits,expected.to_bits())) {
      std::fprintf(stderr,"atan2 y=%08x x=%08x actual=%08x scalar=%08x at%zu\n",y,x,bits,expected.to_bits(),checks);std::abort();
    }
    if(capture)reference.push_back(bits);
    else if(!same(bits,reference.at(index))){std::fprintf(stderr,"mode/policy mismatch at%zu\n",index);std::abort();}
    ++index;++checks;
  }
  template<class F,std::size_t N> void vectors() {
    using V=simd::vec<F,N,FTZ_TEST_ARCH>;
    static_assert(std::same_as<decltype(atan2(V{},V{})),V>);
    auto verify=[](V result,std::size_t offset,bool record) {
      std::array<F,N> values;simd::store_simd(values.data(),result);
      for(std::size_t j=0;j<N;++j) {
        check(values[j],offset+j);
        if constexpr(N==4)if(record && capture)packet.push_back(values[j].to_bits());
      }
    };
    for(std::size_t i=0;i+2*N<=inputs.size();i+=2*N) {
      std::array<F,2*N> y,x;
      for(std::size_t j=0;j<2*N;++j){y[j]=value<F>(inputs[i+j].first);x[j]=value<F>(inputs[i+j].second);}
      V a=simd::load_simd<V>(y.data()),b=simd::load_simd<V>(x.data());
      V c=simd::load_simd<V>(y.data()+N),d=simd::load_simd<V>(x.data()+N);
      verify(atan2(a,b),i,false);
      std::array ys{a,c},xs{b,d};auto result=atan2(ys,xs);
      for(std::size_t j=0;j<2;++j)verify(result[j],i+j*N,true);
      simd::wide wy{a,c},wx{b,d};auto wr=atan2(wy,wx);
      for(std::size_t j=0;j<2;++j)verify(wr.registers[j],i+j*N,false);
    }
    std::array<V,0> empty;if(!atan2(empty,empty).empty())std::abort();
    simd::wide<V,0> wide_empty;(void)atan2(wide_empty,wide_empty);
  }
  template<class F> void run() {
    for(std::size_t i=0;i+2<=inputs.size();i+=2) {
      std::array y{value<F>(inputs[i].first),value<F>(inputs[i+1].first)};
      std::array x{value<F>(inputs[i].second),value<F>(inputs[i+1].second)};
      auto result=atan2(y,x);for(std::size_t j=0;j<2;++j)check(result[j],i+j);
      simd::wide wy{y},wx{x};auto wr=atan2(wy,wx);
      for(std::size_t j=0;j<2;++j)check(wr.registers[j],i+j);
    }
    vectors<F,1>();vectors<F,2>();vectors<F,3>();vectors<F,4>();
#if FTZ_TEST_PROFILE != 128
    vectors<F,8>();
#endif
#if FTZ_TEST_PROFILE == 512
    vectors<F,16>();
#endif
    if(!capture && index!=reference.size())std::abort();index=0;capture=false;
  }
  void quadrants(std::uint32_t y,std::uint32_t x) {
    for(auto sy:{0u,0x80000000u})for(auto sx:{0u,0x80000000u}) {
      inputs.emplace_back(y^sy,x^sx);inputs.emplace_back(x^sx,y^sy);
    }
  }
}
int main(int argc,char **argv) {
  std::vector<std::uint32_t> special;
  for(auto x:{0u,1u,0x007fffffu,0x00800000u,0x00800001u,0x00ffffffu,
      0x39800000u,0x3f000000u,0x3f800000u,0x40000000u,0x7f7fffffu,
      0x7f800000u,0x7fc00000u,0x7f800001u}) {
    special.push_back(x);special.push_back(x^0x80000000u);
  }
  for(auto y:special)for(auto x:special)inputs.emplace_back(y,x);
  for(int d=-32;d<=32;++d)quadrants(0x39800000u+d,0x3f800000u);
  for(unsigned exponent=1;exponent<255;++exponent)
    for(int d=-2;d<=2;++d)quadrants((exponent<<23)+d,0x3f800000u);
  for(auto a:{0x00800000u,0x00800001u,0x00ffffffu,0x01000000u})
    for(auto b:{0x3f800000u,0x40000000u})for(int d=-16;d<=16;++d)quadrants(a,b+d);
  std::uint32_t state=0xfa728311u;
  auto next=[&]{state^=state<<13;state^=state>>17;state^=state<<5;return state;};
  for(unsigned i=0;i<8192;++i){auto y=next(),x=next();inputs.emplace_back(y,x);}
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);run<ftz::m32>();}
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);run<ftz::m32>();run<ftz::h32>();}
  if(argc==2) {
    auto file=std::fopen(argv[1],"wb");if(!file)return 2;
    auto count=std::fwrite(packet.data(),sizeof(packet[0]),packet.size(),file);std::fclose(file);
    if(count!=packet.size())return 3;
  }
  std::printf("scalar-oracle/cross-mode atan2 comparisons: %zu; common packet words: %zu\n",checks,packet.size());
}
