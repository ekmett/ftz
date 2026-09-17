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
template<class T> using V=simd::vec<T,8,arch>;
template<class T> [[gnu::always_inline]] inline auto read(float const *p){
  return std::array{V<T>::unsafe_from_float32(R::loadu(p)),V<T>::unsafe_from_float32(R::loadu(p+8)),V<T>::unsafe_from_float32(R::loadu(p+16))};
}
template<class T> [[gnu::always_inline]] inline void write(float *p,std::array<V<T>,3> const & a){
  a[0].to_native().storeu(p);a[1].to_native().storeu(p+8);a[2].to_native().storeu(p+16);
}
#define LEAF(Name,T,Operation) extern "C" [[gnu::noinline,gnu::flatten]] void Name(float *o,float const*a,float const*b){write<T>(o,ftz::Operation(read<T>(a),read<T>(b)));}
LEAF(manual_array_add,manual_type,add)
LEAF(manual_array_sub,manual_type,sub)
LEAF(manual_array_mul,manual_type,mul)
LEAF(hardware_array_add,hardware_type,add)
LEAF(hardware_array_sub,hardware_type,sub)
LEAF(hardware_array_mul,hardware_type,mul)
#undef LEAF
#define LEAF(Name,T) extern "C" [[gnu::noinline,gnu::flatten]] void Name(float *o,float const*a,float const*b,float const*c){write<T>(o,ftz::fma(read<T>(a),read<T>(b),read<T>(c)));}
LEAF(manual_array_fma,manual_type)
LEAF(hardware_array_fma,hardware_type)
#undef LEAF
