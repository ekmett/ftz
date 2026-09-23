# Selectable exponential coefficients

`exp<Degree=6>` selects degrees 1 through 7 at compile time. Degree 6 is the
new default; ordinary calls keep their spelling but some approximate result
words change from the old degree-seven graph. Degree 7 preserves that graph's
rounded coefficient words. `expm1(x)` uses default degree 6 only in its positive
`x>1` continuation; its cancellation-safe core and damping gain are unchanged.

The original Sollya fits in [fit.sollya](fit.sollya) use 256-bit precision,
binary32 coefficients and constant term 1 over
`[-0x1.62e5d2p-2,0x1.62e52ep-2]`. This interval encloses the existing rounded
native reduction. Degrees 1–5 minimize relative polynomial error; degree 6 uses
an absolute-error fit whose free linear coefficient rounds to exactly one.
The coefficients are shared with native's selected exp fits. Reproduce with:

```sh
sollya --flush tests/exp_fit/fit.sollya
```

The retained selection was reproduced with Sollya 8.0, MPFR 4.2.2 and GMP 6.3.0.
`fpminimax` is heuristic, and `dirtyinfnorm` outputs are estimates of exact real
coefficient-polynomial error. Neither proves optimal binary32 coefficients or
bounds the rounded reduction, Horner operations and reconstruction.
Lower degrees intentionally exchange accuracy for fewer fused stages. Degree 1
is piecewise affine after range reduction and scaling. Every degree returns
exactly one for either input zero. There is no even/odd degree restriction.

Each row below is in descending Horner order and contains exact binary32 words:

| Degree | Coefficient words |
| --- | --- |
| 1 | `3f76382a 3f800000` |
| 2 | `3eff9d09 3f81cf0b 3f800000` |
| 3 | `3e2924d1 3f010eb2 3f80066b 3f800000` |
| 4 | `3d2a0993 3e2be74c 3f0001fb 3f7ffdd4 3f800000` |
| 5 | `3c07cfd2 3d2b9d0e 3e2aad40 3efffee3 3f7ffffb 3f800000` |
| 6 | `3ab6aafa 3c091f16 3d2aaa70 3e2aaa45 3f000000 3f800000 3f800000` |
| 7 | `3950eb8a 3ab6d3ab 3c08882e 3d2aaa32 3e2aaaab 3f000000 3f800000 3f800000` |

The common C++/HLSL coefficient record owns these words; the independent test
oracle retains a separate frozen table. The polynomial requires `Degree` fused
multiply-adds, plus the unchanged two reduction FMAs. Compile-time selection
adds no runtime branch. Reduction, cutoffs, scaling and the NaN-payload exception
retain their prior rules; the selected degree changes only the polynomial.

The [validation record](../../docs/validation.md) reports actual scalar, SIMD,
array, wide and Metal checks for each degree. Its bit agreement concerns the
specified FTZ graph, and does not make a correctly-rounded libm or exhaustive
accuracy claim. The broader native MPFR experiments motivated the default but
are not represented as FTZ execution results here.

These fitting scripts and degrees 1–6 are new project work under the repository's
dual BSD-2-Clause/Apache-2.0 license. Degree 7 retains the prior FTZ coefficient
words; existing source attribution and license notices remain in their files.
