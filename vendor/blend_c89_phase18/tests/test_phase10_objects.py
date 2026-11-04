"""Phase10 Object and Armature modifier validation; synthetic + genuine corpus."""
from pathlib import Path
import subprocess, tempfile, struct, sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase04_animation import Fixture
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'

def command(*args):
    return subprocess.run([str(EXE),*map(str,args)],capture_output=True,text=True,timeout=25)

def fixture(ptr=8,cycle=False,broken=False,endian="<"):
    # Serialized pointer widths and endianness are independent of host width.
    f=Fixture(ptr=ptr,endian=endian)
    f.sizes['short']=2
    f.types.append('short')
    f.add('ID',[('char','name[66]')])
    f.add('ModifierData',[('ModifierData','*next'),('ModifierData','*prev')])
    f.add('ArmatureModifierData',[('ModifierData','modifier'),('short','deformflag'),('short','multi'),('Object','*object'),('char','defgrp_name[64]')])
    f.add('Bone',[('Bone','*next'),('Bone','*prev'),('Bone','*parent'),('ListBase','childbase'),('char','name[64]'),('float','arm_mat[4][4]')])
    f.add('bArmature',[('ListBase','bonebase')])
    f.add('Mesh',[('MVert','*mvert'),('MFace','*mface'),('int','totvert'),('int','totface'),('MDeformVert','*dvert'),('ListBase','vertex_group_names')])
    f.add('MVert',[('float','co[3]')])
    f.add('MFace',[('int','v1'),('int','v2'),('int','v3'),('int','v4')])
    f.add('MDeformVert',[('MDeformWeight','*dw'),('int','totweight')])
    f.add('MDeformWeight',[('int','def_nr'),('float','weight')])
    f.add('bDeformGroup',[('bDeformGroup','*next'),('bDeformGroup','*prev'),('char','name[64]')])
    f.defs['Object']=[('ID','id'),('bPose','*pose'),('Object','*parent'),('char','*data'),('ListBase','modifiers'),('float','loc[3]'),('float','size[3]'),('float','rot[3]'),('float','quat[4]'),('short','rotmode'),('short','padding')]
    for name,fields in f.defs.items():
        for _,field in fields:
            if field not in f.names:f.names.append(field)
    # Object depends on ID and ListBase; recompute it last.
    f.sizes.pop('Object',None)
    for name in list(f.defs):
        if name != 'Object':f.size(name)
    f.size('Object')
    def obj(idname,data,mod,loc,scale,pose=0):
        fields={'id':f.obj('ID',{'name':('OB'+idname).encode()+b'\0'}),
                'data':f.packptr(data),'pose':f.packptr(pose),
                'parent':f.packptr(0),'modifiers':f.packptr(mod)+f.packptr(mod),
                'loc':loc,'size':scale,'rot':[0,0,0], 'quat':[1,0,0,0],
                'rotmode':struct.pack(endian+'h',1)}
        return f.obj('Object',fields)
    ident=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
    f.block(b'AR\0\0',f.obj('bArmature',{'bonebase':f.packptr(0x1000)+f.packptr(0x1000)}),0x900,'bArmature')
    f.block(b'DATA',f.obj('Bone',{'name':b'Root\0','arm_mat':ident}),0x1000,'Bone')
    f.block(b'AC\0\0',f.obj('bAction',{'curves':f.packptr(0x300)+f.packptr(0x300)}),0x200,'bAction')
    f.block(b'DATA',f.obj('FCurve',{'fpt':0x500,'totvert':2,'array_index':0,'rna_path':0x800}),0x300,'FCurve')
    f.block(b'DATA',f.obj('FPoint',{'vec':[1,2.5]})+f.obj('FPoint',{'vec':[9,7]}),0x500,'FPoint',2)
    f.block(b'DATA',b'pose.bones["Root"].location\0',0x800)
    f.block(b'ME\0\0',f.obj('Mesh',{'mvert':f.packptr(0x2000),'mface':f.packptr(0x2100),'totvert':3,'totface':1,'dvert':f.packptr(0x2200),
        'vertex_group_names':f.packptr(0x2500)+f.packptr(0x2500)}),0x2600,'Mesh')
    f.block(b'DATA',b''.join(f.obj('MVert',{'co':co}) for co in ([1,0,0],[0,1,0],[0,0,0])),0x2000,'MVert',3)
    f.block(b'DATA',f.obj('MFace',{'v1':0,'v2':1,'v3':2,'v4':0}),0x2100,'MFace')
    dws=f.size('MDeformWeight')
    f.block(b'DATA',b''.join(f.obj('MDeformVert',{'dw':f.packptr(0x2300+i*dws),'totweight':1}) for i in range(3)),0x2200,'MDeformVert',3)
    f.block(b'DATA',b''.join(f.obj('MDeformWeight',{'def_nr':0,'weight':[1]}) for _ in range(3)),0x2300,'MDeformWeight',3)
    f.block(b'DATA',f.obj('bDeformGroup',{'name':b'Root\0'}),0x2500,'bDeformGroup')
    # Two Object instances: mesh object at 5 units, rig object at 1 unit with scale 2.
    f.block(b'OB\0\0',obj('Mesh Object',0x2600,0x4400,[5,0,0],[1,1,1]),0x4200,'Object')
    f.block(b'OB\0\0',obj('Arm Object',0x900,0,[1,0,0],[2,2,2]),0x4300,'Object')
    modifier=bytearray(f.obj('ArmatureModifierData',{'modifier':f.obj('ModifierData',{'next':f.packptr(0x4400 if cycle else 0)}),
         'object':f.packptr(0x9999 if broken else 0x4300)}))
    off,_=f.layouts['ArmatureModifierData']['deformflag']
    modifier[off:off+2]=struct.pack(endian+'H',1)
    f.block(b'DATA',bytes(modifier),0x4400,'ArmatureModifierData')
    return f.build()

def test_links():
    with tempfile.TemporaryDirectory() as t:
        p=Path(t);inp=p/'rig.blend';out=p/'obj.tsv';inp.write_bytes(fixture())
        res=command('--objects',inp,out)
        assert res.returncode==0,res.stderr
        rows=[x.split('\t') for x in out.read_text().splitlines()[1:]]
        assert len(rows)==2,rows
        assert rows[0][:10]==['0','Mesh Object','ME','-1','1','1','1','0','0','1'],rows[0][:10]
        assert rows[1][1:3]==['Arm Object','AR'],rows[1]
        tgt=p/'frame.obj';res=command('--play-obj',inp,5,tgt)
        assert res.returncode==0,res.stderr
        assert 'v 10.5000 0.0000 0.0000' in tgt.read_text(),tgt.read_text()[:400]
        # Endianness and 32-bit serialized addresses tested independently.
        for options in ({'ptr':4},{'ptr':4,'endian':'>'},{'ptr':8,'endian':'>'}):
            inp.write_bytes(fixture(**options))
            res=command('--play-obj',inp,5,tgt)
            assert res.returncode==0,(options,res.stderr)
            assert 'v 10.5000 0.0000 0.0000' in tgt.read_text(),(options,tgt.read_text()[:400])
        # Fail closed on broken rig links and cyclic modifier lists.
        for kw in ({'broken':True},{'cycle':True}):
            inp.write_bytes(fixture(**kw))
            res=command('--play-obj',inp,5,tgt)
            assert res.returncode!=0,(kw,res.stdout,res.stderr)
            if tgt.exists():tgt.unlink()
    print('PASS: armature target linkage, local affine correction, cyclic/broken pointer rejection')

def test_real():
    base=Path('/mnt/data/_blend_real_corpus')
    sources=[base/'blender-2.79a/release__datafiles__startup.blend',
             base/'blender-3.6.3/release__datafiles__startup.blend',
             base/'blender-4.5.14/release__datafiles__startup.blend']
    with tempfile.TemporaryDirectory() as t:
        for s in sources:
            if not s.exists():continue
            dest=Path(t)/'list.tsv';res=command('--objects',s,dest)
            assert res.returncode==0,(s,res.stderr)
            content=dest.read_text()
            assert '\tCube\tME\t' in content,(s,content[:800])
            assert len(content.splitlines())>=4
    print('PASS: source-archive authentic startup files, saved Object/ID graph & local TRS')

if __name__=='__main__':test_links();test_real()
