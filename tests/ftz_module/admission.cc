// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstdio>
#include <initializer_list>
#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif
import ftz.controls;
namespace {
  enum class fault { none, normal, fusion, separate, input, output, core, explicit_core, nan, zero, controls };
  fault selected{};
  unsigned calls = 0;
  unsigned evaluator(ftz::detail::ftz32_cpu_witness row, ftz::detail::ftz32_cpu_path path) {
    using P = ftz::detail::ftz32_cpu_path;
    ++calls;
    bool corrupt = (selected == fault::normal && path == P::raw && row.operation == 0 && row.a == 0x3f800000u) ||
      (selected == fault::fusion && path == P::raw && row.operation == 2 && row.a == 0x3f800001u) ||
      (selected == fault::separate && path == P::raw && row.operation == 3) ||
      (selected == fault::input && path == P::raw && row.a == 1u) ||
      (selected == fault::output && path == P::raw && row.a == 0x00800000u && row.b == 0x3f000000u) ||
      (selected == fault::core && path == P::compiled_core && row.operation == 4 && row.a == 0x00800000u) ||
      (selected == fault::explicit_core && path == P::explicit_core && row.operation == 4 && row.a == 0x00800000u);
    if (selected == fault::nan && row.operation == 4 && row.a == 0xff800001u) return 0xffc12345u;
    if (selected == fault::zero && path == P::compiled_core && row.operation == 4 && row.a == 1u) return 0x80000000u;
#if defined(_M_X64) || defined(__x86_64__)
    if (selected == fault::controls && calls == 1) _mm_setcsr(_mm_getcsr() ^ 0x2000u);
#endif
    return row.expected ^ (corrupt ? 1u : 0u);
  }
  ftz::ftz32_cpu_admission observe(fault value, bool hardware) {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    selected = value; calls = 0;
    return ftz::detail::ftz32_cpu_probe(evaluator, hardware);
  }
}
int main() {
  auto caller = ftz::read_native_fp_state();
  using F = ftz::ftz32_cpu_failure;
  for (bool hardware : {false,true}) {
    auto good = observe(fault::none,hardware);
    if (!good.admitted() || !good.explicit_compatible() || calls != 55) return 1;
    for (auto failure : {fault::normal,fault::fusion,fault::separate})
      if (observe(failure,hardware).failure != F::normal_arithmetic) return 2;
    for (auto failure : {fault::input,fault::output}) {
      auto result = observe(failure,hardware);
      if (result.admitted() == hardware || (hardware && result.failure != F::hardware_ftz)) return 3;
    }
    if (observe(fault::core,hardware).failure != F::core_contract) return 4;
    auto explicit_bad = observe(fault::explicit_core,hardware);
    if (!explicit_bad.admitted() || explicit_bad.explicit_compatible()) return 5;
    if (!observe(fault::nan,hardware).admitted()) return 6;
    if (observe(fault::zero,hardware).failure != F::core_contract) return 7;
  }
#if defined(_M_X64) || defined(__x86_64__)
  auto changed = observe(fault::controls,false);
  if (changed.failure != F::normal_arithmetic || changed.controls_stable) return 8;
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    auto saved = _mm_getcsr();
    _mm_setcsr(saved & ~0x1000u);
    calls = 0; selected = fault::none;
    auto result = ftz::detail::ftz32_cpu_probe(evaluator,false);
    _mm_setcsr(saved);
    if (calls != 0 || result.failure != F::normal_arithmetic) return 9;
  }
#endif
  {
    ftz::native_fp32_scope borrowed(ftz::native_fp32_mode::gradual);
    for (auto mode : {ftz::native_fp32_mode::gradual,ftz::native_fp32_mode::flush}) {
      auto before = ftz::read_native_fp_state();
      calls = 0;
      if (!ftz::set_native_fp32_mode(mode) || calls != 0) return 11;
      auto after = ftz::read_native_fp_state();
      auto expected = before.control;
#if defined(_M_X64) || defined(__x86_64__)
      expected = (expected & ~0xe040ull) | 0x1f80u;
      if (mode == ftz::native_fp32_mode::flush) expected |= 0x8040u;
#elif defined(__aarch64__) || defined(__arm64__)
      expected &= ~(7ull | (0x1full << 8) | (1ull << 15) | (3ull << 22) | (1ull << 24));
      if (mode == ftz::native_fp32_mode::flush) expected |= 1ull << 24;
#endif
      if (after.control != expected || after.status != 0) return 12;
      if (ftz::set_native_fp32_mode(static_cast<ftz::native_fp32_mode>(99)) ||
          ftz::read_native_fp_state() != after) return 13;
    }
  }
  if (ftz::read_native_fp_state() != caller) return 10;
  std::puts("common admission: all 55 observations, failure precedence, NaN/zero matching and caller restoration pass");
}
