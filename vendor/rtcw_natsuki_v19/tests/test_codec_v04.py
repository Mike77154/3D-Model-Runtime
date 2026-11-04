"""v0.4 deterministic synthetic bone-space and independent torso/legs tests."""
from pathlib import Path
import struct, subprocess, sys, tempfile
sys.path.insert(0,str(Path(__file__).parent))
from test_codec import mds
root=Path(sys.argv[1]).resolve(); runner=root/'rtcw_mds_anim';anim=root/'rtcw_anim'

def execute(exe,model,la,lb,lt,ta,tb,tt,yaw,t,ok=True):
    args=[str(exe),str(model),*map(str,(la,lb,lt,ta,tb,tt,yaw)),
          str(t/'pose.obj'),str(t/'pose.ppm'),str(t/'pose.txt')]
    p=subprocess.run(args,capture_output=True,text=True)
    assert (p.returncode==0)==ok,(p.returncode,p.stderr,args)
    if not ok:return None
    lines=(t/'pose.obj').read_text().splitlines()
    vertex=[[float(x) for x in s.split()[1:]] for s in lines if s.startswith('v ')]
    report=(t/'pose.txt').read_text()
    return vertex,report

def mutate(data,offset,fmt,value):
    b=bytearray(data);struct.pack_into('<'+fmt,b,offset,value);return bytes(b)

with tempfile.TemporaryDirectory() as temp:
    t=Path(temp);a=mds();filename=t/'body.mds';filename.write_bytes(a)
    # Default frames have bone positioned at (10,0,0) and frame 1 rotates 90 degrees.
    verts,r=execute(runner,filename,0,1,2048,0,1,2048,0,t)
    assert 2.5<verts[1][0]<3.3 and 6.7<verts[1][1]<7.4,verts[1]
    print('PASS 01 bone-space arc (not linear skinned-vertex chord)')
    # Check identical output through the compatible rtcw_anim CLI.
    p=subprocess.run([str(anim),str(filename),'0','1','2048',str(t/'legacy.obj'),str(t/'legacy.ppm'),str(t/'legacy.txt')],capture_output=True,text=True)
    assert p.returncode==0,p.stderr
    assert (t/'pose.obj').read_bytes()==(t/'legacy.obj').read_bytes()
    print('PASS 02 backwards compatible blend API uses skeleton for MDS')
    # Distinct upper/lower frames, weight exactly 1 for the child bone (offset 68 of second bone).
    bones_off=120+2*(52+2*12)
    full=mutate(a,bones_off+80+68,'f',1.0)
    filename.write_bytes(full)
    verts,r=execute(runner,filename,0,0,0,1,1,0,0,t)
    assert abs(verts[1][0])<0.25 and abs(verts[1][1])<0.25,verts[1]
    assert 'tag_axisQ14' in r
    print('PASS 03 independent torso frame drives upper-bone rotation and tags')
    verts,r=execute(runner,filename,0,0,0,0,0,0,0,t)
    assert 9.7<verts[1][0]<10.3 and 9.7<verts[1][1]<10.3,verts[1]
    print('PASS 04 legs frame alone does not rotate full torso-weight bone')
    # Torso weight half, inherited 90 degree turn -> approx 45 degrees
    half=mutate(a,bones_off+80+68,'f',0.5)
    filename.write_bytes(half)
    verts,r=execute(runner,filename,0,0,0,1,1,0,0,t)
    assert 2.5<verts[1][0]<3.3 and 6.7<verts[1][1]<7.4,verts[1]
    print('PASS 05 partial torso weight blends shortest Euler angular difference')
    # Torso yaw about skeleton's torsoParent (root bone), weight=1.
    filename.write_bytes(full)
    verts,r=execute(runner,filename,0,0,0,0,0,0,16384,t)
    assert -10.3<verts[1][0]<-9.7 and 9.7<verts[1][1]<10.3,verts[1]
    print('PASS 06 torso yaw transforms geometry around torso pivot')
    # Smallest 16bit arc across 360 -> 0, no 180-degree flip.
    last=120+52+2*12+52+12+2
    wrap=mutate(mutate(a,120+52+12+2,'h',-1820),last,'h',1820)
    filename.write_bytes(wrap)
    verts,r=execute(runner,filename,0,1,2048,0,1,2048,0,t)
    assert verts[1][0]>9.4 and 9.4<verts[1][1]<10.6,verts[1]
    print('PASS 07 angle interpolation takes short path across wraparound')
    # Reject invalid root, cycle, invalid torso weight, bad ranges.
    filename.write_bytes(mutate(a,bones_off+80+68,'f',1.5))
    execute(runner,filename,0,1,2048,0,1,2048,0,t,False)
    print('PASS 08 out-of-range torso influence rejected')
    filename.write_bytes(mutate(a,bones_off+64,'i',1))
    execute(runner,filename,0,0,0,0,0,0,0,t,False)
    print('PASS 09 cyclic bone parent rejected')
    filename.write_bytes(a)
    execute(runner,filename,0,3,2000,0,1,0,0,t,False)
    execute(runner,filename,0,1,4100,0,1,0,0,t,False)
    print('PASS 10 invalid frame and interpolation fraction rejected')
    # MDS roots of synthetic fixture are zero, moving root should move both mesh and tags.
    frame1_origin_offset=120+(52+2*12)+40
    with_root=mutate(a,frame1_origin_offset,'f',8.0)
    filename.write_bytes(with_root)
    verts,r=execute(runner,filename,0,1,2048,0,1,2048,0,t)
    assert 6.4<verts[1][0]<7.4,verts[1]
    print('PASS 11 root translation lerped together with bones')
    # Real MDS interpolates OFFSET direction vectors before translation.
    # frame1 child bone ofsYaw sits at frame1+52+bone1*12+10.
    frame1_child_ofs_yaw=120+(52+2*12)+52+12+10
    moving=mutate(a,frame1_child_ofs_yaw,'h',16384)
    filename.write_bytes(moving)
    verts,r=execute(runner,filename,0,1,2048,0,1,2048,0,t)
    assert 6.6<verts[0][0]<7.5 and 6.6<verts[0][1]<7.5,verts[0]
    print('PASS 12 child offset direction interpolated as normalized 3D vectors')
    # RTCW external torso rotation uses weighted matrix (1-w)I + w*R,
    # which differs from simply scaling a yaw angle.
    filename.write_bytes(half)
    verts,r=execute(runner,filename,0,0,0,0,0,0,16384,t)
    assert abs(verts[1][0])<0.3 and 9.5<verts[1][1]<10.5,verts[1]
    print('PASS 13 partial external torso yaw uses weighted rotation matrix')
print('ALL v0.4 TESTS PASS')
