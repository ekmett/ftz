// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <mpfr.h>
#include "../core_regression/support/imports.h"

namespace {
  using word = std::uint32_t;
  constexpr word seed = 0xfa728311u;
  constexpr unsigned samples = 16384;
  constexpr std::array cutovers{0x00800000u, 0x33000000u, 0xb3000000u,
    0xbf000000u, 0x3f800000u, 0xbf800000u, 0x3fc00000u};
  std::FILE *report = stdout;

  word normalize(word x) {
    return (x & 0x7fffffffu) < 0x00800000u ? x & 0x80000000u : x;
  }
  bool nan(word x) { return (x & 0x7fffffffu) > 0x7f800000u; }
  bool finite(word x) { return (x & 0x7fffffffu) < 0x7f800000u; }
  word ordered(word x) { return x & 0x80000000u ? ~x : x ^ 0x80000000u; }
  bool same(word a, word b) { return a == b || (nan(a) && nan(b)); }
  std::uint64_t distance(word a, word b) {
    auto x = std::uint64_t(ordered(a)), y = std::uint64_t(ordered(b));
    return x > y ? x - y : y - x;
  }
  struct real {
    mpfr_t value;
    explicit real(mpfr_prec_t precision) { mpfr_init2(value, precision); }
    ~real() { mpfr_clear(value); }
    real(real const &) = delete;
    real &operator=(real const &) = delete;
  };
  void evaluate(mpfr_ptr y, mpfr_srcptr x, unsigned operation) {
    if (operation == 0) mpfr_log(y, x, MPFR_RNDN);
    else mpfr_log1p(y, x, MPFR_RNDN);
  }
  word rounded(mpfr_srcptr x) {
    return normalize(std::bit_cast<word>(mpfr_get_flt(x, MPFR_RNDN)));
  }
  std::vector<word> bank() {
    std::vector<word> result;
    auto neighbors = [&](word center, int radius) {
      for (int delta = -radius; delta <= radius; ++delta) result.push_back(center + delta);
    };
    for (word center : cutovers) neighbors(center, 32);
    for (word center : {0u, 1u, 0x007fffffu, 0x00800000u, 0x7f7fffffu,
                        0x7f800000u, 0x7fc00000u, 0x7f800001u}) {
      neighbors(center, 4);
      neighbors(center ^ 0x80000000u, 4);
    }
    neighbors(0x3f800000u, 4096);
    neighbors(0xbf800000u, 4096);
    for (unsigned exponent = 1; exponent < 255; ++exponent) {
      neighbors(exponent << 23, 2);
      neighbors((exponent << 23) ^ 0x80000000u, 2);
      // Mantissa reduction switches at 1.5 * 2^e for every normal exponent.
      neighbors((exponent << 23) | 0x00400000u, 2);
    }
    word state = seed;
    for (unsigned i = 0; i < samples; ++i) {
      state ^= state << 13; state ^= state >> 17; state ^= state << 5;
      result.push_back(state);
    }
    std::sort(result.begin(), result.end(), [](word a, word b) { return ordered(a) < ordered(b); });
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }
  struct outputs { std::array<word, 2> words; };
  template<class F>
  std::vector<outputs> graph(std::vector<word> const &inputs, ftz::native_fp32_mode mode) {
    ftz::native_fp32_scope scope(mode);
    if (!ftz::probe_ftz32_cpu<F>().admitted()) {
      std::fprintf(stderr, "MPFR fixture: compiled arithmetic profile not admitted\n");
      std::exit(2);
    }
    std::vector<outputs> result;
    result.reserve(inputs.size());
    for (word raw : inputs) {
      auto x = F::from_bits(raw);
      result.push_back({{ftz::log(x).to_bits(), ftz::log1p(x).to_bits()}});
    }
    return result;
  }
  struct observation {
    word raw = 0, input = 0, actual = 0, reference = 0;
    std::uint64_t ulp = 0;
    double absolute = 0;
  };
  void print_case(char const *label, observation const &x) {
    std::fprintf(report, "%s raw=%08x input=%08x actual=%08x reference=%08x ulp_distance=%llu absolute_error=%.17g\n",
      label, x.raw, x.input, x.actual, x.reference,
      static_cast<unsigned long long>(x.ulp), x.absolute);
  }
  bool check(std::vector<word> const &inputs, std::vector<outputs> const &values,
             char const *policy) {
    // Keep all MPFR conversions/diagnostics outside the graph's flushing scope.
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    real input(512), low(256), high(512), actual_value(512), error(512), cutoff(512);
    mpfr_set_ui_2exp(cutoff.value, 1, -12, MPFR_RNDN);
    bool okay = true;
    for (unsigned op = 0; op < 2; ++op) {
      observation worst, near_zero;
      std::size_t monotonic = 0, specials = 0, finite_count = 0;
      bool previous_valid = false;
      word previous_input = 0, previous_output = 0;
      std::fprintf(report, "\npolicy=%s operation=%s\n", policy, op == 0 ? "log" : "log1p");
      for (std::size_t i = 0; i < inputs.size(); ++i) {
        word raw = inputs[i], x = normalize(raw), y = values[i].words[op];
        mpfr_set_flt(input.value, std::bit_cast<float>(x), MPFR_RNDN);
        evaluate(low.value, input.value, op);
        evaluate(high.value, input.value, op);
        word reference = rounded(high.value);
        if (!same(reference, rounded(low.value))) {
          std::fprintf(report, "precision disagreement raw=%08x\n", raw);
          okay = false;
        }
        bool special = !finite(reference) ||
          (op == 0 && x == 0x3f800000u) || (op == 1 && (x & 0x7fffffffu) == 0);
        if (special) {
          ++specials;
          for (word cutover : cutovers)
            if (raw == cutover - 1 || raw == cutover || raw == cutover + 1)
              std::fprintf(report, "cutover_special raw=%08x input=%08x actual=%08x reference=%08x\n",
                raw, x, y, reference);
          if (!same(y, reference)) {
            std::fprintf(report, "special/domain failure raw=%08x actual=%08x reference=%08x\n", raw, y, reference);
            okay = false;
          }
        }
        if (normalize(y) != y || (finite(reference) && !finite(y))) {
          std::fprintf(report, "noncanonical/unexpected nonfinite result raw=%08x actual=%08x\n", raw, y);
          okay = false;
        }
        if (!nan(reference) && !nan(y)) {
          if (previous_valid && ordered(x) > ordered(previous_input) &&
              ordered(y) < ordered(previous_output)) {
            if (monotonic < 8)
              std::fprintf(report, "monotonicity input=%08x,%08x output=%08x,%08x\n",
                previous_input, x, previous_output, y);
            ++monotonic;
          }
          previous_input = x; previous_output = y; previous_valid = true;
        }
        if (!finite(reference) || !finite(y)) continue;
        ++finite_count;
        mpfr_set_flt(actual_value.value, std::bit_cast<float>(y), MPFR_RNDN);
        mpfr_sub(error.value, actual_value.value, high.value, MPFR_RNDN);
        mpfr_abs(error.value, error.value, MPFR_RNDN);
        observation current{raw, x, y, reference, distance(y, reference), mpfr_get_d(error.value, MPFR_RNDU)};
        if (current.ulp > worst.ulp || finite_count == 1) worst = current;
        // |mathematical result| <= 2^-12; absolute error is against unflushed MPFR.
        if (mpfr_cmpabs(high.value, cutoff.value) <= 0 && current.absolute >= near_zero.absolute)
          near_zero = current;
        for (word cutover : cutovers)
          if (raw == cutover - 1 || raw == cutover || raw == cutover + 1)
            print_case("cutover", current);
      }
      print_case("maximum_ulp", worst);
      print_case("near_zero_maximum_absolute", near_zero);
      std::fprintf(report, "finite=%zu special_domain=%zu monotonicity_violations=%zu\n",
        finite_count, specials, monotonic);
    }
    return okay;
  }
}

int main(int argc, char **argv) {
  if (argc > 2) return 2;
  if (argc == 2) { report = std::fopen(argv[1], "w"); if (!report) return 2; }
  auto inputs = bank();
  std::fprintf(report, "MPFR %s; reference=256/512 bits, RNDN, binary32 RNDN then signed FTZ\n", mpfr_get_version());
  std::fprintf(report, "profile=%d unique_raw_inputs=%zu xorshift32_seed=%08x samples=%u\n",
    FTZ_TEST_PROFILE, inputs.size(), seed, samples);
  std::fprintf(report, "ULP distance counts binary32 representations after reference FTZ; not a proven bound or correct-rounding requirement.\n");
  bool okay = true;
  auto manual = graph<ftz::m32>(inputs, ftz::native_fp32_mode::gradual);
  okay &= check(inputs, manual, "m32/gradual");
  auto manual_flush = graph<ftz::m32>(inputs, ftz::native_fp32_mode::flush);
  okay &= check(inputs, manual_flush, "m32/flush");
  auto hardware = graph<ftz::h32>(inputs, ftz::native_fp32_mode::flush);
  okay &= check(inputs, hardware, "h32/flush");
  if (report != stdout && std::fclose(report) != 0) return 2;
  return okay ? 0 : 1;
}
