"""Synthetic binary fixtures. No Blender-generated corpus is claimed."""
import pathlib
import struct
import subprocess
import tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'

def fixture(generation, ptr, le, version):
    order='<' if le else '>'
    u16=lambda x:struct.pack(order+'H',x)
    u32=lambda x:struct.pack(order+'I',x)
    u64=lambda x:struct.pack(order+'Q',x)
    ptrbytes=lambda x:u32(x) if ptr==4 else u64(x)
    names=['*mvert','*mface','totvert','totface','co[3]','v1','v2','v3','v4']
    types=['int','float','Mesh','MVert','MFace']
    sizes=[4,4,ptr*2+8,12,16]
    structs=[(2,[(3,0),(4,1),(0,2),(0,3)]),
             (3,[(1,4)]),
             (4,[(0,5),(0,6),(0,7),(0,8)])]
    dna=b'SDNANAME'+u32(len(names))+b''.join(x.encode()+b'\x00' for x in names)
    dna+=b'\x00'*((-len(dna))%4)
    dna+=b'TYPE'+u32(len(types))+b''.join(x.encode()+b'\x00' for x in types)
    dna+=b'\x00'*((-len(dna))%4)
    dna+=b'TLEN'+b''.join(u16(n) for n in sizes)
    dna+=b'\x00'*((-len(dna))%4)
    dna+=b'STRC'+u32(len(structs))
    for t,fields in structs:
        dna+=u16(t)+u16(len(fields))
        for typ,name in fields:dna+=u16(typ)+u16(name)
    vertices=b''.join(u32(x) for x in [0x3f800000,0,0, 0,0x3f800000,0, 0,0,0x3f800000])
    mesh=ptrbytes(0x100)+ptrbytes(0x200)+u32(3)+u32(1)
    face=u32(0)+u32(1)+u32(2)+u32(0)
    if generation==0:
        head=b'BLENDER'+(b'_' if ptr==4 else b'-')+(b'v' if le else b'V')+f'{version:03d}'.encode()
        def block(code,data,old,sdna,count):
            return code+u32(len(data))+ptrbytes(old)+u32(sdna)+u32(count)+data
    else:
        head=b'BLENDER17-01v'+f'{version:04d}'.encode()
        def block(code,data,old,sdna,count):
            return code+u32(sdna)+u64(old)+u64(len(data))+u64(count)+data
    return (head+block(b'ME\0\0',mesh,0x300,0,1)+
            block(b'DATA',vertices,0x100,1,3)+
            block(b'DATA',face,0x200,2,1)+
            block(b'DNA1',dna,0x400,0,1)+
            block(b'ENDB',b'',0,0,0))

def run_case(gen,ptr,le,version):
    with tempfile.TemporaryDirectory() as td:
        inp=pathlib.Path(td)/'x.blend'
        out=pathlib.Path(td)/'x.obj'
        inp.write_bytes(fixture(gen,ptr,le,version))
        a=subprocess.run([str(EXE),'--info',str(inp)],capture_output=True,text=True)
        assert a.returncode==0,(a.stdout,a.stderr)
        assert f'BLEND version: {version}' in a.stdout
        assert 'Mesh schema: recognized' in a.stdout,a.stdout
        b=subprocess.run([str(EXE),'--obj',str(inp),str(out)],capture_output=True,text=True)
        assert b.returncode==0,(b.stdout,b.stderr)
        data=out.read_text()
        assert 'v 1.0000 0.0000 0.0000' in data,data
        assert 'v 0.0000 1.0000 0.0000' in data,data
        assert 'f 1 2 3' in data,data
        print('PASS:',gen,ptr,'little' if le else 'big',version)

for opts in [(0,4,True,249),(0,4,False,279),(0,8,True,280),
             (0,8,False,305),(0,8,True,404),(1,8,True,405),
             (1,8,True,500),(1,8,True,502)]:
    run_case(*opts)
print('Eight artificial fixtures passed; NOT real-version interoperability tests.')

def fixture_modern():
    u16=lambda x:struct.pack('<H',x)
    u32=lambda x:struct.pack('<I',x)
    u64=lambda x:struct.pack('<Q',x)
    names=['verts_num','faces_num','corners_num','*face_offset_indices',
           'attribute_storage','*dna_attributes','dna_attributes_num',
           '_pad[4]','*name','*data']
    types=['int','char','Mesh','AttributeStorage','Attribute','AttributeArray']
    sizes=[4,1,40,16,16,8]
    specs=[(2,[(0,0),(0,1),(0,2),(0,3),(3,4)]),
           (3,[(0,5),(0,6),(1,7)]),
           (4,[(1,8),(1,9)]),
           (5,[(1,9)])]
    dna=b'SDNANAME'+u32(len(names))+b''.join(v.encode()+b'\0' for v in names)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'TYPE'+u32(len(types))+b''.join(v.encode()+b'\0' for v in types)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'TLEN'+b''.join(u16(x) for x in sizes)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'STRC'+u32(len(specs))
    for t,fields in specs:
        dna+=u16(t)+u16(len(fields))
        for a,b in fields:dna+=u16(a)+u16(b)
    header=b'BLENDER17-01v0500'
    def block(code,data,old,sdna,count):
        return code+u32(sdna)+u64(old)+u64(len(data))+u64(count)+data
    mesh=u32(3)+u32(1)+u32(3)+u32(0)+u64(0x900)+u64(0x700)+u32(2)+u32(0)
    attrs=u64(0x300)+u64(0x500)+u64(0x400)+u64(0x600)
    positions=b''.join(u32(x) for x in [0x3f800000,0,0, 0,0x3f800000,0, 0,0,0x3f800000])
    corners=u32(0)+u32(1)+u32(2)
    blocks=[(b'ME\0\0',mesh,0x800,0,1),
            (b'DATA',attrs,0x700,2,2),
            (b'DATA',b'position\0',0x300,0,1),
            (b'DATA',b'.corner_vert\0',0x400,0,1),
            (b'DATA',u64(0x100),0x500,3,1),
            (b'DATA',u64(0x200),0x600,3,1),
            (b'DATA',positions,0x100,0,9),
            (b'DATA',corners,0x200,0,3),
            (b'DATA',u32(0)+u32(3),0x900,0,2),
            (b'DNA1',dna,0xa00,0,1),
            (b'ENDB',b'',0,0,0)]
    return header+b''.join(block(*x) for x in blocks)

with tempfile.TemporaryDirectory() as td:
    inp=pathlib.Path(td)/'modern.blend';out=pathlib.Path(td)/'modern.obj'
    inp.write_bytes(fixture_modern())
    info=subprocess.run([str(EXE),'--info',str(inp)],text=True,capture_output=True)
    assert info.returncode==0,(info.stdout,info.stderr)
    assert 'Mesh schema: recognized' in info.stdout,info.stdout
    result=subprocess.run([str(EXE),'--obj',str(inp),str(out)],text=True,capture_output=True)
    assert result.returncode==0,(result.stdout,result.stderr)
    assert 'f 1 2 3' in out.read_text(),out.read_text()
    print('PASS: synthetic Blender 5.x AttributeStorage positions and corner topology')
with tempfile.TemporaryDirectory() as td:
    src=pathlib.Path(td)/'valid.blend';src.write_bytes(fixture(0,4,True,279))
    schema=subprocess.run([str(EXE),'--schema',str(src),'MVert'],text=True,capture_output=True)
    assert schema.returncode==0 and 'co[3]' in schema.stdout,schema.stderr
    blocks=subprocess.run([str(EXE),'--blocks',str(src)],text=True,capture_output=True)
    assert blocks.returncode==0 and 'DNA1' in blocks.stdout,blocks.stderr
    for magic in [b'bad filexxxxxxxx',b'\x1f\x8b' + b'\0'*30,b'\x28\xb5\x2f\xfd' + b'\0'*30]:
        bad=pathlib.Path(td)/'bad.blend';bad.write_bytes(magic)
        result=subprocess.run([str(EXE),'--info',str(bad)],capture_output=True)
        assert result.returncode!=0,result.stdout
    broken=pathlib.Path(td)/'broken.blend'
    broken.write_bytes(src.read_bytes()[:-5])
    result=subprocess.run([str(EXE),'--info',str(broken)],capture_output=True)
    assert result.returncode!=0
    print('PASS: schema/blocks CLI and four malformed or wrapped inputs rejected')

# Phase 02: independent real-code paths with synthetic serialized blocks.
with tempfile.TemporaryDirectory() as td:
    source=fixture(0,4,True,279)
    head=12; msize=20+16
    extra=bytearray(source[head:head+msize]); extra[8:12]=struct.pack('<I',0x777)
    multi=source[:head+msize]+extra+source[head+msize:]
    inp=pathlib.Path(td)/'multi.blend'; obj=pathlib.Path(td)/'multi.obj'
    inp.write_bytes(multi)
    ret=subprocess.run([str(EXE),'--obj',str(inp),str(obj)],capture_output=True,text=True)
    assert ret.returncode==0,ret.stderr
    output=obj.read_text()
    assert output.count('\nv ') == 6,output
    assert 'f 1 2 3' in output and 'f 4 5 6' in output,output
    report=subprocess.run([str(EXE),'--meshes',str(inp)],capture_output=True,text=True)
    assert report.returncode==0 and 'mesh[1]' in report.stdout,report.stderr
    print('PASS: two independent meshes and OBJ global vertex index offsets')
    single=bytearray(source)
    single[32:36]=struct.pack('<I',0x104)  # interior mvert reference
    vb=12+msize
    assert single[vb:vb+4]==b'DATA'
    single[vb+4:vb+8]=struct.pack('<I',40)  # prefixed vertex block
    interior=single[:vb+20]+b'JUNK'+single[vb+20:]
    inp.write_bytes(interior)
    ret=subprocess.run([str(EXE),'--obj',str(inp),str(obj)],capture_output=True,text=True)
    assert ret.returncode==0,ret.stderr
    assert 'f 1 2 3' in obj.read_text()
    print('PASS: interior serialized reference, with 4-byte pointer displacement')
    invalid=bytearray(source)
    # locate the MFace data-block after 3 MVert entries
    fpos=source.index(b'DATA',vb+1)+20
    invalid[fpos+4:fpos+8]=struct.pack('<I',999)
    inp.write_bytes(invalid)
    ret=subprocess.run([str(EXE),'--obj',str(inp),str(obj)],capture_output=True,text=True)
    assert ret.returncode!=0
    assert not obj.exists(), 'partial output file should have been removed'
    print('PASS: invalid face indices rejected and partial OBJ cleaned up')

def fixture_uv_poly(uv_type=16):
    u16=lambda x:struct.pack('<H',x)
    u32=lambda x:struct.pack('<I',x)
    names=['*mvert','*mpoly','*mloop','totvert','totpoly','totloop','ldata',
           'co[3]','loopstart','v','*layers','totlayer','type','*data',
           'name[8]','uv[2]','flag']
    types=['int','float','char','Mesh','MVert','MPoly','MLoop',
           'CustomData','CustomDataLayer','MLoopUV']
    specs=[(3,[(4,0),(5,1),(6,2),(0,3),(0,4),(0,5),(7,6)]),
           (4,[(1,7)]),(5,[(0,8),(0,5)]),(6,[(0,9)]),
           (7,[(8,10),(0,11)]),(8,[(0,12),(0,13),(2,14)]),
           (9,[(1,15),(0,16)])]
    sizes=[4,4,1,32,12,8,4,8,16,12]
    dna=b'SDNANAME'+u32(len(names))+b''.join(x.encode()+b'\0' for x in names)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'TYPE'+u32(len(types))+b''.join(x.encode()+b'\0' for x in types)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'TLEN'+b''.join(u16(x) for x in sizes)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'STRC'+u32(len(specs))
    for t, fields in specs:
        dna+=u16(t)+u16(len(fields))
        for ty,nm in fields:dna+=u16(ty)+u16(nm)
    def block(code,body,old,sdna,count=1):
        return code+u32(len(body))+u32(old)+u32(sdna)+u32(count)+body
    points=b''.join(u32(x) for x in [0,0,0,0x3f800000,0,0,0,0x3f800000,0])
    mesh=b''.join(u32(x) for x in [0x100,0x200,0x300,3,1,3,0x400,1])
    uv_values=([0,0,0,0x3f800000,0,0,0,0x3f800000,0] if uv_type==16 else [0,0,0x3f800000,0,0,0x3f800000])
    uv=b''.join(u32(x) for x in uv_values)
    layer=u32(uv_type)+u32(0x600)+b'UVMap\0\0\0'
    blocks=[(b'ME\0\0',mesh,0x700,0,1),
            (b'DATA',points,0x100,1,3),
            (b'DATA',u32(0)+u32(3),0x200,2,1),
            (b'DATA',u32(0)+u32(1)+u32(2),0x300,3,3),
            (b'DATA',layer,0x400,5,1),
            (b'DATA',uv,0x600,6,3),
            (b'DNA1',dna,0x800,0,1),
            (b'ENDB',b'',0,0,0)]
    return b'BLENDER_v279'+b''.join(block(*x) for x in blocks)

with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td)/'uv.blend';out=pathlib.Path(td)/'uv.obj'
    p.write_bytes(fixture_uv_poly())
    r=subprocess.run([str(EXE),'--info',str(p)],capture_output=True,text=True)
    assert r.returncode==0 and 'Mesh schema: recognized' in r.stdout,(r.stdout,r.stderr)
    r=subprocess.run([str(EXE),'--obj',str(p),str(out)],capture_output=True,text=True)
    assert r.returncode==0,(r.stdout,r.stderr)
    content=out.read_text()
    assert 'vt 1.0000 0.0000' in content,content
    assert 'f 1/1 2/2 3/3' in content,content
    print('PASS: MPoly/MLoop topology plus MLoopUV UVMap in OBJ')

with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td)/'uv_float2.blend';out=pathlib.Path(td)/'uv_float2.obj'
    p.write_bytes(fixture_uv_poly(49))
    r=subprocess.run([str(EXE),'--obj',str(p),str(out)],capture_output=True,text=True)
    assert r.returncode==0,(r.stdout,r.stderr)
    assert 'f 1/1 2/2 3/3' in out.read_text(),out.read_text()
    print('PASS: modern CD_PROP_FLOAT2 UV layer serialized in CustomData')

def fixture_modern_layout(single=False, oversized=False):
    # Realistic field offsets in the *minimal synthetic types*, NOT a real Blender Mesh.
    U16=lambda v:struct.pack('<H',v)
    U32=lambda v:struct.pack('<I',v)
    U64=lambda v:struct.pack('<Q',v)
    names=['verts_num','faces_num','corners_num','_align[4]','*face_offset_indices',
           'attribute_storage','*dna_attributes','dna_attributes_num','_pad[4]',
           '*runtime','*name','data_type','domain','storage_type','*data',
           '*sharing_info','size','is_single','_pad2[7]']
    types=['int','char','short','int64_t','Mesh','AttributeStorage',
           'Attribute','AttributeArray']
    sizes=[4,1,2,8,48,24,24,32]
    specs=[(4,[(0,0),(0,1),(0,2),(1,3),(0,4),(5,5)]),
           (5,[(6,6),(0,7),(1,8),(7,9)]),
           (6,[(1,10),(2,11),(1,12),(1,13),(1,8),(1,14)]),
           (7,[(1,14),(1,15),(3,16),(1,17),(1,18)])]
    dna=b'SDNANAME'+U32(len(names))+b''.join(x.encode()+b'\0' for x in names)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'TYPE'+U32(len(types))+b''.join(x.encode()+b'\0' for x in types)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'TLEN'+b''.join(U16(v) for v in sizes)
    dna+=b'\0'*(-len(dna)%4)
    dna+=b'STRC'+U32(len(specs))
    for t, fields in specs:
        dna+=U16(t)+U16(len(fields))
        for f,n in fields:dna+=U16(f)+U16(n)
    def block(code,data,old,sdna,count):
        return code+U32(sdna)+U64(old)+U64(len(data))+U64(count)+data
    mesh=U32(3)+U32(1)+U32(3)+U32(0)+U64(0x900)+U64(0x700)+U32(2)+U32(0)+U64(0)
    def attr(name, data, kind, domain):
        return U64(name)+U16(kind)+bytes([domain,0])+b'\x00'*4+U64(data)
    attrs=attr(0x300,0x500,7,0)+attr(0x400,0x600,3,3)
    def array(ptr):
        return U64(ptr)+U64(0)+U64(1000 if oversized else 3)+bytes([1 if single else 0])+b'\x00'*7
    vertices=b''.join(U32(v) for v in [0,0,0,0x3f800000,0,0,0,0x3f800000,0])
    corners=U32(0)+U32(1)+U32(2)
    blocks=[(b'ME\0\0',mesh,0x800,0,1),
            (b'DATA',attrs,0x700,2,2),
            (b'DATA',b'position\0',0x300,0,1),
            (b'DATA',b'.corner_vert\0',0x400,0,1),
            (b'DATA',array(0x100),0x500,3,1),
            (b'DATA',array(0x200),0x600,3,1),
            (b'DATA',vertices,0x100,0,3),
            (b'DATA',corners,0x200,0,3),
            (b'DATA',U32(0)+U32(3),0x900,0,2),
            (b'DNA1',dna,0xa00,0,1),
            (b'ENDB',b'',0,0,0)]
    return b'BLENDER17-01v0501'+b''.join(block(*b) for b in blocks)

with tempfile.TemporaryDirectory() as td:
    inp=pathlib.Path(td)/'modern-realistic.blend';out=pathlib.Path(td)/'modern-realistic.obj'
    inp.write_bytes(fixture_modern_layout())
    info=subprocess.run([str(EXE),'--info',str(inp)],capture_output=True,text=True)
    assert info.returncode==0 and 'Mesh schema: recognized' in info.stdout,(info.stdout,info.stderr)
    result=subprocess.run([str(EXE),'--obj',str(inp),str(out)],capture_output=True,text=True)
    assert result.returncode==0,(result.stdout,result.stderr)
    assert 'f 1 2 3' in out.read_text()
    print('PASS: realistic synthetic 5.1 layout: Attribute/Array and 64-bit size')
    for testcase in (fixture_modern_layout(single=True),fixture_modern_layout(oversized=True)):
        inp.write_bytes(testcase)
        result=subprocess.run([str(EXE),'--obj',str(inp),str(out)],capture_output=True,text=True)
        assert result.returncode!=0,(result.stdout,result.stderr)
        assert not out.exists()
    print('PASS: 5.1 single-value/oversized attributes rejected safely')
