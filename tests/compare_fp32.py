#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
"""Compare flat little-endian FP32 fixture packets; only NaN payload/sign may differ."""
import argparse
from pathlib import Path
import struct
p=argparse.ArgumentParser(__doc__)
p.add_argument('first',type=Path);p.add_argument('second',type=Path)
a=p.parse_args();x=a.first.read_bytes();y=a.second.read_bytes()
if len(x)!=len(y) or len(x)%4:p.error('packet lengths must match and contain complete FP32 words')
nans=0
for i,((u,),(v,)) in enumerate(zip(struct.iter_unpack('<I',x),struct.iter_unpack('<I',y))):
    if u==v:continue
    if u&0x7fffffff>0x7f800000 and v&0x7fffffff>0x7f800000:nans+=1;continue
    raise SystemExit(f'word {i}: {u:08x} != {v:08x}')
print(f'{len(x)//4} words match; {nans} permitted NaN representation differences')
