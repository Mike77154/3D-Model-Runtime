#!/usr/bin/env python3
"""Synthetic SE1 format examples, for native-code regression; no game assets."""
from pathlib import Path
import struct,zipfile
D=Path(__file__).resolve().parent/'fixtures';D.mkdir(exist_ok=True)
def p(fmt,*a):return struct.pack('<'+fmt,*a)
def st(s):d=s.encode('utf-8');return p('I',len(d))+d
# A skinned mesh (triangle), with one UV map and no weights/animations.
mesh=b'MESH'+p('II',11,1)+st('synthetic.am')+p('fII',12,0,3)
mesh+=p('fffI',0,0,0,0)+p('fffI',1,0,0,0)+p('fffI',0,1,0,0)
mesh+=p('fffI',0,0,1,0)*3
mesh+=p('I',1)+st('UV0')+p('ffffff',0,0,1,0,0,1)
mesh+=p('I',1)+st('mat')+p('III',0,3,1)+p('III',0,1,2)+p('I',0)
mesh+=p('II',0,0) # no bone weights, no morph weights
(D/'skeleton_mesh.bm').write_bytes(mesh)
(D/'skeleton_mesh12.bm').write_bytes(mesh[:4]+p('I',12)+mesh[8:])
# Minimal SKEL v6 with a root bone and a child bone
skel=b'SKEL'+p('II',6,1)+st('synthetic.as')+p('fI',100,2)
for name,parent in [('ROOT',''),('HEAD','ROOT')]:
 skel+=st(name)+st(parent)+p('f'*12,*([0.0]*12))
 skel+=p('f'*7,*([0.0]*3+[0.0,0.0,0.0,1.0]))+p('ff',0.1,1.0)
(D/'root_child.skl').write_bytes(skel)
# Skeletal ANIM v14, one idle clip with one bone position + rotation
asanim=b'ANIM'+p('II',14,1)+st('synthetic.aa')+st('IDLE')+p('fIfII',0.1,2,0.1,0,0)
asanim+=p('I',1)+st('ROOT')+p('f'*12,*([0.0]*12))+p('I',1)
asanim+=p('H2xfff',0,0,0,0)+p('I',1)+p('H2xffff',0,0,0,0,1)+p('f',0.3)+p('I',0)
(D/'idle.ba').write_bytes(asanim)
# Classic sprite animation ADAT
(D/'independent.ani').write_bytes(b'ADAT'+p('I',1)+b'RUN\0'.ljust(32,b'\0')+p('fI',0.2,3)+p('III',0,1,2))
# TEX v4 RGB and v3 RGBA4444
(D/'tiny4.tex').write_bytes(b'TVER'+p('I',4)+b'TDAT'+p('IIIIII',0,2,2,1,0,1)+b'FRMS'+bytes([255,0,0,0,255,0,0,0,255,255,255,255]))
(D/'tiny3.tex').write_bytes(b'TVER'+p('I',3)+b'TDAT'+p('IIIIIIII',1,2,2,2,2,0,10,1)+b'FRMS'+p('H'*5,0xf00f,0x0f0f,0x00ff,0xffff,0))
(D/'basic.smc').write_text('NAME "Tester";\n{ MESH TFNM "synthetic.bm"; }\n')
with zipfile.ZipFile(D/'sample.gro','w') as z:
 z.write(D/'skeleton_mesh.bm','Models/skeleton_mesh.bm',compress_type=zipfile.ZIP_DEFLATED)
 z.write(D/'tiny4.tex','Textures/tiny4.tex',compress_type=zipfile.ZIP_STORED)
print('resource fixtures generated')
