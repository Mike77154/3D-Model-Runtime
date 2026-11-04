"""Test-only ARG 1.1 and 1.2 generation using the original fragMOTION SDK field order.
The model itself is synthetic; it cannot validate production exporter quirks.
"""
from pathlib import Path
import struct
P = Path(__file__).parent
u32=lambda x:struct.pack('<I',x)
u16=lambda x:struct.pack('<H',x)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def model(version, bad_index=False):
    # Character header, no animation/bones
    head=u32(version)+b'arg00'+b'TestCharacter'.ljust(64,b'\0')+u32(0)+u32(0)
    idx=900 if bad_index else 2
    if version>=0x10002:
        tri=struct.pack('<HHHHHHH',0,0,0,0,0,1,idx)
    else:
        tri=struct.pack('<HHHHHH',0,0,0,0,1,idx)
    # Triangles are per-corner UV and normals
    tri+=f(0,0,1, 0,0,1, 0,0,1)
    tri+=f(0,0, 1,0, 0,1)
    tri+=f(0,0,1)
    if version>=0x10002:tri+=u32(0)*6+u16(0)*3
    assert len(tri)==(116 if version>=0x10002 else 84)
    verts=b''.join(f(*p,1,0,0)+b'\x00\x00\x00\x00'+u16(0) for p in ((0,0,0),(1,0,0),(0,1,0)))
    assert len(verts)==90
    return head+u32(1)+tri+u32(3)+verts+u32(0)*3 + (u32(0) if version>=0x10002 else b'')
for version in (0x10001,0x10002):
    (P/f'synthetic_arg_{version:x}.arg').write_bytes(model(version))
(P/'bad_index.arg').write_bytes(model(0x10002,True))
(P/'truncated.arg').write_bytes(model(0x10002)[:-5])
