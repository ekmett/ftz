# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
"""Exact-rational bounds for the observed native atan2 arithmetic stages."""
from fractions import Fraction as Q
from pathlib import Path
import json,re,struct
root=Path(__file__).resolve().parents[2]
scalar=(root/'src/shared/ftz/math/atan2.h').read_text()
scalar_h=scalar.split('float h=',1)[1].split('float q=',1)[0]
coefficients=re.findall(r'fp32_decode\((0x[0-9a-f]+)u\)',scalar_h)
assert len(coefficients)==8
native=(root/'src/cxx/ftz/math.h').read_text().split('std::array<V,N> atan2_ftz',1)[1]
native_h=native.split('auto [...h]=',1)[1].split('auto const [...q]',1)[0]
assert re.findall(r'constant\((0x[0-9a-f]+)u\)',native_h)==coefficients
approx=(root/'src/shared/ftz/math/approx.h').read_text()
assert '0x7ef311c3u-mb' in approx and '0x7ef311c3u)-mb' in native
assert native.count('r=fma(r,e,r)')==3 and native.count('fma(-m,r,V(1.0f))')==3
# For m=1+t and t in [0,1), the bit-derived reciprocal seed has two affine
# pieces. Enclose their exact products using endpoints and the quadratic peak.
breakpoint=Q(0x7311c3,2**23)
products=[]
for left,right,c,k in [(Q(0),breakpoint,Q(0xf311c3,2**24),Q(1,2)),
                        (breakpoint,Q(1),Q(0x17311c3,2**25),Q(1,4))]:
    points=[left,right];peak=(c-k)/(2*k)
    if left<=peak<=right:points.append(peak)
    products.extend((1+t)*(c-k*t)for t in points)
u=Q(1,2**24)
error=max(abs(1-p)for p in products)*(1+u)
assert error<Q(1,16)
errors=[error]
# Exact residual epsilon=1-m*r, rounded e and updated r yield the following
# conservative relative residual bound. It remains within1/16 throughout.
for _ in range(3):
    error=error*error+(1+error)*u*error+u*(1+error*error+(1+error)*u*error)
    assert error<Q(1,16);errors.append(error)
# Every reciprocal stays in (15/32,17/16). Its binary32 grid is at worst2^-25;
# m lies on2^-23. A nonzero exact residual is therefore at least2^-48, normal.
# The update r+r*e cannot cancel because |e|<1/16.
assert Q(15,32)*(1-Q(1,16))>Q(1,4)
assert Q(1,2**48)>Q(1,2**126)
def value(word):
    return Q.from_float(struct.unpack('<f',struct.pack('<I',int(word,16)))[0])
lo=hi=value(coefficients[0]);minimum=abs(lo);maximum=abs(lo)
for word in coefficients[1:]:
    possibilities=[a*z+value(word)for a in(lo,hi)for z in(Q(0),Q(1))]
    lo,hi=min(possibilities),max(possibilities)
    error=max(abs(lo),abs(hi))/2**23;lo-=error;hi+=error
    assert lo>0 or hi<0
    minimum=min(minimum,abs(lo),abs(hi));maximum=max(maximum,abs(lo),abs(hi))
assert hi<0 and lo>Q(-1,2) and minimum>Q(1,2**10)
# Ratios are canonical positive/zero and clamped <=1 after integer rescaling.
# Observed polynomial lanes exceed2^-12. Their square, z*h and final angle
# stay normal. Hardware tiny lanes may flush unobserved stages and select t.
assert Q(1,2**24)*(1-u)*min(abs(lo),abs(hi))>Q(1,2**28)
assert (1+(1+u)**2*lo)*(1-u)>Q(1,2)
# Before quadrant reconstruction angle is nonnegative and <=1. The two folds
# subtract from pi/2 and pi, leaving normal magnitudes (or an unchanged zero).
assert value('0x3fc90fdb')-1>Q(1,2)
assert value('0x40490fdb')-value('0x3fc90fdb')>1
print(json.dumps({'seed_and_newton_relative_error_bounds':[float(x)for x in errors],
    'reciprocal_residual_nonzero_min':str(Q(1,2**48)),
    'horner_min_live_abs_gt':float(minimum),'horner_max_live_abs_lt':float(maximum),
    'native_coefficients_equal_scalar':True,'newton_steps':3},indent=2))
