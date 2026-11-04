#!/usr/bin/env python3
"""R10 opt-in raw frame matrix skinning fixtures. Synthetic, never actual games."""
import pathlib, subprocess, sys, struct
p=pathlib.Path(sys.argv[1]);p.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,'tests/create_mdl7_r9.py',str(p)],check=True)
b=(p/'MDL7_matrix_valid.mdl').read_bytes()
r=lambda x,i:struct.unpack_from('<I',x,i)[0]
start=48+r(b,8)*36
f0=start+44+4*8+2*16+4*26
f1=f0+24+26+68
m1=f1+24+26
assert b[f1:f1+5]==b'pose1'
# Matrix row-vector affine requirement (3,7,11 must be zero, 15=one).
bad=bytearray(b)
struct.pack_into('<f',bad,m1+3*4,0.125)
(p/'MDL7_preview_perspective_bad.mdl').write_bytes(bad)
# A deformer with vertex 0 total weight .75; accepted as unnormalized metadata
# but explicitly rejected by R10's rigid preview.
bad=bytearray(b)
# The first weight pair appears after group frames, deformer[16], element[28].
end=f1+24+26+68
assert b[end:end+2]==b'\0\0'
firstweight=end+16+28+4
assert struct.unpack_from('<f',b,firstweight)[0]==.75
struct.pack_into('<f',bad,firstweight,.50)
(p/'MDL7_preview_weight_sum_bad.mdl').write_bytes(bad)
print('R10 fixtures: valid weighted pose, invalid perspective matrix, invalid sum')
