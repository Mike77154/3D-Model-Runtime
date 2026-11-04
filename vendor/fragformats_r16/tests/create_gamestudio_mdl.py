#!/usr/bin/env python3
"""Synthetic binary corpus for MDL3, MDL4, MDL5 and MDL7 identification."""
from pathlib import Path
import struct
import sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path('tests/output/gs')
root.mkdir(parents=True,exist_ok=True)
uv=[(0,0),(32,0),(32,32),(0,32)]
tris=[(0,1,2,0,1,2),(0,2,3,0,2,3)]
vertices=[(0,0,0),(1,0,0),(1,1,0),(0,1,0)]
for version in [3,4,5]:
 for fmt in ([0] if version==3 else [0,2]):
  output=bytearray(b'MDL'+str(version).encode())
  output+=struct.pack('<I3f3ff3f9I',0,1,1,1,0,0,0,0,0,0,0,1,32,32,4,2,2,4,0,0)
  assert len(output)==84,len(output)
  if version==5:
   output+=struct.pack('<III',10,32,32)
   for side in [32,16,8,4]:output+=bytes(side*side*2)
  else:
   output+=struct.pack('<I',0)+bytes(32*32)
  for a,b in uv:output+=struct.pack('<hh',a,b)
  for t in tris:output+=struct.pack('<6H',*t)
  for fi in range(2):
   output+=struct.pack('<I',fmt)
   if fmt==0:
    output+=bytes(4)+bytes(4)
   else:
    output+=bytes(8)+bytes(8)
   output+=f'pose{fi}'.encode().ljust(16,b'\x00')
   for x,y,z in vertices:
    xx=x+fi
    if fmt==0:output+=struct.pack('<4B',xx,y,z,5)
    else:output+=struct.pack('<3HBB',xx,y,z,5,0)
  name=f'MDL{version}_P{8 if fmt==0 else 16}.mdl'
  (root/name).write_bytes(output)
# Valid structure: MDL7 header, bones=0, groups=0 (not a mesh).
size=48
mdl7=struct.pack('<4s6I10H',b'MDL7',0,0,0,size,0,0,28,32,16,68,8,26,16,16,68,24)
assert len(mdl7)==48
(root/'MDL7_header_only.mdl').write_bytes(mdl7)
# MDL7 synthetic real triangular group, one embedded RGB565 skin.
group=bytearray()
for xyz in [(0.0,0.0),(1.0,0.0),(1.0,1.0),(0.0,1.0)]:
    group+=struct.pack('<2f',*xyz)
for face in tris:
    group+=struct.pack('<6HI3HI',*face,0,0,0,0,0)
for x,y,z in vertices:
    group+=struct.pack('<3fHH',float(x),float(y),float(z),0,0)
skin=struct.pack('<B3xII16s',2,2,2,b'testskin')+bytes(8)
content=skin+group[:32]+group[32:]
# group vertices follow triangle table (uvs first, then triangles, then 3D positions).
group_header=struct.pack('<4BI16s5I',1,0,0,0,44+len(content),b'sample',1,4,2,4,0)
header=struct.pack('<4s6I10H',b'MDL7',0,0,1,48+len(group_header)+len(content),0,0,28,28,16,68,8,26,16,16,68,24)
(root/'MDL7_mesh.mdl').write_bytes(header+group_header+content)
# Material-only MDL7 with zero UVs, matching observed Assimp MDL7 layout.
material_skin=struct.pack('<B3xII16s',16,0,0,b'Skin1')+bytes(68)
material_tris=bytearray()
for tri in tris:
    material_tris+=struct.pack('<6HI',*tri,0)  # six index shorts + material ID
material_verts=b''.join(struct.pack('<3fH3f',float(x),float(y),float(z),0,0.0,0.0,1.0) for x,y,z in vertices)
material_content=material_skin+material_tris+material_verts
material_group=struct.pack('<4BI16s5I',1,0,0,0,44+len(material_content),b'mat_only',1,0,2,4,0)
material_header=struct.pack('<4s6I10H',b'MDL7',0,0,1,48+len(material_group)+len(material_content),0,0,36,28,16,68,8,16,26,26,68,24)
(root/'MDL7_noUV_material.mdl').write_bytes(material_header+material_group+material_content)

(root/'MDL2_unsupported.mdl').write_bytes(b'MDL2'+bytes(80))
(root/'MDL3_truncated.mdl').write_bytes((root/'MDL3_P8.mdl').read_bytes()[:-4])
(root/'MDL3_invalid_index.mdl').write_bytes((root/'MDL3_P8.mdl').read_bytes()[:84+4+32*32+4*4]+struct.pack('<6H',999,1,2,0,1,2)+(root/'MDL3_P8.mdl').read_bytes()[84+4+32*32+4*4+12:])
print('generated',len(list(root.glob('*.mdl'))),'synthetic fixtures')
