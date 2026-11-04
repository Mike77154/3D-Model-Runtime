"""Phase 15 IK2 analytic solver and downstream skinning; synthetic math tests.
These tests are NOT Blender IK semantic equivalence tests."""
import math
import random
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'blendc89'

def run(*args):
    return subprocess.run([str(EXE), *map(str,args)], capture_output=True, text=True, timeout=20)

def numbers(path):
    rows={}
    for line in path.read_text().splitlines():
        if not line.strip() or line.startswith('#'): continue
        name, *values=line.split()
        rows[name]=tuple(float(v) for v in values)
    return rows

def fmt(*a): return ' '.join(f'{x:.4f}' for x in a)

def write_goals(path, root, target, pole, lengths):
    path.write_text('root '+fmt(*root)+'\ntarget '+fmt(*target)+'\npole '+fmt(*pole)+'\nlengths '+fmt(*lengths)+'\n')

def close(v,w,eps=0.008):
    assert all(abs(a-b)<eps for a,b in zip(v,w)),(v,w)

def main():
    rng=random.Random(15015)
    with tempfile.TemporaryDirectory() as d:
        d=Path(d); inp=d/'goal.tsv';pal=d/'pal.tsv';joints=d/'joints.tsv';
        verts=ROOT/'examples/phase15_ik2_vertices.tsv'; skin=d/'skinned.tsv'
        write_goals(inp,(0,0,0),(1,1,0),(-1,1,0),(1,1))
        assert run('--ik2-palette',inp,pal,joints).returncode==0
        p=numbers(joints)
        close(p['root'],(0,0,0));close(p['elbow'],(0,1,0));close(p['tip'],(1,1,0))
        assert run('--skin-tsv',pal,verts,skin).returncode==0
        rows=numbers(skin)
        close(rows['2'],(1,1,0));close(rows['3'],(0.5,1,0))
        # Spatial bend tests: random 3D targets in reachable region, noncolinear poles.
        for i in range(75):
            L1=rng.uniform(0.75,2.0);L2=rng.uniform(0.75,2.0)
            length=rng.uniform(abs(L1-L2)+0.10,L1+L2-0.10)
            raw=[rng.uniform(-1,1) for _ in range(3)]
            n=math.sqrt(sum(x*x for x in raw)); unit=[x/n for x in raw]
            root=[rng.uniform(-2,2) for _ in range(3)]
            target=[root[j]+length*unit[j] for j in range(3)]
            rawpole=[rng.uniform(-3,3) for _ in range(3)]
            # Enforce noncolinear pole.
            if abs(sum(rawpole[j]*unit[j] for j in range(3))) > 0.95*math.sqrt(sum(x*x for x in rawpole)):
                rawpole=[1.2,-2.0,0.9]
            pole=[root[j]+rawpole[j] for j in range(3)]
            write_goals(inp,root,target,pole,(L1,L2))
            proc=run('--ik2-palette',inp,pal,joints)
            assert proc.returncode==0,(i,proc.stderr,inp.read_text())
            p=numbers(joints)
            close(p['root'],root,0.0002)
            close(p['tip'],target,0.0002)
            seg1=math.dist(p['root'],p['elbow']);seg2=math.dist(p['elbow'],p['tip'])
            assert abs(seg1-L1)<0.005 and abs(seg2-L2)<0.005,(i,seg1,L1,seg2,L2)
            assert len(numbers(pal))==2
        # Rejected goals never leave partial output.
        rejected=[((0,0,0),(3,0,0),(0,1,0),(1,1)),
                  ((0,0,0),(0,0,0),(1,0,0),(1,1)),
                  ((0,0,0),(1,1,0),(2,2,0),(1,1)),
                  ((0,0,0),(0.2,0,0),(0,1,0),(2,1)),
                  ((0,0,0),(1,1,0),(-1,1,0),(0,1))]
        for i,(r,t,p,l) in enumerate(rejected):
            write_goals(inp,r,t,p,l)
            pal.unlink(missing_ok=True);joints.unlink(missing_ok=True)
            proc=run('--ik2-palette',inp,pal,joints)
            assert proc.returncode!=0,(i,proc.stderr)
            assert not pal.exists() and not joints.exists()
        inp.write_text('root 0 0 0\ntarget 1 1 0\npole -1 1 0\nlengths 1 1\nEVIL 1\n')
        assert run('--ik2-palette',inp,pal,joints).returncode!=0
        # Endian-agnostic serialized IK auditor smoke test on a genuine .blend
        for path in [
            Path('/mnt/data/_blend_real_corpus/blender-2.79a/release__datafiles__startup.blend'),
            Path('/mnt/data/_blend_real_corpus/blender-4.5.14/release__datafiles__startup.blend')]:
            if path.exists():
                output=d/'audit.tsv'
                proc=run('--ik-audit',path,output)
                assert proc.returncode==0,(path,proc.stderr)
                assert output.read_text().startswith('# ordinal')
    print('PASS phase15: 1 composed IK+skinning, 75 3D randomized analytic cases, 5 unreachable/degenerate, 1 malformed, real .blend IK audits')

if __name__=='__main__': main()
