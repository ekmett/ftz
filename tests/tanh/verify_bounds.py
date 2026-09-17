# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
"""Exact-rational enclosure of the existing live scalar tanh Horner chains."""
from pathlib import Path
from fractions import Fraction as Q
import re, struct, json
root=Path(__file__).resolve().parents[2]
text=(root/'src/shared/ftz/math/tanh.h').read_text()
pieces=re.split(r'if \(magnitude <= 0x3f800000u\)|else if \(magnitude <= [^\n]+|    else \{',text)[1:]
def value(word):
    return Q.from_float(struct.unpack('<f',struct.pack('<I',int(word,16)))[0])
rows=[]
for piece in pieces:
    terms=re.findall(r'h = (?:fp32_decode\((0x[0-9a-f]+)u\)|fp32_fma<true,Hardware>\(h, t, fp32_decode\((0x[0-9a-f]+)u\)\))',piece)
    rows.append([a or b for a,b in terms])
assert list(map(len,rows))==[9,11,10,10,12,10,8]
affine=re.findall(r't = fp32_fma<true,Hardware>\(z, fp32_decode\((0x[0-9a-f]+)u\), fp32_decode\((0x[0-9a-f]+)u\)\)',text)
assert len(affine)==7
scales=[value(scale) for scale,_ in affine]
biases=[value(bias) for _,bias in affine]
# The inclusive squared endpoints are exactly representable. RNE is monotone,
# so the rounded square and affine operation stay within these exact bounds.
x_bounds=[Q(x) for x in (0,1,2,3,4,6,8,10)]
bounds=[(a*a*scale+bias,b*b*scale+bias)
        for a,b,scale,bias in zip(x_bounds,x_bounds[1:],scales,biases)]
assert bounds==[(-1,1),(Q(-3,4),Q(3,4)),(Q(-5,8),Q(5,8)),(Q(-7,8),Q(7,8)),
                (Q(-5,8),Q(5,8)),(Q(-7,8),Q(7,8)),(Q(-9,16),Q(9,16))]
proof=[]
for row,(low,high) in zip(rows,bounds):
    low,high=Q(low),Q(high);lo=hi=value(row[0]);minimum=abs(lo);maximum=abs(lo)
    for word in row[1:]:
        endpoints=[a*b+value(word) for a in (lo,hi) for b in (low,high)]
        lo,hi=min(endpoints),max(endpoints)
        assert lo>Q(1,2**126) or hi<-Q(1,2**126), 'Exact fused result may enter the underflow strip'
        # A full relative binary32 ULP is a conservative bound for RNE FMA.
        error=max(abs(lo),abs(hi))/2**23;lo-=error;hi+=error
        assert lo>0 or hi<0
        minimum=min(minimum,abs(lo),abs(hi));maximum=max(maximum,abs(lo),abs(hi))
    assert minimum>Q(1,2**22) and maximum<2
    assert lo>Q(1,16), 'Final positive h lower bound required for normal x*h'
    proof.append({'coefficients':len(row),'t_bounds':[str(low),str(high)],
                  'all_live_h_min_abs_gt':float(minimum),'all_live_h_max_abs_lt':float(maximum)})
# The native columns are the same coefficient rows, only zero-padded at the top.
native=(root/'src/cxx/ftz/math.h').read_text().split('std::array<V,N> tanh_ftz',1)[1]
columns=re.findall(r'coefficient\(interval,((?:0x[0-9a-f]+u,?)+)\)',native)
assert len(columns)==14
columns=[[x[:-1] for x in line.split(',')] for line in columns]
assert all(len(column)==7 for column in columns)
assert list(zip(columns[0],columns[1]))==affine, 'Native affine words differ'
columns=columns[2:]
for i,row in enumerate(rows):
    assert [int(column[i],16) for column in columns]==[0]*(12-len(row))+list(map(lambda x:int(x,16),row))
# Each affine center is representable in binary32. Its nearest neighbors prove
# the smallest nonzero exact residual; rounding cannot turn it subnormal.
centers=[-bias/scale for bias,scale in zip(biases,scales)]
assert centers==[Q(1,2),Q(5,2),Q(13,2),Q(25,2),Q(26),Q(50),Q(82)]
residuals=[]
for center,scale in zip(centers,scales):
    bits=struct.unpack('<I',struct.pack('<f',float(center)))[0]
    below=value(hex(bits-1));above=value(hex(bits+1))
    residual=min(center-below,above-center)*scale
    assert residual>=Q(1,2**24);residuals.append(str(residual))
print(json.dumps({'regions':proof,'minimum_nonzero_affine_residuals':residuals,
                 'affine_centers':[str(x) for x in centers],
                 'native_affine_and_coefficients_equal_scalar':True,'leading_zero_steps_exact':True},indent=2))
