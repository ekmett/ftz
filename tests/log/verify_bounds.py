# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
"""Normality bounds for observed stages of the scalar log/log1p graphs."""
from pathlib import Path
from fractions import Fraction as Q
import json,re,struct
root=Path(__file__).resolve().parents[2]
scalar=(root/'src/shared/ftz/math/log.h').read_text()
parts=scalar.split('log1p_kernel',1)[1].split('return fp32_fma',1)[0].split('    else {')
def value(word):
 return Q.from_float(struct.unpack('<f',struct.pack('<I',int(word,16)))[0])
rows=[];proof=[];bounds=[]
for part in parts:
 terms=re.findall(r'h = (?:fp32_decode\((0x[0-9a-f]+)u\)|fp32_fma<true,Hardware>\(h,t,fp32_decode\((0x[0-9a-f]+)u\)\))',part)
 row=[a or b for a,b in terms];rows.append(row)
 lo=hi=value(row[0]);minimum=abs(lo);maximum=abs(lo)
 for word in row[1:]:
  endpoints=[a*b+value(word) for a in (lo,hi) for b in (Q(-1),Q(1))]
  lo,hi=min(endpoints),max(endpoints)
  assert lo>Q(1,2**126) or hi<-Q(1,2**126)
  error=max(abs(lo),abs(hi))/2**23;lo-=error;hi+=error
  assert lo>0 or hi<0
  minimum=min(minimum,abs(lo),abs(hi));maximum=max(maximum,abs(lo),abs(hi))
 assert minimum>Q(1,2**26) and maximum<1 and hi<0
 bounds.append((lo,hi));proof.append({'coefficients':len(row),'min_live_abs_gt':float(minimum),'max_live_abs_lt':float(maximum)})
assert list(map(len,rows))==[13,15]
native=(root/'src/cxx/ftz/math.h').read_text().split('std::array<V,N> log_ftz',1)[1]
columns=re.findall(r'coefficient\(negative,(0x[0-9a-f]+)u,(0x[0-9a-f]+)u\)',native)
assert columns[:2]==[('0x40800000','0x40000000'),('0x3f800000','0xbf800000')]
columns=columns[2:];assert len(columns)==15
for i,row in enumerate(rows):
 assert [int(col[i],16) for col in columns]==[0]*(15-len(row))+[int(word,16)for word in row]
# Direct log1p results at |x|<=2^-25 return the original canonical word.
# Manual kernels mask these lanes before squaring; hardware kernels may flush
# their unobserved square. The remaining direct arguments exceed 2^-25. Reduced log
# arguments are zero or have magnitude >=2^-24 (the predecessor of one).
# In either case the square is zero or normal, with magnitude <=1.
assert Q(1,2**50)>Q(1,2**126)
# Both affine centers are exactly representable. At those centers t=0;
# neighboring binary32 arguments bound every nonzero cancellation residual.
residuals=[]
for center,scale in [(Q(-1,4),Q(4)),(Q(1,2),Q(2))]:
 bits=struct.unpack('<I',struct.pack('<f',float(center)))[0]
 neighbors=[value(hex(bits-1)),value(hex(bits+1))]
 residual=min(abs(center-n)for n in neighbors)*scale
 assert residual>=Q(1,2**24);residuals.append(str(residual))
# Negative arguments add two negative terms in x + round(x*x)*h.
# Positive arguments retain more than .49*x before the final RNE step,
# using an outward bound on the square and the complete positive h chain.
u=Q(1,2**24)
positive_factor=1-(1+u)*abs(bounds[1][0])
assert positive_factor>Q(49,100)
minimum_polynomial=Q(1,2**25)*positive_factor*(1-u)
assert minimum_polynomial>Q(1,2**27)
# Thus each rounded nonzero polynomial is a multiple of 2^-50. The integer
# exponent is in [-126,128], and both ln2 pieces are binary32 constants.
# Their exact FMA sums lie on grids no finer than 2^-50 (low) and 2^-73
# (final); every nonzero residual is normal, even at cancellation.
low=value('0x35bfbe8e');high=value('0x3f317200')
assert low.denominator<=2**50 and high.denominator<=2**73
assert Q(1,2**50)>Q(1,2**126) and Q(1,2**73)>Q(1,2**126)
# Outer negative log1p uses Sterbenz on (-1,-.5): its smallest positive
# sum is 2^-24. For x>1, RNE(1+x) cannot exceed maximum finite binary32.
assert Q(1,2**24)>Q(1,2**126)
print(json.dumps({'horner':proof,'affine_minimum_nonzero_residuals':residuals,
 'square_nonzero_min':str(Q(1,2**50)),
 'polynomial_nonzero_abs_gt':float(minimum_polynomial),
 'reconstruction_nonzero_min_bound':str(Q(1,2**73)),
 'native_coefficients_equal_scalar':True,'leading_zero_steps_exact':True},indent=2))
