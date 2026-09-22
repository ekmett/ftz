// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "math_contract.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "support/imports.h"

namespace {
  // Frozen scalar oracle for the chosen host/shader graph, with independent binary64
  // power-of-two reconstruction. It does not call FTZ or native exp/scaleb.
  std::uint32_t reference(std::uint32_t word) {
    auto const magnitude = word & 0x7fffffffu;
    if (magnitude > 0x7f800000u) return 0x7fc00000u;
    float x = std::bit_cast<float>(word);
    if (x < -87.33654022216796875f) return 0;
    if (x > 88.37625885009765625f) return 0x7f800000u;
    float const n = std::nearbyint(x * std::bit_cast<float>(0x3fb8aa3bu));
    float const first = std::fma(n, std::bit_cast<float>(0xbf317200u), x);
    float const r = std::fma(n, std::bit_cast<float>(0xb5bfbe8eu), first);
    float y = std::fma(r, std::bit_cast<float>(0x3950eb8au), std::bit_cast<float>(0x3ab6d3abu));
    for (auto coefficient : {0x3c08882eu, 0x3d2aaa32u, 0x3e2aaaabu, 0x3f000000u, 0x3f800000u, 0x3f800000u})
      y = std::fma(r, y, std::bit_cast<float>(coefficient));
    auto const scaled = std::bit_cast<std::uint32_t>(float(std::ldexp(double(y), int(n))));
    return scaled < 0x00800000u ? 0u : scaled;
  }
  std::vector<std::uint32_t> inputs() {
    std::vector<std::uint32_t> result{0, 0x80000000u, 1, 0x807fffffu, 0x00800000u,
      0x80800000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xff800001u,
      0x7f7fffffu, 0xff7fffffu, 0x42b0c0a5u, 0x42b17217u, 0x42b17218u};
    // Every binary32 input across the FTZ cutoff and across the upper interval
    // n=127/128 early-overflow transition and the old endpoint, including neighbors.
    for (std::uint32_t word = 0xc2ae0000u; word <= 0xc2b00000u; ++word) result.push_back(word);
    for (std::uint32_t word = 0x42b00000u; word <= 0x42b20000u; ++word) result.push_back(word);
    // Probe each reducer transition in the normal-output interval.
    for (int index = -126; index <= 127; ++index) {
      auto const word = std::bit_cast<std::uint32_t>(float((index + .5) * .6931471805599453094));
      for (int offset = -8; offset <= 8; ++offset) result.push_back(word + offset);
    }
    std::uint32_t state = 0x830781a3u;
    for (unsigned i = 0; i < 32768; ++i) {
      state ^= state << 13; state ^= state >> 17; state ^= state << 5;
      result.push_back(state);
    }
    return result;
  }
  template<class T, std::size_t L, std::size_t N>
  void check(std::vector<std::uint32_t> const & bank, std::vector<std::uint32_t> const & expected) {
    using R = native::simd<T, L, FTZ_TEST_ARCH>;
    using W = native::wide<R, N>;
    if constexpr (N == 0) {
      if (exp(W{}).registers.size() != 0) std::abort();
    } else {
      for (std::size_t offset = 0; offset < bank.size(); offset += L * N) {
        W input{};
        std::array<std::array<std::uint32_t, L>, N> words{};
        for (std::size_t reg = 0; reg < N; ++reg) {
          for (std::size_t lane = 0; lane < L; ++lane) words[reg][lane] = bank[(offset + reg * L + lane) % bank.size()];
          input.registers[reg] = R::load_bits(words[reg].data());
        }
        auto const value = exp(input);
        for (std::size_t reg = 0; reg < N; ++reg) {
          std::array<std::uint32_t, L> actual{};
          value.registers[reg].store_bits(actual.data());
          for (std::size_t lane = 0; lane < L; ++lane) {
            auto const want = expected[(offset + reg * L + lane) % bank.size()];
            if (!ftz::math_test::equivalent_fp32(actual[lane], want)) {
              std::fprintf(stderr, "exp L=%zu N=%zu input=%08x actual=%08x expected=%08x\n", L, N, words[reg][lane], actual[lane], want);
              std::abort();
            }
          }
          input.registers[reg].store_bits(actual.data());
          for (std::size_t lane = 0; lane < L; ++lane) {
            auto const canonical = (words[reg][lane] & 0x7f800000u) == 0 ? words[reg][lane] & 0x80000000u : words[reg][lane];
            if (actual[lane] != canonical) std::abort();
          }
        }
      }
    }
  }
  template<class T> void check_policy(std::vector<std::uint32_t> const & bank, std::vector<std::uint32_t> const & expected) {
    for (std::size_t i = 0; i < bank.size(); ++i) {
      auto const actual = exp(T::from_bits(bank[i])).to_bits();
      if (!ftz::math_test::equivalent_fp32(actual, expected[i])) {
        std::fprintf(stderr, "scalar exp input=%08x actual=%08x expected=%08x\n", bank[i], actual, expected[i]);
        std::abort();
      }
    }
    check<T, 1, 1>(bank, expected);
    check<T, 2, 3>(bank, expected);
    check<T, 3, 3>(bank, expected);
    check<T, 4, 0>(bank, expected);
    check<T, 4, 1>(bank, expected);
    check<T, 4, 3>(bank, expected);
    check<T, 4, 8>(bank, expected);
#if FTZ_TEST_PROFILE >= 256
    check<T, 8, 1>(bank, expected);
    check<T, 8, 3>(bank, expected);
    check<T, 8, 8>(bank, expected);
#endif
#if FTZ_TEST_PROFILE == 512
    check<T, 16, 1>(bank, expected);
    check<T, 16, 3>(bank, expected);
#endif
  }
}
int main(int argc, char ** argv) {
  if (argc > 2) return 2;
  auto const bank = inputs();
  std::vector<std::uint32_t> expected;
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    for (auto word : bank) expected.push_back(reference(word));
    if (reference(0x42b0c0a5u) == 0x7f800000u || reference(0x42b0c0a6u) != 0x7f800000u || reference(0xc2aeac4fu) == 0 || reference(0xc2aeac50u) != 0) std::abort();
    check_policy<ftz::m32>(bank, expected);
  }
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    check_policy<ftz::m32>(bank, expected);
    check_policy<ftz::h32>(bank, expected);
  }
  if (argc == 2) {
    // Headerless little-endian uint32 pairs: canonical input, oracle output.
    // This is also the GPU fixture bank; raw source NaNs remain unmodified.
    auto * file = std::fopen(argv[1], "wb");
    if (!file) return 3;
    for (std::size_t i = 0; i < bank.size(); ++i) {
      auto const input = (bank[i] & 0x7f800000u) == 0 ? bank[i] & 0x80000000u : bank[i];
      for (auto word : {input, expected[i]})
        for (unsigned shift = 0; shift < 32; shift += 8)
          if (std::fputc(int((word >> shift) & 255u), file) == EOF) return 4;
    }
    if (std::fclose(file)) return 5;
  }
  std::printf("exp contract: %zu inputs, scalar and independent register shapes, m32 gradual/flush and h32 flush pass\n", bank.size());
}
