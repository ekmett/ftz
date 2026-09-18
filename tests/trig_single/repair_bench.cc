// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include "../core_regression/support/imports.h"
extern "C" void ftz_sincos_manual(float*,float*,float const*);
extern "C" void ftz_sincos_hardware(float*,float*,float const*);
int main(int argc,char **argv){
  std::size_t iterations=argc==2?std::strtoull(argv[1],nullptr,10):50000;
  if(iterations==0 || argc>2)return 1;
  constexpr std::size_t lanes=FTZ_TEST_PROFILE/32;
  std::array<float,lanes> input,sine,cosine;
  for(bool hardware:{false,true}){
    ftz::native_fp32_scope scope(hardware?ftz::native_fp32_mode::flush:ftz::native_fp32_mode::gradual);
    auto function=hardware?ftz_sincos_hardware:ftz_sincos_manual;
    for(std::size_t repairs:{std::size_t(0),std::size_t(1),lanes}){
      for(std::size_t i=0;i<lanes;++i)
        input[i]=i<repairs?std::bit_cast<float>(0x5f123456u+std::uint32_t(i)*0x00100101u):0.25f+float(i)*0.125f;
      for(unsigned warm=0;warm<1000;++warm)function(sine.data(),cosine.data(),input.data());
      auto start=std::chrono::steady_clock::now();
      for(std::size_t i=0;i<iterations;++i)function(sine.data(),cosine.data(),input.data());
      auto end=std::chrono::steady_clock::now();
      std::uint32_t checksum=0;
      for(std::size_t i=0;i<lanes;++i)checksum^=std::bit_cast<std::uint32_t>(sine[i])^std::bit_cast<std::uint32_t>(cosine[i]);
      auto elapsed=std::chrono::duration<double,std::nano>(end-start).count();
      std::printf("%s,%zu,%zu,%.6f,%08x\n",hardware?"hardware":"manual",lanes,repairs,elapsed/double(iterations),checksum);
    }
  }
}
