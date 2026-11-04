"""Deterministic finite mutation smoke tests. No claim of full fuzz coverage."""
import pathlib
import random
import subprocess
import tempfile
from test_phase04_animation import Fixture, put_ipo, put_modern, put_pose

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXE = ROOT/'blendc89'
CASES=[]
f=Fixture();put_modern(f);CASES.append(('--keys', f.build()))
f=Fixture();put_ipo(f);CASES.append(('--ipo', f.build()))
f=Fixture();put_pose(f);CASES.append(('--pose', f.build()))
rng=random.Random(20261102)
for command, corpus in CASES:
    for i in range(40):
        mutation=bytearray(corpus)
        for _ in range(1+i%5):
            at=rng.randrange(len(mutation))
            mutation[at]^=1<<rng.randrange(8)
        with tempfile.TemporaryDirectory() as td:
            source=pathlib.Path(td)/'f.blend'; dest=pathlib.Path(td)/'out.tsv'
            source.write_bytes(mutation)
            r=subprocess.run([str(EXE),command,str(source),str(dest)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=4)
            assert r.returncode>=0,(command,i,r.returncode)
            assert r.returncode in (0,1,2,3),(command,i,r.returncode)
            if r.returncode!=0:assert not dest.exists(),(command,i,'partial output file')
    print('PASS:',command,'40 mutated fixtures')
print('PASS: 120 Phase04 mutated fixtures, no signal exit')
