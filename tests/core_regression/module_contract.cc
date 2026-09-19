#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <type_traits>
#include <utility>
#include "support/imports.h"

static_assert(std::same_as<decltype(simd::vec<float,1,FTZ_TEST_ARCH>{1.0f}), simd::vec<float, 1,FTZ_TEST_ARCH>>);
static_assert(sizeof(ftz::ftz32) == sizeof(float));
static_assert(std::is_trivially_copyable_v<ftz::ftz32>);
static_assert(std::same_as<decltype(ftz::sincos(ftz::ftz32{})),
  std::pair<ftz::ftz32, ftz::ftz32>>);

void emit(std::ostream & out, std::uint32_t bits) {
  // NaN payload and sign are explicitly outside the reproducibility contract.
  if ((bits & 0x7fffffffu) > 0x7f800000u) bits = 0x7fc00000u;
  out.write(reinterpret_cast<char const *>(&bits), sizeof(bits));
}

template<std::size_t N> void capture(std::ostream & out) {
  using V = simd::vec<ftz::ftz32, N,FTZ_TEST_ARCH>;
  using F = simd::vec<float, N,FTZ_TEST_ARCH>;
  using I = simd::vec<std::uint32_t, N,FTZ_TEST_ARCH>;
  std::uint32_t seed = 0x893bfd12u;
  for (std::size_t step = 0; step < 512; ++step) {
    std::array<float, N> input{};
    for (auto & x : input) {
      seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
      x = float(int(seed & 0xffffu) - 32768) * 0x1p-10f;
    }
    auto a = V::loadu(input.data());
    simd::wide<V, 3> values{a, a * V(.5f), a * V(.25f)};
    auto [...lanes] = values;
    auto reconstructed = simd::wide{lanes...};
    auto result = exp(reconstructed);
    auto [s, c] = sincos(reconstructed);
    auto m = expm1(reconstructed);
    for (std::size_t i = 0; i < 3; ++i) {
      for (auto v : {result.registers[i], s.registers[i], c.registers[i], m.registers[i]}) {
        std::array<std::uint32_t, N> words{};
        v.store_bits(words.data());
        for (auto word : words) emit(out, word);
      }
    }
    auto raw = exp(simd::wide<F, 2>{F::loadu(input.data())});
    for (auto v : raw.registers) {
      std::array<std::uint32_t, N> words{};
      v.store_bits(words.data());
      for (auto word : words) emit(out, word);
    }
    auto mask = a > V(0);
    auto chosen = select(mask, a, -a);
    std::array<std::uint32_t, N> words{};
    chosen.store_bits(words.data());
    for (auto word : words) emit(out, word);
    auto integer = I::loadu(words.data());
    auto incremented = integer + I(3u);
    incremented.storeu(words.data());
    for (auto word : words) out.write(reinterpret_cast<char const *>(&word), sizeof(word));
  }
}

int main(int argc, char ** argv) {
  if (argc != 2) return 2;
  std::ofstream out(argv[1], std::ios::binary);
  if (!out) return 3;
  auto before = ftz::read_native_fp_state();
  {
    ftz::native_fp32_scope controls(ftz::native_fp32_mode::flush);
    if (!ftz::probe_ftz32_cpu().admitted()) return 4;
    capture<1>(out);
#if defined(__AVX2__) || defined(__ARM_NEON)
    capture<4>(out);
#endif
#if defined(__AVX2__)
    capture<8>(out);
#endif
#if defined(__AVX512F__) && defined(__AVX512DQ__)
    capture<16>(out);
#endif
    for (auto bits : {0u, 0x80000000u, 1u, 0x807fffffu, 0x00800000u,
      0x3f800000u, 0xbf800000u, 0x7f800000u, 0xff800000u, 0x7fc00000u}) {
      auto x = ftz::ftz32::from_bits(bits);
      for (auto y : {ftz::sin(x), ftz::cos(x), ftz::exp(x), ftz::expm1(x),
        ftz::tanh(x), ftz::log(x), ftz::log1p(x), ftz::sqrt(x),
        ftz::atan2(x, ftz::ftz32(1)), ftz::fma(x, x, x)}) emit(out, y.to_bits());
    }
    if (!controls.controls_match()) return 5;
  }
  if (before != ftz::read_native_fp_state()) return 6;
  out.close();
  return out ? 0 : 7;
}

// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
