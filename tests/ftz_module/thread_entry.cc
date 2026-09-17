// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
import ftz.controls;
extern "C"
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
bool ftz_thread_entry() {
  return ftz::set_native_fp32_mode(ftz::native_fp32_mode::flush);
}
int main() {
  auto caller=ftz::read_native_fp_state();
  {
    ftz::native_fp32_scope borrowed(ftz::native_fp32_mode::gradual);
    if(!ftz_thread_entry())return 1;
    auto state=ftz::read_native_fp_state();
#if defined(_M_X64) || defined(__x86_64__)
    if((state.control&0xffc0u)!=0x9fc0u)return 2;
#elif defined(__aarch64__) || defined(__arm64__)
    if((state.control&((3ull<<22)|(1ull<<24)|7u))!=(1ull<<24))return 2;
#endif
    if(state.status!=0)return 3;
  }
  return ftz::read_native_fp_state()!=caller?4:0;
}
