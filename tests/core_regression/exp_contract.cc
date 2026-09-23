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
  template<unsigned Degree> constexpr auto coefficients() {
    if constexpr (Degree == 1) return std::array{0x3f76382au, 0x3f800000u};
    else if constexpr (Degree == 2) return std::array{0x3eff9d09u, 0x3f81cf0bu, 0x3f800000u};
    else if constexpr (Degree == 3) return std::array{0x3e2924d1u, 0x3f010eb2u, 0x3f80066bu, 0x3f800000u};
    else if constexpr (Degree == 4) return std::array{0x3d2a0993u, 0x3e2be74cu, 0x3f0001fbu, 0x3f7ffdd4u, 0x3f800000u};
    else if constexpr (Degree == 5) return std::array{0x3c07cfd2u, 0x3d2b9d0eu, 0x3e2aad40u, 0x3efffee3u, 0x3f7ffffbu, 0x3f800000u};
    else if constexpr (Degree == 6) return std::array{0x3ab6aafau, 0x3c091f16u, 0x3d2aaa70u, 0x3e2aaa45u, 0x3f000000u, 0x3f800000u, 0x3f800000u};
    else return std::array{0x3950eb8au, 0x3ab6d3abu, 0x3c08882eu, 0x3d2aaa32u, 0x3e2aaaabu, 0x3f000000u, 0x3f800000u, 0x3f800000u};
  }
  template<unsigned Degree> std::uint32_t reference(std::uint32_t word) {
    auto const magnitude = word & 0x7fffffffu;
    if (magnitude > 0x7f800000u) return 0x7fc00000u;
    float x = std::bit_cast<float>(word);
    if (x < -87.33654022216796875f) return 0;
    if (x > 88.37625885009765625f) return 0x7f800000u;
    float const n = std::nearbyint(x * std::bit_cast<float>(0x3fb8aa3bu));
    float const first = std::fma(n, std::bit_cast<float>(0xbf317200u), x);
    float const r = std::fma(n, std::bit_cast<float>(0xb5bfbe8eu), first);
    constexpr auto c = coefficients<Degree>();
    float y = std::bit_cast<float>(c[0]);
    for (std::size_t i = 1; i < c.size(); ++i) y = std::fma(r, y, std::bit_cast<float>(c[i]));
    auto const scaled = std::bit_cast<std::uint32_t>(float(std::ldexp(double(y), int(n))));
    return scaled < 0x00800000u ? 0u : scaled;
  }
  std::vector<std::uint32_t> inputs() {
    std::vector<std::uint32_t> result{0, 0x80000000u, 1, 0x807fffffu, 0x00800000u,
      0x80800000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xff800001u,
      0x7f7fffffu, 0xff7fffffu, 0x3f7fffffu, 0x3f800000u, 0x3f800001u, 0x42b0c0a5u, 0x42b17217u, 0x42b17218u};
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
  template<unsigned Degree, class T, std::size_t L, std::size_t N>
  void check(std::vector<std::uint32_t> const & bank, std::vector<std::uint32_t> const & expected) {
    using R = native::simd<T, L, FTZ_TEST_ARCH>;
    using W = native::wide<R, N>;
    if constexpr (N == 0) {
      if (exp<false, Degree>(W{}).registers.size() != 0 || !ftz::exp<Degree>(std::array<R, 0>{}).empty()) std::abort();
    } else {
      for (std::size_t offset = 0; offset < bank.size(); offset += L * N) {
        W input{};
        std::array<std::array<std::uint32_t, L>, N> words{};
        for (std::size_t reg = 0; reg < N; ++reg) {
          for (std::size_t lane = 0; lane < L; ++lane) words[reg][lane] = bank[(offset + reg * L + lane) % bank.size()];
          input.registers[reg] = R::load_bits(words[reg].data());
        }
        auto const value = exp<false, Degree>(input);
        auto const array_value = ftz::exp<Degree>(input.registers);
        for (std::size_t reg = 0; reg < N; ++reg) {
          std::array<std::uint32_t, L> actual{};
          value.registers[reg].store_bits(actual.data());
          for (std::size_t lane = 0; lane < L; ++lane) {
            auto const want = expected[(offset + reg * L + lane) % bank.size()];
            if (!ftz::math_test::equivalent_fp32(actual[lane], want)) {
              std::fprintf(stderr, "exp degree=%u L=%zu N=%zu input=%08x actual=%08x expected=%08x\n", Degree, L, N, words[reg][lane], actual[lane], want);
              std::abort();
            }
          }
          std::array<std::uint32_t, L> other{};
          array_value[reg].store_bits(other.data());
          for (std::size_t lane = 0; lane < L; ++lane)
            if (!ftz::math_test::equivalent_fp32(actual[lane], other[lane])) std::abort();
          ftz::exp<Degree>(input.registers[reg]).store_bits(other.data());
          for (std::size_t lane = 0; lane < L; ++lane)
            if (!ftz::math_test::equivalent_fp32(actual[lane], other[lane])) std::abort();
          if constexpr (Degree == 6) {
            exp(input).registers[reg].store_bits(other.data());
            for (std::size_t lane = 0; lane < L; ++lane)
              if (!ftz::math_test::equivalent_fp32(actual[lane], other[lane])) std::abort();
            expm1(input).registers[reg].store_bits(other.data());
            for (std::size_t lane = 0; lane < L; ++lane) {
              auto const word = words[reg][lane];
              if (word > 0x3f800000u && word < 0x7f800000u) {
                auto const want = std::bit_cast<std::uint32_t>(std::bit_cast<float>(actual[lane]) - 1.f);
                if (!ftz::math_test::equivalent_fp32(other[lane], want)) std::abort();
              }
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
  template<unsigned Degree, class T> void check_policy(std::vector<std::uint32_t> const & bank, std::vector<std::uint32_t> const & expected) {
    for (std::size_t i = 0; i < bank.size(); ++i) {
      auto const actual = ftz::exp<Degree>(T::from_bits(bank[i])).to_bits();
      if (!ftz::math_test::equivalent_fp32(actual, expected[i])) {
        std::fprintf(stderr, "scalar exp degree=%u input=%08x actual=%08x expected=%08x\n", Degree, bank[i], actual, expected[i]);
        std::abort();
      }
      // Positive arguments above one continue through this exp graph. The
      // cancellation-safe expm1 core at smaller arguments is independently tested.
      if constexpr (Degree == 6) {
        if (!ftz::math_test::equivalent_fp32(exp(T::from_bits(bank[i])).to_bits(), actual)) std::abort();
      }
      if (Degree == 6 && bank[i] > 0x3f800000u && bank[i] < 0x7f800000u) {
        auto const want = std::bit_cast<std::uint32_t>(std::bit_cast<float>(expected[i]) - 1.f);
        if (!ftz::math_test::equivalent_fp32(expm1(T::from_bits(bank[i])).to_bits(), want))
          std::abort();
      }
    }
    check<Degree, T, 1, 1>(bank, expected);
    check<Degree, T, 2, 3>(bank, expected);
    check<Degree, T, 3, 3>(bank, expected);
    check<Degree, T, 4, 0>(bank, expected);
    check<Degree, T, 4, 1>(bank, expected);
    check<Degree, T, 4, 3>(bank, expected);
    check<Degree, T, 4, 6>(bank, expected);
    check<Degree, T, 4, 8>(bank, expected);
#if FTZ_TEST_PROFILE >= 256
    check<Degree, T, 8, 1>(bank, expected);
    check<Degree, T, 8, 3>(bank, expected);
    check<Degree, T, 8, 6>(bank, expected);
    check<Degree, T, 8, 8>(bank, expected);
#endif
#if FTZ_TEST_PROFILE == 512
    check<Degree, T, 16, 1>(bank, expected);
    check<Degree, T, 16, 3>(bank, expected);
    check<Degree, T, 16, 6>(bank, expected);
#endif
  }
  template<unsigned Degree, class T> concept admits_degree = requires(T x) { ftz::exp<Degree>(x); };
  template<class T> constexpr bool admission() {
    return !admits_degree<0, T> && admits_degree<1, T> && admits_degree<7, T> && !admits_degree<8, T>;
  }
  static_assert(admission<ftz::m32>() && admission<ftz::h32>());
  static_assert(admission<native::simd<ftz::m32, 4, FTZ_TEST_ARCH>>());
  static_assert(admission<std::array<ftz::m32, 6>>());

  template<unsigned Degree> void run(std::vector<std::uint32_t> const & bank, char const * packet) {
    std::vector<std::uint32_t> expected;
    {
      ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
      for (auto word : bank) expected.push_back(reference<Degree>(word));
      if (reference<Degree>(0x42b0c0a5u) == 0x7f800000u || reference<Degree>(0x42b0c0a6u) != 0x7f800000u ||
          reference<Degree>(0xc2aeac4fu) == 0 || reference<Degree>(0xc2aeac50u) != 0) std::abort();
      check_policy<Degree, ftz::m32>(bank, expected);
    }
    {
      ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
      check_policy<Degree, ftz::m32>(bank, expected);
      check_policy<Degree, ftz::h32>(bank, expected);
    }
    if (packet) {
      // Headerless little-endian uint32 pairs: canonical input, oracle output.
      // This is also the GPU fixture bank; raw source NaNs remain unmodified.
      auto * file = std::fopen(packet, "wb");
      if (!file) std::exit(3);
      for (std::size_t i = 0; i < bank.size(); ++i) {
        auto const input = (bank[i] & 0x7f800000u) == 0 ? bank[i] & 0x80000000u : bank[i];
        for (auto word : {input, expected[i]})
          for (unsigned shift = 0; shift < 32; shift += 8)
            if (std::fputc(int((word >> shift) & 255u), file) == EOF) std::exit(4);
      }
      if (std::fclose(file)) std::exit(5);
    }
    std::printf("exp degree=%u contract: %zu inputs, scalar/SIMD/array/wide, m32 gradual/flush and h32 flush pass\n", Degree, bank.size());
  }
}
int main(int argc, char ** argv) {
  if (argc > 3) return 2;
  unsigned const degree = argc == 3 ? unsigned(std::atoi(argv[2])) : 6;
  if (degree < 1 || degree > 7) return 2;
  auto const bank = inputs();
  auto const packet = [=](unsigned d) { return argc >= 2 && d == degree ? argv[1] : nullptr; };
  run<1>(bank, packet(1));
  run<2>(bank, packet(2));
  run<3>(bank, packet(3));
  run<4>(bank, packet(4));
  run<5>(bank, packet(5));
  run<6>(bank, packet(6));
  run<7>(bank, packet(7));
  std::puts("default degree-six identity and positive expm1 continuation pass");
}
