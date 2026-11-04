import struct
from pathlib import Path
p=Path(__file__).parent
orig=(p/'synthetic_plane.dbo').read_bytes()
pat=struct.pack('<III',117,4,4)
i=orig.find(pat)
assert i>=0
(p/'nontri_primitive.dbo').write_bytes(orig[:i+8]+struct.pack('<I',5)+orig[i+12:])
pat=struct.pack('<III',114,4,6)
i=orig.find(pat)
assert i>=0
(p/'bad_primitive_index_count.dbo').write_bytes(orig[:i+8]+struct.pack('<I',5)+orig[i+12:])
