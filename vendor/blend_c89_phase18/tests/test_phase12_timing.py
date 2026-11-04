"""Phase11 end-to-end implicit Action selection; synthetic rig with valid modifier."""
from pathlib import Path
import tempfile, subprocess, sys, struct
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase04_animation import Fixture
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'

def command(*args):
    return subprocess.run([str(EXE),*map(str,args)],capture_output=True,text=True,timeout=30)

def fixture(ptr=8,cycle=False,broken=False,endian="<",nla=False,bad_action=False,
            nla_simple=False,nla_scale=1.0,nla_repeat=1.0,nla_flags=0,
            track_flags=0,second_track=None,blendin=0.0,two_rigs=False,anim_flags=0):
    # Serialized pointer widths and endianness are independent of host width.
    f=Fixture(ptr=ptr,endian=endian)
    f.sizes['short']=2
    f.types.append('short')
    f.add('AnimData',[('bAction','*action'),('int','slot_handle'),('ListBase','nla_tracks'),('int','flag')])
    f.add('NlaTrack',[('NlaTrack','*next'),('NlaTrack','*prev'),('ListBase','strips'),('int','flag'),('char','name[64]')])
    f.add('NlaStrip',[('NlaStrip','*next'),('NlaStrip','*prev'),('bAction','*act'),('int','action_slot_handle'),('char','name[64]'),('float','start'),('float','end'),('float','actstart'),('float','actend'),('float','scale'),('float','repeat'),('float','influence'),('float','blendin'),('float','blendout'),('short','type'),('short','blendmode'),('int','flag'),('ListBase','strips'),('ListBase','fcurves'),('ListBase','modifiers')])
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
    f.defs['Object']=[('ID','id'),('AnimData','*adt'),('bPose','*pose'),('Object','*parent'),('char','*data'),('ListBase','modifiers'),('float','loc[3]'),('float','size[3]'),('float','rot[3]'),('float','quat[4]'),('short','rotmode'),('short','padding')]
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
                'adt':f.packptr(0x4100 if idname=='Arm Object' else 0),
                'parent':f.packptr(0),'modifiers':f.packptr(mod)+f.packptr(mod),
                'loc':loc,'size':scale,'rot':[0,0,0], 'quat':[1,0,0,0],
                'rotmode':struct.pack(endian+'h',1)}
        return f.obj('Object',fields)
    ident=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
    f.block(b'AR\0\0',f.obj('bArmature',{'bonebase':f.packptr(0x1000)+f.packptr(0x1000)}),0x900,'bArmature')
    f.block(b'DATA',f.obj('Bone',{'name':b'Root\0','arm_mat':ident}),0x1000,'Bone')
    f.block(b'AC\0\0',f.obj('bAction',{}),0x100,'bAction')
    f.block(b'AC\0\0',f.obj('bAction',{'curves':f.packptr(0x300)+f.packptr(0x300)}),0x200,'bAction')
    f.block(b'DATA',f.obj('AnimData',{'action':f.packptr(0 if nla_simple else (0x3333 if bad_action else 0x200)), 'slot_handle':0,
         'nla_tracks':f.packptr(0x5000 if nla else 0)+f.packptr(0x5000 if nla else 0), 'flag':anim_flags}),0x4100,'AnimData')
    if nla:
        f.block(b'DATA',f.obj('NlaTrack',{'next':f.packptr(0x5100 if second_track else 0), 'flag':track_flags, 'strips':f.packptr(0x6000)+f.packptr(0x6000), 'name':b'Track\0'}),0x5000,'NlaTrack')
        f.block(b'DATA',f.obj('NlaStrip',{'act':f.packptr(0x200),'name':b'Walk\0', 'start':[1.0], 'end':[1.0+11.0*nla_scale*nla_repeat], 'actstart':[1.0], 'actend':[12.0], 'scale':[nla_scale], 'repeat':[nla_repeat], 'influence':[1.0], 'flag':nla_flags, 'blendin':[blendin]}),0x6000,'NlaStrip')
        if second_track:
            tf=4 if second_track=='muted_track' else 0
            sf=4096 if second_track=='muted_strip' else 0
            ss=40.0 if second_track=='out_of_range' else 1.0
            f.block(b'DATA',f.obj('NlaTrack',{'flag':tf,
                  'strips':f.packptr(0x6100)+f.packptr(0x6100),
                  'name':b'Second\0'}),0x5100,'NlaTrack')
            f.block(b'DATA',f.obj('NlaStrip',{'act':f.packptr(0x200),
                  'name':b'Second clip\0','start':[ss], 'end':[ss+11.0],
                  'actstart':[1.0], 'actend':[12.0], 'scale':[1.0],
                  'repeat':[1.0], 'influence':[1.0], 'flag':sf}),0x6100,'NlaStrip')
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
    if two_rigs:
        f.block(b'DATA',f.obj('AnimData',{'action':f.packptr(0x100)}),0x4500,'AnimData')
        second=bytearray(obj('Second Arm',0x900,0,[0,0,0],[1,1,1]))
        offset,_=f.layouts['Object']['adt']
        second[offset:offset+ptr]=f.packptr(0x4500)
        f.block(b'OB\0\0',bytes(second),0x4600,'Object')
    return f.build()



def first_vertices(text):
    return [line for line in text.splitlines() if line.startswith('v ')]

def main():
    with tempfile.TemporaryDirectory() as d:
        td=Path(d);src=td/'rig.blend';out=td/'out.obj'
        def sample(kwargs,frame,expect_ok=True):
            src.write_bytes(fixture(nla=True,nla_simple=True,**kwargs))
            r=command('--play-obj',src,frame,out,'auto')
            if expect_ok:
                assert r.returncode==0,(kwargs,frame,r.stderr)
                return first_vertices(out.read_text())
            assert r.returncode!=0,(kwargs,frame,r.stdout,r.stderr)
            return None
        for ptr,endian in ((8,'<'),(4,'<'),(8,'>'),(4,'>')):
            base={'ptr':ptr,'endian':endian}
            src.write_bytes(fixture(ptr=ptr,endian=endian))
            r=command('--play-obj',src,5,out,'auto')
            assert r.returncode==0,r.stderr
            reference=first_vertices(out.read_text())
            assert sample(dict(base,nla_scale=2),9)==reference
            assert sample(dict(base,nla_repeat=2),16)==reference
            assert sample(dict(base,nla_scale=2,nla_repeat=2),31)==reference
            assert sample(dict(base,nla_flags=3,track_flags=3),5)==reference
            assert sample(dict(base,second_track='muted_track'),5)==reference
            assert sample(dict(base,second_track='muted_strip'),5)==reference
            assert sample(dict(base,second_track='out_of_range'),5)==reference
            sample(dict(base,second_track='overlap'),5,False)
            sample(dict(base,nla_flags=2048),5,False) # reverse needs distinct rules
            sample(dict(base,nla_repeat=1.5),5,False) # partial cycle not yet supported
            sample(dict(base,blendin=1.0),5,False) # influence ramp
            sample(dict(base,track_flags=8),5,False) # solo track not yet evaluated
            sample(dict(base,anim_flags=2),5,False) # NLA explicitly disabled
            sample(dict(base,anim_flags=1),5,False) # global NLA solo mode
            sample(dict(base,nla_scale=0.5),5) # fractional scale supported
        # Across repeat boundary, local time wraps exactly to actstart.
        src.write_bytes(fixture(nla=True,nla_simple=True,nla_repeat=2.0))
        r=command('--play-sequence',src,11,13,td/'loop','auto')
        assert r.returncode==0,(r.stdout,r.stderr)
        frames=[first_vertices((td/f'loop_frame{i:04}.obj').read_text()) for i in range(3)]
        for i,frame in enumerate((11,1,2)):
            src.write_bytes(fixture())
            r=command('--play-obj',src,frame,out,'auto')
            assert r.returncode==0,(frame,r.stderr)
            assert frames[i]==first_vertices(out.read_text()),(i,frame)
        # A disjoint second track is eligible at its own interval.
        src.write_bytes(fixture(nla=True,nla_simple=True,second_track='out_of_range'))
        r=command('--play-obj',src,43,out,'auto')
        assert r.returncode==0,r.stderr
        second=first_vertices(out.read_text())
        src.write_bytes(fixture())
        r=command('--play-obj',src,4,out,'auto')
        assert r.returncode==0,r.stderr
        assert second==first_vertices(out.read_text())
        # Audit every armature even in scenes with more than one rig. This is
        # intentionally separate from the single-rig OBJ evaluator.
        for ptr,endian in ((8,'<'),(4,'>')):
            src.write_bytes(fixture(nla=True,nla_simple=True,ptr=ptr,endian=endian,
                                    nla_scale=2.0,two_rigs=True))
            report=td/'rig_timeline.tsv'
            r=command('--rig-timeline',src,9,report)
            assert r.returncode==0,(r.stderr,ptr,endian)
            lines=[ln for ln in report.read_text().splitlines() if not ln.startswith('#')]
            assert len(lines)==2,lines
            assert 'Arm Object\tnla\t1\t0\t5.0000' in lines[0],lines
            assert 'Second Arm\tdirect\t0\t0\t9.0000' in lines[1],lines
            r=command('--play-obj',src,9,out,'auto')
            assert r.returncode!=0,('multi-rig incorrectly accepted',r.stdout)
        print('PASS: NLA timeline retiming/repeat/flags, four serializations, conservative refusals')
if __name__=='__main__':main()
