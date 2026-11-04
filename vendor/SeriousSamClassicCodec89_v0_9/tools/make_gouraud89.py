#!/usr/bin/env python3
"""Generate optional 256x3 Q15 user-local lookup from Serious Engine Normals.cpp.

This tool does not package the original GPL data: users point it to a lawful
copy of the upstream source that they have already acquired.
"""
import argparse
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import re
import struct
import zipfile


def convert_source(source):
    match = re.search(r'avGouraudNormals\s*\[\s*MAX_GOURAUDNORMALS\s*\]\s*=\s*\{(.*?)\};', source, re.S)
    if not match:
        raise ValueError('avGouraudNormals[] not found')
    triples = re.findall(r'FLOAT3D\(\s*([-+]?\d+(?:\.\d+)?)f?\s*,\s*([-+]?\d+(?:\.\d+)?)f?\s*,\s*([-+]?\d+(?:\.\d+)?)f?\s*\)', match.group(1))
    if len(triples) != 256:
        raise ValueError(f'expected 256 normals, found {len(triples)}')
    values = []
    for triple in triples:
        ints = []
        for word in triple:
            val = int((Decimal(word)*Decimal(32767)).to_integral_value(rounding=ROUND_HALF_UP))
            if not -32767 <= val <= 32767:
                raise ValueError('normal component outside range')
            ints.append(val)
        length2 = sum(v*v for v in ints)
        if not 1065000000 <= length2 <= 1090000000:
            raise ValueError('normal is not unit length')
        values += ints
    return struct.pack('<768h', *values)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('normals_cpp', nargs='?', help='Serious Engine Sources/Engine/Models/Normals.cpp')
    cli.add_argument('output', help='Locally generated ss89_gouraud.q15 (1536 bytes)')
    cli.add_argument('--engine-zip', help='Read the original Normals.cpp directly from a Serious-Engine source ZIP')
    a = cli.parse_args()
    if a.engine_zip:
        with zipfile.ZipFile(a.engine_zip) as archive:
            names = [name for name in archive.namelist() if name.endswith('Sources/Engine/Models/Normals.cpp')]
            if len(names) != 1:
                raise ValueError('expected exactly one original Normals.cpp inside ZIP')
            source = archive.read(names[0]).decode('utf-8', errors='replace')
    elif a.normals_cpp:
        source = Path(a.normals_cpp).read_text(encoding='utf-8', errors='replace')
    else:
        cli.error('supply Normals.cpp or --engine-zip')
    b = convert_source(source)
    Path(a.output).write_bytes(b)
    print('Generated user-local Gouraud table: 256 vectors, 1536 bytes')


if __name__ == '__main__':
    main()
