// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstdio>
#include <thread>
// The package advertises the exact minimum inherited by baseline consumers.
#if !defined(NATIVE_MINIMAL_HAS_AVX2) || !defined(NATIVE_MINIMAL_HAS_AVX512)
#error The native package must publish its configured minimum capabilities
#endif
#if defined(__AVX2__) != NATIVE_MINIMAL_HAS_AVX2
#error Consumer AVX2 capability differs from the configured SIMD minimum
#endif
#if (defined(__AVX512F__) || defined(__AVX512DQ__) || defined(__AVX512BW__) || defined(__AVX512VL__)) != NATIVE_MINIMAL_HAS_AVX512
#error Consumer AVX512 capability differs from the configured SIMD minimum
#endif
import ftz.controls;

int main() {
  if (!ftz::native_fp32_environment_available()) return 1;
  auto const caller = ftz::read_native_fp_state();
  for (auto mode : {ftz::native_fp32_mode::gradual, ftz::native_fp32_mode::flush}) {
    {
      ftz::native_fp32_scope scope(mode);
      if (!scope.controls_match() || scope.previous() != caller ||
          ftz::read_native_fp_state() != scope.requested()) return 2;
      auto const region = ftz::read_native_fp_state();
      {
        ftz::native_fp32_scope::external_scope external(scope);
        if (ftz::read_native_fp_state() != caller) return 3;
      }
      if (ftz::read_native_fp_state() != region) return 4;
      bool child_ok = false;
      std::thread child([&] {
        auto const previous = ftz::read_native_fp_state();
        {
          ftz::native_fp32_scope nested(ftz::native_fp32_mode::flush);
          child_ok = nested.controls_match();
        }
        child_ok = child_ok && ftz::read_native_fp_state() == previous;
      });
      child.join();
      if (!child_ok || !scope.controls_match()) return 5;
    }
    if (ftz::read_native_fp_state() != caller) return 6;
  }
  std::puts("baseline controls: both modes, nested external and thread state restored");
}
