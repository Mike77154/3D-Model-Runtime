"""Generated SDNA 'Bone.arm_mat' synthetic records, 32-bit and 64-bit references."""
import pathlib,struct,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'
def u16(n): return struct.pack('<H',n)
def u32(n): return struct.pack('<I',n)
def u64(n): return struct.pack('<Q',n)
def f32(n): return struct.pack('<f',n)
def dna(ps):
    types=['char','int','float','ListBase','bArmature','Bone']
    sizes=[1,4,4,2*ps,2*ps,3*ps+2*ps+64+24+64]
    fields=[('ListBase',[('char','*first'),('char','*last')]),('bArmature',[('ListBase','bonebase')]),
            ('Bone',[('Bone','*next'),('Bone','*prev'),('Bone','*parent'),('ListBase','childbase'),
                     ('char','name[64]'),('float','arm_head[3]'),('float','arm_tail[3]'),('float','arm_mat[4][4]')])]
    names=[]
    for _,ff in fields:
        for _,nm in ff:
            if nm not in names:names.append(nm)
    out=b'SDNANAME'+u32(len(names))+b''.join(a.encode()+b'\0' for a in names)
    out+=b'\0'*(-len(out)%4)
    out+=b'TYPE'+u32(len(types))+b''.join(a.encode()+b'\0' for a in types)
    out+=b'\0'*(-len(out)%4)
    out+=b'TLEN'+b''.join(u16(v) for v in sizes)
    out+=b'\0'*(-len(out)%4)
    out+=b'STRC'+u32(len(fields))
    for t, ff in fields:
        out+=u16(types.index(t))+u16(len(ff))
        for ft,nm in ff:out+=u16(types.index(ft))+u16(names.index(nm))
    return out

def fixture(ps):
    high=0x100000000 if ps==8 else 0
    P=u32 if ps==4 else u64
    d=dna(ps)
    def block(code,blob,addr,sdna,count=1):
        if ps==4:return code+u32(len(blob))+u32(addr)+u32(sdna)+u32(count)+blob
        return code+u32(sdna)+u64(addr)+u64(len(blob))+u64(count)+blob
    mat=(1,0,0,0,0,1,0,0,0,0,1,0,2,3,4,1)
    bone=(P(0)+P(0)+P(0)+P(0)+P(0)+b'Head\0'.ljust(64,b'\0')+
          b''.join(f32(v) for v in [0,0,0,0,1,0]+list(mat)))
    chunks=[block(b'AR\0\0',P(high+0x1000)+P(high+0x1000),high+0x900,1),
            block(b'DATA',bone,high+0x1000,2),block(b'DNA1',d,high+0xa00,0),
            block(b'ENDB',b'',0,0,0)]
    header=b'BLENDER_v279' if ps==4 else b'BLENDER17-01v0501'
    return header+b''.join(chunks)

with tempfile.TemporaryDirectory() as td:
    td=pathlib.Path(td)
    inp=td/'x.blend';out=td/'rest.tsv'
    for ps in (4,8):
        inp.write_bytes(fixture(ps))
        p=subprocess.run([str(EXE),'--rest-matrices',str(inp),str(out)],capture_output=True,text=True)
        assert p.returncode==0,(ps,p.stderr)
        row=[r for r in out.read_text().splitlines() if not r.startswith('#')][0].split('\t')
        assert row[:4]==['0','0','-1','Head'],row
        assert row[7]=='2.0000' and row[11]=='3.0000' and row[15]=='4.0000',row
    inp.write_bytes(fixture(4).replace(b'arm_mat[4][4]',b'arm_vat[4][4]'))
    p=subprocess.run([str(EXE),'--rest-matrices',str(inp),str(out)],capture_output=True,text=True)
    assert p.returncode!=0 and not out.exists(),p
print('PASS: rest-matrix DNA 32-bit / 64-bit pointer samples, unsupported format rejection')
