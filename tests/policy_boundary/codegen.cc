// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "imports.h"
#if REVIEW_BASELINE
using manual_type=ftz::ftz32;
using hardware_type=ftz::ftz32;
#else
using manual_type=ftz::m32;
using hardware_type=ftz::h32;
#endif
using R=simd::vec<float,8,arch>;
using HM=simd::vec<hardware_type,8,arch>;
using MM=simd::vec<manual_type,8,arch>;
extern "C" [[gnu::noinline]] std::uint32_t hardware_add(hardware_type a,hardware_type b){return (a+b).to_bits();}
extern "C" [[gnu::noinline]] std::uint32_t hardware_sub(hardware_type a,hardware_type b){return (a-b).to_bits();}
extern "C" [[gnu::noinline]] std::uint32_t manual_add(manual_type a,manual_type b){return (a+b).to_bits();}
extern "C" [[gnu::noinline]] std::uint32_t manual_sub(manual_type a,manual_type b){return (a-b).to_bits();}
template<class V> void add(float * out,float const * a,float const * b) {
  (V::unsafe_from_float32(R::loadu(a))+V::unsafe_from_float32(R::loadu(b))).to_native().storeu(out);
}
template<class V> void sub(float * out,float const * a,float const * b) {
  (V::unsafe_from_float32(R::loadu(a))-V::unsafe_from_float32(R::loadu(b))).to_native().storeu(out);
}
template<class V,char Op> void wide_op(float * out,float const * a,float const * b,float const * c) {
  auto get=[](float const * p) {return simd::wide{V::unsafe_from_float32(R::loadu(p)),V::unsafe_from_float32(R::loadu(p+8)),V::unsafe_from_float32(R::loadu(p+16))};};
  auto x=get(a),y=get(b);
  auto z=[&]{if constexpr(Op=='+')return x+y;else if constexpr(Op=='-')return x-y;else if constexpr(Op=='*')return x*y;else return fma(x,y,get(c));}();
  z.registers[0].to_native().storeu(out);z.registers[1].to_native().storeu(out+8);z.registers[2].to_native().storeu(out+16);
}
extern "C" [[gnu::noinline]] void hardware_vec_add(float*o,float const*a,float const*b){add<HM>(o,a,b);}
extern "C" [[gnu::noinline]] void hardware_vec_sub(float*o,float const*a,float const*b){sub<HM>(o,a,b);}
extern "C" [[gnu::noinline]] void manual_vec_add(float*o,float const*a,float const*b){add<MM>(o,a,b);}
extern "C" [[gnu::noinline]] void manual_vec_sub(float*o,float const*a,float const*b){sub<MM>(o,a,b);}
#define LEAF(Name,Type,Op) extern "C" [[gnu::noinline]] void Name(float*o,float const*a,float const*b,float const*c){wide_op<Type,Op>(o,a,b,c);}
LEAF(hardware_wide_add,HM,'+') LEAF(hardware_wide_sub,HM,'-') LEAF(hardware_wide_mul,HM,'*') LEAF(hardware_wide_fma,HM,'f')
LEAF(manual_wide_add,MM,'+') LEAF(manual_wide_sub,MM,'-') LEAF(manual_wide_mul,MM,'*') LEAF(manual_wide_fma,MM,'f')
#undef LEAF
