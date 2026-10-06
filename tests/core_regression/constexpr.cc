// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "math_contract.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include "support/imports.h"

namespace {
  struct input {
    std::uint32_t a, b, c;
  };
  // Representative ordinary values and arithmetic/function boundaries. This is
  // a constexpr/runtime equivalence check, not an approximation-error sweep.
  constexpr std::array bank{
    input{0, 0x3f800000u, 0},
    input{0x80000000u, 0xbf800000u, 0x80000000u},
    input{0x3e800000u, 0x3fc00000u, 0xbf000000u},
    input{0xbf200000u, 0x40200000u, 0x3e000000u},
    input{0x3fc00000u, 0xbf800000u, 0x3f000000u},
    input{0x40200000u, 0x3f800001u, 0xc0000000u},
    input{0x00800000u, 0x3f7fffffu, 0}, // minimum-normal multiplication tie
    input{0x80800000u, 0x3f7fffffu, 0x80000000u},
    input{0x00800000u, 0x3f7ffffeu, 0}, // below the tie: signed FTZ
    input{0x00800001u, 0x3f000000u, 0},
    input{0x00800000u, 0x00800001u, 0x00800000u},
    input{0x7f7fffffu, 0x40000000u, 0xff7fffffu},
    input{0xff7fffffu, 0x40000000u, 0x7f7fffffu},
    input{0x7f800000u, 0, 0xff800000u},
    input{0xff800000u, 0x7f800000u, 0x3f800000u},
    input{0x7fc12345u, 0x3f800000u, 0},
    input{0xff800001u, 0xbf800000u, 0},
    input{1, 0x807fffffu, 0}, // canonical imports
    input{0x42b17217u, 0x3f800000u, 0}, // exp overflow boundary
    input{0x42b17218u, 0x3f800000u, 0},
    input{0xc2aeac4fu, 0x3f800000u, 0}, // exp FTZ boundary
    input{0xc2aeac50u, 0x3f800000u, 0},
    input{0xbf800000u, 0x80000000u, 0}, // log1p domain boundary
    input{0xbf800001u, 0x3f800000u, 0},
    input{0x45ffffffu, 0x46000000u, 0}, // trig reduction boundary
    input{0xc6000001u, 0xc6000000u, 0},
  };
  constexpr std::array names{
    "add", "sub", "mul", "div", "fma", "sqrt",
    "exp1", "exp2", "exp3", "exp4", "exp5", "exp6", "exp7",
    "expm1", "log", "log1p", "tanh", "atan2", "sin", "cos",
    "sincos.sin", "sincos.cos", "floor", "ceil", "trunc",
    "mixed.add", "mixed.sub", "mixed.mul", "mixed.div", "mixed.fma",
    "mixed.atan2", "abs", "neg", "copysign", "classification", "comparison"
  };
  using row = std::array<std::uint32_t, names.size()>;
  template<class F> constexpr row evaluate(input words) {
    auto a = F::from_bits(words.a), b = F::from_bits(words.b), c = F::from_bits(words.c);
    auto const [s, co] = sincos(a);
    // Metadata occupies separate words: compare it bitwise, including NaN tests.
    auto const classification = unsigned(isnan(a)) | unsigned(isinf(a)) << 1 |
      unsigned(isfinite(a)) << 2 | unsigned(signbit(a)) << 3;
    auto const comparison = unsigned(a == b) | unsigned(a != b) << 1 |
      unsigned(a < b) << 2 | unsigned(a <= b) << 3 | unsigned(a > b) << 4 |
      unsigned(a >= b) << 5 | unsigned(a == 1.f) << 6 | unsigned(1.f != a) << 7 |
      unsigned(a < 1.f) << 8 | unsigned(1.f <= a) << 9 |
      unsigned(a > 1.f) << 10 | unsigned(1.f >= a) << 11;
    return {(a+b).to_bits(), (a-b).to_bits(), (a*b).to_bits(), (a/b).to_bits(),
      fma(a,b,c).to_bits(), sqrt(a).to_bits(),
      ftz::exp<1>(a).to_bits(), ftz::exp<2>(a).to_bits(), ftz::exp<3>(a).to_bits(),
      ftz::exp<4>(a).to_bits(), ftz::exp<5>(a).to_bits(), ftz::exp<6>(a).to_bits(),
      ftz::exp<7>(a).to_bits(), expm1(a).to_bits(), log(a).to_bits(),
      log1p(a).to_bits(), tanh(a).to_bits(), atan2(a,b).to_bits(),
      sin(a).to_bits(), cos(a).to_bits(), s.to_bits(), co.to_bits(),
      floor(a).to_bits(), ceil(a).to_bits(), trunc(a).to_bits(),
      (a+2.f).to_bits(), (2.f-a).to_bits(), (a*2.f).to_bits(), (2.f/a).to_bits(),
      ftz::fma(a,2.f,1.f).to_bits(), ftz::atan2(1.f,a).to_bits(),
      abs(a).to_bits(), neg(a).to_bits(), copysign(a,b).to_bits(), classification, comparison};
  }
  template<class F, std::size_t I> constexpr row constant_row = evaluate<F>(bank[I]);
  template<class F, std::size_t... I> consteval auto table(std::index_sequence<I...>) {
    return std::array{constant_row<F, I>...};
  }
  constexpr auto manual = table<ftz::m32>(std::make_index_sequence<bank.size()>{});
  constexpr auto hardware = table<ftz::h32>(std::make_index_sequence<bank.size()>{});
  template<class F> consteval bool basics() {
    auto const one = F(1.f), two = F(2.f), zero = F::from_bits(0x80000000u);
    return (one+two).to_bits() == 0x40400000u && (two-one).to_bits() == 0x3f800000u &&
      (two*two).to_bits() == 0x40800000u && (one/two).to_bits() == 0x3f000000u &&
      fma(two,two,one).to_bits() == 0x40a00000u && sqrt(F(4.f)).to_bits() == 0x40000000u &&
      sqrt(zero).to_bits() == 0x80000000u && sin(zero).to_bits() == 0x80000000u &&
      cos(zero).to_bits() == 0x3f800000u && exp(F(0.f)).to_bits() == 0x3f800000u &&
      floor(F(-1.5f)).to_bits() == 0xc0000000u && ceil(F(-1.5f)).to_bits() == 0xbf800000u &&
      trunc(F(-.5f)).to_bits() == 0x80000000u &&
      (F::from_bits(0x00800000u)*F::from_bits(0x3f7fffffu)).to_bits() == 0x00800000u &&
      (F::from_bits(0x00800000u)*F::from_bits(0x3f7ffffeu)).to_bits() == 0 &&
      (F::from_bits(0x7f7fffffu)*two).to_bits() == 0x7f800000u &&
      isnan(F::from_bits(0x7fc12345u)) && isinf(F::from_bits(0xff800000u)) &&
      isfinite(zero) && signbit(zero) && zero == 0.f && 0.f == zero &&
      one < 2.f && 2.f > one && !(F::from_bits(0x7fc12345u) <= 1.f);
  }
  static_assert(basics<ftz::m32>() && basics<ftz::h32>());
  template<class F> void check(char const * mode, std::array<row, bank.size()> const & expected) {
    for (std::size_t i = 0; i < bank.size(); ++i) {
      volatile std::uint32_t a = bank[i].a, b = bank[i].b, c = bank[i].c;
      auto const actual = evaluate<F>(input{a,b,c});
      for (std::size_t j = 0; j < names.size(); ++j) {
        bool const same = j >= names.size()-2 ? actual[j] == expected[i][j] :
          ftz::math_test::equivalent_fp32(actual[j], expected[i][j]);
        if (!same) {
          std::fprintf(stderr, "constexpr %s %s input=%08x,%08x,%08x runtime=%08x constant=%08x\n",
            mode, names[j], bank[i].a, bank[i].b, bank[i].c, actual[j], expected[i][j]);
          std::abort();
        }
      }
    }
  }
}
int main() {
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    check<ftz::m32>("m32 gradual", manual);
  }
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    check<ftz::m32>("m32 flush", manual);
    check<ftz::h32>("h32 flush", hardware);
  }
  std::puts("constexpr scalar arithmetic/math/classification: m32 gradual/flush and h32 flush pass");
}
