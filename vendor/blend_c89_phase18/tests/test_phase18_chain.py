"""Phase 18: independent C89 multi-link IK, explicit rest frames. Synthetic only."""
import math
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / 'blendc89'


def testcase(n=3, axis='y', target=(1.25, 2.25, 0), limits=None, malformed=None, iterations=96):
    if axis == 'y':
        vec = (0., 1., 0.)
        m = (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0)
    elif axis == 'x':
        vec = (1., 0., 0.)
        m = (0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0)
    elif axis == 'z':
        vec = (0., 0., 1.)
        m = (1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0)
    else:
        raise ValueError(axis)
    out = [f'chain\t{n}', 'target\t%s\t%s\t%s' % target, f'iterations\t{iterations}']
    for j in range(n+1):
        out.append('node\t%d\t%s\t%s\t%s' % (j, *(j*x for x in vec)))
    for j in range(1, n):
        out.append('limit\t%d\t%s' % (j, (limits or {}).get(j, -1)))
    for j in range(n):
        mm = list(m)
        for k in range(3):
            mm[4*k+3] = j * vec[k]
        if malformed == 'scale' and j == 0:
            mm[0] = 2
        if malformed == 'translation' and j == 1:
            mm[7] += 1
        out.append('matrix\t%d\t%s' % (j, '\t'.join(map(str, mm))))
    return '\n'.join(out)+'\n'


def execute(tmp, text):
    source = tmp / 'chain.tsv'; palette = tmp / 'palette.tsv'; joints = tmp / 'joints.tsv'
    source.write_text(text, encoding='ascii')
    palette.unlink(missing_ok=True)
    joints.unlink(missing_ok=True)
    p = subprocess.run([str(BIN), '--ik-chain', str(source), str(palette), str(joints)],
                       capture_output=True, text=True, timeout=15)
    return p, palette, joints


def read_joint(path):
    lines = [s for s in path.read_text().splitlines() if s and not s.startswith('#')]
    return [tuple(map(float, line.split('\t')[1:4])) for line in lines]


def verify(path, target, n):
    joints = read_joint(path)
    assert len(joints) == n+1, (len(joints), n)
    assert math.dist(joints[0], (0, 0, 0)) <= .0001
    assert math.dist(joints[-1], target) <= 0.015, (joints[-1], target)
    for a,b in zip(joints,joints[1:]):
        assert abs(math.dist(a,b)-1)<0.018,(a,b,math.dist(a,b))
    return joints


def main():
    positives=0
    negatives=0
    with tempfile.TemporaryDirectory() as d:
        tmp=Path(d)
        for axis,targets in [
            ('y',[(1.25,2.25,0),(0.75,2.5,0),(1,1.5,1)]),
            ('x',[(2.25,1.25,0),(2.2,0.4,0.6)]),
            ('z',[(0.5,1,2.25),(0,1.25,2.5)])
        ]:
            for n in (3,4,5,6,8):
                for base_target in targets:
                    scale = n/3.0
                    target = tuple(round(v*scale,4) for v in base_target)
                    p,palette,joints=execute(tmp,testcase(n,axis,target))
                    assert p.returncode==0,(axis,n,target,p.stderr)
                    verify(joints,target,n)
                    data=[s for s in palette.read_text().splitlines() if s and not s.startswith('#')]
                    assert len(data)==n
                    positives+=1
        for n,axis,target,limits in [(3,'y',(0.5,2.5,0),{1:0.0}),
                                     (4,'x',(3.0,1.0,0),{1:0.0,2:-1,3:-1})]:
            p,palette,joints=execute(tmp,testcase(n,axis,target,limits=limits))
            assert p.returncode==0,('cone positive',p.stderr)
            jnts=verify(joints,target,n)
            for i,minimum in limits.items():
                p0=[jnts[i][k]-jnts[i-1][k] for k in range(3)]
                p1=[jnts[i+1][k]-jnts[i][k] for k in range(3)]
                dot=sum(a*b for a,b in zip(p0,p1))/math.dist(jnts[i],jnts[i-1])/math.dist(jnts[i+1],jnts[i])
                assert dot>=minimum-0.02,(dot,minimum)
            positives+=1
        for n,axis,target,limits,malformed in [
            (3,'y',(5,5,0),None,None),
            (3,'y',(0.5,2.5,0),{1:1,2:1},None),
            (3,'y',(1,2,0),None,'scale'),
            (3,'y',(1,2,0),None,'translation'),
        ]:
            p,pal,joints=execute(tmp,testcase(n,axis,target,limits,malformed))
            assert p.returncode!=0,(p.stdout,p.stderr)
            assert not pal.exists() and not joints.exists()
            negatives+=1
        for sample in ['chain\t0\n','chain\t9\n','chain\t3\ninvalid',
                       testcase(3)[:-5],testcase(3).replace('matrix\t1','matrix\t10'),
                       testcase(3).replace('matrix\t0', 'broken\t0')]:
            p,pal,joints=execute(tmp,sample)
            assert p.returncode!=0,(p.stdout,p.stderr)
            assert not pal.exists() and not joints.exists()
            negatives+=1
    print('PASS phase18: %d successful solved chains, %d safe rejections' % (positives,negatives))

if __name__=='__main__': main()
