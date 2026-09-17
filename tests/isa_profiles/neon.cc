// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "contract.h"
#include <array>
#include <cstdio>
#include <type_traits>
import ftz;

int main(int argc, char ** argv) {
  using capture = std::array<std::uint32_t, profile_test::words>;
  capture imported{}, baseline{};
  auto before = ftz::read_native_fp_state();
  {
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::flush);
    if (!scope.controls_match() || !ftz::probe_ftz32_cpu().admitted()) return 1;
    if (profile_neon(imported.data(), imported.size()) != imported.size()) return 2;
    if (!scope.controls_match()) return 4;
  }
  if (before != ftz::read_native_fp_state()) return 5;
  if (argc != 3) return 6;
  auto * out = std::fopen(argv[1], "wb");
  if (!out) return 7;
  bool written = std::fwrite(imported.data(), sizeof(imported[0]), imported.size(), out) == imported.size();
  bool closed = std::fclose(out) == 0;
  if (!written || !closed) return 8;
  auto * in = std::fopen(argv[2], "rb");
  if (!in) return 9;
  bool read = std::fread(baseline.data(), sizeof(baseline[0]), baseline.size(), in) == baseline.size();
  bool eof = std::fgetc(in) == EOF;
  closed = std::fclose(in) == 0;
  if (!read || !eof || !closed) return 10;
  for (std::size_t i = 0; i < imported.size(); ++i) if (imported[i] != baseline[i]) {
    std::fprintf(stderr, "NEON/PC: column=%zu lane=%zu %08x != %08x\n",
      i / profile_test::count, i % profile_test::count, unsigned(imported[i]), unsigned(baseline[i]));
    return 11;
  }
  std::printf("neon import/baseline words=%zu policy=%u exact; FPCR restored\n",
    imported.size(), unsigned(FTZ_FP32_HARDWARE_FTZ));
}
