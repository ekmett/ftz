module;
#include <array>
module package;
namespace package {
  bool check() {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    std::array<float,4> in{1.f,2.f,3.f,4.f}, out{};
    fused(in.data(),out.data());
    return scope.controls_match() && out == std::array<float,4>{3.f,5.f,7.f,9.f};
  }
}
// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
