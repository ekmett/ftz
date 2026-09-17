#pragma once
#include <cstddef>
#include <cstdint>

namespace profile_test {
  inline constexpr std::size_t count = 96;
  inline constexpr std::size_t columns = 23;
  inline constexpr std::size_t words = count * columns;
  using entry = std::size_t (*)(std::uint32_t *, std::size_t);
}

extern "C" std::size_t profile_avx2(std::uint32_t *, std::size_t);
extern "C" std::size_t profile_avx512(std::uint32_t *, std::size_t);
extern "C" std::size_t profile_neon(std::uint32_t *, std::size_t);
