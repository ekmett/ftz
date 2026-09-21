// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "imports.h"
using M=ftz::m32;
using H=ftz::h32;
template<class T> using V=::native::simd<T,4,arch>;
template<class T> using W=::native::wide<T,2>;
#define CHECKABLE(Name, Expression) template<class A,class B> concept Name=requires(A a,B b){Expression;}
CHECKABLE(add,a+b); CHECKABLE(sub,a-b); CHECKABLE(mul,a*b); CHECKABLE(dividable,a/b);
CHECKABLE(threeway,a<=>b); CHECKABLE(eq,a==b); CHECKABLE(ne,a!=b); CHECKABLE(lt,a<b); CHECKABLE(gt,a>b); CHECKABLE(le,a<=b); CHECKABLE(ge,a>=b);
CHECKABLE(add_assign,a+=b); CHECKABLE(sub_assign,a-=b); CHECKABLE(mul_assign,a*=b); CHECKABLE(div_assign,a/=b); CHECKABLE(assign,a=b);
CHECKABLE(fma_aab,fma(a,a,b)); CHECKABLE(fma_aba,fma(a,b,a)); CHECKABLE(fma_baa,fma(b,a,a));
CHECKABLE(angle,atan2(a,b)); CHECKABLE(sign, copysign(a,b)); CHECKABLE(minimum,min(a,b)); CHECKABLE(maximum,max(a,b));
CHECKABLE(scale,scaleb(a,b));
#undef CHECKABLE

template<class A,class B> consteval bool separate() {
  static_assert(!add<A,B> && !sub<A,B> && !mul<A,B> && !dividable<A,B>);
  static_assert(!threeway<A,B> && !eq<A,B> && !ne<A,B> && !lt<A,B> && !gt<A,B> && !le<A,B> && !ge<A,B>);
  static_assert(!add_assign<A,B> && !sub_assign<A,B> && !mul_assign<A,B> && !div_assign<A,B> && !assign<A,B>);
  static_assert(!fma_aab<A,B> && !fma_aba<A,B> && !fma_baa<A,B>);
  static_assert(!angle<A,B> && !sign<A,B> && !minimum<A,B> && !maximum<A,B> && !scale<A,B>);
  return true;
}
static_assert(separate<M,H>() && separate<H,M>());
static_assert(separate<V<M>,V<H>>() && separate<V<H>,V<M>>());
static_assert(separate<V<M>,H>() && separate<V<H>,M>());
static_assert(separate<M,V<H>>() && separate<H,V<M>>());
static_assert(separate<W<M>,W<H>>() && separate<W<H>,W<M>>());
static_assert(separate<W<V<M>>,W<V<H>>>() && separate<W<V<H>>,W<V<M>>>());
static_assert(separate<W<V<M>>,H>() && separate<W<V<H>>,M>());

template<class A,class B> concept selected=requires(typename A::mask m,A a,B b){select(m,a,b);};
template<class A,class B> concept masked_exponent=requires(typename A::mask m,A a,B b){masked_scaleb(m,a,a,b);};
template<class A,class B> concept masked_prior=requires(typename A::mask m,A a,B b){masked_scaleb(m,b,a,a);};
template<class A,class B> concept masked_value=requires(typename A::mask m,A a,B b){masked_scaleb(m,a,b,a);};
template<class A,class B> concept masked_zero=requires(typename A::mask m,A a,B b){masked_scaleb_zero(m,a,b);};
template<class A,class B> concept swizzle_assign=requires(A a,B b){a.xy=b.xy;};
template<class A,class B> concept loaded=requires(B const * p){native::load_simd<A>(p);};
template<class A,class B> concept stored=requires(B * p,A a){native::store_simd(p,a);};
template<class A,class B> consteval bool mixed_vectors() {
  static_assert(!selected<A,B> && !masked_exponent<A,B> && !masked_prior<A,B> && !masked_value<A,B> && !masked_zero<A,B>);
  static_assert(!swizzle_assign<A,B>);
  static_assert(!loaded<A,typename B::value_type> && !stored<A,typename B::value_type>);
  return true;
}
static_assert(mixed_vectors<V<M>,V<H>>() && mixed_vectors<V<H>,V<M>>());
static_assert(selected<V<M>,V<M>> && selected<V<H>,V<H>>);
static_assert(masked_exponent<V<M>,V<M>> && masked_exponent<V<H>,V<H>>);
static_assert(swizzle_assign<V<M>,V<M>> && swizzle_assign<V<H>,V<H>>);

template<class R> consteval bool retained() {
  static_assert(requires(R a) {
    {a+a}->std::same_as<R>; {a-a}->std::same_as<R>; {a*a}->std::same_as<R>; {a/a}->std::same_as<R>;
    {fma(a,a,a)}->std::same_as<R>; {sqrt(a)}->std::same_as<R>;
    {sin(a)}->std::same_as<R>; {cos(a)}->std::same_as<R>; {exp(a)}->std::same_as<R>; {expm1(a)}->std::same_as<R>;
    {sincos(a)}->std::same_as<std::pair<R,R>>;
  });
  return true;
}
static_assert(retained<M>() && retained<H>());
static_assert(retained<V<M>>() && retained<V<H>>());
static_assert(retained<W<M>>() && retained<W<H>>());
static_assert(retained<W<V<M>>>() && retained<W<V<H>>>());
static_assert(std::constructible_from<M,H> && std::constructible_from<H,M>);
static_assert(!std::convertible_to<M,H> && !std::convertible_to<H,M>);
int main(){return 0;}
