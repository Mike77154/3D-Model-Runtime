"""Phase 12 synthetic regressions, no redistribution of the user model corpus."""
import struct
from pathlib import Path
P=Path(__file__).parent/'fixtures'
# Move a valid existing deformer table outside groupdata_size, like Tusker.
b=bytearray((P/'mdl7_weighted_valid.mdl').read_bytes())
header=48+2*36
original=struct.unpack_from('<I',b,header+4)[0]
struct.pack_into('<I',b,header+4,original-88)
(P/'mdl7_postgroup_weighted.mdl').write_bytes(b)
# Add 3 UV points and a valid 26-byte triangle with an unused UV2 slot.
b=bytearray((P/'mdl7_morph_fraction.mdl').read_bytes())
# First group: no bones, no skins; triangle starts at 48+44.
tri_offset=92
newuv=b''.join(struct.pack('<2f',u,v) for u,v in [(0.,0.),(1.,0.),(0.,1.)])
tri=bytes(b[tri_offset:tri_offset+16])
tri26=tri+struct.pack('<3HI',65535,65535,65535,0xffffffff)
new=bytearray(b[:tri_offset]+newuv+tri26+b[tri_offset+16:])
struct.pack_into('<I',new,16,len(new))
struct.pack_into('<H',new,28+5*2,26) # triangle stride
struct.pack_into('<I',new,48+4,struct.unpack_from('<I',new,48+4)[0]+34)
struct.pack_into('<I',new,48+28,3) # uv count
(P/'mdl7_unused_second_uv.mdl').write_bytes(new)
bad=bytearray(new)
# Change unused material 0xffffffff to 0 while keeping out-of-range UV2.
second_mat=tri_offset+len(newuv)+22
struct.pack_into('<I',bad,second_mat,0)
(P/'mdl7_bad_second_uv.mdl').write_bytes(bad)
print('PHASE12 synthetic fixtures generated')
