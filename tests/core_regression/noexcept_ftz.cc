#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include "support/imports.h"

namespace {
  struct events { int conversions = 0; int cleanups = 0; };
  struct cleanup {
    events * state;
    ~cleanup() noexcept { ++state->cleanups; }
  };
  struct conversion_failure {};

  void check(bool condition) {
    if (!condition) {
      std::fputs("FTZ conversion noexcept/side-effect check failed\n", stderr);
      std::abort();
    }
  }

  // The named adapter parameter is an lvalue, even when its caller supplied
  // an rvalue. Deliberately different ref-qualified promises catch that detail.
  template<class V, bool Throws>
  struct conversion {
    events * state;
    operator V() & noexcept(!Throws) {
      ++state->conversions;
      cleanup guard{state};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
      if constexpr (Throws) throw conversion_failure{};
#endif
      return V(2.f);
    }
    operator V() && noexcept(Throws) { return V(2.f); }
  };

  template<bool Throws>
  struct lane_conversion {
    events * state;
    operator ftz::ftz32() && noexcept { return ftz::ftz32(2.f); }
    operator float() & noexcept(!Throws) {
      ++state->conversions;
      cleanup guard{state};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
      if constexpr (Throws) throw conversion_failure{};
#endif
      return 2.f;
    }
  };

  template<class V, class X, std::size_t... I>
  V lanes(X value, std::index_sequence<I...>)
    noexcept(noexcept(V((static_cast<void>(I), value)...))) {
    return V((static_cast<void>(I), value)...);
  }

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  template<class F>
  void catches(events & state, F action) {
    auto const before = state;
    bool caught = false;
    try { action(); }
    catch (conversion_failure const &) { caught = true; }
    check(caught && state.conversions == before.conversions + 1 &&
      state.cleanups == before.cleanups + 1);
  }
#endif

  template<std::size_t N>
  void vector_checks() {
    using V = ::native::simd<ftz::ftz32,N,FTZ_TEST_ARCH>;
    using R = ::native::simd<float,N,FTZ_TEST_ARCH>;
    using M = typename V::mask;
    using bad = conversion<V,true>;
    using good = conversion<V,false>;
    using bad_exponent = conversion<R,true>;
    using good_exponent = conversion<R,false>;
    static_assert(std::convertible_to<bad,V> && std::convertible_to<good,V>);
    static_assert(!noexcept(std::declval<V>() + std::declval<bad>()));
    static_assert(noexcept(std::declval<V>() + std::declval<good>()));
    static_assert(!noexcept(std::declval<bad>() - std::declval<V>()));
    static_assert(noexcept(std::declval<good>() - std::declval<V>()));
    static_assert(!noexcept(std::declval<V>() * std::declval<bad>()));
    static_assert(noexcept(std::declval<V>() * std::declval<good>()));
    static_assert(!noexcept(std::declval<bad>() / std::declval<V>()));
    static_assert(noexcept(std::declval<good>() / std::declval<V>()));
    static_assert(!noexcept(std::declval<V>() == std::declval<bad>()));
    static_assert(noexcept(std::declval<V>() == std::declval<good>()));
    static_assert(!noexcept(fma(std::declval<V>(),std::declval<bad>(),std::declval<V>())));
    static_assert(noexcept(fma(std::declval<V>(),std::declval<good>(),std::declval<V>())));
    static_assert(!noexcept(fma(std::declval<bad>(),std::declval<V>(),std::declval<V>())));
    static_assert(!noexcept(fma(std::declval<bad>(),std::declval<bad>(),std::declval<V>())));
    static_assert(!noexcept(masked_scaleb(std::declval<M>(),std::declval<V>(),
      std::declval<bad>(),std::declval<R>())));
    static_assert(!noexcept(masked_scaleb(std::declval<M>(),std::declval<bad>(),
      std::declval<V>(),std::declval<R>())));
    static_assert(!noexcept(masked_scaleb(std::declval<M>(),std::declval<V>(),
      std::declval<V>(),std::declval<bad_exponent>())));
    static_assert(noexcept(masked_scaleb(std::declval<M>(),std::declval<V>(),
      std::declval<V>(),std::declval<good_exponent>())));
    static_assert(!noexcept(masked_scaleb_zero(std::declval<M>(),std::declval<V>(),
      std::declval<bad_exponent>())));
    static_assert(noexcept(masked_scaleb_zero(std::declval<M>(),std::declval<V>(),
      std::declval<good_exponent>())));
    static_assert(!noexcept(scaleb(std::declval<V>(),std::declval<bad_exponent>())));
    static_assert(noexcept(scaleb(std::declval<V>(),std::declval<good_exponent>())));
    static_assert(!noexcept(fma(std::declval<R>(),std::declval<ftz::ftz32>(),
      std::declval<bad>())));
    static_assert(noexcept(fma(std::declval<R>(),std::declval<ftz::ftz32>(),
      std::declval<good>())));
    static_assert(noexcept(std::declval<V>() + std::declval<V>()));
    static_assert(noexcept(fma(std::declval<V>(),std::declval<V>(),std::declval<V>())));
    static_assert(noexcept(scaleb(std::declval<V>(),std::declval<R>())));

    events state;
    V value(1.f);
    M active(true);
    // Ignore numerical results on purpose: observable user conversions must
    // remain even when the caller discards the fixed arithmetic result.
    (void)(value + good{&state});
    (void)(good{&state} + value);
    (void)(value - good{&state});
    (void)(good{&state} - value);
    (void)(value * good{&state});
    (void)(good{&state} * value);
    (void)(value / good{&state});
    (void)(good{&state} / value);
    (void)(value < good{&state});
    (void)(good{&state} < value);
    (void)(value > good{&state});
    (void)(good{&state} > value);
    (void)(value == good{&state});
    (void)(good{&state} == value);
    (void)(value != good{&state});
    (void)(good{&state} != value);
    (void)(value <= good{&state});
    (void)(good{&state} <= value);
    (void)(value >= good{&state});
    (void)(good{&state} >= value);
    (void)fma(value,good{&state},value);
    (void)fma(good{&state},value,value);
    (void)fma(good{&state},good{&state},value);
    (void)masked_scaleb(active,value,good{&state},R(1.f));
    (void)masked_scaleb(active,good{&state},value,R(1.f));
    (void)masked_scaleb(active,value,value,good_exponent{&state});
    (void)masked_scaleb_zero(active,value,good_exponent{&state});
    (void)scaleb(value,good_exponent{&state});
    (void)fma(R(1.f),ftz::ftz32(1.f),good{&state});
    check(state.conversions == 30 && state.cleanups == 30);
    if constexpr (N>1) {
      static_assert(!noexcept(lanes<V>(lane_conversion<true>{nullptr},std::make_index_sequence<N>{})));
      static_assert(noexcept(lanes<V>(lane_conversion<false>{nullptr},std::make_index_sequence<N>{})));
      (void)lanes<V>(lane_conversion<false>{&state},std::make_index_sequence<N>{});
      check(state.conversions == 30 + N && state.cleanups == 30 + N);
    }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    catches(state,[&] { (void)(value + bad{&state}); });
    catches(state,[&] { (void)(bad{&state} - value); });
    catches(state,[&] { (void)(value * bad{&state}); });
    catches(state,[&] { (void)(bad{&state} / value); });
    catches(state,[&] { (void)(value == bad{&state}); });
    catches(state,[&] { (void)fma(value,bad{&state},value); });
    catches(state,[&] { (void)fma(bad{&state},value,value); });
    catches(state,[&] { (void)fma(bad{&state},bad{&state},value); });
    catches(state,[&] { (void)masked_scaleb(active,value,bad{&state},R(1.f)); });
    catches(state,[&] { (void)masked_scaleb(active,bad{&state},value,R(1.f)); });
    catches(state,[&] { (void)masked_scaleb(active,value,value,bad_exponent{&state}); });
    catches(state,[&] { (void)masked_scaleb_zero(active,value,bad_exponent{&state}); });
    catches(state,[&] { (void)scaleb(value,bad_exponent{&state}); });
    catches(state,[&] { (void)fma(R(1.f),ftz::ftz32(1.f),bad{&state}); });
    if constexpr (N>1)
      catches(state,[&] { (void)lanes<V>(lane_conversion<true>{&state},std::make_index_sequence<N>{}); });
#endif
  }

  void scalar_checks() {
    using F = ftz::ftz32;
    using bad = conversion<F,true>;
    static_assert(noexcept(std::declval<F>() + 1.f));
    static_assert(noexcept(ftz::fma(std::declval<F>(),1.f,2.f)));
    // Scalar adapters are restricted to built-in arithmetic. A user conversion
    // into a fixed ftz32 parameter happens before entering that noexcept body.
    static_assert(!noexcept(std::declval<bad &>() + std::declval<F>()));
    static_assert(!noexcept(ftz::sin(std::declval<bad &>())));
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    events state;
    bad value{&state};
    catches(state,[&] { (void)(value + F(1.f)); });
    catches(state,[&] { (void)ftz::sin(value); });
#endif
  }
}

int main() {
  scalar_checks();
  vector_checks<1>();
  vector_checks<2>();
  vector_checks<3>();
#if defined(__AVX2__) || defined(__ARM_NEON)
  vector_checks<4>();
#endif
#if defined(__AVX2__)
  vector_checks<8>();
#endif
#if defined(__AVX512F__) && defined(__AVX512DQ__)
  vector_checks<16>();
#endif
  std::puts("FTZ generic conversion promises and side effects passed");
}

// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
