"""Small deterministic mutation smoke test. Not security certification."""
from pathlib import Path
import tempfile, subprocess, random, sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase11_bindings import build
EXE=Path(__file__).resolve().parents[1]/'blendc89'
rand=random.Random(20261102)
with tempfile.TemporaryDirectory() as d:
    tmp=Path(d);inp=tmp/'mut.blend';out=tmp/'out.tsv'
    for num in range(96):
        src=bytearray(build(ptr=8,endian='<',nla=True))
        for _ in range(1+num%4):
            p=rand.randrange(12,len(src)-24)
            src[p]=rand.randrange(256)
        inp.write_bytes(src)
        for cmd in ('--bindings','--nla'):
            r=subprocess.run([str(EXE),cmd,str(inp),str(out)],capture_output=True,timeout=4)
            assert r.returncode>=0,(num,cmd,r.returncode)
print('PASS: 192 mutated binary invocations completed without signal termination')
