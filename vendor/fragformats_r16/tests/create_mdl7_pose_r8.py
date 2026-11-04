#!/usr/bin/env python3
"""Extend the R7 synthetic rig with two nontrivial frame-vertex positions.
NOT original GameStudio assets. Every offset is validated against R7 fixture.
"""
import pathlib,struct,sys,subprocess
root=pathlib.Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,'tests/create_mdl7_rig_r7.py',str(root)],check=True)
data=bytearray((root/'MDL7_weighted_valid.mdl').read_bytes())
p=struct.pack
read=lambda b,o:struct.unpack_from('<I',b,o)[0]
assert data[:4]==b'MDL7'
bone_count=read(data,8)
group_start=48+bone_count*36
assert data[group_start]==1
assert read(data,group_start+40)==1
frame_start=group_start+44+4*8+2*16+4*26
assert data[frame_start:frame_start+5]==b'pose0'
# First frame: vertex 2 -> X=2.0, Y=3.0, Z=4.0 (absolute).
struct.pack_into('<3f',data,frame_start+24,2.,3.,4.)
frame_len=24+26+68
assert data[frame_start+frame_len] == 0  # MD7 deformer version
# Second frame: vertex 2 -> X=7.0; different animation pose.
newframe=(p('<16sII',b'pose1',1,1)
        +p('<3fH3f',7.,8.,9.,2,0.,0.,1.)
        +p('<16fH2x',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1))
assert len(newframe)==frame_len
at=frame_start+frame_len
data[at:at]=newframe
struct.pack_into('<I',data,group_start+40,2)
struct.pack_into('<I',data,group_start+4,read(data,group_start+4)+frame_len)
struct.pack_into('<I',data,16,read(data,16)+frame_len)
(root/'MDL7_two_poses.mdl').write_bytes(data)
bad=bytearray(data)
# Wrong index for the second frame's single override.
struct.pack_into('<H',bad,at+24+12,999)
(root/'MDL7_two_poses_bad_index.mdl').write_bytes(bad)
(root/'MDL7_two_poses_truncated.mdl').write_bytes(data[:-3])
