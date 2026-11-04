"""Bounded mutation smoke check for --sample; not a security proof."""
import pathlib,random,subprocess,tempfile,sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from test_phase05_eval import make_pair
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'
rng=random.Random(5031)
seed=make_pair(2).build()
with tempfile.TemporaryDirectory() as d:
    p=pathlib.Path(d)/'input.blend'
    o=pathlib.Path(d)/'output.tsv'
    for i in range(80):
        raw=bytearray(seed)
        for _ in range(1+i%4):
            pos=rng.randrange(len(raw))
            raw[pos]=rng.randrange(256)
        p.write_bytes(raw)
        try:
            r=subprocess.run([str(EXE),'--sample',str(p),str(o),'5.5'],capture_output=True,timeout=3)
        except subprocess.TimeoutExpired as e:
            raise AssertionError('sample parser timeout at case '+str(i)) from e
        assert r.returncode>=0,(i,'terminated by signal',r.returncode)
        if r.returncode!=0:
            assert not o.exists(),(i,'partial output preserved on error')
        elif o.exists():
            o.unlink()
print('PASS: 80 deterministic mutations on synthetic --sample fixture (no signal termination)')
