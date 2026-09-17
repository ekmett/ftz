#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
"""Make flat uint4 packets for signed-FTZ shader add/sub admission."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

SIGN = 0x80000000
INF = 0x7f800000


def canonical(word):
    return word & SIGN if word & 0x7fffffff < 0x00800000 else word


def add(a, b):
    """Exact integer sum, binary32 RNE, then signed output FTZ."""
    a, b = canonical(a), canonical(b)
    aa, bb = a & 0x7fffffff, b & 0x7fffffff
    if aa > INF or bb > INF:
        return 0x7fc00000
    if aa == INF or bb == INF:
        if aa == bb == INF and (a ^ b) & SIGN:
            return 0x7fc00000
        return a if aa == INF else b

    def units(word):
        magnitude = word & 0x7fffffff
        if magnitude == 0:
            return 0
        value = ((magnitude & 0x7fffff) | 0x800000) << ((magnitude >> 23) - 1)
        return -value if word & SIGN else value

    value = units(a) + units(b)
    if value == 0:
        return (a & b) & SIGN
    sign = SIGN if value < 0 else 0
    value = abs(value)
    # Subnormal sums are exact integer multiples of 2^-149, so no rounding
    # midpoint can turn a tiny sum into a normal result.
    if value < 0x800000:
        return sign
    shift = max(0, value.bit_length() - 24)
    mantissa = value >> shift
    if shift:
        remainder = value - (mantissa << shift)
        halfway = 1 << (shift - 1)
        if remainder > halfway or (remainder == halfway and mantissa & 1):
            mantissa += 1
    if mantissa == 0x1000000:
        mantissa >>= 1
        shift += 1
    exponent = shift + 1
    return sign | INF if exponent >= 255 else sign | (exponent << 23) | (mantissa & 0x7fffff)


def cases():
    magnitudes = (0, 1, 0x7fffff, 0x800000, 0x800001, 0xffffff,
                  0x1000000, 0x3f000000, 0x3f7fffff, 0x3f800000,
                  0x3f800001, 0x7f7fffff, INF, INF + 1, 0x7fc12345)
    words = [word | sign for word in magnitudes for sign in (0, SIGN)]
    for a in words:
        for b in words:
            yield a, b
    for exponent in range(1, 255):
        for fraction in (0, 1, 2, 0x3fffff, 0x7ffffe, 0x7fffff):
            a = exponent << 23 | fraction
            for delta in (-2, -1, 0, 1, 2):
                b = min(INF - 1, max(0, a + delta))
                for sign_a in (0, SIGN):
                    for sign_b in (0, SIGN):
                        yield a | sign_a, b | sign_b
    state = 0x93ba2c67
    def next_word():
        nonlocal state
        state ^= (state << 13) & 0xffffffff
        state ^= state >> 17
        state ^= (state << 5) & 0xffffffff
        return state
    for _ in range(65536):
        yield next_word(), next_word()


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    pairs = list(cases())
    inputs = b''.join(struct.pack('<4I', a, b, 0, 0) for a, b in pairs)
    expected = b''.join(struct.pack('<4I', add(a, b), add(a, b),
                                    add(a, b ^ SIGN), add(a, b ^ SIGN)) for a, b in pairs)
    (args.output / 'input.bin').write_bytes(inputs)
    (args.output / 'expected.bin').write_bytes(expected)
    report = {'records': len(pairs), 'stride': 16, 'format': 'flat little-endian uint4',
              'input_sha256': hashlib.sha256(inputs).hexdigest(),
              'expected_sha256': hashlib.sha256(expected).hexdigest()}
    (args.output / 'bank.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
