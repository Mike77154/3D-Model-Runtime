"""Generated synthetic DDS/skin fixtures, no third-party model asset content."""
import struct
from pathlib import Path
P=Path(__file__).parent/'fixtures';P.mkdir(exist_ok=True)
def dds(block,code):
    h=bytearray(128);h[0:4]=b'DDS '
    struct.pack_into('<I',h,4,124);struct.pack_into('<I',h,12,4)
    struct.pack_into('<I',h,16,4);struct.pack_into('<I',h,76,32)
    struct.pack_into('<I',h,80,4);h[84:88]=code
    return bytes(h)+block
red=struct.pack('<HHI',0xf800,0x07e0,0)
variants={'BC1':dds(red,b'DXT1'),
          'BC2':dds(bytes([0xff]*8)+red,b'DXT3'),
          'BC3':dds(bytes([255,0])+bytes(6)+red,b'DXT5')}
raw=bytearray(128);raw[0:4]=b'DDS '
struct.pack_into('<I',raw,4,124);struct.pack_into('<I',raw,12,4);struct.pack_into('<I',raw,16,4)
struct.pack_into('<I',raw,76,32);struct.pack_into('<I',raw,80,0x40)
struct.pack_into('<I',raw,88,32);struct.pack_into('<4I',raw,92,0x00ff0000,0x0000ff00,0x000000ff,0xff000000)
variants['RGBA']=bytes(raw)+bytes([0,0,255,255])*16
# 5 skins: DDS BC1, BC2, BC3, raw 32-bit and a reference to slot 0.
skins=[]
for k,v in variants.items():
    skins.append(struct.pack('<B3xII16s',6,len(v),1,k.encode())+v)
skins.append(struct.pack('<B3xII16s',1,0,0,b'alias BC1'))
tri=struct.pack('<6Hi',0,1,2,0,0,0,0)
verts=b''.join(struct.pack('<3fHBB',x,y,z,65535,0,0) for x,y,z in ((0.,0.,0.),(1.,0.,0.),(0.,1.,0.)))
body=b''.join(skins)+tri+verts
group=struct.pack('<4bI16s5I',1,0,0,0,44+len(body),b'texturetest',5,0,1,3,0)
data=group+body
header=struct.pack('<4s6I10H',b'MDL7',0,0,1,48+len(data),0,0,36,28,16,68,8,16,16,16,68,24)
(P/'mdl7_all_dds_and_reference.mdl').write_bytes(header+data)
# Single external skin: caller must inject path resolution/decoder.
skin=struct.pack('<B3xII16s',7,1,1,b'external')+b'fake_external.tga\0'
body=skin+tri+verts;group=struct.pack('<4bI16s5I',1,0,0,0,44+len(body),b'external',1,0,1,3,0)
data=group+body
(P/'mdl7_external_callback.mdl').write_bytes(struct.pack('<4s6I10H',b'MDL7',0,0,1,48+len(data),0,0,36,28,16,68,8,16,16,16,68,24)+data)
# Invalid DDS signature, truncated payload, invalid alias / loops.
b=bytearray((P/'mdl7_all_dds_and_reference.mdl').read_bytes())
# First 28-byte skin header immediately after 48+44.
b[48+44+28:48+44+32]=b'BAD!'
(P/'mdl7_dds_bad_magic.mdl').write_bytes(b)
(P/'mdl7_dds_truncated.mdl').write_bytes((P/'mdl7_all_dds_and_reference.mdl').read_bytes()[:-5])
# unsupported format must be rejected by texture decoder, not by file header.
print('phase14 textures generated:', ', '.join(variants))
# A DDS blob larger than 65535 bytes exercises the MDL7 uint32 byte-length
# path; the on-disk width field here is NOT the texture's pixel width.
large_header=bytearray(128);large_header[:4]=b'DDS '
for off,val in [(4,124),(12,256),(16,512),(76,32),(80,4)]:struct.pack_into('<I',large_header,off,val)
large_header[84:88]=b'DXT1'
blob=bytes(large_header)+red*(512//4)*(256//4)
skin=struct.pack('<B3xII16s',6,len(blob),1,b'dds_large')+blob
body=skin+tri+verts
group=struct.pack('<4bI16s5I',1,0,0,0,44+len(body),b'large',1,0,1,3,0)
data=group+body
(P/'mdl7_dds_large_blob.mdl').write_bytes(struct.pack('<4s6I10H',b'MDL7',0,0,1,48+len(data),0,0,36,28,16,68,8,16,16,16,68,24)+data)
print('large DDS blob test:',len(blob),'bytes')
