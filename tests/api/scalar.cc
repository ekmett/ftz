// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <bit>
#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <utility>
import ftz;

//! [scalar_import]
using F = ftz::m32;
auto canonical_zero = F::from_bits(0x80000001u); // Negative subnormal -> -0.
auto known = F::unsafe_from_float32(1.5f);       // Already canonical.
float ordinary = known;                        // Leaves the FTZ type contract.
auto hardware_value = ftz::h32::from_bits(known.to_bits()); // Explicit policy choice.
//! [scalar_import]

static_assert(sizeof(F) == sizeof(float));
static_assert(std::is_trivially_copyable_v<F>);
static_assert(F{}.to_bits() == 0u);
static_assert(F::from_bits(0x80000001u).to_bits() == 0x80000000u);

template<class T> bool math_example() {
  //! [scalar_math]
  T zero(0.f), one(1.f), negative_one(-1.f);
  auto fused = fma(one, T(2.f), T(1.f));
  auto [sine, cosine] = sincos(zero); // std::pair<T,T>, sine first.
  auto minus_infinity = log(zero);
  auto pole = log1p(negative_one);
  auto invalid = sqrt(negative_one);
  auto down = floor(T(-.25f));
  auto toward_zero = trunc(T(-.25f)); // Preserves negative zero.
  //! [scalar_math]
  static_assert(std::is_same_v<decltype(sincos(zero)),std::pair<T,T>>);
  return fused.to_bits()==0x40400000u && sine.to_bits()==0 && cosine.to_bits()==0x3f800000u &&
    minus_infinity.to_bits()==0xff800000u && pole.to_bits()==0xff800000u && isnan(invalid) &&
    down.to_bits()==0xbf800000u && toward_zero.to_bits()==0x80000000u &&
    exp(zero).to_bits()==one.to_bits() && expm1(zero).to_bits()==0 &&
    tanh(zero).to_bits()==0 && log(one).to_bits()==0 &&
    atan2(zero,negative_one).to_bits()==0x40490fdbu;
}

int main() {
  if (canonical_zero.to_bits()!=0x80000000u || hardware_value.to_bits()!=known.to_bits()) return 1;
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    if (!ftz::probe_ftz32_cpu<ftz::m32>().admitted() || !math_example<ftz::m32>()) return 2;
  }
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    if (!ftz::probe_ftz32_cpu<ftz::h32>().admitted() || !math_example<ftz::h32>()) return 3;
  }
  //! [scalar_bits]
  auto payload = F::from_bits(0x7fc12345u);
  auto signed_payload = copysign(payload, F::from_bits(0x80000000u));
  bool classified = isnan(signed_payload) && signbit(signed_payload);
  auto bits = signed_payload.to_bits(); // 0xffc12345: transport preserves payload.
  //! [scalar_bits]
  if (!classified || bits!=0xffc12345u) return 4;
  std::puts("Scalar imports, policies, special values and math examples pass");
}
