// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>
#include <iterator>
int main(int argc,char **argv) {
  if(argc!=3) {std::fprintf(stderr,"usage: run shader.metal input-expected.bin\n");return 2;}
  @autoreleasepool {
    NSError *error=nil;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    if(!device) return 77;
    NSString *source=[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&error];
    if(!source) {std::fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
    MTLCompileOptions *options=[MTLCompileOptions new];
    options.mathMode=MTLMathModeSafe;
    options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
    id<MTLLibrary> library=[device newLibraryWithSource:source options:options error:&error];
    if(!library) {std::fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 3;}
    id<MTLFunction> function=[library newFunctionWithName:@"exp_check"];
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:function error:&error];
    if(!pipeline) {std::fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 4;}
    std::ifstream stream(argv[2],std::ios::binary);
    std::vector<char> raw((std::istreambuf_iterator<char>(stream)),{});
    if(raw.empty() || raw.size()%8) return 5;
    auto count=raw.size()/8;
    auto input=(std::uint32_t const *)raw.data();
    id<MTLBuffer> a=[device newBufferWithBytes:raw.data() length:raw.size() options:MTLResourceStorageModeShared];
    id<MTLBuffer> b=[device newBufferWithLength:count*4 options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue=[device newCommandQueue];
    id<MTLCommandBuffer> command=[queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];
    [encoder setBuffer:a offset:0 atIndex:0];
    [encoder setBuffer:b offset:0 atIndex:1];
    // SPIRV-Cross requires a buffer-size table for StructuredBuffer.GetDimensions.
    std::uint32_t sizes[2]={std::uint32_t(raw.size()),std::uint32_t(count*4)};
    [encoder setBytes:sizes length:sizeof(sizes) atIndex:25];
    [encoder dispatchThreads:MTLSizeMake(count,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
    [encoder endEncoding];[command commit];[command waitUntilCompleted];
    if(command.status!=MTLCommandBufferStatusCompleted) {std::fprintf(stderr,"%s\n",command.error.localizedDescription.UTF8String);return 6;}
    auto actual=(std::uint32_t const *)b.contents;
    unsigned mismatch=0,nans=0;
    for(std::size_t i=0;i<count;++i) {
      auto expected=input[2*i+1];
      bool nan_a=(actual[i]&0x7fffffffu)>0x7f800000u;
      bool nan_e=(expected&0x7fffffffu)>0x7f800000u;
      if(nan_a && nan_e) {++nans;continue;}
      if(actual[i]!=expected) {
        if(mismatch<12) std::fprintf(stderr,"x=%08x gpu=%08x cpu=%08x\n",input[2*i],actual[i],expected);
        ++mismatch;
      }
    }
    std::printf("device=%s inputs=%zu mismatches=%u NaN-pairs=%u\n",device.name.UTF8String,count,mismatch,nans);
    return mismatch ? 1:0;
  }
}
