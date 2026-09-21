// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstddef>
#include "../core_regression/support/imports.h"
constexpr auto arch=FTZ_TEST_ARCH;
#if FTZ_TEST_PROFILE == 512
constexpr std::size_t lanes=16;
#elif FTZ_TEST_PROFILE == 256
constexpr std::size_t lanes=8;
#else
constexpr std::size_t lanes=4;
#endif
extern "C" void m32_floor(ftz::m32 const * input,ftz::m32 * output) {
  using V=::native::simd<ftz::m32,lanes,arch>;
  native::store_simd(output,floor(native::load_simd<V>(input)));
}
extern "C" void m32_ceil(ftz::m32 const * input,ftz::m32 * output) {
  using V=::native::simd<ftz::m32,lanes,arch>;
  native::store_simd(output,ceil(native::load_simd<V>(input)));
}
extern "C" void m32_trunc(ftz::m32 const * input,ftz::m32 * output) {
  using V=::native::simd<ftz::m32,lanes,arch>;
  native::store_simd(output,trunc(native::load_simd<V>(input)));
}
extern "C" void h32_floor(ftz::h32 const * input,ftz::h32 * output) {
  using V=::native::simd<ftz::h32,lanes,arch>;
  native::store_simd(output,floor(native::load_simd<V>(input)));
}
extern "C" void h32_ceil(ftz::h32 const * input,ftz::h32 * output) {
  using V=::native::simd<ftz::h32,lanes,arch>;
  native::store_simd(output,ceil(native::load_simd<V>(input)));
}
extern "C" void h32_trunc(ftz::h32 const * input,ftz::h32 * output) {
  using V=::native::simd<ftz::h32,lanes,arch>;
  native::store_simd(output,trunc(native::load_simd<V>(input)));
}
extern "C" float m32_scalar_floor(float input) {
  return ftz::floor(ftz::m32::unsafe_from_float32(input)).to_float();
}
extern "C" float m32_scalar_ceil(float input) {
  return ftz::ceil(ftz::m32::unsafe_from_float32(input)).to_float();
}
extern "C" float m32_scalar_trunc(float input) {
  return ftz::trunc(ftz::m32::unsafe_from_float32(input)).to_float();
}
extern "C" float h32_scalar_floor(float input) {
  return ftz::floor(ftz::h32::unsafe_from_float32(input)).to_float();
}
extern "C" float h32_scalar_ceil(float input) {
  return ftz::ceil(ftz::h32::unsafe_from_float32(input)).to_float();
}
extern "C" float h32_scalar_trunc(float input) {
  return ftz::trunc(ftz::h32::unsafe_from_float32(input)).to_float();
}
