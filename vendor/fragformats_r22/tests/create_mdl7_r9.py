#!/usr/bin/env python3
"""R9 independent synthetic (not genuine GameStudio) MDL7 rig/matrix fixtures."""
import pathlib, subprocess, sys, struct
p=pathlib.Path(sys.argv[1]);p.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,'tests/create_mdl7_pose_r8.py',str(p)],check=True)
read=lambda a,o:struct.unpack_from('<I',a,o)[0]
src=(p/'MDL7_two_poses.mdl').read_bytes()
b=bytearray(src)
assert b[:4]==b'MDL7' and read(b,8)==2
start=48+read(b,8)*36
assert read(b,start+40)==2
f0=start+44+4*8+2*16+4*26
L=24+26+68
assert bytes(b[f0:f0+5])==b'pose0'
assert bytes(b[f0+L:f0+L+5])==b'pose1'
f1=f0+L
# Make second pose matrix visibly different from first, while valid.
m1=f1+24+26
struct.pack_into('<f',b,m1+0,1.5)
struct.pack_into('<f',b,m1+12*4,3.25)
struct.pack_into('<f',b,m1+13*4,-1.25)
(p/'MDL7_matrix_valid.mdl').write_bytes(b)
# Correct 28-byte frame headers (R8 incorrectly always assumed 24 bytes).
ext=bytearray(b)
# Insert from last to first, so frame0's offset stays unchanged.
ext[f1+24:f1+24]=b'R9!!'
ext[f0+24:f0+24]=b'R9!!'
struct.pack_into('<H',ext,28+9*2,28)
struct.pack_into('<I',ext,start+4,read(ext,start+4)+8)
struct.pack_into('<I',ext,16,read(ext,16)+8)
(p/'MDL7_frameheader28_valid.mdl').write_bytes(ext)
# Negative fixtures
bad=bytearray(b);struct.pack_into('<H',bad,48,1)
(p/'MDL7_bone_cycle.mdl').write_bytes(bad)
bad=bytearray(b);struct.pack_into('<I',bad,48+4,0x7fc00000)
(p/'MDL7_bone_nan.mdl').write_bytes(bad)
bad=bytearray(b);struct.pack_into('<I',bad,m1,0x7fc00000)
(p/'MDL7_matrix_nan.mdl').write_bytes(bad)
bad=bytearray(b);struct.pack_into('<H',bad,28+9*2,23)
(p/'MDL7_frameheader_bad_stride.mdl').write_bytes(bad)
bad=bytearray(b);struct.pack_into('<I',bad,f0+20,2)
(p/'MDL7_matrix_count_bad.mdl').write_bytes(bad)
bad=bytearray(b);struct.pack_into('<H',bad,m1+64,99)
(p/'MDL7_matrix_bone_bad.mdl').write_bytes(bad)
(p/'MDL7_matrix_truncated.mdl').write_bytes(bytes(b)[:-19])
print('R9 fixtures generated: 2 valid, 7 invalid')
