"""Q16 rest->pose matrices from multi-link IK are consumed by existing TSV skinning."""
from pathlib import Path
import subprocess
import tempfile
import math
ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'blendc89'
EX=ROOT/'examples'
def run(*args):
    p=subprocess.run([str(BIN),*map(str,args)],capture_output=True,text=True,timeout=12)
    assert p.returncode==0,(p.stderr,p.stdout)
def main():
    with tempfile.TemporaryDirectory() as d:
        p=Path(d); palette=p/'palette.tsv'; joints=p/'joints.tsv'; skin=p/'skinned.tsv'
        run('--ik-chain',EX/'phase18_chain_3link.tsv',palette,joints)
        run('--skin-tsv',palette,EX/'phase18_vertices.tsv',skin)
        rows=[tuple(map(float,row.split()[1:4])) for row in skin.read_text().splitlines()
              if row and not row.startswith('#')]
        pose=[tuple(map(float,row.split()[1:4])) for row in joints.read_text().splitlines()
              if row and not row.startswith('#')]
        assert len(rows)==4 and len(pose)==4
        for src,dst in zip(rows,pose):
            assert math.dist(src,dst)<.003,(src,dst)
        assert math.dist(rows[-1],(1.25,2.25,0))<.015
    print('PASS phase18: IK palette -> existing --skin-tsv -> articulated vertices')
if __name__=='__main__':main()
