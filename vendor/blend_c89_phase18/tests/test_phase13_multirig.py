"""Two genuinely distinct serialized armatures, meshes, actions and same-name bones.
Fixtures generated in Python; not genuine Blender-authored playback references.
"""
from pathlib import Path
import struct,subprocess,tempfile,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase04_animation import Fixture
EXE=Path(__file__).resolve().parents[1]/'blendc89'

def make_file(ptr=8,endian='<',bad_target=False,unsupported_modifier=False,nla_first=False,
              constraint=None,ik_bad=None,ik_arm_x=0,ik_rest_variant=None):
    f=Fixture(ptr=ptr,endian=endian)
    f.types.append('short');f.sizes['short']=2
    f.add('AnimData',[('bAction','*action'),('int','slot_handle'),('ListBase','nla_tracks'),('int','flag')])
    f.add('NlaTrack',[('NlaTrack','*next'),('NlaTrack','*prev'),('ListBase','strips'),('int','flag'),('char','name[64]')])
    f.add('NlaStrip',[('NlaStrip','*next'),('NlaStrip','*prev'),('bAction','*act'),('int','action_slot_handle'),('char','name[64]'),('float','start'),('float','end'),('float','actstart'),('float','actend'),('float','scale'),('float','repeat'),('float','influence'),('float','blendin'),('float','blendout'),('short','type'),('short','blendmode'),('int','flag'),('ListBase','strips'),('ListBase','fcurves'),('ListBase','modifiers')])
    f.add('ID',[('char','name[66]')])
    f.add('ModifierData',[('ModifierData','*next'),('ModifierData','*prev')])
    f.add('ArmatureModifierData',[('ModifierData','modifier'),('short','deformflag'),('short','multi'),('Object','*object'),('char','defgrp_name[64]')])
    f.add('Bone',[('Bone','*next'),('Bone','*prev'),('Bone','*parent'),('ListBase','childbase'),('char','name[64]'),('float','arm_mat[4][4]'),('float','arm_head[3]'),('float','arm_tail[3]')])
    f.add('bArmature',[('ListBase','bonebase')])
    if constraint is not None:
        f.sizes.pop('bPose',None)
        f.sizes.pop('bPoseChannel',None)
        f.add('bPose',[('ListBase','chanbase')])
        f.add('bPoseChannel',[('bPoseChannel','*next'),('bPoseChannel','*prev'),
            ('ListBase','constraints'),('char','name[64]'),('float','loc[3]'),
            ('float','quat[4]'),('float','scale[3]'),('float','eul[3]'),('short','rotmode')])
        f.add('bConstraint',[('bConstraint','*next'),('bConstraint','*prev'),
            ('char','*data'),('short','type'),('short','flag'),('char','name[64]'),('char','ownspace'),
            ('char','tarspace'),('float','enforce'),('float','headtail')])
        f.add('bLocateLikeConstraint',[('Object','*tar'),('int','flag'),
            ('int','reserved1'),('char','subtarget[64]')])
        f.add('bRotateLikeConstraint',[('Object','*tar'),('int','flag'),
            ('char','mix_mode'),('char','euler_order'),('char','_pad[2]'),('char','subtarget[64]')])
        f.add('bTransLikeConstraint',[('Object','*tar'),('int','flag'),
            ('char','mix_mode'),('char','_pad[3]'),('char','subtarget[64]')])
        if constraint=='ik':
            f.add('bKinematicConstraint',[('Object','*tar'),('short','iterations'),('short','flag'),
                ('short','rootbone'),('short','max_rootbone'),('char','subtarget[64]'),
                ('Object','*poletar'),('char','polesubtarget[64]'),('float','poleangle'),
                ('float','weight'),('float','orientweight'),('float','grabtarget[3]'),
                ('short','type'),('short','mode'),('float','dist')])
    f.add('Mesh',[('MVert','*mvert'),('MFace','*mface'),('int','totvert'),('int','totface'),('MDeformVert','*dvert'),('ListBase','vertex_group_names')])
    f.add('MVert',[('float','co[3]')]);f.add('MFace',[('int','v1'),('int','v2'),('int','v3'),('int','v4')])
    f.add('MDeformVert',[('MDeformWeight','*dw'),('int','totweight')])
    f.add('MDeformWeight',[('int','def_nr'),('float','weight')])
    f.add('bDeformGroup',[('bDeformGroup','*next'),('bDeformGroup','*prev'),('char','name[64]')])
    f.defs['Object']=[('ID','id'),('AnimData','*adt'),('bPose','*pose'),('Object','*parent'),('char','*data'),('ListBase','modifiers'),('float','loc[3]'),('float','size[3]'),('float','rot[3]'),('float','quat[4]'),('short','rotmode'),('short','padding')]
    for nm,fields in f.defs.items():
        for _,fld in fields:
            if fld not in f.names:f.names.append(fld)
    f.sizes.pop('Object',None)
    for nm in list(f.defs):
        if nm!='Object':f.size(nm)
    f.size('Object')
    def object_data(name,data,modifier=0,adt=0):
        return f.obj('Object',{'id':f.obj('ID',{'name':('OB'+name).encode()+b'\0'}),
            'data':f.packptr(data),'modifiers':f.packptr(modifier)+f.packptr(modifier),
            'adt':f.packptr(adt),'size':[1,1,1],'quat':[1,0,0,0],
            'rotmode':struct.pack(endian+'h',1)})
    ident=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
    # Phase17 synthetic rest orientations. All matrices are serialized
    # column-major exactly as Bone.arm_mat stores them in Blender DNA.
    phase17_root=ident
    phase17_child=[1,0,0,0,0,1,0,0,0,0,1,0,0,1,0,1]
    phase17_head1=[0,1,0];phase17_tail0=[0,1,0]
    phase17_tail1=[0,2,0];phase17_vertex=[0,2,0]
    if constraint=='ik' and ik_rest_variant:
        if ik_rest_variant in ('minusx','roll90','broken_axis','broken_chain','scaled'):
            phase17_root=[0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1]
            phase17_child=[0,1,0,0,-1,0,0,0,0,0,1,0,-1,0,0,1]
            phase17_head1=[-1,0,0];phase17_tail0=[-1,0,0]
            phase17_tail1=[-2,0,0];phase17_vertex=[-2,0,0]
            if ik_rest_variant=='roll90':
                phase17_root=[0,0,-1,0,-1,0,0,0,0,1,0,0,0,0,0,1]
                phase17_child=[0,0,-1,0,-1,0,0,0,0,1,0,0,-1,0,0,1]
            if ik_rest_variant=='broken_axis':
                phase17_root=ident
            if ik_rest_variant=='broken_chain':
                phase17_head1=[-2,0,0]
                phase17_child[-4]=-2
            if ik_rest_variant=='scaled':
                phase17_root=list(phase17_root);phase17_root[1]=2
        elif isinstance(ik_rest_variant,tuple) and len(ik_rest_variant)==2 and ik_rest_variant[0]=='basis':
            x,y,z=ik_rest_variant[1]
            phase17_root=[*x,0,*y,0,*z,0,0,0,0,1]
            phase17_child=[*x,0,*y,0,*z,0,*y,1]
            phase17_head1=list(y);phase17_tail0=list(y)
            phase17_tail1=[2*v for v in y];phase17_vertex=[2*v for v in y]
        elif ik_rest_variant=='plusz':
            phase17_root=[1,0,0,0,0,0,1,0,0,-1,0,0,0,0,0,1]
            phase17_child=[1,0,0,0,0,0,1,0,0,-1,0,0,0,0,1,1]
            phase17_head1=[0,0,1];phase17_tail0=[0,0,1]
            phase17_tail1=[0,0,2];phase17_vertex=[0,0,2]
        else:
            raise ValueError('unknown IK rest variant')
    # Rig ID 0 and Rig ID 1 have IDENTICAL bone names to test isolation.
    for ar,bone in ((0x900,0x1000),(0xa00,0x1100)):
        f.block(b'AR\0\0',f.obj('bArmature',{'bonebase':f.packptr(bone)+f.packptr(bone)}),ar,'bArmature')
        f.block(b'DATA',f.obj('Bone',{'name':b'Root\0','arm_mat':phase17_root if constraint=='ik' and ar==0xa00 else ident,
            'arm_head':[0,0,0],'arm_tail':phase17_tail0 if constraint=='ik' and ar==0xa00 else [0,1,0],
            'next':f.packptr(0x1180) if constraint is not None and ar==0xa00 and constraint!='ik' else 0,
            'childbase':f.packptr(0x1180)*2 if constraint=='ik' and ar==0xa00 else f.packptr(0)*2}),bone,'Bone')
        if constraint is not None and ar==0xa00:
            f.blocks[-2]=(f.blocks[-2][0],f.obj('bArmature',
                {'bonebase':f.packptr(bone)+f.packptr(bone if constraint=='ik' else 0x1180)}),ar,
                f.blocks[-2][3],f.blocks[-2][4])
            f.block(b'DATA',f.obj('Bone',{'name':b'Follower\0',
                'parent':f.packptr(bone) if constraint=='ik' else 0,
                'arm_mat':phase17_child if constraint=='ik' else ident,
                'arm_head':phase17_head1 if constraint=='ik' else [0,1,0],
                'arm_tail':phase17_tail1 if constraint=='ik' else [0,2,0]}),0x1180,'Bone')
    # Action 0 moves X +2; Action 1 moves Y -3.
    for action,curve,sample,path,component,value in (
        (0x200,0x400,0x600,0x800,0,2.0),
        (0x300,0x500,0x700,0x840,1,-3.0)):
        extra=(constraint=='rotation' and action==0x300)
        f.block(b'AC\0\0',f.obj('bAction',{'curves':f.packptr(curve)+f.packptr(0x580 if extra else curve)}),action,'bAction')
        f.block(b'DATA',f.obj('FCurve',{'next':0x580 if extra else 0,'fpt':sample,'totvert':2,'array_index':component,'rna_path':path}),curve,'FCurve')
        f.block(b'DATA',f.obj('FPoint',{'vec':[1,value if component==0 else -1.0]})+f.obj('FPoint',{'vec':[9,value if component==0 else -5.0]}),sample,'FPoint',2)
        f.block(b'DATA',b'pose.bones["Root"].location\0',path)
        if extra:
            f.block(b'DATA',f.obj('FCurve',{'fpt':0x780,'totvert':2,'array_index':2,'rna_path':0x880}),0x580,'FCurve')
            f.block(b'DATA',f.obj('FPoint',{'vec':[1,1.5707963268]})+f.obj('FPoint',{'vec':[9,1.5707963268]}),0x780,'FPoint',2)
            f.block(b'DATA',b'pose.bones["Root"].rotation_euler\0',0x880)
    for p,a in ((0x4800,0x200),(0x4900,0x300)):
        f.block(b'DATA',f.obj('AnimData',{'action':f.packptr(0 if nla_first and a==0x300 else a), 'nla_tracks':f.packptr(0x5000)+f.packptr(0x5000) if nla_first and a==0x300 else f.packptr(0)*2}),p,'AnimData')
    if nla_first:
        f.block(b'DATA',f.obj('NlaTrack',{'strips':f.packptr(0x6000)*2}),0x5000,'NlaTrack')
        f.block(b'DATA',f.obj('NlaStrip',{'act':f.packptr(0x300),'start':[1],'end':[17], 'actstart':[1], 'actend':[9], 'scale':[2], 'repeat':[1], 'influence':[1]}),0x6000,'NlaStrip')
    dws=f.size('MDeformWeight')
    for mesh,vs,face,dv,dw,group,initial in (
        (0x2600,0x2000,0x2200,0x2400,0x2800,0x2a00,1),
        (0x2700,0x2100,0x2300,0x2500,0x2900,0x2b00,3)):
        f.block(b'ME\0\0',f.obj('Mesh',{'mvert':f.packptr(vs),'mface':f.packptr(face),
            'totvert':3,'totface':1,'dvert':f.packptr(dv),
            'vertex_group_names':f.packptr(group)+f.packptr(group)}),mesh,'Mesh')
        f.block(b'DATA',b''.join(f.obj('MVert',{'co':coords}) for coords in (phase17_vertex if constraint=='ik' and mesh==0x2600 else [initial,0,0],[0,1,0],[0,0,0])),vs,'MVert',3)
        f.block(b'DATA',f.obj('MFace',{'v1':0,'v2':1,'v3':2}),face,'MFace')
        f.block(b'DATA',b''.join(f.obj('MDeformVert',{'dw':f.packptr(dw+i*dws),'totweight':1}) for i in range(3)),dv,'MDeformVert',3)
        f.block(b'DATA',b''.join(f.obj('MDeformWeight',{'def_nr':0,'weight':[1]}) for i in range(3)),dw,'MDeformWeight',3)
        f.block(b'DATA',f.obj('bDeformGroup',{'name':b'Follower\0' if constraint is not None and mesh==0x2600 else b'Root\0'}),group,'bDeformGroup')
    # CROSS-BIND: Mesh 0 -> Rig 1 (action 1), Mesh 1 -> Rig 0 (action 0)
    meshone=bytearray(object_data('FirstMesh',0x2600,0x4600))
    if constraint=='ik' and ik_arm_x:
        off,_=f.layouts['Object']['loc']
        meshone[off:off+12]=struct.pack(endian+'fff',float(ik_arm_x),0.0,0.0)
    f.block(b'OB\0\0',bytes(meshone),0x4200,'Object')
    f.block(b'OB\0\0',object_data('RigZero',0x900,adt=0x4800),0x4300,'Object')
    f.block(b'OB\0\0',object_data('SecondMesh',0x2700,0x4700),0x4400,'Object')
    rigone=bytearray(object_data('RigOne',0xa00,adt=0x4900))
    if constraint is not None:
        off,_=f.layouts['Object']['pose']
        rigone[off:off+ptr]=f.packptr(0x5200)
    if constraint=='ik' and ik_arm_x:
        off,_=f.layouts['Object']['loc']
        rigone[off:off+12]=struct.pack(endian+'fff',float(ik_arm_x),0.0,0.0)
    f.block(b'OB\0\0',bytes(rigone),0x4500,'Object')
    if constraint is not None:
        f.block(b'DATA',f.obj('bPose',{'chanbase':f.packptr(0x5300)+f.packptr(0x5400)}),0x5200,'bPose')
        f.block(b'DATA',f.obj('bPoseChannel',{'name':b'Root\0','next':0x5400,
            'quat':[1,0,0,0],'scale':[1,1,1],
            'rotmode':struct.pack(endian+'h',1 if constraint=='rotation' else 0)}),0x5300,'bPoseChannel')
        f.block(b'DATA',f.obj('bPoseChannel',{'name':b'Follower\0',
            'constraints':f.packptr(0x5500)*2,'quat':[1,0,0,0],
            'scale':[1,1,1]}),0x5400,'bPoseChannel')
        flags=512 if constraint=='disabled' or ik_bad=='disabled' else 0
        kind={'location':9,'transforms':23,'rotation':8,
              'unsupported':3,'disabled':9,'wrong_target':9,'forward':9,
              'partial':9,'mix':23,'ik':3}[constraint]
        c=bytearray(f.obj('bConstraint',{'data':f.packptr(0x5600),
            'enforce':[0.5 if constraint=='partial' else 1], 'headtail':[0]}))
        for key,value in [('type',kind),('flag',flags)]:
            off,_=f.layouts['bConstraint'][key]
            c[off:off+2]=struct.pack(endian+'H',value)
        for key in ('ownspace','tarspace'):
            off,_=f.layouts['bConstraint'][key]
            c[off]=2
        f.block(b'DATA',bytes(c),0x5500,'bConstraint')
        dt='bKinematicConstraint' if constraint=='ik' else {9:'bLocateLikeConstraint',23:'bTransLikeConstraint',8:'bRotateLikeConstraint'}.get(kind,'bLocateLikeConstraint')
        target=b'Follower\0' if constraint=='forward' else b'Root\0'
        dst=f.obj(dt,({'tar':f.packptr(0xdead0000 if ik_bad=='bad_target' else 0x4b00),
            'poletar':f.packptr(0 if ik_bad=='no_pole' else 0x4c00),
            'flag':struct.pack(endian+'h',49 if ik_bad=='stretch' else 33),
            'rootbone':struct.pack(endian+'h',3 if ik_bad=='chain3' else 2),
            'weight':[0.5 if ik_bad=='partial_weight' else 1],
            'orientweight':[0], 'poleangle':[0.2 if ik_bad=='pole_angle' else 0]} if constraint=='ik' else {
            'tar':f.packptr(0xDEADBEEF if constraint=='wrong_target' else 0x4500),
            'flag':7 if kind in (8,9) else 0,'subtarget':target}))
        if constraint=='mix':
            dst=bytearray(dst);off,_=f.layouts[dt]['mix_mode'];dst[off]=1
            dst=bytes(dst)
        f.block(b'DATA',dst,0x5600,dt)
    if constraint=='ik':
        target=bytearray(object_data('TargetIK',0))
        poletar=bytearray(object_data('PoleIK',0))
        off,_=f.layouts['Object']['loc']
        # A root translation Action is evaluated in the root's rest-local frame.
        # Rotate static IK target/pole consistently with the rest orientation.
        if ik_rest_variant in ('minusx','roll90','broken_axis','broken_chain','scaled'):
            goal=(ik_arm_x+4,1.0,0.0); pole=(ik_arm_x+3,2.0,0.0)
        elif isinstance(ik_rest_variant,tuple) and ik_rest_variant[0]=='basis':
            y=ik_rest_variant[1][1]
            origin=(-3*y[0],-3*y[1],-3*y[2])
            goal=(ik_arm_x+origin[0]+1,origin[1]+1,origin[2])
            pole=(ik_arm_x+origin[0],origin[1]+2,origin[2])
        elif ik_rest_variant=='plusz':
            goal=(ik_arm_x+1,0.0,-2.0);pole=(ik_arm_x,0.0,-1.0)
        else:
            goal=(ik_arm_x+1,-2.0,0.0);pole=(ik_arm_x,-1.0,0.0)
        if ik_bad=='unreachable':goal=(ik_arm_x+5,-2.0,0.0)
        if ik_bad=='colinear':pole=goal
        target[off:off+12]=struct.pack(endian+'fff',*goal)
        poletar[off:off+12]=struct.pack(endian+'fff',*pole)
        if ik_bad=='animated_target':
            off,_=f.layouts['Object']['adt'];target[off:off+ptr]=f.packptr(0x4800)
        if ik_bad=='parented_target':
            off,_=f.layouts['Object']['parent'];target[off:off+ptr]=f.packptr(0x4300)
        f.block(b'OB\0\0',bytes(target),0x4b00,'Object')
        f.block(b'OB\0\0',bytes(poletar),0x4c00,'Object')
    for addr,owner in ((0x4600,0x4500),(0x4700,0x4300)):
        if bad_target and addr==0x4700:owner=0xDEADBEEF
        mod=bytearray(f.obj('ArmatureModifierData',{'object':f.packptr(owner)}))
        off,_=f.layouts['ArmatureModifierData']['deformflag']
        mod[off:off+2]=struct.pack(endian+'H',2 if unsupported_modifier and addr==0x4700 else 1)
        f.block(b'DATA',bytes(mod),addr,'ArmatureModifierData')
    return f.build()

def run(*args):return subprocess.run([str(EXE),*map(str,args)],capture_output=True,text=True,timeout=30)
def vertices(text):return [x for x in text.splitlines() if x.startswith('v ')]
def main():
    with tempfile.TemporaryDirectory() as tmp:
        d=Path(tmp);blend=d/'scene.blend';pref=d/'frame'
        for ptr,endian in ((4,'<'),(8,'<'),(4,'>'),(8,'>')):
            blend.write_bytes(make_file(ptr,endian))
            report=d/'rig_pairs.tsv'
            r=run('--rig-pairs',blend,report)
            assert r.returncode==0,(ptr,endian,r.stderr)
            mappings=[ln.split('\t')[:3] for ln in report.read_text().splitlines() if not ln.startswith('#')]
            assert mappings==[['0','1','3'],['1','0','1']],mappings
            r=run('--play-rigs',blend,5,pref)
            assert r.returncode==0,(ptr,endian,r.stderr)
            a=d/'frame_mesh0000.obj';b=d/'frame_mesh0001.obj'
            assert a.exists() and b.exists()
            av=vertices(a.read_text());bv=vertices(b.read_text())
            assert len(av)==len(bv)==3,(av,bv)
            assert av[0]=='v 1.0000 -3.0000 0.0000',(ptr,endian,av)
            assert bv[0]=='v 5.0000 0.0000 0.0000',(ptr,endian,bv)
            assert 'f 1 2 3' in a.read_text() and 'f 1 2 3' in b.read_text()
        # RigOne uses scene frame 9 -> Action frame 5 through NLA scale=2.
        # RigZero still uses direct frame 9; same bone name remains isolated.
        blend.write_bytes(make_file(nla_first=True))
        r=run('--play-rigs',blend,9,pref)
        assert r.returncode==0,r.stderr
        assert vertices((d/'frame_mesh0000.obj').read_text())[0]=='v 1.0000 -3.0000 0.0000'
        assert vertices((d/'frame_mesh0001.obj').read_text())[0]=='v 5.0000 0.0000 0.0000'
        for kw in ({'bad_target':True},{'unsupported_modifier':True}):
            for p in d.glob('frame_mesh*.obj'):p.unlink()
            blend.write_bytes(make_file(**kw))
            report=d/'rig_pairs.tsv'
            r=run('--rig-pairs',blend,report)
            assert r.returncode!=0,(kw,r.stdout,r.stderr)
            r=run('--play-rigs',blend,5,pref)
            assert r.returncode!=0,(kw,r.stdout,r.stderr)
            assert not list(d.glob('frame_mesh*.obj')),(kw,'partial files')
    print('PASS: two independent rigs, cross-bound meshes, identical bone names, mixed NLA/direct, four binary layouts, fail-closed graphs')
if __name__=='__main__':main()
