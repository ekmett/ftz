// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>
import ftz;
import native;
import native.math;

template<auto Arch> concept vector_shape = requires {
  sizeof(native::simd<ftz::m32, 4, Arch>);
};
static_assert(std::same_as<decltype(native::scalar), native::isa<> const>);
static_assert(!std::convertible_to<native::isa<native::x86>, native::isa<native::arm>>);
static_assert(!std::convertible_to<native::isa<native::arm>, native::isa<native::x86>>);
static_assert(!vector_shape<native::isa<native::wasm>{}>);
#if defined(__aarch64__) || defined(_M_ARM64)
constexpr auto arch = native::neon;
static_assert(!vector_shape<native::avx2>);
#else
constexpr auto arch = native::avx2;
static_assert(!vector_shape<native::neon>);
#endif

template<class F> constexpr bool types() {
  using V = native::simd<F, 4, arch>;
  using R = native::simd<float, 4, arch>;
  using M = native::mask<V>;
  static_assert(std::same_as<native::mask<F const &>, bool>);
  static_assert(std::same_as<native::mask<std::array<F, 3>>, std::array<bool, 3>>);
  static_assert(std::same_as<decltype(isfinite(native::wide<F, 3>{})), native::wide<bool, 3>>);
  static_assert(std::same_as<M, native::mask<R>>);
  static_assert(std::same_as<decltype(V{} < V{}), M>);
  static_assert(std::same_as<decltype(isfinite(native::wide<V, 2>{})), native::wide<M, 2>>);
  static_assert(std::same_as<decltype(V::architecture), native::isa<> const>);
  static_assert(std::same_as<typename native::simd<F, 1>::value_type, F>);
  static_assert(native::simd<F, 1>::architecture == native::simd<float, 1>::architecture);
  static_assert(sizeof(V) == sizeof(R) && alignof(V) == alignof(R));
  static_assert(F::from_bits(1).to_bits() == 0);
  static_assert(F::from_bits(0x80000001u).to_bits() == 0x80000000u);
  static_assert((-F::from_bits(0)).to_bits() == 0x80000000u);
  return true;
}
static_assert(types<ftz::m32>() && types<ftz::h32>());

static_assert(std::countl_zero(std::uint32_t{}) == 32);
static_assert(std::countr_zero(std::uint32_t{}) == 32);

int main(int argc, char **) {
  // Exercise <bit> from a profiled importer of the baseline PCH-built provider.
  // CTest invokes this fixture with no arguments, so argc supplies runtime one.
  auto one = static_cast<std::uint32_t>(argc);
  if (std::countl_zero(one) != 31 || std::countr_zero(one) != 0) return 2;
  ftz::native_fp32_scope region(ftz::native_fp32_mode::gradual);
  using V = native::simd<ftz::m32, 4, arch>;
  std::array<ftz::m32, 4> input{1.f, 2.f, 3.f, 4.f}, output{};
  auto value = native::load_simd<V>(input);
  native::store_simd(output.data(), value + V(ftz::m32(1.f)));
  for (std::size_t i = 0; i < output.size(); ++i)
    if (output[i] != ftz::m32(float(i + 2))) return 1;
  return 0;
}
