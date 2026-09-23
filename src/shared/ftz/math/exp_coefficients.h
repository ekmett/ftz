#pragma once
// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0

// Coefficient words shared by the C++ and HLSL fused Horner stages.
// Degrees 1..5 are constant-one relative fits; degree 6 is the constant-one
// absolute fit with linear term one. Degree 7 retains the original graph.
// See tests/exp_fit for the fitting scripts and accuracy limitations.
namespace ftz { namespace detail { namespace math {
  template <unsigned int Degree> struct exp_coefficients;
  template <> struct exp_coefficients<1> {
    static const unsigned int leading = 0x3f76382au;
    static const unsigned int next = 0x3f800000u;
    static const unsigned int c5 = 0x00000000u;
    static const unsigned int c4 = 0x00000000u;
    static const unsigned int c3 = 0x00000000u;
    static const unsigned int c2 = 0x00000000u;
    static const unsigned int c1 = 0x3f76382au;
    static const unsigned int c0 = 0x3f800000u;
  };
  template <> struct exp_coefficients<2> {
    static const unsigned int leading = 0x3eff9d09u;
    static const unsigned int next = 0x3f81cf0bu;
    static const unsigned int c5 = 0x00000000u;
    static const unsigned int c4 = 0x00000000u;
    static const unsigned int c3 = 0x00000000u;
    static const unsigned int c2 = 0x3eff9d09u;
    static const unsigned int c1 = 0x3f81cf0bu;
    static const unsigned int c0 = 0x3f800000u;
  };
  template <> struct exp_coefficients<3> {
    static const unsigned int leading = 0x3e2924d1u;
    static const unsigned int next = 0x3f010eb2u;
    static const unsigned int c5 = 0x00000000u;
    static const unsigned int c4 = 0x00000000u;
    static const unsigned int c3 = 0x3e2924d1u;
    static const unsigned int c2 = 0x3f010eb2u;
    static const unsigned int c1 = 0x3f80066bu;
    static const unsigned int c0 = 0x3f800000u;
  };
  template <> struct exp_coefficients<4> {
    static const unsigned int leading = 0x3d2a0993u;
    static const unsigned int next = 0x3e2be74cu;
    static const unsigned int c5 = 0x00000000u;
    static const unsigned int c4 = 0x3d2a0993u;
    static const unsigned int c3 = 0x3e2be74cu;
    static const unsigned int c2 = 0x3f0001fbu;
    static const unsigned int c1 = 0x3f7ffdd4u;
    static const unsigned int c0 = 0x3f800000u;
  };
  template <> struct exp_coefficients<5> {
    static const unsigned int leading = 0x3c07cfd2u;
    static const unsigned int next = 0x3d2b9d0eu;
    static const unsigned int c5 = 0x3c07cfd2u;
    static const unsigned int c4 = 0x3d2b9d0eu;
    static const unsigned int c3 = 0x3e2aad40u;
    static const unsigned int c2 = 0x3efffee3u;
    static const unsigned int c1 = 0x3f7ffffbu;
    static const unsigned int c0 = 0x3f800000u;
  };
  template <> struct exp_coefficients<6> {
    static const unsigned int leading = 0x3ab6aafau;
    static const unsigned int next = 0x3c091f16u;
    static const unsigned int c5 = 0x3c091f16u;
    static const unsigned int c4 = 0x3d2aaa70u;
    static const unsigned int c3 = 0x3e2aaa45u;
    static const unsigned int c2 = 0x3f000000u;
    static const unsigned int c1 = 0x3f800000u;
    static const unsigned int c0 = 0x3f800000u;
  };
  template <> struct exp_coefficients<7> {
    static const unsigned int leading = 0x3950eb8au;
    static const unsigned int next = 0x3ab6d3abu;
    static const unsigned int c5 = 0x3c08882eu;
    static const unsigned int c4 = 0x3d2aaa32u;
    static const unsigned int c3 = 0x3e2aaaabu;
    static const unsigned int c2 = 0x3f000000u;
    static const unsigned int c1 = 0x3f800000u;
    static const unsigned int c0 = 0x3f800000u;
  };
}}}
