#include <array>
#include <cstdio>
import package;
int main() {
  ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
  std::array<float,4> in{.25f,.5f,1.f,2.f}, out{};
  package::fused(in.data(),out.data());
  if (out != std::array<float,4>{1.5f,2.f,3.f,5.f} || !package::check()) return 1;
  std::puts("Independent consumer links package -> ftz -> simd, including inline SIMD wrappers and native controls");
}
// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
