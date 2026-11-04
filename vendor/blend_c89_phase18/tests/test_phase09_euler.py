"""Compare independent integer C89 Euler kernel with Blender's documented
order/parity quaternion equations in Python reference arithmetic.
Synthetic math and binary integration tests; not Blender-renderer equivalence.
"""
import math,random,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'

def reference(e,mode):
    axes=[(0,1,2),(0,2,1),(1,0,2),(1,2,0),(2,0,1),(2,1,0)]
    parity=[0,1,1,0,0,1]
    i,j,k=axes[mode-1]
    ti=e[i]*.5;tj=e[j]*(-.5 if parity[mode-1] else .5);th=e[k]*.5
    ci,cj,ch=math.cos(ti),math.cos(tj),math.cos(th)
    si,sj,sh=math.sin(ti),math.sin(tj),math.sin(th)
    cc,cs,sc,ss=ci*ch,ci*sh,si*ch,si*sh
    q=[cj*cc+sj*ss,0.,0.,0.]
    q[i+1]=cj*sc-sj*cs
    q[j+1]=cj*ss+sj*cc
    q[k+1]=cj*cs-sj*sc
    if parity[mode-1]:q[j+1]=-q[j+1]
    return q

def test_numeric():
    random.seed(9470)
    cases=[]
    for mode in range(1,7):
        for e in [(0.,0.,0.),(math.pi/2,0.,0.),(0.,math.pi/2,0.),(0.,0.,math.pi/2),(.4,-.3,1.2),(-3.0,2.4,-.7),(-6,4,8)]:
            cases.append((mode,e))
        for j in range(40):cases.append((mode,tuple(random.uniform(-9,9) for _ in range(3))))
    with tempfile.TemporaryDirectory() as td:
        probe=pathlib.Path(td)/'probe'
        subprocess.run(['cc','-std=c89','-pedantic','-Wall','-Wextra','-Werror','-O2','-I'+str(ROOT/'include'),str(ROOT/'tests/euler_probe.c'),str(ROOT/'src/bc_euler_q16.c'),str(ROOT/'src/bc_rig_q16.c'),'-o',str(probe)],check=True)
        data=''.join(f'{mode} '+ ' '.join(str(round(a*65536)) for a in e)+'\n' for mode,e in cases)
        r=subprocess.run([str(probe)],input=data,capture_output=True,text=True,check=True)
        lines=r.stdout.splitlines()
        assert len(lines)==len(cases)
        errors=[]
        for (mode,e),line in zip(cases,lines):
            got=[int(x)/65536 for x in line.split()]
            ref=reference(tuple(round(a*65536)/65536 for a in e),mode)
            err=max(abs(a-b) for a,b in zip(got,ref))
            errors.append(err)
            assert err < .0005,(mode,e,got,ref,err)
    print('PASS:',len(cases),'Euler rotation cases, all 6 Blender orders. Max component err:',max(errors))

def test_tsv():
    with tempfile.TemporaryDirectory() as td:
        td=pathlib.Path(td)
        ident=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
        (td/'rest.tsv').write_text('0 0 -1 Root '+' '.join(map(str,ident))+'\n')
        (td/'verts.tsv').write_text('0 1 0 0 1 0 1\n')
        (td/'euler.tsv').write_text('0 0 0 0 0 pose.bones["Root"].rotation_euler 2 5 1.5708\n')
        out=td/'out.tsv'
        res=subprocess.run([str(EXE),'--animate-tsv',str(td/'rest.tsv'),str(td/'euler.tsv'),str(td/'verts.tsv'),str(out)],capture_output=True,text=True)
        assert res.returncode==0,res.stderr
        row=out.read_text().splitlines()[1].split()
        assert abs(float(row[1])) < .002, row
        assert abs(float(row[2])-1) < .002, row
        bad=(td/'euler.tsv')
        bad.write_text(bad.read_text()+'0 0 0 0 1 pose.bones["Root"].rotation_quaternion 0 5 1\n')
        assert subprocess.run([str(EXE),'--animate-tsv',str(td/'rest.tsv'),str(bad),str(td/'verts.tsv'),str(out)],capture_output=True).returncode!=0
    print('PASS: Euler Z 90 degrees -> skinned OBJ-space point, quaternion/euler collision rejected')
if __name__=='__main__':test_numeric();test_tsv()

def test_name_with_spaces():
    with tempfile.TemporaryDirectory() as td:
        td=pathlib.Path(td)
        ident=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
        # Generated TSV uses tabs; bone names and RNA paths may have spaces.
        (td/'rest.tsv').write_text('\t'.join(['0','0','-1','Upper Arm.L']+list(map(str,ident)))+'\n')
        (td/'verts.tsv').write_text('0 1 0 0 1 0 1\n')
        (td/'keys.tsv').write_text('\t'.join(['0','0','0','0','0','pose.bones["Upper Arm.L"].rotation_euler','2','5','1.5708'])+'\n')
        target=td/'result.tsv'
        p=subprocess.run([str(EXE),'--animate-tsv',str(td/'rest.tsv'),str(td/'keys.tsv'),str(td/'verts.tsv'),str(target)],capture_output=True,text=True)
        assert p.returncode==0,(p.stderr,p.stdout)
        row=target.read_text().splitlines()[1].split()
        assert abs(float(row[1]))<.002 and abs(float(row[2])-1)<.002,row
        # Blender RNA paths quote/escape strings, not just simple names.
        bone='Upper "Arm".L'
        (td/'rest.tsv').write_text('\t'.join(['0','0','-1',bone]+list(map(str,ident)))+'\n')
        (td/'keys.tsv').write_text('\t'.join(['0','0','0','0','0','pose.bones["Upper \\"Arm\\".L"].rotation_euler','2','5','1.5708'])+'\n')
        p=subprocess.run([str(EXE),'--animate-tsv',str(td/'rest.tsv'),str(td/'keys.tsv'),str(td/'verts.tsv'),str(target)],capture_output=True,text=True)
        assert p.returncode==0,(p.stderr,p.stdout)
    print('PASS: bone names containing spaces and escaped quotes')

if __name__=='__main__':test_name_with_spaces()

def test_embedded_pose_rotation_mode():
    from test_phase04_animation import Fixture
    f=Fixture(ptr=8)
    f.defs['bPoseChannel']=[('bPoseChannel','*next'),('bPoseChannel','*prev'),('char','name[64]'),
        ('float','loc[3]'),('float','quat[4]'),('float','scale[3]'),('float','eul[3]'),('int','rotmode')]
    for _,nm in f.defs['bPoseChannel']:
        if nm not in f.names:f.names.append(nm)
    f.sizes.pop('bPoseChannel');f.size('bPoseChannel')
    f.add('Bone',[('Bone','*next'),('Bone','*prev'),('Bone','*parent'),('ListBase','childbase'),('char','name[64]'),('float','arm_mat[4][4]')])
    f.add('bArmature',[('ListBase','bonebase')])
    f.size('Bone');f.size('bArmature')
    ident=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
    f.block(b'AR\0\0',f.obj('bArmature',{'bonebase':f.packptr(0x1000)+f.packptr(0x1000)}),0x900,'bArmature')
    f.block(b'DATA',f.obj('Bone',{'name':b'Upper Arm.L\0','arm_mat':ident}),0x1000,'Bone')
    f.block(b'OB\0\0',f.obj('Object',{'pose':0x1900}),0x1800,'Object')
    f.block(b'DATA',f.obj('bPose',{'chanbase':f.packptr(0x1a00)+f.packptr(0x1a00)}),0x1900,'bPose')
    f.block(b'DATA',f.obj('bPoseChannel',{'name':b'Upper Arm.L\0','quat':[1,0,0,0],
        'scale':[1,1,1], 'eul':[0,0,0], 'rotmode':6}),0x1a00,'bPoseChannel')
    ptrs=[0x300,0x380,0x400]
    f.block(b'AC\0\0',f.obj('bAction',{'curves':f.packptr(ptrs[0])+f.packptr(ptrs[-1])}),0x200,'bAction')
    angles=(.4,-.3,1.2)
    for k,ptr in enumerate(ptrs):
        nxt=ptrs[k+1] if k<2 else 0
        f.block(b'DATA',f.obj('FCurve',{'next':nxt,'fpt':0x500+k*0x80,'totvert':2,
            'array_index':k,'rna_path':0x800}),ptr,'FCurve')
        f.block(b'DATA',f.obj('FPoint',{'vec':[1,angles[k]]})+f.obj('FPoint',{'vec':[9,angles[k]]}),0x500+k*0x80,'FPoint',2)
    f.block(b'DATA',b'pose.bones["Upper Arm.L"].rotation_euler\0',0x800)
    with tempfile.TemporaryDirectory() as td:
        td=pathlib.Path(td);src=td/'test.blend';src.write_bytes(f.build())
        pose=td/'pose.tsv';p=subprocess.run([str(EXE),'--pose',str(src),str(pose)],capture_output=True,text=True)
        assert p.returncode==0,(p.stderr,p.stdout)
        assert '\tUpper Arm.L\t' in pose.read_text() and pose.read_text().splitlines()[1].endswith('\t6')
        vertices=td/'vertices.tsv';vertices.write_text('0 1 0 0 1 0 1\n')
        out=td/'out.tsv'
        p=subprocess.run([str(EXE),'--play-blend',str(src),'5',str(vertices),str(out)],capture_output=True,text=True)
        assert p.returncode==0,(p.stdout,p.stderr)
        got=[float(x) for x in out.read_text().splitlines()[1].split()[1:]]
        qw,qx,qy,qz=reference(angles,6)
        # Rotate the X unit vector via quaternion rotation matrix (row-major).
        ref=[1-2*(qy*qy+qz*qz),2*(qx*qy+qz*qw),2*(qx*qz-qy*qw)]
        assert max(abs(x-y) for x,y in zip(got,ref)) < .003,(got,ref)
    print('PASS: embedded bPoseChannel rotmode=ZYX and Euler FCurve drive skinning from a binary SDNA fixture')
if __name__=='__main__':test_embedded_pose_rotation_mode()
