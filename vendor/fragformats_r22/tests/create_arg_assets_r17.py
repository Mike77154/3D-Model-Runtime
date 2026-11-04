"""Synthetic ARG 1.2 with mesh material, external color texture and clip event."""
from pathlib import Path
import struct
P=Path(__file__).parent
I=lambda v:struct.pack('<I',v)
H=lambda v:struct.pack('<H',v)
F=lambda *v:struct.pack('<'+'f'*len(v),*v)
S=lambda s,n:s.encode('ascii').ljust(n,b'\0')
name='TinyCharacter'
header=I(0x10002)+b'arg00'+S(name,64)
event=S('footstep',64)+I(5)+S('step.wav',256)
anim=S('walk',64)+H(0)+F(24)+b'\x01'+I(10)+I(0)+I(9)+I(1)+event
tri=H(0)+H(0)+H(0)+H(0)+H(0)+H(1)+H(2)
tri+=F(0,0,1)*3+F(0,0,1,0,0,1)+F(0,0,1)
tri+=I(0xFFFF0000)+I(0xFF00FF00)+I(0xFF0000FF)
tri+=I(0xFF101010)*3+H(1)*3
assert len(tri)==116
verts=b''.join(F(*p,1,0,0)+b'\x00'*4+H(0) for p in [(0,0,0),(1,0,0),(0,1,0)])
tex=S('ColorMap',64)+H(0)+I(0)+S('tiny.png',1024)
material=S('MatColor',64)+H(0)+I(0x00FFFFFF)+I(0x00000000)+I(0x00808080)+I(0)+F(32)+F(1)+F(0)+I(1)+H(0)+H(0)
out=header+I(1)+anim+I(0)+I(1)+tri+I(3)+verts+I(1)+tex+I(0)+I(1)+material+I(0)
(P/'r17_material_event.arg').write_bytes(out)
(P/'r17_material_event_truncated.arg').write_bytes(out[:-1])
print('Generated', len(out),'bytes ARG1.2')
