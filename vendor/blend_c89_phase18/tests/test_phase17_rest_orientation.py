"""Phase17 synthetic Blender DNA fixture: arbitrary rigid bone rest orientations,
including 90deg bone swing and 90deg local Y roll. Not a Blender IK parity test."""
import sys,subprocess,tempfile,math,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase13_multirig import make_file
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'

def run(*args):
    return subprocess.run([str(EXE),*map(str,args)],capture_output=True,text=True,timeout=25)

def vertex(path):
    line=next(s for s in path.read_text().splitlines() if s.startswith('v '))
    return tuple(map(float,line.split()[1:4]))

def near(actual,expected,tol=0.012):
    assert all(abs(x-y)<tol for x,y in zip(actual,expected)),(actual,expected)

def main():
    accepted=0; rejected=0
    with tempfile.TemporaryDirectory() as d:
        p=Path(d);blend=p/'phase17.blend';prefix=p/'frame'
        for ptr in (4,8):
            for endian in ('<','>'):
                for variant in ('minusx','roll90','plusz'):
                    blend.write_bytes(make_file(ptr,endian,constraint='ik',ik_rest_variant=variant))
                    r=run('--play-rigs',blend,5,prefix)
                    assert r.returncode==0,(ptr,endian,variant,r.stderr)
                    near(vertex(p/'frame_mesh0000.obj'),{'minusx':(4,1,0),'roll90':(4,1,0),'plusz':(1,0,-2)}[variant])
                    near(vertex(p/'frame_mesh0001.obj'),(5,0,0),tol=.001)
                    accepted+=1
                for variant in ('broken_axis','broken_chain','scaled'):
                    for out in p.glob('frame_mesh*.obj'):out.unlink()
                    blend.write_bytes(make_file(ptr,endian,constraint='ik',ik_rest_variant=variant))
                    r=run('--play-rigs',blend,5,prefix)
                    assert r.returncode!=0,(ptr,endian,variant,r.stdout,r.stderr)
                    assert not list(p.glob('frame_mesh*.obj')),(ptr,endian,variant)
                    rejected+=1
        # Additional genuinely oblique rest rotations, with independent twist.
        # Test fixtures intentionally use Python floating-point to produce
        # realistic serialized IEEE754 Blender DNA; C runtime never does.
        rng=random.Random(17017)
        for index in range(36):
            a=rng.uniform(-3.0,3.0)
            b=rng.uniform(-1.3,1.3)
            c=rng.uniform(-3.0,3.0)
            ca,sa=math.cos(a),math.sin(a)
            cb,sb=math.cos(b),math.sin(b)
            cc,sc=math.cos(c),math.sin(c)
            m=(
                (ca*cb, sa*cb, -sb),
                (ca*sb*sc-sa*cc, sa*sb*sc+ca*cc, cb*sc),
                (ca*sb*cc+sa*sc, sa*sb*cc-ca*sc, cb*cc)
            )
            # m consists of 3 Blender-style column vectors.
            ptr=4 if index%2 else 8
            endian='<' if index%4<2 else '>'
            blend.write_bytes(make_file(ptr,endian,constraint='ik',ik_rest_variant=('basis',m)))
            r=run('--play-rigs',blend,5,prefix)
            assert r.returncode==0,(index,ptr,endian,r.stderr)
            y=m[1]
            expected=(-3*y[0]+1,-3*y[1]+1,-3*y[2])
            near(vertex(p/'frame_mesh0000.obj'),expected,0.018)
            near(vertex(p/'frame_mesh0001.obj'),(5,0,0),0.001)
            accepted+=1
    print('PASS phase17: %d rest-orientation playback samples and %d malformed-rest rejections' % (accepted,rejected))

if __name__=='__main__':main()
