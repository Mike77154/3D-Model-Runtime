"""Creates strictly synthetic DBO-like test data for C89 structural checks; not an authentic exported .dbo."""
import struct,pathlib
u32=lambda n:struct.pack('<I',n)
block=lambda typ,payload:u32(typ)+u32(len(payload))+payload
f=lambda *xs:struct.pack('<'+len(xs)*'f',*xs)
vertices=[(0.,0.,0., 0.,0.,1., 0.,0.),(1.,0.,0., 0.,0.,1., 1.,0.),(1.,1.,0., 0.,0.,1., 1.,1.),(0.,1.,0., 0.,0.,1., 0.,1.)]
mesh=b''.join([
 block(111,u32(274)),block(112,u32(32)),block(113,u32(4)),block(114,u32(6)),
 block(115,b''.join(f(*v) for v in vertices)),block(116,struct.pack('<6H',0,1,2,0,2,3)),
 block(117,u32(4)),block(127,u32(0)),u32(0)+u32(0)
])
root=b''.join([block(101,u32(9)+b'testplane'),block(102,f(*[1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.])),block(103,mesh),u32(0)+u32(0)])
header=u32(8)+b'MAGICDBO'+u32(1062)+u32(0)+u32(0)
path=pathlib.Path(__file__).parent/'synthetic_plane.dbo'
path.write_bytes(header+block(1,root))
print('Generated',path, len(header+block(1,root)))
