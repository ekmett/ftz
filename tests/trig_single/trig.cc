// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>
#include "../core_regression/support/imports.h"
namespace {
 std::vector<std::uint32_t> inputs;
 std::FILE * packet;
 bool common_packet=false;
 std::size_t checks=0;
 bool same(std::uint32_t a,std::uint32_t b){return a==b || ((a&0x7fffffffu)>0x7f800000u && (b&0x7fffffffu)>0x7f800000u);}
 template<class F> void compare(F s,F c,F ps,F pc){
  if(!same(s.to_bits(),ps.to_bits()) || !same(c.to_bits(),pc.to_bits())) {
   std::fprintf(stderr,"trig mismatch at %zu: %08x/%08x %08x/%08x\n",checks,s.to_bits(),ps.to_bits(),c.to_bits(),pc.to_bits());std::abort();
  }
  std::uint32_t words[]={ps.to_bits(),pc.to_bits()};
  if(packet && std::fwrite(words,sizeof(words),1,packet)!=1)std::abort();
  ++checks;
 }
 template<class F> void scalar(){
  std::array<F,0> empty;(void)sin(empty);(void)cos(empty);(void)sincos(empty);
  ::native::wide<F,0> wide_empty;(void)sin(wide_empty);(void)cos(wide_empty);(void)sincos(wide_empty);
  for(auto bits:inputs){F a=F::from_bits(bits);auto pair=sincos(a);compare(sin(a),cos(a),pair.first,pair.second);}
  for(std::size_t i=0;i+1<inputs.size();i+=2){
   ::native::wide a{F::from_bits(inputs[i]),F::from_bits(inputs[i+1])};
   auto pair=sincos(a);auto s=sin(a),c=cos(a);
   for(std::size_t j=0;j<2;++j)compare(s.registers[j],c.registers[j],pair.first.registers[j],pair.second.registers[j]);
  }
 }
 // Explicit repair densities compare every packed output to the scalar pair,
 // rather than comparing two vector paths that might share the same error.
 template<class F,std::size_t N> void repair_densities(){
  using V=::native::simd<F,N,FTZ_TEST_ARCH>;
  constexpr std::array bounded{0u,0x80000000u,0x3f800000u,0xbf800000u,0x45ffffffu,0xc5ffffffu};
  constexpr std::array exceptional{0x46000000u,0xc6000000u,0x46000001u,0xc6000001u,
   0x4b000000u,0xcb000000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,
   0x7fc00000u,0x7f800001u};
  auto check=[](std::array<F,2*N> const & values){
   V a=::native::load_simd<V>(values.data()),b=::native::load_simd<V>(values.data()+N);
   auto check_register=[&](auto const & pair,std::size_t offset){
    std::array<F,N> sine,cosine;
    ::native::store_simd(sine.data(),pair.first);::native::store_simd(cosine.data(),pair.second);
    for(std::size_t lane=0;lane<N;++lane){
     auto expected=sincos(values[offset+lane]);
     compare(expected.first,expected.second,sine[lane],cosine[lane]);
    }
   };
   check_register(sincos(a),0);check_register(sincos(b),N);
   auto array_pair=sincos(std::array{a,b});
   auto wide_pair=sincos(::native::wide{a,b});
   for(std::size_t j=0;j<2;++j){
    check_register(std::pair{array_pair.first[j],array_pair.second[j]},j*N);
    check_register(std::pair{wide_pair.first.registers[j],wide_pair.second.registers[j]},j*N);
   }
  };
  for(std::size_t offset=0;offset<exceptional.size();++offset){
   std::array<F,2*N> values;
   for(std::size_t lane=0;lane<2*N;++lane)values[lane]=F::from_bits(bounded[(lane+offset)%bounded.size()]);
   check(values); // No repairs, including both sides immediately below 8192.
   for(std::size_t lane=0;lane<2*N;++lane){
    auto saved=values[lane];values[lane]=F::from_bits(exceptional[offset]);
    check(values);values[lane]=saved; // One repair, at every logical lane.
   }
   for(std::size_t lane=0;lane<2*N;++lane)values[lane]=F::from_bits(exceptional[(lane+offset)%exceptional.size()]);
   check(values); // Every lane repairs; large finite values and special values mix.
  }
 }
 template<class F,std::size_t N> void vectors(){
  repair_densities<F,N>();
  using V=::native::simd<F,N,FTZ_TEST_ARCH>;
  auto check=[](V s,V c,V ps,V pc){
   std::array<F,N> a,b,pa,pb;
   ::native::store_simd(a.data(),s);::native::store_simd(b.data(),c);::native::store_simd(pa.data(),ps);::native::store_simd(pb.data(),pc);
   for(std::size_t j=0;j<N;++j)compare(a[j],b[j],pa[j],pb[j]);
  };
  for(std::size_t i=0;i+2*N<=inputs.size();i+=2*N){
   std::array<F,2*N> values;
   for(std::size_t j=0;j<2*N;++j)values[j]=F::from_bits(inputs[i+j]);
   V a=::native::load_simd<V>(values.data()),b=::native::load_simd<V>(values.data()+N);
   auto pair=sincos(a);check(sin(a),cos(a),pair.first,pair.second);
   std::array pack{a,b};auto pair_pack=sincos(pack);auto s=sin(pack),c=cos(pack);
   for(std::size_t j=0;j<2;++j)check(s[j],c[j],pair_pack.first[j],pair_pack.second[j]);
   ::native::wide wide{a,b};auto pair_wide=sincos(wide);auto ws=sin(wide),wc=cos(wide);
   for(std::size_t j=0;j<2;++j)check(ws.registers[j],wc.registers[j],pair_wide.first.registers[j],pair_wide.second.registers[j]);
  }
  std::array<V,0> empty;auto pair=sincos(empty);
  static_assert(std::same_as<decltype(sin(empty)),std::array<V,0>>);
  if(!sin(empty).empty() || !cos(empty).empty() || !pair.first.empty() || !pair.second.empty())std::abort();
  ::native::wide<V,0> wide_empty;(void)sin(wide_empty);(void)cos(wide_empty);(void)sincos(wide_empty);
 }
 template<class F> void run(){
  scalar<F>();vectors<F,1>();vectors<F,2>();vectors<F,3>();vectors<F,4>();
  auto saved_packet=packet;
  if(common_packet)packet=nullptr;
#if FTZ_TEST_PROFILE != 128
  vectors<F,8>();
#endif
#if FTZ_TEST_PROFILE == 512
  vectors<F,16>();
#endif
  packet=saved_packet;
 }
}
int main(int argc,char**argv){
 if(argc>3 || (argc==3 && std::string_view(argv[2])!="--common"))return 1;
 common_packet=argc==3;
 for(std::uint32_t center:{0u,1u,0x007fffffu,0x00800000u,0x39800000u,0x3f800000u,0x3fc90fdbu,0x40490fdbu,0x4096cbe4u,0x40c90fdbu,0x45ffffffu,0x46000000u,0x46000001u,0x4b000000u,0x7f7fffffu,0x7f800000u,0x7fc00000u,0x7f800001u})
  for(int delta=-4;delta<=4;++delta){auto word=center+delta;inputs.push_back(word);inputs.push_back(word^0x80000000u);}
 std::uint32_t state=0xa123f901u;
 for(unsigned i=0;i<4096;++i){state^=state<<13;state^=state>>17;state^=state<<5;inputs.push_back(state);}
 if(argc>=2){packet=std::fopen(argv[1],"wb");if(!packet)return 2;}
 {ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);run<ftz::m32>();}
 {ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);run<ftz::m32>();run<ftz::h32>();}
 if(packet && std::fclose(packet)!=0)return 3;
 std::printf("single/pair exact finite and signed-zero comparisons: %zu\n",checks);
}
