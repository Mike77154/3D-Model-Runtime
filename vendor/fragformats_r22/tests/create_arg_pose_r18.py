"""Test-only ARG 1.1 single-bone animation with three independent key tracks."""
from pathlib import Path
import struct
P=Path(__file__).parent
U=lambda v:struct.pack('<I',v)
H=lambda v:struct.pack('<H',v)
F=lambda *v:struct.pack('<'+'f'*len(v),*v)
S=lambda t,n:t.encode('ascii').ljust(n,b'\0')
K=lambda frame,x,y,z:U(frame)+F(x,y,z)
ident=b''.join(F(1 if i%5==0 else 0) for i in range(16))
anim=S('walk',64)+H(0)+F(20)+b'\0'+U(11)+U(0)+U(10)+U(0)
bone=S('root',64)+b'\1'+ident+H(0)+F(*([0]*6))
assert len(bone)==155
bone+=U(0)+U(0)+U(1)   # no attachments/children; one animation ref
bone+=H(0)
bone+=U(2)+K(0,0,0,0)+K(10,1,0,0)
bone+=U(2)+K(0,0,0,0)+K(10,10,0,0)
bone+=U(2)+K(0,1,1,1)+K(10,2,2,2)
tri=H(0)+H(0)+H(0)+H(0)+H(1)+H(2)+F(0,0,1)*3+F(0,0,1,0,0,1)+F(0,0,1)
assert len(tri)==84
verts=b''.join(F(*p,1,0,0)+b'\1\0\0\0'+H(0) for p in [(0,0,0),(1,0,0),(0,1,0)])
source=U(0x10001)+b'arg00'+S('PoseCase',64)+U(1)+anim+U(1)+bone+U(1)+tri+U(3)+verts+U(0)*3
(P/'r18_pose.arg').write_bytes(source)
# Illegal bone index in first vertex; geometry validates skeleton references.
bad=bytearray(source)
vert_start=len(source)-3*30-12
bad[vert_start+24]=99
(P/'r18_bad_bone.arg').write_bytes(bad)
print('R18 pose fixture',len(source),'bytes')
