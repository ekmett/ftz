// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../core_regression/support/imports.h"
namespace {
 std::vector<std::uint32_t> inputs,reference;
 std::FILE * packet=nullptr; bool record_common=true;
 std::size_t checks=0,index=0;bool capture=true;
 bool same(std::uint32_t a,std::uint32_t b){return a==b || ((a&0x7fffffffu)>0x7f800000u && (b&0x7fffffffu)>0x7f800000u);}
 template<class F> void check(F actual,F expected,std::uint32_t input,char const* op){
  auto bits=actual.to_bits();
  if(!same(bits,expected.to_bits())){
   std::fprintf(stderr,"%s input=%08x actual=%08x scalar=%08x at%zu\n",op,input,bits,expected.to_bits(),checks);std::abort();
  }
  if(capture)reference.push_back(bits);
  else if(!same(bits,reference.at(index))){std::fprintf(stderr,"mode/policy mismatch at%zu\n",index);std::abort();}
  if(packet && record_common && std::fwrite(&bits,sizeof(bits),1,packet)!=1)std::abort();
  ++index;++checks;
 }
 template<class F> F input(std::size_t i){return F::from_bits(inputs[i]);}
 template<class F,std::size_t N> void vectors(){
  using V=::native::simd<F,N,FTZ_TEST_ARCH>;
  static_assert(std::same_as<decltype(log(V{})),V> && std::same_as<decltype(log1p(V{})),V>);
  auto verify=[](V a,V b,std::size_t offset){
   std::array<F,N> x,y;::native::store_simd(x.data(),a);::native::store_simd(y.data(),b);
   for(std::size_t j=0;j<N;++j){F original=input<F>(offset+j);check(x[j],ftz::log(original),inputs[offset+j],"log");check(y[j],ftz::log1p(original),inputs[offset+j],"log1p");}
  };
  for(std::size_t i=0;i+2*N<=inputs.size();i+=2*N){
   std::array<F,2*N> values;for(std::size_t j=0;j<2*N;++j)values[j]=input<F>(i+j);
   V a=::native::load_simd<V>(values.data()),b=::native::load_simd<V>(values.data()+N);
   verify(log(a),log1p(a),i);
   std::array array{a,b};auto x=log(array),y=log1p(array);
   for(std::size_t j=0;j<2;++j)verify(x[j],y[j],i+j*N);
   ::native::wide wide{a,b};auto wx=log(wide),wy=log1p(wide);
   for(std::size_t j=0;j<2;++j)verify(wx.registers[j],wy.registers[j],i+j*N);
  }
  std::array<V,0> empty;if(!log(empty).empty() || !log1p(empty).empty())std::abort();
  ::native::wide<V,0> wide_empty;(void)log(wide_empty);(void)log1p(wide_empty);
 }
 template<class F> void run(){
  for(std::size_t i=0;i+2<=inputs.size();i+=2){
   std::array a{input<F>(i),input<F>(i+1)};auto x=log(a),y=log1p(a);
   ::native::wide w{a};auto wx=log(w),wy=log1p(w);
   for(std::size_t j=0;j<2;++j){auto sx=ftz::log(a[j]),sy=ftz::log1p(a[j]);
    check(x[j],sx,inputs[i+j],"array log");check(y[j],sy,inputs[i+j],"array log1p");
    check(wx.registers[j],sx,inputs[i+j],"wide log");check(wy.registers[j],sy,inputs[i+j],"wide log1p");}
  }
  vectors<F,1>();vectors<F,2>();vectors<F,3>();vectors<F,4>();
  record_common=false;
#if FTZ_TEST_PROFILE != 128
  vectors<F,8>();
#endif
#if FTZ_TEST_PROFILE == 512
  vectors<F,16>();
#endif
  record_common=true;
  if(!capture && index!=reference.size())std::abort();index=0;capture=false;
 }
}
int main(int argc,char ** argv){
 if(argc>2)return 1;
 if(argc==2){packet=std::fopen(argv[1],"wb");if(!packet)return 2;}
 for(std::uint32_t center:{0u,1u,0x007fffffu,0x00800000u,0x33000000u,0x3e800000u,0x3f000000u,0x3f400000u,0x3f800000u,0x3fc00000u,0x40000000u,0x7f7fffffu,0x7f800000u,0x7fc00000u,0x7f800001u})
  for(int d=-16;d<=16;++d){auto word=center+d;inputs.push_back(word);inputs.push_back(word^0x80000000u);}
 // Neighboring floats around1 exercise log cancellation and log1p's -1 edge.
 for(int d=-4096;d<=4096;++d){inputs.push_back(0x3f800000u+d);inputs.push_back(0xbf800000u+d);}
 for(unsigned exponent=1;exponent<255;++exponent)
  for(int d=-2;d<=2;++d){inputs.push_back((exponent<<23)+d);inputs.push_back(((exponent<<23)+d)^0x80000000u);}
 std::uint32_t state=0xfa728311u;for(unsigned i=0;i<8192;++i){state^=state<<13;state^=state>>17;state^=state<<5;inputs.push_back(state);}
 {ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);run<ftz::m32>();}
 {ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);run<ftz::m32>();run<ftz::h32>();}
 if(packet && std::fclose(packet)!=0)return 3;
 std::printf("scalar-oracle and cross-mode log/log1p comparisons: %zu\n",checks);
}
