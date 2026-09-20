// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <array>
#include <type_traits>
#include <cstdio>
#include "../core_regression/support/imports.h"

static_assert(std::is_same_v<::native::simd<ftz::ftz32,4,FTZ_TEST_ARCH>::value_type, ftz::ftz32>);
int main() {
  auto const caller = ftz::read_native_fp_state();
  for (auto mode : {ftz::native_fp32_mode::gradual, ftz::native_fp32_mode::flush}) {
    ftz::native_fp32_scope scope(mode);
    auto admission = ftz::probe_ftz32_cpu();
    bool expected = !FTZ_FP32_HARDWARE_FTZ || mode == ftz::native_fp32_mode::flush;
    if (admission.admitted() != expected || !admission.explicit_compatible()) return 1;
    if (!expected && admission.failure != ftz::ftz32_cpu_failure::hardware_ftz) return 2;
    if (!expected) continue;
    using F = ftz::ftz32;
    using V = ::native::simd<F,4,FTZ_TEST_ARCH>;
    F scalar = ftz::fma(F(1.f), F(2.f), F(3.f));
    if (scalar.to_bits() != 0x40a00000u) return 3;
    ::native::wide<V,3> a(V(1.f)), b(V(2.f)), c(V(3.f));
    auto result = fma(a,b,c);
    bool correct = true;
    for (auto value : result.registers) {
      std::array<unsigned,4> words{}; value.store_bits(words.data());
      correct = correct && words == std::array{0x40a00000u,0x40a00000u,0x40a00000u,0x40a00000u};
    }
    if (!correct || !scope.controls_match()) return 4;
  }
  if (ftz::read_native_fp_state() != caller) return 5;
  std::puts("FTZ opt-in: admission classification and scalar/SIMD/wide arithmetic pass");
}
