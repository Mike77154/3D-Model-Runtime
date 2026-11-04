"""Synthetic SDNA fixtures constructed from Blender 2.28a and 3.6.3 source-layout observations.
NOT saved by Blender, NOT authentic Blender-version compatibility tests.
"""
from pathlib import Path
import tempfile,subprocess,struct
ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'blendc89'
pack16=lambda v:struct.pack('<H',v)
pack32=lambda v:struct.pack('<I',v)

def sdna(names,types,typesizes,structures):
    data=b'SDNANAME'+pack32(len(names))+b''.join(s.encode()+b'\x00' for s in names)
    data+=b'\0'*(-len(data)%4)
    data+=b'TYPE'+pack32(len(types))+b''.join(s.encode()+b'\0' for s in types)
    data+=b'\0'*(-len(data)%4)
    data+=b'TLEN'+b''.join(pack16(v) for v in typesizes)
    data+=b'\0'*(-len(data)%4)
    data+=b'STRC'+pack32(len(structures))
    for tid,fields in structures:
        data+=pack16(tid)+pack16(len(fields))
        for ftype,fname in fields:data+=pack16(ftype)+pack16(fname)
    return data

def blend(version,blocks):
    def block(code,data,old,sdna_id,count):
        return code+pack32(len(data))+pack32(old)+pack32(sdna_id)+pack32(count)+data
    return (b'BLENDER_v'+f'{version:03}'.encode()+
            b''.join(block(*row) for row in blocks)+
            block(b'ENDB',b'',0,0,0))

VERTS = b''.join(pack32(v) for v in [0x3f800000,0,0, 0,0x3f800000,0, 0,0,0x3f800000])

def historical_228():
    # 2.28a: MFace.v[1..4] = unsigned short, 2 bytes each.
    names=['*mvert','*mface','totvert','totface','co[3]',
           'v1','v2','v3','v4','pad[4]']
    types=['int','float','ushort','char','Mesh','MVert','MFace']
    lens=[4,4,2,1,16,12,12]
    structures=[(4,[(5,0),(6,1),(0,2),(0,3)]),
                (5,[(1,4)]),
                (6,[(2,5),(2,6),(2,7),(2,8),(3,9)])]
    mesh=pack32(0x100)+pack32(0x200)+pack32(3)+pack32(1)
    faces=b''.join(pack16(v) for v in [0,1,2,0])+b'\0'*4
    d=sdna(names,types,lens,structures)
    return blend(228,[(b'ME\0\0',mesh,0x300,0,1),(b'DATA',VERTS,0x100,1,3),
                      (b'DATA',faces,0x200,2,1),(b'DNA1',d,0x400,0,1)])

def historical_228_uv():
    # 2.28a TFace.uv has four float2 corners per polygon and is NOT MLoopUV.
    names=['*mvert','*mface','*tface','totvert','totface','co[3]',
           'v1','v2','v3','v4','pad[4]','*tpage','uv[4][2]']
    types=['int','float','ushort','char','Mesh','MVert','MFace','TFace']
    lens=[4,4,2,1,20,12,12,36]
    specs=[(4,[(5,0),(6,1),(7,2),(0,3),(0,4)]),
           (5,[(1,5)]),
           (6,[(2,6),(2,7),(2,8),(2,9),(3,10)]),
           (7,[(3,11),(1,12)])]
    mesh=pack32(0x100)+pack32(0x200)+pack32(0x500)+pack32(3)+pack32(1)
    face=b''.join(pack16(v) for v in [0,1,2,0])+b'\0'*4
    uvs=b''.join(pack32(v) for v in [0,0,0x3f800000,0,0,0x3f800000,0x3f800000,0x3f800000])
    tface=pack32(0)+uvs
    d=sdna(names,types,lens,specs)
    return blend(228,[(b'ME\0\0',mesh,0x300,0,1),
                      (b'DATA',VERTS,0x100,1,3),
                      (b'DATA',face,0x200,2,1),
                      (b'DATA',tface,0x500,3,1),
                      (b'DNA1',d,0x700,0,1)])

def transitional_363():
    # Blender 3.6.x: totpoly, totloop, poly_offset_indices, generic .corner_vert.
    names=['totvert','totpoly','totloop','*poly_offset_indices',
           '*mvert','ldata','co[3]','*layers','totlayer','type','name[16]','*data']
    types=['int','float','char','Mesh','MVert','CustomData','CustomDataLayer']
    lens=[4,4,1,28,12,8,24]
    structures=[(3,[(0,0),(0,1),(0,2),(0,3),(4,4),(5,5)]),
                (4,[(1,6)]),
                (5,[(6,7),(0,8)]),
                (6,[(0,9),(2,10),(0,11)])]
    mesh=pack32(3)+pack32(1)+pack32(3)+pack32(0x600)+pack32(0x100)+pack32(0x500)+pack32(1)
    # CD_PROP_INT32 = 11; name identifies corner vertex indices.
    layer=pack32(11)+b'.corner_vert\0'.ljust(16,b'\0')+pack32(0x400)
    d=sdna(names,types,lens,structures)
    return blend(306,[(b'ME\0\0',mesh,0x300,0,1),(b'DATA',VERTS,0x100,1,3),
                      (b'DATA',pack32(0)+pack32(1)+pack32(2),0x400,0,3),
                      (b'DATA',layer,0x500,3,1),
                      (b'DATA',pack32(0)+pack32(3),0x600,0,2),
                      (b'DNA1',d,0x700,0,1)])

def run(data,label):
    with tempfile.TemporaryDirectory() as tmp:
        fi=Path(tmp)/'model.blend';out=Path(tmp)/'model.obj'
        fi.write_bytes(data)
        cp=subprocess.run([str(BIN),'--obj',str(fi),str(out)],text=True,capture_output=True)
        assert cp.returncode==0,(label,cp.stderr,cp.stdout)
        result=out.read_text()
        assert 'f 1 2 3\n' in result,(label,result)
        assert result.count('v ') == 3,(label,result)
        print('PASS',label)

if __name__=='__main__':
    run(historical_228(),'source-informed MFace 16-bit indexes (2.28a)')
    run(transitional_363(),'source-informed poly_offset_indices (3.6.3)')
    with tempfile.TemporaryDirectory() as tmp:
        fi=Path(tmp)/'old_uv.blend';obj=Path(tmp)/'old_uv.obj'
        fi.write_bytes(historical_228_uv())
        result=subprocess.run([str(BIN),'--obj',str(fi),str(obj)],text=True,capture_output=True)
        assert result.returncode==0,result.stderr
        content=obj.read_text()
        assert content.count('vt ') == 4,content
        assert 'f 1/1 2/2 3/3\n' in content,content
        print('PASS historical TFace.uv corner coordinates (2.28a)')
