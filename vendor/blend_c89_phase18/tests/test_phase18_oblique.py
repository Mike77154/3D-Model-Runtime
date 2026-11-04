"""Deterministic 3D rest-frame QA for the independent 3-8-link IK solver."""
import math
import random
import subprocess
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'

def unit(v):
    d=math.sqrt(sum(x*x for x in v))
    return tuple(x/d for x in v)

def cross(a,b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])

def main():
    r=random.Random(18018)
    okay=0
    with tempfile.TemporaryDirectory() as d:
        tmp=Path(d)
        for case in range(80):
            y=unit(tuple(r.uniform(-1,1) for _ in range(3)))
            z=unit(cross((0,0,1) if abs(y[2])<0.9 else (1,0,0),y))
            x=unit(cross(y,z))
            z=unit(cross(x,y))
            n=(3,4,5,6,8)[case%5]
            points=[tuple(round(k*component,4) for component in y) for k in range(n+1)]
            target=tuple(round(n*.68*y[k]+n*.20*x[k],4) for k in range(3))
            rows=[f'chain {n}', 'target '+' '.join(map(str,target)), 'iterations 96']
            rows+=['node %d %s'%(i,' '.join(map(str,points[i]))) for i in range(n+1)]
            rows+=['limit %d -1'%i for i in range(1,n)]
            for i in range(n):
                mat=(x[0],y[0],z[0],points[i][0],
                     x[1],y[1],z[1],points[i][1],
                     x[2],y[2],z[2],points[i][2])
                rows+=['matrix %d %s'%(i,' '.join('%.4f'%v for v in mat))]
            src=tmp/'in.tsv'; pal=tmp/'pal.tsv'; joints=tmp/'joints.tsv'
            src.write_text('\n'.join(rows)+'\n')
            result=subprocess.run([str(EXE),'--ik-chain',str(src),str(pal),str(joints)],
                                  capture_output=True,text=True,timeout=12)
            assert result.returncode==0,(case,result.stderr)
            coords=[tuple(map(float,row.split()[1:])) for row in joints.read_text().splitlines()
                    if row and not row.startswith('#')]
            assert len(coords)==n+1
            assert math.dist(coords[-1],target)<.025,(case,coords[-1],target)
            assert all(abs(math.dist(a,b)-1)<.025 for a,b in zip(coords,coords[1:])),case
            assert len([row for row in pal.read_text().splitlines() if row and not row.startswith('#')])==n
            okay+=1
    print(f'PASS phase18 oblique: {okay} random rigid rest frames')

if __name__=='__main__': main()
