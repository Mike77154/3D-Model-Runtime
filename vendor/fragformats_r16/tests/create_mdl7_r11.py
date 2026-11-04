#!/usr/bin/env python3
"""Synthetic MDL7 3x4 matrix / rest pivot test; not a genuine model."""
import pathlib, subprocess, sys, struct
p=pathlib.Path(sys.argv[1]);p.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,'tests/create_mdl7_r10.py',str(p)],check=True)
b=bytearray((p/'MDL7_matrix_valid.mdl').read_bytes())
u32=lambda pos:struct.unpack_from('<I',b,pos)[0]
start=48+u32(8)*36
f0=start+44+4*8+2*16+4*26
f1=f0+24+26+68
m1=f1+24+26
assert b[f1:f1+5]==b'pose1'
# For joint index 1, convert MDL7 3x4 column-order matrix to row-vector
# rotation about Z: (x,y)->(-y,x). Rest pivot of arm is (0,1,0).
# Fixture vertex0=(0,0,0); weights root .75 and arm .25 -> (.25,.25,0).
col3x4=[0.,-1.,0.,0., 1.,0.,0.,0., 0.,0.,1.,0.]
for k,val in enumerate(col3x4):struct.pack_into('<f',b,m1+k*4,val)
(p/'MDL7_pivot_3x4.mdl').write_bytes(b)
# A missing / nonfinite 3x4 component must be rejected.
invalid=bytearray(b)
struct.pack_into('<I',invalid,m1+4*4,0x7fc00000)
(p/'MDL7_pivot_nan.mdl').write_bytes(invalid)
# A parent index cycle must be rejected before sampling.
invalid=bytearray(b)
struct.pack_into('<H',invalid,48,1)
(p/'MDL7_pivot_cycle.mdl').write_bytes(invalid)
# A weight sum != 1 cannot be silently normalized in the pivot preview.
invalid=bytearray(b)
df=f1+24+26+68
assert struct.unpack_from('<f',invalid,df+16+28+4)[0]==.75
struct.pack_into('<f',invalid,df+16+28+4,.5)
(p/'MDL7_pivot_weights.mdl').write_bytes(invalid)
# Arm position affects only the experimental pivot, not the raw preview.
shift=bytearray(b)
struct.pack_into('<f',shift,48+36+8,2.)  # arm bind Y becomes 2
(p/'MDL7_pivot_shifted_bind.mdl').write_bytes(shift)
print('R11 fixtures: column-major 3x4, pivot shift, NaN, cycles and weights')
