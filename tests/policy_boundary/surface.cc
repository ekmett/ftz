// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstdio>
#include "imports.h"
#define UNARY(Op) template<class R> concept has_##Op=requires(R a){{Op(a)}->std::same_as<R>;};
UNARY(sqrt) UNARY(sin) UNARY(cos) UNARY(exp) UNARY(expm1) UNARY(log) UNARY(log1p) UNARY(tanh)
#undef UNARY
template<class R> concept has_atan2=requires(R a){{atan2(a,a)}->std::same_as<R>;};
template<class R> void show(char const* name){std::printf("%s sqrt=%d sin=%d cos=%d exp=%d expm1=%d log=%d log1p=%d tanh=%d atan2=%d\n",name,has_sqrt<R>,has_sin<R>,has_cos<R>,has_exp<R>,has_expm1<R>,has_log<R>,has_log1p<R>,has_tanh<R>,has_atan2<R>);}
template<class T> void check(){
 using R=::native::simd<T,4,arch>;using W=::native::wide<T,2>;using WV=::native::wide<R,2>;
 static_assert(has_log<T> && has_log1p<T> && has_tanh<T> && has_atan2<T>);
 static_assert(has_log<W> && has_log1p<W> && has_tanh<W> && has_atan2<W>);
 static_assert(has_log<R> && has_log1p<R> && has_tanh<R> && has_atan2<R>);
 static_assert(has_log<WV> && has_log1p<WV> && has_tanh<WV> && has_atan2<WV>);
 // Instantiate the available scalar-wide paths, beyond return-type detection.
 auto x=W::broadcast(T(1.0f));auto a=log(x),b=log1p(x),c=tanh(x);(void)a;(void)b;(void)c;
 show<T>("scalar");show<R>("vec4");show<W>("wide<scalar,2>");show<WV>("wide<vec4,2>");
}
int main(){
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);check<ftz::m32>();}
  {ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);check<ftz::h32>();}
}
