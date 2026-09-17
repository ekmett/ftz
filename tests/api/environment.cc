// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include <cstdio>
import ftz;

bool borrowed_region() {
  //! [borrowed_region]
  ftz::native_fp32_scope region(ftz::native_fp32_mode::flush);
  auto admission = ftz::probe_ftz32_cpu<ftz::h32>();
  if (!admission.admitted()) return false;
  // Establish this agreement on each numerical thread. Run qualification at startup.
  auto value = ftz::h32(2.f) * ftz::h32(3.f);
  {
    ftz::native_fp32_scope::external_scope external(region);
    // Call third-party code here under the environment preceding region.
    if (ftz::read_native_fp_state() != region.previous()) return false;
  }
  // The numerical region is restored, including its saved status flags.
  return region.controls_match() && value.to_bits() == 0x40c00000u;
  //! [borrowed_region]
}

bool owned_thread_entry() {
  //! [owned_thread_entry]
  if (!ftz::set_native_fp32_mode(ftz::native_fp32_mode::flush)) return false;
  // This establishes RNE and masked exceptions, and clears status flags.
  // It does not run admission or restore the previous state on return.
  return true;
  //! [owned_thread_entry]
}

int main() {
  auto before = ftz::read_native_fp_state();
  if (!borrowed_region() || ftz::read_native_fp_state()!=before) return 1;
  {
    // The example initializer changes the caller's state; this test borrows a scope.
    ftz::native_fp32_scope scope(ftz::native_fp32_mode::gradual);
    if (!owned_thread_entry()) return 2;
  }
  if (ftz::read_native_fp_state()!=before) return 3;
  std::puts("Per-thread controls, admission and complete state restoration pass");
}
