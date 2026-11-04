"""Crash smoke-test only. Mutations need not all be rejected (some remain valid)."""
import random
import subprocess
import tempfile
from pathlib import Path
from test_phase18_chain import testcase

EXE=Path(__file__).resolve().parents[1]/'blendc89'

def main():
    rand=random.Random(18080)
    original=bytearray(testcase(3,'y',(1.25,2.25,0)).encode('ascii'))
    with tempfile.TemporaryDirectory() as d:
        p=Path(d)
        for i in range(160):
            buf=bytearray(original)
            for _ in range(rand.randrange(1,5)):
                action=rand.randrange(4)
                if action==0 and buf:
                    buf[rand.randrange(len(buf))]=rand.randrange(256)
                elif action==1 and buf:
                    del buf[rand.randrange(len(buf)):]
                elif action==2:
                    buf.extend(rand.randbytes(rand.randrange(1,60)))
                elif buf:
                    pos=rand.randrange(len(buf))
                    buf[pos:pos]=bytes([rand.randrange(256)])
            a=p/'in.tsv';a.write_bytes(buf)
            b=p/'pal.tsv';c=p/'joints.tsv'
            result=subprocess.run([str(EXE),'--ik-chain',str(a),str(b),str(c)],
                                  stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=4)
            assert result.returncode>=0,(i,result.returncode)
    print('PASS phase18 mutations: 160 crash-smoke runs (not a security proof)')

if __name__=='__main__':main()
