"""Synthetic SDNA rig and skin fixtures; not Blender-produced files."""
import pathlib
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXE = ROOT / 'blendc89'
U16=lambda x:struct.pack('<H',x)
U32=lambda x:struct.pack('<I',x)
F32=lambda x:struct.pack('<f',x)

def make_dna(defs,sz):
    # defs: [(type, [(type,name),...])]
    name=[]
    type_names=['int','float','char','ListBase','Mesh','Object','MDeformVert','MDeformWeight','bDeformGroup','bArmature','Bone']
    def ni(x):
        if x not in name:name.append(x)
        return name.index(x)
    for _,fields in defs:
        for _,nm in fields:ni(nm)
    out=b'SDNANAME'+U32(len(name))+b''.join(x.encode()+b'\0' for x in name)
    out+=b'\0'*(-len(out)%4)
    out+=b'TYPE'+U32(len(type_names))+b''.join(x.encode()+b'\0' for x in type_names)
    out+=b'\0'*(-len(out)%4)
    out+=b'TLEN'+b''.join(U16(sz[x]) for x in type_names)
    out+=b'\0'*(-len(out)%4)
    out+=b'STRC'+U32(len(defs))
    for ty,fields in defs:
        out+=U16(type_names.index(ty))+U16(len(fields))
        for ft,fn in fields:out+=U16(type_names.index(ft))+U16(ni(fn))
    return out

def pack(name,data,ptr,sdna,count=1):
    return name+U32(len(data))+U32(ptr)+U32(sdna)+U32(count)+data

def build_skin(cycle=False,negative=False,use_object=False):
    defs=[('ListBase',[('char','*first'),('char','*last')]),
      ('Mesh',[('MDeformVert','*dvert'),('int','totvert'),('ListBase','vertex_group_names')]),
      ('Object',[('Mesh','*data'),('ListBase','defbase')]),
      ('MDeformVert',[('MDeformWeight','*dw'),('int','totweight'),('int','flag')]),
      ('MDeformWeight',[('int','def_nr'),('float','weight')]),
      ('bDeformGroup',[('bDeformGroup','*next'),('bDeformGroup','*prev'),('char','name[64]')])]
    lens={'int':4,'float':4,'char':1,'ListBase':8,'Mesh':16,'Object':12,
          'MDeformVert':12,'MDeformWeight':8,'bDeformGroup':72,'bArmature':8,'Bone':108}
    dna=make_dna(defs,lens)
    mesh=U32(0x200)+U32(3)+U32(0 if use_object else 0x500)+U32(0 if use_object else 0x600)
    ob=U32(0x100)+U32(0x500)+U32(0x600)
    dvert=U32(0x300)+U32(2)+U32(0)+U32(0x400)+U32(1)+U32(0)+U32(0)+U32(0)+U32(0)
    weight1=U32(0)+F32(0.75)+U32(1)+F32(-0.25 if negative else 0.25)
    weight2=U32(1)+F32(1)
    group1=U32(0x500 if cycle else 0x600)+U32(0)+b'Root\0'.ljust(64,b'\0')
    group2=U32(0)+U32(0x500)+b'Arm.L\0'.ljust(64,b'\0')
    chunks=[pack(b'ME\0\0',mesh,0x100,1),pack(b'OB\0\0',ob,0x800,2),
      pack(b'DATA',dvert,0x200,3,3),pack(b'DATA',weight1,0x300,4,2),
      pack(b'DATA',weight2,0x400,4),pack(b'DATA',group1,0x500,5),
      pack(b'DATA',group2,0x600,5),pack(b'DNA1',dna,0x900,0),
      pack(b'ENDB',b'',0,0,0)]
    return b'BLENDER_v279'+b''.join(chunks)

def build_bones(cycle=False):
    defs=[('ListBase',[('char','*first'),('char','*last')]),
          ('bArmature',[('ListBase','bonebase')]),
          ('Bone',[('Bone','*next'),('Bone','*prev'),('Bone','*parent'),
                   ('ListBase','childbase'),('char','name[64]'),
                   ('float','arm_head[3]'),('float','arm_tail[3]')])]
    lens={'int':4,'float':4,'char':1,'ListBase':8,'Mesh':16,'Object':12,
          'MDeformVert':12,'MDeformWeight':8,'bDeformGroup':72,'bArmature':8,'Bone':108}
    dna=make_dna(defs,lens)
    def bone(next_,prev,parent,child,name,head,tail):
        return (U32(next_)+U32(prev)+U32(parent)+U32(child)+U32(child)+
                name.encode().ljust(64,b'\0')+b''.join(F32(x) for x in head+tail))
    root=bone(0,0,0,0x1200,'Root',(0,0,0),(0,1,0))
    child=bone(0x1100 if cycle else 0,0,0x1100,0,'Forearm',(0,1,0),(1,1,0))
    chunks=[pack(b'AR\0\0',U32(0x1100)+U32(0x1100),0x900,1),
            pack(b'DATA',root,0x1100,2),pack(b'DATA',child,0x1200,2),
            pack(b'DNA1',dna,0xa00,0),pack(b'ENDB',b'',0,0,0)]
    return b'BLENDER_v279'+b''.join(chunks)

def check(cmd,data,expect,good=True):
    with tempfile.TemporaryDirectory() as td:
        inp=pathlib.Path(td)/'rig.blend';out=pathlib.Path(td)/'out.tsv'
        inp.write_bytes(data)
        ret=subprocess.run([str(EXE),cmd,str(inp),str(out)],capture_output=True,text=True)
        if good:
            assert ret.returncode==0,(cmd,ret.stderr,ret.stdout)
            assert expect in out.read_text(),out.read_text()
        else:
            assert ret.returncode!=0,(cmd,ret.stdout,ret.stderr)
            assert not out.exists(),"partial TSV not cleaned"

check('--weights',build_skin(),'0\t0\t0\tRoot\t0.7500')
check('--weights',build_skin(use_object=True),'0\t0\t1\tArm.L\t0.2500')
check('--weights',build_skin(negative=True),'',False)
check('--weights',build_skin(cycle=True),'',False)
check('--bones',build_bones(),'0\t1\t0\tForearm\t0.0000\t1.0000\t0.0000')
check('--bones',build_bones(cycle=True),'',False)
print('PASS: 6 rig fixtures: mesh names, object fallback, invalid weight, group cycle, bone hierarchy, bone cycle')

# LargeBHead8 and 64-bit serialized pointer tokens, including high 32-bit limbs.
def build_bones64():
    defs=[('ListBase',[('char','*first'),('char','*last')]),
          ('bArmature',[('ListBase','bonebase')]),
          ('Bone',[('Bone','*next'),('Bone','*prev'),('Bone','*parent'),
                   ('ListBase','childbase'),('char','name[64]'),
                   ('float','arm_head[3]'),('float','arm_tail[3]')])]
    sizes={'int':4,'float':4,'char':1,'ListBase':16,'Mesh':32,'Object':24,
          'MDeformVert':16,'MDeformWeight':8,'bDeformGroup':80,'bArmature':16,'Bone':128}
    dna=make_dna(defs,sizes)
    P=lambda v:struct.pack('<Q',v)
    base=0x10000000000
    def b64(name,prev,parent,child):
        return (P(0)+P(prev)+P(parent)+P(child)+P(child)+
                name.encode().ljust(64,b'\0')+
                b''.join(F32(x) for x in (0,0,0,0,2,0)))
    def block(code,data,old,sdna,count=1):
        return code+U32(sdna)+P(old)+P(len(data))+P(count)+data
    return b'BLENDER17-01v0501'+b''.join([
        block(b'AR\0\0',P(base+0x1000)+P(base+0x1000),base+0x900,1),
        block(b'DATA',b64('Main',0,0,base+0x2000),base+0x1000,2),
        block(b'DATA',b64('Extra',0,base+0x1000,0),base+0x2000,2),
        block(b'DNA1',dna,base+0xa00,0),
        block(b'ENDB',b'',0,0)])

check('--bones',build_bones64(),'0\t1\t0\tExtra')
print('PASS: LargeBHead8 + 64-bit serialized bone addresses with nonzero high word')
