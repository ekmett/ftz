// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <hint.h>
import native;
import ftz;

constexpr auto emulated = native::isa<>(native::polyfill);

template<class F>
constexpr auto values(float input) {
  using V = native::simd<F, 4, emulated>;
  std::array<float, 4> lanes{input, -input, 0.0f, 1.0f};
  V a{lanes}, b{std::array<float, 4>{2.0f, 4.0f, 1.0f, 0.0f}};
  std::array<std::array<std::uint32_t, 4>, 32> result{};
  unsigned i = 0;
  auto save = [&](V value) { value.store_bits(result[i++].data()); };
  save(a + b); save(a - b); save(a * b); save(a / b);
  save(fma(a, b, b)); save(sqrt(b));
  save(exp(a)); save(expm1(a)); save(log(b)); save(log1p(b));
  save(sin(a)); save(cos(a)); save(tanh(a)); save(atan2(a, b));
  auto [s, c] = sincos(a); save(s); save(c);
  // Raw vectors with an FTZ scalar must enter the FTZ overloads in both orders.
  native::simd<float, 4, emulated> raw{lanes};
  F scalar{2.0f};
  save(raw + scalar); save(scalar + raw);
  save(raw - scalar); save(scalar - raw);
  save(raw / scalar); save(scalar / raw);
  save(select(raw < scalar, a, b)); save(select(scalar >= raw, a, b));
  auto x = std::array{a, b}, y = std::array{b, a};
  for (auto value : ftz::add(x, y)) save(value);
  for (auto value : ftz::sub(x, y)) save(value);
  for (auto value : ftz::mul(x, y)) save(value);
  for (auto value : ftz::fma(x, y, y)) save(value);
  return result;
}

constexpr bool same_words(auto const & actual, auto const & expected) {
  for (unsigned row = 0; row < actual.size(); ++row)
    for (unsigned lane = 0; lane < actual[row].size(); ++lane) {
      auto a = actual[row][lane], b = expected[row][lane];
      if (a != b && !((a & 0x7fffffffu) > 0x7f800000u && (b & 0x7fffffffu) > 0x7f800000u)) return false;
    }
  return true;
}

template<class F>
hint_noinline bool check(float input, auto const & expected) {
  return same_words(values<F>(input), expected);
}

constexpr auto ordinary = values<ftz::m32>(1.0f);
constexpr auto negative_zero = values<ftz::m32>(-0.0f);
static_assert(ordinary[0][0] == 0x40400000u); // 1 + 2 = 3
static_assert(ordinary[3][0] == 0x3f000000u); // 1 / 2 = 0.5
static_assert(ordinary[4][0] == 0x40800000u); // fma(1, 2, 2) = 4
static_assert(negative_zero[2][0] == 0x80000000u);

int main(int argc, char **) {
  // These public scalar overloads were otherwise exercised only at compile time.
  ftz::m32 scalar{static_cast<float>(argc)};
  if (!(float(argc) == scalar) || !(float(argc + 1) > scalar) || !(scalar <= float(argc))) return 1;
  if (ftz::ceil(1.25f).to_float() != 2.0f || ftz::trunc(-1.25f).to_float() != -1.0f) return 2;
  { ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    if (!check<ftz::m32>(1.0f, ordinary) || !check<ftz::m32>(-0.0f, negative_zero)) return 3;
  }
  { ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    if (!check<ftz::m32>(1.0f, ordinary) || !check<ftz::h32>(1.0f, ordinary) ||
        !check<ftz::h32>(-0.0f, negative_zero)) return 4;
  }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  auto before = ftz::read_native_fp_state();
  try {
    ftz::native_fp32_scope invalid(static_cast<ftz::native_fp32_mode>(-1));
    return 5;
  } catch (std::invalid_argument const &) {}
  if (ftz::read_native_fp_state() != before) return 6;
#endif
  std::puts("FTZ public scalar and emulated vector probes passed.");
}
