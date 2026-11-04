#!/usr/bin/env python3
"""Original synthetic MDL7 rig: bound-check regression, NOT a game asset."""
import struct
from pathlib import Path
import sys
root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
p=struct.pack
size=(36,28,16,68,8,16,26,26,68,24)
bones=p('<H2x3f20s',65535,0,0,0,b'root')+p('<H2x3f20s',0,0,1,0,b'arm')
uv=b''.join(p('<2f',float(x),float(y)) for x,y in [(0,0),(1,0),(1,1),(0,1)])
tris=b''.join(p('<6HI',*t,0) for t in [(0,1,2,0,1,2),(0,2,3,0,2,3)])
verts=b''.join(p('<3fH3f',float(x),float(y),0.,0,0,0,1) for x,y in [(0,0),(1,0),(1,1),(0,1)])
framehead=p('<16sII',b'pose0',1,1)
frameverts=p('<3fH3f',0,0,0,2,0,0,1)
bmat=p('<16fH2x',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1)
frames=framehead+frameverts+bmat

def deformer(invalid=None):
    weight1=1.25 if invalid=='bad_weight' else .75
    verts1=[(0,weight1),(1,1.),(2,1.),(3,1.)]
    verts2=[(0,.25)]
    if invalid=='bad_vertex':verts2=[(55,.25)]
    element2bone=6 if invalid=='bad_bone' else 1
    a=p('<I20sI',0,b'root',len(verts1))+b''.join(p('<If',v,w) for v,w in verts1)
    b=p('<I20sI',element2bone,b'arm',len(verts2))+b''.join(p('<If',v,w) for v,w in verts2)
    return p('<BB2xIII',0,0,0,2,16+len(a)+len(b))+a+b

def file(bad=None):
    df=deformer(bad)
    if bad=='bad_size':
        df=df[:12]+p('<I',12)+df[16:]
    group_body=uv+tris+verts+frames+df
    mw=1 if bad=='bad_maxweights' else 2
    group=p('<BBBBI16s5I',1,1,mw,0,44+len(group_body),b'skin',0,4,2,4,1)+group_body
    total=48+len(bones)+len(group)
    header=p('<4s6I10H',b'MDL7',0,2,1,total,0,0,*size)
    return header+bones+group
(root/'MDL7_weighted_valid.mdl').write_bytes(file())
for bad in ['bad_weight','bad_vertex','bad_bone','bad_size','bad_maxweights']:
    (root/f'MDL7_{bad}.mdl').write_bytes(file(bad))
(root/'MDL7_truncated.mdl').write_bytes(file()[:-8])
