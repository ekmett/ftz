// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
import ftz;
import simd.scalar;
import simd;

namespace fixture {
  // A separate test-only value domain. It has no implicit float conversion and
  // supplies no ISA-specific declarations or public FTZ policy.
  struct second_scalar { float value = 0.f; };
}

namespace simd {
  template <> struct simd_traits<fixture::second_scalar> { using storage_type = float; };
  template <class Raw, class Self>
  struct simd_customization<fixture::second_scalar,Raw,Self> {
    using value_type = fixture::second_scalar;
    using register_type = Raw;
    using mask_type = typename Raw::mask;
    using mask = mask_type;
    static constexpr std::size_t lanes = Raw::lanes;
    simd_customization() noexcept = default;
    explicit simd_customization(value_type value) noexcept : raw_(value.value) {}
    explicit simd_customization(Raw value) noexcept : raw_(value) {}
    Raw to_native() const noexcept { return raw_; }
    friend Self operator+(Self a,Self b) noexcept { return Self(a.raw_ + b.raw_); }
    friend mask operator<(Self a,Self b) noexcept { return a.raw_ < b.raw_; }
  private:
    Raw raw_{};
  };
}

template <class Arch,std::size_t N> constexpr bool independent_elements() {
  using R = simd::vec<float,N,Arch>;
  using F = simd::vec<ftz::ftz32,N,Arch>;
  using S = simd::vec<fixture::second_scalar,N,Arch>;
  static_assert(std::same_as<typename F::value_type,ftz::ftz32>);
  static_assert(std::same_as<typename S::value_type,fixture::second_scalar>);
  static_assert(std::same_as<typename F::register_type,R>);
  static_assert(std::same_as<typename S::register_type,R>);
  static_assert(std::same_as<typename R::template rebind<ftz::ftz32>,F>);
  static_assert(std::same_as<typename R::template rebind<fixture::second_scalar>,S>);
  static_assert(std::same_as<typename F::mask,typename R::mask>);
  static_assert(std::same_as<typename S::mask,typename R::mask>);
  static_assert(std::same_as<decltype(std::declval<S>()+std::declval<S>()),S>);
  static_assert(std::same_as<decltype(std::declval<S>()<std::declval<S>()),typename R::mask>);
  static_assert(sizeof(S)==sizeof(R) && sizeof(F)==sizeof(R));
  if constexpr (N==1)
    static_assert(std::same_as<decltype(simd::vec{Arch{},fixture::second_scalar{}}),
      simd::vec<fixture::second_scalar,1,Arch>>);
  else
    static_assert(std::constructible_from<S,Arch,fixture::second_scalar>);
  return true;
}
static_assert(independent_elements<simd::scalar,1>());
static_assert(independent_elements<simd::avx2,4>());
static_assert(independent_elements<simd::avx512,4>());
static_assert(!std::same_as<simd::vec<fixture::second_scalar,4,simd::avx2>,
  simd::vec<fixture::second_scalar,4,simd::avx512>>);

template<class Arch,std::size_t N>
auto add_second() {
  using S = simd::vec<fixture::second_scalar,N,Arch>;
  return S(Arch{},fixture::second_scalar{1.f}) + S(Arch{},fixture::second_scalar{2.f});
}
// Instantiate the dependent constructor/operator bodies as well as declarations.
// This OBJECT fixture is never linked into a baseline entry point.
template auto add_second<simd::scalar,1>();
template auto add_second<simd::avx2,4>();
template auto add_second<simd::avx512,4>();
