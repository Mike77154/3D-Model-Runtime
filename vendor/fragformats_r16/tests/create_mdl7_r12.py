#!/usr/bin/env python3
"""R12 synthetic two-joint hierarchy test. Not a native GameStudio animation."""
import pathlib,subprocess,struct,sys
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,'tests/create_mdl7_r11.py',str(out)],check=True)
source=bytearray((out/'MDL7_pivot_3x4.mdl').read_bytes())
u32=lambda data,p:struct.unpack_from('<I',data,p)[0]
p32=lambda data,p,v:struct.pack_into('<I',data,p,v)
p16=lambda data,p,v:struct.pack_into('<H',data,p,v)
start=48+u32(source,8)*36
f0=start+44+4*8+2*16+4*26
f1=f0+24+26+68
arm_mat=f1+24+26
assert source[f1:f1+5]==b'pose1'
assert u32(source,f1+20)==1
# Add a root 90-deg Z rotation in the same frame; root is at absolute (0,0,0).
# Values use the exact column-storage layout in the R11 fixture.
rot=[0.,-1.,0.,0., 1.,0.,0.,0., 0.,0.,1.,0., 0.,0.,0.,1.]
root_mat=struct.pack('<16fH2x',*rot,0)
assert len(root_mat)==68
end_mat=arm_mat+68
source[end_mat:end_mat]=root_mat
p32(source,f1+20,2)
p32(source,start+4,u32(source,start+4)+68)
p32(source,16,u32(source,16)+68)
(out/'MDL7_twojoint_hierarchy.mdl').write_bytes(source)
# Identity arm + rotating root: hierarchy should propagate root motion to child.
rootonly=bytearray(source)
ident=[1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.]
struct.pack_into('<16fH2x',rootonly,arm_mat,*ident,1)
(out/'MDL7_parent_only.mdl').write_bytes(rootonly)
# Swap the two bones, resulting in parent index 1 for child index 0.
forward=bytearray(source)
root=bytes(forward[48:84]);arm=bytes(forward[84:120])
forward[48:84]=arm;forward[84:120]=root
p16(forward,48,1);p16(forward,84,65535)
# frame0 original arm transform, frame1 original arm transform, inserted root transform.
p16(forward,f0+24+26+64,0)
p16(forward,arm_mat+64,0)
p16(forward,end_mat+64,1)
# Deformer element order: root first, arm second; swap their bone indices.
df=end_mat+68
assert forward[df:df+2]==b'\0\0'
p32(forward,df+16,1)    # first element = root (old index 0)
p32(forward,df+16+28+4*8,0)  # next element = arm (old index 1)
(out/'MDL7_forward_parent.mdl').write_bytes(forward)
# Invalid double matrix (same bone appears twice in a frame).
bad=bytearray(source);p16(bad,end_mat+64,1)
(out/'MDL7_duplicate_transform.mdl').write_bytes(bad)
# Invalid parent cycle.
bad=bytearray(source);p16(bad,48,1)
(out/'MDL7_hierarchy_cycle.mdl').write_bytes(bad)
# Parent matrix nonfinite.
bad=bytearray(source);p32(bad,end_mat,0x7fc00000)
(out/'MDL7_hierarchy_nan.mdl').write_bytes(bad)
print('R12 synthetic 2 joint hierarchy cases generated')
