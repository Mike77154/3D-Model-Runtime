"""Only generates invented XIR1 intermediate test fixtures, NEVER genuine Blade3D XMDL."""
import struct,pathlib
P=pathlib.Path(__file__).resolve().parent
u32=lambda n: struct.pack('<I',n)
f32=lambda *a: struct.pack('<'+len(a)*'f',*a)
attrs=[0,1,4] # xyz, normal xyz, uv0 xy (UV label provisional from RE)
vertices=[(0.,0.,0.,0.,0.,1.,0.,0.),(1.,0.,0.,0.,0.,1.,1.,0.),(1.,1.,0.,0.,0.,1.,1.,1.),(0.,1.,0.,0.,0.,1.,0.,1.)]
tris=[(0,1,2),(0,2,3)]
blobs=[u32(len(attrs))+b''.join(u32(v) for v in attrs),u32(len(vertices))+b''.join(f32(*v) for v in vertices),u32(len(tris))+b''.join(struct.pack('<III',*tri) for tri in tris)]
offset=0;recs=[]
for typ,blob in zip([1,3,4],blobs):
 recs.append(struct.pack('<IIII',typ,0,offset,len(blob)));offset+=len(blob)
fixture=b'XIR1'+u32(len(recs))+u32(offset)+b''.join(recs)+b''.join(blobs)
(P/'synthetic_mesh.xir').write_bytes(fixture)
# Indices out of vertex range must fail; no accidental facial geometry silently omitted.
bad=bytearray(fixture)
bad[-12:-8]=u32(9000)
(P/'bad_index_mesh.xir').write_bytes(bad)
# Truncated payload and overlarge count must also fail.
(P/'truncated_mesh.xir').write_bytes(fixture[:-5])
bad2=bytearray(fixture);bad2[4:8]=u32(0xffffffff)
(P/'bad_count_mesh.xir').write_bytes(bad2)
