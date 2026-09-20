// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <array>
#include <cstdio>
#include <type_traits>
#include <utility>
import ftz;
import native;
#if API_PROFILE == 512
constexpr auto arch=::native::avx512;
#elif API_PROFILE == 128
constexpr auto arch=::native::neon;
#else
constexpr auto arch=::native::avx2;
#endif
import native.wide;

template<class F> bool vector_example() {
  //! [vector_memory]
  using V = ::native::simd<F,4,arch>;
  std::array<F,4> input{F(1.f),F(2.f),F(3.f),F(4.f)};
  auto value = ::native::load_simd<V>(input.data());
  auto snapshot = value.zyx;               // Owning vec<F,3,arch>.
  value.xy = value.yx;                      // Materialize before overlapping writes.
  std::array<F,4> output{};
  ::native::store_simd(output.data(), value);    // Exactly four typed elements.
  //! [vector_memory]
  static_assert(std::is_same_v<decltype(snapshot),::native::simd<F,3,arch>>);
  if (output!=std::array<F,4>{F(2.f),F(1.f),F(3.f),F(4.f)}) return false;
  //! [vector_masks]
  auto finite = isfinite(value);            // V::mask, not a float vector.
  static_assert(std::is_same_v<decltype(finite),typename V::mask>);
  auto positive = select(value > V(F(0.f)), value, V(F(0.f)));
  auto negative = copysign(positive, V(F(-1.f)));
  //! [vector_masks]
  if (!all(finite) || !all(signbit(negative))) return false;
  //! [array_math]
  std::array<V,2> registers{V(F(.25f)),V(F(.5f))};
  auto exponential = ftz::expm1(registers);  // std::array<V,2>.
  auto [array_sine,array_cosine] = ftz::sincos(registers);
  auto fused_registers = ftz::fma(registers, registers, registers);
  static_assert(std::is_same_v<decltype(exponential),std::array<V,2>>);
  //! [array_math]
  if (!all(isfinite(exponential[0])) || !all(isfinite(fused_registers[1]))) return false;
  //! [wide_math]
  ::native::wide<V,2> values{registers};
  auto result = expm1(values);               // ADL uses FTZ's array kernel.
  auto [sine,cosine] = sincos(values);       // A pair of wide values.
  auto masks = isfinite(result);            // One native mask per register.
  auto angles = atan2(values,values);        // Matching wide operands.
  auto integers = floor(values);
  static_assert(std::is_same_v<decltype(masks),::native::wide<typename V::mask,2>>);
  static_assert(std::is_same_v<decltype(sine),::native::wide<V,2>>);
  //! [wide_math]
  if (!all(masks.registers[0]) || !all(isfinite(angles.registers[1])) ||
      !all(integers.registers[0]==V(F(0.f)))) return false;
  return true;
}

int main() {
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    if (!ftz::probe_ftz32_cpu<ftz::m32>().admitted() || !vector_example<ftz::m32>()) return 1;
  }
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    if (!ftz::probe_ftz32_cpu<ftz::h32>().admitted() || !vector_example<ftz::h32>()) return 2;
  }
  std::puts("Typed vectors, native masks, owning swizzles, arrays and wide examples pass");
}
