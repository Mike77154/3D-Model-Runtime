"""Deterministic smoke test for wrapped compressed inputs, not a security proof."""
import pathlib,random,subprocess,gzip,tempfile
SRC=pathlib.Path('/mnt/data/_blend_real_corpus/blender-2.79a/release__datafiles__startup.blend')
EXE=pathlib.Path(__file__).resolve().parents[1]/'blendc89'
if not SRC.exists():
    print('SKIP: authentic source corpus not available')
else:
    with tempfile.TemporaryDirectory() as tmp:
        tmp=pathlib.Path(tmp)
        plain=SRC.read_bytes()
        zz=tmp/'orig.zst'
        subprocess.run(['zstd','-q','-f','-o',str(zz),str(SRC)],check=True)
        inputs=[gzip.compress(plain),zz.read_bytes()]
        rng=random.Random(42008)
        for k,raw in enumerate(inputs):
            for j in range(80):
                mutant=bytearray(raw)
                if j%4==0:mutant=mutant[:rng.randrange(5,len(mutant))]
                else:
                    for _ in range((j%7)+1):
                        p=rng.randrange(len(mutant));mutant[p]^=rng.randrange(1,256)
                path=tmp/'mutated.blend';path.write_bytes(mutant)
                r=subprocess.run([str(EXE),'--info',str(path)],capture_output=True,timeout=5)
                assert r.returncode >= 0,(k,j,r.returncode)
        print('PASS: 160 gzip/zstd corrupted frames terminated without OS signal')
