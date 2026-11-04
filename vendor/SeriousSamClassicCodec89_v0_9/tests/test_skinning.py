#!/usr/bin/env python3
"""Test end-to-end C89 SKA pose against analytically known rotations.
Fixture is newly generated, contains no proprietary models or copied source.
"""
import math
import struct
import subprocess
from pathlib import Path

BASE=Path(__file__).resolve().parents[1]
OUT=BASE/'tests'/'fixtures'/'ska_skin'
OUT.mkdir(parents=True,exist_ok=True)
BIN=BASE/'bin'/'ss89pose'
def pack(fmt,*items):return struct.pack('<'+fmt,*items)
def u32(n):return pack('I',n)
def txt(s): b=s.encode('ascii');return u32(len(b))+b
def vec(x,y,z):return pack('fff',x,y,z)
IDENT=(1,0,0,0,0,1,0,0,0,0,1,0)
# Triangle in X/Y plane. Root and child weight maps plus split influence.
mesh=b'MESH'+u32(11)+u32(1)+txt('testmesh.am')+pack('fII',100,0,3)
for v in [(0,0,0),(2,0,0),(2,1,0)]:mesh+=vec(*v)+u32(0)
for i in range(3):mesh+=vec(0,0,1)+u32(0)
mesh+=u32(0) # uv sets
mesh+=u32(1)+txt('Material')+pack('III',0,3,1)+pack('III',0,1,2)+u32(0)
mesh+=u32(2)
for name,weights in [('ROOT',[(0,1.0),(2,.5)]),('CHILD',[(1,1.0),(2,.5)])]:
 mesh+=txt(name)+u32(len(weights))
 for v,w in weights:mesh+=pack('If',v,w)
mesh+=u32(0) # no morph maps
(OUT/'blend.bm').write_bytes(mesh)
# Rest global bone matrix and local poses.
skel=b'SKEL'+u32(6)+u32(1)+txt('test.as')+pack('fI',100,2)
for name,parent,tx in [('ROOT','',0),('CHILD','ROOT',1)]:
 mat=list(IDENT);mat[3]=tx
 skel+=txt(name)+txt(parent)+pack('12f',*mat)+vec(tx,0,0)+pack('4f',1,0,0,0)+pack('ff',0,1)
(OUT/'blend.skl').write_bytes(skel)
# As authored by original ANIM v14: frame num u16 + two padding bytes, quaternion w x y z.
anim=b'ANIM'+u32(14)+u32(1)+txt('blend.aa')+txt('BEND')+pack('fIfII',.1,2,.005,1,0)
anim+=u32(2)
for name,tx in [('ROOT',0),('CHILD',1)]:
 anim+=txt(name)+pack('12f',*IDENT)+u32(2)
 for frame in (0,1):anim+=pack('Hxxfff',frame,tx,0,0)
 anim+=u32(2)
 for frame in (0,1):
  w=math.sqrt(.5) if name=='CHILD' and frame==1 else 1
  z=math.sqrt(.5) if name=='CHILD' and frame==1 else 0
  anim+=pack('Hxxffff',frame,w,0,0,z)
 anim+=pack('f',0) # no offset scaling
anim+=u32(0) # zero morph envelope
(OUT/'bend.ba').write_bytes(anim)

def invoke(frame,sub=0,loop=False):
 path=OUT/f'frame_{frame}_{sub}.obj'
 args=[str(BIN),str(OUT/'blend.bm'),str(OUT/'blend.skl'),str(OUT/'bend.ba'),'--frame',str(frame),'--sub',str(sub),'--obj',str(path)]
 if loop:args.append('--loop')
 cmd=subprocess.run(args,capture_output=True,text=True)
 assert cmd.returncode==0,cmd.stdout+'\n'+cmd.stderr
 return [tuple(map(float,line.split()[1:])) for line in path.read_text().splitlines() if line.startswith('v ')]

def near(a,b,tol=.01):assert all(abs(x-y)<tol for x,y in zip(a,b)),(a,b)
for frame,sub,expected in [(0,0,(2,0,0)),(1,0,(1,1,0)),(0,32768,(1+math.sqrt(.5),math.sqrt(.5),0))]:
 v=invoke(frame,sub)
 near(v[0],(0,0,0));near(v[1],expected)
 print('PASS frame',frame,'sub',sub,'child vertex',v[1])
near(invoke(1)[2],(1,1,0))
near(invoke(2,0,True)[1],(2,0,0))
print('PASS mixed weights; looping wrap')
# Truncated input must fail safely.
(OUT/'broken.ba').write_bytes(anim[:45])
p=subprocess.run([str(BIN),str(OUT/'blend.bm'),str(OUT/'blend.skl'),str(OUT/'broken.ba')],capture_output=True)
assert p.returncode!=0
print('PASS invalid/truncated ANIM rejected')
# Check non-topological serialization: child listed before root.
def bonechunk(name,parent,tx):
 mat=list(IDENT);mat[3]=tx
 return txt(name)+txt(parent)+pack('12f',*mat)+vec(tx,0,0)+pack('4f',1,0,0,0)+pack('ff',0,1)
skel_reverse=b'SKEL'+u32(6)+u32(1)+txt('test.as')+pack('fI',100,2)+bonechunk('CHILD','ROOT',1)+bonechunk('ROOT','',0)
(OUT/'reversed.skl').write_bytes(skel_reverse)
p=subprocess.run([str(BIN),str(OUT/'blend.bm'),str(OUT/'reversed.skl'),str(OUT/'bend.ba'),'--frame','1','--obj',str(OUT/'reversed.obj')],capture_output=True,text=True)
assert p.returncode==0,p.stdout+' '+p.stderr
coords=[tuple(map(float,v.split()[1:])) for v in (OUT/'reversed.obj').read_text().splitlines() if v.startswith('v ')]
near(coords[1],(1,1,0))
print('PASS reverse-ordered bone hierarchy')
# Cyclic parent topology fails rather than looping indefinitely.
cycle=skel.replace(txt('ROOT')+txt('')+pack('12f',*IDENT), txt('ROOT')+txt('CHILD')+pack('12f',*IDENT),1)
(OUT/'cycle.skl').write_bytes(cycle)
p=subprocess.run([str(BIN),str(OUT/'blend.bm'),str(OUT/'cycle.skl'),str(OUT/'bend.ba')],capture_output=True)
assert p.returncode!=0
print('PASS cyclic rig rejected')
