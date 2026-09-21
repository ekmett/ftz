// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "contract.h"
#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <memory>
#include "support/guarded_pages.h"

#include "support/imports.h"

namespace {
  constexpr std::size_t lanes = FTZ_TEST_PROFILE / 32;
  using V = ::native::simd<ftz::ftz32, lanes,FTZ_TEST_ARCH>;
  using F = ::native::simd<float, lanes,FTZ_TEST_ARCH>;
  static_assert(sizeof(V) == sizeof(float) * lanes);
  static_assert(std::is_trivially_copyable_v<V>);
  using packed_values = ::native::wide<V,3>;
  static_assert(std::same_as<decltype(exp(std::declval<packed_values const &>())),packed_values>);
  static_assert(std::same_as<decltype(sincos(std::declval<packed_values const &>())),
    std::pair<packed_values,packed_values>>);
  static_assert(std::same_as<decltype(ftz::sincos(ftz::ftz32{})),
    std::pair<ftz::ftz32, ftz::ftz32>>);

  std::array<float, profile_test::count> inputs() {
    constexpr std::uint32_t edges[] = {
      0, 0x80000000u, 1, 0x80000001u, 0x007fffffu, 0x807fffffu,
      0x00800000u, 0x80800000u, 0x00800001u, 0x80800001u,
      0x3f800000u, 0xbf800000u, 0x3f000000u, 0xbf000000u,
      0x33800000u, 0xb3800000u, 0x35800000u, 0xb5800000u,
      0x3fc90fdbu, 0xbfc90fdbu, 0x40490fdbu, 0xc0490fdbu,
      0xc2aeac4fu, 0xc2aeac50u, 0x42b17217u, 0x42b17218u,
      0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u,
      0x7fc01234u, 0xffc05678u
    };
    std::array<float, profile_test::count> result;
    for (std::size_t i = 0; i < std::size(edges); ++i)
      result[i] = std::bit_cast<float>(edges[i]);
    std::uint32_t seed = 0xa2395e11u;
    for (std::size_t i = std::size(edges); i < result.size(); ++i) {
      seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
      result[i] = float(int(seed & 0xffffu) - 32768) * 0x1p-10f;
    }
    return result;
  }

  template<class R> void store_column(std::uint32_t * out, std::size_t column,
      std::size_t offset, R value) {
    value.store_bits(out + column * profile_test::count + offset);
  }

  template<std::size_t N> bool tails() {
    using R = ::native::simd<ftz::ftz32, N,FTZ_TEST_ARCH>;
    ftz::test::guarded_pages input_page, output_page;
    for (std::size_t n = 0; n <= N; ++n) {
      auto * input = reinterpret_cast<float *>(input_page.end()) - n;
      auto * output = reinterpret_cast<float *>(output_page.end()) - n;
      for (std::size_t i = 0; i < n; ++i) {
        std::construct_at(input + i, float(int(i) - 3));
        std::construct_at(output + i, 19.0f);
      }
      std::construct_at(output - 1, 19.0f);
      auto value = R::load_partial(input, n, 17);
      std::array<float, N> loaded;
      value.storeu(loaded.data());
      for (std::size_t i = 0; i < N; ++i)
        if (loaded[i] != (i < n ? input[i] : 17)) return false;
      value.store_partial(output, n);
      if (output[-1] != 19) return false;
      for (std::size_t i = 0; i < n; ++i)
        if (output[i] != input[i]) return false;
    }
    auto empty = R::load_partial(static_cast<float const *>(nullptr), 0, 17);
    empty.store_partial(static_cast<float *>(nullptr), 0);
    std::array<float, N> loaded;
    empty.storeu(loaded.data());
    for (float value : loaded) if (value != 17) return false;
    return true;
  }

}

extern "C" std::size_t FTZ_TEST_ENTRY(std::uint32_t * out, std::size_t capacity) {
  if (capacity < profile_test::words || (!tails<4>() || !tails<lanes>())) return 0;
  auto input = inputs();
  for (std::size_t i = 0; i < input.size(); ++i) {
    auto x = ftz::ftz32::from_float(input[i]);
    std::array results{ftz::sin(x), ftz::cos(x), ftz::exp(x), ftz::expm1(x),
      ftz::log(x), ftz::log1p(x), ftz::tanh(x), ftz::sqrt(x),
      ftz::atan2(x, ftz::ftz32(1)), ftz::fma(x, x, x)};
    for (std::size_t op = 0; op < results.size(); ++op)
      out[op * profile_test::count + i] = results[op].to_bits();
  }
  ::native::wide<V, profile_test::count / lanes> packed;
  for (std::size_t i = 0; i < packed.registers.size(); ++i)
    packed.registers[i] = V::loadu(input.data() + i * lanes);
  auto exponential = ::native::exp(packed);
  auto exp_minus_one = native::expm1(packed);
  auto [sine, cosine] = native::sincos(packed);
  for (std::size_t i = 0; i < packed.registers.size(); ++i) {
    auto offset = i * lanes;
    store_column(out, 10, offset, exponential.registers[i]);
    store_column(out, 11, offset, exp_minus_one.registers[i]);
    store_column(out, 12, offset, sine.registers[i]);
    store_column(out, 13, offset, cosine.registers[i]);
    auto x = packed.registers[i];
    auto positive = abs(x);
    auto denominator = positive + V(1);
    store_column(out, 14, offset, x + V(0.5f));
    store_column(out, 15, offset, x * V(0.5f));
    store_column(out, 16, offset, x / denominator);
    store_column(out, 17, offset, fma(x, V(0.5f), V(-1)));
    store_column(out, 18, offset, sqrt(positive));
    store_column(out, 19, offset, select(x > V(0), x, -x));
    std::array<float, lanes> raw_input;
    for (std::size_t j = 0; j < lanes; ++j)
      raw_input[j] = float(int(offset + j) - 48) * .25f;
    store_column(out, 20, offset, ::native::exp(::native::wide<F, 1>{F::loadu(raw_input.data())}).registers[0]);
    // Compile and execute the direct-register adapter as well as the wide path.
    std::array<std::uint32_t, lanes> direct;
    ::native::exp(F::loadu(raw_input.data())).store_bits(direct.data());
    for (std::size_t j = 0; j < lanes; ++j)
      if (direct[j] != out[20 * profile_test::count + offset + j]) return 0;
    using I = ::native::simd<std::uint32_t, lanes,FTZ_TEST_ARCH>;
    auto integer = I::loadu(out + 19 * profile_test::count + offset);
    ((integer + I(3u)) ^ I(0x9e3779b9u)).storeu(out + 21 * profile_test::count + offset);
    store_column(out, 22, offset, masked_scaleb_zero(
      x > V(0), x, V(-1)));
  }
  for (std::size_t i = 0; i < profile_test::words; ++i) {
    // NaN payload/sign equivalence is only for numerical columns, never integer bits.
    if (i / profile_test::count != 21 && (out[i] & 0x7fffffffu) > 0x7f800000u)
      out[i] = 0x7fc00000u;
  }
  return profile_test::words;
}
